#include "let.h"
#include "print.h"
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

        let.size = 0;
        let.memcost = 0;
        let.disp_count = 0;
        let.swap_count = 0;
        let.color = -1;
        let.is_let = YES;
        let.is_allocated = NO;
        let.id = ctx.tcs_count;

        t.c = 0;
        t.t = 0;
        t.r = 0;
        t.u = 0;
        t.p = 1; /* always highest priority */
        t.id = 0;
        t.tc_id = let.id;
        t.is_let = YES;

        let.v_tasks.push_back(t);
}

void update_let(struct core &b, int gcd)
{
        struct task *let;

        /* update let task */
        let = _get_let_task(b);
        switch (b.memcost) {

                case 1: 
                        let->c = gen_rand(50, 100);
                        break;
                case 2:
                        let->c = gen_rand(100, 200);
                        break;
                case 3: 
                        let->c = gen_rand(200, 300);
                        break;
                default:
                        let->c = 400;
                        break;
        }
        let->t = gcd;
        let->u = ceil(((float)let->c / (float)let->t) * PERMILL);

        /* update let tc */
        b.v_tcs[0].size = let->u;
        b.v_tcs[0].gcd = gcd;
        b.v_tcs[0].v_tasks.clear();
        b.v_tcs[0].v_tasks.push_back(*let);
}

int check_if_fit_tc(struct core &b, struct tc &tc, int &gcd)
{
        struct core tmp_b;
        int total_coreload;
        int target_coreload;
        vector<struct task> v_tasks;

        gcd = 0;
        tmp_b = b;
        total_coreload = 0;
        target_coreload = 0;

        /* add tc memcost to core memcost */
        cmp_core_memcost(tmp_b);
        tmp_b.memcost += tc.memcost;

        /* copy v_tasks */
        add_tasks_to_v_tasks(v_tasks, tc.v_tasks);
        for (unsigned int i = 0; i < tmp_b.v_tcs.size(); i++) {
                if (tmp_b.v_tcs[i].is_let == YES)
                        continue;
                add_tasks_to_v_tasks(v_tasks, tmp_b.v_tcs[i].v_tasks);
        }

        /* cmp gcd */
        gcd = cmp_gcd(v_tasks);

        for (unsigned int i = 0; i < tmp_b.v_tcs.size(); i++) {
                if (tmp_b.v_tcs[i].is_let == YES) {
                        if (gcd < tmp_b.v_tcs[i].v_tasks[0].c) {
                                printf("ERR! gcd < c --> TC %d can never be allocated, increase EPSILON\n", tc.id);
                                exit(0);
                        }
                }
        }

        /* update let tc and let task */
        update_let(tmp_b, gcd);

        /* compute total core load */
        cmp_core_load(tmp_b, target_coreload);
        total_coreload = tc.size + target_coreload;

        return total_coreload;
}
