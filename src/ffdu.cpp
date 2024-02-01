#include "let.h"
#include "print.h"
#include "mapping.h"
#include "generator.h"

static int _find_frst_core(vector<struct core> &v_cores, struct tc &tc,
                struct context &ctx, int &frst_load, int &frst_gcd, int color)
{
        int core_id;

        core_id = 0;

        for (unsigned int i = 0; i < v_cores.size(); i++) {
                if (v_cores[i].color != color && color != WHITE) 
                        continue;

                frst_load = check_if_fit_tc(v_cores[i], tc, frst_gcd);
                if (frst_load <= v_cores[i].phi) {
                        core_id = v_cores[i].id;
                        return core_id;
                }
        }
        return -1;
}

void ffdu(vector<struct tc> &v_tcs, vector<struct core> &v_cores, 
                struct context &ctx)
{
        int n;
        int ret;
        int load;
        int gcd;
        int core_id;
        int alloc_count;
        int cycl_count;

        cycl_count = 0;
        n = v_tcs.size();
        sort_inc_tc_color(v_tcs);

        /* TODO sort by size also */

        /* STEP - 1, place all possible tcs in cores using FRST */
        printf("\n<--------------------------------------->\n");
        printf("STEP 1, FFST\n");
        printf("<--------------------------------------->\n");
        while (alloc_count != n) {
                alloc_count = 0;
                for (int i = 0; i < n; i++) {
                        ret = 0;
                        gcd = 0;
                        load = 0;
                        core_id = 0;
                        if (v_tcs[i].is_allocated == YES) 
                                continue;

                        /* find best core to fit tc */
                        ret = _find_frst_core(v_cores, v_tcs[i], ctx, load, gcd, v_tcs[i].color);

                        /* core found add tc to it */
                        if (ret != -1) {
                                printf("First Core to accomodate TC %d idx: %d is core %d\n", 
                                                v_tcs[i].id, v_tcs[i].tc_idx, ret);
                                core_id = ret;
                                add_tc_to_v_cores(v_cores, v_tcs[i], core_id, ctx, 
                                                load, gcd);
                                v_tcs[i].is_allocated = YES;
                                continue;

                                /* no core was found */
                        } else {
                                printf("No Core was found to accomodate TC %d idx: %d size: %d\n", 
                                                v_tcs[i].id, v_tcs[i].tc_idx, v_tcs[i].size);

                                if (v_tcs[i].color == WHITE) {
                                        add_core(v_cores, gen_rand(0, 5), 1, ctx);
                                        cycl_count++;

                                } else {
                                        add_core(v_cores, v_tcs[i].color, 1, ctx);
                                        cycl_count++;
                                }

                                if (cycl_count > 10000) {
                                        printf("ERR!: Impossible to allocate all TC, System Unfeasible\n");
                                        STATE = FAILED;
                                        return;
                                }
                                continue;
                        }
                }
                /* count remaining tc to be allocated */
                for (int i = 0; i < n; i++) {
                        if (v_tcs[i].is_allocated == YES)
                                alloc_count++;
                }
        }
}
