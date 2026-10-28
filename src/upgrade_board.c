#include "upgrade_board.h"
#include "upgrade_backend.h"
static const upgrade_backend *backend(uint8_t target) {
    if (target==0x71) return &upgrade_local_backend;
    if (target>=0x72 && target<=0x74) return &upgrade_aurora_backend;
    return NULL;
}
int upgrade_board_prepare(uint8_t target,const up_metadata *m,const uint8_t *image,size_t n) {
    const upgrade_backend *b=backend(target);
    return b ? b->prepare(target,m,image,n) : -1;
}
int upgrade_board_prepare_step(uint8_t target,uint8_t *fault) {
    const upgrade_backend *b=backend(target);
    *fault=0; return b ? b->prepare_step(target,fault) : -1;
}
int upgrade_board_begin(uint8_t target) {
    const upgrade_backend *b=backend(target); return b ? b->begin(target) : -1;
}
int upgrade_board_step(uint8_t target,uint8_t *progress) {
    const upgrade_backend *b=backend(target);
    *progress=0; return b ? b->step(target,progress) : -1;
}
void upgrade_board_abort(uint8_t target) {
    const upgrade_backend *b=backend(target); if (b) b->abort(target);
}
void upgrade_board_time(uint32_t *seconds,uint32_t *milliseconds) {
    /* Replace with system UTC source. No UTC clock was supplied in the SDK. */
    *seconds=0; *milliseconds=0;
}
