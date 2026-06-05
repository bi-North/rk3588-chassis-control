#include <rtthread.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>

#define RTT_BRIDGE_MAGIC 0x524B4348U
#define RTT_BRIDGE_VERSION 1U
#define RTT_BRIDGE_MSG_CHASSIS_STATE 2U
#define RTT_BRIDGE_MSG_HEARTBEAT 3U
#define RTT_BRIDGE_MSG_CURRENT_COMMAND 5U

#define RTT_BRIDGE_HEADER_SIZE 20U
#define RTT_BRIDGE_STATE_PAYLOAD_SIZE 28U
#define RTT_BRIDGE_CURRENT_PAYLOAD_SIZE 10U
#define RTT_BRIDGE_LOCAL_PORT 21001
#define RTT_BRIDGE_LINUX_IP "10.10.10.31"
#define RTT_BRIDGE_LINUX_PORT 21001
#define RTT_BRIDGE_THREAD_STACK_SIZE 4096
#define RTT_BRIDGE_THREAD_PRIORITY 18
#define RTT_BRIDGE_THREAD_TICK 10
#define RTT_BRIDGE_START_DELAY_MS 5000
#define RTT_BRIDGE_PERIOD_MS 20
#define RTT_BRIDGE_PRINT_PERIOD_MS 1000

typedef struct
{
    rt_int16_t target_rpm[4];
    rt_int16_t feedback_rpm[4];
    rt_int16_t current_command[4];
    rt_uint16_t online_mask;
    rt_uint16_t fault_mask;
} RttChassisState;

static rt_thread_t bridge_thread;
static rt_uint32_t tx_sequence;

static rt_uint16_t read_u16(const rt_uint8_t *buffer, rt_size_t offset)
{
    return (rt_uint16_t)(((rt_uint16_t)buffer[offset] << 8U) | (rt_uint16_t)buffer[offset + 1U]);
}

static rt_uint32_t read_u32(const rt_uint8_t *buffer, rt_size_t offset)
{
    return ((rt_uint32_t)buffer[offset] << 24U) |
           ((rt_uint32_t)buffer[offset + 1U] << 16U) |
           ((rt_uint32_t)buffer[offset + 2U] << 8U) |
           (rt_uint32_t)buffer[offset + 3U];
}

static rt_int16_t read_i16(const rt_uint8_t *buffer, rt_size_t offset)
{
    return (rt_int16_t)read_u16(buffer, offset);
}

static void write_u16(rt_uint8_t *buffer, rt_size_t offset, rt_uint16_t value)
{
    buffer[offset] = (rt_uint8_t)(value >> 8U);
    buffer[offset + 1U] = (rt_uint8_t)(value & 0xFFU);
}

static void write_i16(rt_uint8_t *buffer, rt_size_t offset, rt_int16_t value)
{
    write_u16(buffer, offset, (rt_uint16_t)value);
}

static void write_u32(rt_uint8_t *buffer, rt_size_t offset, rt_uint32_t value)
{
    buffer[offset] = (rt_uint8_t)(value >> 24U);
    buffer[offset + 1U] = (rt_uint8_t)((value >> 16U) & 0xFFU);
    buffer[offset + 2U] = (rt_uint8_t)((value >> 8U) & 0xFFU);
    buffer[offset + 3U] = (rt_uint8_t)(value & 0xFFU);
}

static rt_uint32_t monotonic_ms32(void)
{
    return (rt_uint32_t)(rt_tick_get_millisecond() & 0xFFFFFFFFU);
}

static void write_header(rt_uint8_t *buffer,
                         rt_uint16_t type,
                         rt_uint32_t sequence,
                         rt_uint16_t payload_length,
                         rt_uint16_t flags)
{
    write_u32(buffer, 0U, RTT_BRIDGE_MAGIC);
    write_u16(buffer, 4U, RTT_BRIDGE_VERSION);
    write_u16(buffer, 6U, type);
    write_u32(buffer, 8U, sequence);
    write_u32(buffer, 12U, monotonic_ms32());
    write_u16(buffer, 16U, payload_length);
    write_u16(buffer, 18U, flags);
}

static int parse_chassis_state(const rt_uint8_t *packet, rt_size_t length, RttChassisState *state)
{
    if (length != (RTT_BRIDGE_HEADER_SIZE + RTT_BRIDGE_STATE_PAYLOAD_SIZE))
    {
        return 0;
    }
    if (read_u32(packet, 0U) != RTT_BRIDGE_MAGIC ||
        read_u16(packet, 4U) != RTT_BRIDGE_VERSION ||
        read_u16(packet, 6U) != RTT_BRIDGE_MSG_CHASSIS_STATE ||
        read_u16(packet, 16U) != RTT_BRIDGE_STATE_PAYLOAD_SIZE)
    {
        return 0;
    }

    rt_size_t offset = RTT_BRIDGE_HEADER_SIZE;
    for (rt_uint8_t i = 0U; i < 4U; ++i, offset += 2U)
    {
        state->target_rpm[i] = read_i16(packet, offset);
    }
    for (rt_uint8_t i = 0U; i < 4U; ++i, offset += 2U)
    {
        state->feedback_rpm[i] = read_i16(packet, offset);
    }
    for (rt_uint8_t i = 0U; i < 4U; ++i, offset += 2U)
    {
        state->current_command[i] = read_i16(packet, offset);
    }
    state->online_mask = read_u16(packet, offset);
    offset += 2U;
    state->fault_mask = read_u16(packet, offset);
    return 1;
}

