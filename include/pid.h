#ifndef PID_H
#define PID_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
    float kp;
    float ki;
    float kd;

    float target;
    float actual;
    float error;
    float last_error;
    float prev_error;

    float integral;
    float derivative;
    float output;

    float output_max;
    float output_min;
    float integral_max;
    float integral_min;
} PIDController;

void pid_init(PIDController *pid,
              float kp,
              float ki,
              float kd,
              float output_max,
              float output_min,
              float integral_max,
              float integral_min);

void pid_reset(PIDController *pid);

float pid_calculate(PIDController *pid, float target, float actual, float dt_s);

#ifdef __cplusplus
}
#endif

#endif /* PID_H */
