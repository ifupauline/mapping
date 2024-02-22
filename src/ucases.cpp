#include "ucases.h"

static int chain_static[1][6] = {
        {1000, 2000, 5000, 10000, 20000, 50000},
};

static int chain_dynamic[1][8] = {
        {1000, 2000, 5000, 10000, 20000, 50000, 100000, 200000},
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
        struct task can_polling;
        struct task lidar;
        struct task cam_grabber;
        struct task localization;
        struct task detection;
        struct task lane_detection;
        struct task ekf;
        struct task planner;
        struct task control;
        struct task can_write;

        waters2019 = {0};
        can_polling = {0};
        lidar = {0};
        cam_grabber = {0};
        localization = {0};
        detection = {0};
        lane_detection = {0};
        ekf = {0};
        planner = {0};
        control = {0};
        can_write = {0};

        /* can_polling */
        can_polling = {0};
        can_polling.c = 499;
        can_polling.t = 10000;
        can_polling.r = 0;
        can_polling.task_id = 0;
        can_polling.tc_id = tc.id;
        can_polling.is_let = NO;
        can_polling.u = ceil(((float)can_polling.c/(float)can_polling.t) * PERMILL);

        /* lidar */
        lidar = {0};
        lidar.c = 11759;
        lidar.t = 33000;
        lidar.r = 0;
        lidar.task_id = 1;
        lidar.tc_id = tc.id;
        lidar.is_let = NO;
        lidar.u = ceil(((float)lidar.c/(float)lidar.t) * PERMILL);

        /* cam_grabber */
        cam_grabber = {0};
        cam_grabber.c = 14986;
        cam_grabber.t = 33000;
        cam_grabber.r = 0;
        cam_grabber.task_id = 2;
        cam_grabber.tc_id = tc.id;
        cam_grabber.is_let = NO;
        cam_grabber.u = ceil(((float)cam_grabber.c/(float)cam_grabber.t) * PERMILL);

        /* localization */
        localization = {0};
        localization.c = 142375;
        localization.t = 400000;
        localization.r = 0;
        localization.task_id = 3;
        localization.tc_id = tc.id;
        localization.is_let = NO;
        localization.u = ceil(((float)localization.c/(float)localization.t) * PERMILL);

        /* detection */
        detection = {0};
        detection.c = 124956;
        detection.t = 200000;
        detection.r = 0;
        detection.task_id = 4;
        detection.tc_id = tc.id;
        detection.is_let = NO;
        detection.u = ceil(((float)detection.c/(float)detection.t) * PERMILL);

        /* lane detection */
        lane_detection = {0};
        lane_detection.c = 36416;
        lane_detection.t = 66000;
        lane_detection.r = 0;
        lane_detection.task_id = 5;
        lane_detection.tc_id = tc.id;
        lane_detection.is_let = NO;
        lane_detection.u = ceil(((float)lane_detection.c/(float)lane_detection.t) * PERMILL);

        /* ekf */
        ekf = {0};
        ekf.c = 4399;
        ekf.t = 15000;
        ekf.r = 0;
        ekf.task_id = 6;
        ekf.tc_id = tc.id;
        ekf.is_let = NO;
        ekf.u = ceil(((float)ekf.c/(float)ekf.t) * PERMILL);

        /* planner */
        planner = {0};
        planner.c = 11371;
        planner.t = 15000;
        planner.r = 0;
        planner.task_id = 7;
        planner.tc_id = tc.id;
        planner.is_let = NO;
        planner.u = ceil(((float)planner.c/(float)planner.t) * PERMILL);

        /* control */
        control = {0};
        control.c = 1609;
        control.t = 5000;
        control.r = 0;
        control.task_id = 8;
        control.tc_id = tc.id;
        control.is_let = NO;
        control.u = ceil(((float)control.c/(float)control.t) * PERMILL);

        /* can write */
        can_write = {0};
        can_write.c = 499;
        can_write.t = 10000;
        can_write.r = 0;
        can_write.task_id = 9;
        can_write.tc_id = tc.id;
        can_write.is_let = NO;
        can_write.u = ceil(((float)can_write.c/(float)can_write.t) * PERMILL);

        waters2019 = {0};
        waters2019.id = 0;
        waters2019.tc_idx = 0;
        waters2019.comcost = 3;
        waters2019.color = RED;
        waters2019.is_let = NO;
        waters2019.is_alloc = NO;

        waters2019.u = can_polling.u + lidar.u + cam_grabber.u + localization.u + 
                detection.u + lane_detection.u + ekf.u + planner.u + control.u + 
                can_write.u;

        waters2019.v_tasks.push_back(can_polling);
        waters2019.v_tasks.push_back(lidar);
        waters2019.v_tasks.push_back(cam_grabber);
        waters2019.v_tasks.push_back(localization);
        waters2019.v_tasks.push_back(detection);
        waters2019.v_tasks.push_back(lane_detection);
        waters2019.v_tasks.push_back(ekf);
        waters2019.v_tasks.push_back(planner);
        waters2019.v_tasks.push_back(control);
        waters2019.v_tasks.push_back(can_write);

        tc = waters2019;
}
