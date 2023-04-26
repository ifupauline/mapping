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
        return &b.v_itms[idx].tc.v_tasks[0];
}

static void _init_let_task(struct item &let, struct context &ctx)
{
        struct task t;

        t = {0};

        let.size = 0;
        let.tc.u = 0;
        let.nbr_cut = 0;
        let.frag_id = -1;
        let.disp_count = 0;
        let.swap_count = 0;
        let.is_let = YES;
        let.is_frag = NO;
        let.is_fragmented = NO;
        let.is_allocated = NO;
        let.id = ctx.itms_count;

        t.c = gen_rand(MINLETWCET, MAXLETWCET);
        t.t = 0;
        t.d = t.t;
        t.r = 0;
        t.u = 0;
        t.p = 1; /* always highest priority */
        t.id = 0;

        let.tc.v_tasks.push_back(t);
}

void insert_let_tasks(vector<struct bin> &v_bins, struct context &ctx)
{
        struct item let;

        for (unsigned int i = 0; i < v_bins.size(); i++) {
                let = {0};
                _init_let_task(let, ctx);
                ctx.itms_count++;
                add_itm_to_bin(v_bins, let, v_bins[i].id, ctx, let.size, let.tc.v_tasks[0].t);
                let.is_allocated = YES;
        }
}

void update_let(struct bin &b, int gcd)
{
        struct task *let;

        /* update let task */
        let = _get_let_task(b);
        let->t = gcd;
        let->d = let->t;
        let->u = ceil(((float)let->c / (float)let->t) * PERMILL);

        /* update let item */
        b.v_itms[0].size = let->u;
        b.v_itms[0].tc.u = let->u;
        b.v_itms[0].tc.v_tasks.clear();
        b.v_itms[0].tc.v_tasks.push_back(*let);

        //printf("Update LET\n");
        //printf("LET wcet:        %d\n", let->c);
        //printf("LET period:      %d\n", let->t);
        //printf("LET deadline:    %d\n", let->d);
        //printf("LET utilization: %f\n", let->u);
        //printf("LET Item size:   %d\n", b.v_itms[0].size);
        //printf("LET Item tc.u:   %f\n", b.v_itms[0].tc.u);
}

int check_if_fit(struct bin &b, struct item &itm, struct context &ctx, int &gcd)
{
        int target_binload;
        int total_binload;
        struct bin tmp_b;
        vector<struct task> v_tasks;

        gcd = 0;
        target_binload = 0;
        total_binload = 0;
        tmp_b = b;

        printf("Trying to fit Item %d in Bin %d\n", itm.id, b.id);

        /* copy v_tasks */
        add_tasks_to_v_tasks(v_tasks, itm.tc.v_tasks);
        for (unsigned int i = 0; i < tmp_b.v_itms.size(); i++) {
                add_tasks_to_v_tasks(v_tasks, tmp_b.v_itms[i].tc.v_tasks);
        }

        /* cmp gcd */
        gcd = compute_gcd(v_tasks);

        /* update let item and let task */
        update_let(tmp_b, gcd);

        /* compute total bin load */
        compute_bin_load(tmp_b, target_binload);
        total_binload = itm.size + target_binload;

        if (total_binload > ctx.prm.phi) {
                printf("Item %d of size %d cannot fit in Bin %d with potential load: %d\n", 
                                itm.id, itm.size, b.id, total_binload);

        } else {
                printf("Item %d of size %d can fit in Bin %d with potential load: %d\n\n", 
                                itm.id, itm.size, b.id, total_binload);
        }
        return total_binload;
}
