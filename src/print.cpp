#include "print.h"
#include "mapping.h"
#include "sched_analysis.h"

static void _cores_ratio(vector<struct item> &v_itms, struct context &ctx)
{
        int min_nbr_cuts;

        min_nbr_cuts = 0;

        for (unsigned int i = 0; i < v_itms.size(); i++)
                if (v_itms[i].size > PHI)
                        min_nbr_cuts++;

        ctx.p.cr = (float)ctx.bins_count / (float)ctx.bins_min;
}

static void _execution_time(struct context &ctx)
{
        ctx.p.et = ctx.p.frag_time+ ctx.p.allo_time + ctx.p.schd_time + 
                ctx.p.disp_time + ctx.p.swap_time + ctx.p.comm_time;
}

static void _schedulability_rate(struct context &ctx)
{
        ctx.p.sched_rate_allo = ctx.p.sched_rate_allo * PERCENT;
        ctx.p.disp_gain = (ctx.p.sched_rate_disp * PERCENT) - ctx.p.sched_rate_allo;
        ctx.p.swap_gain = (ctx.p.sched_rate_swap * PERCENT) - (ctx.p.sched_rate_disp * PERCENT);
        ctx.p.opti_gain = ctx.p.disp_gain + ctx.p.swap_gain;
        ctx.p.fr = ((float)ctx.cuts_count / (float)ctx.prm.n) * PERCENT;
}

static void _utilization_rate(vector<struct bin> &v_bins, struct context &ctx)
{
        ctx.p.let = 0.0;
        ctx.p.sys = 0.0;
        ctx.p.unu = 0.0;
        ctx.p.maxu = 0.0;

        for (unsigned int i = 0; i < v_bins.size(); i++) {
                ctx.p.sys += v_bins[i].load;
                ctx.p.unu += v_bins[i].load_rem;
                for (unsigned int j = 0; j < v_bins[i].v_itms.size(); j++) {
                        if (v_bins[i].v_itms[j].is_let == YES) 
                                ctx.p.let += v_bins[i].v_itms[j].size;
                }
        }
        ctx.p.let /= PERMILL;
        ctx.p.sys /= PERMILL;
        ctx.p.unu /= PERMILL;
        ctx.p.maxu = ctx.bins_count * ((float)PHI / (float)PERMILL);
}

float sched_rate(vector<struct bin> &v_bins, struct context &ctx)
{
        float sched_rate;

        sched_rate = 0.0;
        ctx.sched_ok_count = 0;
        ctx.sched_failed_count = 0;

        for (unsigned int i = 0; i < v_bins.size(); i++) {
                if (v_bins[i].flag == SCHED_OK)
                        ctx.sched_ok_count++;
        }

        for (unsigned int i = 0; i < v_bins.size(); i++) {
                if (v_bins[i].flag == SCHED_FAILED)
                        ctx.sched_failed_count++;
        }

        sched_rate = (float)ctx.sched_ok_count / (float)ctx.bins_count;

        return sched_rate;
}

void cmp_stats(vector<struct bin> &v_bins, vector<struct item> &v_itms, 
                struct context &ctx)
{
        for (unsigned int i = 0; i < v_bins.size(); i++) {
                for (unsigned int j = 0; j < v_bins[i].v_tasks.size(); j++)
                        ctx.tasks_count++;
        }

        _cores_ratio(v_itms, ctx);
        _schedulability_rate(ctx);
        _execution_time(ctx);
        _utilization_rate(v_bins, ctx);
}

void print_task_chains(vector<struct item> &v_itms)
{
        int tasknbr;

        tasknbr = 0;

        for (unsigned int i = 0; i < v_itms.size(); i++) {
                printf("==============================================\n");
                printf("tc.id: %-3d tc.idx: %-3d u: %.3f memcost: %d\n", 
                                v_itms[i].id, v_itms[i].tc_idx, 
                                (float)v_itms[i].size / PERMILL, 
                                v_itms[i].memcost);
                printf("==============================================\n");
                for (unsigned int j = 0; j < v_itms[i].v_tasks.size(); j++) {
                        printf("tau %d: u: %.3f c: %-6d t: %-6d\n",
                                        v_itms[i].v_tasks[j].id, 
                                        v_itms[i].v_tasks[j].u / PERMILL, 
                                        v_itms[i].v_tasks[j].c, 
                                        v_itms[i].v_tasks[j].t);
                        tasknbr++;
                }
                printf("----------------------------------------------\n");
                printf("\n\n");
        }
        printf("Total Number of Tasks: %d\n", tasknbr);
        printf("Total Number of Task-Chains: %lu\n\n", v_itms.size());
}

