/* UDP 适配层：让 lwIP 和升级业务连接起来。
 * 收包路径：on_udp_received → upgrade_app_on_datagram。
 * 主循环路径：upgrade_udp_sdk_poll → upgrade_app_poll。
 */
#include "upgrade_udp_sdk.h"
#include "upgrade_app.h"
#include "upgrade_board.h"
#include "lwip/udp.h"
#include "lwip/ip_addr.h"
#include "lwip/pbuf.h"
#include "xil_printf.h"
#include <string.h>
#if UP_BOARD_ENABLED && (UP_BOARD_UUID == 0)
#error "Set the A-board UUID before enabling UDP upgrade"
#endif
#if UP_BOARD_ENABLED && LWIP_IPV6
#error                                                                                             \
    "This A-board upgrade adapter currently requires IPv4 (LWIP_IPV6=0); main_sdk.c uses static IPv4"
#endif
static volatile uint32_t ticks_ms;
void upgrade_udp_sdk_tick_250ms(void)
{
    ticks_ms += 250u;
}
#if UP_BOARD_ENABLED
static struct udp_pcb *upgrade_pcb;
static upgrade_app_t session;
/* 固件暂存数组，由链接器放入 DDR。上板前必须核对 map 和其他 DDR 使用者。 */
static uint8_t image[UP_BOARD_IMAGE_CAPACITY];
static uint8_t packet[UP_MAX_PACKET];
/* 为一份完整应答分配 lwIP 缓冲，复制内容，发送后释放缓冲。 */
static int send_report(void *context, upgrade_peer_t peer, const uint8_t *data, size_t length)
{
    struct pbuf *packet_buffer;
    ip_addr_t dest;
    err_t error;
    (void)context;
    memset(&dest, 0, sizeof(dest));
    ip4_addr_set_u32(ip_2_ip4(&dest), peer.address);
    packet_buffer = pbuf_alloc(PBUF_TRANSPORT, (u16_t)length, PBUF_RAM);
    if (!packet_buffer)
    {
        return -1;
    }
    error = pbuf_take(packet_buffer, data, (u16_t)length);
    if (error == ERR_OK)
    {
        error = udp_sendto(upgrade_pcb, packet_buffer, &dest, peer.port);
    }
    pbuf_free(packet_buffer);
    if (error == ERR_OK)
    {
        return 0;
    }
    return -1;
}
/* 以下小函数用于把业务配置中的接口接到 upgrade_board.c。 */
static void clock_time(void *context, uint32_t *seconds, uint32_t *milliseconds)
{
    (void)context;
    upgrade_board_time(seconds, milliseconds);
}
static int prepare_target(void *context, uint8_t target, const icd_summary_t *summary,
                          const uint8_t *data, size_t length)
{
    (void)context;
    return upgrade_board_prepare(target, summary, data, length);
}
static int poll_target_prepare(void *context, uint8_t target, uint8_t *fault)
{
    (void)context;
    return upgrade_board_prepare_step(target, fault);
}
static int start_target_burn(void *context, uint8_t target)
{
    (void)context;
    return upgrade_board_begin(target);
}
static int poll_target_burn(void *context, uint8_t target, uint8_t *progress)
{
    (void)context;
    return upgrade_board_step(target, progress);
}
static void abort_target(void *context, uint8_t target)
{
    (void)context;
    upgrade_board_abort(target);
}
/* lwIP 自动调用这个函数；pbuf 可能由多段组成，必须复制整条链。 */
static void on_udp_received(void *arg, struct udp_pcb *pcb, struct pbuf *packet_buffer,
                            const ip_addr_t *address, u16_t port)
{
    upgrade_peer_t peer;
    size_t length;
    (void)arg;
    (void)pcb;
    if (!packet_buffer)
    {
        return;
    }
    length = packet_buffer->tot_len;
    if (port == UP_MLC_PORT && length <= sizeof(packet) &&
        pbuf_copy_partial(packet_buffer, packet, (u16_t)length, 0) == length)
    {
        peer.address = ip4_addr_get_u32(ip_2_ip4(address));
        peer.port = port;
        (void)upgrade_app_on_datagram(&session, peer, packet, length, ticks_ms);
    }
    pbuf_free(packet_buffer);
    /* 此处不烧写 Flash。报告发送和硬件步骤由主循环继续处理。 */
}
/* 创建 UDP 服务，注册接收回调，并初始化业务所需的配置。 */
int upgrade_udp_sdk_init(void)
{
    upgrade_config_t config;
    err_t error;
    if (upgrade_pcb)
    {
        return 0;
    }
    memset(&config, 0, sizeof(config));
    config.local_uuid = UP_BOARD_UUID;
    config.module = UP_BOARD_MODULE;
    /* 逐项赋值，方便核对每张卡的编号。B/C/D 的程序编号为零时不启用。 */
    config.targets[0].enabled = 1;
    config.targets[0].configuration = UP_BOARD_CONFIGURATION;
    config.targets[0].program = UP_BOARD_PROGRAM;
    config.targets[1].enabled = (UP_BOARD_PROGRAM_B != 0);
    config.targets[1].configuration = UP_BOARD_CONFIGURATION_B;
    config.targets[1].program = UP_BOARD_PROGRAM_B;
    config.targets[2].enabled = (UP_BOARD_PROGRAM_C != 0);
    config.targets[2].configuration = UP_BOARD_CONFIGURATION_C;
    config.targets[2].program = UP_BOARD_PROGRAM_C;
    config.targets[3].enabled = (UP_BOARD_PROGRAM_D != 0);
    config.targets[3].configuration = UP_BOARD_CONFIGURATION_D;
    config.targets[3].program = UP_BOARD_PROGRAM_D;
    config.image = image;
    config.capacity = sizeof(image);
    config.send = send_report;
    config.time = clock_time;
    config.prepare = prepare_target;
    config.prepare_step = poll_target_prepare;
    config.begin = start_target_burn;
    config.step = poll_target_burn;
    config.abort = abort_target;
    if (upgrade_app_init(&session, &config))
    {
        return -1;
    }
    upgrade_pcb = udp_new();
    if (!upgrade_pcb)
    {
        return -1;
    }
    error = udp_bind(upgrade_pcb, IP_ADDR_ANY, UP_BOARD_PORT);
    if (error != ERR_OK)
    {
        udp_remove(upgrade_pcb);
        upgrade_pcb = NULL;
        return -1;
    }
    udp_recv(upgrade_pcb, on_udp_received, NULL);
    xil_printf("UDP upgrade listening on 10001 (configured A/B/C/D targets)\r\n");
    return 0;
}
/* 主循环入口：使用中断维护的毫秒计数进行超时检查。 */
void upgrade_udp_sdk_poll(void)
{
    if (upgrade_pcb)
    {
        upgrade_app_poll(&session, ticks_ms);
    }
}
#else
int upgrade_udp_sdk_init(void)
{
    xil_printf("A-board upgrade disabled: configure upgrade_config.h first\r\n");
    return 0;
}
void upgrade_udp_sdk_poll(void)
{
}
#endif
