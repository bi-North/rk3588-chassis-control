#ifndef CHASSIS_CONTROL_H
#define CHASSIS_CONTROL_H

#include <stdint.h>
#include "chassis_types.h"
#include "pid.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
    float forward;
    float strafe;
    float rotate;
} ChassisCommand;

typedef struct
{
    float max_translate_rpm;
    float max_rotate_rpm;
    float command_timeout_ms;
    float feedback_timeout_ms;
    float startup_boost_scale;
    float startup_boost_min_command;
    float startup_boost_ms;
} ChassisConfig;

typedef struct
{
    ChassisConfig config;
    PIDController pid[CHASSIS_MOTOR_COUNT + 1U];
    Motor3508Feedback feedback[CHASSIS_MOTOR_COUNT + 1U];
    float target_rpm[CHASSIS_MOTOR_COUNT + 1U];
    ChassisCommand command;
    uint64_t last_command_ms;
    uint64_t startup_boost_start_ms;
    uint8_t startup_boost_active;
} ChassisController;

void chassis_default_config(ChassisConfig *config);
void chassis_init(ChassisController *controller, const ChassisConfig *config, uint64_t now_ms);
void chassis_set_command(ChassisController *controller,
                         float forward,
                         float strafe,
                         float rotate,
                         uint64_t now_ms);
void chassis_stop(ChassisController *controller, uint64_t now_ms);
void chassis_update_feedback(ChassisController *controller, const Motor3508Feedback *feedback);
int chassis_step(ChassisController *controller,
                 uint64_t now_ms,
                 float dt_s,
                 int16_t currents[CHASSIS_MOTOR_COUNT + 1U]);
int chassis_feedback_all_online(const ChassisController *controller, uint64_t now_ms);

#ifdef __cplusplus
}
#endif

#endif /* CHASSIS_CONTROL_H */