void print_core(struct bin &b)
{
        printf("Core: %d Load: %d Lrem: %d MemCost %d\n", b.id, b.load, b.load_rem, b.memcost);
        for (unsigned int i = 0; i < b.v_itms.size(); i++) {
                for (unsigned int j = 0; j < b.v_itms[i].v_tasks.size(); j++) {
                        printf("TC %-3d tc_idx %d u: %-3d tau %-3d p: %-3d itm_idx: %-3d uniq_id: %-3d sched: %d\n", 
                                        b.v_itms[i].id, b.v_itms[i].tc_idx, 
                                        b.v_itms[i].size,
                                        b.v_itms[i].v_tasks[j].id, 
                                        b.v_itms[i].v_tasks[j].p,
                                        b.v_itms[i].v_tasks[j].idx.itm_idx, 
                                        b.v_itms[i].v_tasks[j].uniq_id, b.flag);
                }
        }
        printf("----------------------------------------------------------------------------\n");
        for (unsigned int i = 0; i < b.v_tasks.size(); i++)
                printf("tau %-3d u: %-3d p %-3d itm_idx %-3d tc_id: %-3d uniq_id: %-3d\n", 
                                b.v_tasks[i].id, (int)b.v_tasks[i].u, 
                                b.v_tasks[i].p, b.v_tasks[i].idx.itm_idx, 
                                b.v_tasks[i].tc_id, b.v_tasks[i].uniq_id);
        printf("\n");
}

void print_cores(vector<struct bin> &v_bins, struct context &ctx)
{
        printf("+=====================================+\n");
        if (ctx.prm.a == BFDU_F)
                printf("| PRINT CORE BFDU_F                   |\n");
        if (ctx.prm.a == WFDU_F)
                printf("| PRINT CORE WFDU_F                   |\n");
        if (ctx.prm.a == FRST_F)
                printf("| PRINT CORE FIRST_F                  |\n");
        printf("+=====================================+\n\n");

        for (unsigned int i = 0; i < v_bins.size(); i++)
                compute_bin_memcost(v_bins[i]);

        for (unsigned int i = 0; i < v_bins.size(); i++) {
                printf("+==============================================================+\n");
                printf("|Core: %d\n", v_bins[i].id);
                printf("|Load: %.3f\n", (float)v_bins[i].load / PERMILL);
                printf("|Lrem: %.3f\n", ((float)v_bins[i].load_rem / PERMILL));
                printf("|Memc: %d\n", v_bins[i].memcost);
                if (v_bins[i].flag == SCHED_OK)
                        printf("|Sched: OK\n");

                if (v_bins[i].flag == SCHED_FAILED)
                        printf("|Sched: FAILED\n");

                for (unsigned int j = 0; j < v_bins[i].v_itms.size(); j++) {

                        if (v_bins[i].v_itms[j].is_let == YES){

                                printf("|--------------------------------------------------------------|\n");
                                printf("|LET: %-3d size %-3d gcd %-3d\n", 
                                                v_bins[i].v_itms[j].id, 
                                                v_bins[i].v_itms[j].size,
                                                v_bins[i].v_itms[j].gcd);
                                printf("|--------------------------------------------------------------|\n");
                                for (unsigned int k = 0; k < v_bins[i].v_itms[j].v_tasks.size(); k++) {
                                        printf("|tau: %-2d u: %-.3f p: %-2d r: %-8d c: %-8d t: %d", 
                                                        v_bins[i].v_itms[j].v_tasks[k].id, 
                                                        v_bins[i].v_itms[j].v_tasks[k].u / PERMILL,
                                                        v_bins[i].v_itms[j].v_tasks[k].p,
                                                        v_bins[i].v_itms[j].v_tasks[k].r,
                                                        v_bins[i].v_itms[j].v_tasks[k].c,
                                                        v_bins[i].v_itms[j].v_tasks[k].t);

                                        if (v_bins[i].v_itms[j].v_tasks[k].r > v_bins[i].v_itms[j].v_tasks[k].t)
                                                printf(" ---------------> deadline  missed!");
                                        if (v_bins[i].v_itms[j].v_tasks[k].r == -1 && 
                                                        v_bins[i].flag == SCHED_OK) {
                                                printf("\nERR! \n");
                                                exit(0);

                                        } else
                                                printf("\n");
                                }

                        } else {
                                printf("|--------------------------------------------------------------|\n");
                                printf("|TC:  %-3d tc_idx %d size %-3d gcd %-3d memcost %d\n", 
                                                v_bins[i].v_itms[j].id, 
                                                v_bins[i].v_itms[j].tc_idx, 
                                                v_bins[i].v_itms[j].size,
                                                v_bins[i].v_itms[j].gcd,
                                                v_bins[i].v_itms[j].memcost);

                                printf("|--------------------------------------------------------------|\n");
                                for (unsigned int k = 0; k < v_bins[i].v_itms[j].v_tasks.size(); k++) {
                                        printf("|tau: %-2d u: %-.3f p: %-2d r: %-8d c: %-8d t: %d", 
                                                        v_bins[i].v_itms[j].v_tasks[k].id, 
                                                        v_bins[i].v_itms[j].v_tasks[k].u / PERMILL,
                                                        v_bins[i].v_itms[j].v_tasks[k].p,
                                                        v_bins[i].v_itms[j].v_tasks[k].r,
                                                        v_bins[i].v_itms[j].v_tasks[k].c,
                                                        v_bins[i].v_itms[j].v_tasks[k].t);

                                        if (v_bins[i].v_itms[j].v_tasks[k].r > v_bins[i].v_itms[j].v_tasks[k].t)
                                                printf(" ---------------> deadline  missed!");
                                        if (v_bins[i].v_itms[j].v_tasks[k].r == -1 && 
                                                        v_bins[i].flag == SCHED_OK) {
                                                printf("\nERR!\n");
                                                exit(0);

                                        } else
                                                printf("\n");
                                }
                        }
                }
                printf("+==============================================================+\n\n");
        }
}

