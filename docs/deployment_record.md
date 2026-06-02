# Deployment Record

更新时间：2026-06-02

## 当前部署状态

Linux 侧部署已完成，RT-Thread 新镜像尚未部署。

## 已部署内容

### RK3588 Linux chassis daemon

状态：已部署并验收通过。

服务：

```text
rk3588-can.service
rk3588-chassis.service
```

验证：

```text
systemctl status rk3588-chassis.service --no-pager
journalctl -u rk3588-chassis.service -n 20 --no-pager
```

结果：

```text
服务 active running。
chassis_daemon 输出 online=1。
drops=0。
```

## 尚未部署内容

### 新 RT-Thread / HyperBoot

状态：未部署。

原因：

```text
RT-Thread 基线编译尚未恢复。
```

部署前必须完成：

```text
1. 原始 RK3588 BSP 基线编译通过。
2. 生成新的 rtthread.bin。
3. HyperSDK 生成新的 HyperBoot.bin。
4. 记录新旧文件大小和校验值。
5. 备份当前 /boot 文件。
6. 明确回滚方式。
```

## 禁止操作

在 RT-Thread 基线编译和本地产物检查通过前，不要覆盖：

```text
/boot/HyperBoot.bin
TF 卡中的稳定 rockpi_car.img 系统
厂家原始 rockchip-hypercar 目录
```

