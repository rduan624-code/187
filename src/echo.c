/******************************************************************************
 *
 * Copyright (C) 2009 - 2014 Xilinx, Inc.  All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * Use of the Software is limited solely to applications:
 * (a) running on a Xilinx device, or
 * (b) that interact with a Xilinx device through a bus or interconnect.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * XILINX  BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF
 * OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * Except as contained in this notice, the name of the Xilinx shall not be used
 * in advertising or otherwise to promote the sale, use or other dealings in
 * this Software without prior written authorization from Xilinx.
 *
 ******************************************************************************/

#include <stdio.h>
#include <string.h>

#include "lwip/err.h"
#include "lwip/tcp.h"
#if defined (__arm__) || defined (__aarch64__)
#include "xil_printf.h"
#endif

#include "Xil_io.h"

#include "Queue.h"
#include "jtag_gpio.h"

//#define XVC_DEBUG
QUEUE recvhead;
//Deque sndhead = {NULL, NULL};

uint8_t reply_buf[4096];
uint8_t cmd_buf[64];//1024情况下“：”分割在同一个包中，uint8_t cmd_buf[32];2048情况子啊需要增加位64
uint8_t data_buf[4096];
uint8_t ln_buf[4];
uint32_t reply_len;
uint32_t cmd_len;
uint32_t data_len;
uint32_t ln_len;
unsigned long nsperiod = 0;//固定周期500,与delay 17搭配2MHz
#define MAX_PACKET_LEN 4096
#ifndef XVC_VERSION
#define XVC_VERSION 10
#endif

volatile uint8_t g_active_chan = CHAN_IDLE;
struct tcp_pcb *g_active_pcb =NULL;

static err_t xvc_start(DP *recv_dp);

int Que_Init() {
//	Init_queue(&sndhead);
	Init_queue(&recvhead);
	return 0;
}

int transfer_data() {
	static unsigned int prev_id = 0;
	err_t err;
//	if(recvhead.front != NULL)
//	{
	   // if(!Isempty(&recvhead))
		while(!Isempty(&recvhead))//由原来的一次处理一条消息，变为处理完队里的所有消息
		{
			DP dp;
			Out_queue(&recvhead, &dp);
			err = xvc_start(&dp);
			free(dp.payload);
			if(err!=0)
			{

			}
			if(prev_id + 1 != dp.id)
				xil_printf("Lost %d, packet\n\r", dp.id - prev_id);
			else {
//				xil_printf("Packet %d:", prev_id);
				prev_id = dp.id;
			}
		}
//	}
	return 0;
}

void print_app_header() {
#if (LWIP_IPV6==0)
	xil_printf("\n\r\n\r-----lwIP TCP echo server ------\n\r");
#else
	xil_printf("\n\r\n\r-----lwIPv6 TCP echo server ------\n\r");
#endif
	xil_printf("TCP packets sent to port 6001 will be echoed back\n\r");
}

//err_t recv_callback(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err) {
//	/* do not read the packet if we are not in ESTABLISHED state */
//	static unsigned int id = 0;
//	if (!p || err != ERR_OK) {
//		tcp_close(tpcb);
//		tcp_recv(tpcb, NULL);
//		g_active_chan =CHAN_IDLE;
//		g_active_pcb =NULL;
//		return ERR_CLSD;
//	}
//
//	/* indicate that the packet has been received */
//	tcp_recved(tpcb, p->len);
////	xil_printf("cmd:%s\n\r", p->payload);
//	/* echo back the payload */
//	/* in this case, we assume that the payload is < TCP_SND_BUF */
//	if (tcp_sndbuf(tpcb) > p->len) {
////		if(sndhead.front == NULL)
////			Init_queue(&sndhead);
////		else if(recvhead.front == NULL)
////			Init_queue(&recvhead);
////		else
////		{
//			unsigned char *temp;
//			temp = (unsigned char *)malloc(sizeof(unsigned char)*(p->len+1));
////			memset(temp, 0, p->len+1);
//			if(temp == NULL)
//			{
//				xil_printf("Dynamic memory allocation failed!\n");
//				exit(0);
//			}
//			for(int i=0; i<p->len; i++)
//				temp[i]=*((unsigned char *)p->payload+i);
//			temp[p->len+1]=0;
////			memcpy(temp, p->payload, p->len);
//			id ++;
//			DP dp = {id, temp, p->len, tpcb, g_active_chan};
//			In_queue(&recvhead, dp);
//
////		}
////		xvc_start(tpcb, p->payload, p->len, err);
////		err = tcp_write(tpcb, p->payload, p->len, 1);
//	} else
//		xil_printf("no space in tcp_sndbuf\n\r");
//
//	/* free the received pbuf */
//	pbuf_free(p);
//
//	return ERR_OK;
//}



