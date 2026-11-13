// SPDX-License-Identifier: Apache-2.0
/*
 * Input files (JSON reader, configuration and application files) and the
 * generic platform: templates, custom topologies, round trips.
 */
#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "mapping/assign.h"
#include "mapping/generator.h"
#include "mapping/io.h"
#include "mapping/json.h"
#include "mapping/network.h"
#include "mapping/pipeline.h"
#include "unit.h"

#define TEMP "test_io_temp.json"

static int parse(const char *text, struct mp_json *root)
{
	char error[200];

	return mp_json_parse(text, root, error, sizeof(error));
}

static void test_json_values(void)
{
	struct mp_json root;
	const struct mp_json *v;

	CHECK_EQ(parse(" {\"a\": [1, -2.5e2, true, false, null],\n"
		       " \"b\": \"x\\u00e9\\ud83d\\ude00\\n\", \"c\": {}} ",
		       &root), 0);
	CHECK_EQ(root.type, MP_JSON_OBJECT);
	CHECK_EQ(root.count, 3);
	v = mp_json_get(&root, "a");
	CHECK(v && v->type == MP_JSON_ARRAY && v->count == 5);
	if (v && v->count == 5) {
		CHECK_NEAR(v->items[0].number, 1, 0);
		CHECK_NEAR(v->items[1].number, -250, 0);
		CHECK(v->items[2].type == MP_JSON_BOOL && v->items[2].boolean);
		CHECK(v->items[3].type == MP_JSON_BOOL && !v->items[3].boolean);
		CHECK_EQ(v->items[4].type, MP_JSON_NULL);
	}
	v = mp_json_get(&root, "b");
	/* e-acute (2 bytes), U+1F600 (4 bytes), a line feed */
	CHECK(v && !strcmp(v->string, "x\xc3\xa9\xf0\x9f\x98\x80\n"));
	CHECK_EQ(mp_json_get(&root, "c")->line, 2);
	CHECK(!mp_json_get(&root, "d"));
	mp_json_free(&root);
}

static void test_json_errors(void)
{
	static const char *const wrong[] = {
		"{\"a\": 1,}", "[1, 2", "{\"a\": 1, \"a\": 2}", "01", "1.",
		"\"\\ud800\"", "\"a\nb\"", "{a: 1}", "[1] 2", "", "\"\\u0000\"",
		"tru", "-", "1e", "\"\\x\"",
	};
	struct mp_json root;
	char deep[200];
	size_t i;

	for (i = 0; i < sizeof(wrong) / sizeof(wrong[0]); i++)
		CHECK_EQ(parse(wrong[i], &root), -EINVAL);
	memset(deep, '[', 100);
	memset(deep + 100, ']', 99);
	deep[199] = '\0';
	CHECK_EQ(parse(deep, &root), -EINVAL);	/* deeper than 64 */
}

static void write_text(const char *text)
{
	FILE *out = fopen(TEMP, "w");

	fputs(text, out);
	fclose(out);
}

/* the configuration file of @params, as text */
static char *configuration_text(const struct mp_params *params)
{
	char *text = NULL;
	size_t size = 0;
	FILE *out = open_memstream(&text, &size);

	mp_write_configuration(out, params);
	fclose(out);
	return text;
}

/* write, read, write again: the same text and the same platform */
static void test_templates_round_trip(void)
{
	static const char *const names[] = {"zonal", "domain", "hybrid",
					    "distributed"};
	struct mp_params params, again;
	char error[300];
	size_t i;

	for (i = 0; i < 4; i++) {
		char *text, *text_again;

		CHECK_EQ(mp_params_template(&params, names[i]), 0);
		text = configuration_text(&params);
		write_text(text);
		CHECK_EQ(mp_read_configuration(TEMP, &again, error,
					       sizeof(error)), 0);
		text_again = configuration_text(&again);
		CHECK(!strcmp(text, text_again));
		CHECK_EQ(memcmp(&params.platform, &again.platform,
				sizeof(params.platform)), 0);
		/* a backbone rate of 0 means the link rate, written out */
		if (!params.network.backbone_mbps)
			params.network.backbone_mbps = params.network.link_mbps;
		CHECK_EQ(memcmp(&params.network, &again.network,
				sizeof(params.network)), 0);
		free(text);
		free(text_again);
	}
	CHECK_EQ(mp_params_template(&params, "central"), -EINVAL);
}

