#include <rtthread.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/socket.h>

#define RTT_UDP_ECHO_PORT 21001
#define RTT_UDP_ECHO_BUFFER_SIZE 256
#define RTT_UDP_ECHO_THREAD_STACK_SIZE 2048
#define RTT_UDP_ECHO_THREAD_PRIORITY 20
#define RTT_UDP_ECHO_THREAD_TICK 10

static rt_thread_t echo_thread;

static void rtt_udp_echo_entry(void *parameter)
{
    char buffer[RTT_UDP_ECHO_BUFFER_SIZE];
    struct sockaddr_in local_address;
    struct sockaddr_in peer_address;
    socklen_t peer_length;
    int sock;

    RT_UNUSED(parameter);

    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0)
    {
        rt_kprintf("rtt_udp_echo: socket failed\n");
        echo_thread = RT_NULL;
        return;
    }

    memset(&local_address, 0, sizeof(local_address));
    local_address.sin_family = AF_INET;
    local_address.sin_port = htons(RTT_UDP_ECHO_PORT);
    local_address.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(sock, (struct sockaddr *)&local_address, sizeof(local_address)) < 0)
    {
        rt_kprintf("rtt_udp_echo: bind udp/%d failed\n", RTT_UDP_ECHO_PORT);
        closesocket(sock);
        echo_thread = RT_NULL;
        return;
    }

    rt_kprintf("rtt_udp_echo: listening on udp/0.0.0.0:%d\n", RTT_UDP_ECHO_PORT);

    while (1)
    {
        int received;

        peer_length = sizeof(peer_address);
        received = recvfrom(
            sock,
            buffer,
            sizeof(buffer),
            0,
            (struct sockaddr *)&peer_address,
            &peer_length);
        if (received < 0)
        {
            rt_kprintf("rtt_udp_echo: recvfrom failed\n");
            break;
        }

        if (sendto(
                sock,
                buffer,
                received,
                0,
                (struct sockaddr *)&peer_address,
                peer_length) != received)
        {
            rt_kprintf("rtt_udp_echo: sendto failed\n");
            break;
        }
    }

    closesocket(sock);
    echo_thread = RT_NULL;
}

static int rtt_udp_echo_start(void)
{
    if (echo_thread != RT_NULL)
    {
        rt_kprintf("rtt_udp_echo: already running\n");
        return 0;
    }

    echo_thread = rt_thread_create(
        "udp_echo",
        rtt_udp_echo_entry,
        RT_NULL,
        RTT_UDP_ECHO_THREAD_STACK_SIZE,
        RTT_UDP_ECHO_THREAD_PRIORITY,
        RTT_UDP_ECHO_THREAD_TICK);
    if (echo_thread == RT_NULL)
    {
        rt_kprintf("rtt_udp_echo: create thread failed\n");
        return -RT_ERROR;
    }

    rt_thread_startup(echo_thread);
    return 0;
}
MSH_CMD_EXPORT(rtt_udp_echo_start, start the RK3588 chassis UDP echo endpoint);
