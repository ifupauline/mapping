// SPDX-License-Identifier: Apache-2.0
/* Design-point context (see context.h). */
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "mapping/context.h"
#include "mapping/let.h"
#include "mapping/model.h"

void mp_params_default(struct mp_params *params)
{
	memset(params, 0, sizeof(*params));
	params->algorithm = MP_FFDU;
	params->sigma = 400000;
	params->pcu_cores = 8;
	params->zcu_cores = 4;
	params->pcu_speed = MP_REFERENCE_SPEED;
	/* zone controllers are microcontrollers, about half as fast */
	params->zcu_speed = 50;
	params->little_speed = 40;
	params->fill_limit = MP_FILL_LIMIT;
	params->let_wcet = MP_LET_WCET;
	mp_net_config_default(&params->network);
	params->affinity = true;
	params->repair_deadlines = true;
	params->separation = MP_SEPARATE_CONTROLLERS;
	params->consolidate = true;
	params->swap_moves = false;
	params->relocation_moves = true;
	params->memory_latency = 0;
	params->access_density = 1000;
}

int mp_access_stall(const struct mp_params *params, int cores)
{
	return cores > 1 ? params->memory_latency * (cores - 1) : 0;
}

int mp_competing_cores(const struct mp_params *params, int controller)
{
	int cores = mp_ecu_initial_cores(params, controller);

	if (controller >= 0 && controller < MP_MAX_ECUS &&
	    params->competing_cores[controller] > cores)
		cores = params->competing_cores[controller];
	return cores;
}

int mp_default_accesses(const struct mp_params *params, int wcet)
{
	return (int)(((int64_t)wcet * params->access_density + 999) / 1000);
}

/* ---- platform ------------------------------------------------------------ */

static const char *const zonal_names[MP_ZONAL_ECUS] = {
	"PCU", "ZCU1", "ZCU2", "ZCU3", "ZCU4", "ZCU5",
};

/* ECU @ecu of the built-in zonal platform */
static void zonal_ecu(const struct mp_params *params, int ecu,
		      struct mp_ecu *out)
{
	memset(out, 0, sizeof(*out));
	if (ecu < 0 || ecu >= MP_ZONAL_ECUS)
		return;
	strcpy(out->name, zonal_names[ecu]);
	if (ecu == MP_CONTROLLER_PCU) {
		out->role = MP_ROLE_CENTRAL;
		out->cores[MP_CORE_BIG] = params->pcu_cores;
		out->cores[MP_CORE_LITTLE] = params->pcu_little_cores;
		out->speed[MP_CORE_BIG] = params->pcu_speed;
		out->speed[MP_CORE_LITTLE] = params->little_speed;
		out->added_type = MP_CORE_BIG;
		out->memory = params->pcu_memory;
	} else {
		out->role = MP_ROLE_ZONE;
		out->cores[MP_CORE_MCU] = params->zcu_cores;
		out->cores[MP_CORE_LOCKSTEP] = params->zcu_lockstep_cores;
		out->speed[MP_CORE_MCU] = params->zcu_speed;
		out->speed[MP_CORE_LOCKSTEP] = params->zcu_speed;
		out->added_type = MP_CORE_MCU;
		out->memory = params->zcu_memory;
	}
}

int mp_ecu_count(const struct mp_params *params)
{
	return params->platform.count ? params->platform.count : MP_ZONAL_ECUS;
}

void mp_ecu_get(const struct mp_params *params, int ecu, struct mp_ecu *out)
{
	if (!params->platform.count) {
		zonal_ecu(params, ecu, out);
		return;
	}
	if (ecu >= 0 && ecu < params->platform.count)
		*out = params->platform.ecus[ecu];
	else
		memset(out, 0, sizeof(*out));
}

const char *mp_ecu_name(const struct mp_params *params, int ecu)
{
	if (ecu == MP_CONTROLLER_ANY)
		return "any";
	if (ecu < 0 || ecu >= mp_ecu_count(params))
		return "?";
	return params->platform.count ? params->platform.ecus[ecu].name :
	       zonal_names[ecu];
}

enum mp_ecu_role mp_ecu_role(const struct mp_params *params, int ecu)
{
	if (!params->platform.count)
		return ecu == MP_CONTROLLER_PCU ? MP_ROLE_CENTRAL : MP_ROLE_ZONE;
	return ecu >= 0 && ecu < params->platform.count ?
	       params->platform.ecus[ecu].role : MP_ROLE_ECU;
}

int mp_ecu_speed(const struct mp_params *params, int ecu, int type)
{
	struct mp_ecu e;

	if (type < 0 || type >= MP_CORE_TYPES)
		return 0;
	if (params->platform.count)
		return ecu >= 0 && ecu < params->platform.count ?
		       params->platform.ecus[ecu].speed[type] : 0;
	zonal_ecu(params, ecu, &e);
	return e.speed[type];
}

unsigned mp_ecu_types(const struct mp_params *params, int ecu)
{
	unsigned types = 0;
	int t;

	for (t = 0; t < MP_CORE_TYPES; t++)
		if (mp_ecu_speed(params, ecu, t) > 0)
			types |= 1u << t;
	return types;
}

int mp_ecu_memory(const struct mp_params *params, int ecu)
{
	if (!params->platform.count)
		return ecu == MP_CONTROLLER_PCU ? params->pcu_memory :
		       params->zcu_memory;
	return ecu >= 0 && ecu < params->platform.count ?
	       params->platform.ecus[ecu].memory : 0;
}

int mp_ecu_initial_cores(const struct mp_params *params, int ecu)
{
	struct mp_ecu e;
	int cores = 0, t;

	mp_ecu_get(params, ecu, &e);
	for (t = 0; t < MP_CORE_TYPES; t++)
		cores += e.cores[t];
	return cores;
}

