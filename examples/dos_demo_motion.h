/* SPDX-License-Identifier: MIT */
#ifndef DOS_DEMO_MOTION_H
#define DOS_DEMO_MOTION_H

static unsigned short dos_demo_next_x(unsigned short x)
{
    return x >= 304U ? 8U : (unsigned short)(x + 1U);
}

#endif
