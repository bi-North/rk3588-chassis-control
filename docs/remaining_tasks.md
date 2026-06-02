# Remaining Tasks

更新时间：2026-06-02

## A. RT-Thread 编译恢复

状态：进行中，当前阻塞。

1. 下载官方 RT-Thread `v5.2.2` 源码压缩包。
2. 将源码解压到虚拟机工作区：

```text
~/Desktop/rock_ws/rt-thread-official-v5.2.2
```

3. 使用 `RTT_ROOT=/work/rt-thread-official-v5.2.2` 编译厂家 RK3588 BSP。
4. 如果仍然缺少 RK3588 私有头文件或驱动文件，记录缺失项并判断是否必须向厂家索要完整源码。
5. 基线编译通过后，保存构建命令和产物路径。

## B. RT-Thread UDP 回环验证

状态：待开始。

1. 将 `rtthread_port/rtt_udp_echo.c` 集成到实际 RK3588 RT-Thread 工程。
2. 重新编译生成 `rtthread.bin`。
3. 使用 HyperSDK 生成新的 `HyperBoot.bin`。
4. 先备份当前开发板启动文件，再部署新产物。
5. 在 RT-Thread MSH 中启动：

```text
rtt_udp_echo_start
```

6. 在 Linux 侧运行 `tools/test_rtt_udp_echo.py` 完成 UDP 回环验收。

## C. RT-Thread 底盘控制任务

状态：未开始。

1. 在 RT-Thread 中实现速度命令接收线程。
2. 在 RT-Thread 中实现麦克纳姆轮运动学计算。
3. 在 RT-Thread 中实现速度 PID 或控制输出状态生成。
4. 加入急停、超时清零、通信心跳。
5. 输出电机目标速度、电流命令或中间控制状态给 Linux CAN 代理。

## D. Linux CAN 代理

状态：未开始。

1. 在 Linux 中保留 USB-CAN SocketCAN 访问。
2. 实现 RT-Thread 到 Linux 的控制报文接收。
3. 将 RT-Thread 控制输出转换成 DJI 3508 CAN 控制帧。
4. 将 3508 反馈帧解析后回传给 RT-Thread。
5. 加入代理级安全保护：超时清零、掉线清零、异常状态日志。

## E. 端到端验收

状态：未开始。

1. Linux 与 RT-Thread UDP 心跳稳定运行。
2. RT-Thread 速度指令能驱动 Linux CAN 代理发送控制帧。
3. 四轮反馈能从 Linux 回传给 RT-Thread。
4. 前进、后退、左移、右移、左旋、右旋方向全部正确。
5. 急停、超时清零、服务重启后安全状态全部通过。
6. 输出完整测试记录，用于答辩和项目归档。