void print_vectors(vector<struct bin> &v_bins, vector<struct item> &v_itms, 
                struct context &ctx)
{
        printf("\n+=====================================+\n");
        if (ctx.prm.a == BFDU_F)
                printf("| PRINT VECTORS BFDU_F                |\n");
        if (ctx.prm.a == WFDU_F)
                printf("| PRINT VECTORS WFDU_F                |\n");
        if (ctx.prm.a == FRST_F)
                printf("| PRINT VECTORS FIRST_F               |\n");
        printf("+=====================================+\n");

        int sched_ok;
        int sched_failed;

        sched_ok = 0;
        sched_failed = 0;

        for (int i = 0; i < ctx.prm.n; i++) {
                if (v_itms[i].is_allocated == NO) {
                        printf("ERR! some TC were not allocated!\n");
                        exit(0);
                }
        }

        printf("Vector:\n");
        for (unsigned int i = 0; i < v_itms.size(); i++) {
                if (v_itms[i].is_allocated == YES) {
                        printf(" %u ", v_itms[i].size);
                        ctx.alloc_count++;
                }
        }
        printf("\n");
        printf("Number of Task-Chains allocated: %u\n", ctx.alloc_count);
        printf("\n");

        printf("Vector:\n");
        for (unsigned int i = 0; i < v_bins.size(); i++) {
                if (v_bins[i].flag == SCHED_OK) {
                        printf("%d  ", v_bins[i].id);
                        sched_ok++;
                }
        }
        printf("\n");
        printf("Number of Cores SCHED_OK: %u\n", sched_ok);
        printf("\n");

        printf("Vector:\n");
        for (unsigned int i = 0; i < v_bins.size(); i++) {
                if (v_bins[i].flag == SCHED_FAILED) {
                        printf("%d  ", v_bins[i].id);
                        sched_failed++;
                }
        }
        printf("\n");
        printf("Number of Cores SCHED_FAILED: %u\n", sched_failed);
        printf("\n");
}

void print_stats(vector<struct item> &v_itms, vector<struct bin> &v_bins, 
                struct context &ctx)
{
        printf("\n+===========================================+\n");
        if (ctx.prm.a == BFDU_F)
                printf("| PRINT STATS BFDU_F                        |\n");
        if (ctx.prm.a == WFDU_F)
                printf("| PRINT STATS WFDU_F                        |\n");
        if (ctx.prm.a == FRST_F)
                printf("| PRINT STATS FIRST_F                       |\n");
        printf("+===========================================+\n");

