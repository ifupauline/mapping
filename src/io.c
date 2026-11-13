// SPDX-License-Identifier: Apache-2.0
/* Application and configuration files (see io.h). */
#include <errno.h>
#include <math.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "mapping/io.h"
#include "mapping/json.h"
#include "mapping/network.h"

static const char *const type_names[MP_CORE_TYPES] = {
	"big", "little", "mcu", "lockstep",
};

static const char *const role_names[MP_ROLE_COUNT] = {
	"central", "domain", "zone", "ecu",
};

static const char *const algorithm_names[] = {
	"BFDU", "WFDU", "FFDU", "CADU",
};

static const char *const shaping_names[] = {"CBS", "TAS", "ATS"};

static const char *const topology_names[] = {
	"star", "tree", "ring", "custom",
};

const char *mp_core_type_name(int type)
{
	return type >= 0 && type < MP_CORE_TYPES ? type_names[type] : "?";
}

const char *mp_role_name(enum mp_ecu_role role)
{
	return (unsigned)role < MP_ROLE_COUNT ? role_names[role] : "?";
}

void mp_names_free(struct mp_names *names)
{
	free(names->chain);
	free(names->task);
	memset(names, 0, sizeof(*names));
}

/* ---- reading: checks with the place of the error ------------------------- */

struct reader {
	const char *path;
	char *error;
	size_t size;
};

static int bad(const struct reader *r, const struct mp_json *at,
	       const char *format, ...)
{
	char message[256];
	va_list args;

	va_start(args, format);
	vsnprintf(message, sizeof(message), format, args);
	va_end(args);
	snprintf(r->error, r->size, "%s:%d: %s", r->path, at ? at->line : 1,
		 message);
	return -EINVAL;
}

/* @object is an object whose keys are all in @keys (NULL-terminated) */
static int check_object(const struct reader *r, const struct mp_json *object,
			const char *where, const char *const *keys)
{
	int i, k;

	if (!object || object->type != MP_JSON_OBJECT)
		return bad(r, object, "%s must be an object", where);
	for (i = 0; i < object->count; i++) {
		for (k = 0; keys[k]; k++)
			if (!strcmp(object->keys[i], keys[k]))
				break;
		if (!keys[k])
			return bad(r, &object->items[i], "%s: unknown key \"%s\"",
				   where, object->keys[i]);
	}
	return 0;
}

/* an integer in [min, max]; absent: *value unchanged, or an error if
 * @required */
static int get_int(const struct reader *r, const struct mp_json *object,
		   const char *where, const char *key, long min, long max,
		   bool required, int *value)
{
	const struct mp_json *v = mp_json_get(object, key);

	if (!v) {
		if (required)
			return bad(r, object, "%s: \"%s\" is missing", where, key);
		return 0;
	}
	if (v->type != MP_JSON_NUMBER || v->number != floor(v->number))
		return bad(r, v, "%s: \"%s\" must be an integer", where, key);
	if (v->number < (double)min || v->number > (double)max)
		return bad(r, v, "%s: \"%s\" must be between %ld and %ld", where,
			   key, min, max);
	*value = (int)v->number;
	return 0;
}

/* a number in (min, max] (min excluded when @open_min), as ppm or
 * permille: *value = round(number * scale) */
static int get_fraction(const struct reader *r, const struct mp_json *object,
			const char *where, const char *key, double min,
			double max, bool open_min, double scale, int *value)
{
	const struct mp_json *v = mp_json_get(object, key);

	if (!v)
		return 0;
	if (v->type != MP_JSON_NUMBER)
		return bad(r, v, "%s: \"%s\" must be a number", where, key);
	if ((open_min ? v->number <= min : v->number < min) || v->number > max)
		return bad(r, v, "%s: \"%s\" must be %s %g and at most %g", where,
			   key, open_min ? "above" : "at least", min, max);
	*value = (int)lround(v->number * scale);
	return 0;
}

static int get_bool(const struct reader *r, const struct mp_json *object,
		    const char *where, const char *key, bool *value)
{
	const struct mp_json *v = mp_json_get(object, key);

	if (!v)
		return 0;
	if (v->type != MP_JSON_BOOL)
		return bad(r, v, "%s: \"%s\" must be true or false", where, key);
	*value = v->boolean;
	return 0;
}

/* a string, at most MP_NAME_SIZE - 1 bytes */
static int get_name(const struct reader *r, const struct mp_json *object,
		    const char *where, const char *key, bool required,
		    char *name)
{
	const struct mp_json *v = mp_json_get(object, key);

	if (!v) {
		if (required)
			return bad(r, object, "%s: \"%s\" is missing", where, key);
		return 0;
	}
	if (v->type != MP_JSON_STRING || !v->string[0])
		return bad(r, v, "%s: \"%s\" must be a non-empty string", where,
			   key);
	if (strlen(v->string) >= MP_NAME_SIZE)
		return bad(r, v, "%s: \"%s\" is longer than %d characters",
			   where, key, MP_NAME_SIZE - 1);
	strcpy(name, v->string);
	return 0;
}

/* a string among @names (@count of them): *value = its index */
static int get_choice(const struct reader *r, const struct mp_json *object,
		      const char *where, const char *key,
		      const char *const *names, int count, bool required,
		      int *value)
{
	const struct mp_json *v = mp_json_get(object, key);
	char list[160] = "";
	int i;

	if (!v) {
		if (required)
			return bad(r, object, "%s: \"%s\" is missing", where, key);
		return 0;
	}
	if (v->type == MP_JSON_STRING)
		for (i = 0; i < count; i++)
			if (!strcmp(v->string, names[i])) {
				*value = i;
				return 0;
			}
	for (i = 0; i < count; i++) {
		strncat(list, i ? ", " : "", sizeof(list) - strlen(list) - 1);
		strncat(list, names[i], sizeof(list) - strlen(list) - 1);
	}
	return bad(r, v, "%s: \"%s\" must be one of %s", where, key, list);
}

/* the mask of every ECU of the platform */
static unsigned all_ecus(const struct mp_params *params)
{
	int count = mp_ecu_count(params);

	return count >= 32 ? 0xffffffffu : (1u << count) - 1;
}

/* an array of ECU names into a mask (0 if it names every ECU) */
static int get_ecus(const struct reader *r, const struct mp_params *params,
		    const struct mp_json *object, const char *where,
		    const char *key, unsigned *mask)
{
	const struct mp_json *v = mp_json_get(object, key);
	int i, ecu;

	if (!v)
		return 0;
	if (v->type != MP_JSON_ARRAY || !v->count)
		return bad(r, v, "%s: \"%s\" must be a non-empty array of ECU "
			   "names", where, key);
	*mask = 0;
	for (i = 0; i < v->count; i++) {
		const struct mp_json *item = &v->items[i];

		if (item->type != MP_JSON_STRING)
			return bad(r, item, "%s: \"%s\" must list ECU names",
				   where, key);
		ecu = mp_find_ecu(params, item->string);
		if (ecu < 0)
			return bad(r, item, "%s: no ECU called \"%s\"", where,
				   item->string);
		*mask |= MP_CONTROLLER_BIT(ecu);
	}
	/* every ECU: no restriction (as if absent) */
	if (*mask == all_ecus(params))
		*mask = 0;
	return 0;
}

