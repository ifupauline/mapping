#include "stats.h"
#include "sched_analysis.h"

int ext_pa_count = 0;
int ext_ba_win_count = 0;
int ext_re_win_count = 0;

static void _find_hp_tasks(vector<struct task> &v_tasks,
                vector<struct task> &hp_tasks, struct task &tau)
{
        for (unsigned int i = 0; i < v_tasks.size(); i++) {
                /* a LET task of an empty core (C = T = 0) never interferes */
                if (v_tasks[i].t == 0)
                        continue;
                if (v_tasks[i].p < tau.p)
                        hp_tasks.push_back(v_tasks[i]);
        }
}

/*
 * Worst-case response time of tau under fixed priorities (Eq. 3.3):
 * least positive fixed point of R = C + sum_{j in hp} ceil(R / Tj) * Cj.
 * The iteration stops as soon as R exceeds the implicit deadline T, so the
 * returned value is either the exact WCRT or a value > T.
 */
static int _resp_task(vector<struct task> &hp_tasks, struct task &tau)
{
        long r;
        long r_prev;

        r = tau.c;
        r_prev = -1;

        while (r != r_prev && r <= tau.t) {
                r_prev = r;
                r = tau.c;
                for (unsigned int j = 0; j < hp_tasks.size(); j++)
                        r += ((r_prev + hp_tasks[j].t - 1) / hp_tasks[j].t) *
                                hp_tasks[j].c;
        }
        return (int)r;
}

static void _resp(vector<struct task> &v_tasks, int &flag)
{
        vector<struct task> hp_tasks;

        flag = SCHED_OK;

        sort_inc_task_priority(v_tasks);

        for (unsigned int i = 0; i < v_tasks.size(); i++) {
                /* LET task of an empty core performs no communication */
                if (v_tasks[i].t == 0 && v_tasks[i].c == 0) {
                        v_tasks[i].r = 0;
                        continue;
                }

                if (v_tasks[i].c > v_tasks[i].t) {
                        printf("ERR! Execution Time %d of tau %d > Period %d\n\n",
                                        v_tasks[i].c, v_tasks[i].task_id, v_tasks[i].t);
                        exit(0);
                }

                hp_tasks.clear();
                _find_hp_tasks(v_tasks, hp_tasks, v_tasks[i]);
                v_tasks[i].r = _resp_task(hp_tasks, v_tasks[i]);

                if (v_tasks[i].r > v_tasks[i].t)
                        flag = SCHED_FAILED;
        }
}

int wcrt(vector<struct task> &v_tasks)
{
        int flag;

        flag = -1;

        _resp(v_tasks, flag);

        if (flag == SCHED_OK)
                return SCHED_OK;

        else if (flag == SCHED_FAILED)
                return SCHED_FAILED;
        else {
                printf("ERR! wcrt!\n");
                exit(0);
        }
        return -1;
}

static void _save_priorities(struct core &b)
{
        int p;

        p = 2;

        sort_inc_task_priority(b.v_tasks);

        for (unsigned int i = 0; i < b.v_tasks.size(); i++) {
                if (b.v_tasks[i].is_let == YES) {
                        b.v_tasks[i].p = 1;
                        continue;
                }
                b.v_tasks[i].p = p;
                p++;
        }
}

/* identity of a task inside a core (priorities change during swapping) */
static int _same_task(const struct task &a, const struct task &b)
{
        return a.is_let == b.is_let && a.tc_id == b.tc_id &&
                a.task_id == b.task_id;
}

static int _find_task(vector<struct task> &v_tasks, const struct task &tau)
{
        for (unsigned int i = 0; i < v_tasks.size(); i++) {
                if (_same_task(v_tasks[i], tau))
                        return i;
        }
        printf("ERR! task %d of tc %d not found in core\n", tau.task_id,
                        tau.tc_id);
        exit(0);
}

static int _is_sched(const struct task &tau)
{
        return tau.is_let == YES || tau.r <= tau.t;
}

/*
 * Reassignment of one unschedulable task tau (Section 3.5.2.2).
 * The eligible higher priority tasks hps(tau) (not LET, not from the same
 * original task-chain as tau) are moved, from the adjacent one upwards, to the
 * priority level right below tau. After each move the WCRT test is run:
 *  - Condition 3.1: tau schedulable and no task that was schedulable before
 *                   became unschedulable -> keep the new ordering (YES)
 *  - Condition 3.2: a swapped task (or any previously schedulable task) became
 *                   unschedulable -> stop (NO)
 *  - Condition 3.3: all swaps tried and tau still unschedulable -> stop (NO)
 * b.v_tasks must hold an up-to-date WCRT result when this is called.
 */
