#include <rtthread.h>
#include "system.h"

#include "rt_encoder_soft.h"
#include "motor.h"

extern int rtt_chassis_bridge_start(void);

int main(int argc, char** argv)
{
    RT_UNUSED(argc);
    RT_UNUSED(argv);

    rt_kprintf("Hi, this is RT-Thread!!\n");
    rt_kprintf("Starting RK3588 chassis bridge from main...\n");
    rtt_chassis_bridge_start();

    return 0;
}

void chassis_car_app()
{
    systemInit();

    rt_kprintf("Initial chassis gpio mode!\n");
    rk_soft_encoder_init();

    rt_kprintf("Initial chassis pwm!\n");
    chassis_pwm_init();
    rt_kprintf("Initial chassis encoder!\n");
    encoder_init();

    rt_kprintf("Initial chassis speed by default!\n");
    set_chassis_target(0, 0, 0);

    rt_kprintf("ROS CAR IS STARTTING...\n");
}
MSH_CMD_EXPORT(chassis_car_app, chassis car app init);