/* an array of core type names into a mask */
static int get_types(const struct reader *r, const struct mp_json *object,
		     const char *where, const char *key, unsigned *mask)
{
	const struct mp_json *v = mp_json_get(object, key);
	int i, t;

	if (!v)
		return 0;
	if (v->type != MP_JSON_ARRAY || !v->count)
		return bad(r, v, "%s: \"%s\" must be a non-empty array of core "
			   "types", where, key);
	*mask = 0;
	for (i = 0; i < v->count; i++) {
		const struct mp_json *item = &v->items[i];

		for (t = 0; t < MP_CORE_TYPES; t++)
			if (item->type == MP_JSON_STRING &&
			    !strcmp(item->string, type_names[t]))
				break;
		if (t == MP_CORE_TYPES)
			return bad(r, item, "%s: \"%s\" lists big, little, mcu "
				   "or lockstep", where, key);
		*mask |= 1u << t;
	}
	return 0;
}

/* an array of integers in [min, max], @count of them exactly */
static int get_int_list(const struct reader *r, const struct mp_json *object,
			const char *where, const char *key, int count,
			long min, long max, int *values)
{
	const struct mp_json *v = mp_json_get(object, key);
	int i;

	if (!v)
		return 0;
	if (v->type != MP_JSON_ARRAY || v->count != count)
		return bad(r, v, "%s: \"%s\" must be an array of %d integer%s",
			   where, key, count, count == 1 ? "" : "s");
	for (i = 0; i < count; i++) {
		const struct mp_json *item = &v->items[i];

		if (item->type != MP_JSON_NUMBER ||
		    item->number != floor(item->number) ||
		    item->number < (double)min || item->number > (double)max)
			return bad(r, item, "%s: \"%s\": integers between %ld "
				   "and %ld", where, key, min, max);
		values[i] = (int)item->number;
	}
	return 0;
}

/* a value that must be the single model the tool implements */
static int get_fixed(const struct reader *r, const struct mp_json *object,
		     const char *where, const char *key, const char *only)
{
	const struct mp_json *v = mp_json_get(object, key);

	if (v && (v->type != MP_JSON_STRING || strcmp(v->string, only)))
		return bad(r, v, "%s: \"%s\" can only be \"%s\"", where, key,
			   only);
	return 0;
}

/* ---- configuration --------------------------------------------------------- */

static int read_ecu(const struct reader *r, const struct mp_json *object,
		    struct mp_ecu *ecu)
{
	static const char *const keys[] = {
		"name", "role", "cores", "added_core_type", "memory_kb",
		"max_cores", NULL,
	};
	static const char *const core_keys[] = {
		"type", "count", "speed_percent", NULL,
	};
	const struct mp_json *cores;
	char where[64] = "platform: ECU";
	int role = MP_ROLE_ECU, added = -1, i, err;
	unsigned seen = 0;

	memset(ecu, 0, sizeof(*ecu));
	err = check_object(r, object, where, keys);
	if (!err)
		err = get_name(r, object, where, "name", true, ecu->name);
	if (err)
		return err;
	snprintf(where, sizeof(where), "ECU %s", ecu->name);
	err = get_choice(r, object, where, "role", role_names, MP_ROLE_COUNT,
			 true, &role);
	if (err)
		return err;
	ecu->role = (enum mp_ecu_role)role;
	cores = mp_json_get(object, "cores");
	if (!cores || cores->type != MP_JSON_ARRAY || !cores->count)
		return bad(r, cores ? cores : object, "%s: \"cores\" must be a "
			   "non-empty array of {type, count, speed_percent}",
			   where);
	for (i = 0; i < cores->count; i++) {
		const struct mp_json *group = &cores->items[i];
		int type = 0, count = 0, speed = 0;

		err = check_object(r, group, where, core_keys);
		if (!err)
			err = get_choice(r, group, where, "type", type_names,
					 MP_CORE_TYPES, true, &type);
		if (!err)
			err = get_int(r, group, where, "count", 0, 1024, true,
				      &count);
		if (!err)
			err = get_int(r, group, where, "speed_percent", 1, 10000,
				      true, &speed);
		if (err)
			return err;
		if (seen & (1u << type))
			return bad(r, group, "%s: two groups of %s cores", where,
				   type_names[type]);
		seen |= 1u << type;
		ecu->cores[type] = count;
		ecu->speed[type] = speed;
		if (added < 0)
			added = type;	/* default: the first type listed */
	}
	err = get_choice(r, object, where, "added_core_type", type_names,
			 MP_CORE_TYPES, false, &added);
	if (err)
		return err;
	if (!(seen & (1u << added)))
		return bad(r, mp_json_get(object, "added_core_type"),
			   "%s: \"added_core_type\" must be one of its core "
			   "types", where);
	ecu->added_type = added;
	err = get_int(r, object, where, "memory_kb", 0, 1L << 30, false,
		      &ecu->memory);
	if (!err)
		err = get_int(r, object, where, "max_cores", 0, 4096, false,
			      &ecu->max_cores);
	return err;
}

static int read_platform(const struct reader *r, const struct mp_json *object,
			 struct mp_params *params)
{
	static const char *const keys[] = {"ecus", NULL};
	const struct mp_json *ecus;
	char message[160];
	int i, err;

	err = check_object(r, object, "platform", keys);
	if (err)
		return err;
	ecus = mp_json_get(object, "ecus");
	if (!ecus || ecus->type != MP_JSON_ARRAY || !ecus->count ||
	    ecus->count > MP_MAX_ECUS)
		return bad(r, ecus ? ecus : object, "platform: \"ecus\" must be "
			   "an array of 1 to %d ECUs", MP_MAX_ECUS);
	memset(&params->platform, 0, sizeof(params->platform));
	for (i = 0; i < ecus->count; i++) {
		err = read_ecu(r, &ecus->items[i], &params->platform.ecus[i]);
		if (err)
			return err;
		params->platform.count = i + 1;
		if (mp_platform_check(params, message, sizeof(message)))
			return bad(r, &ecus->items[i], "platform: %s", message);
	}
	return 0;
}

/* "communication": "implicit" (the default) or "LET" (strict) */
static int read_communication(const struct reader *r,
			      const struct mp_json *object, const char *where,
			      struct mp_params *params)
{
	static const char *const names[] = {"implicit", "LET"};
	int value = params->network.strict_let;
	int err = get_choice(r, object, where, "communication", names, 2,
			     false, &value);

	params->network.strict_let = value == 1;
	return err;
}

static int read_execution(const struct reader *r, const struct mp_json *object,
			  struct mp_params *params)
{
	static const char *const keys[] = {
		"scheduling", "communication", "utilization_cap",
		"let_task_wcet_us", "wcet_margin_percent", NULL,
	};
	const char *where = "execution_model";
	int err;

