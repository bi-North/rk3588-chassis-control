#include "chassis_control.h"
#include "motor_3508.h"
#include "socketcan.h"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#define CONTROL_PERIOD_MS 10
#define PRINT_PERIOD_MS 200
#define ZERO_SEND_REPEAT 20
#define DEFAULT_BIND_IP "127.0.0.1"
#define DEFAULT_UDP_PORT 20001
#define UDP_BUFFER_SIZE 128

typedef struct
{
    uint64_t start_ms;
    uint64_t feedback_count[CHASSIS_MOTOR_COUNT + 1U];
    uint64_t udp_command_count;
    uint64_t invalid_command_count;
    uint64_t online_drop_count;
    int last_online;
} DaemonStats;

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

static uint64_t elapsed_ms(uint64_t now_ms, uint64_t previous_ms)
{
    return (now_ms >= previous_ms) ? (now_ms - previous_ms) : 0U;
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

static void drain_feedback(int can_fd, ChassisController *controller, DaemonStats *stats)
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
                stats->feedback_count[feedback.id]++;
            }
        }
    }
}

static int wait_for_all_feedback(int can_fd, ChassisController *controller, DaemonStats *stats, int timeout_ms)
{
    uint64_t start = monotonic_ms();
    while ((monotonic_ms() - start) < (uint64_t)timeout_ms)
    {
        drain_feedback(can_fd, controller, stats);
        if (chassis_feedback_all_online(controller, monotonic_ms()))
        {
            return 1;
        }
        sleep_ms(5);
    }
    return 0;
}

static int open_udp_socket(const char *bind_ip, int port)
{
    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0)
    {
        return -1;
    }

    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0 || fcntl(fd, F_SETFL, flags | O_NONBLOCK) != 0)
    {
        close(fd);
        return -1;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((uint16_t)port);
    if (inet_pton(AF_INET, bind_ip, &addr.sin_addr) != 1)
    {
        errno = EINVAL;
        close(fd);
        return -1;
    }

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) != 0)
    {
        close(fd);
        return -1;
    }

    return fd;
}

static void receive_commands(int udp_fd, ChassisController *controller, DaemonStats *stats, uint64_t now_ms)
{
    for (;;)
    {
        char buffer[UDP_BUFFER_SIZE];
        ssize_t length = recvfrom(udp_fd, buffer, sizeof(buffer) - 1U, 0, NULL, NULL);
        if (length < 0)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                return;
            }
            fprintf(stderr, "UDP receive failed: %s\n", strerror(errno));
            return;
        }

        buffer[length] = '\0';

        float forward = 0.0f;
        float strafe = 0.0f;
        float rotate = 0.0f;
        char extra = '\0';
        int parsed = sscanf(buffer, " %f %f %f %c", &forward, &strafe, &rotate, &extra);
        if (parsed != 3)
        {
            stats->invalid_command_count++;
            fprintf(stderr, "Ignored invalid command: %s\n", buffer);
            continue;
        }

        chassis_set_command(controller, forward, strafe, rotate, now_ms);
        stats->udp_command_count++;
    }
}

static void update_online_stats(DaemonStats *stats, int online)
{
    if (stats->last_online == 1 && online == 0)
    {
        stats->online_drop_count++;
    }
    stats->last_online = online;
}

