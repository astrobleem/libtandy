/* SPDX-License-Identifier: MIT */
#include "lt_event.h"

#if defined(__WATCOMC__)
#include <i86.h>
#endif

static int lte_event_storage_valid(lte_event LTE_FAR *events,
                                   lte_u8 capacity)
{
    if (events == 0 || capacity == 0U ||
        capacity > LTE_EVENT_CAPACITY_MAX) return 0;
#if defined(__WATCOMC__)
    if ((unsigned long)FP_OFF(events) +
        (unsigned long)capacity * (unsigned long)sizeof(lte_event) >
        65536UL) return 0;
#endif
    return 1;
}

static int lte_event_queue_valid(const lte_event_queue *queue)
{
    return queue != 0 &&
           lte_event_storage_valid(queue->events, queue->capacity) &&
           queue->head < queue->capacity && queue->count <= queue->capacity;
}

static int lte_event_type_valid(lte_u8 type)
{
    return type == LTE_EVENT_KEY_PRESS || type == LTE_EVENT_MOUSE_STATE ||
           type >= LTE_EVENT_USER;
}

lte_status lte_event_queue_init(lte_event_queue *queue,
                                lte_event LTE_FAR *storage,
                                lte_u8 capacity)
{
    lte_event_queue next;
    if (queue == 0 || !lte_event_storage_valid(storage, capacity))
        return LTE_INVALID;
    next.events = storage;
    next.capacity = capacity;
    next.head = 0;
    next.count = 0;
    next.dropped = 0;
    *queue = next;
    return LTE_OK;
}

lte_status lte_event_push(lte_event_queue *queue, const lte_event *event)
{
    lte_u8 index;
    if (!lte_event_queue_valid(queue) || event == 0 ||
        !lte_event_type_valid(event->type)) return LTE_INVALID;
    if (queue->count == queue->capacity) {
        if (queue->dropped != 65535U) ++queue->dropped;
        return LTE_FULL;
    }
    index = (lte_u8)(queue->head + queue->count);
    if (index >= queue->capacity) index = (lte_u8)(index - queue->capacity);
    queue->events[index] = *event;
    ++queue->count;
    return LTE_OK;
}

lte_status lte_event_pop(lte_event_queue *queue, lte_event *event)
{
    if (!lte_event_queue_valid(queue) || event == 0) return LTE_INVALID;
    if (queue->count == 0U) return LTE_EMPTY;
    *event = queue->events[queue->head];
    ++queue->head;
    if (queue->head == queue->capacity) queue->head = 0;
    --queue->count;
    return LTE_OK;
}
