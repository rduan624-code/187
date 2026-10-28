#ifndef UPGRADE_BOARD_H
#define UPGRADE_BOARD_H
#include "upgrade_session.h"
/* Enable only after setting UUID/MARK and checking DDR placement in the BSP.
   A is the MicroBlaze host (0x71); B/C/D (0x72..0x74) use the Aurora backend. */
#ifndef UP_BOARD_ENABLED
#define UP_BOARD_ENABLED 0
#endif
#ifndef UP_BOARD_UUID
#define UP_BOARD_UUID 0u /* REQUIRED: do not infer UUID from the processor number. */
#endif
#ifndef UP_BOARD_MODULE
#define UP_BOARD_MODULE 0u /* REQUIRED: one-byte MARK, not the low byte of UUID. */
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
#define UP_BOARD_IP 172,16,10,157
#define UP_BOARD_NETMASK 255,255,255,0
#define UP_BOARD_GATEWAY 0,0,0,0
/* The ICD only provides one program mapping. Remote mappings must be supplied. */
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
/* Dispatcher: A -> local Flash, B/C/D -> AXI/FIFO/Aurora transport.
   Backend implementations must obey upgrade_session.h's two-phase contract. */
int upgrade_board_prepare(uint8_t target,const up_metadata *,const uint8_t *,size_t);
int upgrade_board_prepare_step(uint8_t target,uint8_t *fault);
int upgrade_board_begin(uint8_t target);
int upgrade_board_step(uint8_t target,uint8_t *progress);
void upgrade_board_abort(uint8_t target);
void upgrade_board_time(uint32_t *seconds,uint32_t *milliseconds);
#endif
