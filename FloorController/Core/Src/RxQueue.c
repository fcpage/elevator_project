/**
******************************************************************************** 
* @file     RxQueue.c
* @brief    Rx Queue implementation
* By        Nigel Sinclair & Ryan Pratt
******************************************************************************** 
*/

#include "RxQueue.h"

enum {
    CAN_RX_QUEUE_CAPACITY = 8,
    CAN_RX_QUEUE_STORAGE_SIZE = CAN_RX_QUEUE_CAPACITY + 1,
};

struct RxQueue {
    const u8 capacity;
    const u8 storageSize;
    CanRxFrame* data __attribute__((counted_by(storageSize)));
    volatile u8 head;
    volatile u8 tail;
    volatile u32 droppedFrameCount;
};

void initRxQueue(RxQueue* queue, u8 capacity) {
    *queue = (RxQueue){
        .capacity = capacity,
        .storageSize = capacity + 1,
        .data = NULL,
        .droppedFrameCount = 0,
        .head = 0,
        .tail = 0,
    };
}

u8 advanceRxQueueIndex(const u8 index)
{
    return (u8)((index + 1U) % CAN_RX_QUEUE_STORAGE_SIZE);
}

bool tryPop(RxQueue* q, CanRxFrame* frame)
{
    const u8 tail = q->tail;
    if (tail == q->head)
    {
        return 0U;
    }

    __DMB();
    *frame = q->data[tail];
    __DMB();
    q->tail = advanceRxQueueIndex(tail);
    return 1U;
}

