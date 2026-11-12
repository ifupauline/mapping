// SPDX-License-Identifier: Apache-2.0
/* Metaheuristic baselines: simulated annealing, genetic algorithm. */
#include <errno.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "mapping/assign.h"
#include "mapping/communication.h"
#include "mapping/let.h"
#include "mapping/partition.h"
#include "mapping/repair.h"
#include "mapping/rng.h"
#include "mapping/sched.h"
#include "mapping/search.h"
#include "mapping/stats.h"

#define MAX_SPARE 64		/* spare cores per ECU and type, at most */

struct search {
	struct mp_ctx ctx;
	struct mp_chain_array fragments;	/* partitioning output */
	struct mp_core_array empty;		/* the candidate cores */
	int *by_speed;				/* their order in first fit */
	struct mp_rng rng;
	struct mp_search_trace *trace;
	double start;
	long evaluations;
	int best_cores;
};

void mp_search_params_default(struct mp_search_params *params,
			      enum mp_search_method method, long budget)
{
	memset(params, 0, sizeof(*params));
	params->method = method;
	params->budget = budget;
	params->seed = 1;
	params->initial_temperature = 0.03;
	params->final_temperature = 0.0003;
	params->population = 40;
}

const char *mp_search_method_name(enum mp_search_method method)
{
	return method == MP_SEARCH_GENETIC ? "GA" : "SA";
}

/* ---- the candidate cores ---------------------------------------------------- */

static int build_candidates(struct search *s)
{
	const struct mp_params *params = &s->ctx.params;
	int c, t, i, n, err;

	err = mp_architecture_init(&s->ctx, &s->empty);
	if (err)
		return err;
	for (c = 0; c < mp_ecu_count(params); c++) {
		int64_t load[MP_CORE_TYPES] = {0};
		int max = mp_ecu_max_cores(params, c);
		int have = mp_ecu_initial_cores(params, c);

		/* the load each type would take: the sub-chains whose best
		 * type on this ECU it is */
		for (i = 0; i < s->fragments.count; i++) {
			const struct mp_chain *f = &s->fragments.items[i];

			if (!(mp_chain_allowed(f) & MP_CONTROLLER_BIT(c)))
				continue;
			t = mp_best_core_type(params, f, c);
			if (t >= 0)
				load[t] += mp_chain_utilization_on(f,
					mp_ecu_speed(params, c, t), t);
		}
		for (t = 0; t < MP_CORE_TYPES; t++) {
			int spare = (int)((load[t] + params->fill_limit - 1) /
					  params->fill_limit);

			if (spare > MAX_SPARE)
				spare = MAX_SPARE;
			for (n = 0; n < spare && (!max || have < max); n++, have++) {
				err = mp_add_typed_core(&s->ctx, &s->empty, c, t);
				if (err)
					return err;
			}
		}
	}
	return 0;
}

/* ---- loads and energy --------------------------------------------------- */

/* the slot of fragment @f on @core, -1 if absent */
static int slot_of(const struct mp_core *core, const struct mp_chain *f)
{
	int j;

	for (j = 0; j < core->chains.count; j++) {
		const struct mp_chain *chain = &core->chains.items[j];

		if (!chain->is_let && chain->id == f->id &&
		    chain->fragment_index == f->fragment_index)
			return j;
	}
	return -1;
}

/*
 * the load of @core without its chain in slot @out (-1: none) and with
 * @in (NULL: none), the LET task resized; as mp_core_load_with_chain()
 */
static int load_exchange(const struct mp_core *core, int out,
			 const struct mp_chain *in)
{
	int load = 0, gcd = 0, i, j;

	for (i = 0; i < core->chains.count; i++) {
		const struct mp_chain *chain = &core->chains.items[i];

		if (chain->is_let || i == out)
			continue;
		for (j = 0; j < chain->tasks.count; j++)
			gcd = mp_gcd(gcd, chain->tasks.items[j].period);
		load += chain->utilization;
	}
	if (in) {
		for (j = 0; j < in->tasks.count; j++)
			gcd = mp_gcd(gcd, in->tasks.items[j].period);
		load += mp_chain_utilization_on_core(in, core);
	}
	return gcd ? load + mp_let_utilization(core->let_wcet, gcd) : 0;
}

