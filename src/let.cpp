#include "let.h"
#include "generator.h"

static int _get_let_idx(struct core &b)
{
        for (unsigned int i = 0; i < b.v_tcs.size(); i++) {
                if (b.v_tcs[i].is_let == YES)
                        return i;
        }
        printf("ERR! Core %d has no LET task\n", b.id);
        exit(0);
}

/* write the LET task back into the LET tc, wherever it sits in v_tcs */
static void _set_let(struct core &b, int idx, struct task let, int gcd)
{
        let.is_let = YES;
        let.p = 1;

        b.v_tcs[idx].u = let.u;
        b.v_tcs[idx].gcd = gcd;
        b.v_tcs[idx].v_tasks.clear();
        b.v_tcs[idx].v_tasks.push_back(let);
}

void init_let_task(struct tc &let, struct context &ctx)
{
        struct task t;

        t = {0};

        let.u = 0;
        let.color = -1;
        let.is_let = YES;
        let.is_assign = NO;
        let.id = ctx.tcs_count++;

        t.c = 0;
        t.t = 0;
        t.r = 0;
        t.u = 0;
        t.p = 1; /* always highest priority */
        t.task_id = 0;
        t.tc_id = let.id;
        t.uniq_id = -1; /* uniq_id doesn't matter for let task */
        t.is_let = YES;

        let.v_tasks.push_back(t);
}

void update_let_with_c(struct core &b, int gcd, int let_c)
{
        int idx;
        struct task let;

        idx = _get_let_idx(b);
        let = b.v_tcs[idx].v_tasks[0];

        /* gcd == 0: no task left on the core, LET performs no communication */
        if (gcd == 0) {
                let.c = 0;
                let.t = 0;
                let.u = 0;
        } else {
                let.c = let_c;
                let.t = gcd;
                let.u = ceil(((float)let.c / (float)let.t) * PERMILL);
        }
        _set_let(b, idx, let, gcd);
}

void update_let(struct core &b, int gcd)
{
        /* LET WCET of 200us, Section 4.2 */
        update_let_with_c(b, gcd, 200);
}

int check_if_fit_tc(struct core &b, struct tc &tc, int &_gcd)
{
        struct core tmp_b;
        int total_coreload;
        int target_coreload;
        vector<struct task> v_tasks;

        _gcd = 0;
        tmp_b = {0};

        tmp_b = b;
        total_coreload = 0;
        target_coreload = 0;

        /* copy v_tasks */
        add_tasks_to_v_tasks(v_tasks, tc.v_tasks);
        for (unsigned int i = 0; i < tmp_b.v_tcs.size(); i++) {
                if (tmp_b.v_tcs[i].is_let == YES)
                        continue;
                add_tasks_to_v_tasks(v_tasks, tmp_b.v_tcs[i].v_tasks);
        }

        /* cmp gcd */
        _gcd = gcd(v_tasks);

        /* update let tc and let task */
        update_let(tmp_b, _gcd);

        /* compute total core load */
        target_coreload = core_load(tmp_b);

        /* add utilization rate of potential tc */
        total_coreload = tc.u + target_coreload;

        return total_coreload;
}
