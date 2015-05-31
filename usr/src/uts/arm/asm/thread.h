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

#ifndef _ASM_THREAD_H
#define	_ASM_THREAD_H

#include <sys/ccompile.h>
#include <sys/types.h>

#ifdef	__cplusplus
extern "C" {
#endif

#if !defined(__lint) && defined(__GNUC__)
struct _kthread;


extern __GNU_INLINE struct _kthread *
threadp(void)
{
	void *__value;

#if defined(__arm__)
	__asm__ __volatile__(
	    "mrc p15, 0, %0, c13, c0, 4"
	    : "=r" (__value));
#elif defined(__aarch64__)
/* XXXARM64: NOT YET IMPLEMENTED */
	__value = NULL;
#else
#error	"port me"
#endif
	return (__value);
}


#endif	/* !__lint && __GNUC__ */

#ifdef	__cplusplus
}
#endif

#endif	/* _ASM_THREAD_H */