/* the hosts of the free sub-chains: the central computers, or without
 * one the ECUs with the fastest cores */
static void test_hosts(void)
{
	struct mp_params params;
	int order[MP_MAX_ECUS], c;

	mp_params_default(&params);		/* built-in zonal */
	CHECK(mp_ecu_is_host(&params, 0));
	for (c = 1; c < mp_ecu_count(&params); c++)
		CHECK(!mp_ecu_is_host(&params, c));

	CHECK_EQ(mp_params_template(&params, "hybrid"), 0);
	CHECK(mp_ecu_is_host(&params, mp_find_ecu(&params, "PCU")));
	CHECK(!mp_ecu_is_host(&params, mp_find_ecu(&params, "ADAS")));

	/* domain: ADAS and Infotainment have big cores at 100 % */
	CHECK_EQ(mp_params_template(&params, "domain"), 0);
	CHECK(mp_ecu_is_host(&params, mp_find_ecu(&params, "ADAS")));
	CHECK(mp_ecu_is_host(&params, mp_find_ecu(&params, "Infotainment")));
	CHECK(!mp_ecu_is_host(&params, mp_find_ecu(&params, "Powertrain")));
	CHECK(!mp_ecu_is_host(&params, mp_find_ecu(&params, "Body")));
	CHECK(mp_host_allowed(&params, MP_ALL_CONTROLLERS, 1u << MP_CORE_BIG));
	CHECK(!mp_host_allowed(&params, MP_ALL_CONTROLLERS,
			       1u << MP_CORE_LOCKSTEP));
	CHECK_EQ(mp_ecu_preference(&params, order), 5);
	CHECK_EQ(order[0], mp_find_ecu(&params, "ADAS"));
	CHECK_EQ(order[1], mp_find_ecu(&params, "Infotainment"));
	CHECK_EQ(order[2], mp_find_ecu(&params, "Powertrain"));

	CHECK_EQ(mp_params_template(&params, "distributed"), 0);
	CHECK(mp_ecu_is_host(&params, mp_find_ecu(&params, "ADAS")));
	CHECK(!mp_ecu_is_host(&params, mp_find_ecu(&params, "ESC")));
	CHECK(!mp_ecu_is_host(&params, -1));
}

/* the explicit zonal template is the built-in platform, ECU by ECU */
static void test_zonal_template_is_builtin(void)
{
	struct mp_params builtin, explicit;
	int c, t;

	mp_params_default(&builtin);
	CHECK_EQ(mp_params_template(&explicit, "zonal"), 0);
	CHECK_EQ(mp_ecu_count(&explicit), mp_ecu_count(&builtin));
	for (c = 0; c < MP_ZONAL_ECUS; c++) {
		CHECK(!strcmp(mp_ecu_name(&builtin, c), mp_ecu_name(&explicit, c)));
		CHECK_EQ(mp_ecu_role(&builtin, c), mp_ecu_role(&explicit, c));
		CHECK_EQ(mp_ecu_memory(&builtin, c), mp_ecu_memory(&explicit, c));
		for (t = 0; t < MP_CORE_TYPES; t++)
			CHECK_EQ(mp_ecu_speed(&builtin, c, t),
				 mp_ecu_speed(&explicit, c, t));
	}
	CHECK_EQ(mp_io_ecus(&builtin), ZONAL_ZONES);
	CHECK_EQ(mp_io_ecus(&explicit), ZONAL_ZONES);
}

static void check_error(const char *text, const char *expected)
{
	struct mp_params params;
	char error[300] = "";

	write_text(text);
	CHECK_EQ(mp_read_configuration(TEMP, &params, error, sizeof(error)),
		 -EINVAL);
	if (!strstr(error, expected))
		printf("  error \"%s\", expected \"%s\"\n", error, expected);
	CHECK(strstr(error, expected) != NULL);
}

