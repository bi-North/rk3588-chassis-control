#include "chassis_control.h"

#include <math.h>
#include <stddef.h>

#define CHASSIS_PID_OUTPUT_LIMIT 6500.0f
#define CHASSIS_PID_INTEGRAL_LIMIT 2500.0f

static float clamp_float(float value, float min_value, float max_value)
{
    if (value > max_value)
    {
        return max_value;
    }
    if (value < min_value)
    {
        return min_value;
    }
    return value;
}

static float abs_float(float value)
{
    return (value >= 0.0f) ? value : -value;
}

static int command_is_zero(const ChassisCommand *command)
{
    return abs_float(command->forward) < 0.0001f &&
           abs_float(command->strafe) < 0.0001f &&
           abs_float(command->rotate) < 0.0001f;
}

static float command_peak(const ChassisCommand *command)
{
    float peak = abs_float(command->forward);
    if (abs_float(command->strafe) > peak)
    {
        peak = abs_float(command->strafe);
    }
    if (abs_float(command->rotate) > peak)
    {
        peak = abs_float(command->rotate);
    }
    return peak;
}

static uint64_t elapsed_ms(uint64_t now_ms, uint64_t previous_ms)
{
    return (now_ms >= previous_ms) ? (now_ms - previous_ms) : 0U;
}

static void reset_motion_state(ChassisController *controller)
{
    controller->command.forward = 0.0f;
    controller->command.strafe = 0.0f;
    controller->command.rotate = 0.0f;
    controller->startup_boost_active = 0U;

    for (uint8_t i = 1U; i <= CHASSIS_MOTOR_COUNT; ++i)
    {
        controller->target_rpm[i] = 0.0f;
        pid_reset(&controller->pid[i]);
    }
}

static void normalize_wheel_targets(float target[CHASSIS_MOTOR_COUNT + 1U], float max_abs_rpm)
{
    float peak = abs_float(target[1]);
    for (uint8_t i = 2U; i <= CHASSIS_MOTOR_COUNT; ++i)
    {
        float value = abs_float(target[i]);
        if (value > peak)
        {
            peak = value;
        }
    }

    if (peak > max_abs_rpm && peak > 0.0f)
    {
        float scale = max_abs_rpm / peak;
        for (uint8_t i = 1U; i <= CHASSIS_MOTOR_COUNT; ++i)
        {
            target[i] *= scale;
        }
    }
}

static void update_targets_from_command(ChassisController *controller, uint64_t now_ms)
{
    float boost = 1.0f;
    float peak = command_peak(&controller->command);
    if (controller->startup_boost_active)
    {
        if ((float)elapsed_ms(now_ms, controller->startup_boost_start_ms) >= controller->config.startup_boost_ms)
        {
            controller->startup_boost_active = 0U;
        }
        else if (peak > 0.0f && peak < controller->config.startup_boost_scale)
        {
            boost = controller->config.startup_boost_scale / peak;
        }
    }

    float forward = -clamp_float(controller->command.forward * boost, -1.0f, 1.0f) * controller->config.max_translate_rpm;
    float strafe = clamp_float(controller->command.strafe * boost, -1.0f, 1.0f) * controller->config.max_translate_rpm;
    float rotate = clamp_float(controller->command.rotate * boost, -1.0f, 1.0f) * controller->config.max_rotate_rpm;

    /*
     * Wheel layout and signs follow the original STM32 chassis task:
     *   2\   /4
     *   1/   \3
     */
    controller->target_rpm[1] = -(forward + strafe) - rotate;
    controller->target_rpm[2] = -(forward - strafe) - rotate;
    controller->target_rpm[3] =  (forward - strafe) - rotate;
    controller->target_rpm[4] =  (forward + strafe) - rotate;

    normalize_wheel_targets(controller->target_rpm, controller->config.max_translate_rpm);
}

void chassis_default_config(ChassisConfig *config)
{
    if (config == NULL)
    {
        return;
    }

    config->max_translate_rpm = 500.0f;
    config->max_rotate_rpm = 400.0f;
    config->command_timeout_ms = 300.0f;
    config->feedback_timeout_ms = 250.0f;
    config->startup_boost_scale = 0.75f;
    config->startup_boost_min_command = 0.20f;
    config->startup_boost_ms = 250.0f;
}

