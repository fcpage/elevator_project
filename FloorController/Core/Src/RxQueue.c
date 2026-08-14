/**
******************************************************************************** 
* @file     RxQueue.c
* @brief    Rx Queue implementation
* By        Nigel Sinclair & Ryan Pratt
******************************************************************************** 
*/

#include <assert.h>
#define __RX_QUEUE_C
#include "RxQueue.h"

#define RX_QUEUE_ASSERT_INIT \
    do { \
        assert(queue != 0); \
        assert(queue->capacity != 0); \
        assert(queue->storageSize != 0); \
        assert(queue->data != NULL); \
    } while(0);

struct RxQueue {
    u8 capacity;
    u8 storageSize;
    CanRxFrame* data __attribute__((counted_by(storageSize)));
    volatile u8 head;
    volatile u8 tail;
    volatile u32 droppedFrameCount;
};

static u8 advanceRxQueueIndex(RxQueue* queue, const u8 index)
{
    RX_QUEUE_ASSERT_INIT;

    return (u8)((index + 1U) % queue->storageSize);
}

void initRxQueue(RxQueue* queue) {
    *queue = (RxQueue){
        .capacity = __rxQueueCapacity,
        .storageSize = __rxQueueCapacity + 1,
        .data = __rxQueueDataPtr,
        .droppedFrameCount = 0,
        .head = 0,
        .tail = 0,
    };
}

bool rxQueueTryPush(RxQueue* queue, CanRxFrame frame)
{
    const u8 tail = queue->tail;
    if (tail == queue->head)
    {
        return false;
    }

    __DMB();
    *frame = queue->data[tail];
    __DMB();
    queue->tail = advanceRxQueueIndex(queue, tail);
    return true;
}

bool rxQueueTryPop(RxQueue* queue, CanRxFrame* frame)
{
    const u8 tail = queue->tail;
    if (tail == queue->head)
    {
        return false;
    }

    __DMB();
    *frame = queue->data[tail];
    __DMB();
    queue->tail = advanceRxQueueIndex(queue, tail);
    return true;
}

u8 rxQueueGetSize(RxQueue* queue)
{
    RX_QUEUE_ASSERT_INIT;

    return queue->capacity;
}

