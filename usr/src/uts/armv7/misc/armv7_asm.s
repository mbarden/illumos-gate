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

#endif	/* __lint */
