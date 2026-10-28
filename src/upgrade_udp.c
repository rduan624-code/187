#include "upgrade_udp.h"
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
#error "This A-board upgrade adapter currently requires IPv4 (LWIP_IPV6=0); main.c skips DHCP while upgrade is enabled"
#endif
static volatile uint32_t ticks_ms;
void upgrade_udp_tick_250ms(void) { ticks_ms+=250u; }
#if UP_BOARD_ENABLED
static struct udp_pcb *upgrade_pcb;
static up_session session;
/* Dedicated BSS in DDR per lscript.ld. Check the linker map and other DDR users. */
static uint8_t image[UP_BOARD_IMAGE_CAPACITY];
static uint8_t packet[UP_MAX_PACKET];
static int send_report(void *ctx,up_peer peer,const uint8_t *data,size_t n) {
    struct pbuf *p;
    ip_addr_t dest;
    err_t err;
    (void)ctx;
    memset(&dest,0,sizeof(dest));
    ip4_addr_set_u32(ip_2_ip4(&dest),peer.address);
    p=pbuf_alloc(PBUF_TRANSPORT,(u16_t)n,PBUF_RAM);
    if (!p) return -1;
    err=pbuf_take(p,data,(u16_t)n);
    if (err==ERR_OK) err=udp_sendto(upgrade_pcb,p,&dest,peer.port);
    pbuf_free(p); return err==ERR_OK ? 0 : -1;
}
static void clock_time(void *ctx,uint32_t *sec,uint32_t *ms) {
    (void)ctx; upgrade_board_time(sec,ms);
}
static int prepare(void *ctx,uint8_t target,const up_metadata *m,const uint8_t *d,size_t n) {
    (void)ctx; return upgrade_board_prepare(target,m,d,n);
}
static int prepare_step(void *ctx,uint8_t target,uint8_t *fault) {
    (void)ctx; return upgrade_board_prepare_step(target,fault);
}
static int begin(void *ctx,uint8_t target) { (void)ctx; return upgrade_board_begin(target); }
static int step(void *ctx,uint8_t target,uint8_t *progress) {
    (void)ctx; return upgrade_board_step(target,progress);
}
static void abort_burn(void *ctx,uint8_t target) { (void)ctx; upgrade_board_abort(target); }
static void receive(void *arg,struct udp_pcb *pcb,struct pbuf *p,
                    const ip_addr_t *address,u16_t port) {
    up_peer peer;
    size_t n;
    (void)arg; (void)pcb;
    if (!p) return;
    n=p->tot_len;
    if (port==UP_MLC_PORT && n<=sizeof(packet) &&
        pbuf_copy_partial(p,packet,(u16_t)n,0)==n) {
        peer.address=ip4_addr_get_u32(ip_2_ip4(address)); peer.port=port;
        (void)up_receive(&session,peer,packet,n,ticks_ms);
    }
    pbuf_free(p);
    /* Reports/backend work are handled in the main loop, outside the RX callback. */
}
int upgrade_udp_start(void) {
    up_config cfg;
    err_t err;
    if (upgrade_pcb) return 0;
    memset(&cfg,0,sizeof(cfg));
    cfg.local_uuid=UP_BOARD_UUID; cfg.module=UP_BOARD_MODULE;
    cfg.targets[0]=(up_target){1,UP_BOARD_CONFIGURATION,UP_BOARD_PROGRAM};
    cfg.targets[1]=(up_target){UP_BOARD_PROGRAM_B!=0,UP_BOARD_CONFIGURATION_B,UP_BOARD_PROGRAM_B};
    cfg.targets[2]=(up_target){UP_BOARD_PROGRAM_C!=0,UP_BOARD_CONFIGURATION_C,UP_BOARD_PROGRAM_C};
    cfg.targets[3]=(up_target){UP_BOARD_PROGRAM_D!=0,UP_BOARD_CONFIGURATION_D,UP_BOARD_PROGRAM_D};
    cfg.image=image; cfg.capacity=sizeof(image); cfg.send=send_report; cfg.time=clock_time;
    cfg.prepare=prepare; cfg.prepare_step=prepare_step; cfg.begin=begin; cfg.step=step; cfg.abort=abort_burn;
    if (up_init(&session,&cfg)) return -1;
    upgrade_pcb=udp_new();
    if (!upgrade_pcb) return -1;
    err=udp_bind(upgrade_pcb,IP_ADDR_ANY,UP_BOARD_PORT);
    if (err!=ERR_OK) { udp_remove(upgrade_pcb); upgrade_pcb=NULL; return -1; }
    udp_recv(upgrade_pcb,receive,NULL);
    xil_printf("UDP upgrade listening on 10001 (configured A/B/C/D targets)\r\n");
    return 0;
}
void upgrade_udp_poll(void) { if (upgrade_pcb) up_poll(&session,ticks_ms); }
#else
int upgrade_udp_start(void) {
    xil_printf("A-board upgrade disabled: configure upgrade_board.h first\r\n");
    return 0;
}
void upgrade_udp_poll(void) { }
#endif
