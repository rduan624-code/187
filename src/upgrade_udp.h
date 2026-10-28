#ifndef UPGRADE_UDP_H
#define UPGRADE_UDP_H
int upgrade_udp_start(void);
void upgrade_udp_poll(void);
/* Call from the existing 250ms MicroBlaze timer ISR; only increments a counter. */
void upgrade_udp_tick_250ms(void);
#endif
