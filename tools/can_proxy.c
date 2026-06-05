#include "motor_3508.h"
#include "rtt_bridge_protocol.h"
#include "socketcan.h"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#define HEADER_SIZE 20U
#define STATE_PAYLOAD_SIZE 28U
#define CURRENT_PAYLOAD_SIZE 10U
#define UDP_BUFFER_SIZE 128U
#define DEFAULT_BIND_IP "10.10.10.31"
#define DEFAULT_PORT 21001
#define DEFAULT_COMMAND_TIMEOUT_MS 100U
#define LOOP_SLEEP_MS 2
#define STATE_PERIOD_MS 20U
#define PRINT_PERIOD_MS 500U
#define ZERO_REPEAT 20
#define DEFAULT_MAX_CURRENT 2000

typedef struct
{
    Motor3508Feedback feedback[CHASSIS_MOTOR_COUNT + 1U];
    int16_t current_command[CHASSIS_MOTOR_COUNT + 1U];
    int16_t target_rpm[CHASSIS_MOTOR_COUNT + 1U];
    uint64_t last_feedback_ms[CHASSIS_MOTOR_COUNT + 1U];
    uint64_t last_rt_ms;
    uint64_t last_current_command_ms;
    uint64_t rx_heartbeat;
    uint64_t rx_current_command;
    uint64_t invalid_packets;
    uint32_t tx_sequence;
    uint16_t current_timeout_ms;
    int peer_known;
    struct sockaddr_in peer;
} ProxyState;

typedef struct
{
    uint32_t magic;
    uint16_t version;
    uint16_t type;
    uint32_t sequence;
    uint32_t monotonic_ms;
    uint16_t payload_length;
    uint16_t flags;
} BridgeHeaderDecoded;

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

static uint16_t read_u16(const uint8_t *buffer, size_t offset)
{
    return (uint16_t)(((uint16_t)buffer[offset] << 8U) | (uint16_t)buffer[offset + 1U]);
}

static uint32_t read_u32(const uint8_t *buffer, size_t offset)
{
    return ((uint32_t)buffer[offset] << 24U) |
           ((uint32_t)buffer[offset + 1U] << 16U) |
           ((uint32_t)buffer[offset + 2U] << 8U) |
           (uint32_t)buffer[offset + 3U];
}

static int16_t read_i16(const uint8_t *buffer, size_t offset)
{
    return (int16_t)read_u16(buffer, offset);
}

static void write_u16(uint8_t *buffer, size_t offset, uint16_t value)
{
    buffer[offset] = (uint8_t)(value >> 8U);
    buffer[offset + 1U] = (uint8_t)(value & 0xFFU);
}

static void write_i16(uint8_t *buffer, size_t offset, int16_t value)
{
    write_u16(buffer, offset, (uint16_t)value);
}

static void write_u32(uint8_t *buffer, size_t offset, uint32_t value)
{
    buffer[offset] = (uint8_t)(value >> 24U);
    buffer[offset + 1U] = (uint8_t)((value >> 16U) & 0xFFU);
    buffer[offset + 2U] = (uint8_t)((value >> 8U) & 0xFFU);
    buffer[offset + 3U] = (uint8_t)(value & 0xFFU);
}

static void write_header(uint8_t *buffer,
                         uint16_t type,
                         uint32_t sequence,
                         uint16_t payload_length,
                         uint16_t flags)
{
    write_u32(buffer, 0U, RTT_BRIDGE_MAGIC);
    write_u16(buffer, 4U, RTT_BRIDGE_VERSION);
    write_u16(buffer, 6U, type);
    write_u32(buffer, 8U, sequence);
    write_u32(buffer, 12U, (uint32_t)(monotonic_ms() & 0xFFFFFFFFU));
    write_u16(buffer, 16U, payload_length);
    write_u16(buffer, 18U, flags);
}

static int parse_header(const uint8_t *buffer, size_t length, BridgeHeaderDecoded *header)
{
    if (length < HEADER_SIZE)
    {
        return 0;
    }

    header->magic = read_u32(buffer, 0U);
    header->version = read_u16(buffer, 4U);
    header->type = read_u16(buffer, 6U);
    header->sequence = read_u32(buffer, 8U);
    header->monotonic_ms = read_u32(buffer, 12U);
    header->payload_length = read_u16(buffer, 16U);
    header->flags = read_u16(buffer, 18U);

    if (header->magic != RTT_BRIDGE_MAGIC || header->version != RTT_BRIDGE_VERSION)
    {
        return 0;
    }

    return length == (HEADER_SIZE + (size_t)header->payload_length);
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

    struct sockaddr_in address;
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = htons((uint16_t)port);
    if (inet_pton(AF_INET, bind_ip, &address.sin_addr) != 1)
    {
        close(fd);
        errno = EINVAL;
        return -1;
    }

    if (bind(fd, (struct sockaddr *)&address, sizeof(address)) != 0)
    {
        close(fd);
        return -1;
    }

    return fd;
}