int mp_ecu_max_cores(const struct mp_params *params, int ecu)
{
	return params->platform.count && ecu >= 0 &&
	       ecu < params->platform.count ?
	       params->platform.ecus[ecu].max_cores : 0;
}

bool mp_ecu_is_central(const struct mp_params *params, int ecu)
{
	return ecu >= 0 && ecu < mp_ecu_count(params) &&
	       mp_ecu_role(params, ecu) == MP_ROLE_CENTRAL;
}

/* the speed of the fastest initial core of @ecu (0: none) */
static int fastest_core(const struct mp_params *params, int ecu)
{
	struct mp_ecu e;
	int speed = 0, t;

	mp_ecu_get(params, ecu, &e);
	for (t = 0; t < MP_CORE_TYPES; t++)
		if (e.cores[t] && e.speed[t] > speed)
			speed = e.speed[t];
	return speed;
}

bool mp_ecu_is_host(const struct mp_params *params, int ecu)
{
	int c, best = 0;

	if (ecu < 0 || ecu >= mp_ecu_count(params))
		return false;
	for (c = 0; c < mp_ecu_count(params); c++) {
		if (mp_ecu_is_central(params, c))
			return mp_ecu_is_central(params, ecu);
		if (fastest_core(params, c) > best)
			best = fastest_core(params, c);
	}
	return fastest_core(params, ecu) == best;
}

int mp_find_ecu(const struct mp_params *params, const char *name)
{
	int c;

	for (c = 0; c < mp_ecu_count(params); c++)
		if (!strcmp(mp_ecu_name(params, c), name))
			return c;
	return -1;
}

int mp_ecu_preference(const struct mp_params *params, int *order)
{
	int count = 0, c, pass;

	for (pass = 0; pass < 2; pass++)
		for (c = 0; c < mp_ecu_count(params); c++)
			if (mp_ecu_is_host(params, c) == !pass)
				order[count++] = c;
	return count;
}

unsigned mp_io_ecus(const struct mp_params *params)
{
	unsigned io = 0;
	int c;

	for (c = 0; c < mp_ecu_count(params); c++)
		if (!mp_ecu_is_central(params, c))
			io |= MP_CONTROLLER_BIT(c);
	if (!io)
		for (c = 0; c < mp_ecu_count(params); c++)
			io |= MP_CONTROLLER_BIT(c);
	return io;
}

bool mp_host_allowed(const struct mp_params *params, unsigned allowed,
		     unsigned types)
{
	int c;

	for (c = 0; c < mp_ecu_count(params); c++)
		if (mp_ecu_is_host(params, c) &&
		    (allowed & MP_CONTROLLER_BIT(c)) &&
		    (types & mp_ecu_types(params, c)))
			return true;
	return false;
}

bool mp_central_allowed(const struct mp_params *params, unsigned allowed,
			unsigned types)
{
	int c;

	for (c = 0; c < mp_ecu_count(params); c++)
		if (mp_ecu_is_central(params, c) &&
		    (allowed & MP_CONTROLLER_BIT(c)) &&
		    (types & mp_ecu_types(params, c)))
			return true;
	return false;
}

void mp_platform_zonal(const struct mp_params *params,
		       struct mp_platform *platform)
{
	int c;

	memset(platform, 0, sizeof(*platform));
	platform->count = MP_ZONAL_ECUS;
	for (c = 0; c < MP_ZONAL_ECUS; c++)
		zonal_ecu(params, c, &platform->ecus[c]);
}

int mp_platform_check(const struct mp_params *params, char *error,
		      size_t size)
{
	int c, d, t;

	if (params->platform.count < 0 || params->platform.count > MP_MAX_ECUS) {
		snprintf(error, size, "a platform has 1 to %d ECUs", MP_MAX_ECUS);
		return -EINVAL;
	}
	for (c = 0; c < mp_ecu_count(params); c++) {
		struct mp_ecu e;
		int cores = 0;

		mp_ecu_get(params, c, &e);
		if (!e.name[0]) {
			snprintf(error, size, "ECU %d has no name", c);
			return -EINVAL;
		}
		for (d = 0; d < c; d++)
			if (!strcmp(mp_ecu_name(params, d), e.name)) {
				snprintf(error, size, "two ECUs are called %s",
					 e.name);
				return -EINVAL;
			}
		if ((int)e.role < 0 || e.role >= MP_ROLE_COUNT) {
			snprintf(error, size, "ECU %s: unknown role", e.name);
			return -EINVAL;
		}
		for (t = 0; t < MP_CORE_TYPES; t++) {
			if (e.cores[t] < 0 || e.speed[t] < 0 ||
			    (e.cores[t] > 0 && e.speed[t] == 0)) {
				snprintf(error, size, "ECU %s: the cores of a "
					 "type need a positive speed", e.name);
				return -EINVAL;
			}
			cores += e.cores[t];
		}
		if (e.added_type < 0 || e.added_type >= MP_CORE_TYPES ||
		    e.speed[e.added_type] <= 0) {
			snprintf(error, size, "ECU %s: the added cores must be "
				 "of a type it has", e.name);
			return -EINVAL;
		}
		if (e.memory < 0 || e.max_cores < 0 ||
		    (e.max_cores && cores > e.max_cores)) {
			snprintf(error, size, "ECU %s: negative memory, or more "
				 "initial cores than its maximum", e.name);
			return -EINVAL;
		}
	}
	return 0;
}

void mp_ctx_init(struct mp_ctx *ctx, uint64_t seed)
{
	memset(ctx, 0, sizeof(*ctx));
	mp_rng_seed(&ctx->rng, seed);
	mp_params_default(&ctx->params);
}
