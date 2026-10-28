#ifndef UPGRADE_BOARD_H
#define UPGRADE_BOARD_H
#include "icd_upgrade.h"
#include "upgrade_config.h"

/* 这三个值表示走哪条硬件通路，不是 FPGA 的编号。 */
#define UP_TARGET_INVALID 0
#define UP_TARGET_LOCAL 1
#define UP_TARGET_REMOTE 2
int upgrade_board_get_target_type(uint8_t target);

/* prepare: 0 表示已启动准备，负数表示无法启动。
 * prepare_step / step: 0 表示还在忙，1 表示完成，负数表示失败。
 * begin: 0 表示已启动烧写，负数表示无法启动。
 * 这些函数每次只做一小步，不能在里面一直等待硬件。 */
int upgrade_board_prepare(uint8_t target, const icd_summary_t *summary, const uint8_t *image,
                          size_t image_size);
int upgrade_board_prepare_step(uint8_t target, uint8_t *fault);
int upgrade_board_begin(uint8_t target);
int upgrade_board_step(uint8_t target, uint8_t *progress);
void upgrade_board_abort(uint8_t target);
void upgrade_board_time(uint32_t *seconds, uint32_t *milliseconds);
#endif
