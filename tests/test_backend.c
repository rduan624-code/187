#include "upgrade_board.h"
#include <assert.h>
#include <stdio.h>
int main(void) {
    unsigned target;
    uint8_t fault, progress, image=0;
    icd_summary_t meta={0};
    for(target=0x71;target<=0x74;target++) {
        assert(upgrade_board_prepare((uint8_t)target,&meta,&image,1)<0);
        assert(upgrade_board_prepare_step((uint8_t)target,&fault)<0);
        assert(fault==0);
        assert(upgrade_board_begin((uint8_t)target)<0);
        assert(upgrade_board_step((uint8_t)target,&progress)<0);
        assert(progress==0); upgrade_board_abort((uint8_t)target);
    }
    assert(upgrade_board_begin(0x75)<0);
    puts("PASS: unimplemented A/B/C/D hardware backends never return success");
    return 0;
}
