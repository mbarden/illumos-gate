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
 * Copyright 2013 (c) Joyent, Inc. All rights reserved.
 */

/*
 * This file contains various ARM subroutines that are generic across all ARM
 * platforms.
 */

#include <sys/asm_linkage.h>
#include <sys/cpu_asm.h>
#include <sys/machlock.h>

#if defined(__lint)
#include <sys/thread.h>
#endif /* __lint */

	/*
	 * a() calls b() calls caller()
	 * return the return address in a()
	 *
	 * NOTE: There is no ABI-defined frame format.  To make matters
	 * worse, gcc and clang use a different definition.  The below
	 * assumes gcc behavior.
	 */
	ENTRY(caller)
	ldr	r0, [r9]
	bx	lr
	SET_SIZE(caller)

	ENTRY(callee)
	mov	r0, lr
	bx	lr
	SET_SIZE(callee)

#define	SETPRI(lvl)							\
	mov	r0, #lvl;						\
	b	do_splx

#define	RAISE(lvl)							\
	mov	r0, #lvl;						\
	b	splr

	ENTRY(spl8)
	SETPRI(15)
	SET_SIZE(spl8)

	ENTRY(spl7)
	RAISE(13)
	SET_SIZE(spl7)

	/* sun specific - highest priority onboard serial i/o asy ports */
	ENTRY(splzs)
	SETPRI(12)	/* Can't be a RAISE, as it's used to lower us */
	SET_SIZE(splzs)

	ENTRY(splhi)
	ALTENTRY(splhigh)
	ALTENTRY(spl6)
	ALTENTRY(i_ddi_splhigh)

	RAISE(DISP_LEVEL)

	SET_SIZE(i_ddi_splhigh)
	SET_SIZE(spl6)
	SET_SIZE(splhigh)
	SET_SIZE(splhi)

	/* allow all interrupts */
	ENTRY(spl0)
	SETPRI(0)
	SET_SIZE(spl0)

	ENTRY(splx)
	b	do_splx
	SET_SIZE(splx)

#if defined(__lint)

/*
 * Return the current kernel thread that's running. Note that this is also
 * available as an inline function.
 */
kthread_id_t
threadp(void)
{ return ((kthread_id_t)0); }

#else	/* __lint */

	ENTRY(threadp)
	mrc	CP15_TPIDRPRW(r0)
	bx	lr
	SET_SIZE(threadp)

#endif	/* __lint */

#if defined(__lint)

/*
 * Subroutine used to spin for a little bit
 */

void
arm_smt_pause(void)
{}

#else	/* __lint */

	ENTRY(arm_smt_pause)
	yield
	bx	lr
	SET_SIZE(arm_smt_pause)

#endif	/* __lint */

	ENTRY(getfp)
	mov	r0, r9
	bx	lr
	SET_SIZE(getfp)