static void test_configuration_errors(void)
{
	check_error("{\"method\": {\"sigma\": 0}}", "\"sigma\" must be above 0");
	check_error("{\"methods\": {}}", "unknown key \"methods\"");
	check_error("{\"execution_model\": {\"scheduling\": \"EDF\"}}",
		    "can only be \"fixed-priority preemptive\"");
	check_error("{\"platform\": {\"ecus\": [{\"name\": \"A\", \"role\": "
		    "\"zone\", \"cores\": [{\"type\": \"gpu\", \"count\": 1, "
		    "\"speed_percent\": 100}]}]}}", "must be one of big");
	check_error("{\"platform\": {\"ecus\": [{\"name\": \"A\", \"role\": "
		    "\"zone\", \"cores\": [{\"type\": \"mcu\", \"count\": 3, "
		    "\"speed_percent\": 50}], \"max_cores\": 2}]}}",
		    "more initial cores than its maximum");
	check_error("{\"platform\": {\"ecus\": [{\"name\": \"A\", \"role\": "
		    "\"zone\", \"cores\": [{\"type\": \"mcu\", \"count\": 1, "
		    "\"speed_percent\": 50}]}, {\"name\": \"A\", \"role\": "
		    "\"zone\", \"cores\": [{\"type\": \"mcu\", \"count\": 1, "
		    "\"speed_percent\": 50}]}]}}", "two ECUs are called A");
	check_error("{\"network_model\": {\"topology\": \"ring\", \"switches\": "
		    "[]}}", "are for a custom topology");
	check_error("{\"network_model\": {\"topology\": \"custom\", "
		    "\"switches\": [{\"name\": \"S\", \"ecus\": [\"PCU\"]}]}}",
		    "ECU ZCU1 is linked to no switch");
	check_error("{\"network_model\": {\"traffic_classes\": 2, "
		    "\"idle_slopes_percent\": [60, 50]}}", "inconsistent network");
	check_error("{\"network_model\": {\"frer\": true}}",
		    "inconsistent network");
	check_error("{\"method\": {\"sigma\": 0.9}, \"execution_model\": "
		    "{\"utilization_cap\": 0.8}}", "must not exceed");
}

/* a custom topology: a ring of four switches, one of them without ECU */
static void test_custom_routes(void)
{
	struct mp_params params;
	struct mp_net_config *net;
	int links[MP_NET_MAX_ROUTE], second[MP_NET_MAX_ROUTE];
	int count, n, h, from, to, k;
	char error[300];

	write_text("{\"network_model\": {\"topology\": \"custom\", \"frer\": "
		   "true, \"switches\": ["
		   "{\"name\": \"S0\", \"ecus\": [\"PCU\", \"ZCU1\"]},"
		   "{\"name\": \"S1\", \"ecus\": [\"ZCU2\", \"ZCU3\"]},"
		   "{\"name\": \"S2\", \"ecus\": [\"ZCU4\", \"ZCU5\"]},"
		   "{\"name\": \"S3\", \"ecus\": []}],"
		   "\"switch_links\": [{\"from\": \"S0\", \"to\": \"S1\"},"
		   "{\"from\": \"S1\", \"to\": \"S2\", \"mbps\": 100},"
		   "{\"from\": \"S2\", \"to\": \"S3\"},"
		   "{\"from\": \"S3\", \"to\": \"S0\"}]}}");
	CHECK_EQ(mp_read_configuration(TEMP, &params, error, sizeof(error)), 0);
	net = &params.network;
	CHECK_EQ(mp_net_switch_count(net), 4);
	CHECK_EQ(mp_net_ecu_count(net), 6);
	/* 2 * 6 ECU links + 2 * 4 switch links */
	CHECK_EQ(mp_net_link_count(net), 20);
	/* same switch: the egress port, then the switch port */
	CHECK_EQ(mp_net_route(net, MP_CONTROLLER_PCU, MP_CONTROLLER_ZCU1, links),
		 2);
	CHECK_EQ(mp_net_secondary_route(net, MP_CONTROLLER_PCU,
					MP_CONTROLLER_ZCU1, second), 0);
	/* PCU to ZCU4: S0, then S1 (first link on the tie), S2 */
	count = mp_net_route(net, MP_CONTROLLER_PCU, MP_CONTROLLER_ZCU4, links);
	CHECK_EQ(count, 4);
	mp_net_link(net, links[1], &from, &to);
	CHECK_EQ(from, 6 + 0);
	CHECK_EQ(to, 6 + 1);
	/* the 100 Mbit/s link between S1 and S2 */
	CHECK_NEAR(mp_net_link(net, links[2], &from, &to), 100, 0);
	/* the second route goes through S3, shares only the end ports */
	n = mp_net_secondary_route(net, MP_CONTROLLER_PCU, MP_CONTROLLER_ZCU4,
				   second);
	CHECK_EQ(n, 4);
	CHECK_EQ(second[0], links[0]);
	CHECK_EQ(second[n - 1], links[count - 1]);
	for (h = 1; h < n - 1; h++) {
		for (k = 1; k < count - 1; k++)
			CHECK(second[h] != links[k]);
		mp_net_link(net, second[h], &from, &to);
		CHECK(from != 6 + 1 && to != 6 + 1);	/* avoids S1 */
	}
	mp_net_link(net, second[1], &from, &to);
	CHECK_EQ(to, 6 + 3);
}

