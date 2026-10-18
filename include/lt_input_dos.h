/* SPDX-License-Identifier: MIT */
#ifndef LT_INPUT_DOS_H
#define LT_INPUT_DOS_H
#include "lt_event.h"
typedef struct {
 unsigned mouse_enabled,mouse_seen,buttons,x,y;
} lti_dos_input;
typedef enum {LTI_OK=0,LTI_EMPTY=1,LTI_FULL=2,LTI_INVALID=3,
 LTI_UNSUPPORTED=4,LTI_CONTEXT=5} lti_status;
void lti_dos_init(lti_dos_input *input);
/* Opt-in assertion: caller has an initialized compatible INT33 driver.
 * Checks vector/non-IRET only; never resets/installs/displays a mouse cursor.
 * Confirmed=0 disables. Unknown/uninstalled input returns UNSUPPORTED. */
lti_status lti_dos_mouse_enable(lti_dos_input *input,int confirmed);
lti_status lti_dos_mouse(lti_dos_input *input,lte_event *event,
                         unsigned long ticks);
/* At most one legacy BIOS key, then one changed/initial mouse snapshot.
 * KEY_PRESS code=ASCII AL,data=BIOS AH; no key-up/held/repeat/Unicode claim.
 * Mouse code=button bits, x/y preserve16-bit driver coordinate bits, no
 * pixel scaling. First snapshot flags=1. Intermediate transitions may be lost.
 * Full queue backpressures BIOS reads and retains last delivered mouse state.
 * Requires IF=1 and caller serialization; events stamped with supplied ticks. */
lti_status lti_dos_pump(lti_dos_input *input,lte_event_queue *queue,
                        unsigned long ticks);
#endif
