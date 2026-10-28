/*
 * Queue.h
 *
 *  Created on: 2019年4月8日
 *      Author: WangFule
 */

#ifndef SRC_QUEUE_H_
#define SRC_QUEUE_H_

#include "lwip/tcp.h"

typedef struct DataP
{
	unsigned int id;
	void *payload;
	unsigned short len;
	struct tcp_pcb *pcb;//绑定对应的pcb
	uint8_t active_chan;
} DP;

typedef  DP QueueElementDataType;

typedef struct Queue
{
	QueueElementDataType * array;
    int front;
    int tail;
}QUEUE,*PQUEUE;

void Init_queue(PQUEUE);
void In_queue(PQUEUE,QueueElementDataType val);
void Out_queue(PQUEUE,QueueElementDataType* val);
void Traverse_queue(PQUEUE);
int Isempty(PQUEUE);
int Isfull(PQUEUE);


#endif /* SRC_QUEUE_H_ */
