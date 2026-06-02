# Test Acceptance Log

更新时间：2026-06-02

## 记录规则

每次硬件或软件验收都应记录：

```text
日期
测试目标
测试环境
执行命令
预期现象
实际现象
是否通过
关键日志或截图路径
后续动作
```

## 已完成验收

### CAN 通信验收

状态：通过。

结果：

```text
can0 可启动。
位率为 1000000。
可通过 candump 看到 0x201 到 0x204 的 3508 电调反馈帧。
CAN 状态为 ERROR-ACTIVE。
bus-errors、error-warn、error-pass、bus-off 均无异常增长。
```

备注：

```text
当前 USB-CAN 适配器或内核不支持 restart-ms / berr-reporting 参数时，可以只设置 bitrate。
```

### 单电机与速度 PID 验收

状态：通过。

结果：

```text
四个 3508 电机均可单独转动。
速度 PID 测试工具可控制目标 rpm。
批量扫参工具已生成 PID 测试结果。
```

### 底盘方向验收

状态：通过。

方向定义：

```text
forward > 0  前进
strafe  > 0  右移
rotate  > 0  左旋
```

### Linux systemd 自启动验收

状态：通过。

结果：

```text
rk3588-can.service 可配置 can0。
rk3588-chassis.service 可启动 chassis_daemon。
重启后 chassis_daemon 为 active running。
```

### Linux 与 RT-Thread 虚拟网卡验收

状态：通过。

结果：

```text
Linux enp255s5: 10.10.10.31/24
RT-Thread:      10.10.10.30
ping 4 packets transmitted, 4 received, 0% packet loss
```

## 待完成验收

1. RT-Thread 官方 `v5.2.2` 源码基线编译。
2. RT-Thread UDP echo 回环。
3. Heartbeat 消息。
4. ESTOP 消息。
5. RT-Thread 控制任务解析速度命令。
6. Linux CAN proxy 执行 RT-Thread 控制输出。
7. 端到端底盘运动验收。