	err = check_object(r, object, where, keys);
	if (!err)
		err = get_fixed(r, object, where, "scheduling",
				"fixed-priority preemptive");
	if (!err)
		err = read_communication(r, object, where, params);
	if (!err)
		err = get_fraction(r, object, where, "utilization_cap", 0, 1,
				   true, MP_UTILIZATION_UNIT, &params->fill_limit);
	if (!err)
		err = get_int(r, object, where, "let_task_wcet_us", 0, 1000000,
			      false, &params->let_wcet);
	if (!err)
		err = get_int(r, object, where, "wcet_margin_percent", 0, 1000,
			      false, &params->wcet_margin);
	return err;
}

static int read_interference(const struct reader *r,
			     const struct mp_json *object,
			     struct mp_params *params)
{
	static const char *const keys[] = {
		"shared_memory_arbitration", "memory_latency_ns",
		"memory_accesses_per_ms", NULL,
	};
	const char *where = "interference_model";
	int err;

	err = check_object(r, object, where, keys);
	if (!err)
		err = get_fixed(r, object, where, "shared_memory_arbitration",
				"round-robin");
	if (!err)
		err = get_int(r, object, where, "memory_latency_ns", 0, 100000,
			      false, &params->memory_latency);
	if (!err)
		err = get_int(r, object, where, "memory_accesses_per_ms", 0,
			      100000000, false, &params->access_density);
	return err;
}

/* the switch called @name, or -1 */
static int find_switch(const struct mp_net_config *net, const char *name)
{
	int s;

	for (s = 0; s < net->switch_count; s++)
		if (!strcmp(net->switch_names[s], name))
			return s;
	return -1;
}

/* the switches and the links of a custom topology */
static int read_custom(const struct reader *r, const struct mp_json *object,
		       struct mp_params *params)
{
	static const char *const switch_keys[] = {"name", "ecus", NULL};
	static const char *const link_keys[] = {"from", "to", "mbps", NULL};
	struct mp_net_config *net = &params->network;
	const struct mp_json *switches = mp_json_get(object, "switches");
	const struct mp_json *links = mp_json_get(object, "switch_links");
	const char *where = "network_model";
	unsigned attached = 0;
	int i, c, err;

	if (!switches || switches->type != MP_JSON_ARRAY || !switches->count ||
	    switches->count > MP_NET_MAX_SWITCHES)
		return bad(r, switches ? switches : object, "%s: a custom "
			   "topology needs \"switches\", 1 to %d of them",
			   where, MP_NET_MAX_SWITCHES);
	net->switch_count = 0;
	for (i = 0; i < switches->count; i++) {
		const struct mp_json *sw = &switches->items[i], *list;
		char *name = net->switch_names[i];
		unsigned ecus = 0;

		err = check_object(r, sw, "switch", switch_keys);
		if (!err)
			err = get_name(r, sw, "switch", "name", true, name);
		if (!err && (find_switch(net, name) >= 0 ||
			     mp_find_ecu(params, name) >= 0))
			err = bad(r, sw, "switch %s: the name is already used",
				  name);
		if (err)
			return err;
		list = mp_json_get(sw, "ecus");
		if (!list)
			return bad(r, sw, "switch %s: \"ecus\" is missing (an "
				   "empty list for a switch without ECUs)", name);
		/* a switch of the backbone may have no ECU */
		if (list->type != MP_JSON_ARRAY || list->count) {
			err = get_ecus(r, params, sw, name, "ecus", &ecus);
			if (err)
				return err;
			if (!ecus)	/* every ECU */
				ecus = all_ecus(params);
		}
		if (ecus & attached)
			return bad(r, sw, "switch %s: an ECU has one link, to "
				   "one switch", name);
		attached |= ecus;
		for (c = 0; c < MP_MAX_ECUS; c++)
			if (ecus & MP_CONTROLLER_BIT(c))
				net->ecu_switch[c] = i;
		net->switch_count = i + 1;
	}
	for (c = 0; c < mp_ecu_count(params); c++)
		if (!(attached & MP_CONTROLLER_BIT(c)))
			return bad(r, switches, "%s: ECU %s is linked to no switch",
				   where, mp_ecu_name(params, c));
	net->trunk_count = 0;
	if (!links && net->switch_count > 1)
		return bad(r, object, "%s: several switches need "
			   "\"switch_links\"", where);
	if (!links)
		return 0;
	if (links->type != MP_JSON_ARRAY || links->count > MP_NET_MAX_TRUNKS)
		return bad(r, links, "%s: \"switch_links\" must be an array of "
			   "at most %d links", where, MP_NET_MAX_TRUNKS);
	for (i = 0; i < links->count; i++) {
		const struct mp_json *link = &links->items[i];
		struct mp_net_trunk *t = &net->trunks[i];
		char from[MP_NAME_SIZE] = "", to[MP_NAME_SIZE] = "";
		int k;

		err = check_object(r, link, "switch link", link_keys);
		if (!err)
			err = get_name(r, link, "switch link", "from", true, from);
		if (!err)
			err = get_name(r, link, "switch link", "to", true, to);
		if (!err)
			err = get_int(r, link, "switch link", "mbps", 1, 1000000,
				      false, &t->mbps);
		if (err)
			return err;
		t->from = find_switch(net, from);
		t->to = find_switch(net, to);
		if (t->from < 0 || t->to < 0 || t->from == t->to)
			return bad(r, link, "switch link: \"from\" and \"to\" "
				   "must be two different switches");
		for (k = 0; k < i; k++)
			if ((net->trunks[k].from == t->from &&
			     net->trunks[k].to == t->to) ||
			    (net->trunks[k].from == t->to &&
			     net->trunks[k].to == t->from))
				return bad(r, link, "switch link: %s and %s are "
					   "already linked", from, to);
		net->trunk_count = i + 1;
	}
	/* every switch reachable from the first one */
	{
		bool reached[MP_NET_MAX_SWITCHES] = {true};
		bool grown = true;

		while (grown) {
			grown = false;
			for (i = 0; i < net->trunk_count; i++) {
				const struct mp_net_trunk *t = &net->trunks[i];

				if (reached[t->from] != reached[t->to]) {
					reached[t->from] = reached[t->to] = true;
					grown = true;
				}
			}
		}
		for (i = 0; i < net->switch_count; i++)
			if (!reached[i])
				return bad(r, links, "%s: switch %s has no path "
					   "to switch %s", where,
					   net->switch_names[i],
					   net->switch_names[0]);
	}
	return 0;
}

static int read_network(const struct reader *r, const struct mp_json *object,
			struct mp_params *params)
{
	static const char *const keys[] = {
		"standard", "topology", "tree_root", "switches", "switch_links",
		"link_mbps", "backbone_mbps", "ecu_link_mbps",
		"switch_latency_us", "link_limit_percent", "traffic_shaping",
		"traffic_classes", "class_max_periods_us",
		"idle_slopes_percent", "proportional_idle_slopes",
		"tas_cycle_us", "tas_windows_percent", "frame_preemption",
		"frer", NULL,
	};
	struct mp_net_config *net = &params->network;
	const char *where = "network_model";
	const struct mp_json *rates;
	char root[MP_NAME_SIZE] = "";
	int topology = net->topology, shaping = net->shaping;
	int classes = net->class_count, i, err;

