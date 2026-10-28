
/*
 * jtag_gpio.c
 *
 *  Created on: 2026濡ょ姷鍎戦幏锟�5闂佸搫鐗為幏锟�20闂佸搫鍠涢幏锟�
 *      Author: TW
 */
#include "jtag_gpio.h"
#include "xil_io.h"
#include "sleep.h"
#include "xparameters.h"


static uint32_t jtag_delay = 4;//1M閿涳拷36,10M:4



void jtag1_init(void) {
	GPIO0_OUT_DIR  = 0x00000000U; //3bit 闁稿繈鍔忕欢顓㈠礄閿燂拷
	GPIO0_OUT_DATA = 0x00000000U; //閺夊牊鎸搁崵顓㈠礆濠靛棭娼楀ù锝忔嫹
	GPIO1_IN_DIR   = 0xFFFFFFFFU; //1bit 闁稿繈鍔忕欢顓㈠礂閿燂拷
}

void jtag2_init(void) {
	GPIO2_OUT_DIR  = 0x00000000U; //3bit 闁稿繈鍔忕欢顓㈠礄閿燂拷
	GPIO2_OUT_DATA = 0x00000000U; //閺夊牊鎸搁崵顓㈠礆濠靛棭娼楀ù锝忔嫹
	GPIO3_IN_DIR   = 0xFFFFFFFFU; //1bit 闁稿繈鍔忕欢顓㈠礂閿燂拷
}


void jtag3_init(void) {
	GPIO4_OUT_DIR  = 0x00000000U; //3bit 闁稿繈鍔忕欢顓㈠礄閿燂拷
	GPIO4_OUT_DATA = 0x00000000U; //閺夊牊鎸搁崵顓㈠礆濠靛棭娼楀ù锝忔嫹
	GPIO5_IN_DIR   = 0xFFFFFFFFU; //1bit 闁稿繈鍔忕欢顓㈠礂閿燂拷
}


void jtag_delay_loop(void) {
	for(uint32_t i = 0; i<jtag_delay; i++){
		__asm__ __volatile__("nop");
	}
}


uint32_t DB_JtagShift(uint32_t bits,uint32_t tms, uint32_t tdi) {

	//鑷韩閫氶亾锛岄噸鍐檇ebug bridge

	if(bits == 0 || bits > 0xFFFFU){
		return 0;
	}

	DB_CTRL_REG = bits;

	DB_DATA_REG = ((tms & 0xFFFFU) << 16) | (tdi & 0xFFFFU);
	//瓒呮椂闃叉姝婚攣
	uint32_t timeout_cnt = 0;
	const uint32_t MAX_TIMEOUT = 100000U;
	while(!(DB_STATUS_REG & 0x01U)){
		timeout_cnt++;
		if(timeout_cnt >= MAX_TIMEOUT){
			//xil_prinft("DB_Shift Timeout!\r\n");
			return 0;
		}
	}

	return DB_DATA_REG & 0xFFFFU;
}



uint32_t jtag_xfer1(uint32_t bits,uint32_t tms, uint32_t tdi) {

	uint32_t tdo = 0;
	for(uint32_t i = 0; i<bits; i++){

		uint8_t tdi_bit = (tdi>>i) &1U;
		uint8_t tms_bit = (tms>>i) &1U;

		//1鎷変綆tck,杈撳嚭tms鍜宼di
		GPIO0_OUT_DATA = (0 << PIN_TCK) | (tms_bit << PIN_TMS) | (tdi_bit << PIN_TDI);
		jtag_delay_loop();
		//2鎷夐珮tck,淇濇寔tms鍜宼di
		GPIO0_OUT_DATA = (1 << PIN_TCK) | (tms_bit << PIN_TMS) | (tdi_bit << PIN_TDI);
		jtag_delay_loop();
		//3tck楂樼數骞充腑闂撮噰鏍稵DO
		uint8_t tdo_bit = (GPIO1_IN_DATA >> PIN_TDO) & 0x01U;

		tdo |= (tdo_bit << i);

		//1鎷変綆tck,杈撳嚭tms鍜宼di
		GPIO0_OUT_DATA = (0 << PIN_TCK) | (tms_bit << PIN_TMS) | (tdi_bit << PIN_TDI);
		jtag_delay_loop();

	}
	return tdo;
}


uint32_t jtag_xfer2(uint32_t bits,uint32_t tms, uint32_t tdi) {

	uint32_t tdo = 0;
	for(uint32_t i = 0; i<bits; i++){

		uint8_t tdi_bit = (tdi>>i) &1U;
		uint8_t tms_bit = (tms>>i) &1U;

		//1鎷変綆tck,杈撳嚭tms鍜宼di
		GPIO2_OUT_DATA = (0 << PIN_TCK) | (tms_bit << PIN_TMS) | (tdi_bit << PIN_TDI);
		jtag_delay_loop();
		//2鎷夐珮tck,淇濇寔tms鍜宼di
		GPIO2_OUT_DATA = (1 << PIN_TCK) | (tms_bit << PIN_TMS) | (tdi_bit << PIN_TDI);
		jtag_delay_loop();
		//3tck楂樼數骞充腑闂撮噰鏍稵DO
		uint8_t tdo_bit = (GPIO3_IN_DATA >> PIN_TDO) & 0x01U;

		tdo |= (tdo_bit << i);

		//1鎷変綆tck,杈撳嚭tms鍜宼di
		GPIO2_OUT_DATA = (0 << PIN_TCK) | (tms_bit << PIN_TMS) | (tdi_bit << PIN_TDI);
		jtag_delay_loop();

	}
	return tdo;
}