/*
 * M - 0.9 sum((1 - u/phi)^2) / M over the loads @load of @count cores:
 * the free capacity of the cores in use, squared, so that it gathers on
 * few cores (the next ones to empty); less load also costs less, so a
 * fragment prefers a faster core
 */
static double energy(const int *load, int count, double phi, int *used)
{
	double slack = 0;
	int i, m = 0;

	for (i = 0; i < count; i++)
		if (load[i]) {
			double x = 1 - load[i] / phi;

			m++;
			slack += x * x;
		}
	*used = m;
	return m ? m - 0.9 * slack / m : 0;
}

/* ---- moves -------------------------------------------------------------- */

/*
 * @fragment (a copy, or NULL) into a copy of @core, without its chain in
 * slot @out (-1: none): *@ok if the copy is schedulable
 */
static int trial(struct search *s, const struct mp_core *core, int core_index,
		 int out, const struct mp_chain *fragment, struct mp_core *copy,
		 bool *ok)
{
	int gcd, err;

	*ok = false;
	err = mp_core_copy(copy, core);
	if (err)
		return err;
	if (out >= 0) {
		const struct mp_chain *leaving = &core->chains.items[out];

		err = mp_core_remove_chain(copy, leaving->id,
					   leaving->fragment_index);
		if (err)
			return err;
	}
	if (fragment) {
		mp_core_load_with_chain(copy, fragment, &gcd);
		err = mp_core_add_chain(copy, fragment, gcd);
	} else {
		err = mp_core_build_task_list(copy, core_index);
	}
	if (err)
		return err;
	s->evaluations++;
	/*
	 * no task left, or a schedulable core that only lost a fragment:
	 * the same priorities still work (response times only decrease)
	 */
	if (!mp_core_period_gcd(copy) || !fragment) {
		*ok = true;
		return 0;
	}
	err = mp_assign_priorities_first_fit(&s->ctx, copy);
	if (err)
		return err;
	*ok = copy->schedulable;
	return 0;
}

/* replace core @i of @cores by @copy (which it now owns) */
static void commit(struct mp_core_array *cores, int i, struct mp_core *copy)
{
	mp_core_free(&cores->items[i]);
	cores->items[i] = *copy;
	memset(copy, 0, sizeof(*copy));
}

/* the improvement of the best mapping, for the trace */
static void improved(struct search *s, int cores)
{
	struct mp_search_trace *trace = s->trace;

	if (cores >= s->best_cores)
		return;
	s->best_cores = cores;
	if (trace && trace->count < MP_SEARCH_TRACE) {
		trace->time[trace->count] = mp_thread_cpu_seconds() - s->start;
		trace->cores[trace->count++] = cores;
	}
}

/*
 * first fit of the fragments in @order onto a copy of the candidates,
 * the cores tried the fastest first. With @opened_first, as in bin
 * packing: each fragment goes to the first core in use that takes it
 * (the fastest first, then in the order they were opened), else opens
 * the first other candidate core that does (the fastest first, then in
 * candidate order); without, every candidate core is tried in that
 * order, used or not. @where gets the core of each fragment; false in
 * *@feasible if one fits nowhere.
 */
static int first_fit(struct search *s, const int *order, bool opened_first,
		     struct mp_core_array *cores, int *where, bool *feasible)
{
	struct mp_core copy;
	int *opened, open_count = 0, k, n, i, err;

