# RFAM-187 在线升级框架：按步骤阅读版

这版按照你提供的 `RFAM187_OnlineUpgrade_Full_V1` 的文件结构整理：文件平铺、步骤展开、中文注释。协议规则和已经验证的接收保护保留。硬件接口集中在 `upgrade_board.c`，A 用本地接口，B/C/D 用远端接口。

**这是一套可测试的软件框架，还不能真实烧写 Flash。** 芯片手册和新增 AXI/Aurora 控制接口尚未接入；没有驱动时返回失败，不报告虚假成功。

## 建议阅读顺序

先看 [逐函数讲解](README_逐函数讲解.md)，然后按下面顺序打开源码：

| 顺序 | 文件 | 先理解什么 |
|---|---|---|
| 1 | `src/main_sdk.c` | 网络怎么初始化，主循环每一圈做什么 |
| 2 | `src/upgrade_config.h` | IP、端口、目标编号、缓冲区和超时配置 |
| 3 | `src/upgrade_udp_sdk.c` | lwIP 怎么调用收包函数，怎么把应答发回去 |
| 4 | `src/icd_upgrade.c` | 怎么从字节数组读出字段，怎么生成应答 |
| 5 | `src/upgrade_app.c` | 第一帧、后续帧、校验、准备、烧写各怎么处理 |
| 6 | `src/upgrade_board.c` | A/B/C/D 怎么选择硬件通路，以后在哪里补驱动 |

对应 `.h` 文件说明了结构体字段及函数参数。原 SDK 的平台文件继续保留；`echo.c` 和 JTAG 源码作为原工程参考保留，但新的 `main_sdk.c` 只启动升级网络流程，不启动 XVC。

## 本版规则

- UDP 本机端口 10001；MLC 源端口要求为 10000，应答发回该来源 IP/端口。
- 多字节整数大端，不附加应用报文 CRC。0008 长度为 `31+载荷长度`；0088 为 29 字节；0089/008A 为 28 字节。
- 首帧仅含 60 字节摘要，后续每帧 1～1024 字节。摘要偏移 4 起及整份程序内容逐字节累加；临时使用 64 位，超过 32 位时拒绝而不偷偷截断。
- 一次升级一个目标，绑定发送端、UUID、模块、处理器、配置项、文件编号和总帧数。
- 非连续帧回复失败并返回最近收到的帧号，不重复追加数据。结束后的最后帧重传可重发结果，不重复烧写。
- 收齐文件后先检查本地镜像，再准备目标；目标容量/校验确认后回 0089，真正烧写的状态通过 008A 回传。
- 等待接收和结果保留时间为 30 秒；准备及烧写阶段各限时 120 秒。使用毫秒时钟，不使用主循环次数判断超时。

这些是沿用原实现的规则。参考压缩包中的“重复帧成功应答”“可选 AXI 寄存器布局”等内容没有直接变成新的协议或硬件事实。

## 编译与测试

在电脑上测试：

```sh
make test
```

包含协议、收包状态机、四目标会话、后端失败与分发测试，并执行 10000 组畸形报文输入。启用 AddressSanitizer/UndefinedBehaviorSanitizer；执行器不支持 LeakSanitizer，所以仅关闭泄漏检查。

使用完整 BSP 的头文件检查 SDK 接口：

```sh
make check-bsp BSP_INCLUDE=/你的路径/RFAM_187_XVC_bsp/microblaze_0/include
```

这只是主机上的语法/API 检查，不是 MicroBlaze 交叉编译或上板测试。测试中的 UUID 只是测试值，不能据此确认本机实际编号。

## 放入 SDK 时怎么做

1. **用 `main_sdk.c` 替换旧 `main.c`，工程只能有一个 `main()`。** `tests/` 不能编入 SDK 应用。
2. 加入 `icd_upgrade.c`、`upgrade_app.c`、`upgrade_board.c`、`upgrade_udp_sdk.c` 及相应头文件和 `upgrade_config.h`。
3. 从上一版迁移时，移除旧 `upgrade_protocol.*`、`upgrade_session.*`、`upgrade_udp.*`、`upgrade_local.c`、`upgrade_aurora.c`、`upgrade_backend.h`；本版已更名或合并，不能两套一起编。
4. 保留配套 BSP 和平台代码；本仓库已在 `platform.c` 加入毫秒计数，在 `platform_mb.c` 按实际 100 MHz 时钟计算 250 ms 定时周期。
5. 在 `upgrade_config.h` 填真实 UUID、模块 MARK 和各目标的配置项/程序编号，再将 `UP_BOARD_ENABLED` 设为 1。默认 0，服务不启用。B/C/D 编号未知时保持禁用。
6. `main_sdk.c` 使用静态 `172.16.10.157/24`、网关 0，不启动 DHCP，即使 BSP 编译了 DHCP 支持。MAC 沿用原 SDK，现场若分配了不同 MAC/IP/网关，应相应修改。
7. 默认暂存数组 32 MiB，位于链接器分配的 DDR BSS。上板必须检查 map、堆栈、DMA 和共享区，不能仅凭 DDR 总容量判断不冲突。
8. 后端需要完成真实容量检查、传输、擦写、读回验证和取消操作；报文 UTC 时钟暂填零，后续也在板级文件接入。框架不自动重启或重配置 FPGA。

[硬件后端接口说明](docs/升级后端接口.md) 记录了已核对的 BSP/BD 和仍需接入的硬件。现有 BD 为 Vivado 2018.3 / XC7VX690T，尚未包含图示新增升级控制、Aurora 和 Flash 控制模块；参考文件位于 `docs/bd-reference/` 和 `docs/bsp-reference/`。
