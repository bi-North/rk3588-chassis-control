#include "socketcan.h"

#include <errno.h>
#include <linux/can.h>
#include <linux/can/raw.h>
#include <net/if.h>
#include <poll.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>

int socketcan_open(const char *ifname)
{
    int fd = socket(PF_CAN, SOCK_RAW, CAN_RAW);
    if (fd < 0)
    {
        return -1;
    }

    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, ifname, IFNAMSIZ - 1U);

    if (ioctl(fd, SIOCGIFINDEX, &ifr) < 0)
    {
        close(fd);
        return -1;
    }

    struct sockaddr_can addr;
    memset(&addr, 0, sizeof(addr));
    addr.can_family = AF_CAN;
    addr.can_ifindex = ifr.ifr_ifindex;

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        close(fd);
        return -1;
    }

    return fd;
}

int socketcan_send(int fd, uint32_t can_id, const uint8_t *data, uint8_t dlc)
{
    if (dlc > CAN_MAX_DLEN)
    {
        errno = EINVAL;
        return -1;
    }

    struct can_frame frame;
    memset(&frame, 0, sizeof(frame));
    frame.can_id = can_id;
    frame.can_dlc = dlc;
    memcpy(frame.data, data, dlc);

    ssize_t written = write(fd, &frame, sizeof(frame));
    if (written != (ssize_t)sizeof(frame))
    {
        return -1;
    }

    return 0;
}

int socketcan_receive(int fd, uint32_t *can_id, uint8_t *data, uint8_t *dlc, int timeout_ms)
{
    struct pollfd pfd;
    pfd.fd = fd;
    pfd.events = POLLIN;
    pfd.revents = 0;

    int poll_ret = poll(&pfd, 1, timeout_ms);
    if (poll_ret < 0)
    {
        return -1;
    }
    if (poll_ret == 0)
    {
        return 0;
    }

    struct can_frame frame;
    ssize_t read_len = read(fd, &frame, sizeof(frame));
    if (read_len != (ssize_t)sizeof(frame))
    {
        return -1;
    }

    *can_id = frame.can_id & CAN_SFF_MASK;
    *dlc = frame.can_dlc;
    memcpy(data, frame.data, frame.can_dlc);

    return 1;
}

void socketcan_close(int fd)
{
    if (fd >= 0)
    {
        close(fd);
    }
}