	memset(&copy, 0, sizeof(copy));
	*feasible = true;
	mp_core_array_free(cores);
	err = mp_core_array_copy(cores, &s->empty);
	if (err)
		return err;
	opened = malloc(((size_t)cores->count + 1) * sizeof(*opened));
	if (!opened)
		return -ENOMEM;
	if (!s->by_speed) {
		/* the candidates, the fastest first (stable) */
		s->by_speed = malloc(((size_t)cores->count + 1) *
				     sizeof(*s->by_speed));
		if (!s->by_speed) {
			free(opened);
			return -ENOMEM;
		}
		for (i = 0, n = 0; i < cores->count; i++) {
			int j = n++;

			while (j > 0 && cores->items[s->by_speed[j - 1]].speed <
			       cores->items[i].speed) {
				s->by_speed[j] = s->by_speed[j - 1];
				j--;
			}
			s->by_speed[j] = i;
		}
	}
	for (k = 0; k < s->fragments.count; k++) {
		const struct mp_chain *f = &s->fragments.items[order[k]];
		int tried = opened_first ? open_count : 0;
		bool ok = false;
		int gcd;

		/* the cores in use, then the others in candidate order */
		for (n = 0; n < tried + cores->count && !ok; n++) {
			if (n < tried) {
				i = opened[n];
			} else {
				i = s->by_speed[n - tried];
				if (opened_first &&
				    mp_core_period_gcd(&cores->items[i]))
					continue;	/* in use: tried */
			}
			if (!mp_placement_allowed(&s->ctx, cores, f, i, NULL) ||
			    mp_core_load_with_chain(&cores->items[i], f, &gcd) >
			    cores->items[i].fill_limit)
				continue;
			err = trial(s, &cores->items[i], i, -1, f, &copy, &ok);
			if (err) {
				mp_core_free(&copy);
				free(opened);
				return err;
			}
			if (!ok) {
				mp_core_free(&copy);
				continue;
			}
			commit(cores, i, &copy);
			where[order[k]] = i;
			if (opened_first && n >= tried) {
				/* in use: after the cores as fast as it */
				int j = open_count++;

				while (j > 0 && cores->items[opened[j - 1]].speed <
				       cores->items[i].speed) {
					opened[j] = opened[j - 1];
					j--;
				}
				opened[j] = i;
			}
		}
		if (!ok) {
			*feasible = false;
			break;
		}
	}
	free(opened);
	return 0;
}

/* fragments by decreasing utilization (the partitioning order) */
static void identity(int *order, int count)
{
	int i;

	for (i = 0; i < count; i++)
		order[i] = i;
}

/* ---- simulated annealing ---------------------------------------------------- */