static int16_t clamp_current(int value, int max_abs_current)
{
    if (value > max_abs_current)
    {
        return (int16_t)max_abs_current;
    }
    if (value < -max_abs_current)
    {
        return (int16_t)(-max_abs_current);
    }
    return (int16_t)value;
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
    for (int i = 0; i < ZERO_REPEAT; ++i)
    {
        (void)send_currents(can_fd, currents);
        sleep_ms(2);
    }
}

static void drain_feedback(int can_fd, ProxyState *state)
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
                state->feedback[feedback.id] = feedback;
                state->last_feedback_ms[feedback.id] = feedback.last_update_ms;
            }
        }
    }
}

static uint16_t online_mask(const ProxyState *state, uint64_t now_ms)
{
    uint16_t mask = 0U;
    for (uint8_t i = 1U; i <= CHASSIS_MOTOR_COUNT; ++i)
    {
        if (state->last_feedback_ms[i] != 0U && (now_ms - state->last_feedback_ms[i]) <= 200U)
        {
            mask |= (uint16_t)(1U << (i - 1U));
        }
    }
    return mask;
}

static int send_chassis_state(int udp_fd, ProxyState *state, uint64_t now_ms)
{
    if (!state->peer_known)
    {
        return 0;
    }

    uint8_t packet[HEADER_SIZE + STATE_PAYLOAD_SIZE];
    write_header(
        packet,
        RTT_BRIDGE_MSG_CHASSIS_STATE,
        ++state->tx_sequence,
        STATE_PAYLOAD_SIZE,
        0U);

    size_t offset = HEADER_SIZE;
    for (uint8_t i = 1U; i <= CHASSIS_MOTOR_COUNT; ++i, offset += 2U)
    {
        write_i16(packet, offset, state->target_rpm[i]);
    }
    for (uint8_t i = 1U; i <= CHASSIS_MOTOR_COUNT; ++i, offset += 2U)
    {
        write_i16(packet, offset, state->feedback[i].speed_rpm);
    }
    for (uint8_t i = 1U; i <= CHASSIS_MOTOR_COUNT; ++i, offset += 2U)
    {
        write_i16(packet, offset, state->current_command[i]);
    }
    write_u16(packet, offset, online_mask(state, now_ms));
    offset += 2U;
    write_u16(packet, offset, 0U);

    ssize_t sent = sendto(
        udp_fd,
        packet,
        sizeof(packet),
        0,
        (const struct sockaddr *)&state->peer,
        sizeof(state->peer));
    return sent == (ssize_t)sizeof(packet) ? 0 : -1;
}

static void remember_peer(ProxyState *state, const struct sockaddr_in *peer, uint64_t now_ms)
{
    state->peer = *peer;
    state->peer_known = 1;
    state->last_rt_ms = now_ms;
}

static void receive_udp(int udp_fd,
                        ProxyState *state,
                        int allow_current,
                        int max_current,
                        uint64_t now_ms)
{
    for (;;)
    {
        uint8_t packet[UDP_BUFFER_SIZE];
        struct sockaddr_in peer;
        socklen_t peer_len = sizeof(peer);
        ssize_t length = recvfrom(
            udp_fd,
            packet,
            sizeof(packet),
            0,
            (struct sockaddr *)&peer,
            &peer_len);
        if (length < 0)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                return;
            }
            fprintf(stderr, "UDP receive failed: %s\n", strerror(errno));
            return;
        }

        BridgeHeaderDecoded header;
        if (!parse_header(packet, (size_t)length, &header))
        {
            state->invalid_packets++;
            continue;
        }

        remember_peer(state, &peer, now_ms);

        if (header.type == RTT_BRIDGE_MSG_HEARTBEAT)
        {
            state->rx_heartbeat++;
        }
        else if (header.type == RTT_BRIDGE_MSG_ESTOP)
        {
            memset(state->current_command, 0, sizeof(state->current_command));
        }
        else if (header.type == RTT_BRIDGE_MSG_CURRENT_COMMAND)
        {
            if (header.payload_length != CURRENT_PAYLOAD_SIZE)
            {
                state->invalid_packets++;
                continue;
            }

            state->rx_current_command++;
            if (!allow_current)
            {
                memset(state->current_command, 0, sizeof(state->current_command));
                continue;
            }

            size_t offset = HEADER_SIZE;
            for (uint8_t i = 1U; i <= CHASSIS_MOTOR_COUNT; ++i, offset += 2U)
            {
                state->current_command[i] = clamp_current(read_i16(packet, offset), max_current);
            }
            state->current_timeout_ms = read_u16(packet, offset);
            if (state->current_timeout_ms == 0U)
            {
                state->current_timeout_ms = DEFAULT_COMMAND_TIMEOUT_MS;
            }
            state->last_current_command_ms = now_ms;
        }
    }
}

