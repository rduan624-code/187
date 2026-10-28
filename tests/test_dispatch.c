#include "upgrade_board.h"
#include <assert.h>
#include <stdio.h>
int main(void)
{
    assert(upgrade_board_get_target_type(0x71) == UP_TARGET_LOCAL);
    assert(upgrade_board_get_target_type(0x72) == UP_TARGET_REMOTE);
    assert(upgrade_board_get_target_type(0x73) == UP_TARGET_REMOTE);
    assert(upgrade_board_get_target_type(0x74) == UP_TARGET_REMOTE);
    assert(upgrade_board_get_target_type(0) == UP_TARGET_INVALID);
    assert(upgrade_board_get_target_type(0x75) == UP_TARGET_INVALID);
    puts("PASS: A uses local hardware; B/C/D use remote hardware; invalid targets rejected");
    return 0;
}
