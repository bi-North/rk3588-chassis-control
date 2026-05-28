#include "motor_3508.h"
#include "pid.h"
#include "socketcan.h"

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define DEFAULT_PERIOD_MS 10
#define DEFAULT_KP 1.80f
#define DEFAULT_KI 0.00f
#define DEFAULT_KD 0.00f
#define DEFAULT_OUTPUT_LIMIT 6500.0f
#define DEFAULT_INTEGRAL_LIMIT 2500.0f
#define DEFAULT_TARGET_LIMIT_RPM 3800
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

static int parse_float(const char *text, float *value)
{
    char *end = NULL;
    float parsed = strtof(text, &end);
    if (end == text || *end != '\0')
    {
        return 0;
    }
    *value = parsed;
    return 1;
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

static int wait_for_motor_feedback(int can_fd,
                                   int motor_id,
                                   Motor3508Feedback feedbacks[CHASSIS_MOTOR_COUNT + 1U],
                                   int timeout_ms)
{
    uint64_t start = monotonic_ms();
    while ((monotonic_ms() - start) < (uint64_t)timeout_ms)
    {
        uint64_t now = monotonic_ms();
        drain_feedback(can_fd, feedbacks, now);
        if (feedbacks[motor_id].online)
        {
            return 1;
        }
        sleep_ms(5);
    }
    return 0;
}

static void print_usage(const char *program)
{
    fprintf(stderr, "Usage: %s <can_ifname> <motor_id 1-4> <target_rpm> <duration_ms> [kp ki kd] [period_ms]\n", program);
    fprintf(stderr, "Examples:\n");
    fprintf(stderr, "  %s can0 1 500 5000\n", program);
    fprintf(stderr, "  %s can0 1 -500 5000 1.8 0.0 0.0\n", program);
    fprintf(stderr, "  %s can0 1 500 5000 1.8 0.0 0.0 10\n", program);
}

int main(int argc, char **argv)
{
    if (argc != 5 && argc != 8 && argc != 9)
    {
        print_usage(argv[0]);
        return 2;
    }

    int motor_id = 0;
    int target_rpm = 0;
    int duration_ms = 0;
    int period_ms = DEFAULT_PERIOD_MS;
    float kp = DEFAULT_KP;
    float ki = DEFAULT_KI;
    float kd = DEFAULT_KD;

    if (!parse_int(argv[2], &motor_id) ||
        !parse_int(argv[3], &target_rpm) ||
        !parse_int(argv[4], &duration_ms))
    {
        print_usage(argv[0]);
        return 2;
    }

    if (argc == 8 || argc == 9)
    {
        if (!parse_float(argv[5], &kp) ||
            !parse_float(argv[6], &ki) ||
            !parse_float(argv[7], &kd))
        {
            print_usage(argv[0]);
            return 2;
        }
    }

    if (argc == 9 && !parse_int(argv[8], &period_ms))
    {
        print_usage(argv[0]);
        return 2;
    }

    if (motor_id < 1 || motor_id > 4)
    {
        fprintf(stderr, "motor_id must be 1..4.\n");
        return 2;
    }
    if (target_rpm < -DEFAULT_TARGET_LIMIT_RPM || target_rpm > DEFAULT_TARGET_LIMIT_RPM)
    {
        fprintf(stderr, "target_rpm must be in -%d..%d for this test.\n",
                DEFAULT_TARGET_LIMIT_RPM,
                DEFAULT_TARGET_LIMIT_RPM);
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

    Motor3508Feedback feedbacks[CHASSIS_MOTOR_COUNT + 1U] = {0};
    printf("Waiting for %s feedback...\n", motor3508_feedback_name((uint8_t)motor_id));
    if (!wait_for_motor_feedback(can_fd, motor_id, feedbacks, 1000))
    {
        fprintf(stderr, "No feedback from motor %d within 1000 ms. Stop.\n", motor_id);
        send_zero_current(can_fd);
        socketcan_close(can_fd);
        return 1;
    }

    PIDController pid;
    pid_init(&pid,
             kp,
             ki,
             kd,
             DEFAULT_OUTPUT_LIMIT,
             -DEFAULT_OUTPUT_LIMIT,
             DEFAULT_INTEGRAL_LIMIT,
             -DEFAULT_INTEGRAL_LIMIT);

    printf("Speed PID test on %s %s: target=%d rpm duration=%dms period=%dms kp=%.3f ki=%.3f kd=%.3f\n",
           ifname,
           motor3508_feedback_name((uint8_t)motor_id),
           target_rpm,
           duration_ms,
           period_ms,
           kp,
           ki,
           kd);
    printf("Lift the chassis. Press Ctrl+C to stop and send zero current.\n");

    int16_t currents[CHASSIS_MOTOR_COUNT + 1U] = {0};
    uint64_t start_ms = monotonic_ms();
    uint64_t last_loop_ms = start_ms;
    uint64_t last_print_ms = 0;

    while (g_running)
    {
        uint64_t now = monotonic_ms();
        if ((now - start_ms) >= (uint64_t)duration_ms)
        {
            break;
        }

        drain_feedback(can_fd, feedbacks, now);
        Motor3508Feedback *fb = &feedbacks[motor_id];

        if (!fb->online || (now - fb->last_update_ms) > 200U)
        {
            fprintf(stderr, "%s feedback timeout. Stop.\n", motor3508_feedback_name((uint8_t)motor_id));
            break;
        }

        float dt_s = (float)(now - last_loop_ms) / 1000.0f;
        last_loop_ms = now;

        float pid_output = pid_calculate(&pid, (float)target_rpm, (float)fb->speed_rpm, dt_s);
        currents[motor_id] = (int16_t)pid_output;

        if (send_currents(can_fd, currents) < 0)
        {
            fprintf(stderr, "CAN send failed: %s\n", strerror(errno));
            break;
        }

        if ((now - last_print_ms) >= 100U)
        {
            printf("%s target=%6d rpm=%6d cmd_current=%6d fb_current=%6d temp=%3dC err=%8.1f\n",
                   motor3508_feedback_name((uint8_t)motor_id),
                   target_rpm,
                   fb->speed_rpm,
                   currents[motor_id],
                   fb->current,
                   fb->temperature,
                   pid.error);
            last_print_ms = now;
        }

        sleep_ms(period_ms);
    }

    printf("Sending zero current...\n");
    send_zero_current(can_fd);
    socketcan_close(can_fd);
    printf("test_speed_pid stopped.\n");
    return 0;
}
