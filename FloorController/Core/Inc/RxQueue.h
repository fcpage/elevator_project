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

RxQueue* initRxQueue();
bool rxQueueTryPush(RxQueue* queue, CanRxFrame frame);
bool rxQueueTryPop(RxQueue* queue, CanRxFrame* frameOut);
u8   rxQueueGetNumberOfElements(RxQueue* queue);
bool rxQueueHasFrame(RxQueue* queue);
bool rxQueueGetDroppedFrameCount(RxQueue* queue);
bool rxQueueHasError();

/*** RxQueue Internal data initialization ***/

#if defined(__RX_QUEUE_C)

extern CanRxFrame* __rxQueueDataPtr;
extern const u8 __rxQueueCapacity;

#elif defined(RX_QUEUE_CAPACITY)

static CanRxFrame __rxQueueData[RX_QUEUE_CAPACITY] = {0};
const u8 __rxQueueCapacity = RX_QUEUE_CAPACITY;
CanRxFrame* __rxQueueDataPtr = __rxQueueData;

#else
#error "Rx Queue Capacity must be predefined"
#endif 

#endif
