#ifndef RTT_BRIDGE_PROTOCOL_H
#define RTT_BRIDGE_PROTOCOL_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define RTT_BRIDGE_MAGIC 0x524B4348U
#define RTT_BRIDGE_VERSION 1U
#define RTT_BRIDGE_DEFAULT_PORT 21001U

#define RTT_BRIDGE_MSG_VELOCITY_COMMAND 1U
#define RTT_BRIDGE_MSG_CHASSIS_STATE 2U
#define RTT_BRIDGE_MSG_HEARTBEAT 3U
#define RTT_BRIDGE_MSG_ESTOP 4U
#define RTT_BRIDGE_MSG_CURRENT_COMMAND 5U

#define RTT_BRIDGE_FLAG_ESTOP 0x0001U

/*
 * Wire format uses network byte order. Do not cast network buffers directly
 * to these structures. Serialize and parse each field explicitly so Linux and
 * RT-Thread implementations remain portable.
 */
typedef struct
{
    uint32_t magic;
    uint16_t version;
    uint16_t type;
    uint32_t sequence;
    uint32_t monotonic_ms;
    uint16_t payload_length;
    uint16_t flags;
} RttBridgeHeader;

typedef struct
{
    int16_t forward_permille;
    int16_t strafe_permille;
    int16_t rotate_permille;
    uint16_t timeout_ms;
} RttBridgeVelocityCommand;

typedef struct
{
    int16_t target_rpm[4];
    int16_t feedback_rpm[4];
    int16_t current_command[4];
    uint16_t online_mask;
    uint16_t fault_mask;
} RttBridgeChassisState;

typedef struct
{
    int16_t current_command[4];
    uint16_t timeout_ms;
} RttBridgeCurrentCommand;

#ifdef __cplusplus
}
#endif

#endif /* RTT_BRIDGE_PROTOCOL_H */
