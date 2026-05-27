#include "motor_3508.h"

#include <stddef.h>

static int16_t bytes_to_i16(uint8_t high, uint8_t low)
{
    return (int16_t)((uint16_t)((uint16_t)high << 8U) | (uint16_t)low);
}

static void i16_to_bytes(int16_t value, uint8_t *high, uint8_t *low)
{
    *high = (uint8_t)((uint16_t)value >> 8U);
    *low = (uint8_t)((uint16_t)value & 0xFFU);
}

void motor3508_pack_current(int16_t motor1_current,
                            int16_t motor2_current,
                            int16_t motor3_current,
                            int16_t motor4_current,
                            uint8_t data[8])
{
    i16_to_bytes(motor1_current, &data[0], &data[1]);
    i16_to_bytes(motor2_current, &data[2], &data[3]);
    i16_to_bytes(motor3_current, &data[4], &data[5]);
    i16_to_bytes(motor4_current, &data[6], &data[7]);
}

void motor3508_pack_current_array(const int16_t currents[CHASSIS_MOTOR_COUNT + 1U],
                                  uint8_t data[8])
{
    motor3508_pack_current(currents[1], currents[2], currents[3], currents[4], data);
}

int motor3508_parse_feedback(uint32_t can_id,
                             const uint8_t data[8],
                             Motor3508Feedback *feedback,
                             uint64_t now_ms)
{
    if (feedback == NULL)
    {
        return 0;
    }

    if (can_id < MOTOR3508_FIRST_FEEDBACK_ID || can_id > MOTOR3508_LAST_FEEDBACK_ID)
    {
        return 0;
    }

    feedback->id = (uint8_t)(can_id - MOTOR3508_FEEDBACK_ID_BASE);
    feedback->angle = bytes_to_i16(data[0], data[1]);
    feedback->speed_rpm = bytes_to_i16(data[2], data[3]);
    feedback->current = bytes_to_i16(data[4], data[5]);
    feedback->temperature = (int8_t)data[6];
    feedback->last_update_ms = now_ms;
    feedback->online = 1U;

    return 1;
}

int motor3508_update_feedback(uint32_t can_id,
                              const uint8_t data[8],
                              Motor3508Feedback feedbacks[CHASSIS_MOTOR_COUNT + 1U],
                              uint64_t now_ms)
{
    Motor3508Feedback feedback;
    if (!motor3508_parse_feedback(can_id, data, &feedback, now_ms))
    {
        return 0;
    }

    feedbacks[feedback.id] = feedback;
    return 1;
}

const char *motor3508_feedback_name(uint8_t motor_id)
{
    switch (motor_id)
    {
        case 1U:
            return "M1";
        case 2U:
            return "M2";
        case 3U:
            return "M3";
        case 4U:
            return "M4";
        default:
            return "M?";
    }
}