	err = check_object(r, object, where, keys);
	if (!err)
		err = get_fixed(r, object, where, "standard", "TSN");
	if (!err)
		err = get_choice(r, object, where, "topology", topology_names, 4,
				 false, &topology);
	if (err)
		return err;
	net->topology = (enum mp_net_topology)topology;
	if (topology == MP_NET_CUSTOM) {
		err = read_custom(r, object, params);
		if (err)
			return err;
	} else if (mp_json_get(object, "switches") ||
		   mp_json_get(object, "switch_links")) {
		return bad(r, object, "%s: \"switches\" and \"switch_links\" "
			   "are for a custom topology", where);
	}
	err = get_name(r, object, where, "tree_root", false, root);
	if (err)
		return err;
	if (root[0]) {
		if (topology != MP_NET_TREE)
			return bad(r, mp_json_get(object, "tree_root"),
				   "%s: \"tree_root\" is for a tree", where);
		net->tree_root = mp_find_ecu(params, root);
		if (net->tree_root < 0)
			return bad(r, mp_json_get(object, "tree_root"),
				   "%s: no ECU called \"%s\"", where, root);
	}
	err = get_int(r, object, where, "link_mbps", 1, 1000000, false,
		      &net->link_mbps);
	if (!err)
		err = get_int(r, object, where, "backbone_mbps", 1, 1000000,
			      false, &net->backbone_mbps);
	if (err)
		return err;
	rates = mp_json_get(object, "ecu_link_mbps");
	if (rates) {
		if (rates->type != MP_JSON_OBJECT)
			return bad(r, rates, "%s: \"ecu_link_mbps\" must map ECU "
				   "names to rates", where);
		for (i = 0; i < rates->count; i++) {
			int ecu = mp_find_ecu(params, rates->keys[i]);
			const struct mp_json *v = &rates->items[i];

			if (ecu < 0)
				return bad(r, v, "%s: no ECU called \"%s\"",
					   where, rates->keys[i]);
			if (v->type != MP_JSON_NUMBER ||
			    v->number != floor(v->number) || v->number < 1 ||
			    v->number > 1000000)
				return bad(r, v, "%s: the rate of %s must be an "
					   "integer between 1 and 1000000", where,
					   rates->keys[i]);
			net->ecu_mbps[ecu] = (int)v->number;
		}
	}
	err = get_int(r, object, where, "switch_latency_us", 0, 1000000, false,
		      &net->switch_latency);
	if (!err)
		err = get_int(r, object, where, "link_limit_percent", 1, 100,
			      false, &net->link_limit_percent);
	if (!err)
		err = get_choice(r, object, where, "traffic_shaping",
				 shaping_names, 3, false, &shaping);
	if (!err)
		err = get_int(r, object, where, "traffic_classes", 1,
			      MP_NET_MAX_CLASSES, false, &classes);
	if (err)
		return err;
	net->shaping = (enum mp_net_shaping)shaping;
	/* the default boundaries and shares of that many classes first */
	if (classes != net->class_count)
		mp_net_set_classes(net, classes);
	err = get_int_list(r, object, where, "class_max_periods_us",
			   classes - 1, 1, 100000000, net->class_period);
	if (!err)
		err = get_int_list(r, object, where, "idle_slopes_percent",
				   classes, 1, 100, net->idle_slope_percent);
	if (!err)
		err = get_bool(r, object, where, "proportional_idle_slopes",
			       &net->proportional_slopes);
	if (!err)
		err = get_int(r, object, where, "tas_cycle_us", 1, 100000000,
			      false, &net->tas_cycle);
	if (!err)
		err = get_int_list(r, object, where, "tas_windows_percent",
				   classes, 1, 100, net->tas_window_percent);
	if (!err)
		err = get_bool(r, object, where, "frame_preemption",
			       &net->preemption);
	if (!err)
		err = get_bool(r, object, where, "frer", &net->frer);
	return err;
}

static int read_method(const struct reader *r, const struct mp_json *object,
		       struct mp_params *params)
{
	static const char *const keys[] = {
		"allocation", "sigma", "affinity", "consolidation",
		"swap_moves", "relocation_moves", "local_search",
		"replica_separation", "fixed_hardware", "target_scaling", NULL,
	};
	static const char *const separations[] = {"ecu", "core"};
	const char *where = "method";
	int algorithm = params->algorithm - MP_BFDU;
	int separation = params->separation == MP_SEPARATE_CORES;
	int err;

	err = check_object(r, object, where, keys);
	if (!err)
		err = get_choice(r, object, where, "allocation", algorithm_names,
				 4, false, &algorithm);
	if (!err)
		err = get_fraction(r, object, where, "sigma", 0, 1, true,
				   MP_UTILIZATION_UNIT, &params->sigma);
	if (!err)
		err = get_bool(r, object, where, "affinity", &params->affinity);
	if (!err)
		err = get_bool(r, object, where, "consolidation",
			       &params->consolidate);
	if (!err)
		err = get_bool(r, object, where, "swap_moves",
			       &params->swap_moves);
	if (!err)
		err = get_bool(r, object, where, "relocation_moves",
			       &params->relocation_moves);
	if (!err)
		err = get_bool(r, object, where, "local_search",
			       &params->repair_deadlines);
	if (!err)
		err = get_choice(r, object, where, "replica_separation",
				 separations, 2, false, &separation);
	if (!err)
		err = get_bool(r, object, where, "fixed_hardware",
			       &params->fixed_hardware);
	if (!err)
		err = get_fraction(r, object, where, "target_scaling", 0, 100,
				   false, 1000, &params->target_scaling);
	if (err)
		return err;
	params->algorithm = (enum mp_algorithm)(algorithm + MP_BFDU);
	params->separation = separation ? MP_SEPARATE_CORES :
			     MP_SEPARATE_CONTROLLERS;
	if (params->sigma > params->fill_limit)
		return bad(r, mp_json_get(object, "sigma"), "method: sigma must "
			   "not exceed the utilization cap");
	return 0;
}

/* the network follows the platform: its ECUs, the central computers on
 * the backbone, the first one at the root of a tree */
static void network_of_platform(struct mp_params *params)
{
	int order[MP_MAX_ECUS], c;

	params->network.ecu_count = mp_ecu_count(params);
	params->network.backbone_ecus = 0;
	for (c = 0; c < mp_ecu_count(params); c++)
		if (mp_ecu_is_central(params, c))
			params->network.backbone_ecus |= 1u << c;
	mp_ecu_preference(params, order);
	params->network.tree_root = order[0];
}

/* the text of @path, parsed */
static int parse_file(const struct reader *r, struct mp_json *root)
{
	char message[200];
	char *text;
	int err;

	err = mp_read_file(r->path, &text);
	if (err) {
		snprintf(r->error, r->size, "%s: %s", r->path,
			 err == -EILSEQ ? "a NUL byte in the file" :
			 strerror(-err));
		return err;
	}
	err = mp_json_parse(text, root, message, sizeof(message));
	free(text);
	if (err)
		snprintf(r->error, r->size, "%s: %s", r->path, message);
	return err;
}

