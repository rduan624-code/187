/*
 * jtag_gpio.h
 *
 *  Created on: 2026妤犵儑鎷�5闁哄牞鎷�20闁哄喛鎷�
 *      Author: TW
 */

#ifndef JTAG_GPIO_H
#define JTAG_GPIO_H

#include "xil_types.h"
#include "xgpio.h"
#include "xparameters.h"


#define DB_BASE_ADDR         XPAR_DEBUG_BRIDGE_0_BASEADDR           //debug bridge 瀵勫瓨鍣ㄥ熀鍦板潃+鍋忕Щ
//瀵勫瓨鍣ㄨ鍐欏畯瀹氫箟
#define DB_DATA_REG         (*((volatile uint32_t*)(XPAR_DEBUG_BRIDGE_0_BASEADDR + 0x00)))
#define DB_CTRL_REG         (*((volatile uint32_t*)(XPAR_DEBUG_BRIDGE_0_BASEADDR + 0x04)))
#define DB_STATUS_REG       (*((volatile uint32_t*)(XPAR_DEBUG_BRIDGE_0_BASEADDR + 0x08)))


#define CHAN_EXT1     1
#define CHAN_EXT2     2
#define CHAN_EXT3     3
#define CHAN_SELF     4
#define CHAN_IDLE     0


#define GPIO0_OUT_DATA     (*(volatile uint32_t*)(XPAR_AXI_GPIO_0_BASEADDR + 0x00))//3bit闁哄鐗婇幐鎼佸吹椤撱垺鏅柛锔炬巢K,TMS,TDI闂佹寧鍐婚幏锟�
#define GPIO0_OUT_DIR      (*(volatile uint32_t*)(XPAR_AXI_GPIO_0_BASEADDR + 0x04))

#define GPIO1_IN_DATA      (*(volatile uint32_t*)(XPAR_AXI_GPIO_1_BASEADDR + 0x00))//1bit闁哄鐗婇幐鎼佸矗閿燂拷 TDO
#define GPIO1_IN_DIR       (*(volatile uint32_t*)(XPAR_AXI_GPIO_1_BASEADDR + 0x04))


#define GPIO2_OUT_DATA     (*(volatile uint32_t*)(XPAR_AXI_GPIO_2_BASEADDR + 0x00))//3bit闁哄鐗婇幐鎼佸吹椤撱垺鏅柛锔炬巢K,TMS,TDI闂佹寧鍐婚幏锟�
#define GPIO2_OUT_DIR      (*(volatile uint32_t*)(XPAR_AXI_GPIO_2_BASEADDR + 0x04))

#define GPIO3_IN_DATA      (*(volatile uint32_t*)(XPAR_AXI_GPIO_3_BASEADDR + 0x00))//1bit闁哄鐗婇幐鎼佸矗閿燂拷 TDO
#define GPIO3_IN_DIR       (*(volatile uint32_t*)(XPAR_AXI_GPIO_3_BASEADDR + 0x04))


#define GPIO4_OUT_DATA     (*(volatile uint32_t*)(XPAR_AXI_GPIO_4_BASEADDR + 0x00))//3bit闁哄鐗婇幐鎼佸吹椤撱垺鏅柛锔炬巢K,TMS,TDI闂佹寧鍐婚幏锟�
#define GPIO4_OUT_DIR      (*(volatile uint32_t*)(XPAR_AXI_GPIO_4_BASEADDR + 0x04))

#define GPIO5_IN_DATA      (*(volatile uint32_t*)(XPAR_AXI_GPIO_5_BASEADDR + 0x00))//1bit闁哄鐗婇幐鎼佸矗閿燂拷 TDO
#define GPIO5_IN_DIR       (*(volatile uint32_t*)(XPAR_AXI_GPIO_5_BASEADDR + 0x04))


#define PIN_TCK     2
#define PIN_TMS     1
#define PIN_TDI     0
#define PIN_TDO     0


extern volatile uint8_t g_active_chan;

void jtag1_init(void);
void jtag2_init(void);
void jtag3_init(void);
void jtag_delay_loop(void);
uint32_t jtag_xfer1(uint32_t bits,uint32_t tms, uint32_t tdo);
uint32_t jtag_xfer2(uint32_t bits,uint32_t tms, uint32_t tdo);
uint32_t jtag_xfer3(uint32_t bits,uint32_t tms, uint32_t tdo);
uint32_t DB_JtagShift(uint32_t bits,uint32_t tms, uint32_t tdi);

uint32_t jtag_read_idcode_test1(void);
uint32_t jtag_read_idcode_test2(void);
uint32_t jtag_read_idcode_test3(void);


#endif