static void print_status(const ChassisController *controller,
                         const int16_t currents[CHASSIS_MOTOR_COUNT + 1U],
                         int online,
                         uint64_t now_ms,
                         const DaemonStats *stats)
{
    printf("online=%d age=%llums cmd=[%.2f %.2f %.2f] udp=%llu rx=[%llu %llu %llu %llu] drops=%llu ",
           online,
           (unsigned long long)elapsed_ms(now_ms, controller->last_command_ms),
           controller->command.forward,
           controller->command.strafe,
           controller->command.rotate,
           (unsigned long long)stats->udp_command_count,
           (unsigned long long)stats->feedback_count[1],
           (unsigned long long)stats->feedback_count[2],
           (unsigned long long)stats->feedback_count[3],
           (unsigned long long)stats->feedback_count[4],
           (unsigned long long)stats->online_drop_count);

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

static void write_csv(FILE *log_file,
                      const ChassisController *controller,
                      const int16_t currents[CHASSIS_MOTOR_COUNT + 1U],
                      int online,
                      uint64_t now_ms,
                      const DaemonStats *stats)
{
    if (log_file == NULL)
    {
        return;
    }

    fprintf(log_file,
            "%llu,%d,%llu,%.4f,%.4f,%.4f,"
            "%.0f,%d,%d,%.0f,%d,%d,%.0f,%d,%d,%.0f,%d,%d,"
            "%llu,%llu,%llu,%llu,%llu,%llu,%llu\n",
            (unsigned long long)elapsed_ms(now_ms, stats->start_ms),
            online,
            (unsigned long long)elapsed_ms(now_ms, controller->last_command_ms),
            controller->command.forward,
            controller->command.strafe,
            controller->command.rotate,
            controller->target_rpm[1], controller->feedback[1].speed_rpm, currents[1],
            controller->target_rpm[2], controller->feedback[2].speed_rpm, currents[2],
            controller->target_rpm[3], controller->feedback[3].speed_rpm, currents[3],
            controller->target_rpm[4], controller->feedback[4].speed_rpm, currents[4],
            (unsigned long long)stats->udp_command_count,
            (unsigned long long)stats->invalid_command_count,
            (unsigned long long)stats->online_drop_count,
            (unsigned long long)stats->feedback_count[1],
            (unsigned long long)stats->feedback_count[2],
            (unsigned long long)stats->feedback_count[3],
            (unsigned long long)stats->feedback_count[4]);
    fflush(log_file);
}

static void print_usage(const char *program)
{
    fprintf(stderr, "Usage: %s <can_ifname> [bind_ip] [udp_port] [max_translate_rpm] [max_rotate_rpm] [csv_log]\n", program);
    fprintf(stderr, "Example: %s can0 127.0.0.1 20001 500 400 ../docs/chassis_daemon_run.csv\n", program);
}

int main(int argc, char **argv)
{
    if (argc < 2 || argc > 7)
    {
        print_usage(argv[0]);
        return 2;
    }

    const char *bind_ip = (argc >= 3) ? argv[2] : DEFAULT_BIND_IP;
    const char *csv_log_path = (argc >= 7) ? argv[6] : NULL;
    int udp_port = DEFAULT_UDP_PORT;
    ChassisConfig config;
    chassis_default_config(&config);

    if ((argc >= 4 && !parse_int_arg(argv[3], &udp_port)) ||
        (argc >= 5 && !parse_float_arg(argv[4], &config.max_translate_rpm)) ||
        (argc >= 6 && !parse_float_arg(argv[5], &config.max_rotate_rpm)))
    {
        print_usage(argv[0]);
        return 2;
    }
    if (udp_port < 1 || udp_port > 65535)
    {
        fprintf(stderr, "udp_port must be between 1 and 65535.\n");
        return 2;
    }

    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    int can_fd = socketcan_open(argv[1]);
    if (can_fd < 0)
    {
        fprintf(stderr, "Failed to open %s: %s\n", argv[1], strerror(errno));
        return 1;
    }

    int udp_fd = open_udp_socket(bind_ip, udp_port);
    if (udp_fd < 0)
    {
        fprintf(stderr, "Failed to bind UDP %s:%d: %s\n", bind_ip, udp_port, strerror(errno));
        send_zero_current(can_fd);
        socketcan_close(can_fd);
        return 1;
    }

    FILE *log_file = NULL;
    if (csv_log_path != NULL)
    {
        log_file = fopen(csv_log_path, "w");
        if (log_file == NULL)
        {
            fprintf(stderr, "Failed to open CSV log %s: %s\n", csv_log_path, strerror(errno));
            send_zero_current(can_fd);
            close(udp_fd);
            socketcan_close(can_fd);
            return 1;
        }
        fprintf(log_file,
                "elapsed_ms,online,command_age_ms,forward,strafe,rotate,"
                "m1_target_rpm,m1_rpm,m1_current,m2_target_rpm,m2_rpm,m2_current,"
                "m3_target_rpm,m3_rpm,m3_current,m4_target_rpm,m4_rpm,m4_current,"
                "udp_commands,invalid_commands,online_drops,m1_feedback,m2_feedback,m3_feedback,m4_feedback\n");
    }

    ChassisController controller;
    chassis_init(&controller, &config, monotonic_ms());
    DaemonStats stats = {0};
    stats.start_ms = monotonic_ms();
    stats.last_online = -1;

    printf("Waiting for all motor feedback...\n");
    if (!wait_for_all_feedback(can_fd, &controller, &stats, 1500))
    {
        fprintf(stderr, "Not all motors are online. Stop.\n");
        send_zero_current(can_fd);
        if (log_file != NULL)
        {
            fclose(log_file);
        }
        close(udp_fd);
        socketcan_close(can_fd);
        return 1;
    }

    printf("chassis_daemon listening on udp://%s:%d can=%s max_translate=%.0f max_rotate=%.0f timeout=%.0fms\n",
           bind_ip,
           udp_port,
           argv[1],
           config.max_translate_rpm,
           config.max_rotate_rpm,
           config.command_timeout_ms);
    printf("Command format: <forward> <strafe> <rotate>. Press Ctrl+C to stop.\n");
    if (csv_log_path != NULL)
    {
        printf("CSV logging enabled: %s\n", csv_log_path);
    }

    int16_t currents[CHASSIS_MOTOR_COUNT + 1U] = {0};
    uint64_t last_loop_ms = monotonic_ms();
    uint64_t last_print_ms = 0;

    while (g_running)
    {
        uint64_t now = monotonic_ms();

        receive_commands(udp_fd, &controller, &stats, now);
        drain_feedback(can_fd, &controller, &stats);

        float dt_s = (float)(now - last_loop_ms) / 1000.0f;
        last_loop_ms = now;

        int online = chassis_step(&controller, now, dt_s, currents);
        update_online_stats(&stats, online);
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
            print_status(&controller, currents, online, now, &stats);
            write_csv(log_file, &controller, currents, online, now, &stats);
            last_print_ms = now;
        }

        sleep_ms(CONTROL_PERIOD_MS);
    }

    printf("\nSending zero current...\n");
    chassis_stop(&controller, monotonic_ms());
    send_zero_current(can_fd);
    if (log_file != NULL)
    {
        fclose(log_file);
    }
    close(udp_fd);
    socketcan_close(can_fd);
    printf("chassis_daemon stopped.\n");
    return 0;
}
