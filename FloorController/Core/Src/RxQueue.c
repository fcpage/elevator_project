/**
******************************************************************************** 
* @file     RxQueue.c
* @brief    Rx Queue implementation
* By        Nigel Sinclair & Ryan Pratt
******************************************************************************** 
*/

#include <assert.h>
#define __RX_QUEUE_C
#include "basic_defs.h"
#include "RxQueue.h"

struct RxQueue {
    u8 capacity;
    u8 head;
    volatile u8 tail;
    volatile u32 droppedFrameCount;
    CanRxFrame* data ;
};

static bool hasError = false;
static RxQueue rxQueue = {0};

static inline bool rxQueueIsFull(RxQueue* queue)
{
    if(queue == 0 
    || queue->capacity == 0 
    || queue->data == NULL
    ) return (hasError = true);

    return queue->tail >= queue->capacity;
}

static inline bool rxQueueIsEmpty(RxQueue* queue)
{
    if(queue == 0 
    || queue->capacity == 0 
    || queue->data == NULL
    ) return (hasError = true);

    return queue->tail == queue->head;
}

bool rxQueueHasFrame(RxQueue* queue) {
    return queue->tail > 0;
}

bool rxQueueGetDroppedFrameCount(RxQueue* queue) {
    return queue->droppedFrameCount;
}

bool rxQueueHasError() { return hasError; }

RxQueue* initRxQueue() {
    rxQueue = (RxQueue){
        .capacity = __rxQueueCapacity,
        .data = __rxQueueDataPtr,
        .droppedFrameCount = 0,
        .head = 0,
        .tail = 0,
    };
    return &rxQueue;
}

bool rxQueueTryPush(RxQueue* queue, CanRxFrame frame)
{
    assert(queue != NULL);
    assert(queue->capacity != 0);
    assert(queue->data != NULL);

    if (rxQueueIsFull(queue))
    {
        ++queue->droppedFrameCount;
        return false;
    }

    queue->data[queue->tail] = frame;
    __DMB();
    queue->tail++;

    return true;
}

bool rxQueueTryPop(RxQueue* queue, CanRxFrame* frame)
{
    assert(queue != NULL);
    assert(queue->capacity != 0);
    assert(queue->data != NULL);

    if (rxQueueIsEmpty(queue))
    {
        return false;
    }

    __DMB();
    *frame = queue->data[queue->tail];
    __DMB();
    queue->tail--;
    return true;
}

u8 rxQueueGetNumberOfElements(RxQueue* queue)
{
    if(queue == 0 
    || queue->capacity == 0 
    || queue->data == NULL
    ) {
        hasError = true;
        return 0;
    }

    return queue->tail;
}