static int anneal(struct search *s, const struct mp_search_params *sp,
		  int *best_where, bool *feasible)
{
	struct mp_core_array cores = {0};
	struct mp_core copy_a, copy_b;
	int nf = s->fragments.count, *where, *load, *used_list, err, i;
	double phi = s->ctx.params.fill_limit, e, best_e;
	long k;

	memset(&copy_a, 0, sizeof(copy_a));
	memset(&copy_b, 0, sizeof(copy_b));
	where = malloc(((size_t)nf + 1) * sizeof(*where));
	load = malloc(((size_t)s->empty.count + 1) * sizeof(*load));
	used_list = malloc(((size_t)s->empty.count + 1) * sizeof(*used_list));
	if (!where || !load || !used_list) {
		err = -ENOMEM;
		goto out;
	}
	identity(where, nf);
	{
		int *order = malloc(((size_t)nf + 1) * sizeof(*order));

		if (!order) {
			err = -ENOMEM;
			goto out;
		}
		identity(order, nf);
		err = first_fit(s, order, false, &cores, where, feasible);
		free(order);
	}
	if (err || !*feasible)
		goto out;
	for (i = 0; i < cores.count; i++)
		load[i] = mp_core_period_gcd(&cores.items[i]) ?
			  cores.items[i].load : 0;
	{
		int m;

		e = energy(load, cores.count, phi, &m);
		best_e = e;
		improved(s, m);
	}
	memcpy(best_where, where, (size_t)nf * sizeof(*where));

	for (k = 0; k < sp->budget; k++) {
		double t = sp->initial_temperature *
			   pow(sp->final_temperature / sp->initial_temperature,
			       (double)k / (double)sp->budget);
		bool exchange = mp_rng_uniform(&s->rng) < 0.3;
		int f1 = mp_rng_range(&s->rng, 0, nf - 1), f2 = -1;
		int a, b, used = 0, out_a, out_b, la, lb, m;
		const struct mp_chain *chain1, *chain2 = NULL;
		bool ok;
		double e_new;

		/*
		 * the source: half of the time a fragment of the less loaded of
		 * two cores in use (moves that empty cores), else any fragment
		 */
		if (mp_rng_uniform(&s->rng) < 0.5) {
			int g = mp_rng_range(&s->rng, 0, nf - 1);

			if (load[where[g]] < load[where[f1]])
				f1 = g;
		}
		a = where[f1];

		/* the target core: one in use (80 %), or any candidate */
		for (i = 0; i < cores.count; i++)
			if (load[i] && i != a)
				used_list[used++] = i;
		if (used && mp_rng_uniform(&s->rng) < 0.8)
			b = used_list[mp_rng_range(&s->rng, 0, used - 1)];
		else
			b = mp_rng_range(&s->rng, 0, cores.count - 1);
		if (b == a)
			continue;
		out_a = slot_of(&cores.items[a], &s->fragments.items[f1]);
		chain1 = &cores.items[a].chains.items[out_a];
		out_b = -1;
		if (exchange) {
			/* a fragment of core b */
			int on_b = 0, pick;

			for (i = 0; i < nf; i++)
				on_b += where[i] == b;
			if (!on_b)
				continue;
			pick = mp_rng_range(&s->rng, 0, on_b - 1);
			for (i = 0; i < nf; i++)
				if (where[i] == b && !pick--) {
					f2 = i;
					break;
				}
			out_b = slot_of(&cores.items[b], &s->fragments.items[f2]);
			chain2 = &cores.items[b].chains.items[out_b];
		}
		/* the spatial constraints and the cap */
		if (!mp_placement_allowed(&s->ctx, &cores, chain1, b, chain2) ||
		    (chain2 && !mp_placement_allowed(&s->ctx, &cores, chain2, a,
						     chain1)))
			continue;
		la = load_exchange(&cores.items[a], out_a, chain2);
		lb = load_exchange(&cores.items[b], out_b, chain1);
		if (la > cores.items[a].fill_limit ||
		    lb > cores.items[b].fill_limit)
			continue;
		/* the energy, then Metropolis */
		{
			int old_a = load[a], old_b = load[b];

			load[a] = la;
			load[b] = lb;
			e_new = energy(load, cores.count, phi, &m);
			load[a] = old_a;
			load[b] = old_b;
		}
		if (e_new > e && mp_rng_uniform(&s->rng) >= exp((e - e_new) / t))
			continue;
		/* schedulable: core b, and core a if it receives a fragment */
		err = trial(s, &cores.items[b], b, out_b, chain1, &copy_b, &ok);
		if (!err && ok)
			err = trial(s, &cores.items[a], a, out_a, chain2, &copy_a,
				    &ok);
		if (err)
			goto out;
		if (!ok) {
			mp_core_free(&copy_a);
			mp_core_free(&copy_b);
			continue;
		}
		commit(&cores, a, &copy_a);
		commit(&cores, b, &copy_b);
		where[f1] = b;
		if (f2 >= 0)
			where[f2] = a;
		load[a] = la;
		load[b] = lb;
		e = e_new;
		if (e < best_e - 1e-12) {
			best_e = e;
			memcpy(best_where, where, (size_t)nf * sizeof(*where));
			improved(s, m);
		}
	}
	err = 0;
out:
	mp_core_free(&copy_a);
	mp_core_free(&copy_b);
	mp_core_array_free(&cores);
	free(where);
	free(load);
	free(used_list);
	return err;
}

/* ---- genetic algorithm ------------------------------------------------- */

struct individual {
	int *order;
	int *where;
	double energy;		/* HUGE_VAL if a fragment fits nowhere */
	int cores;
};

static int decode(struct search *s, struct individual *x,
		  struct mp_core_array *cores)
{
	bool feasible;
	int err, i, m, *load;

	err = first_fit(s, x->order, true, cores, x->where, &feasible);
	if (err)
		return err;
	x->energy = HUGE_VAL;
	x->cores = 0;
	if (!feasible)
		return 0;
	load = malloc(((size_t)cores->count + 1) * sizeof(*load));
	if (!load)
		return -ENOMEM;
	for (i = 0; i < cores->count; i++)
		load[i] = mp_core_period_gcd(&cores->items[i]) ?
			  cores->items[i].load : 0;
	x->energy = energy(load, cores->count, s->ctx.params.fill_limit, &m);
	x->cores = m;
	free(load);
	improved(s, m);
	return 0;
}

