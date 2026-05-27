#ifndef MOTOR_3508_H
#define MOTOR_3508_H

#include <stdint.h>
#include "chassis_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MOTOR3508_CMD_ID_1_TO_4 0x200U
#define MOTOR3508_FEEDBACK_ID_BASE 0x200U
#define MOTOR3508_FIRST_FEEDBACK_ID 0x201U
#define MOTOR3508_LAST_FEEDBACK_ID 0x204U

void motor3508_pack_current(int16_t motor1_current,
                            int16_t motor2_current,
                            int16_t motor3_current,
                            int16_t motor4_current,
                            uint8_t data[8]);

void motor3508_pack_current_array(const int16_t currents[CHASSIS_MOTOR_COUNT + 1U],
                                  uint8_t data[8]);

int motor3508_parse_feedback(uint32_t can_id,
                             const uint8_t data[8],
                             Motor3508Feedback *feedback,
                             uint64_t now_ms);

int motor3508_update_feedback(uint32_t can_id,
                              const uint8_t data[8],
                              Motor3508Feedback feedbacks[CHASSIS_MOTOR_COUNT + 1U],
                              uint64_t now_ms);

const char *motor3508_feedback_name(uint8_t motor_id);

#ifdef __cplusplus
}
#endif

#endif /* MOTOR_3508_H */
