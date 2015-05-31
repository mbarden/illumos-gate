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

#ifndef _SYS_PRIVREGS_H
#define	_SYS_PRIVREGS_H

/*
 * ARM privregs.h stub.
 */

#ifdef __cplusplus
extern "C" {
#endif

/*
 * XXX: This header file generally contains private details of working with
 * privileged registers which while containing things to interact with some of
 * the normal registers, is also meant to deal with the interrupt controller,
 * etc. For now, this is being punted on until we get a better sense of what we
 * need.
 */

#if !defined(__arm__) && !defined(__aarch64__)
#error	"non-ARM code depends on ARM privilieged header!"
#endif

#ifndef _ASM

struct regs {
	greg_t	r_r0;
	greg_t	r_r1;
	greg_t	r_r2;
	greg_t	r_r3;
	greg_t	r_r4;
	greg_t	r_r5;
	greg_t	r_r6;
	greg_t	r_r7;
	greg_t	r_r8;
	greg_t	r_fp;
	greg_t	r_r10;
	greg_t	r_r11;
	greg_t	r_ip;
	greg_t	r_sp;
	greg_t	r_lr;
	greg_t	r_pc;
	greg_t	r_cpsr;
};

#endif /* _ASM */

#ifdef __cplusplus
}
#endif

#endif /* _SYS_PRIVREGS_H */
