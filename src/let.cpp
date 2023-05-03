#include "let.h"
#include "print.h"
#include "generator.h"

static struct task *_get_let_task(struct bin &b)
{
        int idx;

        idx = 0;

        for (unsigned int i = 0; i < b.v_itms.size(); i++) {
                if (b.v_itms[i].is_let == YES)
                        idx = i;
        }
        return &b.v_itms[idx].v_tasks[0];
}

void init_let_task(struct item &let, struct context &ctx)
{
        struct task t;

        t = {0};

        let.size = 0;
        let.nbr_cut = 0;
        let.frag_id = -1;
        let.memcost = 0;
        let.disp_count = 0;
        let.swap_count = 0;
        let.is_let = YES;
        let.is_frag = NO;
        let.is_fragmented = NO;
        let.is_allocated = NO;
        let.id = ctx.itms_count;

        t.c = 0;
        t.t = 0;
        t.d = t.t;
        t.r = 0;
        t.u = 0;
        t.p = 1; /* always highest priority */
        t.id = 0;
        t.is_let = YES;

        let.v_tasks.push_back(t);
}

void update_let(struct bin &b, int gcd)
{
        struct task *let;

        /* update let task */
        let = _get_let_task(b);
        switch (b.memcost) {

                case 1: 
                        let->c = gen_rand(2, 33);
                        break;
                case 2:
                        let->c = gen_rand(34, 66);
                        break;
                case 3: 
                        let->c = gen_rand(67, 99);
                        break;
                case 4:
                        let->c = gen_rand(100, 132);
                        break;
                case 5:
                        let->c = gen_rand(133, 165);
                        break;

                default:
                        let->c = 165;
                        break;
        }
        let->t = gcd;
        let->d = let->t;
        let->u = ceil(((float)let->c / (float)let->t) * PERMILL);

        /* update let item */
        b.v_itms[0].size = let->u;
        b.v_itms[0].gcd = gcd;
        b.v_itms[0].v_tasks.clear();
        b.v_itms[0].v_tasks.push_back(*let);
}

int check_if_fit_itm(struct bin &b, struct item &itm, int &gcd)
{
        struct bin tmp_b;
        int total_binload;
        int target_binload;
        vector<struct task> v_tasks;

        gcd = 0;
        tmp_b = b;
        total_binload = 0;
        target_binload = 0;

        /* add itm memcost to bin memcost */
        compute_bin_memcost(tmp_b);
        tmp_b.memcost += itm.memcost;

        /* copy v_tasks */
        add_tasks_to_v_tasks(v_tasks, itm.v_tasks);
        for (unsigned int i = 0; i < tmp_b.v_itms.size(); i++) {
                if (tmp_b.v_itms[i].is_let == YES)
                        continue;
                add_tasks_to_v_tasks(v_tasks, tmp_b.v_itms[i].v_tasks);
        }

        /* cmp gcd */
        gcd = compute_gcd(v_tasks);

        /* update let item and let task */
        update_let(tmp_b, gcd);

        /* compute total bin load */
        compute_bin_load(tmp_b, target_binload);
        total_binload = itm.size + target_binload;

        //if (total_binload > b.phi) {
        //        printf("Item %d of size %d cannot fit in Bin %d with potential load: %d\n", 
        //                        itm.id, itm.size, b.id, total_binload);

        //} else {
        //        printf("Item %d of size %d can fit in Bin %d with potential load: %d\n\n", 
        //                        itm.id, itm.size, b.id, total_binload);
        //}
        return total_binload;
}

int check_if_fit_cut(struct bin &b, struct cut &c, int &gcd, int memcost, int side)
{
        struct bin tmp_b;
        int total_binload;
        int target_binload;
        vector<struct task> v_tasks;

        gcd = 0;
        tmp_b = b;
        total_binload = 0;
        target_binload = 0;

        /* add itm memcost to bin memcost */
        compute_bin_memcost(tmp_b);
        tmp_b.memcost += memcost;

        /* copy tasks of left fragment */
        if (side == LEFT)
                add_tasks_to_v_tasks(v_tasks, c.v_tasks_lf);

        /* copy tasks of right fragment */
        if (side == RIGHT)
                add_tasks_to_v_tasks(v_tasks, c.v_tasks_rf);

        for (unsigned int i = 0; i < tmp_b.v_itms.size(); i++) {
                if (tmp_b.v_itms[i].is_let == YES)
                        continue;
                add_tasks_to_v_tasks(v_tasks, tmp_b.v_itms[i].v_tasks);
        }

        /* cmp gcd */
        gcd = compute_gcd(v_tasks);

        /* update let item and let task */
        update_let(tmp_b, gcd);

        /* compute total bin load */
        compute_bin_load(tmp_b, target_binload);

        if (side == LEFT) {
                total_binload = c.c_pair.first + target_binload;
                //if (total_binload > b.phi) {
                //        printf("Left Cut %d of size %d cannot fit in Bin %d with potential load: %d\n", 
                //                        c.id, c.c_pair.first, b.id, total_binload);

                //} else {
                //        printf("Left Cut %d of size %d can fit in Bin %d with potential load: %d\n\n", 
                //                        c.id, c.c_pair.first, b.id, total_binload);
                //}
        }

        if (side == RIGHT) {
                total_binload = c.c_pair.second + target_binload;
                //if (total_binload > b.phi) {
                //        printf("Right Cut %d of size %d cannot fit in Bin %d with potential load: %d\n", 
                //                        c.id, c.c_pair.second, b.id, total_binload);

                //} else {
                //        printf("Right Cut %d of size %d can fit in Bin %d with potential load: %d\n\n", 
                //                        c.id, c.c_pair.second, b.id, total_binload);
                //}
        }
        return total_binload;
}