//重写接受回调的拼包
static uint8_t rx_buf[4096];
static uint32_t rx_len = 0;
err_t recv_callback(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err) {
	/* do not read the packet if we are not in ESTABLISHED state */
	static unsigned int id = 0;
	if (!p|| err != ERR_OK) {
		tcp_close(tpcb);
		tcp_recv(tpcb, NULL);
		rx_len = 0;
        g_active_chan =CHAN_IDLE;
        g_active_pcb =NULL;
		while(!Isempty(&recvhead))
		{
			DP dp;
			Out_queue(&recvhead, &dp);
			if(dp.payload !=NULL) free(dp.payload);
		}
		return ERR_CLSD;
	}

	tcp_recved(tpcb, p->len);
    if(rx_len + p->len <= sizeof(rx_buf)){
    	memcpy(rx_buf + rx_len, p->payload ,p->len);
    	rx_len += p->len;
    }else{
    	rx_len = 0;
    }

    while(1){
    	//找命令结束符号:
    	int colon = -1;
    	for(int i = 0; i<rx_len;i++){
    		if(rx_buf[i] == ':'){
    			colon = i;
    			break;
    		}
    	}
		if(colon == -1) break;//没有完整命令，退出等下一个包

		uint32_t total_len = 0;
		if(memcmp(rx_buf, "getinfo:", 8) == 0){
			total_len = 8;
		}else if(memcmp(rx_buf, "settck:", 7) == 0){
			total_len = 7+4;
		}else if(memcmp(rx_buf, "shift:", 6) == 0){
			if(rx_len < 6 + 4) break;
			uint32_t bits = *(uint32_t*)(rx_buf + 6);
			uint32_t bytes = (bits + 7)/8;
			total_len = 6+4+2*bytes; 	//shfit命令：6字节头+4字节长度+2*bytes数据
		}
		if(total_len == 0 || rx_len < total_len) break; //缓冲区数据不足，退出等下一包
		//完整命令入队：
		//if (tcp_sndbuf(tpcb) > total_len && !Isfull(&recvhead)) {
		if (!Isfull(&recvhead)) {
				unsigned char *temp;
				temp = (unsigned char *)malloc(total_len+1);
				if(temp == NULL)
				{
					xil_printf("Dynamic memory allocation failed!\n");
					exit(0);
				}
				for(int i=0; i<total_len; i++){
					//temp[i]=*((unsigned char *)p->payload+i);
					temp[i]=rx_buf[i];
				}
			    temp[total_len]=0;
				id ++;
				DP dp = {id, temp, total_len, tpcb, g_active_chan};
				In_queue(&recvhead, dp);
		}
		memmove(rx_buf,rx_buf + total_len,rx_len -total_len);		//一处已经处理的命令，剩余数据移动到缓冲区开头
		rx_len -= total_len;
	}
	/* free the received pbuf */
	pbuf_free(p);

	return ERR_OK;
}


//默认IDLE,连接冲突检测
static err_t check_connection_lock(struct tcp_pcb *newpcb){
	
	if(g_active_pcb != NULL){
		tcp_abort(newpcb);
		return ERR_CONN;
	}
	g_active_pcb = newpcb;
	return ERR_OK;
}



