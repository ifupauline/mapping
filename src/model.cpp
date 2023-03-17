#include "model.h"
#include "print.h"
#include "mapping.h"
#include "optimization.h"
#include "sched_analysis.h"

static int _cmp_inc_task_priority(const struct task &a, const struct task &b)
{
        return a.p < b.p;
}

static int _cmp_inc_task_id(const struct task &a, const struct task &b)
{
        return a.id < b.id;
}

static int _cmp_dec_itm_size(const struct item &a, const struct item &b)
{
        return a.size > b.size;
}

static int _cmp_inc_bin_cap_rem(const struct bin &a, const struct bin &b)
{
        return a.cap_rem < b.cap_rem;
}

void sort_inc_task_priority(vector<struct task> &v_tasks)
{
        sort(v_tasks.begin(), v_tasks.end(), _cmp_inc_task_priority);
}

void sort_inc_task_id(vector<struct task> &v_tasks)
{
        sort(v_tasks.begin(), v_tasks.end(), _cmp_inc_task_id);
}

void sort_dec_itm_size(vector<struct item> &v_itms)
{
        sort(v_itms.begin(), v_itms.end(), _cmp_dec_itm_size);
}

void sort_inc_bin_cap_rem(vector<struct bin> &v_bins)
{
        sort(v_bins.begin(), v_bins.end(), _cmp_inc_bin_cap_rem);
}

void add_bin(vector<struct bin> &v_bins, struct context &ctx)
{
        struct bin tmp_bin;

        tmp_bin.id = ctx.bins_count;
        tmp_bin.flag = SCHED_OK;
        tmp_bin.cap_rem = ctx.prm.phi;
        tmp_bin.phi = ctx.prm.phi;
        v_bins.push_back(tmp_bin);
        ctx.bins_count++;
        printf("Bin %d Created\n", ctx.bins_count - 1);
}

void add_itm_to_bin(vector<struct bin> &v_bins, struct item &itm, int bin_id, 
                struct context &ctx)
{
        for (int i = 0; i < ctx.bins_count; i++) {
                if (v_bins[i].id == bin_id) {
                        if (v_bins[i].cap_rem < itm.size) {
                                printf("ERR Bin %d Overflow with itm.size %d\n", 
                                                v_bins[i].id, itm.size);
                                exit(0);
                        }
                        itm.is_allocated = YES;
                        v_bins[i].v_itms.push_back(itm);
                        v_bins[i].cap_rem -= itm.size;

                        if (itm.is_frag == NO) {
                                printf("Item %d added in Bin %d\n\n", 
                                                itm.id, v_bins[i].id);
                                return;

                        } else {
                                printf("Fragment %d added in Bin %d\n\n", 
                                                itm.id, v_bins[i].id);
                                return;
                        }
                }
        }
}

void copy_back_prio_to_tc(struct bin &b)
{
        int itm_idx;
        int task_idx;

        for (unsigned int i = 0; i < b.v_tasks.size(); i++) {
                itm_idx = b.v_tasks[i].idx.itm_idx;
                task_idx = b.v_tasks[i].idx.task_idx;
                b.v_itms[itm_idx].tc.v_tasks[task_idx].p = b.v_tasks[i].p;
        }
}

void copy_back_resp_to_tc(struct bin &b)
{
        int itm_idx;
        int task_idx;

        for (unsigned int i = 0; i < b.v_tasks.size(); i++) {
                itm_idx = b.v_tasks[i].idx.itm_idx;
                task_idx = b.v_tasks[i].idx.task_idx;
                b.v_itms[itm_idx].tc.v_tasks[task_idx].r = b.v_tasks[i].r;
        }
}

void copy_tc_to_v_tasks_with_pos(struct bin &b, int bin_idx, int itm_idx)
{
        for (unsigned int i = 0; i < b.v_itms[itm_idx].tc.v_tasks.size(); i++) {
                b.v_tasks.push_back(b.v_itms[itm_idx].tc.v_tasks[i]);
                b.v_tasks.back().idx.bin_idx = bin_idx;
                b.v_tasks.back().idx.itm_idx = itm_idx;
                b.v_tasks.back().idx.task_idx = i;
        }
        /* sort tasks by id */
        sort_inc_task_id(b.v_tasks);
}

void compute_tc_load(struct item &itm)
{
        for (unsigned int i = 0; i < itm.tc.v_tasks.size(); i++)
                itm.tc.u += itm.tc.v_tasks[i].u;
}

void compute_bin_cap_rem(struct bin &b)
{
        int load;

        load = 0;
        b.cap_rem = 0;

        for (unsigned int i = 0; i < b.v_itms.size(); i++)
                load += b.v_itms[i].size;

        b.cap_rem = b.phi - load;

        if (b.cap_rem < 0) {
                printf("Core: %d cap_rem: %d\n", b.id, b.cap_rem);
                exit(0);
        }
}

void replace_bin_by_id(vector<struct bin> &v_bins, struct bin &b)
{
        for (unsigned int i = 0; i < v_bins.size(); i++) {
                if (v_bins[i].id == b.id) {
                        v_bins[i] = b;
                        return;
                }
        }
}

struct item retrieve_tc_by_id(vector<struct bin> &v_bins, int tc_id)
{
        for (unsigned int i = 0; i < v_bins.size(); i++) {
                for (unsigned int j = 0; j < v_bins[i].v_itms.size(); j++) {
                        if (v_bins[i].v_itms[j].id == tc_id) {
                                return v_bins[i].v_itms[j];
                        }
                }
        }
        printf("ERR! Could not retrieve tc %d by id!\n", tc_id);
        exit(0);
}

struct bin retrieve_core_by_id(vector<struct bin> &v_bins, int bin_id)
{
        for (unsigned int i = 0; i < v_bins.size(); i++) {
                if (v_bins[i].id == bin_id)
                        return v_bins[i];
        }
        printf("ERR! Could not retrieve core by id!\n");
        exit(0);
}

int get_bin_idx_by_id(vector<struct bin> &v_bins, int bin_id)
{
        for (unsigned int i = 0; i < v_bins.size(); i++) {
                if (v_bins[i].id == bin_id)
                        return i;
        }
        return -1;
}

void insert_itm_to_core(struct bin &b, struct item &itm)
{
        b.v_itms.push_back(itm);
        printf("Added TC %d to Core %d\n", itm.id, b.id);
        compute_bin_cap_rem(b);
}

void delete_itm_by_id(struct bin &b, int itm_id)
{
        int flag = NO;
        for (unsigned int j = 0; j < b.v_itms.size(); j++) {
                if (b.v_itms[j].id == itm_id) {
                        b.v_itms.erase(b.v_itms.begin() + j);
                        printf("Removed TC %d from Core %d\n", itm_id, b.id);
                        compute_bin_cap_rem(b);
                        flag = YES;
                }
        }
        if (flag == NO) {
                printf("ERR! TC %d not removed from Core %d\n", itm_id, b.id);
                print_core(b);
                exit(0);
        }
}
