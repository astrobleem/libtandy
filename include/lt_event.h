/* SPDX-License-Identifier: MIT */
#ifndef LT_EVENT_H
#define LT_EVENT_H

/* Original bounded foreground event queue. C89, caller-owned storage. */
#include <limits.h>
#if CHAR_BIT != 8 || UCHAR_MAX != 255 || USHRT_MAX != 65535U || \
    ULONG_MAX < 0xFFFFFFFFUL
#error libtandy events require 8-bit char, 16-bit short, and 32-bit long
#endif

#if defined(__WATCOMC__)
#define LTE_FAR __far
#else
#define LTE_FAR
#endif

typedef unsigned char lte_u8;
typedef unsigned short lte_u16;
typedef signed short lte_i16;

typedef enum {
    LTE_OK = 0,
    LTE_FULL = 1,
    LTE_EMPTY = 2,
    LTE_INVALID = 3
} lte_status;

#define LTE_EVENT_KEY_PRESS 1U
#define LTE_EVENT_MOUSE_STATE 2U
#define LTE_EVENT_USER 128U
#define LTE_EVENT_CAPACITY_MAX 64U

typedef struct {
    lte_u8 type;
    lte_u8 flags;
    lte_u16 code;
    lte_u16 data;
    lte_i16 x;
    lte_i16 y;
    lte_u16 tick_lo;
    lte_u16 tick_hi;
} lte_event;

typedef struct {
    lte_event LTE_FAR *events;
    lte_u8 capacity;
    lte_u8 head;
    lte_u8 count;
    lte_u16 dropped;
} lte_event_queue;

/* The queue object, backing array, and push/pop event records must be
 * separate non-overlapping caller-owned objects. This is foreground-only;
 * producers and consumers must not call concurrently or from an ISR. */
lte_status lte_event_queue_init(lte_event_queue *queue,
                                lte_event LTE_FAR *storage,
                                lte_u8 capacity);
lte_status lte_event_push(lte_event_queue *queue, const lte_event *event);
/* Empty pop returns LTE_EMPTY and leaves *event byte-for-byte unchanged. */
lte_status lte_event_pop(lte_event_queue *queue, lte_event *event);

#endif
