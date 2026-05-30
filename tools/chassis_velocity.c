#include "chassis_control.h"
#include "motor_3508.h"
#include "socketcan.h"

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define CONTROL_PERIOD_MS 10
#define PRINT_PERIOD_MS 100
#define ZERO_SEND_REPEAT 20

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
    struct timespec req;
    req.tv_sec = ms / 1000;
    req.tv_nsec = (long)(ms % 1000) * 1000000L;
    nanosleep(&req, NULL);
}

static float clamp_float(float value, float min_value, float max_value)
{
    if (value > max_value)
    {
        return max_value;
    }
    if (value < min_value)
    {
        return min_value;
    }
    return value;
}

static int parse_float_arg(const char *text, float *value)
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

static int parse_int_arg(const char *text, int *value)
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

static void drain_feedback(int can_fd, ChassisController *controller)
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
            Motor3508Feedback feedback;
            if (motor3508_parse_feedback(can_id, data, &feedback, monotonic_ms()))
            {
                chassis_update_feedback(controller, &feedback);
            }
        }
    }
}

static int wait_for_all_feedback(int can_fd, ChassisController *controller, int timeout_ms)
{
    uint64_t start = monotonic_ms();
    while ((monotonic_ms() - start) < (uint64_t)timeout_ms)
    {
        drain_feedback(can_fd, controller);
        if (chassis_feedback_all_online(controller, monotonic_ms()))
        {
            return 1;
        }
        sleep_ms(5);
    }
    return 0;
}

static void print_usage(const char *program)
{
    fprintf(stderr, "Usage: %s <can_ifname> <forward> <strafe> <rotate> <duration_ms> [max_translate_rpm] [max_rotate_rpm]\n", program);
    fprintf(stderr, "All command values are normalized from -1.0 to 1.0.\n");
    fprintf(stderr, "Example: %s can0 0.4 0.0 0.0 3000 500 400\n", program);
}

static void print_status(const ChassisController *controller,
                         const int16_t currents[CHASSIS_MOTOR_COUNT + 1U],
                         int online,
                         int remaining_ms)
{
    printf("remain=%dms online=%d cmd=[%.2f %.2f %.2f] ",
           remaining_ms,
           online,
           controller->command.forward,
           controller->command.strafe,
           controller->command.rotate);

    for (uint8_t i = 1U; i <= CHASSIS_MOTOR_COUNT; ++i)
    {
        printf("M%u tgt=%5.0f rpm=%5d cur=%5d ",
               i,
               controller->target_rpm[i],
               controller->feedback[i].speed_rpm,
               currents[i]);
    }
    printf("\n");
}

int main(int argc, char **argv)
{
    if (argc != 6 && argc != 8)
    {
        print_usage(argv[0]);
        return 2;
    }

    ChassisCommand command;
    int duration_ms = 0;
    if (!parse_float_arg(argv[2], &command.forward) ||
        !parse_float_arg(argv[3], &command.strafe) ||
        !parse_float_arg(argv[4], &command.rotate) ||
        !parse_int_arg(argv[5], &duration_ms))
    {
        print_usage(argv[0]);
        return 2;
    }

    if (duration_ms <= 0)
    {
        fprintf(stderr, "duration_ms must be positive.\n");
        return 2;
    }

    command.forward = clamp_float(command.forward, -1.0f, 1.0f);
    command.strafe = clamp_float(command.strafe, -1.0f, 1.0f);
    command.rotate = clamp_float(command.rotate, -1.0f, 1.0f);

    ChassisConfig config;
    chassis_default_config(&config);
    if (argc == 8)
    {
        if (!parse_float_arg(argv[6], &config.max_translate_rpm) ||
            !parse_float_arg(argv[7], &config.max_rotate_rpm))
        {
            print_usage(argv[0]);
            return 2;
        }
    }

    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    int can_fd = socketcan_open(argv[1]);
    if (can_fd < 0)
    {
        fprintf(stderr, "Failed to open %s: %s\n", argv[1], strerror(errno));
        return 1;
    }

    ChassisController controller;
    chassis_init(&controller, &config, monotonic_ms());

    printf("Waiting for all motor feedback...\n");
    if (!wait_for_all_feedback(can_fd, &controller, 1500))
    {
        fprintf(stderr, "Not all motors are online. Stop.\n");
        send_zero_current(can_fd);
        socketcan_close(can_fd);
        return 1;
    }

    printf("Velocity test on %s cmd=[%.2f %.2f %.2f] duration=%dms max_translate=%.0f max_rotate=%.0f\n",
           argv[1],
           command.forward,
           command.strafe,
           command.rotate,
           duration_ms,
           config.max_translate_rpm,
           config.max_rotate_rpm);
    printf("Press Ctrl+C to stop and send zero current.\n");

    int16_t currents[CHASSIS_MOTOR_COUNT + 1U] = {0};
    uint64_t start_ms = monotonic_ms();
    uint64_t last_loop_ms = start_ms;
    uint64_t last_print_ms = 0;

    while (g_running)
    {
        uint64_t now = monotonic_ms();
        int elapsed_ms = (int)(now - start_ms);
        if (elapsed_ms >= duration_ms)
        {
            break;
        }

        chassis_set_command(&controller, command.forward, command.strafe, command.rotate, now);
        drain_feedback(can_fd, &controller);

        float dt_s = (float)(now - last_loop_ms) / 1000.0f;
        last_loop_ms = now;

        int online = chassis_step(&controller, now, dt_s, currents);
        if (!online)
        {
            for (uint8_t i = 0U; i <= CHASSIS_MOTOR_COUNT; ++i)
            {
                currents[i] = 0;
            }
        }

        if (send_currents(can_fd, currents) < 0)
        {
            fprintf(stderr, "CAN send failed: %s\n", strerror(errno));
            break;
        }

        if ((now - last_print_ms) >= PRINT_PERIOD_MS)
        {
            print_status(&controller, currents, online, duration_ms - elapsed_ms);
            last_print_ms = now;
        }

        sleep_ms(CONTROL_PERIOD_MS);
    }

    printf("\nSending zero current...\n");
    chassis_stop(&controller, monotonic_ms());
    send_zero_current(can_fd);
    socketcan_close(can_fd);
    printf("chassis_velocity stopped.\n");
    return 0;
}
