/*
 * This file and its contents are supplied under the terms of the
 * Common Development and Distribution License ("CDDL"), version 1.0.
 * You may only use this file in accordance with the terms of version
 * 1.0 of the CDDL.
 *
 * A full copy of the text of the CDDL should have accompanied this
 * source.  A copy of the CDDL is also available via the Internet at
 * http://www.illumos.org/license/CDDL.
 */

/*
 * Copyright 2013 Joyent, Inc.  All rights reserved.
 */

	.file	"cache.s"

/*
 * Cache and memory barrier operations
 */

#include <sys/asm_linkage.h>
#include <sys/cpu_asm.h>

#if defined(lint) || defined(__lint)

void
membar_sync(void)
{}

void
membar_enter(void)
{}

void
membar_exit(void)
{}

void
membar_producer(void)
{}

void
membar_consumer(void)
{}

void
instr_sbarrier(void)
{}

void
data_sbarrier(void)
{}

#else	/* __lint */

	/*
	 * NOTE: membar_enter, membar_exit, membar_producer, and
	 * membar_consumer are identical routines.  We define them
	 * separately, instead of using ALTENTRY definitions to alias
	 * them together, so that DTrace and debuggers will see a unique
	 * address for them, allowing more accurate tracing.
	 */
	ENTRY(membar_enter)
	ALTENTRY(membar_sync)
	dmb
	bx lr
	SET_SIZE(membar_sync)
	SET_SIZE(membar_enter)

	ENTRY(membar_exit)
	dmb
	bx lr
	SET_SIZE(membar_exit)

	ENTRY(membar_producer)
	dmb
	bx lr
	SET_SIZE(membar_producer)

	ENTRY(membar_consumer)
	dmb
	bx lr
	SET_SIZE(membar_consumer)

	ENTRY(instr_sbarrier)
	isb
	bx lr
	SET_SIZE(membar_consumer)

	ENTRY(data_sbarrier)
	isb
	bx lr
	SET_SIZE(data_sbarrier)

#endif	/* __lint */

#if defined(lint) || defined(__lint)

/*
 * The ARM architecture uses a modified Harvard Architecture which means that we
 * get the joys of fixing up this mess. Primarily this means that when we update
 * data, it gets written to do the data cache. That needs to be flushed to main
 * memory and then the instruction cache needs to be invalidated. This is
 * particularly important for things like krtld and DTrace. While the data cache
 * does write itself out over time, we cannot rely on it having written itself
 * out to the state that we care about by the time that we'd like it to. As
 * such, we need to ensure that it's been flushed out ourselves. This also means
 * that we could accidentally flush a region of the icache that's already
 * updated itself, but that's just what we have to do to keep Von Neumann's
 * spirt and great gift alive.
 *
 * The controllers for the caches have a few different options for invalidation.
 * One may:
 *
 *   o Invalidate or flush the entire cache
 *   o Invalidate or flush a cache line
 *   o Invalidate or flush a cache range
 *
 * We opt to take the third option here for the general case of making sure that
 * text has been synchronized. While the data cache allows us to both invalidate
 * and flush the cache line, we don't currently have a need to do the
 * invalidation.
 *
 * Note that all of these operations should be aligned on an 8-byte boundary.
 * The instructions actually only end up using bits [31:5] of an address.
 * Callers are required to ensure that this is the case.
 */

void
armv7_icache_disable(void)
{}

void
armv7_icache_enable(void)
{}

void
armv7_dcache_disable(void)
{}

void
armv7_dcache_enable(void)
{}

void
armv7_icache_inval(void)
{}

void
armv7_dcache_inval(void)
{}

void
armv7_dcache_clean_inval(void)
{}

void
armv7_dcache_flush(void)
{}

void
armv7_text_flush_range(caddr_t start, size_t len)
{}

void
armv7_text_flush(void)
{}

#else	/* __lint */

	ENTRY(armv7_icache_enable)
	mrc	CP15_sctlr(r0)
	orr	r0, #(ARM_SCTLR_I)
	mcr	CP15_sctlr(r0)
	isb
	bx	lr
	SET_SIZE(armv7_icache_enable)

	ENTRY(armv7_dcache_enable)
	mrc	CP15_sctlr(r0)
	orr	r0, #(ARM_SCTLR_C)
	mcr	CP15_sctlr(r0)
	isb
	bx	lr
	SET_SIZE(armv7_dcache_enable)

	ENTRY(armv7_icache_disable)
	mrc	CP15_sctlr(r0)
	bic	r0, #(ARM_SCTLR_I)
	mcr	CP15_sctlr(r0)
	isb
	bx	lr
	SET_SIZE(armv7_icache_disable)

	ENTRY(armv7_dcache_disable)
	mrc	CP15_sctlr(r0)
	bic	r0, #(ARM_SCTLR_C)
	mcr	CP15_sctlr(r0)
	isb
	bx	lr
	SET_SIZE(armv7_dcache_disable)

	ENTRY(armv7_icache_inval)
	mov	r0, #0
	mcr	CP15_inval_icache(r0)		@ Invalidate i-cache
	isb
	bx	lr
	SET_SIZE(armv7_icache_inval)

	/* based on ARM Architecture Reference Manual */
	ENTRY(iter_by_sw)
	stmfd	sp!, {r4-r11, lr}
	mov	r6, r0
	mrc	CP15_read_clidr(r11)		@ Read CLIDR
	ands	r3, r11, #0x7000000
	mov	r3, r3, lsr #23			@ Cache level value (naturally aligned)
	beq	Finished

	/* for each cache level */
	mov	r10, #0

