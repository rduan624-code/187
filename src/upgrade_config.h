#ifndef UPGRADE_CONFIG_H
#define UPGRADE_CONFIG_H

/* 集中配置：先填写真实 UUID、模块编号及镜像缓冲大小，再启用升级。
 * A 是 MicroBlaze 所在板；B/C/D 通过远端硬件通道升级。 */
#ifndef UP_BOARD_ENABLED
#define UP_BOARD_ENABLED 0
#endif
#ifndef UP_BOARD_UUID
#define UP_BOARD_UUID 0u /* 待填写：不要根据处理器编号猜 UUID。 */
#endif
#ifndef UP_BOARD_MODULE
#define UP_BOARD_MODULE 0u /* 待填写：这是 1 字节 MARK，不是 UUID 低字节。 */
#endif
#ifndef UP_BOARD_CONFIGURATION
#define UP_BOARD_CONFIGURATION 0x004bu
#endif
#ifndef UP_BOARD_PROGRAM
#define UP_BOARD_PROGRAM 0x4b0au
#endif
#ifndef UP_BOARD_IMAGE_CAPACITY
#define UP_BOARD_IMAGE_CAPACITY (32u * 1024u * 1024u)
#endif
#define UP_BOARD_IP 172, 16, 10, 157
#define UP_BOARD_NETMASK 255, 255, 255, 0
#define UP_BOARD_GATEWAY 0, 0, 0, 0
/* B/C/D 的配置项和程序编号未知，填好后才启用相应目标。 */
#ifndef UP_BOARD_PROGRAM_B
#define UP_BOARD_PROGRAM_B 0u
#endif
#ifndef UP_BOARD_PROGRAM_C
#define UP_BOARD_PROGRAM_C 0u
#endif
#ifndef UP_BOARD_PROGRAM_D
#define UP_BOARD_PROGRAM_D 0u
#endif
#ifndef UP_BOARD_CONFIGURATION_B
#define UP_BOARD_CONFIGURATION_B 0u
#endif
#ifndef UP_BOARD_CONFIGURATION_C
#define UP_BOARD_CONFIGURATION_C 0u
#endif
#ifndef UP_BOARD_CONFIGURATION_D
#define UP_BOARD_CONFIGURATION_D 0u
#endif
#define UP_BOARD_PORT 10001u
#define UP_MLC_PORT 10000u

/* 等待时间单位为毫秒，不是主循环次数。 */
#define UP_REPORT_QUEUE 16u
#define UP_RECEIVE_TIMEOUT_MS 30000u
#define UP_BURN_TIMEOUT_MS 120000u
#endif
