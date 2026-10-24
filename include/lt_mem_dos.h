/* SPDX-License-Identifier: MIT */
#ifndef LT_MEM_DOS_H
#define LT_MEM_DOS_H

/* DOS conventional-memory accounting for a foreground program (Open
 * Watcom, 16-bit): how much memory the program found at launch, a pool of
 * DOS blocks released together on every exit path, and the totals to
 * report. The requirement itself belongs to the program: measure it (e.g.
 * with tools/eatlow.asm) and compare with ltmem_launch_paras() before
 * changing the video mode. */
#define LTMEM_MAX_BLOCKS 24U

typedef struct {
    unsigned seg[LTMEM_MAX_BLOCKS];
    unsigned paras[LTMEM_MAX_BLOCKS];
    unsigned count;
    unsigned long total_paras;      /* held now */
    unsigned long peak_paras;
    unsigned char failed_free;      /* a DOS free call failed */
} ltmem_pool;

typedef enum {
    LTMEM_OK = 0,
    LTMEM_INVALID = 1,
    LTMEM_FULL = 2,                 /* LTMEM_MAX_BLOCKS blocks held */
    LTMEM_NO_MEMORY = 3             /* DOS had no block that large */
} ltmem_status;

/* Memory available to this program when it was started: its own block
 * (as DOS gave it, after the C startup returned what it did not need)
 * plus the largest block free now, in paragraphs. Call before allocating.
 * Under a loader that keeps memory below the program (a shell, a test
 * wrapper) only what was given to this program is counted. */
unsigned long ltmem_launch_paras(void);
/* Largest free DOS block now, in paragraphs. */
unsigned ltmem_largest_paras(void);

void ltmem_init(ltmem_pool *pool);
/* bytes 1..1,048,560; on success *seg is the block's segment. */
ltmem_status ltmem_alloc(ltmem_pool *pool, unsigned long bytes,
                         unsigned *seg);
/* Free every block, newest first. Returns 1 if all DOS frees succeeded.
 * Safe to call again or on an empty pool. */
int ltmem_release_all(ltmem_pool *pool);

#define LTMEM_KB(paras) ((unsigned long)(paras) / 64UL)

#endif
