#include "chassis_control.h"
#include "motor_3508.h"
#include "socketcan.h"

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#define CONTROL_PERIOD_MS 10
#define PRINT_PERIOD_MS 100
#define ZERO_SEND_REPEAT 20

static volatile sig_atomic_t g_running = 1;
static struct termios g_original_termios;
static int g_terminal_configured = 0;

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

static void restore_terminal(void)
{
    if (g_terminal_configured)
    {
        tcsetattr(STDIN_FILENO, TCSANOW, &g_original_termios);
        g_terminal_configured = 0;
    }
}

static int setup_terminal(void)
{
    if (tcgetattr(STDIN_FILENO, &g_original_termios) != 0)
    {
        return -1;
    }

    struct termios raw = g_original_termios;
    raw.c_lflag &= (tcflag_t)~(ICANON | ECHO);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 0;

    if (tcsetattr(STDIN_FILENO, TCSANOW, &raw) != 0)
    {
        return -1;
    }

    int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    if (flags < 0)
    {
        restore_terminal();
        return -1;
    }
    if (fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK) != 0)
    {
        restore_terminal();
        return -1;
    }

    g_terminal_configured = 1;
    return 0;
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
    fprintf(stderr, "Usage: %s <can_ifname> [max_translate_rpm] [max_rotate_rpm]\n", program);
    fprintf(stderr, "Example: %s can0 500 400\n", program);
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

static void print_help(void)
{
    printf("\nKeyboard chassis test\n");
    printf("  Hold keys, release to timeout-stop.\n");
    printf("  w/s: forward/backward\n");
    printf("  a/d: left/right strafe\n");
    printf("  q/e: rotate left/right\n");
    printf("  +/-: adjust speed scale\n");
    printf("  space: stop now\n");
    printf("  x: exit\n\n");
}

static int handle_key(int key, ChassisController *controller, float *scale, uint64_t now_ms)
{
    float value = *scale;

    switch (key)
    {
        case 'w':
        case 'W':
            chassis_set_command(controller, value, 0.0f, 0.0f, now_ms);
            break;
        case 's':
        case 'S':
            chassis_set_command(controller, -value, 0.0f, 0.0f, now_ms);
            break;
        case 'a':
        case 'A':
            chassis_set_command(controller, 0.0f, -value, 0.0f, now_ms);
            break;
        case 'd':
        case 'D':
            chassis_set_command(controller, 0.0f, value, 0.0f, now_ms);
            break;
        case 'q':
        case 'Q':
            chassis_set_command(controller, 0.0f, 0.0f, value, now_ms);
            break;
        case 'e':
        case 'E':
            chassis_set_command(controller, 0.0f, 0.0f, -value, now_ms);
            break;
        case '+':
        case '=':
            *scale += 0.1f;
            if (*scale > 1.0f)
            {
                *scale = 1.0f;
            }
            printf("scale=%.2f\n", *scale);
            break;
        case '-':
        case '_':
            *scale -= 0.1f;
            if (*scale < 0.1f)
            {
                *scale = 0.1f;
            }
            printf("scale=%.2f\n", *scale);
            break;
        case ' ':
            chassis_stop(controller, now_ms);
            break;
        case 'x':
        case 'X':
            return 0;
        default:
            break;
    }

    return 1;
}

static void print_status(const ChassisController *controller,
                         const int16_t currents[CHASSIS_MOTOR_COUNT + 1U],
                         float scale,
                         int online)
{
    printf("scale=%.2f online=%d cmd=[%.2f %.2f %.2f] ",
           scale,
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
    if (argc != 2 && argc != 4)
    {
        print_usage(argv[0]);
        return 2;
    }

    ChassisConfig config;
    chassis_default_config(&config);
    if (argc == 4)
    {
        if (!parse_float_arg(argv[2], &config.max_translate_rpm) ||
            !parse_float_arg(argv[3], &config.max_rotate_rpm))
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

    if (setup_terminal() != 0)
    {
        fprintf(stderr, "Failed to configure terminal: %s\n", strerror(errno));
        send_zero_current(can_fd);
        socketcan_close(can_fd);
        return 1;
    }
    atexit(restore_terminal);

    print_help();

    int16_t currents[CHASSIS_MOTOR_COUNT + 1U] = {0};
    float scale = 0.4f;
    uint64_t last_loop_ms = monotonic_ms();
    uint64_t last_print_ms = 0;

    while (g_running)
    {
        uint64_t now = monotonic_ms();
        int key = getchar();
        while (key != EOF)
        {
            if (!handle_key(key, &controller, &scale, now))
            {
                g_running = 0;
                break;
            }
            key = getchar();
        }

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
            print_status(&controller, currents, scale, online);
            last_print_ms = now;
        }

        sleep_ms(CONTROL_PERIOD_MS);
    }

    restore_terminal();
    printf("\nSending zero current...\n");
    send_zero_current(can_fd);
    socketcan_close(can_fd);
    printf("test_chassis_keyboard stopped.\n");
    return 0;
}
