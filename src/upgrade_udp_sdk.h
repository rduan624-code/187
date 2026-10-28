#ifndef UPGRADE_UDP_SDK_H
#define UPGRADE_UDP_SDK_H
int upgrade_udp_sdk_init(void);
void upgrade_udp_sdk_poll(void);
/* Call from the existing 250ms MicroBlaze timer ISR; only increments a counter. */
void upgrade_udp_sdk_tick_250ms(void);
#endif