err_t accept_callback_ch1(void *arg, struct tcp_pcb *newpcb, err_t err) {
	
    if(check_connection_lock(newpcb) != ERR_OK) return ERR_OK;
	
	g_active_chan = CHAN_EXT1;
	/* just use an integer number indicating the connection id as the
	 callback argument */
	tcp_arg(newpcb, newpcb);

	/* set the receive callback for this connection */
	tcp_recv(newpcb, recv_callback);

	return ERR_OK;
}

err_t accept_callback_ch2(void *arg, struct tcp_pcb *newpcb, err_t err) {
	
    if(check_connection_lock(newpcb) != ERR_OK) return ERR_OK;
	
	g_active_chan = CHAN_EXT2;
	/* just use an integer number indicating the connection id as the
	 callback argument */
	tcp_arg(newpcb, newpcb);

	/* set the receive callback for this connection */
	tcp_recv(newpcb, recv_callback);

	return ERR_OK;
}

err_t accept_callback_ch3(void *arg, struct tcp_pcb *newpcb, err_t err) {
	
    if(check_connection_lock(newpcb) != ERR_OK) return ERR_OK;

	g_active_chan = CHAN_EXT3;
	/* just use an integer number indicating the connection id as the
	 callback argument */
	tcp_arg(newpcb, newpcb);

	/* set the receive callback for this connection */
	tcp_recv(newpcb, recv_callback);

	return ERR_OK;
}

err_t accept_callback_self(void *arg, struct tcp_pcb *newpcb, err_t err) {
	
    if(check_connection_lock(newpcb) != ERR_OK) return ERR_OK;

	g_active_chan = CHAN_SELF;
	/* just use an integer number indicating the connection id as the
	 callback argument */
	tcp_arg(newpcb, newpcb);

	/* set the receive callback for this connection */
	tcp_recv(newpcb, recv_callback);

	return ERR_OK;
}



int start_application_ch1() {
	struct tcp_pcb *pcb1;
	err_t err;
	unsigned port = 2542;

	/* create new TCP PCB structure */
	//pcb = tcp_new_ip_type(IPADDR_TYPE_ANY);
	pcb1 = tcp_new();
	if (!pcb1) {
		xil_printf("Error creating PCB1. Out of Memory\n\r");
		return -1;
	}

	/* bind to specified @port */
	//err = tcp_bind(pcb, IP_ANY_TYPE, port);
	err = tcp_bind(pcb1, IP_ADDR_ANY, port);
	if (err != ERR_OK) {
		xil_printf("Unable to bind to port %d: err = %d\n\r", port, err);
		return -2;
	}

	/* we do not need any arguments to callback functions */
	tcp_arg(pcb1, pcb1);

	/* listen for connections */
	pcb1 = tcp_listen(pcb1);
	if (!(pcb1)) {
		xil_printf("Out of memory while tcp1_listen\n\r");
		return -3;
	}
	pcb1->flags = TF_NODELAY;
	/* specify callback to use for incoming connections */
	tcp_accept(pcb1, accept_callback_ch1);
    xil_printf("TCP1 echo server started @ port %d\n\r", port);
	return 0;
}


int start_application_ch2() {
	struct tcp_pcb *pcb2;
	err_t err;
	unsigned port = 2543;

	/* create new TCP PCB structure */
	//pcb = tcp_new_ip_type(IPADDR_TYPE_ANY);
	pcb2 = tcp_new();
	if (!pcb2) {
		xil_printf("Error creating PCB2. Out of Memory\n\r");
		return -1;
	}

	/* bind to specified @port */
	//err = tcp_bind(pcb, IP_ANY_TYPE, port);
	err = tcp_bind(pcb2, IP_ADDR_ANY, port);
	if (err != ERR_OK) {
		xil_printf("Unable to bind to port %d: err = %d\n\r", port, err);
		return -2;
	}

	/* we do not need any arguments to callback functions */
	tcp_arg(pcb2, pcb2);

	/* listen for connections */
	pcb2 = tcp_listen(pcb2);
	if (!(pcb2)) {
		xil_printf("Out of memory while tcp2_listen\n\r");
		return -3;
	}
	pcb2->flags = TF_NODELAY;
	/* specify callback to use for incoming connections */
	tcp_accept(pcb2, accept_callback_ch2);
    xil_printf("TCP2 echo server started @ port %d\n\r", port);
	return 0;
}


