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

	.file	"armv7_asm.s"

#include <sys/asm_linkage.h>
#include <sys/cpu_asm.h>

	.arch_extension virt
	.arch_extension idiv
	.arch_extension sec
	.arch_extension mp

#if defined(lint) || defined(__lint)
	
uintptr_t
armv7_va_to_pa(uintptr_t va)
{}

u_longlong_t
randtick(void)
{}

#else	/* __lint */

	ENTRY(armv7_va_to_pa)
	mcr	CP15_ATS1CPR(r0)
	isb
	mrc	CP15_PAR(r0)
	bx	lr
	SET_SIZE(armv7_va_to_pa)

	ENTRY(randtick)
	isb
	mrrc	CP15_CNTPCT(r0, r1)
	bx	lr
	SET_SIZE(randtick)

	ENTRY(stack_test1)
	ldr	r0, =0x12345678
	ldr	r1, =0x87654321
	push	{r0, r1}
	add	r2, sp, #4
	pop	{r0, r1}
	ldr	r0, [r2]
	bx	lr
	SET_SIZE(stack_test1)

	ENTRY(stack_test2)
	ldr	r0, =0x12345678
	ldr	r1, =0x87654321
	push	{r0, r1}
	ldr	r2, [sp]
	pop	{r0, r1}
	mov	r0, r2
	bx	lr
	SET_SIZE(stack_test2)

	
#endif	/* __lint */
