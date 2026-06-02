# RK3588 Chassis Control Project Status

更新时间：2026-06-02

## 项目目标

将原 STM32 底盘程序迁移到 RK3588 的 Linux + RT-Thread 虚拟化混合部署环境中。当前硬件 CAN 方案采用 USB-CAN 接口，3508 电调通过 Linux SocketCAN 访问。

最终目标控制链路：

```text
Linux 上层应用 / ROS2 / 导航
        |
        | 速度指令
        v
RT-Thread 底盘控制任务
        |
        | 目标电机状态 / 电流命令 / 安全状态
        v
Linux CAN 代理
        |
        | SocketCAN + USB-CAN
        v
DJI 3508 电调
```

## 当前阶段结论

Linux 阶段已基本完成，可以通过 RK3588 Linux + USB-CAN 控制四个 3508 电机并完成底盘运动。

RT-Thread 阶段已验证 Linux 与 RT-Thread 虚拟网卡通信正常，但 RT-Thread 工程编译被厂家源码不完整阻塞。

## 已完成内容

1. USB-CAN 在 RK3588 Linux 下可用，`can0` 能正常收发 3508 电调数据。
2. CAN 位率使用 `1000000`，适配器不支持 `restart-ms` 和 `berr-reporting` 时可省略，不影响当前通信结果。
3. 已确认 3508 反馈帧为 `0x201` 到 `0x204`，控制帧为 `0x200`。
4. 已完成单电机开环测试、速度 PID 测试、自动 PID 扫参、批量扫参。
5. 已完成四个电机 PID 参数初步整定。
6. 已完成麦克纳姆轮方向修正：
   - `forward > 0` 表示前进。
   - `strafe > 0` 表示向右平移。
   - `rotate > 0` 表示左旋。
7. 已完成 Linux 侧底盘控制核心代码。
8. 已完成 `chassis_daemon` UDP 控制守护进程。
9. 已完成 Linux systemd 自启动服务：
   - `rk3588-can.service`
   - `rk3588-chassis.service`
10. 已完成开机后服务状态验证，`chassis_daemon` 可保持运行。
11. 已完成 Linux 与 RT-Thread 虚拟网卡连通性验证：
   - Linux: `10.10.10.31`
   - RT-Thread: `10.10.10.30`
   - `ping` 0% packet loss。
12. 已在仓库中准备 RT-Thread UDP 回环测试代码和 Linux 测试脚本。

## 当前阻塞

厂家提供的 `rockchip-hypercar/software/rt-thread` 源码不完整。原始工程在未加入任何新代码时就无法完成基线编译。

已确认缺失文件包括：

```text
klibc/kerrno.h
ktime.h
setup.h
```

根据 `rtdef.h`，当前残缺源码版本标记为 RT-Thread `v5.2.2`。备用方案是下载官方 RT-Thread `v5.2.2` 源码，作为外部 `RTT_ROOT` 尝试编译厂家 RK3588 BSP。

## 当前稳定回退点

1. 当前 TF 卡中的 `rockpi_car.img` 系统可继续作为稳定回退环境。
2. 已验证的 Linux USB-CAN 底盘控制程序可继续用于测试和演示。
3. 不应在 RT-Thread 基线编译通过前覆盖 `/boot` 或替换当前可用的启动文件。