int start_application_ch3() {
	struct tcp_pcb *pcb3;
	err_t err;
	unsigned port = 2544;

	/* create new TCP PCB structure */
	//pcb = tcp_new_ip_type(IPADDR_TYPE_ANY);
	pcb3 = tcp_new();
	if (!pcb3) {
		xil_printf("Error creating PCB3. Out of Memory\n\r");
		return -1;
	}

	/* bind to specified @port */
	//err = tcp_bind(pcb, IP_ANY_TYPE, port);
	err = tcp_bind(pcb3, IP_ADDR_ANY, port);
	if (err != ERR_OK) {
		xil_printf("Unable to bind to port %d: err = %d\n\r", port, err);
		return -2;
	}

	/* we do not need any arguments to callback functions */
	tcp_arg(pcb3, pcb3);

	/* listen for connections */
	pcb3 = tcp_listen(pcb3);
	if (!(pcb3)) {
		xil_printf("Out of memory while tcp3_listen\n\r");
		return -3;
	}
	pcb3->flags = TF_NODELAY;
	/* specify callback to use for incoming connections */
	tcp_accept(pcb3, accept_callback_ch3);
    xil_printf("TCP3 echo server started @ port %d\n\r", port);
	return 0;
}


int start_application_self() {
	struct tcp_pcb *pcb4;
	err_t err;
	unsigned port = 2545;

	/* create new TCP PCB structure */
	//pcb = tcp_new_ip_type(IPADDR_TYPE_ANY);
	pcb4 = tcp_new();
	if (!pcb4) {
		xil_printf("Error creating PCB4. Out of Memory\n\r");
		return -1;
	}

	/* bind to specified @port */
	//err = tcp_bind(pcb, IP_ANY_TYPE, port);
	err = tcp_bind(pcb4, IP_ADDR_ANY, port);
	if (err != ERR_OK) {
		xil_printf("Unable to bind to port %d: err = %d\n\r", port, err);
		return -2;
	}

	/* we do not need any arguments to callback functions */
	tcp_arg(pcb4, pcb4);

	/* listen for connections */
	pcb4 = tcp_listen(pcb4);
	if (!(pcb4)) {
		xil_printf("Out of memory while tcp4_listen\n\r");
		return -3;
	}
	pcb4->flags = TF_NODELAY;
	/* specify callback to use for incoming connections */
	tcp_accept(pcb4, accept_callback_self);
    xil_printf("TCP4 echo server started @ port %d\n\r", port);
	return 0;
}


static unsigned get_uint_le(void * buf, int len) {
	uint8_t * p = (u8_t *) buf;
	unsigned value = 0;

	while (len-- > 0) {
		value = (value << 8) | p[len];
	}
	return value;
}

static void set_uint_le(void * buf, int len, unsigned value) {
	uint8_t * p = (uint8_t *) buf;

	while (len-- > 0) {
		*p++ = (uint8_t) value;
		value >>= 8;
	}
}

