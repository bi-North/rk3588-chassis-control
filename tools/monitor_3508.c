#include "motor_3508.h"
#include "socketcan.h"

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static volatile sig_atomic_t g_running = 1;

static void handle_signal(int signo)
{
    (void)signo;
    g_running = 0;
}

static uint64_t monotonic_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ((uint64_t)ts.tv_sec * 1000ULL) + ((uint64_t)ts.tv_nsec / 1000000ULL);
}

static void print_usage(const char *program)
{
    fprintf(stderr, "Usage: %s <can_ifname>\n", program);
    fprintf(stderr, "Example: %s can0\n", program);
}

int main(int argc, char **argv)
{
    if (argc != 2)
    {
        print_usage(argv[0]);
        return 2;
    }

    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    const char *ifname = argv[1];
    int can_fd = socketcan_open(ifname);
    if (can_fd < 0)
    {
        fprintf(stderr, "Failed to open %s: %s\n", ifname, strerror(errno));
        return 1;
    }

    printf("Monitoring 3508 feedback on %s. Press Ctrl+C to stop.\n", ifname);

    Motor3508Feedback feedbacks[CHASSIS_MOTOR_COUNT + 1U] = {0};

    while (g_running)
    {
        uint32_t can_id = 0;
        uint8_t data[8] = {0};
        uint8_t dlc = 0;
        int ret = socketcan_receive(can_fd, &can_id, data, &dlc, 1000);

        if (ret < 0)
        {
            fprintf(stderr, "CAN receive failed: %s\n", strerror(errno));
            break;
        }
        if (ret == 0)
        {
            printf("No CAN frame in the last 1000 ms.\n");
            continue;
        }
        if (dlc < 8U)
        {
            continue;
        }

        uint64_t now = monotonic_ms();
        if (motor3508_update_feedback(can_id, data, feedbacks, now))
        {
            Motor3508Feedback *fb = &feedbacks[can_id - MOTOR3508_FEEDBACK_ID_BASE];
            printf("%s id=0x%03X angle=%5d rpm=%6d current=%6d temp=%3dC age=%llums\n",
                   motor3508_feedback_name(fb->id),
                   can_id,
                   fb->angle,
                   fb->speed_rpm,
                   fb->current,
                   fb->temperature,
                   (unsigned long long)(now - fb->last_update_ms));
        }
    }

    socketcan_close(can_fd);
    printf("monitor_3508 stopped.\n");
    return 0;
}
