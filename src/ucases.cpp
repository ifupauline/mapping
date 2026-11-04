#include "ucases.h"

static int chain_static[1][10] = {
        {1000, 2000, 3000, 4000, 5000, 6000, 7000, 8000, 9000, 10000},
};

static int chain_dynamic[1][15] = {
        {1000, 2000, 4000, 8000, 10000, 15000, 20000, 25000, 30000, 40000, 
                50000, 60000, 80000, 90000, 100000},
};

int period_waters2015(int x, int y, int tc_type)
{
        int period;

        period = 0;

        if (tc_type != STATIC && tc_type != DYNAMIC) {
                printf("ERR! task period generation!\n");
                exit(0);
        }

        if (tc_type == STATIC)
                period =  chain_static[x][y];

        if (tc_type == DYNAMIC)
                period =  chain_dynamic[x][y];

        return period;
}

void create_waters2019(struct tc &tc)
{
        struct tc waters2019;
        struct task gps_grabber;
        struct task lidar_grabber;
        struct task cam_grabber;
        struct task localization;
        struct task detection;
        struct task fusion;
        struct task ekf;
        struct task planner;
        struct task control;
        struct task can_write;

        waters2019 = {0};
        gps_grabber = {0};
        lidar_grabber = {0};
        cam_grabber = {0};
        localization = {0};
        detection = {0};
        fusion = {0};
        ekf = {0};
        planner = {0};
        control = {0};
        can_write = {0};

        /* gps_grabber */
        gps_grabber = {0};
        gps_grabber.c = 7000;
        gps_grabber.t = 50000;
        gps_grabber.r = 0;
        gps_grabber.task_id = 0;
        gps_grabber.tc_id = tc.id;
        gps_grabber.is_let = NO;
        gps_grabber.u = ceil(((float)gps_grabber.c/(float)gps_grabber.t) * PERMILL);

        /* lidar_grabber */
        lidar_grabber = {0};
        lidar_grabber.c = 12000;
        lidar_grabber.t = 50000;
        lidar_grabber.r = 0;
        lidar_grabber.task_id = 1;
        lidar_grabber.tc_id = tc.id;
        lidar_grabber.is_let = NO;
        lidar_grabber.u = ceil(((float)lidar_grabber.c/(float)lidar_grabber.t) * PERMILL);

        /* cam_grabber */
        cam_grabber = {0};
        cam_grabber.c = 2000;
        cam_grabber.t = 25000;
        cam_grabber.r = 0;
        cam_grabber.task_id = 2;
        cam_grabber.tc_id = tc.id;
        cam_grabber.is_let = NO;
        cam_grabber.u = ceil(((float)cam_grabber.c/(float)cam_grabber.t) * PERMILL);

        /* localization */
        localization = {0};
        localization.c = 28000;
        localization.t = 50000;
        localization.r = 0;
        localization.task_id = 3;
        localization.tc_id = tc.id;
        localization.is_let = NO;
        localization.u = ceil(((float)localization.c/(float)localization.t) * PERMILL);

        /* detection */
        detection = {0};
        detection.c = 28000;
        detection.t = 50000;
        detection.r = 0;
        detection.task_id = 4;
        detection.tc_id = tc.id;
        detection.is_let = NO;
        detection.u = ceil(((float)detection.c/(float)detection.t) * PERMILL);

        /* ekf */
        ekf = {0};
        ekf.c = 6500;
        ekf.t = 10000;
        ekf.r = 0;
        ekf.task_id = 5;
        ekf.tc_id = tc.id;
        ekf.is_let = NO;
        ekf.u = ceil(((float)ekf.c/(float)ekf.t) * PERMILL);

        /* fusion */
        fusion = {0};
        fusion.c = 25000;
        fusion.t = 50000;
        fusion.r = 0;
        fusion.task_id = 6;
        fusion.tc_id = tc.id;
        fusion.is_let = NO;
        fusion.u = ceil(((float)fusion.c/(float)fusion.t) * PERMILL);

        /* planner */
        planner = {0};
        planner.c = 5000;
        planner.t = 10000;
        planner.r = 0;
        planner.task_id = 7;
        planner.tc_id = tc.id;
        planner.is_let = NO;
        planner.u = ceil(((float)planner.c/(float)planner.t) * PERMILL);

        /* control */
        control = {0};
        control.c = 4500;
        control.t = 10000;
        control.r = 0;
        control.task_id = 8;
        control.tc_id = tc.id;
        control.is_let = NO;
        control.u = ceil(((float)control.c/(float)control.t) * PERMILL);

        waters2019 = {0};
        waters2019.id = 0;
        waters2019.tc_idx = 0;
        waters2019.color = RED;
        waters2019.is_let = NO;
        waters2019.is_assign = NO;

        waters2019.u = gps_grabber.u + lidar_grabber.u + cam_grabber.u + localization.u + 
                detection.u + fusion.u + ekf.u + planner.u + control.u;

        waters2019.v_tasks.push_back(gps_grabber);
        waters2019.v_tasks.push_back(lidar_grabber);
        waters2019.v_tasks.push_back(cam_grabber);
        waters2019.v_tasks.push_back(localization);
        waters2019.v_tasks.push_back(detection);
        /* topological order = task_id order (Table 4.7: EKF xi_0,3, Fusion xi_0,4) */
        waters2019.v_tasks.push_back(ekf);
        waters2019.v_tasks.push_back(fusion);
        waters2019.v_tasks.push_back(planner);
        waters2019.v_tasks.push_back(control);

        /* precedence priorities, as gen_task does (cut relies on them) */
        for (unsigned int i = 0; i < waters2019.v_tasks.size(); i++)
                waters2019.v_tasks[i].p = waters2019.v_tasks[i].task_id + 1;

        tc = waters2019;
}