int mp_read_configuration(const char *path, struct mp_params *params,
			  char *error, size_t size)
{
	static const char *const keys[] = {
		"description", "platform", "execution_model",
		"interference_model", "network_model", "method", NULL,
	};
	struct reader r = {path, error, size};
	const struct mp_json *section;
	struct mp_json root;
	int err;

	mp_params_default(params);
	err = parse_file(&r, &root);
	if (err)
		return err;
	err = check_object(&r, &root, "the configuration", keys);
	section = mp_json_get(&root, "description");
	if (!err && section && section->type != MP_JSON_STRING)
		err = bad(&r, section, "\"description\" must be a string");
	if (!err && (section = mp_json_get(&root, "platform")))
		err = read_platform(&r, section, params);
	if (!err)
		network_of_platform(params);
	if (!err && (section = mp_json_get(&root, "execution_model")))
		err = read_execution(&r, section, params);
	if (!err && (section = mp_json_get(&root, "interference_model")))
		err = read_interference(&r, section, params);
	if (!err && (section = mp_json_get(&root, "network_model")))
		err = read_network(&r, section, params);
	if (!err && (section = mp_json_get(&root, "method")))
		err = read_method(&r, section, params);
	if (!err && mp_net_config_check(&params->network))
		err = bad(&r, mp_json_get(&root, "network_model"),
			  "network_model: inconsistent network (shares above "
			  "100 %%, class periods not increasing, two ECUs "
			  "without a route or a route longer than %d ports, or "
			  "FRER on a star or a tree)", MP_NET_MAX_ROUTE);
	mp_json_free(&root);
	return err;
}

/* ---- application --------------------------------------------------------- */

static int read_task(const struct reader *r, struct mp_params *params,
		     const struct mp_json *object, const char *chain_name,
		     int index, struct mp_task *task, char *name)
{
	static const char *const keys[] = {
		"name", "period_us", "wcet_us", "ecu", "ecus", "core_types",
		"wcet_factor_percent", "memory_kb", "message_bytes",
		"memory_accesses", "colocate_with_next", NULL,
	};
	const struct mp_json *factors;
	char where[96], ecu[MP_NAME_SIZE] = "";
	int i, t, err;

	memset(task, 0, sizeof(*task));
	snprintf(where, sizeof(where), "chain %s, task %d", chain_name, index);
	err = check_object(r, object, where, keys);
	if (!err)
		err = get_name(r, object, where, "name", false, name);
	if (err)
		return err;
	if (name[0])
		snprintf(where, sizeof(where), "chain %s, task %s", chain_name,
			 name);
	err = get_int(r, object, where, "period_us", 1, 1000000000, true,
		      &task->period);
	if (!err)
		err = get_int(r, object, where, "wcet_us", 1, 1000000000, true,
			      &task->wcet);
	if (err)
		return err;
	if (task->wcet > task->period)
		return bad(r, mp_json_get(object, "wcet_us"), "%s: the WCET "
			   "exceeds the period", where);
	task->reference_wcet = task->wcet;
	task->utilization = mp_utilization(task->wcet, task->period);
	task->controller = MP_CONTROLLER_ANY;
	err = get_name(r, object, where, "ecu", false, ecu);
	if (err)
		return err;
	if (ecu[0]) {
		task->controller = mp_find_ecu(params, ecu);
		if (task->controller < 0)
			return bad(r, mp_json_get(object, "ecu"), "%s: no ECU "
				   "called \"%s\"", where, ecu);
		if (mp_json_get(object, "ecus"))
			return bad(r, mp_json_get(object, "ecus"), "%s: \"ecu\" "
				   "and \"ecus\" exclude each other", where);
	}
	err = get_ecus(r, params, object, where, "ecus", &task->allowed);
	if (!err)
		err = get_types(r, object, where, "core_types", &task->types);
	if (err)
		return err;
	factors = mp_json_get(object, "wcet_factor_percent");
	if (factors) {
		if (factors->type != MP_JSON_OBJECT)
			return bad(r, factors, "%s: \"wcet_factor_percent\" maps "
				   "core types to percentages", where);
		for (i = 0; i < factors->count; i++) {
			const struct mp_json *v = &factors->items[i];

			for (t = 0; t < MP_CORE_TYPES; t++)
				if (!strcmp(factors->keys[i], type_names[t]))
					break;
			if (t == MP_CORE_TYPES)
				return bad(r, v, "%s: \"wcet_factor_percent\": "
					   "unknown core type \"%s\"", where,
					   factors->keys[i]);
			if (v->type != MP_JSON_NUMBER ||
			    v->number != floor(v->number) || v->number < 1 ||
			    v->number > 10000)
				return bad(r, v, "%s: a WCET factor is an "
					   "integer between 1 and 10000", where);
			task->type_factor[t] = (short)v->number;
		}
	}
	err = get_int(r, object, where, "memory_kb", 0, 1L << 30, false,
		      &task->memory);
	if (!err)
		err = get_int(r, object, where, "message_bytes", 0, 1L << 24,
			      false, &task->message_bytes);
	if (!err)
		err = get_int(r, object, where, "memory_accesses", 0,
			      1000000000, false, &task->accesses);
	if (!err)
		err = get_bool(r, object, where, "colocate_with_next",
			       &task->colocate_next);
	if (err)
		return err;
	task->id_in_chain = index;
	task->priority = index + 1;
	return 0;
}

/* names[count] = name, growing the array */
static int push_name(char (**names)[MP_NAME_SIZE], int *count, int *capacity,
		     const char *name)
{
	if (*count == *capacity) {
		int bigger = *capacity ? 2 * *capacity : 16;
		char (*grown)[MP_NAME_SIZE] = realloc(*names, (size_t)bigger *
						      sizeof(**names));

		if (!grown)
			return -ENOMEM;
		*names = grown;
		*capacity = bigger;
	}
	strcpy((*names)[(*count)++], name);
	return 0;
}

