/* 升级业务主流程：一份镜像、一位发送者、一个目标，每次只处理一个会话。
 * 阅读顺序：upgrade_app_init → upgrade_app_on_datagram → upgrade_app_poll。
 * app 是保存当前进度的结构体；app->state 表示“现在做到哪一步”。
 */
#include "upgrade_app.h"
#include <string.h>
/* 会话绑定：来自同一个 IP 和端口，才认为是同一位发送者。 */
static int is_same_peer(upgrade_peer_t a, upgrade_peer_t b)
{
    return a.address == b.address && a.port == b.port;
}
/* 核对目标和文件标识，避免升级中途把 A 的数据混入 B 的文件。 */
static int is_same_target(const icd_update_frame_t *a, const icd_update_frame_t *b)
{
    return a->source == b->source && a->destination == b->destination && a->module == b->module &&
           a->processor == b->processor && a->configuration == b->configuration &&
           a->program == b->program && a->total == b->total;
}
/* 将一份完整应答放进环形队列。head 是队首，count 是待发送数量。 */
static void queue_report(upgrade_app_t *app, upgrade_peer_t peer, const icd_update_frame_t *r,
                         uint16_t id, uint8_t result, uint16_t detail)
{
    uint32_t sec = 0, ms = 0;
    upgrade_report_t *frame = &app->pending[(app->head + app->count) % UP_REPORT_QUEUE];
    /* 调用方已预留应答队列空间。这里仅组包入队，稍后再通过 UDP 发出。 */
    if (app->config.time)
    {
        app->config.time(app->config.context, &sec, &ms);
    }
    if (ms > 999)
    {
        sec = 0;
        ms = 0;
    }
    app->report_serial++;
    if (app->report_serial == 0)
    {
        app->report_serial = 1;
    }
    frame->peer = peer;
    frame->length =
        icd_build_report(frame->data, r, app->report_serial, sec, ms, id, result, detail);
    app->count++;
}
/* 0089：文件校验或目标准备失败，此时不应开始烧写。 */
static void report_load_failure(upgrade_app_t *app, uint8_t code)
{
    app->state = UP_FAILED;
    app->load_ok = 0;
    app->load_fault = code;
    queue_report(app, app->peer, &app->request, 0x89, 0, code);
}
/* 008A：已经进入烧写阶段后失败，进度按协议填 0。 */
static void report_burn_failure(upgrade_app_t *app)
{
    app->state = UP_FAILED;
    app->have_burn_result = 1;
    app->burn_result = 0;
    app->progress = 0;
    queue_report(app, app->peer, &app->request, 0x8a, 0, 0);
}
/* 清空上一轮的业务进度；网络配置和函数接口继续保留。 */
static void reset_upgrade(upgrade_app_t *app)
{
    app->state = UP_IDLE;
    app->last_sequence = 0;
    app->received = 0;
    app->sum = 0;
    app->prepare_started = 0;
    app->load_ok = 0;
    app->load_fault = 0;
    app->have_burn_result = 0;
    app->progress = 0;
}
/* 初始化：保存缓冲区、目标表和板级函数接口。成功返回 0。 */
int upgrade_app_init(upgrade_app_t *app, const upgrade_config_t *config)
{
    if (!app || !config || !config->local_uuid || !config->image || !config->capacity ||
        !config->send || config->capacity > UINT32_MAX)
    {
        return -1;
    }
    memset(app, 0, sizeof(*app));
    app->config = *config;
    return 0;
}
/* 收到一帧：解析 → 核对目标 → 检查序号 → 保存数据 → 准备应答。
 * 本函数不等待 Flash，不执行耗时硬件操作。 */