static int _pswap(struct core &b, struct task tau)
{
        int pos;
        vector<struct task> v_hps;
        vector<struct task> v_was_sched;

        sort_inc_task_priority(b.v_tasks);
        pos = _find_task(b.v_tasks, tau);

        for (unsigned int i = 0; i < b.v_tasks.size(); i++) {
                if (_is_sched(b.v_tasks[i]))
                        v_was_sched.push_back(b.v_tasks[i]);
        }

        /* hps(tau), ordered from the adjacent (lowest) one upwards */
        for (int i = pos - 1; i >= 0; i--) {
                if (b.v_tasks[i].is_let == YES)
                        continue;
                if (b.v_tasks[i].tc_id == tau.tc_id)
                        continue;
                v_hps.push_back(b.v_tasks[i]);
        }

        for (unsigned int z = 0; z < v_hps.size(); z++) {
                int h;
                struct task swapped;

                /* move the hp task right below tau, keep everything else */
                sort_inc_task_priority(b.v_tasks);
                h = _find_task(b.v_tasks, v_hps[z]);
                swapped = b.v_tasks[h];
                b.v_tasks.erase(b.v_tasks.begin() + h);
                pos = _find_task(b.v_tasks, tau);
                b.v_tasks.insert(b.v_tasks.begin() + pos + 1, swapped);

                /* renumber in vector order: LET 1, then 2, 3, ... */
                for (unsigned int i = 0, p = 2; i < b.v_tasks.size(); i++) {
                        if (b.v_tasks[i].is_let == YES)
                                b.v_tasks[i].p = 1;
                        else
                                b.v_tasks[i].p = p++;
                }

                b.flag = wcrt(b.v_tasks);

                /* condition 3.2 */
                int broken = NO;
                for (unsigned int i = 0; i < v_was_sched.size(); i++) {
                        if (!_is_sched(b.v_tasks[_find_task(b.v_tasks, v_was_sched[i])]))
                                broken = YES;
                }
                if (broken == YES)
                        return NO;

                /* condition 3.1 */
                if (_is_sched(b.v_tasks[_find_task(b.v_tasks, tau)]))
                        return YES;
        }
        /* condition 3.3 */
        return NO;
}

void _base_assignment(struct core &b)
{
        int p;

        p = 2;

        /* no need to check for TC belonging to the same TC coze of ordering */
        sort_inc_task_id(b.v_tasks);

        for (unsigned int i = 0; i < b.v_tasks.size(); i++) {
                if (b.v_tasks[i].is_let == YES) {
                        b.v_tasks[i].p = 1;
                        continue;
                }
                b.v_tasks[i].p = p;
                p++;
        }
        b.flag = wcrt(b.v_tasks);
        copy_back_prio_to_tc(b);
        copy_back_resp_to_tc(b);
}

/*
 * Case 1 and Case 2 of Section 3.5.2.2: the swapping procedure is applied to
 * each unschedulable task, starting with the highest priority one. A
 * successful swap sequence (Condition 3.1) is kept, a failed one is discarded.
 */
static void _reassignment(struct core &b)
{
        struct core tmp_b;
        vector<struct task> v_unsched;

        sort_inc_task_priority(b.v_tasks);

        for (unsigned int i = 0; i < b.v_tasks.size(); i++) {
                if (b.v_tasks[i].p == 0) {
                        printf("\nERR! Core %d tau %d p %d idx %d\n",
                                        b.id, b.v_tasks[i].task_id, b.v_tasks[i].p,
                                        b.v_tasks[i].idx.tc_idx);
                        exit(0);
                }
                if (!_is_sched(b.v_tasks[i]))
                        v_unsched.push_back(b.v_tasks[i]);
        }

        for (unsigned int i = 0; i < v_unsched.size(); i++) {
                /* may already have been fixed by a previous reassignment */
                if (_is_sched(b.v_tasks[_find_task(b.v_tasks, v_unsched[i])]))
                        continue;

                tmp_b = b;
                if (_pswap(tmp_b, v_unsched[i]) == YES)
                        b = tmp_b;
        }

        _save_priorities(b);
        b.flag = wcrt(b.v_tasks);
        copy_back_prio_to_tc(b);
        copy_back_resp_to_tc(b);

        if (b.flag == SCHED_OK) {
                printf("Core %d SCHED_OK with priority swapping\n", b.id);
                ext_re_win_count++;
        }
}

void priority_assignment(struct core &b)
{
        ext_pa_count++;

        _base_assignment(b);
        if (b.flag == SCHED_FAILED) {
                printf("Core %d SCHED_FAILED\n", b.id);
                _reassignment(b);

        } else if (b.flag == SCHED_OK) {
                printf("Core %d SCHED_OK\n", b.id);
                ext_ba_win_count++;
                return;
        }
}

void sched_analysis(vector<struct core> &v_cores, struct context &ctx)
{
        sort_inc_core_load_rem(v_cores);
        copy_v_tc_to_v_tasks_with_pos(v_cores);

        for (unsigned int i = 0; i < v_cores.size(); i++) {
                if (v_cores[i].load == 0)
                        continue;
                else
                        priority_assignment(v_cores[i]);
        }

        ctx.p.sched_rate_allo = sched_rate(v_cores, ctx);
        ctx.p.sched_imp_allo = ctx.sched_ok_count;
}
