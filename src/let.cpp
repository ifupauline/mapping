#include "let.h"
#include "generator.h"

static struct task *_get_let_task(struct core &b)
{
        int idx;

        idx = 0;

        for (unsigned int i = 0; i < b.v_tcs.size(); i++) {
                if (b.v_tcs[i].is_let == YES)
                        idx = i;
        }
        return &b.v_tcs[idx].v_tasks[0];
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

void update_let(struct core &b, int gcd)
{
        struct task *let;

        /* update let task */
        let = _get_let_task(b);

        let->c = 200;
        let->t = gcd;

        if (let->t != 0)
                let->u = ((float)let->c / (float)let->t) * PERMILL;

        else
                let->u = 0;

        /* update let tc */
        b.v_tcs[0].u = let->u;
        b.v_tcs[0].gcd = gcd;
        b.v_tcs[0].v_tasks.clear();
        b.v_tcs[0].v_tasks.push_back(*let);
}

void update_let_with_c(struct core &b, int gcd, int let_c)
{
        struct task *let;


        /* update let task */
        let = _get_let_task(b);


        let->c = let_c;
        let->t = gcd;
        let->u = ((float)let->c / (float)let->t) * PERMILL;

        /* update let tc */
        b.v_tcs[0].u = let->u;
        b.v_tcs[0].gcd = gcd;
        b.v_tcs[0].v_tasks.clear();
        b.v_tcs[0].v_tasks.push_back(*let);
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
