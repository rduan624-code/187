/* SDK 应用入口：只负责初始化网络，然后反复处理网络和升级任务。
 * 工程只保留这个 main()；不要同时加入旧 main.c 或 tests 中的 main()。
 * 本文件针对已提供的 MicroBlaze + standalone + lwIP RAW BSP。
 */
#include "platform.h"
#include "platform_config.h"
#include "xparameters.h"
#include "xil_printf.h"
#include "netif/xadapter.h"
#include "lwip/init.h"
#include "lwip/tcp.h"
#include "upgrade_config.h"
#include "upgrade_udp_sdk.h"

#if LWIP_IPV6
#error "This application uses the supplied IPv4 BSP"
#endif

/* 保留原平台的定时机制：中断置标志，主循环处理 TCP 定时任务。 */
/* 此版 BSP 的 tcp.h 未公开这两个原有定时函数，沿用原工程声明。 */
void tcp_fasttmr(void);
void tcp_slowtmr(void);
extern volatile int TcpFastTmrFlag;
extern volatile int TcpSlowTmrFlag;

static struct netif g_netif;

/* 辅助宏：将配置中的四个 IP 数字展开给 lwIP。 */
#define SET_IP_ADDRESS(address, ...) IP4_ADDR(address, __VA_ARGS__)

int main(void)
{
    ip_addr_t ip_address;
    ip_addr_t netmask;
    ip_addr_t gateway;
    unsigned char mac_address[6] = {0x00, 0x0A, 0x35, 0x00, 0x01, 0x02};

    /* 第一步：初始化 CPU 平台和 lwIP 网络协议栈。 */
    init_platform();
    lwip_init();

    /* 第二步：设置静态地址。本应用不启动 DHCP 客户端。 */
    SET_IP_ADDRESS(&ip_address, UP_BOARD_IP);
    SET_IP_ADDRESS(&netmask, UP_BOARD_NETMASK);
    SET_IP_ADDRESS(&gateway, UP_BOARD_GATEWAY);

    /* 第三步：把实际 AXI Ethernet 网卡接入 lwIP。 */
    if (xemac_add(&g_netif, &ip_address, &netmask, &gateway, mac_address, PLATFORM_EMAC_BASEADDR) ==
        NULL)
    {
        xil_printf("Unable to initialize Ethernet\r\n");
        return -1;
    }
    netif_set_default(&g_netif);
    platform_enable_interrupts();
    netif_set_up(&g_netif);

    /* 第四步：启动 UDP 升级服务。这里也会初始化升级状态和硬件回调。 */
    if (upgrade_udp_sdk_init() != 0)
    {
        xil_printf("Unable to initialize UDP upgrade\r\n");
        return -2;
    }

    /* 第五步：一直运行，每一圈只推进一点工作，不等待整片 Flash 烧完。 */
    while (1)
    {
        if (TcpFastTmrFlag != 0)
        {
            tcp_fasttmr();
            TcpFastTmrFlag = 0;
        }
        if (TcpSlowTmrFlag != 0)
        {
            tcp_slowtmr();
            TcpSlowTmrFlag = 0;
        }

        /* 收到 UDP 时，lwIP 会自动调用已注册的 on_udp_received()。 */
        xemacif_input(&g_netif);

        /* 发送应答、检查超时、推进目标准备及烧写。 */
        upgrade_udp_sdk_poll();
    }
}