        cmp_stats(v_bins, v_itms, ctx);

        printf("------------------------------------------------------------------------>\n");
        printf("n:      %u\n", ctx.prm.n);
        printf("phi:    %u\n", PHI);
        printf("e:      %u\n", ctx.prm.e);
        if (ctx.prm.a == BFDU_F)
                printf("a:      BFDU_F\n");
        if (ctx.prm.a == WFDU_F)
                printf("a:      WFDU_F\n");
        if (ctx.prm.a == FRST_F)
                printf("a:      FIRST_F\n");
        printf("------------------------------------------------------------------------>\n");
        printf("Initial Number of Cores:       %-3d\n", ctx.bins_min);
        printf("Current Number of Cores:       %-3d\n", ctx.bins_count);
        printf("New Added Cores:               +%-3d\n", ctx.cycl_count);
        printf("------------------------------------------------------------------------>\n");
        printf("Initial Number of TC:          %-3d\n", ctx.prm.n);
        printf("Current Number of TC:          %-3ld\n", v_itms.size());
        printf("Number of TC Allocated:        %-3d\n", ctx.alloc_count);
        printf("Number of TC Fragmented:       %-3d\n", ctx.frags_count);
        printf("Number of Cuts:                %-3d\n", ctx.cuts_count);
        printf("Number of Tasks:               %-3d\n", ctx.tasks_count);
        printf("------------------------------------------------------------------------>\n");
        printf("Fragmentation Time:               %-3.3f ms\n", ctx.p.frag_time * PERMILL);
        printf("Allocation Time:                  %-3.3f ms\n", ctx.p.allo_time * PERMILL);
        printf("Displacement Time:                %-3.3f ms\n", ctx.p.disp_time * PERMILL);
        printf("Swapping Time:                    %-3.3f ms\n", ctx.p.swap_time * PERMILL);
        printf("Communication Time:               %-3.3f ms\n", ctx.p.comm_time * PERMILL);
        printf("------------------------------------------------------------------------>\n");
        if (ctx.prm.a == BFDU_F) {
                printf("WCRT Tests Count:                 %-3d tests\n", bfdu_wcrt_count);
                printf("WCRT Total Computational Time:    %-3.3f ms\n", bfdu_sched_time * PERMILL);
        }
        if (ctx.prm.a == WFDU_F) {
                printf("WCRT Tests Count:                 %-3d tests\n", wfdu_wcrt_count);
                printf("WCRT Total Computational Time:    %-3.3f ms\n", wfdu_sched_time * PERMILL);
        }
        if (ctx.prm.a == FRST_F) {
                printf("WCRT Tests Count:                 %-3d tests\n", frst_wcrt_count);
                printf("WCRT Total Computational Time:    %-3.3f ms\n", frst_sched_time * PERMILL);
        }
        printf("------------------------------------------------------------------------>\n");
        printf("M/M*:                             %-3.3f\n", ctx.p.cr);
        printf("------------------------------------------------------------------------>\n");
        printf("Communication                     %-3d\n", ctx.comm_count);
        printf("------------------------------------------------------------------------>\n");
        printf("Schedulability Rate (allo):       %-3.3f\n", ctx.p.sched_rate_allo);
        printf("Schedulability Rate (disp):       %-3.3f  +%-2d cores\n", 
                        ctx.p.sched_rate_disp * PERCENT, ctx.p.sched_imp_disp);
        printf("Schedulability Rate (swap):       %-3.3f  +%-2d cores\n", 
                        ctx.p.sched_rate_swap * PERCENT, ctx.p.sched_imp_swap);
        printf("------------------------------------------------------------------------>\n");
        printf("Displacement SR Gain:            +%-3.3f\n", ctx.p.disp_gain);
        printf("Swapping SR Gain:                +%-3.3f\n", ctx.p.swap_gain);
        printf("Total Optimization SR Gain:      +%-3.3f\n", ctx.p.opti_gain);
        printf("------------------------------------------------------------------------>\n");
        printf("Total SYS Utilization             %-3.3f\n", (ctx.p.sys / ctx.p.maxu) * PERCENT);
        printf("Total LET Utilization             %-3.3f\n", (ctx.p.let / ctx.p.maxu) * PERCENT);
        printf("------------------------------------------------------------------------>\n");
        printf("Total Execution Time:             %-3.3f ms\n", ctx.p.et * MSEC);
        printf("------------------------------------------------------------------------>\n");
}