/* order crossover (OX1): a slice of @p1, the rest in the order of @p2 */
static void crossover(struct mp_rng *rng, const int *p1, const int *p2,
		      int *child, int n, char *taken)
{
	int a = mp_rng_range(rng, 0, n - 1), b = mp_rng_range(rng, 0, n - 1);
	int i, k;

	if (a > b) {
		int swap = a;

		a = b;
		b = swap;
	}
	memset(taken, 0, (size_t)n);
	for (i = a; i <= b; i++) {
		child[i] = p1[i];
		taken[p1[i]] = 1;
	}
	k = (b + 1) % n;
	for (i = 0; i < n; i++) {
		int gene = p2[(b + 1 + i) % n];

		if (taken[gene])
			continue;
		child[k] = gene;
		k = (k + 1) % n;
	}
}

static int tournament(struct mp_rng *rng, const struct individual *pop, int n)
{
	int a = mp_rng_range(rng, 0, n - 1), b = mp_rng_range(rng, 0, n - 1);

	return pop[a].energy <= pop[b].energy ? a : b;
}

/* the two best of @pop first (stable for equal energies) */
static void two_best(struct individual *pop, int n)
{
	int round, i;

	for (round = 0; round < 2 && round < n; round++) {
		int best = round;

		for (i = round + 1; i < n; i++)
			if (pop[i].energy < pop[best].energy)
				best = i;
		if (best != round) {
			struct individual swap = pop[round];

			pop[round] = pop[best];
			pop[best] = swap;
		}
	}
}

static int genetic(struct search *s, const struct mp_search_params *sp,
		   int *best_where, bool *feasible)
{
	int nf = s->fragments.count, p = sp->population < 4 ? 4 : sp->population;
	struct individual *pop = calloc((size_t)p, sizeof(*pop));
	struct individual *next = calloc((size_t)p, sizeof(*next));
	struct mp_core_array cores = {0};
	char *taken = malloc((size_t)nf + 1);
	int err = 0, i, j;
	long g;

	if (!pop || !next || !taken) {
		err = -ENOMEM;
		goto out;
	}
	for (i = 0; i < p; i++) {
		pop[i].order = malloc(((size_t)nf + 1) * sizeof(int));
		pop[i].where = malloc(((size_t)nf + 1) * sizeof(int));
		next[i].order = malloc(((size_t)nf + 1) * sizeof(int));
		next[i].where = malloc(((size_t)nf + 1) * sizeof(int));
		if (!pop[i].order || !pop[i].where || !next[i].order ||
		    !next[i].where) {
			err = -ENOMEM;
			goto out;
		}
		identity(pop[i].order, nf);
		/* the first one keeps the decreasing utilization order */
		for (j = nf - 1; i && j > 0; j--) {
			int k = mp_rng_range(&s->rng, 0, j), swap;

			swap = pop[i].order[j];
			pop[i].order[j] = pop[i].order[k];
			pop[i].order[k] = swap;
		}
		err = decode(s, &pop[i], &cores);
		if (err)
			goto out;
	}
	for (g = 0; g < sp->budget; g++) {
		two_best(pop, p);
		for (i = 0; i < 2; i++) {
			memcpy(next[i].order, pop[i].order, (size_t)nf * sizeof(int));
			memcpy(next[i].where, pop[i].where, (size_t)nf * sizeof(int));
			next[i].energy = pop[i].energy;
			next[i].cores = pop[i].cores;
		}
		for (i = 2; i < p; i++) {
			int a = tournament(&s->rng, pop, p);
			int b = tournament(&s->rng, pop, p);

			if (mp_rng_uniform(&s->rng) < 0.9)
				crossover(&s->rng, pop[a].order, pop[b].order,
					  next[i].order, nf, taken);
			else
				memcpy(next[i].order, pop[a].order,
				       (size_t)nf * sizeof(int));
			if (mp_rng_uniform(&s->rng) < 0.3) {
				int x = mp_rng_range(&s->rng, 0, nf - 1);
				int y = mp_rng_range(&s->rng, 0, nf - 1);
				int swap = next[i].order[x];

				next[i].order[x] = next[i].order[y];
				next[i].order[y] = swap;
			}
			err = decode(s, &next[i], &cores);
			if (err)
				goto out;
		}
		{
			struct individual *swap = pop;

			pop = next;
			next = swap;
		}
	}
	two_best(pop, p);
	*feasible = pop[0].energy < HUGE_VAL;
	if (*feasible)
		memcpy(best_where, pop[0].where, (size_t)nf * sizeof(int));
out:
	for (i = 0; i < p && pop && next; i++) {
		free(pop[i].order);
		free(pop[i].where);
		free(next[i].order);
		free(next[i].where);
	}
	free(pop);
	free(next);
	free(taken);
	mp_core_array_free(&cores);
	return err;
}