int upgrade_app_on_datagram(upgrade_app_t *app, upgrade_peer_t peer, const uint8_t *data,
                            size_t length, uint32_t now_ms)
{
    icd_update_frame_t frame;
    icd_summary_t summary;
    const upgrade_target_t *target;
    /* 第一步：先确认报文完整，再判断它是不是发给本机的升级指令。 */
    if (!app || icd_parse_update(&frame, data, length) ||
        frame.destination != app->config.local_uuid || frame.module != app->config.module ||
        frame.processor < 0x71 || frame.processor > 0x74 || app->count > UP_REPORT_QUEUE - 3)
    {
        return -1;
    }
    target = &app->config.targets[frame.processor - 0x71];
    if (!target->enabled || frame.configuration != target->configuration ||
        frame.program != target->program)
    {
        return -1;
    }
    /* 上一轮结束且应答已发送后，允许不同的升级任务开始。
       完全相同的首帧仍视为旧任务重传，避免重复烧写。 */
    if ((app->state == UP_DONE || app->state == UP_FAILED) && app->count == 0 &&
        frame.sequence == 1 &&
        icd_parse_summary(&summary, frame.payload, frame.payload_size) == 0 &&
        (!is_same_peer(app->peer, peer) || !is_same_target(&app->request, &frame) ||
         memcmp(summary.raw, app->metadata.raw, UP_METADATA_SIZE) != 0))
    {
        reset_upgrade(app);
    }
    if (app->state != UP_IDLE &&
        (!is_same_peer(app->peer, peer) || !is_same_target(&app->request, &frame)))
    {
        return -1; /* 当前还在升级，不允许其他发送者或目标接管。 */
    }
    /* 第二步：尚未开始升级时，只允许首帧建立会话。 */
    if (app->state == UP_IDLE)
    {
        if (frame.sequence != 1)
        {
            queue_report(app, peer, &frame, 0x88, 0, 0);
            return 0;
        }
        if (icd_parse_summary(&summary, frame.payload, frame.payload_size))
        {
            queue_report(app, peer, &frame, 0x88, 0, 0);
            return 0;
        }
        reset_upgrade(app);
        app->peer = peer;
        app->request = frame;
        app->request.payload = NULL;
        app->request.payload_size = 0;
        app->metadata = summary;
        app->last_activity = now_ms;
        app->last_sequence = 1;
        app->state = UP_RECEIVING;
        queue_report(app, peer, &frame, 0x88, 1, 1);
        if (summary.program != frame.program)
        {
            report_load_failure(app, 4);
            return 0;
        }
        if (summary.image_size > app->config.capacity)
        {
            report_load_failure(app, 1);
            return 0;
        }
        /* 每个数据帧为 1～1024 字节，总帧数还包含第一个摘要帧。 */
        if ((uint64_t)summary.image_size > (uint64_t)(frame.total - 1) * UP_MAX_PAYLOAD ||
            summary.image_size < (uint32_t)(frame.total - 1))
        {
            report_load_failure(app, 3);
            return 0;
        }
        app->sum = icd_checksum_add(0, summary.raw + 4, UP_METADATA_SIZE - 4);
        return 0;
    }
    /* 第三步：后续帧必须连续。拒绝重复追加或乱序追加。 */
    if (app->state != UP_RECEIVING || frame.sequence != (uint32_t)app->last_sequence + 1)
    {
        /* 按已确定规则：非连续帧回复失败，帧号填本地最近接收的序号。
           最后一帧重传时可重发结果，但不能再次启动烧写。 */
        queue_report(app, peer, &frame, 0x88, 0, app->last_sequence);
        if ((app->state == UP_DONE || app->state == UP_FAILED) &&
            frame.sequence == app->last_sequence)
        {
            queue_report(app, peer, &app->request, 0x89, app->load_ok, app->load_fault);
            if (app->have_burn_result)
            {
                queue_report(app, peer, &app->request, 0x8a, app->burn_result, app->progress);
            }
        }
        return 0;
    }
    if (frame.payload_size > app->metadata.image_size - app->received)
    {
        queue_report(app, peer, &frame, 0x88, 0, app->last_sequence);
        report_load_failure(app, 3);
        return 0;
    }
    /* 第四步：先保存数据，再推进序号，最后回复“这一帧接收成功”。 */
    memcpy(app->config.image + app->received, frame.payload, frame.payload_size);
    app->sum = icd_checksum_add(app->sum, frame.payload, frame.payload_size);
    app->received += frame.payload_size;
    app->last_sequence = frame.sequence;
    app->last_activity = now_ms;
    queue_report(app, peer, &frame, 0x88, 1, frame.sequence);
    /* 第五步：最后一帧收齐后，检查整份文件，再进入目标准备阶段。 */
    if (frame.end == 0)
    {
        /* 先核对总长度，再核对累加和。超过 32 位时拒绝，不偷偷截断。 */
        if (app->received != app->metadata.image_size || app->sum > UINT32_MAX ||
            app->sum != app->metadata.checksum)
        {
            report_load_failure(app, 3);
            return 0;
        }
        app->state = UP_PREPARING;
        app->prepare_started = 0;
        app->burn_started = now_ms;
    }
    return 0;
}
/* 主循环每圈调用一次：发送报告 → 检查超时 → 推进当前硬件阶段。 */
void upgrade_app_poll(upgrade_app_t *app, uint32_t now_ms)
{
    int result;
    uint8_t progress, fault = 0;
    uint8_t target;
    if (!app)
    {
        return;
    }
    target = app->request.processor;
    /* 先按入队顺序发送应答。网络发送暂时失败时保留队首，下次循环重试。 */
    while (app->count)
    {
        upgrade_report_t *frame = &app->pending[app->head];
        if (app->config.send(app->config.context, frame->peer, frame->data, frame->length))
        {
            break;
        }
        app->head = (app->head + 1) % UP_REPORT_QUEUE;
        app->count--;
    }
    /* 即使网络发送堵塞，也要检查硬件超时，不能无限等待。 */
    if ((app->state == UP_BURNING || app->state == UP_PREPARING) &&
        (uint32_t)(now_ms - app->burn_started) >= UP_BURN_TIMEOUT_MS)
    {
        if (app->config.abort)
        {
            app->config.abort(app->config.context, target);
        }
        /* 接收入口预留的队列空间保证这里还能报告超时失败。 */
        if (app->state == UP_PREPARING)
        {
            report_load_failure(app, 0);
        }
        else
        {
            report_burn_failure(app);
        }
        app->last_activity = now_ms;
        return;
    }
    if (app->state != UP_PREPARING && app->state != UP_BURNING && app->state != UP_IDLE &&
        (uint32_t)(now_ms - app->last_activity) >= UP_RECEIVE_TIMEOUT_MS)
    {
        if (app->state == UP_WAIT_BURN && app->config.abort)
        {
            app->config.abort(app->config.context, target);
        }
        app->head = 0;
        app->count = 0;
        reset_upgrade(app);
        return;
    }
    if (app->count)
    {
        return;
    }
    /* 目标准备：A 检查本地存储；B/C/D 还需传输数据并等远端确认。 */
    if (app->state == UP_PREPARING)
    {
        if (!app->prepare_started)
        {
            if (!app->config.prepare || !app->config.prepare_step || !app->config.abort ||
                !app->config.begin || !app->config.step ||
                app->config.prepare(app->config.context, target, &app->metadata, app->config.image,
                                    app->received))
            {
                if (app->config.abort)
                {
                    app->config.abort(app->config.context, target);
                }
                report_load_failure(app, 0);
                app->last_activity = now_ms;
                return;
            }
            app->prepare_started = 1;
            return;
        }
        result = app->config.prepare_step(app->config.context, target, &fault);
        if (result < 0 || result > 1 || fault > 4)
        {
            app->config.abort(app->config.context, target);
            report_load_failure(app, fault <= 4 ? fault : 0);
            app->last_activity = now_ms;
        }
        else if (result == 1)
        {
            app->load_ok = 1;
            app->state = UP_WAIT_BURN;
            app->last_activity = now_ms;
            queue_report(app, app->peer, &app->request, 0x89, 1, 0);
        }
        return;
    }
    /* 加载成功报告已经交给网络栈，现在才能启动真正的烧写。 */
    if (app->state == UP_WAIT_BURN)
    {
        if (!app->config.begin || !app->config.step || !app->config.abort ||
            app->config.begin(app->config.context, target))
        {
            /* 驱动无法启动时，真实结果就是失败，不能模拟成功。 */
            if (app->config.abort)
            {
                app->config.abort(app->config.context, target);
            }
            report_burn_failure(app);
            app->last_activity = now_ms;
            return;
        }
        app->burn_started = now_ms;
        app->state = UP_BURNING;
        queue_report(app, app->peer, &app->request, 0x8a, 2, 0);
        return;
    }
    if (app->state != UP_BURNING)
    {
        return;
    }
    progress = app->progress;
    result = app->config.step(app->config.context, target, &progress);
    if (result < 0 || result > 1 || progress > 4 || (result == 0 && progress > 3) ||
        progress < app->progress)
    {
        app->config.abort(app->config.context, target);
        report_burn_failure(app);
        app->last_activity = now_ms;
    }
    else if (result == 1)
    {
        app->state = UP_DONE;
        app->have_burn_result = 1;
        app->burn_result = 1;
        app->progress = 4;
        app->last_activity = now_ms;
        queue_report(app, app->peer, &app->request, 0x8a, 1, 4);
    }
    else if (progress != app->progress)
    {
        app->progress = progress;
        queue_report(app, app->peer, &app->request, 0x8a, 2, progress);
    }
}
