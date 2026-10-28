#include "upgrade_backend.h"
/* B/C/D path: AXI control + shared buffer -> FIFO/Aurora -> remote DDR/Flash.
   These are explicit integration points, NOT an implemented hardware transport.
   No register offsets, command words, link IDs or remote ACK formats were provided.
   prepare_step must await remote staging/checksum/capacity confirmation.
   step success must await remote IMAGE+METADATA persistence/readback confirmation.
   An AXI write, FIFO-empty, or DMA-done indication alone is insufficient. */
static int prepare(uint8_t target,const up_metadata *m,const uint8_t *image,size_t n) {
    (void)target; (void)m; (void)image; (void)n; return -1;
}
static int prepare_step(uint8_t target,uint8_t *fault) { (void)target; *fault=0; return -1; }
static int begin(uint8_t target) { (void)target; return -1; }
static int step(uint8_t target,uint8_t *progress) { (void)target; *progress=0; return -1; }
static void abort_work(uint8_t target) { (void)target; }
const upgrade_backend upgrade_aurora_backend={prepare,prepare_step,begin,step,abort_work};