static int read_chain(const struct reader *r, struct mp_params *params,
		      const struct mp_json *object, int index,
		      struct mp_chain *chain, struct mp_names *names,
		      int *task_capacity)
{
	static const char *const keys[] = {
		"name", "deadline_us", "ecu", "ecus", "replica_group", "tasks",
		NULL,
	};
	const struct mp_json *tasks;
	char where[64], name[MP_NAME_SIZE] = "", ecu[MP_NAME_SIZE] = "";
	unsigned allowed = 0;
	int k, err;

	memset(chain, 0, sizeof(*chain));
	chain->controller = MP_CONTROLLER_ANY;
	snprintf(where, sizeof(where), "chain %d", index);
	err = check_object(r, object, where, keys);
	if (!err)
		err = get_name(r, object, where, "name", false, name);
	if (err)
		return err;
	if (!name[0])
		snprintf(name, sizeof(name), "%d", index);
	snprintf(where, sizeof(where), "chain %s", name);
	err = get_int(r, object, where, "deadline_us", 0, 2000000000, false,
		      &chain->deadline);
	if (!err)
		err = get_int(r, object, where, "replica_group", 0, 1000000000,
			      false, &chain->replica_group);
	if (!err)
		err = get_name(r, object, where, "ecu", false, ecu);
	if (err)
		return err;
	if (ecu[0]) {
		chain->controller = mp_find_ecu(params, ecu);
		if (chain->controller < 0)
			return bad(r, mp_json_get(object, "ecu"), "%s: no ECU "
				   "called \"%s\"", where, ecu);
		if (mp_json_get(object, "ecus"))
			return bad(r, mp_json_get(object, "ecus"), "%s: \"ecu\" "
				   "and \"ecus\" exclude each other", where);
	}
	err = get_ecus(r, params, object, where, "ecus", &allowed);
	if (err)
		return err;
	tasks = mp_json_get(object, "tasks");
	if (!tasks || tasks->type != MP_JSON_ARRAY || !tasks->count)
		return bad(r, tasks ? tasks : object, "%s: \"tasks\" must be a "
			   "non-empty array", where);
	for (k = 0; k < tasks->count; k++) {
		char task_name[MP_NAME_SIZE] = "";
		struct mp_task task;

		err = read_task(r, params, &tasks->items[k], name, k, &task,
				task_name);
		if (err)
			goto err_free;
		/* a chain on one ECU: its tasks there (a local chain) */
		if (chain->controller != MP_CONTROLLER_ANY) {
			if (task.controller != MP_CONTROLLER_ANY &&
			    task.controller != chain->controller) {
				err = bad(r, &tasks->items[k], "%s: a task on "
					  "another ECU than its chain", where);
				goto err_free;
			}
			task.controller = chain->controller;
		}
		if (allowed) {
			unsigned own = mp_task_allowed(&task) & allowed;

			if (!own) {
				err = bad(r, &tasks->items[k], "%s: a task on no "
					  "ECU the chain allows", where);
				goto err_free;
			}
			if (task.controller == MP_CONTROLLER_ANY)
				task.allowed = own;
		}
		if (k == tasks->count - 1 && task.colocate_next) {
			err = bad(r, &tasks->items[k], "%s: the last task has no "
				  "next task", where);
			goto err_free;
		}
		err = mp_task_array_push(&chain->tasks, &task);
		if (!err && names)
			err = push_name(&names->task, &names->task_count,
					task_capacity, task_name);
		if (err)
			goto err_free;
		chain->utilization += task.utilization;
		chain->memory += task.memory;
	}
	/* co-located tasks share one core, so all their constraints */
	if (mp_chain_colocate(chain, &k)) {
		err = bad(r, &tasks->items[k], "%s: tasks that must share a "
			  "core (\"colocate_with_next\") have no ECU or core "
			  "type in common", where);
		goto err_free;
	}
	if (names) {
		/* one more chain name (the array grows one by one) */
		char (*grown)[MP_NAME_SIZE] = realloc(names->chain,
			(size_t)(names->chain_count + 1) * sizeof(*names->chain));

		if (!grown) {
			err = -ENOMEM;
			goto err_free;
		}
		names->chain = grown;
		strcpy(names->chain[names->chain_count++], name);
	}
	return 0;

err_free:
	mp_chain_free(chain);
	return err;
}

int mp_read_application(const char *path, struct mp_params *params,
			struct mp_chain_array *application,
			struct mp_names *names, char *error, size_t size)
{
	static const char *const keys[] = {"description", "chains", NULL};
	struct reader r = {path, error, size};
	const struct mp_json *chains, *description;
	struct mp_json root;
	int task_capacity = 0, unique_id = 0, i, j, err;

	memset(application, 0, sizeof(*application));
	if (names)
		memset(names, 0, sizeof(*names));
	err = parse_file(&r, &root);
	if (err)
		return err;
	err = check_object(&r, &root, "the application", keys);
	description = mp_json_get(&root, "description");
	if (!err && description && description->type != MP_JSON_STRING)
		err = bad(&r, description, "\"description\" must be a string");
	chains = mp_json_get(&root, "chains");
	if (!err && (!chains || chains->type != MP_JSON_ARRAY ||
		     !chains->count))
		err = bad(&r, chains ? chains : &root, "\"chains\" must be a "
			  "non-empty array of task-chains");
	for (i = 0; !err && i < chains->count; i++) {
		struct mp_chain chain;

		err = read_chain(&r, params, &chains->items[i], i, &chain, names,
				 &task_capacity);
		if (err)
			break;
		err = mp_chain_array_push(application, &chain);
		mp_chain_free(&chain);
	}
	/* replica groups need two chains, and distinct chain names */
	for (i = 0; !err && i < application->count; i++) {
		int group = application->items[i].replica_group, mates = 0;

		for (j = 0; j < application->count; j++)
			mates += j != i && group &&
				 application->items[j].replica_group == group;
		if (group && !mates)
			err = bad(&r, &chains->items[i], "chain %d: replica group "
				  "%d has no other chain", i, group);
		if (!err && names)
			for (j = 0; j < i; j++)
				if (!strcmp(names->chain[i], names->chain[j]))
					err = bad(&r, &chains->items[i], "two "
						  "chains are called %s",
						  names->chain[i]);
	}
	mp_json_free(&root);
	if (err) {
		if (err == -ENOMEM)
			snprintf(error, size, "%s: out of memory", path);
		mp_chain_array_free(application);
		if (names)
			mp_names_free(names);
		return err;
	}
	/* numbered as the generator numbers its applications */
	for (i = 0; i < application->count; i++) {
		struct mp_chain *chain = &application->items[i];

		chain->id = i;
		for (j = 0; j < chain->tasks.count; j++) {
			chain->tasks.items[j].chain_id = i;
			chain->tasks.items[j].unique_id = unique_id++;
		}
		chain->period_gcd = mp_tasks_period_gcd(&chain->tasks);
	}
	params->chain_count = application->count;
	return 0;
}

/* ---- writing ------------------------------------------------------------- */

/* a JSON string (names are plain: escape only what JSON requires) */
static void write_string(FILE *out, const char *text)
{
	fputc('"', out);
	for (; *text; text++) {
		unsigned char c = (unsigned char)*text;

		if (c == '"' || c == '\\')
			fprintf(out, "\\%c", c);
		else if (c < 0x20)
			fprintf(out, "\\u%04x", c);
		else
			fputc(c, out);
	}
	fputc('"', out);
}

static void write_int_list(FILE *out, const int *values, int count)
{
	int i;

	fputc('[', out);
	for (i = 0; i < count; i++)
		fprintf(out, "%s%d", i ? ", " : "", values[i]);
	fputc(']', out);
}

static void write_ecu_list(FILE *out, const struct mp_params *params,
			   unsigned mask)
{
	int c, n = 0;

	fputc('[', out);
	for (c = 0; c < mp_ecu_count(params); c++)
		if (mask & MP_CONTROLLER_BIT(c)) {
			fputs(n++ ? ", " : "", out);
			write_string(out, mp_ecu_name(params, c));
		}
	fputc(']', out);
}

