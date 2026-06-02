# RT-Thread Build Blocker

更新时间：2026-06-02

## 现象

在 Ubuntu 虚拟机中使用厂家提供的 `rockchip-hypercar/software/RK3588` 工程进行原始基线编译时失败。失败发生在加入任何新 RT-Thread 代码之前。

典型命令：

```bash
cd ~/Desktop/rock_ws && docker run --rm --user "$(id -u):$(id -g)" -v "$HOME/Desktop/rock_ws:/work" -w /work/rockchip-hypercar/software/RK3588 -e RTT_EXEC_PATH=/work/toolchains/gcc-arm-10.2-2020.11-x86_64-aarch64-none-elf/bin --entrypoint sh hyperenv:rockpi5b -c 'scons -j$(nproc)'
```

典型报错：

```text
fatal error: klibc/kerrno.h: No such file or directory
fatal error: ktime.h: No such file or directory
fatal error: setup.h: No such file or directory
```

## 已排查结论

1. Docker 镜像 `hyperenv:rockpi5b` 可启动。
2. SCons 在 Docker 镜像中存在。
3. AArch64 交叉编译器可通过挂载的 `toolchains` 使用。
4. Docker 镜像内部未发现完整的 `rt-thread` 源码目录。
5. 本机厂家资料目录中未发现缺失的 `kerrno.h`、`ktime.h`、`setup.h`。
6. 厂家提供的 `software/rt-thread/components` 和 `software/rt-thread/libcpu` 内容明显不完整。

## 版本线索

厂家残缺源码中的 `rtdef.h` 标记：

```text
RT_VERSION_MAJOR 5
RT_VERSION_MINOR 2
RT_VERSION_PATCH 2
```

因此当前优先尝试官方 RT-Thread `v5.2.2`。

## 推荐处理策略

不要覆盖厂家目录。新建并行目录：

```text
~/Desktop/rock_ws/rt-thread-official-v5.2.2
```

然后编译时使用外部 `RTT_ROOT`：

```text
RTT_ROOT=/work/rt-thread-official-v5.2.2
```

这样可以验证官方同版本源码是否能补齐缺失内核文件，同时不破坏厂家 BSP 和当前稳定系统。

## 风险

官方 RT-Thread 源码可能无法完全匹配厂家 RK3588 虚拟化 BSP。如果出现 RK3588 私有驱动、HyperSDK 绑定接口或虚拟化接口缺失，则必须向厂家索要完整源码包或对应仓库 commit。

## 不建议操作

1. 不建议直接覆盖 `rockchip-hypercar/software/rt-thread`。
2. 不建议下载最新 RT-Thread 主分支直接替换。
3. 不建议在基线编译通过前部署新的 `HyperBoot.bin`。
4. 不建议修改当前 TF 卡稳定系统作为实验环境。

