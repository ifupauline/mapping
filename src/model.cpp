#include "let.h"
#include "model.h"
#include "print.h"
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

static int _cmp_inc_bin_load_rem(const struct bin &a, const struct bin &b)
{
        return a.load_rem < b.load_rem;
}

static int _cmp_dec_int(const int &a, const int &b)
{
        return a > b;
}

void sort_dec_int(vector<int> &v_int)
{
        sort(v_int.begin(), v_int.end(), _cmp_dec_int);
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

void sort_inc_bin_load_rem(vector<struct bin> &v_bins)
{
        sort(v_bins.begin(), v_bins.end(), _cmp_inc_bin_load_rem);
}

void add_bin(vector<struct bin> &v_bins, struct context &ctx)
{
        struct bin tmp_bin;
        struct item let;

        /* create and insert bin */
        tmp_bin.id = ctx.bins_count;
        tmp_bin.flag = SCHED_OK;
        tmp_bin.load = 0;
        tmp_bin.load_rem = ctx.prm.phi;
        tmp_bin.phi = ctx.prm.phi;
        tmp_bin.memcost = 0;
        v_bins.push_back(tmp_bin);
        ctx.bins_count++;
        printf("Bin %d Created\n", ctx.bins_count - 1);

        /* create and insert let task */
        let = {0};
        init_let_task(let, ctx);
        add_itm_to_v_bins(v_bins, let, tmp_bin.id, ctx, let.size, let.v_tasks[0].t);
        ctx.itms_count++;
        let.is_allocated = YES;
}

void add_itm_to_bin(struct bin &b, struct item &itm, int load, int gcd)
{
        if (b.phi < load) {
                printf("ERR Bin %d Overflow with itm.size %d\n", 
                                b.id, itm.size);
                exit(0);
        }
        itm.is_allocated = YES;
        b.load = load;
        b.load_rem = b.phi - load;
        b.v_itms.push_back(itm);
        compute_bin_memcost(b);
        update_let(b, gcd);
        b.v_tasks.clear();

        for (unsigned int i = 0; i < b.v_itms.size(); i++) {
                for (unsigned int j = 0; j < b.v_itms[i].v_tasks.size(); j++) {
                        b.v_itms[i].v_tasks[j].idx.itm_idx = i;
                        b.v_itms[i].v_tasks[j].idx.task_idx = j;
                }
        }

        for (unsigned int i = 0; i < b.v_itms.size(); i++)
                add_tasks_to_v_tasks(b.v_tasks, b.v_itms[i].v_tasks);

        //if (itm.is_frag == NO) {
        //        printf("Item %d added in Bin %d\n\n", itm.id, b.id);
        //        return;

        //} else {
        //        printf("Fragment %d added in Bin %d\n\n", itm.id, b.id);
        //        return;
        //}
}

void add_itm_to_v_bins(vector<struct bin> &v_bins, struct item &itm, int bin_id, 
                struct context &ctx, int load, int gcd)
{
        for (int i = 0; i < ctx.bins_count; i++) {
                if (v_bins[i].id == bin_id) {
                        if (v_bins[i].phi < load) {
                                printf("ERR Bin %d Overflow with itm.size %d\n", 
                                                v_bins[i].id, itm.size);
                                exit(0);
                        }
                        itm.is_allocated = YES;
                        v_bins[i].load = load;
                        v_bins[i].load_rem = v_bins[i].phi - load;
                        v_bins[i].v_itms.push_back(itm);
                        compute_bin_memcost(v_bins[i]);
                        update_let(v_bins[i], gcd);
                        v_bins[i].v_tasks.clear();

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

        itm_idx = 0;
        task_idx = 0;

        for (unsigned int i = 0; i < b.v_tasks.size(); i++) {
                itm_idx = b.v_tasks[i].idx.itm_idx;
                task_idx = b.v_tasks[i].idx.task_idx;
                b.v_itms[itm_idx].v_tasks[task_idx].p = b.v_tasks[i].p;
        }
}

void copy_back_resp_to_tc(struct bin &b)
{
        int itm_idx;
        int task_idx;

        itm_idx = 0;
        task_idx = 0;

        for (unsigned int i = 0; i < b.v_tasks.size(); i++) {
                itm_idx = b.v_tasks[i].idx.itm_idx;
                task_idx = b.v_tasks[i].idx.task_idx;
                b.v_itms[itm_idx].v_tasks[task_idx].r = b.v_tasks[i].r;
        }
}

void copy_v_tc_to_v_tasks_with_pos(vector<struct bin> &v_bins)
{
        for (unsigned int i = 0; i < v_bins.size(); i++) {
                v_bins[i].v_tasks.clear();
                for (unsigned int j = 0; j < v_bins[i].v_itms.size(); j++)
                        copy_tc_to_v_tasks_with_pos(v_bins[i], i, j);
        }
}

void copy_tc_to_v_tasks_with_pos(struct bin &b, int bin_idx, int itm_idx)
{
        for (unsigned int i = 0; i < b.v_itms[itm_idx].v_tasks.size(); i++) {
                b.v_itms[itm_idx].v_tasks[i].idx.bin_idx = bin_idx;
                b.v_itms[itm_idx].v_tasks[i].idx.itm_idx = itm_idx;
                b.v_itms[itm_idx].v_tasks[i].idx.task_idx = i;
                b.v_tasks.push_back(b.v_itms[itm_idx].v_tasks[i]);
        }
        /* sort tasks by id */
        sort_inc_task_id(b.v_tasks);
}

void compute_itm_load(struct item &itm)
{
        for (unsigned int i = 0; i < itm.v_tasks.size(); i++)
                itm.size += itm.v_tasks[i].u;
}

void compute_bin_load(struct bin &b, int &load)
{
        for (unsigned int i = 0; i < b.v_itms.size(); i++)
                load += b.v_itms[i].size;
}

void compute_bin_load_rem(struct bin &b)
{
        b.load = 0;
        b.load_rem = 0;

        for (unsigned int i = 0; i < b.v_itms.size(); i++)
                b.load += b.v_itms[i].size;

        b.load_rem = b.phi - b.load;

        if (b.load_rem < 0) {
                printf("Core: %d load_rem: %d\n", b.id, b.load_rem);
                exit(0);
        }
}

void compute_bin_memcost(struct bin &b)
{
        b.memcost = 0;
        for (unsigned int i = 0; i < b.v_itms.size(); i++) {
                /* let task has no memcost */
                if (b.v_itms[i].is_let == YES)
                        continue;

                b.memcost += b.v_itms[i].memcost;
        }

        if (b.memcost < 0) {
                printf("Core: %d memcost: %d\n", b.id, b.memcost);
                exit(0);
        }
}

int compute_gcd(vector<struct task> &v_tasks)
{
        int gcd;

        gcd = 0;

        /* if there is only one task (cut) gcd = t */
        if (v_tasks.size() == 1) {
                gcd = v_tasks[0].t;
                return gcd;
        }

        /* if there is only 2 values return gcd */
        if (v_tasks.size() == 2) {
                gcd =__gcd(v_tasks[0].t, v_tasks[1].t);
                return gcd;
        }

        /* cmp gcd for array */
        gcd =__gcd(v_tasks[0].t, v_tasks[1].t);

        for (unsigned int i = 2; i < v_tasks.size(); i++)
                gcd = __gcd(gcd, v_tasks[i].t);

        return gcd;
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
        compute_bin_load_rem(b);
}

void delete_itm_by_id(struct bin &b, int itm_id)
{
        int gcd;
        int flag;
        vector<struct task> v_tasks;

        gcd = 0;
        flag = NO;

        for (unsigned int i = 0; i < b.v_itms.size(); i++) {
                if (b.v_itms[i].id == itm_id) {
                        b.v_itms.erase(b.v_itms.begin() + i);
                        b.v_tasks.clear();
                        flag = YES;
                }
        }

        if (flag == NO) {
                printf("ERR! TC %d not removed from Core %d\n", itm_id, b.id);
                print_core(b);
                exit(0);
        }

        /* check if there is only LET item */
        if (b.v_itms.size() == 1)
                gcd = b.v_itms[0].v_tasks[0].t;

        else {
                for (unsigned int j = 0; j < b.v_itms.size(); j++) {
                        if (b.v_itms[j].is_let == YES)
                                continue;
                        add_tasks_to_v_tasks(v_tasks, b.v_itms[j].v_tasks);
                }
                gcd = compute_gcd(v_tasks);
        }
        update_let(b, gcd);
        compute_bin_load_rem(b);
        compute_bin_memcost(b);
}

void add_tasks_to_v_tasks(vector<struct task> &dst_v_tasks, 
                vector<struct task> &src_v_tasks)
{
        for (unsigned int i = 0; i < src_v_tasks.size(); i++)
                dst_v_tasks.push_back(src_v_tasks[i]);
}
