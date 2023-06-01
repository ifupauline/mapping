#include "communication.h"
#include "sched_analysis.h"

void comm_count(vector<struct bin> &v_bins, vector<struct item> &v_itms, 
                struct context &ctx)
{
        int count;
        int fst_bin_id;
        int scd_bin_id;
        int fst_bin_color;
        int scd_bin_color;
        int tc_idx_count;
        vector<int> v_id;
        vector<struct item> v_tmp;

        fst_bin_id = -1;
        scd_bin_id = -1;

        sort_inc_itm_id(v_itms);

        for (unsigned int i = 0; i < v_itms.size(); i++)
                v_id.push_back(v_itms[i].id);
        sort(v_id.begin(), v_id.end());
        v_id.erase(unique(v_id.begin(), v_id.end()), v_id.end());

        for (unsigned int i = 0; i < v_id.size(); i++) {
                count = 0;
                for (unsigned int j = 0; j < v_itms.size(); j++) {
                        if (v_id[i] == v_itms[j].id) {
                                count++;
                                if (count == 2) {
                                        v_tmp.push_back(v_itms[j]);
                                        v_tmp.push_back(v_itms[j - 1]);
                                }
                                if (count > 2)
                                        v_tmp.push_back(v_itms[j]);
                        }
                }
        }
        sort_inc_itm_id(v_tmp);

        for (unsigned int i = 0; i < v_id.size(); i++) {
                count = 1;
                for (unsigned int j = 0; j < v_tmp.size(); j++) {
                        if (v_id[i] == v_tmp[j].id) {
                                count++;
                                continue;
                        }
                }
                if (count - 2  == -1)
                        break;

                tc_idx_count = count - 2;
                printf("\n<--------------- TC %d frags %d ---------------->\n", 
                                v_id[i], tc_idx_count + 1);

                for (int k = 0; k <= tc_idx_count; k++) {
                        for (unsigned int l = 0; l < v_bins.size(); l++) {
                                for (unsigned int m = 0; m < v_bins[l].v_itms.size(); m++) {
                                        if (k == 0) {
                                                if (v_id[i] == v_bins[l].v_itms[m].id && 
                                                                k == v_bins[l].v_itms[m].tc_idx) {
                                                        printf("Found TC %d tc_idx %d in Core %d\n", 
                                                                        v_id[i], k, v_bins[l].id);
                                                        fst_bin_id = v_bins[l].id;
                                                }
                                                continue;
                                        } 

                                        if (k > 0 && k <= tc_idx_count) {
                                                if (v_id[i] == v_bins[l].v_itms[m].id && 
                                                                k == v_bins[l].v_itms[m].tc_idx) {
                                                        scd_bin_id = v_bins[l].id;
                                                        if (fst_bin_id != scd_bin_id) {
                                                                fst_bin_color = 0;
                                                                scd_bin_color = 0;
                                                                fst_bin_color = retrieve_color_bin(v_bins, fst_bin_id);
                                                                scd_bin_color = retrieve_color_bin(v_bins, scd_bin_id);
                                                                printf("+1\n");
                                                                if (fst_bin_color == scd_bin_color)
                                                                        ctx.intra_comm_count++;
                                                                else
                                                                        ctx.inter_comm_count++;
                                                        }
                                                        printf("Found TC %d tc_idx %d in Core %d\n", 
                                                                        v_id[i], k, v_bins[l].id);

                                                        fst_bin_id = scd_bin_id;
                                                        continue;
                                                }
                                        }
                                }
                        }
                }
        }
}

void e2e_latency(vector<struct bin> &v_bins, vector<struct item> &v_itms, 
                struct context &ctx)
{
        int count;
        int wcrt;
        int wcrt_sum;
        int fst_bin_id;
        int scd_bin_id;
        int tc_idx_count;
        struct bin b;
        vector<int> v_id;
        vector<struct item> v_tmp;

        vector<vector<struct item>> v_frag_itms;
        vector<struct item> v_non_frag_itms;

        fst_bin_id = -1;
        scd_bin_id = -1;

        printf("+=================================+\n");
        printf("| E2E LATENCY NON FRAGMENTED TC   |\n");
        printf("+=================================+\n");