static void shift_tms_tdi(uint32_t len, uint8_t* inbuf, uint8_t *result, uint8_t active_chan) {
	register int nr_bytes = (len + 7) / 8;
	register int bytesLeft = nr_bytes;
	register int bitsLeft = len;
	register int byteIndex = 0;
	uint32_t tdi, tms, tdo;
	memset(result, 0, nr_bytes);

	while (bytesLeft > 0) {
		tms = 0;
		tdi = 0;
		tdo = 0;
		if(g_active_chan != active_chan){
			memset(result, 0, nr_bytes);
			return;
		}
		if (bytesLeft >= 4) {
			memcpy(&tms, &inbuf[byteIndex], 4);
			memcpy(&tdi, &inbuf[byteIndex + nr_bytes], 4);

			if(active_chan == CHAN_EXT1){
				tdo = jtag_xfer1(32,tms,tdi);
			}
			if(active_chan == CHAN_EXT2){
				tdo = jtag_xfer2(32,tms,tdi);
			}
			if(active_chan == CHAN_EXT3){
				tdo = jtag_xfer3(32,tms,tdi);
			}
			if(active_chan == CHAN_SELF){
				tdo = DB_JtagShift(32,tms,tdi);
			}

			memcpy(&result[byteIndex], &tdo, 4);
			bytesLeft -= 4;
			bitsLeft -= 32;
			byteIndex += 4;
#ifdef XVC_DEBUG
			xil_printf("bitsLeft : %d", bitsLeft);
			xil_printf("LEN : 0x%02x", 32);
			xil_printf("TMS : 0x%08x", tms);
			xil_printf("TDI : 0x%08x", tdi);
			xil_printf("TDO : 0x%08x", tdo);
			xil_printf("\n\r");
#endif
		} else {
			memcpy(&tms, &inbuf[byteIndex], bytesLeft);
			memcpy(&tdi, &inbuf[byteIndex + nr_bytes], bytesLeft);

			if(active_chan == CHAN_EXT1){
				tdo = jtag_xfer1(bitsLeft,tms,tdi);
			}
			if(active_chan == CHAN_EXT2){
				tdo = jtag_xfer2(bitsLeft,tms,tdi);
			}
			if(active_chan == CHAN_EXT3){
				tdo = jtag_xfer3(bitsLeft,tms,tdi);
			}
			if(active_chan == CHAN_SELF){
				tdo = DB_JtagShift(bitsLeft,tms,tdi);
			}

			memcpy(&result[byteIndex], &tdo, bytesLeft);
#ifdef XVC_DEBUG
			xil_printf("bitsLeft : %d", bitsLeft);
			xil_printf("LEN : 0x%02x", bitsLeft);
			xil_printf("TMS : 0x%08x", tms);
			xil_printf("TDI : 0x%08x", tdi);
			xil_printf("TDO : 0x%08x", tdo);
			xil_printf("\n\r");
#endif
			bytesLeft = 0;
			bitsLeft = 0;
		}
	}
}



////适配2048包大小的转换
//static void shift_tms_tdi(uint32_t len, uint8_t* inbuf, uint8_t *result) {
//	int nr_bytes = (len + 7) / 8;
//	//int bytesLeft = nr_bytes;
//	int total_bits = len;
//	int byteIndex = 0;
//	uint32_t tdi, tms, tdo;
//	memset(result, 0, nr_bytes);
//
//	while (total_bits > 0) {
//		tms = 0;
//		tdi = 0;
//		tdo = 0;
//		//每次最多处理32位
//		uint32_t bit_to_xfer = (total_bits > 32) ? 32 :total_bits;
//		uint32_t bytes_to_xfer = (bit_to_xfer + 7)/8;
//		tms = 0;
//		tdi = 0;
//
//		memcpy(&tms, &inbuf[byteIndex], bytes_to_xfer);
//		memcpy(&tdi, &inbuf[byteIndex + nr_bytes], bytes_to_xfer);
//
////		tdo = jtag_xfer(bit_to_xfer,tms,tdi);
//        if(active_chan == CHAN_EXT1){
//	        tdo = jtag_xfer1(bit_to_xfer,tms,tdi);
//        }
//        if(active_chan == CHAN_EXT2){
//        	tdo = jtag_xfer2(bit_to_xfer,tms,tdi);
//        }
//        if(active_chan == CHAN_EXT3){
//        	tdo = jtag_xfer3(bit_to_xfer,tms,tdi);
//        }
//        if(active_chan == CHAN_SELF){
//        	tdo = DB_JtagShift(bit_to_xfer,tms,tdi);
//        }
//
//		memcpy(&result[byteIndex], &tdo, bytes_to_xfer);
//		total_bits -= bit_to_xfer;
//		byteIndex  += bytes_to_xfer;
//	}
//}
//










