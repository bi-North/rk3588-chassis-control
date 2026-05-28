#include "pid.h"

#include <stddef.h>

void pid_init(PIDController *pid,
              float kp,
              float ki,
              float kd,
              float output_max,
              float output_min,
              float integral_max,
              float integral_min)
{
    if (pid == NULL)
    {
        return;
    }

    pid->kp = kp;
    pid->ki = ki;
    pid->kd = kd;
    pid->output_max = output_max;
    pid->output_min = output_min;
    pid->integral_max = integral_max;
    pid->integral_min = integral_min;

    pid_reset(pid);
}

void pid_reset(PIDController *pid)
{
    if (pid == NULL)
    {
        return;
    }

    pid->target = 0.0f;
    pid->actual = 0.0f;
    pid->error = 0.0f;
    pid->last_error = 0.0f;
    pid->prev_error = 0.0f;
    pid->integral = 0.0f;
    pid->derivative = 0.0f;
    pid->output = 0.0f;
}

float pid_calculate(PIDController *pid, float target, float actual, float dt_s)
{
    if (pid == NULL)
    {
        return 0.0f;
    }

    if (dt_s <= 0.0f)
    {
        dt_s = 0.001f;
    }

    pid->target = target;
    pid->actual = actual;
    pid->prev_error = pid->last_error;
    pid->last_error = pid->error;
    pid->error = target - actual;
    pid->derivative = (pid->error - pid->last_error) / dt_s;

    float new_integral = pid->integral + pid->error * dt_s;
    if (new_integral > pid->integral_max)
    {
        new_integral = pid->integral_max;
    }
    else if (new_integral < pid->integral_min)
    {
        new_integral = pid->integral_min;
    }

    float output = (pid->kp * pid->error) +
                   (pid->ki * new_integral) +
                   (pid->kd * pid->derivative);

    if (output > pid->output_max)
    {
        output = pid->output_max;
    }
    else if (output < pid->output_min)
    {
        output = pid->output_min;
    }

    if ((output < pid->output_max && output > pid->output_min) ||
        (output == pid->output_max && pid->error < 0.0f) ||
        (output == pid->output_min && pid->error > 0.0f))
    {
        pid->integral = new_integral;
    }

    pid->output = output;
    return output;
}