/* a tree on the domain template: rooted at its first ECU */
static void test_tree_root(void)
{
	struct mp_params params;
	int links[MP_NET_MAX_ROUTE];

	CHECK_EQ(mp_params_template(&params, "domain"), 0);
	params.network.topology = MP_NET_TREE;
	CHECK_EQ(mp_net_config_check(&params.network), 0);
	CHECK_EQ(params.network.tree_root, 0);
	/* Powertrain to Body: its switch, the root switch, Body's switch */
	CHECK_EQ(mp_net_route(&params.network, 2, 4, links), 4);
	CHECK_EQ(mp_net_route(&params.network, 0, 4, links), 3);
}

static int compare_applications(const struct mp_chain_array *a,
				const struct mp_chain_array *b)
{
	int i, k, t, differences = 0;

	if (a->count != b->count)
		return 1;
	for (i = 0; i < a->count; i++) {
		const struct mp_chain *x = &a->items[i], *y = &b->items[i];

		differences += x->id != y->id || x->controller != y->controller ||
			       x->replica_group != y->replica_group ||
			       x->deadline != y->deadline ||
			       x->utilization != y->utilization ||
			       x->memory != y->memory ||
			       x->period_gcd != y->period_gcd ||
			       x->tasks.count != y->tasks.count;
		for (k = 0; k < x->tasks.count && k < y->tasks.count; k++) {
			const struct mp_task *p = &x->tasks.items[k];
			const struct mp_task *q = &y->tasks.items[k];

			differences += p->wcet != q->wcet ||
				p->reference_wcet != q->reference_wcet ||
				p->period != q->period ||
				p->priority != q->priority ||
				p->utilization != q->utilization ||
				p->controller != q->controller ||
				(mp_task_allowed(p) & 0x3f) !=
				(mp_task_allowed(q) & 0x3f) ||
				p->colocate_next != q->colocate_next ||
				mp_task_types(p) != mp_task_types(q) ||
				p->memory != q->memory ||
				p->accesses != q->accesses ||
				p->id_in_chain != q->id_in_chain ||
				p->unique_id != q->unique_id ||
				p->chain_id != q->chain_id ||
				p->message_bytes != q->message_bytes;
			for (t = 0; t < MP_CORE_TYPES; t++)
				differences += p->type_factor[t] !=
					       q->type_factor[t];
		}
	}
	return differences;
}

/* a generated application, written and read back, is the same */
static void test_application_round_trip(void)
{
	struct mp_chain_array generated, read;
	struct mp_gen_params generator;
	struct mp_names names;
	struct mp_ctx ctx;
	char error[300];
	FILE *out;

	mp_ctx_init(&ctx, 11);
	mp_gen_params_default(&generator);
	generator.chain_count = 150;
	generator.replica_percent = 20;
	generator.zone_only_percent = 20;
	generator.lockstep_percent = 10;
	generator.colocate_percent = 30;
	generator.type_variation = 30;
	CHECK_EQ(mp_generate_application(&ctx, &generator, &generated), 0);
	out = fopen(TEMP, "w");
	mp_write_application(out, &ctx.params, &generated, NULL);
	fclose(out);
	ctx.params.chain_count = 0;
	CHECK_EQ(mp_read_application(TEMP, &ctx.params, &read, &names, error,
				     sizeof(error)), 0);
	CHECK_EQ(ctx.params.chain_count, 150);
	CHECK_EQ(compare_applications(&generated, &read), 0);
	CHECK_EQ(names.chain_count, 150);
	CHECK(!strcmp(names.chain[7], "chain7"));
	mp_names_free(&names);
	mp_chain_array_free(&read);
	mp_chain_array_free(&generated);
}