void chassis_init(ChassisController *controller, const ChassisConfig *config, uint64_t now_ms)
{
    if (controller == NULL)
    {
        return;
    }

    ChassisConfig local_config;
    if (config == NULL)
    {
        chassis_default_config(&local_config);
        config = &local_config;
    }

    controller->config = *config;
    controller->command.forward = 0.0f;
    controller->command.strafe = 0.0f;
    controller->command.rotate = 0.0f;
    controller->last_command_ms = now_ms;
    controller->startup_boost_start_ms = now_ms;
    controller->startup_boost_active = 0U;

    for (uint8_t i = 0U; i <= CHASSIS_MOTOR_COUNT; ++i)
    {
        controller->feedback[i].id = i;
        controller->feedback[i].online = 0U;
        controller->target_rpm[i] = 0.0f;
    }

    pid_init(&controller->pid[1], 2.0f, 0.35f, 0.0f,
             CHASSIS_PID_OUTPUT_LIMIT, -CHASSIS_PID_OUTPUT_LIMIT,
             CHASSIS_PID_INTEGRAL_LIMIT, -CHASSIS_PID_INTEGRAL_LIMIT);
    pid_init(&controller->pid[2], 2.0f, 0.30f, 0.0f,
             CHASSIS_PID_OUTPUT_LIMIT, -CHASSIS_PID_OUTPUT_LIMIT,
             CHASSIS_PID_INTEGRAL_LIMIT, -CHASSIS_PID_INTEGRAL_LIMIT);
    pid_init(&controller->pid[3], 2.0f, 0.40f, 0.0f,
             CHASSIS_PID_OUTPUT_LIMIT, -CHASSIS_PID_OUTPUT_LIMIT,
             CHASSIS_PID_INTEGRAL_LIMIT, -CHASSIS_PID_INTEGRAL_LIMIT);
    pid_init(&controller->pid[4], 2.0f, 0.60f, 0.0f,
             CHASSIS_PID_OUTPUT_LIMIT, -CHASSIS_PID_OUTPUT_LIMIT,
             CHASSIS_PID_INTEGRAL_LIMIT, -CHASSIS_PID_INTEGRAL_LIMIT);
}

void chassis_set_command(ChassisController *controller,
                         float forward,
                         float strafe,
                         float rotate,
                         uint64_t now_ms)
{
    if (controller == NULL)
    {
        return;
    }

    int was_zero = command_is_zero(&controller->command);
    controller->command.forward = clamp_float(forward, -1.0f, 1.0f);
    controller->command.strafe = clamp_float(strafe, -1.0f, 1.0f);
    controller->command.rotate = clamp_float(rotate, -1.0f, 1.0f);
    controller->last_command_ms = now_ms;

    float peak = command_peak(&controller->command);
    if (command_is_zero(&controller->command))
    {
        controller->startup_boost_active = 0U;
    }
    else if (was_zero &&
             peak >= controller->config.startup_boost_min_command &&
             peak < controller->config.startup_boost_scale)
    {
        controller->startup_boost_start_ms = now_ms;
        controller->startup_boost_active = 1U;
    }
}

void chassis_stop(ChassisController *controller, uint64_t now_ms)
{
    if (controller == NULL)
    {
        return;
    }

    controller->last_command_ms = now_ms;
    reset_motion_state(controller);
}

void chassis_update_feedback(ChassisController *controller, const Motor3508Feedback *feedback)
{
    if (controller == NULL || feedback == NULL)
    {
        return;
    }

    if (feedback->id < 1U || feedback->id > CHASSIS_MOTOR_COUNT)
    {
        return;
    }

    controller->feedback[feedback->id] = *feedback;
}

int chassis_feedback_all_online(const ChassisController *controller, uint64_t now_ms)
{
    if (controller == NULL)
    {
        return 0;
    }

    for (uint8_t i = 1U; i <= CHASSIS_MOTOR_COUNT; ++i)
    {
        const Motor3508Feedback *feedback = &controller->feedback[i];
        if (!feedback->online)
        {
            return 0;
        }
        if ((float)elapsed_ms(now_ms, feedback->last_update_ms) > controller->config.feedback_timeout_ms)
        {
            return 0;
        }
    }

    return 1;
}

int chassis_step(ChassisController *controller,
                 uint64_t now_ms,
                 float dt_s,
                 int16_t currents[CHASSIS_MOTOR_COUNT + 1U])
{
    if (controller == NULL || currents == NULL)
    {
        return 0;
    }

    for (uint8_t i = 0U; i <= CHASSIS_MOTOR_COUNT; ++i)
    {
        currents[i] = 0;
    }

    if ((float)elapsed_ms(now_ms, controller->last_command_ms) > controller->config.command_timeout_ms)
    {
        reset_motion_state(controller);
    }

    if (command_is_zero(&controller->command))
    {
        reset_motion_state(controller);
        return chassis_feedback_all_online(controller, now_ms);
    }

    update_targets_from_command(controller, now_ms);

    if (!chassis_feedback_all_online(controller, now_ms))
    {
        for (uint8_t i = 1U; i <= CHASSIS_MOTOR_COUNT; ++i)
        {
            pid_reset(&controller->pid[i]);
            controller->target_rpm[i] = 0.0f;
        }
        return 0;
    }

    for (uint8_t i = 1U; i <= CHASSIS_MOTOR_COUNT; ++i)
    {
        float output = pid_calculate(&controller->pid[i],
                                     controller->target_rpm[i],
                                     (float)controller->feedback[i].speed_rpm,
                                     dt_s);
        currents[i] = (int16_t)lrintf(output);
    }

    return 1;
}
