/* SPDX-License-Identifier: Apache-2.0 */
#ifndef MAPPING_IO_H
#define MAPPING_IO_H

/*
 * Input files of the tool (JSON, documented in the user manual).
 *
 * An application file lists task-chains and their tasks: period and WCET
 * on the reference core (mandatory), and optionally the ECU of a sensor or
 * actuator task, the ECUs and core types allowed, the WCET factor of each
 * core type, memory, message size, memory accesses, co-location with the
 * next task, a deadline on the maximum reaction time and a replica group.
 *
 * A configuration file has five sections, each optional (the defaults are
 * those of the built-in zonal platform and of the method):
 *  - platform: the ECUs, their role, cores (type, count, speed), the type
 *    of the cores the assignment may add, memory and largest core count;
 *  - execution_model: fixed-priority preemptive scheduling and LET
 *    communication (the only ones supported), the utilization cap, the
 *    WCET of a LET task, a WCET margin;
 *  - interference_model: shared-memory contention (round-robin
 *    arbitration), the latency of a request and the access density;
 *  - network_model: the TSN backbone: topology (star, tree, ring or the
 *    switches and links of a custom one), rates, switch latency, traffic
 *    shaping (CBS, TAS or ATS), traffic classes, frame preemption, FRER;
 *  - method: the allocation rule, sigma, affinity, consolidation, local
 *    search, separation of replicas, fixed hardware, sizing for a target
 *    critical scaling factor.
 * Unknown keys and values out of range are errors: the message gives the
 * file, the line and the key.
 */

#include <stdio.h>

#include "mapping/context.h"
#include "mapping/model.h"

/* names of the chains and tasks of an application file (may be empty) */
struct mp_names {
	int chain_count;
	char (*chain)[MP_NAME_SIZE];	/* by chain id */
	int task_count;
	char (*task)[MP_NAME_SIZE];	/* by unique task id */
};

void mp_names_free(struct mp_names *names);

/*
 * mp_read_configuration() - read @path into @params (first set to the
 * defaults)
 * @error: a message on failure
 * Return: 0, -EINVAL (invalid file), -ENOMEM or the errno of the read.
 */
int mp_read_configuration(const char *path, struct mp_params *params,
			  char *error, size_t size);

/*
 * mp_read_application() - read the task-chains of @path for the platform
 * of @params (ECU names) into @application, numbered as the generator
 * numbers them; sets params->chain_count
 * @names: set to the names of the file (NULL: not wanted)
 * Return: 0, -EINVAL (invalid file), -ENOMEM or the errno of the read.
 */
int mp_read_application(const char *path, struct mp_params *params,
			struct mp_chain_array *application,
			struct mp_names *names, char *error, size_t size);

/* write @params as a configuration file (the platform as an explicit list
 * of ECUs) */
void mp_write_configuration(FILE *out, const struct mp_params *params);

/* write @application as an application file for the platform of @params */
void mp_write_application(FILE *out, const struct mp_params *params,
			  const struct mp_chain_array *application,
			  const struct mp_names *names);

/*
 * the built-in templates: "zonal" (the default platform: a central
 * computer and five zone controllers), "domain" (five domain
 * controllers), "hybrid" (a central computer, an ADAS domain controller
 * and four zone controllers) and "distributed" (twelve ECUs); -EINVAL for
 * another name
 */
int mp_params_template(struct mp_params *params, const char *name);

/* names of the enumerations, as in the files */
const char *mp_core_type_name(int type);
const char *mp_role_name(enum mp_ecu_role role);

#endif /* MAPPING_IO_H */
