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
 * Copyright (c) 2013 Joyent, Inc.  All rights reserved.
 */

/*
 * Miscelaneous string functions which are found in various assembly files in
 * other platfors which some day could be optimized on ARM, but seriously.
 */

#include <sys/types.h>
#include <sys/varargs.h>
#include <sys/cmn_err.h>
#include <sys/time.h>

size_t
strlen(const char *s)
{
	const char *e;

	/* TODO Panic on debug if below postbootkernelbase */
	for (e = s; *e != '\0'; e++)
		;
	return (e - s);
}

/* stolen from sparc */
int
highbit(ulong_t i)
{
	register int h = 1;

	if (i == 0)
		return (0);
#ifdef _LP64
	if (i & 0xffffffff00000000ul) {
		h += 32; i >>= 32;
	}
#endif
	if (i & 0xffff0000) {
		h += 16; i >>= 16;
	}
	if (i & 0xff00) {
		h += 8; i >>= 8;
	}
	if (i & 0xf0) {
		h += 4; i >>= 4;
	}
	if (i & 0xc) {
		h += 2; i >>= 2;
	}
	if (i & 0x2) {
		h += 1;
	}
	return (h);
}

int
lowbit(ulong_t i)
{
	register int h = 1;

	if (i == 0)
		return (0);

#ifdef _LP64
	if (!(i & 0xffffffff)) {
		h += 32; i >>= 32;
	}
#endif
	if (!(i & 0xffff)) {
		h += 16; i >>= 16;
	}
	if (!(i & 0xff)) {
		h += 8; i >>= 8;
	}
	if (!(i & 0xf)) {
		h += 4; i >>= 4;
	}
	if (!(i & 0x3)) {
		h += 2; i >>= 2;
	}
	if (!(i & 0x1)) {
		h += 1;
	}
	return (h);
}

extern void bop_panic(const char *) __attribute__ ((noreturn));;

void
vpanic(const char *fmt, va_list adx)
{
	char buf[512];
	(void) vsnprintf(buf, sizeof (buf), fmt, adx);
	bop_panic(buf);
}

#if 0
uintptr_t
arm_va_to_pa(uintptr_t va)
{
	unsigned int pa;
	__asm__("\t mcr p15, 0, %0, c7, c8, 0\n"
	    "\t isb\n"
	    "\t mrc p15, 0, %0, c7, c4, 0\n" : "=r" (pa) : "0" (va));

	return (pa);
}
#endif

static hrtime_t _hrtime_time; /* XXX i don't feel like doing this right now */
hrtime_t gethrtime() { return (_hrtime_time++); }
