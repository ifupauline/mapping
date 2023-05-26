#include "print.h"
#include "communication.h"

void comp_comm(vector<struct bin> &v_bins, vector<struct item> &v_itms, 
                struct context &ctx)
{
        int count;
        int fst_bin_id;
        int scd_bin_id;
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
                                                                printf("+1\n");
                                                                ctx.comm_count++;
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
        int wcrt_sum;
        int fst_bin_id;
        int scd_bin_id;
        int tc_idx_count;
        struct bin b;
        vector<int> v_id;
        vector<struct item> v_tmp;

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
                        if (v_bins[i].v_itms[j].is_frag == YES) {
                                //printf("TC %-3d idx %-3d Core %-3d Fragmented\n", 
                                //                v_bins[i].v_itms[j].id, 
                                //                v_bins[i].v_itms[j].tc_idx, 
                                //                v_bins[i].id);
                                continue;
                        }
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
                wcrt_sum = 0;
                for (unsigned int j = 0; j < v_tmp.size(); j++) {
                        if (v_id[i] == v_tmp[j].id) {
                                count++;
                                continue;
                        }
                }
                if (count - 2  == -1)
                        break;

                tc_idx_count = count - 2;
                //printf("\n<--------------- TC %d frags %d ---------------->\n", 
                //                v_id[i], tc_idx_count + 1);

                for (int k = 0; k <= tc_idx_count; k++) {
                        for (unsigned int l = 0; l < v_bins.size(); l++) {
                                for (unsigned int m = 0; m < v_bins[l].v_itms.size(); m++) {
                                        b = {0};
                                        if (k == 0) {
                                                if (v_id[i] == v_bins[l].v_itms[m].id && 
                                                                k == v_bins[l].v_itms[m].tc_idx) {
                                                        //printf("Found TC %d tc_idx %d in Core %d\n", 
                                                        //                v_id[i], k, v_bins[l].id);
                                                        fst_bin_id = v_bins[l].id;
                                                }
                                                continue;
                                        } 

                                        if (k > 0 && k <= tc_idx_count) {
                                                if (v_id[i] == v_bins[l].v_itms[m].id && 
                                                                k == v_bins[l].v_itms[m].tc_idx) {
                                                        scd_bin_id = v_bins[l].id;
                                                        if (fst_bin_id != scd_bin_id) {
                                                                //printf("+1\n");
                                                                /* find previous TC */
                                                                b = retrieve_core_by_id(v_bins, fst_bin_id);
                                                                //printf("TC %d Core %d tc_idx %d\n", v_bins[l].v_itms[m].id, b.id, tc_idx);
                                                                int wcrt = retrieve_wcrt(b, v_bins[l].v_itms[m].id, v_bins[l].v_itms[m].tc_idx - 1);
                                                                wcrt_sum += wcrt;
                                                                //printf("wcrt: %d\n", wcrt);
                                                        }

                                                        if (k > 0 && k < tc_idx_count) {
                                                                //printf("Found TC %d tc_idx %d in Core %d\n", 
                                                                //                v_id[i],v_bins[l].v_itms[m].tc_idx, v_bins[l].id);
                                                        }

                                                        if (k == tc_idx_count) {
                                                                //printf("Found TC %d tc_idx %d in Core %d\n", 
                                                                //                v_id[i], k, v_bins[l].id);
                                                                for (unsigned int z = 0; z < v_bins[l].v_itms[m].v_tasks.size(); z++) {
                                                                        if (z == v_bins[l].v_itms[m].v_tasks.size() - 1) {
                                                                                //printf("wcrt: %d\n", v_bins[l].v_itms[m].v_tasks[z].r);
                                                                                wcrt_sum += v_bins[l].v_itms[m].v_tasks[z].r;
                                                                        }
                                                                }
                                                        }

                                                        fst_bin_id = scd_bin_id;
                                                        continue;
                                                }
                                        }
                                }
                        }
                }
                printf("E2E Latency of TC %-3d : %-8d microsecs\n", v_id[i], wcrt_sum);
        }
}
