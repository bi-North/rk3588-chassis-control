#ifndef SOCKETCAN_H
#define SOCKETCAN_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int socketcan_open(const char *ifname);
int socketcan_send(int fd, uint32_t can_id, const uint8_t *data, uint8_t dlc);
int socketcan_receive(int fd, uint32_t *can_id, uint8_t *data, uint8_t *dlc, int timeout_ms);
void socketcan_close(int fd);

#ifdef __cplusplus
}
#endif

#endif /* SOCKETCAN_H */