/* ---- the result ---------------------------------------------------------- */

/* the mapping of @where on the candidates, scheduled and analysed */
static int finish(struct search *s, const int *where, bool local_search,
		  struct mp_design_point *point)
{
	struct mp_ctx *ctx = &point->ctx;
	int i, err, gcd;

	err = mp_core_array_copy(&point->cores, &s->empty);
	if (err)
		return err;
	for (i = 0; i < s->fragments.count; i++) {
		struct mp_core *core = &point->cores.items[where[i]];

		mp_core_load_with_chain(core, &s->fragments.items[i], &gcd);
		err = mp_core_add_chain(core, &s->fragments.items[i], gcd);
		if (err)
			return err;
		point->fragments.items[i].is_assigned = true;
	}
	for (i = 0; i < point->cores.count; i++)
		point->cores.items[i].is_empty = !point->cores.items[i].load ||
			!mp_core_period_gcd(&point->cores.items[i]);
	err = mp_schedule(ctx, &point->cores);
	if (err)
		return err;
	mp_count_schedulable_cores(ctx, &point->cores);
	ctx->rate_after_swapping = ctx->schedulable_rate;
	ctx->rate_after_displacement = ctx->schedulable_rate;
	/* the same deadline local search as the heuristic, if asked */
	if (local_search && ctx->params.repair_deadlines) {
		err = mp_repair_deadlines(ctx, &point->cores);
		if (err)
			return err;
		mp_count_schedulable_cores(ctx, &point->cores);
	}
	ctx->rate_final = ctx->schedulable_rate;
	err = mp_communication(&point->cores, &ctx->params.network,
			       &point->paths, &point->communication);
	if (err)
		return err;
	err = mp_verify_mapping(&point->cores);
	if (err)
		return err;
	mp_compute_stats(ctx, &point->cores, &point->stats);
	point->stats.lower_bound_cores =
		mp_cores_lower_bound(&ctx->params, &point->fragments);
	return 0;
}

int mp_search(const struct mp_ctx *base,
	      const struct mp_chain_array *application,
	      const struct mp_search_params *params,
	      struct mp_design_point *point, struct mp_search_trace *trace)
{
	struct search s;
	bool feasible = false;
	int *where = NULL, err;

	memset(point, 0, sizeof(*point));
	memset(&s, 0, sizeof(s));
	if (base->params.memory_latency)
		return -ENOTSUP;
	if (trace)
		memset(trace, 0, sizeof(*trace));
	s.ctx = *base;
	s.trace = trace;
	s.best_cores = 1 << 30;
	s.start = mp_thread_cpu_seconds();
	mp_rng_seed(&s.rng, params->seed);
	memset(s.ctx.reserved_memory, 0, sizeof(s.ctx.reserved_memory));

	/* the sub-chains of the heuristic */
	err = mp_chain_array_copy(&s.fragments, application);
	if (!err) {
		mp_prepare_chains(&s.ctx.params, &s.fragments);
		err = mp_partition(&s.ctx, &s.fragments);
	}
	if (!err)
		err = build_candidates(&s);
	if (!err)
		where = malloc(((size_t)s.fragments.count + 1) * sizeof(*where));
	if (!err && !where)
		err = -ENOMEM;
	if (!err)
		err = params->method == MP_SEARCH_GENETIC ?
		      genetic(&s, params, where, &feasible) :
		      anneal(&s, params, where, &feasible);

	point->sigma = s.ctx.params.sigma;
	point->algorithm = s.ctx.params.algorithm;
	point->ctx = s.ctx;
	point->ctx.feasible = feasible;
	point->feasible = feasible;
	if (!err)
		err = mp_chain_array_copy(&point->fragments, &s.fragments);
	if (!err && feasible)
		err = finish(&s, where, params->local_search, point);
	point->times.total = mp_thread_cpu_seconds() - s.start;
	if (trace)
		trace->evaluations = s.evaluations;
	free(where);
	free(s.by_speed);
	mp_chain_array_free(&s.fragments);
	mp_core_array_free(&s.empty);
	return err;
}
