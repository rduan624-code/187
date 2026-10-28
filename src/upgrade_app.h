#ifndef UPGRADE_APP_H
#define UPGRADE_APP_H
#include "icd_upgrade.h"
#include "upgrade_config.h"

typedef struct
{
    uint32_t address;
    uint16_t port;
} upgrade_peer_t;
typedef enum
{
    UP_IDLE, /* 空闲，等待首帧 */

    UP_RECEIVING, /* 接收文件分帧 */

    UP_PREPARING, /* 准备目标：检查空间，必要时向远端传输 */

    UP_WAIT_BURN, /* 等待加载成功应答发出 */

    UP_BURNING, /* 执行和查询实际烧写 */

    UP_DONE, /* 完成，暂存结果以便重传查询 */

    UP_FAILED /* 失败，暂存失败结果 */
} upgrade_state_t;
typedef struct
{
    uint8_t enabled;
    uint16_t configuration; /* 该目标允许的配置项 */
    uint16_t program;       /* 该目标允许的程序编号 */
} upgrade_target_t;
typedef struct
{
    uint16_t local_uuid;
    upgrade_target_t targets[4]; /* 数组下标 0/1/2/3 对应 A/B/C/D */
    uint8_t module;
    uint8_t *image;
    size_t capacity;
    void *context;
    /* 发送接口：0=网络栈已接纳；其他值=暂时失败，下次循环重试。 */
    int (*send)(void *, upgrade_peer_t, const uint8_t *, size_t);
    /* 可选 UTC 时间源。未提供时，时标填零。 */
    void (*time)(void *, uint32_t *seconds, uint32_t *milliseconds);
    /* 硬件接口只由主循环调用。第一次阅读可先跳过函数指针语法，
     * 将 config.prepare(...) 理解为“调用配置好的目标准备函数”。
     * prepare: 返回 0 表示已启动准备，负数表示失败。
     * prepare_step: 0=忙，1=目标文件/空间已检查，负数=失败。
     * begin: 返回 0 表示已启动烧写，负数表示无法启动。
     * step: 0=忙，1=镜像和摘要均已持久化且校验成功，负数=失败。
     * abort: 停止旧任务，返回前保证硬件不再访问旧缓冲。
     * 对 B/C/D，发送完数据不等于远端烧写完成，必须等远端确认。
     * image/metadata 指针在成功或 abort 后不得继续被后端保存使用。 */
    int (*prepare)(void *, uint8_t target, const icd_summary_t *, const uint8_t *, size_t);
    int (*prepare_step)(void *, uint8_t target, uint8_t *fault);
    int (*begin)(void *, uint8_t target);
    int (*step)(void *, uint8_t target, uint8_t *progress);
    void (*abort)(void *, uint8_t target);
} upgrade_config_t;
typedef struct
{
    upgrade_peer_t peer;
    uint8_t data[UP_ACK_SIZE];
    size_t length;
} upgrade_report_t;
typedef struct
{
    upgrade_config_t config;
    upgrade_state_t state;
    upgrade_peer_t peer;
    icd_update_frame_t request;
    icd_summary_t metadata;
    uint16_t last_sequence; /* 最近成功保存的文件帧号 */
    uint16_t report_serial; /* 下一份应答使用的流水号 */
    size_t received;
    uint64_t sum;
    uint32_t last_activity; /* 最近接收时间，单位 ms */
    uint32_t burn_started;  /* 当前硬件阶段的开始时间，单位 ms */
    uint8_t prepare_started;
    uint8_t load_ok;          /* 加载是否成功 */
    uint8_t load_fault;       /* 加载故障码 */
    uint8_t burn_result;      /* 烧写结果 */
    uint8_t progress;         /* 进度编码 0～4 */
    uint8_t have_burn_result; /* 是否已有烧写结果 */
    upgrade_report_t pending[UP_REPORT_QUEUE];
    unsigned head;  /* 待发送队列的队首位置 */
    unsigned count; /* 待发送报告数量 */
} upgrade_app_t;
int upgrade_app_init(upgrade_app_t *app, const upgrade_config_t *config);
/* 0=已处理，-1=丢弃。只接受已配置的 0x71～0x74，同时只有一个升级会话。 */
int upgrade_app_on_datagram(upgrade_app_t *app, upgrade_peer_t peer, const uint8_t *data,
                            size_t length, uint32_t now_ms);
void upgrade_app_poll(upgrade_app_t *app, uint32_t now_ms);
#endif