        /* compute e2e latency of non-fragmented TC */
        for (unsigned int i = 0; i < v_bins.size(); i++) {
                for (unsigned int j = 0; j < v_bins[i].v_itms.size(); j++) {
                        if (v_bins[i].v_itms[j].is_let == YES)
                                continue;
                        if (v_bins[i].v_itms[j].is_frag == YES)
                                continue;
                        else
                                v_non_frag_itms.push_back(v_bins[i].v_itms[j]);
                        for (unsigned int k = 0; k < v_bins[i].v_itms[j].v_tasks.size(); k++) {
                                if (k == v_bins[i].v_itms[j].v_tasks.size() - 1) {
                                        printf("E2E Latency of TC %-3d : %-8d microsecs\n", 
                                                        v_bins[i].v_itms[j].id, 
                                                        v_bins[i].v_itms[j].v_tasks[k].r);
                                }
                        }
                }
        }

        printf("+=================================+\n");
        printf("| E2E LATENCY FRAGMENTED TC       |\n");
        printf("+=================================+\n");

        /* compute e2e latency of fragmented TC */
        sort_inc_itm_id(v_itms);

        for (unsigned int i = 0; i < v_itms.size(); i++)
                v_id.push_back(v_itms[i].id);
        sort(v_id.begin(), v_id.end());
        v_id.erase(unique(v_id.begin(), v_id.end()), v_id.end());

        for (unsigned int i = 0; i < v_id.size(); i++) {
                count = 0;
                for (unsigned int j = 0; j < v_itms.size(); j++) {
                        if (v_id[i] == v_itms[j].id) {
                                count++;
                                if (count == 2) {
                                        v_tmp.push_back(v_itms[j]);
                                        v_tmp.push_back(v_itms[j - 1]);
                                }
                                if (count > 2)
                                        v_tmp.push_back(v_itms[j]);
                        }
                }
        }
        sort_inc_itm_id(v_tmp);

        for (unsigned int i = 0; i < v_id.size(); i++) {
                count = 1;
                wcrt = 0;
                wcrt_sum = 0;
                vector<struct item> v_vec;
                for (unsigned int j = 0; j < v_tmp.size(); j++) {
                        if (v_id[i] == v_tmp[j].id) {
                                count++;
                                continue;
                        }
                }
                if (count - 2  == -1)
                        break;

                tc_idx_count = count - 2;

                for (int k = 0; k <= tc_idx_count; k++) {
                        for (unsigned int l = 0; l < v_bins.size(); l++) {
                                for (unsigned int m = 0; m < v_bins[l].v_itms.size(); m++) {
                                        b = {0};
                                        if (k == 0) {
                                                if (v_id[i] == v_bins[l].v_itms[m].id && 
                                                                k == v_bins[l].v_itms[m].tc_idx) {
                                                        fst_bin_id = v_bins[l].id;
                                                        v_vec.push_back(v_bins[l].v_itms[m]);
                                                        continue;
                                                }
                                        } 

                                        if (k > 0 && k <= tc_idx_count) {
                                                if (v_id[i] == v_bins[l].v_itms[m].id && 
                                                                k == v_bins[l].v_itms[m].tc_idx) {
                                                        scd_bin_id = v_bins[l].id;
                                                        if (fst_bin_id != scd_bin_id) {
                                                                b = retrieve_core_by_id(v_bins, fst_bin_id);
                                                                wcrt = retrieve_wcrt(b, v_bins[l].v_itms[m].id, v_bins[l].v_itms[m].tc_idx - 1);
                                                                wcrt_sum += wcrt;
                                                                v_vec.push_back(v_bins[l].v_itms[m]);
                                                        }

                                                        if (k == tc_idx_count) {
                                                                for (unsigned int z = 0; z < v_bins[l].v_itms[m].v_tasks.size(); z++) {
                                                                        if (z == v_bins[l].v_itms[m].v_tasks.size() - 1)
                                                                                wcrt_sum += v_bins[l].v_itms[m].v_tasks[z].r;
                                                                }
                                                                v_vec.push_back(v_bins[l].v_itms[m]);
                                                        }
                                                        fst_bin_id = scd_bin_id;
                                                        continue;
                                                }
                                        }
                                }
                        }
                }
                sort_inc_itm_tc_idx(v_vec);
                v_frag_itms.push_back(v_vec);
                printf("E2E Latency of TC %-3d : %-8d microsecs\n", v_id[i], wcrt_sum);
        }
        verify_tc_schedulability(v_frag_itms, v_non_frag_itms, ctx);
}
