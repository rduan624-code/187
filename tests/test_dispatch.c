#include "upgrade_board.h"
#include "upgrade_backend.h"
#include <assert.h>
#include <stdio.h>
static unsigned local_calls,remote_calls;
static int local_prepare(uint8_t target,const up_metadata *m,const uint8_t *d,size_t n) {
    assert(target==0x71 && m && d && n==1); local_calls++; return 0;
}
static int remote_prepare(uint8_t target,const up_metadata *m,const uint8_t *d,size_t n) {
    assert(target>=0x72 && target<=0x74 && m && d && n==1); remote_calls++; return 0;
}
static int local_ready(uint8_t target,uint8_t *fault) { assert(target==0x71); *fault=0; return 1; }
static int remote_ready(uint8_t target,uint8_t *fault) { assert(target>=0x72 && target<=0x74); *fault=0; return 0; }
static int local_begin(uint8_t target) { assert(target==0x71); local_calls++; return 0; }
static int remote_begin(uint8_t target) { assert(target>=0x72 && target<=0x74); remote_calls++; return 0; }
static int local_step(uint8_t target,uint8_t *p) { assert(target==0x71); *p=4; return 1; }
static int remote_step(uint8_t target,uint8_t *p) { assert(target>=0x72 && target<=0x74); *p=1; return 0; }
static void local_abort(uint8_t target) { assert(target==0x71); local_calls++; }
static void remote_abort(uint8_t target) { assert(target>=0x72 && target<=0x74); remote_calls++; }
const upgrade_backend upgrade_local_backend={local_prepare,local_ready,local_begin,local_step,local_abort};
const upgrade_backend upgrade_aurora_backend={remote_prepare,remote_ready,remote_begin,remote_step,remote_abort};
int main(void) {
    unsigned target; uint8_t value,image=1; up_metadata meta={0};
    for(target=0x71;target<=0x74;target++) {
        assert(upgrade_board_prepare((uint8_t)target,&meta,&image,1)==0);
        assert(upgrade_board_prepare_step((uint8_t)target,&value)==(target==0x71));
        assert(upgrade_board_begin((uint8_t)target)==0);
        assert(upgrade_board_step((uint8_t)target,&value)==(target==0x71));
        assert(value==(target==0x71 ? 4 : 1));
        upgrade_board_abort((uint8_t)target);
    }
    assert(local_calls==3 && remote_calls==9);
    assert(upgrade_board_prepare(0x75,&meta,&image,1)<0);
    assert(local_calls==3 && remote_calls==9);
    puts("PASS: dispatcher uses local A and remote B/C/D backends without cross-routing");
    return 0;
}
