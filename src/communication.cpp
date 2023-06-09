#include "print.h"
#include "communication.h"
#include "sched_analysis.h"

vector<struct tc_comm> v_com_core;
vector<struct tc_comm> v_com_ecu;

void create_ecus(vector<struct bin> &v_bins, vector<struct ecu> &v_ecus)
{
        struct ecu e;

        e = {0};

        for (unsigned int i = 0; i < 6; i++) {
                e.color = i;
                v_ecus.push_back(e);
                printf("Created ECU %d\n", e.color);
        }

        /* add cores to ecus */
        for (unsigned int i = 0; i < v_ecus.size(); i++) {
                for (unsigned int j = 0; j < v_bins.size(); j++) {
                        if (v_bins[j].color == v_ecus[i].color) {
                                v_ecus[i].v_bins.push_back(v_bins[j]);
                                printf("Added Core %-3d color %-3d to ECU %-3d\n", 
                                                v_bins[j].id, v_bins[j].color, 
                                                v_ecus[i].color);
                        }
                }
        }

        /* assign tc to ecu */
        for (unsigned int i = 0; i < v_ecus.size(); i++) {
                for (unsigned int j = 0; j < v_bins.size(); j++) {
                        for (unsigned int k = 0; k < v_bins[j].v_itms.size();k++)
                                v_bins[j].v_itms[k].ecu = v_ecus[i].color;
                }
        }
}

void retrieve_ecu_id_by_bin_id(vector<struct ecu> &v_ecus, int bin_id)
{
        for (unsigned int i = 0; i < v_ecus.size(); i++) {
                for (unsigned int j = 0; j < v_ecus[i].v_bins.size(); j++) {
                        if (v_ecus[i].v_bins[j].id == bin_id) {
                                printf("ECU %d\n", v_ecus[i].color);
                        }
                }
        }
}

static void _convert_paths(vector<struct ecu> &v_ecus, vector<struct tc_comm> &v_com_core,
                vector<struct tc_comm> &v_com_ecu)
{
        v_com_ecu.clear();

        /* for each TC convert paths to ecu_com */
        for (unsigned int i = 0; i < v_com_core.size(); i++) {
                printf("TC %d\n", v_com_core[i].tc_id);
                for (unsigned int j = 0; j < v_com_core[i].path.size(); j++) {
                        retrieve_ecu_id_by_bin_id(v_ecus, v_com_core[i].path[j].src);
                        retrieve_ecu_id_by_bin_id(v_ecus, v_com_core[i].path[j].dst);
                        printf("\n");
                }
                printf("\n");
        }
}

void comm_count(vector<struct bin> &v_bins, vector<struct item> &v_itms, 
                vector<struct ecu> &v_ecus, struct context &ctx)
{
        int count;
        int src_bin_id;
        int dst_bin_id;
        int src_bin_color;
        int dst_bin_color;
        int tc_idx_count;
        vector<int> v_id;
        vector<struct item> v_tmp;

        src_bin_id = -1;
        dst_bin_id = -1;
        v_com_core.clear();

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

                struct tc_comm com;
                com = {0};
                com.tc_id = v_id[i];

                for (int k = 0; k <= tc_idx_count; k++) {
                        for (unsigned int l = 0; l < v_bins.size(); l++) {
                                for (unsigned int m = 0; m < v_bins[l].v_itms.size(); m++) {
                                        if (k == 0) {
                                                if (v_id[i] == v_bins[l].v_itms[m].id && 
                                                                k == v_bins[l].v_itms[m].tc_idx) {
                                                        printf("Found TC %-3d tc_idx %-3d in Core %-3d ECU %d\n", 
                                                                        v_id[i], k, v_bins[l].id, v_bins[l].color);
                                                        src_bin_id = v_bins[l].id;
                                                }
                                                continue;
                                        } 

                                        if (k > 0 && k <= tc_idx_count) {
                                                if (v_id[i] == v_bins[l].v_itms[m].id && 
                                                                k == v_bins[l].v_itms[m].tc_idx) {
                                                        dst_bin_id = v_bins[l].id;
                                                        if (src_bin_id != dst_bin_id) {
                                                                src_bin_color = 0;
                                                                dst_bin_color = 0;
                                                                src_bin_color = retrieve_color_bin(v_bins, src_bin_id);
                                                                dst_bin_color = retrieve_color_bin(v_bins, dst_bin_id);
                                                                printf("+1 ");
                                                                if (src_bin_color == dst_bin_color) {
                                                                        ctx.intra_comm_count++;
                                                                        struct link lk;
                                                                        lk = {0};
                                                                        lk.src = src_bin_id;
                                                                        lk.dst = dst_bin_id;
                                                                        lk.comm_type = INTRA_ECU;
                                                                        com.path.push_back(lk);
                                                                        printf("inter-core\n");
                                                                } else {
                                                                        ctx.inter_comm_count++;
                                                                        struct link lk;
                                                                        lk = {0};
                                                                        lk.src = src_bin_id;
                                                                        lk.dst = dst_bin_id;
                                                                        lk.comm_type = INTER_ECU;
                                                                        com.path.push_back(lk);
                                                                        printf("inter-ecu\n");
                                                                }
                                                        }
                                                        printf("Found TC %-3d tc_idx %-3d in Core %-3d ECU %d\n", 
                                                                        v_id[i], k, v_bins[l].id, v_bins[l].color);

                                                        src_bin_id = dst_bin_id;
                                                        continue;
                                                }
                                        }
                                }
                        }
                }
                print_tc_comm(com);
                v_com_core.push_back(com);
        }

        /* convert path */
        _convert_paths(v_ecus, v_com_core, v_com_ecu);
}

void e2e_latency(vector<struct bin> &v_bins, vector<struct item> &v_itms, 
                struct context &ctx)
{
        int count;
        int wcrt;
        int wcrt_sum;
        int src_bin_id;
        int dst_bin_id;
        int tc_idx_count;
        struct bin b;
        vector<int> v_id;
        vector<struct item> v_tmp;

        vector<vector<struct item>> v_frag_itms;
        vector<struct item> v_non_frag_itms;

        src_bin_id = -1;
        dst_bin_id = -1;

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
                                                        src_bin_id = v_bins[l].id;
                                                        v_vec.push_back(v_bins[l].v_itms[m]);
                                                        continue;
                                                }
                                        } 

                                        if (k > 0 && k <= tc_idx_count) {
                                                if (v_id[i] == v_bins[l].v_itms[m].id && 
                                                                k == v_bins[l].v_itms[m].tc_idx) {
                                                        dst_bin_id = v_bins[l].id;
                                                        if (src_bin_id != dst_bin_id) {
                                                                b = retrieve_core_by_id(v_bins, src_bin_id);
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
                                                        src_bin_id = dst_bin_id;
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
