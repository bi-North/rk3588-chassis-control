# Issue Log

更新时间：2026-06-02

## 记录规则

每个问题应记录：

```text
问题编号
发现时间
问题现象
影响范围
复现步骤
已验证事实
推测
根因
解决方案
当前状态
相关命令或文件
```

## ISS-001 USB-CAN optional CAN 参数不支持

状态：已规避。

现象：

```text
sudo ip link set can0 type can bitrate 1000000 restart-ms 100 berr-reporting on
```

在当前设备上会报错。

处理：

```text
只使用 bitrate 参数：
sudo ip link set can0 type can bitrate 1000000
```

影响：

```text
不影响当前 CAN 通信、PID 测试和底盘运动验收。
```

## ISS-002 candump 报 Network is down

状态：已解决。

原因：

```text
can0 未 up 或刚被重新配置后未启动。
```

处理：

```text
sudo ip link set can0 up
```

## ISS-003 git push HTTP2 framing layer

状态：已规避。

处理：

```text
改用 SSH 方式推送 GitHub。
```

## ISS-004 Ubuntu 虚拟机 DNS 解析不稳定

状态：部分规避。

现象：

```text
apt 或 git clone 偶发 Could not resolve host。
ping IP 可通，但 DNS 查询失败。
```

处理：

```text
优先通过 Windows 浏览器下载源码，再经 VMware 共享目录传入虚拟机。
```

## ISS-005 厂家 RT-Thread 源码不完整

状态：阻塞中。

现象：

```text
原始 RK3588 BSP 基线编译失败。
```

缺失：

```text
klibc/kerrno.h
ktime.h
setup.h
```

结论：

```text
该问题不是新增 UDP echo 或底盘控制代码造成的。
```

当前方案：

```text
使用官方 RT-Thread v5.2.2 作为外部 RTT_ROOT 尝试基线编译。
```

## ISS-006 键盘控制无法响应

状态：已绕过并修复主要控制链路。

处理：

```text
优先使用命令式测试工具和 UDP daemon 验收底盘运动。
```

## ISS-007 前后运动方向反向

状态：已修复。

当前方向：

```text
forward > 0 表示前进。
```

## ISS-008 停止后手推底盘会自动补偿一小段

状态：已修复。

原因：

```text
零命令时 PID 状态未完全复位。
```

处理：

```text
零底盘命令时重置 PID，避免积分或历史误差造成补偿动作。
```

