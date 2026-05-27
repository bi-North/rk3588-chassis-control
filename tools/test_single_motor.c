#include "motor_3508.h"
#include "socketcan.h"

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define DEFAULT_PERIOD_MS 10
#define MAX_TEST_CURRENT 5000
#define ZERO_SEND_REPEAT 10

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

static void sleep_ms(int ms)
{
    if (ms <= 0)
    {
        return;
    }

    struct timespec req;
    req.tv_sec = ms / 1000;
    req.tv_nsec = (long)(ms % 1000) * 1000000L;
    nanosleep(&req, NULL);
}

static int send_currents(int can_fd, const int16_t currents[CHASSIS_MOTOR_COUNT + 1U])
{
    uint8_t data[8];
    motor3508_pack_current_array(currents, data);
    return socketcan_send(can_fd, MOTOR3508_CMD_ID_1_TO_4, data, 8U);
}

static void send_zero_current(int can_fd)
{
    int16_t currents[CHASSIS_MOTOR_COUNT + 1U] = {0};
    for (int i = 0; i < ZERO_SEND_REPEAT; ++i)
    {
        (void)send_currents(can_fd, currents);
        sleep_ms(2);
    }
}

static void print_usage(const char *program)
{
    fprintf(stderr, "Usage: %s <can_ifname> <motor_id 1-4> <current -5000..5000> <duration_ms> [period_ms]\n", program);
    fprintf(stderr, "Example: %s can0 1 500 2000\n", program);
}

static int parse_int(const char *text, int *value)
{
    char *end = NULL;
    long parsed = strtol(text, &end, 10);
    if (end == text || *end != '\0')
    {
        return 0;
    }
    *value = (int)parsed;
    return 1;
}

static void drain_feedback(int can_fd,
                           Motor3508Feedback feedbacks[CHASSIS_MOTOR_COUNT + 1U],
                           uint64_t now)
{
    for (;;)
    {
        uint32_t can_id = 0;
        uint8_t data[8] = {0};
        uint8_t dlc = 0;
        int ret = socketcan_receive(can_fd, &can_id, data, &dlc, 0);
        if (ret <= 0)
        {
            return;
        }
        if (dlc >= 8U)
        {
            (void)motor3508_update_feedback(can_id, data, feedbacks, now);
        }
    }
}

int main(int argc, char **argv)
{
    if (argc != 5 && argc != 6)
    {
        print_usage(argv[0]);
        return 2;
    }

    int motor_id = 0;
    int current = 0;
    int duration_ms = 0;
    int period_ms = DEFAULT_PERIOD_MS;

    if (!parse_int(argv[2], &motor_id) ||
        !parse_int(argv[3], &current) ||
        !parse_int(argv[4], &duration_ms) ||
        (argc == 6 && !parse_int(argv[5], &period_ms)))
    {
        print_usage(argv[0]);
        return 2;
    }

    if (motor_id < 1 || motor_id > 4)
    {
        fprintf(stderr, "motor_id must be 1..4.\n");
        return 2;
    }
    if (current < -MAX_TEST_CURRENT || current > MAX_TEST_CURRENT)
    {
        fprintf(stderr, "current must be in -%d..%d for this test tool.\n", MAX_TEST_CURRENT, MAX_TEST_CURRENT);
        return 2;
    }
    if (duration_ms <= 0 || period_ms <= 0)
    {
        fprintf(stderr, "duration_ms and period_ms must be positive.\n");
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

    printf("Testing motor %d on %s: current=%d duration=%dms period=%dms.\n",
           motor_id,
           ifname,
           current,
           duration_ms,
           period_ms);
    printf("Lift the chassis. Press Ctrl+C to stop and send zero current.\n");

    Motor3508Feedback feedbacks[CHASSIS_MOTOR_COUNT + 1U] = {0};
    int16_t currents[CHASSIS_MOTOR_COUNT + 1U] = {0};
    currents[motor_id] = (int16_t)current;

    uint64_t start_ms = monotonic_ms();
    uint64_t last_print_ms = 0;

    while (g_running)
    {
        uint64_t now = monotonic_ms();
        if ((now - start_ms) >= (uint64_t)duration_ms)
        {
            break;
        }

        drain_feedback(can_fd, feedbacks, now);

        if (send_currents(can_fd, currents) < 0)
        {
            fprintf(stderr, "CAN send failed: %s\n", strerror(errno));
            break;
        }

        if ((now - last_print_ms) >= 100U)
        {
            Motor3508Feedback *fb = &feedbacks[motor_id];
            if (fb->online)
            {
                printf("%s rpm=%6d current_fb=%6d temp=%3dC age=%llums\n",
                       motor3508_feedback_name((uint8_t)motor_id),
                       fb->speed_rpm,
                       fb->current,
                       fb->temperature,
                       (unsigned long long)(now - fb->last_update_ms));
            }
            else
            {
                printf("%s feedback not received yet.\n", motor3508_feedback_name((uint8_t)motor_id));
            }
            last_print_ms = now;
        }

        sleep_ms(period_ms);
    }

    printf("Sending zero current...\n");
    send_zero_current(can_fd);
    socketcan_close(can_fd);
    printf("test_single_motor stopped.\n");
    return 0;
}