static void check_application_error(const char *text, const char *expected)
{
	struct mp_chain_array application;
	struct mp_params params;
	char error[300] = "";

	mp_params_default(&params);
	write_text(text);
	CHECK_EQ(mp_read_application(TEMP, &params, &application, NULL, error,
				     sizeof(error)), -EINVAL);
	if (!strstr(error, expected))
		printf("  error \"%s\", expected \"%s\"\n", error, expected);
	CHECK(strstr(error, expected) != NULL);
}

static void test_application_errors(void)
{
	check_application_error("{\"chains\": []}", "non-empty array");
	check_application_error("{\"chains\": [{\"tasks\": [{\"period_us\": "
				"100}]}]}", "\"wcet_us\" is missing");
	check_application_error("{\"chains\": [{\"tasks\": [{\"period_us\": "
				"100, \"wcet_us\": 10, \"ecu\": \"ZCU9\"}]}]}",
				"no ECU called \"ZCU9\"");
	check_application_error("{\"chains\": [{\"ecu\": \"ZCU1\", \"tasks\": "
				"[{\"period_us\": 100, \"wcet_us\": 10, \"ecu\": "
				"\"ZCU2\"}]}]}", "another ECU than its chain");
	check_application_error("{\"chains\": [{\"replica_group\": 1, "
				"\"tasks\": [{\"period_us\": 100, \"wcet_us\": "
				"10}]}]}", "has no other chain");
	check_application_error("{\"chains\": [{\"tasks\": [{\"period_us\": "
				"100, \"wcet_us\": 10, \"colocate_with_next\": "
				"true}]}]}", "has no next task");
	check_application_error("{\"chains\": [{\"tasks\": [{\"period_us\": "
				"100, \"wcet_us\": 10, \"core_types\": "
				"[\"dsp\"]}]}]}", "lists big, little");
}

/* the domain and distributed templates map, with sound constraints */
static void test_templates_map(void)
{
	static const char *const names[] = {"domain", "hybrid", "distributed"};
	struct mp_chain_array application;
	struct mp_gen_params generator;
	struct mp_design_point point;
	struct mp_ctx ctx;
	int n, i, k;

	for (n = 0; n < 3; n++) {
		mp_ctx_init(&ctx, 3);
		CHECK_EQ(mp_params_template(&ctx.params, names[n]), 0);
		ctx.params.algorithm = MP_FFDU;
		mp_gen_params_default(&generator);
		generator.chain_count = 80;
		CHECK_EQ(mp_generate_application(&ctx, &generator,
						 &application), 0);
		/* every task bound to an ECU of the platform */
		for (i = 0; i < application.count; i++)
			for (k = 0; k < application.items[i].tasks.count; k++) {
				int c = application.items[i].tasks.items[k].controller;

				CHECK(c == MP_CONTROLLER_ANY ||
				      (c >= 0 && c < mp_ecu_count(&ctx.params)));
			}
		CHECK_EQ(mp_run_design_point(&ctx, &application, NULL, &point),
			 0);
		CHECK(point.feasible);
		for (i = 0; i < point.cores.count; i++) {
			const struct mp_core *core = &point.cores.items[i];
			int max = mp_ecu_max_cores(&ctx.params, core->controller);
			int same = 0, j;

			CHECK(core->speed == mp_ecu_speed(&ctx.params,
							  core->controller,
							  core->type));
			for (j = 0; j < point.cores.count; j++)
				same += point.cores.items[j].controller ==
					core->controller;
			CHECK(!max || same <= max);
		}
		mp_design_point_free(&point);
		mp_chain_array_free(&application);
	}
}

int main(void)
{
	RUN_TEST(test_json_values);
	RUN_TEST(test_json_errors);
	RUN_TEST(test_templates_round_trip);
	RUN_TEST(test_hosts);
	RUN_TEST(test_zonal_template_is_builtin);
	RUN_TEST(test_configuration_errors);
	RUN_TEST(test_custom_routes);
	RUN_TEST(test_tree_root);
	RUN_TEST(test_application_round_trip);
	RUN_TEST(test_application_errors);
	RUN_TEST(test_templates_map);
	remove(TEMP);
	return unit_summary();
}
