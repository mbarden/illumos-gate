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
 * Copyright 2013 (c) Joyent, Inc.  All rights reserved.
 */

#ifndef _SYS_REGSET_H
#define	_SYS_REGSET_H

/*
 * ARM register definitions.
 * XXX I cannot find an ABI which defines these in the arm world. As such we are
 * just going to roll our own. I realize that's a bit janky, but I'm not sure
 * what else there really is to do.
 */
/*
 * XXX This totally punts on floating point.
 */

#include <sys/feature_tests.h>

#if !defined(_ASM)
#include <sys/types.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

#if !defined(_XPG4_2) || defined(__EXTENSIONS__)

/*
 * XXX
 * Our portable aliases usually refer to REG_R0 and REG_R1. However, R0 and R1
 * are what ARM calls its general purpose registesr. To help work aorund this
 * fact, we ourselves define the register numbers to be REG_ARM_<reg> instead of
 * the traditional REG_<reg>
 */

#define	REG_ARM_R0	0
#define	REG_ARM_R1	1
#define	REG_ARM_R2	2
#define	REG_ARM_R3	3
#define	REG_ARM_R4	4
#define	REG_ARM_R5	5
#define	REG_ARM_R6	6
#define	REG_ARM_R7	7
#define	REG_ARM_R8	8
#define	REG_ARM_R9	9
#define	REG_ARM_R10	10
#define	REG_ARM_R11	11
#define	REG_ARM_R12	12
#define	REG_ARM_R13	13
#define	REG_ARM_R14	14
#define	REG_ARM_R15	15
#define	REG_ARM_CPSR	16

/* Portable Aliases */

#define	REG_PC	REG_ARM_R15
#define	REG_SP	REG_ARM_R13
#define	REG_FP	REG_ARM_R9
#define	REG_PS	REG_ARM_CPSR
#define	REG_R0	REG_ARM_R0
#define	REG_R1	REG_ARM_R1

#endif	/* !defined(_XPG4_2) || defined(__EXTENSIONS__) */

/*
 * A gregset_t is defined as an array type for compatibility with the reference
 * source. This is important due to differences in the way the C language
 * treats arrays and structures as parameters.
 */
#define	_NGREG	17
#if !defined(_XPG4_2) || defined(__EXTENSIONS__)
#define	NGREG	_NGREG
#endif	/* !defined(_XPG4_2) || defined(__EXTENSIONS__) */

#if !defined(_ASM)

typedef long	greg_t;
typedef greg_t	gregset_t[_NGREG];
typedef greg_t  gregset32_t[_NGREG]; /* XXX: cheating/wrong */

/*
 * Floating point definitions
 * XXX Fix these later
 */
typedef struct fpu {
	/* 32 64-bit register for VFP3 / NEON */
	uint64_t f_fpregs[32];
} fpregset_t;

#if !defined(_XPG4_2) || defined(__EXTENSIONS__)

typedef struct {
	gregset_t	gregs;	/* General register set */
} mcontext_t;

#if defined(_SYSCALL32)

typedef struct {
	gregset32_t	gregs;	/* General register set */
} mcontext32_t;

#endif

#endif	/* _ASM */
#endif	/* !defined(_XPG4_2) || defined(__EXTENSIONS__) */

#if defined(_XPG4_2) && !defined(__EXTENSIONS__) && !defined(_ASM)

/*
 * The following is here for UNIX 95 compliance (XPG Issue 4, Version 2
 * System Interfaces and Headers). The structures included here are identical
 * to those visible elsewhere in this header except that the structure
 * element names have been changed in accordance with the X/Open namespace
 * rules.  Specifically, depending on the name and scope, the names have
 * been prepended with a single or double underscore (_ or __).  See the
 * structure definitions in the non-X/Open namespace for more detailed
 * comments describing each of these structures.
 */

typedef struct {
	gregset_t	__gregs;	/* General register set */
} mcontext_t;

#endif /* _XPG4_2 && !__EXTENSIONS__ && !_ASM */

#ifdef __cplusplus
}
#endif

#endif /* _SYS_REGSET_H */