static int send_heartbeat(int sock, const struct sockaddr_in *linux_address)
{
    rt_uint8_t packet[RTT_BRIDGE_HEADER_SIZE];
    write_header(packet, RTT_BRIDGE_MSG_HEARTBEAT, ++tx_sequence, 0U, 0U);
    return sendto(
        sock,
        packet,
        sizeof(packet),
        0,
        (const struct sockaddr *)linux_address,
        sizeof(*linux_address));
}

static int send_zero_current_command(int sock, const struct sockaddr_in *linux_address)
{
    rt_uint8_t packet[RTT_BRIDGE_HEADER_SIZE + RTT_BRIDGE_CURRENT_PAYLOAD_SIZE];
    rt_size_t offset = RTT_BRIDGE_HEADER_SIZE;

    write_header(packet, RTT_BRIDGE_MSG_CURRENT_COMMAND, ++tx_sequence, RTT_BRIDGE_CURRENT_PAYLOAD_SIZE, 0U);
    for (rt_uint8_t i = 0U; i < 4U; ++i, offset += 2U)
    {
        write_i16(packet, offset, 0);
    }
    write_u16(packet, offset, 100U);

    return sendto(
        sock,
        packet,
        sizeof(packet),
        0,
        (const struct sockaddr *)linux_address,
        sizeof(*linux_address));
}

static void rtt_chassis_bridge_entry(void *parameter)
{
    struct sockaddr_in local_address;
    struct sockaddr_in linux_address;
    struct timeval timeout;
    RttChassisState last_state;
    rt_uint32_t last_print_ms = 0U;
    rt_uint32_t rx_state_count = 0U;
    int sock;

    RT_UNUSED(parameter);
    rt_memset(&last_state, 0, sizeof(last_state));

    rt_thread_mdelay(RTT_BRIDGE_START_DELAY_MS);

    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0)
    {
        rt_kprintf("rtt_chassis_bridge: socket failed\n");
        bridge_thread = RT_NULL;
        return;
    }

    timeout.tv_sec = 0;
    timeout.tv_usec = 1000;
    (void)setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

    rt_memset(&local_address, 0, sizeof(local_address));
    local_address.sin_family = AF_INET;
    local_address.sin_port = htons(RTT_BRIDGE_LOCAL_PORT);
    local_address.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(sock, (struct sockaddr *)&local_address, sizeof(local_address)) < 0)
    {
        rt_kprintf("rtt_chassis_bridge: bind udp/%d failed\n", RTT_BRIDGE_LOCAL_PORT);
        closesocket(sock);
        bridge_thread = RT_NULL;
        return;
    }

    rt_memset(&linux_address, 0, sizeof(linux_address));
    linux_address.sin_family = AF_INET;
    linux_address.sin_port = htons(RTT_BRIDGE_LINUX_PORT);
    linux_address.sin_addr.s_addr = inet_addr(RTT_BRIDGE_LINUX_IP);

    rt_kprintf("rtt_chassis_bridge: started, linux=%s:%d local=%d\n",
               RTT_BRIDGE_LINUX_IP,
               RTT_BRIDGE_LINUX_PORT,
               RTT_BRIDGE_LOCAL_PORT);

    while (1)
    {
        rt_uint8_t packet[128];
        struct sockaddr_in peer_address;
        socklen_t peer_length = sizeof(peer_address);
        int received;

        (void)send_heartbeat(sock, &linux_address);
        (void)send_zero_current_command(sock, &linux_address);

        do
        {
            received = recvfrom(
                sock,
                packet,
                sizeof(packet),
                0,
                (struct sockaddr *)&peer_address,
                &peer_length);
            if (received > 0 && parse_chassis_state(packet, (rt_size_t)received, &last_state))
            {
                rx_state_count++;
            }
        } while (received > 0);

        if ((monotonic_ms32() - last_print_ms) >= RTT_BRIDGE_PRINT_PERIOD_MS)
        {
            rt_kprintf("rtt_chassis_bridge: states=%u online=0x%X rpm=[%d %d %d %d]\n",
                       rx_state_count,
                       last_state.online_mask,
                       last_state.feedback_rpm[0],
                       last_state.feedback_rpm[1],
                       last_state.feedback_rpm[2],
                       last_state.feedback_rpm[3]);
            last_print_ms = monotonic_ms32();
        }

        rt_thread_mdelay(RTT_BRIDGE_PERIOD_MS);
    }
}

int rtt_chassis_bridge_start(void)
{
    if (bridge_thread != RT_NULL)
    {
        rt_kprintf("rtt_chassis_bridge: already running\n");
        return 0;
    }

    bridge_thread = rt_thread_create(
        "rtt_bridge",
        rtt_chassis_bridge_entry,
        RT_NULL,
        RTT_BRIDGE_THREAD_STACK_SIZE,
        RTT_BRIDGE_THREAD_PRIORITY,
        RTT_BRIDGE_THREAD_TICK);
    if (bridge_thread == RT_NULL)
    {
        rt_kprintf("rtt_chassis_bridge: create thread failed\n");
        return -RT_ERROR;
    }

    rt_thread_startup(bridge_thread);
    return 0;
}
MSH_CMD_EXPORT(rtt_chassis_bridge_start, start RK3588 RT-Thread chassis bridge);
