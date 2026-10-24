/* SPDX-License-Identifier: MIT */
/* DOS memory accounting; see lt_mem_dos.h. */
#if !defined(__WATCOMC__) || !defined(__I86__)
#error lt_mem_dos.c requires Open Watcom 16-bit x86
#endif
#include <dos.h>
#include <i86.h>
#include "lt_mem_dos.h"

extern unsigned _psp;

unsigned ltmem_largest_paras(void)
{
    unsigned avail = 0;
    (void)_dos_allocmem(0xFFFFU, &avail);
    return avail;
}

unsigned long ltmem_launch_paras(void)
{
    /* The MCB just below the PSP: its size word is at offset 3. */
    unsigned own = *(unsigned __far *)MK_FP(_psp - 1U, 3);
    return (unsigned long)own + 1UL + ltmem_largest_paras();
}

void ltmem_init(ltmem_pool *pool)
{
    unsigned i;
    if (pool == 0) return;
    for (i = 0; i < LTMEM_MAX_BLOCKS; ++i) pool->seg[i] = pool->paras[i] = 0;
    pool->count = 0;
    pool->total_paras = 0;
    pool->peak_paras = 0;
    pool->failed_free = 0;
}

ltmem_status ltmem_alloc(ltmem_pool *pool, unsigned long bytes,
                         unsigned *seg)
{
    unsigned paras, got = 0;
    if (pool == 0 || seg == 0 || bytes == 0UL || bytes > 0xFFFF0UL)
        return LTMEM_INVALID;
    if (pool->count >= LTMEM_MAX_BLOCKS) return LTMEM_FULL;
    paras = (unsigned)((bytes + 15UL) >> 4);
    if (_dos_allocmem(paras, &got) != 0) return LTMEM_NO_MEMORY;
    pool->seg[pool->count] = got;
    pool->paras[pool->count] = paras;
    ++pool->count;
    pool->total_paras += paras;
    if (pool->total_paras > pool->peak_paras)
        pool->peak_paras = pool->total_paras;
    *seg = got;
    return LTMEM_OK;
}

int ltmem_release_all(ltmem_pool *pool)
{
    if (pool == 0) return 1;
    while (pool->count) {
        --pool->count;
        if (_dos_freemem(pool->seg[pool->count]) != 0)
            pool->failed_free = 1;
        pool->total_paras -= pool->paras[pool->count];
        pool->seg[pool->count] = pool->paras[pool->count] = 0;
    }
    return !pool->failed_free;
}
