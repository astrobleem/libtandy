/* SPDX-License-Identifier: MIT */
#include <stdio.h>
#include <string.h>
#include "lt_event.h"
#if defined(__WATCOMC__)
#include <dos.h>
#include <i86.h>
#endif

static lte_event guarded[6];
static lte_event_queue queue;
static int failures;
typedef char lte_event_size_must_be_14[(sizeof(lte_event) == 14U) ? 1 : -1];
#if defined(__WATCOMC__)
typedef char lte_event_far_pointer_must_be_4[
    (sizeof(lte_event LTE_FAR *) == 4U) ? 1 : -1];
typedef char lte_event_near_pointer_must_be_2[
    (sizeof(lte_event *) == 2U) ? 1 : -1];
typedef char lte_queue_size_must_be_10[(sizeof(lte_event_queue) == 10U) ? 1 : -1];
#endif

#define CHECK(test) do { if (!(test)) { \
    printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #test); ++failures; \
} } while (0)

static void set_event(lte_event *e, lte_u8 type, lte_u16 code, lte_i16 x)
{
    e->type = type; e->flags = (lte_u8)(code >> 8); e->code = code;
    e->data = (lte_u16)(code ^ 0x55AAU); e->x = x; e->y = x;
    e->tick_lo = (lte_u16)(code + 123U);
    e->tick_hi = (lte_u16)(code ^ 0xA55AU);
}

int main(void)
{
    lte_event input, output, saved;
    lte_event_queue old_queue, saved_queue;
    unsigned int i;
    for (i = 0; i < 6U; ++i) memset(&guarded[i], (int)(0x31U + i), sizeof(lte_event));
    CHECK(lte_event_queue_init(&queue, guarded + 1, 2U) == LTE_OK);
    saved_queue = queue;
    CHECK(lte_event_queue_init(&queue, guarded + 1, 0U) == LTE_INVALID);
    CHECK(memcmp(&queue, &saved_queue, sizeof(queue)) == 0);

    set_event(&input, LTE_EVENT_KEY_PRESS, 0x1234U, -12);
    CHECK(lte_event_push(&queue, &input) == LTE_OK);
    set_event(&input, LTE_EVENT_MOUSE_STATE, 0x00A1U, 300);
    CHECK(lte_event_push(&queue, &input) == LTE_OK);
    CHECK(lte_event_push(&queue, &input) == LTE_FULL);
    CHECK(queue.dropped == 1U && queue.count == 2U);
    CHECK(lte_event_pop(&queue, &output) == LTE_OK);
    set_event(&saved, LTE_EVENT_KEY_PRESS, 0x1234U, -12);
    CHECK(memcmp(&output, &saved, sizeof(output)) == 0);
    set_event(&input, LTE_EVENT_USER, 0xBEEF, -32768);
    CHECK(lte_event_push(&queue, &input) == LTE_OK);
    CHECK(lte_event_pop(&queue, &output) == LTE_OK);
    set_event(&saved, LTE_EVENT_MOUSE_STATE, 0x00A1U, 300);
    CHECK(memcmp(&output, &saved, sizeof(output)) == 0);
    CHECK(lte_event_pop(&queue, &output) == LTE_OK);
    CHECK(memcmp(&output, &input, sizeof(output)) == 0);

    set_event(&output, LTE_EVENT_USER, 7U, 9);
    saved = output;
    CHECK(lte_event_pop(&queue, &output) == LTE_EMPTY);
    CHECK(memcmp(&output, &saved, sizeof(output)) == 0);
    CHECK(guarded[0].type == 0x31U && guarded[3].type == 0x34U);
    CHECK(guarded[4].type == 0x35U && guarded[5].type == 0x36U);

    /* A full queue rejects new records and saturates the drop counter. */
    CHECK(lte_event_queue_init(&queue, guarded + 1, 1U) == LTE_OK);
    set_event(&input, LTE_EVENT_USER, 1U, 1);
    CHECK(lte_event_push(&queue, &input) == LTE_OK);
    queue.dropped = 65534U;
    CHECK(lte_event_push(&queue, &input) == LTE_FULL);
    CHECK(queue.dropped == 65535U);
    CHECK(lte_event_push(&queue, &input) == LTE_FULL);
    CHECK(queue.dropped == 65535U);

    /* Corrupt descriptors and invalid types fail without changing state. */
    old_queue = queue;
    queue.count = 2U;
    saved = output;
    CHECK(lte_event_push(&queue, &input) == LTE_INVALID);
    CHECK(lte_event_pop(&queue, &output) == LTE_INVALID);
    CHECK(memcmp(&output, &saved, sizeof(output)) == 0);
    CHECK(guarded[1].type == input.type);
    queue = old_queue;
    set_event(&input, 3U, 1U, 1);
    saved_queue = queue;
    saved = guarded[1];
    CHECK(lte_event_push(&queue, &input) == LTE_INVALID);
    CHECK(memcmp(&queue, &saved_queue, sizeof(queue)) == 0);
    CHECK(memcmp(&guarded[1], &saved, sizeof(saved)) == 0);

    memset(&old_queue, 0x6D, sizeof(old_queue));
    saved_queue = old_queue;
    CHECK(lte_event_queue_init(&old_queue, guarded + 1, 65U) == LTE_INVALID);
    CHECK(memcmp(&old_queue, &saved_queue, sizeof(old_queue)) == 0);
#if defined(__WATCOMC__)
    /* Synthetic far pointers exercise segment-window checks without access. */
    old_queue = saved_queue;
    CHECK(lte_event_queue_init(&old_queue,
          (lte_event LTE_FAR *)MK_FP(0x1234U, 0xFFF0U), 2U) == LTE_INVALID);
    CHECK(memcmp(&old_queue, &saved_queue, sizeof(old_queue)) == 0);
    queue.events = (lte_event LTE_FAR *)MK_FP(0x1234U, 0xFFF0U);
    queue.capacity = 2U;
    queue.head = 0U;
    queue.count = 0U;
    saved_queue = queue;
    CHECK(lte_event_push(&queue, &input) == LTE_INVALID);
    CHECK(lte_event_pop(&queue, &output) == LTE_INVALID);
    CHECK(memcmp(&queue, &saved_queue, sizeof(queue)) == 0);
    {
        unsigned far_segment;
        lte_event LTE_FAR *far_records;
        lte_event_queue far_queue;
        lte_event far_input, far_output;
        int far_ok = 0;
        if (_dos_allocmem(56U, &far_segment) == 0) {
            far_records = (lte_event LTE_FAR *)MK_FP(far_segment, 0U);
            set_event(&far_input, LTE_EVENT_USER, 0xC35AU, -1234);
            if (lte_event_queue_init(&far_queue, far_records, 64U) == LTE_OK &&
                lte_event_push(&far_queue, &far_input) == LTE_OK &&
                lte_event_pop(&far_queue, &far_output) == LTE_OK &&
                memcmp(&far_input, &far_output, sizeof(far_input)) == 0)
                far_ok = 1;
            if (_dos_freemem(far_segment) != 0) far_ok = 0;
        }
        CHECK(far_ok);
    }
#endif
    if (failures != 0) return 1;
    puts("event core tests passed");
    return 0;
}