cache_level:
	add	r2, r10, r10, lsr #1		@ Work out 3xcachelevel
	mov	r1, r11, lsr r2			@ bottom 3 bits are the Cache type for this level
	and	r1, r1, #7			@ get those 3 bits alone
	cmp	r1, #2
	blt	Skip				@ no cache or only instruction cache at this level
	mcr	CP15_write_cssr(r10)		@ write the Cache Size selection register
	isb					@ ISB to sync the change to the CacheSizeID reg
	mrc	CP15_read_csidr(r1)		@ reads current Cache Size ID register
	and	r2, r1, #0x7			@ extract the line length field
	add	r2, r2, #4			@ add 4 for the line length offset (log2 16 bytes)
	ldr	r4, =0x3ff
	ands	r4, r4, r1, lsr #3		@ R4 is the max number on the way size (right aligned)
	clz	r5, r4				@ R5 is the bit position of the way size increment
	ldr	r7, =0x7fff
	ands	r7, r7, r1, lsr #13		@ R7 is the max number of the index size (right aligned)

cache_index:
	mov	r9, r4				@ R9 working copy of the max way size (right aligned)
cache_way:
	orr	r0, r10, r9, lsl r5		@ factor in the way number and cache number into R0
	orr	r0, r0, r7, lsl r2		@ factor in the index number
	blx	r6				@ clean x by set/way
	subs	r9, r9, #1			@ decrement the way number
	bge	cache_way
	subs	r7, r7, #1			@ decrement the index
	bge	cache_index

Skip:
	add	r10, r10, #2			@ increment the cache number
	cmp	r3, r10
	bgt	cache_level

Finished:
	dsb
	ldmfd	sp!, {r4-r11, lr}
	bx	lr
	SET_SIZE(iter_by_sw)

	ENTRY(dcache_clean_inval_sw)
	mcr	CP15_DCCISW(r0)
	bx	lr
	SET_SIZE(dcache_inval_sw)

	ENTRY(dcache_inval_sw)
	mcr	CP15_DCISW(r0)
	bx	lr
	SET_SIZE(dcache_inval_sw)

	ENTRY(dcache_clean_sw)
	mcr	CP15_DCCSW(r0)
	bx	lr
	SET_SIZE(dcache_clean_sw)

	ENTRY(armv7_dcache_clean_inval)
	stmfd	sp!, {lr}
	adr	r0, dcache_clean_inval_sw
	bl	iter_by_sw
	ldmfd	sp!, {lr}
	bx	lr
	SET_SIZE(armv7_dcache_clean_inval)

	ENTRY(armv7_dcache_inval)		@ BROKEN
	stmfd	sp!, {lr}
	adr	r0, dcache_inval_sw
	bl	iter_by_sw
	ldmfd	sp!, {lr}
	bx	lr
	SET_SIZE(armv7_dcache_inval)

	/* the difference between flush and inval is reg DCCSW vs DCISW */
	ENTRY(armv7_dcache_flush)		@ aka. "clean" BROKEN
	stmfd	sp!, {lr}
	adr	r0, dcache_clean_sw
	bl	iter_by_sw
	ldmfd	sp!, {lr}
	bx	lr
	SET_SIZE(armv7_dcache_flush)

	ENTRY(armv7_text_flush_range)		@ aka. "clean" TODO
	stmfd	sp!, {r4, r5}
	mrc	CP15_ctr(r2)

	mov	r4, #1
	and	r3, r2, #0x0f			@ smallest number of words in an icache line (log2)
	add	r3, #2				@ words -> bytes
	lsl	r4, r3

	mov	r5, #1
	and	r3, r2, #0x0f0000		@ smallest number of words in a dcache line (log2)
	mov	r3, r3, lsr #16
	add	r3, #2				@ words -> bytes
	lsl	r5, r3

	add	r1, r1, r0			@ start + len
	mov	r2, r0				@ backup start addr

flush_iline:					@ invalidate icache range
	mcr	CP15_ICIMVAU(r0)
	add	r0, r4
	cmp	r0, r1
	blt	flush_iline

	mov	r0, r2				@ restore start
flush_dline:					@ invalidate dcache range
	mcr	CP15_DCCMVAC(r0)
	add	r0, r5
	cmp	r0, r1
	blt	flush_dline

	dsb
	isb
	ldmfd	sp!, {r4, r5}
	bx	lr
	SET_SIZE(armv7_text_flush_range)

	ENTRY(armv7_text_flush)			@ aka. "clean" TODO
	stmfd	sp!, {lr}
	bl	armv7_icache_inval
	bl	armv7_dcache_inval
	ldmfd	sp!, {lr}
	bx	lr
	SET_SIZE(armv7_text_flush)

#endif

#ifdef __lint

/*
 * Perform all of the operations necessary for tlb maintenance after an update
 * to the page tables.
 */
void
armv7_tlb_sync(void)
{}

#else	/* __lint */

	ENTRY(armv7_tlb_sync)			@ TODO
	stmfd	sp!, {lr}
	bl	armv7_dcache_flush
	mov	r0, #0
	mcr	CP15_TLBIALL(r0)		@ invalidate tlb
	mcr	CP15_inval_icache(r0)		@ Invalidate I-cache + btc
@	mcr	CP15_BPIALL(r0)			@ Invalidate btc
	dsb
	isb
	ldmfd	sp!, {lr}
	bx	lr
	SET_SIZE(armv7_tlb_sync)

#endif	/* __lint */