void mp_write_configuration(FILE *out, const struct mp_params *params)
{
	const struct mp_net_config *net = &params->network;
	int c, t, n;

	fprintf(out, "{\n  \"platform\": {\n    \"ecus\": [\n");
	for (c = 0; c < mp_ecu_count(params); c++) {
		struct mp_ecu ecu;

		mp_ecu_get(params, c, &ecu);
		fprintf(out, "      {\"name\": ");
		write_string(out, ecu.name);
		fprintf(out, ", \"role\": \"%s\",\n       \"cores\": [",
			role_names[ecu.role]);
		for (t = 0, n = 0; t < MP_CORE_TYPES; t++) {
			if (!ecu.speed[t])
				continue;
			fprintf(out, "%s{\"type\": \"%s\", \"count\": %d, "
				"\"speed_percent\": %d}",
				n++ ? ",\n                 " : "", type_names[t],
				ecu.cores[t], ecu.speed[t]);
		}
		fprintf(out, "],\n       \"added_core_type\": \"%s\", "
			"\"memory_kb\": %d, \"max_cores\": %d}%s\n",
			type_names[ecu.added_type], ecu.memory, ecu.max_cores,
			c + 1 < mp_ecu_count(params) ? "," : "");
	}
	fprintf(out, "    ]\n  },\n");

	fprintf(out, "  \"execution_model\": {\n"
		"    \"scheduling\": \"fixed-priority preemptive\",\n"
		"    \"communication\": \"%s\",\n"
		"    \"utilization_cap\": %g,\n"
		"    \"let_task_wcet_us\": %d,\n"
		"    \"wcet_margin_percent\": %d\n  },\n",
		params->network.strict_let ? "LET" : "implicit",
		params->fill_limit / (double)MP_UTILIZATION_UNIT,
		params->let_wcet, params->wcet_margin);

	fprintf(out, "  \"interference_model\": {\n"
		"    \"shared_memory_arbitration\": \"round-robin\",\n"
		"    \"memory_latency_ns\": %d,\n"
		"    \"memory_accesses_per_ms\": %d\n  },\n",
		params->memory_latency, params->access_density);

	fprintf(out, "  \"network_model\": {\n    \"standard\": \"TSN\",\n"
		"    \"topology\": \"%s\",\n", topology_names[net->topology]);
	if (net->topology == MP_NET_TREE)
		fprintf(out, "    \"tree_root\": \"%s\",\n",
			mp_ecu_name(params, net->tree_root));
	if (net->topology == MP_NET_CUSTOM) {
		fprintf(out, "    \"switches\": [\n");
		for (t = 0; t < net->switch_count; t++) {
			unsigned mask = 0;

			for (c = 0; c < mp_ecu_count(params); c++)
				if (net->ecu_switch[c] == t)
					mask |= MP_CONTROLLER_BIT(c);
			fprintf(out, "      {\"name\": ");
			write_string(out, net->switch_names[t]);
			fprintf(out, ", \"ecus\": ");
			write_ecu_list(out, params, mask);
			fprintf(out, "}%s\n", t + 1 < net->switch_count ? "," : "");
		}
		fprintf(out, "    ],\n    \"switch_links\": [\n");
		for (t = 0; t < net->trunk_count; t++) {
			fprintf(out, "      {\"from\": ");
			write_string(out, net->switch_names[net->trunks[t].from]);
			fprintf(out, ", \"to\": ");
			write_string(out, net->switch_names[net->trunks[t].to]);
			if (net->trunks[t].mbps)
				fprintf(out, ", \"mbps\": %d", net->trunks[t].mbps);
			fprintf(out, "}%s\n", t + 1 < net->trunk_count ? "," : "");
		}
		fprintf(out, "    ],\n");
	}
	fprintf(out, "    \"link_mbps\": %d,\n    \"backbone_mbps\": %d,\n",
		net->link_mbps, net->backbone_mbps ? net->backbone_mbps :
		net->link_mbps);
	for (c = 0, n = 0; c < mp_ecu_count(params); c++)
		if (net->ecu_mbps[c]) {
			fprintf(out, n++ ? ", " : "    \"ecu_link_mbps\": {");
			write_string(out, mp_ecu_name(params, c));
			fprintf(out, ": %d", net->ecu_mbps[c]);
		}
	if (n)
		fprintf(out, "},\n");
	fprintf(out, "    \"switch_latency_us\": %d,\n"
		"    \"link_limit_percent\": %d,\n"
		"    \"traffic_shaping\": \"%s\",\n"
		"    \"traffic_classes\": %d,\n"
		"    \"class_max_periods_us\": ",
		net->switch_latency, net->link_limit_percent,
		shaping_names[net->shaping], net->class_count);
	write_int_list(out, net->class_period, net->class_count - 1);
	fprintf(out, ",\n    \"idle_slopes_percent\": ");
	write_int_list(out, net->idle_slope_percent, net->class_count);
	fprintf(out, ",\n    \"proportional_idle_slopes\": %s,\n"
		"    \"tas_cycle_us\": %d,\n    \"tas_windows_percent\": ",
		net->proportional_slopes ? "true" : "false", net->tas_cycle);
	write_int_list(out, net->tas_window_percent, net->class_count);
	fprintf(out, ",\n    \"frame_preemption\": %s,\n"
		"    \"frer\": %s\n  },\n",
		net->preemption ? "true" : "false",
		net->frer ? "true" : "false");

	fprintf(out, "  \"method\": {\n"
		"    \"allocation\": \"%s\",\n"
		"    \"sigma\": %g,\n"
		"    \"affinity\": %s,\n"
		"    \"consolidation\": %s,\n"
		"    \"swap_moves\": %s,\n"
		"    \"relocation_moves\": %s,\n"
		"    \"local_search\": %s,\n"
		"    \"replica_separation\": \"%s\",\n"
		"    \"fixed_hardware\": %s,\n"
		"    \"target_scaling\": %g\n  }\n}\n",
		algorithm_names[params->algorithm - MP_BFDU],
		params->sigma / (double)MP_UTILIZATION_UNIT,
		params->affinity ? "true" : "false",
		params->consolidate ? "true" : "false",
		params->swap_moves ? "true" : "false",
		params->relocation_moves ? "true" : "false",
		params->repair_deadlines ? "true" : "false",
		params->separation == MP_SEPARATE_CORES ? "core" : "ecu",
		params->fixed_hardware ? "true" : "false",
		params->target_scaling / 1000.0);
}

