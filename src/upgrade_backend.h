#ifndef UPGRADE_BACKEND_H
#define UPGRADE_BACKEND_H
#include "upgrade_protocol.h"
/* prepare may retain image/metadata pointers until success/abort, never afterwards.
   See docs/升级后端接口.md before connecting hardware. */
typedef struct {
    int (*prepare)(uint8_t,const up_metadata *,const uint8_t *,size_t);
    int (*prepare_step)(uint8_t,uint8_t *fault);
    int (*begin)(uint8_t);
    int (*step)(uint8_t,uint8_t *progress);
    void (*abort)(uint8_t);
} upgrade_backend;
extern const upgrade_backend upgrade_local_backend;
extern const upgrade_backend upgrade_aurora_backend;
#endif