uint32_t jtag_xfer3(uint32_t bits,uint32_t tms, uint32_t tdi) {

	uint32_t tdo = 0;
	for(uint32_t i = 0; i<bits; i++){

		uint8_t tdi_bit = (tdi>>i) &1U;
		uint8_t tms_bit = (tms>>i) &1U;

		//1鎷変綆tck,杈撳嚭tms鍜宼di
		GPIO4_OUT_DATA = (0 << PIN_TCK) | (tms_bit << PIN_TMS) | (tdi_bit << PIN_TDI);
		jtag_delay_loop();
		//2鎷夐珮tck,淇濇寔tms鍜宼di
		GPIO4_OUT_DATA = (1 << PIN_TCK) | (tms_bit << PIN_TMS) | (tdi_bit << PIN_TDI);
		jtag_delay_loop();
		//3tck楂樼數骞充腑闂撮噰鏍稵DO
		uint8_t tdo_bit = (GPIO5_IN_DATA >> PIN_TDO) & 0x01U;

		tdo |= (tdo_bit << i);

		//1鎷変綆tck,杈撳嚭tms鍜宼di
		GPIO4_OUT_DATA = (0 << PIN_TCK) | (tms_bit << PIN_TMS) | (tdi_bit << PIN_TDI);
		jtag_delay_loop();

	}
	return tdo;
}



static void jtag_tap_reset1(void){

	jtag_xfer1(10,0x3FFU, 0x00U);//10娑撶寘MS鏉╃偟鐢绘妯兼暩楠烇拷,婢跺秳缍�
	for(volatile int i =0;i<20; i++);
	jtag_xfer1(1,0x00U, 0x00U);
}

static void jtag_tap_reset2(void){

	jtag_xfer2(10,0x3FFU, 0x00U);//10娑撶寘MS鏉╃偟鐢绘妯兼暩楠烇拷,婢跺秳缍�
	for(volatile int i =0;i<20; i++);
	jtag_xfer2(1,0x00U, 0x00U);
}

static void jtag_tap_reset3(void){

	jtag_xfer3(10,0x3FFU, 0x00U);//10娑撶寘MS鏉╃偟鐢绘妯兼暩楠烇拷,婢跺秳缍�
	for(volatile int i =0;i<20; i++);
	jtag_xfer3(1,0x00U, 0x00U);
}


uint32_t jtag_read_idcode_test1(void){

	jtag_tap_reset1();

	jtag_xfer1(1,0x01U, 0x00U);//閸掑洦宕叉潻娑樺弳閺佺増宓侀幍顐ｅ伎

	jtag_xfer1(1,0x00U, 0x00U);//閸掑洦宕叉潻娑樺弳閺佺増宓侀幍顐ｅ伎

	jtag_xfer1(1,0x00U, 0x00U);//閸掑洦宕叉潻娑樺弳閺佺増宓侀幍顐ｅ伎

	uint32_t dev_id = jtag_xfer1(32,0x80000000U, 0x00U);//閼惧嘲褰�32娴ｅ不D

	jtag_xfer1(1,0x01U, 0x00U);

	jtag_xfer1(1,0x00U, 0x00U);//鏉╂柨娲栫粚娲＝

	return dev_id;
}

uint32_t jtag_read_idcode_test2(void){

	jtag_tap_reset2();

	jtag_xfer2(1,0x01U, 0x00U);//閸掑洦宕叉潻娑樺弳閺佺増宓侀幍顐ｅ伎

	jtag_xfer2(1,0x00U, 0x00U);//閸掑洦宕叉潻娑樺弳閺佺増宓侀幍顐ｅ伎

	jtag_xfer2(1,0x00U, 0x00U);//閸掑洦宕叉潻娑樺弳閺佺増宓侀幍顐ｅ伎

	uint32_t dev_id = jtag_xfer2(32,0x80000000U, 0x00U);//閼惧嘲褰�32娴ｅ不D

	jtag_xfer2(1,0x01U, 0x00U);

	jtag_xfer2(1,0x00U, 0x00U);//鏉╂柨娲栫粚娲＝

	return dev_id;
}



uint32_t jtag_read_idcode_test3(void){

	jtag_tap_reset3();

	jtag_xfer3(1,0x01U, 0x00U);//閸掑洦宕叉潻娑樺弳閺佺増宓侀幍顐ｅ伎

	jtag_xfer3(1,0x00U, 0x00U);//閸掑洦宕叉潻娑樺弳閺佺増宓侀幍顐ｅ伎

	jtag_xfer3(1,0x00U, 0x00U);//閸掑洦宕叉潻娑樺弳閺佺増宓侀幍顐ｅ伎

	uint32_t dev_id = jtag_xfer3(32,0x80000000U, 0x00U);//閼惧嘲褰�32娴ｅ不D

	jtag_xfer3(1,0x01U, 0x00U);

	jtag_xfer3(1,0x00U, 0x00U);//鏉╂柨娲栫粚娲＝

	return dev_id;
}



