#ifndef CHASSIS_TYPES_H
#define CHASSIS_TYPES_H

#include <stdint.h>

#define CHASSIS_MOTOR_COUNT 4U

typedef struct
{
    uint8_t id;
    int16_t angle;
    int16_t speed_rpm;
    int16_t current;
    int8_t temperature;
    uint64_t last_update_ms;
    uint8_t online;
} Motor3508Feedback;

#endif /* CHASSIS_TYPES_H */
