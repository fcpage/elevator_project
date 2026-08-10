/**
******************************************************************************** 
* @file     RxQueue.h
* @brief    Rx Queue Interface
* By        Nigel Sinclair & Ryan Pratt
******************************************************************************** 
*/

#ifndef __RX_QUEUE_H
#define __RX_QUEUE_H

#include "basic_defs.h"
#include "CAN_protocol.h"
#include "stm32f3xx_hal.h" 

typedef struct {
    CAN_RxHeaderTypeDef header;
    u8                  data[CAN_PAYLOAD_LENGTH];
} CanRxFrame;

typedef struct RxQueue RxQueue;

void initRxQueue(RxQueue* queue);
u8 advanceRxQueueIndex(const u8 index);
bool tryPush(RxQueue* q, CanRxFrame* frame);
bool tryPop(RxQueue* q, CanRxFrame* frame);

#endif