static void print_status(const ProxyState *state, uint64_t now_ms, int allow_current)
{
    char peer_ip[INET_ADDRSTRLEN] = "-";
    if (state->peer_known)
    {
        (void)inet_ntop(AF_INET, &state->peer.sin_addr, peer_ip, sizeof(peer_ip));
    }

    printf("rt=%s age=%llums hb=%llu cmd=%llu invalid=%llu online=0x%X allow_current=%d ",
           peer_ip,
           (unsigned long long)(state->last_rt_ms == 0U ? 0U : now_ms - state->last_rt_ms),
           (unsigned long long)state->rx_heartbeat,
           (unsigned long long)state->rx_current_command,
           (unsigned long long)state->invalid_packets,
           online_mask(state, now_ms),
           allow_current);

    for (uint8_t i = 1U; i <= CHASSIS_MOTOR_COUNT; ++i)
    {
        printf("M%u rpm=%5d cur=%5d ", i, state->feedback[i].speed_rpm, state->current_command[i]);
    }
    printf("\n");
}

static void usage(const char *program)
{
    fprintf(stderr,
            "Usage: %s [can-ifname] [bind-ip] [port] [--allow-current] [--max-current N]\n"
            "Default: %s can0 %s %d\n",
            program,
            program,
            DEFAULT_BIND_IP,
            DEFAULT_PORT);
}

int main(int argc, char **argv)
{
    const char *ifname = "can0";
    const char *bind_ip = DEFAULT_BIND_IP;
    int port = DEFAULT_PORT;
    int allow_current = 0;
    int max_current = DEFAULT_MAX_CURRENT;

    if (argc >= 2)
    {
        ifname = argv[1];
    }
    if (argc >= 3)
    {
        bind_ip = argv[2];
    }
    if (argc >= 4)
    {
        port = atoi(argv[3]);
    }
    for (int i = 4; i < argc; ++i)
    {
        if (strcmp(argv[i], "--allow-current") == 0)
        {
            allow_current = 1;
        }
        else if (strcmp(argv[i], "--max-current") == 0 && (i + 1) < argc)
        {
            max_current = atoi(argv[++i]);
        }
        else
        {
            usage(argv[0]);
            return 2;
        }
    }

    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    int can_fd = socketcan_open(ifname);
    if (can_fd < 0)
    {
        fprintf(stderr, "Failed to open %s: %s\n", ifname, strerror(errno));
        return 1;
    }

    int udp_fd = open_udp_socket(bind_ip, port);
    if (udp_fd < 0)
    {
        fprintf(stderr, "Failed to bind UDP %s:%d: %s\n", bind_ip, port, strerror(errno));
        socketcan_close(can_fd);
        return 1;
    }

    ProxyState state;
    memset(&state, 0, sizeof(state));
    state.current_timeout_ms = DEFAULT_COMMAND_TIMEOUT_MS;

    printf("can_proxy listening udp://%s:%d can=%s allow_current=%d max_current=%d\n",
           bind_ip,
           port,
           ifname,
           allow_current,
           max_current);

    uint64_t last_state_ms = 0U;
    uint64_t last_print_ms = 0U;
    uint64_t last_current_ms = 0U;

    while (g_running)
    {
        uint64_t now = monotonic_ms();
        drain_feedback(can_fd, &state);
        receive_udp(udp_fd, &state, allow_current, max_current, now);

        if (allow_current &&
            state.last_current_command_ms != 0U &&
            (now - state.last_current_command_ms) <= state.current_timeout_ms)
        {
            if ((now - last_current_ms) >= 10U)
            {
                (void)send_currents(can_fd, state.current_command);
                last_current_ms = now;
            }
        }
        else
        {
            memset(state.current_command, 0, sizeof(state.current_command));
        }

        if ((now - last_state_ms) >= STATE_PERIOD_MS)
        {
            (void)send_chassis_state(udp_fd, &state, now);
            last_state_ms = now;
        }

        if ((now - last_print_ms) >= PRINT_PERIOD_MS)
        {
            print_status(&state, now, allow_current);
            last_print_ms = now;
        }

        sleep_ms(LOOP_SLEEP_MS);
    }

    printf("Sending zero current...\n");
    send_zero_current(can_fd);
    close(udp_fd);
    socketcan_close(can_fd);
    printf("can_proxy stopped.\n");
    return 0;
}