void mp_write_application(FILE *out, const struct mp_params *params,
			  const struct mp_chain_array *application,
			  const struct mp_names *names)
{
	int i, k, t;

	fprintf(out, "{\n  \"chains\": [\n");
	for (i = 0; i < application->count; i++) {
		const struct mp_chain *chain = &application->items[i];

		fprintf(out, "    {\"name\": ");
		if (names && chain->id < names->chain_count &&
		    names->chain[chain->id][0])
			write_string(out, names->chain[chain->id]);
		else
			fprintf(out, "\"chain%d\"", chain->id);
		if (chain->deadline)
			fprintf(out, ", \"deadline_us\": %d", chain->deadline);
		if (chain->replica_group)
			fprintf(out, ", \"replica_group\": %d",
				chain->replica_group);
		if (chain->controller != MP_CONTROLLER_ANY) {
			fprintf(out, ", \"ecu\": ");
			write_string(out, mp_ecu_name(params, chain->controller));
		}
		if (chain->allowed &&
		    (chain->allowed & all_ecus(params)) != all_ecus(params)) {
			fprintf(out, ", \"ecus\": ");
			write_ecu_list(out, params, chain->allowed);
		}
		fprintf(out, ",\n     \"tasks\": [\n");
		for (k = 0; k < chain->tasks.count; k++) {
			const struct mp_task *task = &chain->tasks.items[k];
			bool comma = false;

			fprintf(out, "       {");
			if (names && task->unique_id >= 0 &&
			    task->unique_id < names->task_count &&
			    names->task[task->unique_id][0]) {
				fprintf(out, "\"name\": ");
				write_string(out, names->task[task->unique_id]);
				fprintf(out, ", ");
			}
			fprintf(out, "\"period_us\": %d, \"wcet_us\": %d",
				task->period, task->reference_wcet ?
				task->reference_wcet : task->wcet);
			if (task->controller != MP_CONTROLLER_ANY &&
			    chain->controller == MP_CONTROLLER_ANY) {
				fprintf(out, ", \"ecu\": ");
				write_string(out, mp_ecu_name(params,
							      task->controller));
			}
			if (task->allowed && task->controller ==
			    MP_CONTROLLER_ANY &&
			    (task->allowed & all_ecus(params)) !=
			    all_ecus(params)) {
				fprintf(out, ", \"ecus\": ");
				write_ecu_list(out, params, task->allowed);
			}
			if (task->types) {
				fprintf(out, ", \"core_types\": [");
				for (t = 0; t < MP_CORE_TYPES; t++)
					if (task->types & (1u << t)) {
						fprintf(out, "%s\"%s\"", comma ?
							", " : "", type_names[t]);
						comma = true;
					}
				fprintf(out, "]");
			}
			comma = false;
			for (t = 0; t < MP_CORE_TYPES; t++)
				if (task->type_factor[t]) {
					fprintf(out, "%s\"%s\": %d", comma ? ", " :
						", \"wcet_factor_percent\": {",
						type_names[t], task->type_factor[t]);
					comma = true;
				}
			if (comma)
				fprintf(out, "}");
			if (task->memory)
				fprintf(out, ", \"memory_kb\": %d", task->memory);
			fprintf(out, ", \"message_bytes\": %d",
				task->message_bytes);
			if (task->accesses)
				fprintf(out, ", \"memory_accesses\": %d",
					task->accesses);
			if (task->colocate_next)
				fprintf(out, ", \"colocate_with_next\": true");
			fprintf(out, "}%s\n", k + 1 < chain->tasks.count ? "," :
				"");
		}
		fprintf(out, "     ]}%s\n", i + 1 < application->count ? "," : "");
	}
	fprintf(out, "  ]\n}\n");
}

/* ---- templates ----------------------------------------------------------- */

/* an ECU with up to two groups of cores */
static void set_ecu(struct mp_ecu *ecu, const char *name,
		    enum mp_ecu_role role, int type1, int count1, int speed1,
		    int type2, int count2, int speed2, int max_cores)
{
	memset(ecu, 0, sizeof(*ecu));
	snprintf(ecu->name, sizeof(ecu->name), "%s", name);
	ecu->role = role;
	ecu->cores[type1] = count1;
	ecu->speed[type1] = speed1;
	if (type2 >= 0) {
		ecu->cores[type2] = count2;
		ecu->speed[type2] = speed2;
	}
	ecu->added_type = type1;
	ecu->max_cores = max_cores;
}

int mp_params_template(struct mp_params *params, const char *name)
{
	struct mp_platform *p = &params->platform;

	mp_params_default(params);
	if (!strcmp(name, "zonal")) {
		mp_platform_zonal(params, p);
	} else if (!strcmp(name, "domain")) {
		/*
		 * one domain controller per vehicle domain; the safety domains
		 * (powertrain, chassis) have lock-step cores; the ADAS and
		 * infotainment controllers are application processors
		 */
		p->count = 5;
		set_ecu(&p->ecus[0], "ADAS", MP_ROLE_DOMAIN, MP_CORE_BIG, 4, 100,
			-1, 0, 0, 0);
		set_ecu(&p->ecus[1], "Infotainment", MP_ROLE_DOMAIN, MP_CORE_BIG,
			2, 100, MP_CORE_LITTLE, 4, 40, 0);
		set_ecu(&p->ecus[2], "Powertrain", MP_ROLE_DOMAIN, MP_CORE_MCU,
			3, 50, MP_CORE_LOCKSTEP, 1, 50, 0);
		set_ecu(&p->ecus[3], "Chassis", MP_ROLE_DOMAIN, MP_CORE_MCU, 2,
			50, MP_CORE_LOCKSTEP, 2, 50, 0);
		set_ecu(&p->ecus[4], "Body", MP_ROLE_DOMAIN, MP_CORE_MCU, 2, 50,
			-1, 0, 0, 0);
	} else if (!strcmp(name, "hybrid")) {
		/*
		 * a zonal platform that keeps a domain controller for ADAS:
		 * a central computer, the ADAS controller, four zone
		 * controllers (front and rear, left and right)
		 */
		static const char *const zones[] = {
			"ZCU_FL", "ZCU_FR", "ZCU_RL", "ZCU_RR",
		};
		int i;

		p->count = 6;
		set_ecu(&p->ecus[0], "PCU", MP_ROLE_CENTRAL, MP_CORE_BIG, 8, 100,
			-1, 0, 0, 0);
		set_ecu(&p->ecus[1], "ADAS", MP_ROLE_DOMAIN, MP_CORE_BIG, 4, 100,
			-1, 0, 0, 0);
		for (i = 0; i < 4; i++)
			set_ecu(&p->ecus[2 + i], zones[i], MP_ROLE_ZONE,
				MP_CORE_MCU, 4, 50, -1, 0, 0, 0);
	} else if (!strcmp(name, "distributed")) {
		/*
		 * one ECU per function, mostly single-core microcontrollers,
		 * lock-step ones for the brakes and the steering, at most two
		 * cores each (four for the camera ECU)
		 */
		static const char *const mcus[] = {
			"ECM", "TCM", "BCM", "HVAC", "IPC", "DOOR_FL", "DOOR_FR",
			"DOOR_RL", "DOOR_RR",
		};
		int i;

		p->count = 12;
		set_ecu(&p->ecus[0], "ADAS", MP_ROLE_ECU, MP_CORE_BIG, 2, 100,
			-1, 0, 0, 4);
		set_ecu(&p->ecus[1], "ESC", MP_ROLE_ECU, MP_CORE_LOCKSTEP, 1, 50,
			-1, 0, 0, 2);
		set_ecu(&p->ecus[2], "EPS", MP_ROLE_ECU, MP_CORE_LOCKSTEP, 1, 50,
			-1, 0, 0, 2);
		for (i = 0; i < 9; i++)
			set_ecu(&p->ecus[3 + i], mcus[i], MP_ROLE_ECU,
				MP_CORE_MCU, 1, 50, -1, 0, 0, 2);
		params->network.link_mbps = 100;	/* 100BASE-T1 */
	} else {
		return -EINVAL;
	}
	network_of_platform(params);
	return 0;
}
