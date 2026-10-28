#include "upgrade_backend.h"
/* A-board Flash interface is not present in the supplied SDK/architecture drawing.
   Replace these fail-closed hooks after the actual BD/Flash map is established.
   Self-upgrade must keep MicroBlaze/lwIP alive; never reconfigure on reception. */
static int prepare(uint8_t target,const up_metadata *m,const uint8_t *image,size_t n) {
    (void)target; (void)m; (void)image; (void)n; return -1;
}
static int prepare_step(uint8_t target,uint8_t *fault) { (void)target; *fault=0; return -1; }
static int begin(uint8_t target) { (void)target; return -1; }
static int step(uint8_t target,uint8_t *progress) { (void)target; *progress=0; return -1; }
static void abort_work(uint8_t target) { (void)target; }
const upgrade_backend upgrade_local_backend={prepare,prepare_step,begin,step,abort_work};