static err_t xvc_start(DP *recv_dp) {
	err_t err;
	unsigned char *p;
	unsigned char *pe;
	int enable_status = 0;
	int pending_error[32] = { 0 };
	p = recv_dp->payload;
	pe = p+recv_dp->len;
    struct tcp_pcb *tpcb = recv_dp->pcb;
    uint8_t u_active_chan = recv_dp->active_chan;

read_more:
		for (int i = 0; i < pe-p; i++) {
	#ifdef XVC_DEBUG
			xil_printf("%c", *(p+i));
	#endif
			if ((*(p + i)) == ':') {
				cmd_buf[i] = *(p+i);
				cmd_len = i + 1;
				break;
			}
			cmd_buf[i] = *(p+i);
		}
		p = p + cmd_len;

	//业务逻辑
	if (cmd_len == 8 && memcmp(cmd_buf, "getinfo:", cmd_len) == 0) {
		snprintf((char *) reply_buf, 100, "xvcServer_v%u.%u:%u\n", XVC_VERSION / 10, XVC_VERSION % 10, MAX_PACKET_LEN);
		xil_printf("%s\n\r", reply_buf);
		reply_len = strlen((char *) reply_buf);
		goto reply;
	}
	if (cmd_len == 7 && memcmp(cmd_buf, "settck:", cmd_len) == 0) {
		unsigned long resnsperiod;
		if (nsperiod != 0)
			resnsperiod = nsperiod;
		else
			resnsperiod = get_uint_le(p, 4);
		// if (!pending_error[0])
		// set_tck(c->client_data, nsperiod, &resnsperiod);
		// if (pending_error[0])
		// resnsperiod = nsperiod;
		p+=4;
		set_uint_le(reply_buf, 4, resnsperiod);
		reply_len = 4;
		goto reply_with_optional_status;
	}
	if (cmd_len == 6 && memcmp(cmd_buf, "shift:", cmd_len) == 0) {
#ifdef XVC_COUNT_EN
		// gettimeofday(&start, NULL); //TODO:
#endif
		unsigned bits;
		unsigned bytes;
		bits = get_uint_le(p, 4);
		bytes = (bits + 7) / 8;
		p += 4;
#ifdef XVC_DEBUG
		xil_printf("\nBITS NUM: %d\n", bits);
#endif
		// assert(bytes * 2 <= max_packet_len);
		if (pe-p < 2 * bytes)
			return ERR_MEM;
		if (!pending_error[0]) {
			// fprintf(stdout, "bits received %d %d %x %x\n", bits, bytes, p[0], p[bytes]);
			shift_tms_tdi(bits, p, reply_buf,u_active_chan);
			p+=2*bytes;
		} else {
			xil_printf("XVC: Problem\n");
			memset(reply_buf, 0, bytes);
		}
		reply_len = bytes;

		goto reply_with_optional_status;
	}
	reply_with_optional_status: if (!enable_status)
		goto reply;
#if XVC_VERSION >= 11
	reply_with_status:
	reply_status(c);
#endif
	reply:
	// }
	if (cmd_len) {
		// if (c->handlers->flush)
		// if (c->handlers->flush(c->client_data) < 0) goto error;
		err = tcp_write(tpcb, (void *)reply_buf, reply_len, 1);
		if (err) {
			xil_printf("Reply Error: %d", err);
			return err;
		} else {
//			xil_printf("Send Packet!");
		}
		err = tcp_output(tpcb);
		if (err) {
			xil_printf("Force Reply Error: %d", err);
			return err;
		} else {
//			xil_printf("Send Packet!");
		}
	}

	cmd_len = 0;
	if(pe > p)
		goto read_more;
	//                        LDateTime.getRtc(&stop);
	// if (start.tv_usec != 0)
	//     printf("Shift send packet %lu u-seconds\n", stop.tv_usec - start.tv_usec);
	//          }
	return ERR_OK;
//	error:
////	      xil_printf("XVC connection terminated: error %d\n", errno);
//	xil_printf("Error");
//	cmd_len = 0;
//	return -1;
}

