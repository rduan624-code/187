/* 板级硬件接口：以后拿到 Flash 手册，主要修改本文件。
 * A 卡：本地 Flash。
 * B/C/D 卡：AXI 控制 + 共享缓冲 + FIFO/Aurora + 对端 Flash。
 * 目前还没有真实驱动，下面明确返回失败，不会假装烧写成功。
 */
#include "upgrade_board.h"

int upgrade_board_get_target_type(uint8_t target)
{
    switch (target)
    {
    case 0x71: /* A 卡，也是运行 MicroBlaze 的卡。 */
        return UP_TARGET_LOCAL;
    case 0x72: /* B 卡 */
    case 0x73: /* C 卡 */
    case 0x74: /* D 卡 */
        return UP_TARGET_REMOTE;
    default:
        return UP_TARGET_INVALID;
    }
}

/* A 本地：准备目标的暂存区并检查容量。远端还需传输摘要、镜像并等待确认。 */
static int local_prepare(uint8_t target, const icd_summary_t *summary, const uint8_t *image,
                         size_t image_size)
{
    (void)target;
    (void)summary;
    (void)image;
    (void)image_size;
    /* 待接入真实硬件，目前返回“未实现”。 */
    return -1;
}

/* B/C/D 远端：准备目标的暂存区并检查容量。远端还需传输摘要、镜像并等待确认。 */
static int remote_prepare(uint8_t target, const icd_summary_t *summary, const uint8_t *image,
                          size_t image_size)
{
    (void)target;
    (void)summary;
    (void)image;
    (void)image_size;
    /* 待接入真实硬件，目前返回“未实现”。 */
    return -1;
}

int upgrade_board_prepare(uint8_t target, const icd_summary_t *summary, const uint8_t *image,
                          size_t image_size)
{
    int target_type = upgrade_board_get_target_type(target);
    if (target_type == UP_TARGET_LOCAL)
    {
        return local_prepare(target, summary, image, image_size);
    }
    if (target_type == UP_TARGET_REMOTE)
    {
        return remote_prepare(target, summary, image, image_size);
    }
    return -1;
}

/* A 本地：查询准备状态。只有目标校验通过且空间足够，才允许返回 1。 */
static int local_prepare_step(uint8_t target, uint8_t *fault)
{
    (void)target;
    *fault = 0;
    /* 待接入真实硬件，目前返回“未实现”。 */
    return -1;
}

/* B/C/D 远端：查询准备状态。只有目标校验通过且空间足够，才允许返回 1。 */
static int remote_prepare_step(uint8_t target, uint8_t *fault)
{
    (void)target;
    *fault = 0;
    /* 待接入真实硬件，目前返回“未实现”。 */
    return -1;
}

int upgrade_board_prepare_step(uint8_t target, uint8_t *fault)
{
    int target_type = upgrade_board_get_target_type(target);
    *fault = 0;
    if (target_type == UP_TARGET_LOCAL)
    {
        return local_prepare_step(target, fault);
    }
    if (target_type == UP_TARGET_REMOTE)
    {
        return remote_prepare_step(target, fault);
    }
    return -1;
}

/* A 本地：启动真正的擦写。不要在此阻塞到整个 Flash 烧写结束。 */
static int local_begin(uint8_t target)
{
    (void)target;
    /* 待接入真实硬件，目前返回“未实现”。 */
    return -1;
}

/* B/C/D 远端：启动真正的擦写。不要在此阻塞到整个 Flash 烧写结束。 */
static int remote_begin(uint8_t target)
{
    (void)target;
    /* 待接入真实硬件，目前返回“未实现”。 */
    return -1;
}

int upgrade_board_begin(uint8_t target)
{
    int target_type = upgrade_board_get_target_type(target);
    if (target_type == UP_TARGET_LOCAL)
    {
        return local_begin(target);
    }
    if (target_type == UP_TARGET_REMOTE)
    {
        return remote_begin(target);
    }
    return -1;
}

/* A 本地：查询烧写状态。镜像和摘要均持久化并验证完成，才能返回 1。 */
static int local_step(uint8_t target, uint8_t *progress)
{
    (void)target;
    *progress = 0;
    /* 待接入真实硬件，目前返回“未实现”。 */
    return -1;
}

/* B/C/D 远端：查询烧写状态。镜像和摘要均持久化并验证完成，才能返回 1。 */
static int remote_step(uint8_t target, uint8_t *progress)
{
    (void)target;
    *progress = 0;
    /* 待接入真实硬件，目前返回“未实现”。 */
    return -1;
}

int upgrade_board_step(uint8_t target, uint8_t *progress)
{
    int target_type = upgrade_board_get_target_type(target);
    *progress = 0;
    if (target_type == UP_TARGET_LOCAL)
    {
        return local_step(target, progress);
    }
    if (target_type == UP_TARGET_REMOTE)
    {
        return remote_step(target, progress);
    }
    return -1;
}

/* 停止旧任务，防止旧 DMA 或远端操作继续使用下一次升级的缓冲区。
 * 目前没有启动任何硬件，因此空实现是安全的；接入驱动后必须补齐取消逻辑。 */
void upgrade_board_abort(uint8_t target)
{
    int target_type = upgrade_board_get_target_type(target);
    if (target_type == UP_TARGET_LOCAL)
    {
        /* TODO：停止 A 卡本地任务并释放资源。 */
    }
    else if (target_type == UP_TARGET_REMOTE)
    {
        /* TODO：取消对应 B/C/D 事务，等待确认硬件不再访问旧缓冲。 */
    }
}

/* 报文的 UTC 时标，与用于超时判断的毫秒计数器是两回事。 */
void upgrade_board_time(uint32_t *seconds, uint32_t *milliseconds)
{
    /* 暂无 UTC 时间源，先填零，不把上电时间冒充日期时间。 */
    *seconds = 0;
    *milliseconds = 0;
}
