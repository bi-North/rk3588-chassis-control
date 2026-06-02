# SubAgent Work Plan

更新时间：2026-06-02

## 目的

本项目上下文较长，包含硬件接线、CAN 通信、Linux 控制、RT-Thread 编译、虚拟化部署、GitHub 协作和验收记录。为了减少上下文压缩造成的信息丢失，将工作拆成多个长期责任域。

文档是主记忆，subAgent 是执行分工。任何关键结论都必须写回 `docs/`。

## 当前已启动的 subAgent

| 角色 | Agent 昵称 | Agent ID | 职责 |
| --- | --- | --- | --- |
| Linux 控制 Agent | Tesla | `019e877d-4fbc-7f42-bc5f-3729c2c21488` | Linux SocketCAN、USB-CAN、chassis_daemon、systemd、Linux CAN 代理 |
| RT-Thread 编译 Agent | Pasteur | `019e877d-987e-7631-8ec2-01ef41407c20` | RT-Thread 源码补齐、RK3588 BSP 编译、rtthread.bin、HyperBoot |
| IPC/协议 Agent | Copernicus | `019e877d-d6c6-7350-ab0b-da0e24238039` | Linux 与 RT-Thread UDP 协议、心跳、状态、急停、状态回传 |
| 文档与验收 Agent | Gibbs | `019e877e-30d9-7732-97f6-34f085b23c94` | 项目状态、测试日志、故障记录、答辩材料、交接清单 |

## 角色边界

### Linux 控制 Agent

负责：

1. `src/`、`include/`、`tools/` 中 Linux 侧底盘控制程序。
2. SocketCAN 初始化和 CAN 错误状态解释。
3. `scripts/setup_can.sh` 与 `systemd/`。
4. 未来 Linux CAN proxy 的设计和实现。

不负责：

1. RT-Thread 镜像编译。
2. HyperBoot 替换。
3. RT-Thread 内核裁剪。

### RT-Thread 编译 Agent

负责：

1. 恢复可编译的 RT-Thread 源码树。
2. RK3588 BSP 基线编译。
3. `rtthread.bin` 和 HyperBoot 生成流程。
4. 部署前备份和风险清单。

不负责：

1. 修改 Linux 侧控制算法。
2. 重新调 CAN 或 PID 参数。

### IPC/协议 Agent

负责：

1. `include/rtt_bridge_protocol.h`。
2. `tools/test_rtt_udp_echo.py`。
3. `rtthread_port/rtt_udp_echo.c`。
4. 速度命令、状态回传、心跳、急停协议。

不负责：

1. 直接改动 systemd 服务。
2. 直接替换启动镜像。

### 文档与验收 Agent

负责：

1. `docs/project_status.md`。
2. `docs/remaining_tasks.md`。
3. 测试记录、故障记录、验收步骤。
4. 答辩和项目汇报材料。

不负责：

1. 单独决策是否覆盖系统文件。
2. 未经验证编写“已完成”结论。

## 协作规则

1. 所有 destructive 操作必须先确认，包括覆盖、删除、替换 `/boot` 或 TF 卡文件。
2. 对当前稳定 Linux 底盘控制系统只做增量修改，不回滚他人改动。
3. 新 RT-Thread 源码必须放在并行目录中验证，不直接覆盖厂家源码。
4. 任一 Agent 得出关键结论后，需要同步到 `docs/`。
5. 所有测试结论必须包含命令、现象、结果和是否通过。
6. 对硬件运动测试必须默认先架空底盘，确认急停和清零逻辑。

## 下一步建议

优先级最高的是 RT-Thread 编译恢复：

1. 通过 Windows 下载官方 RT-Thread `v5.2.2` 源码。
2. 放入 VMware 共享目录。
3. 在 Ubuntu 虚拟机中解压为 `~/Desktop/rock_ws/rt-thread-official-v5.2.2`。
4. 使用外部 `RTT_ROOT` 进行 RK3588 BSP 基线编译。
5. 基线通过后再集成 UDP 回环模块。

