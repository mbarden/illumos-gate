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

	.file	"lock_prim.s"

/*
 * Locking primitives for ARMv7 and above
 */

#if defined(lint) || defined(__lint)
#include <sys/types.h>
#include <sys/thread.h>
#include <sys/cpuvar.h>
#else	/* __lint */
#include "assym.h"
#endif	/* __lint */

#include <sys/asm_linkage.h>
#include <sys/mutex_impl.h>
#include <sys/cpu_asm.h>

/*
 * mutex_enter() and mutex_exit().
 *
 * These routines handle the simple cases of mutex_enter() (adaptive
 * lock, not held) and mutex_exit() (adaptive lock, held, no waiters).
 * If anything complicated is going on we punt to mutex_vector_enter().
 *
 * mutex_tryenter() is similar to mutex_enter() but returns zero if
 * the lock cannot be acquired, nonzero on success.
 * 
 * In the case of mutex_enter() and mutex_tryenter() we may encounter a
 * strex failure. We might be tempted to say that we should try again,
 * but we should not.
 *
 * If we're seeing these kinds of failures then that means there is some
 * kind of contention on the adaptive mutex. It may be tempting to try
 * and say that we should therefore loop back again ala the ARM mutex
 * examples; however, for a very hot and highly contended mutex, this
 * could never make forward progress and it effectively causes us to
 * turn this adaptive lock into a spin lock. While this method will
 * induce more calls to mutex_vector_enter(), this is the safest course
 * of behavior.
 *
 * The strex instruction could fail for example because of the fact that
 * it was just cleared by its owner; however, mutex_vector_enter()
 * already handles this case as this is something that can already
 * happen on other systems which have cmpxchg functions. The key
 * observation to make is that mutex_vector_enter() already has to handle any
 * and all states that a mutex may be possibly be therefore entering due
 * to strex failure is not really any different.
 *
 * If mutex_exit() gets preempted in the window between checking waiters
 * and clearing the lock, we can miss wakeups.  Disabling preemption
 * in the mutex code is prohibitively expensive, so instead we detect
 * mutex preemption by examining the trapped PC in the interrupt path.
 * If we interrupt a thread in mutex_exit() that has not yet cleared
 * the lock, cmnint() resets its PC back to the beginning of
 * mutex_exit() so it will check again for waiters when it resumes.
 *
 * The lockstat code below is activated when the lockstat driver
 * calls lockstat_hot_patch() to hot-patch the kernel mutex code.
 * Note that we don't need to test lockstat_event_mask here -- we won't
 * patch this code in unless we're gathering ADAPTIVE_HOLD lockstats.
 * 
 * TODO None of the lockstat patching is implemented yet. It'll be a
 * wonderful day when lockstat is our problem.
 */

#if defined(lint) || defined(__lint)

/* ARGSUSED */
void
mutex_enter(kmutex_t *lp)
{}

/* ARGSUSED */
int
mutex_tryenter(kmutex_t *lp)
{ return (0); }

/* ARGSUSED */
int
mutex_adaptive_tryenter(mutex_impl_t *lp)
{ return (0); }

/* ARGSUSED */
void
mutex_exit(kmutex_t *lp)
{}

/* ARGSUSED */
void *
mutex_owner_running(mutex_impl_t *lp)

#else

	ENTRY(mutex_enter)
	mrc	CP15_TPIDRPRW(r1)		@ r1 = thread ptr
	ldrex	r2, [r0]
	cmp	r2, #0				@ check if unheld adaptive
	bne	mutex_vector_enter		@ Already held, bail
	strex	r3, r1, [r0]			@ Try to grab it
	cmp	r3, #0
	bne	mutex_vector_enter		@ strex failure, bail
	dmb					@ membar
	bx	lr
	SET_SIZE(mutex_enter)

	ENTRY(mutex_tryenter)
	mrc	CP15_TPIDRPRW(r1)		@ r1 = thread ptr
	ldrex	r2, [r0]
	cmp	r2, #0				@ check if unheld adaptive
	bne	mutex_vector_tryenter		@ Already held, bail
	strex	r3, r1, [r0]			@ Grab attempt	
	cmp	r3, #0
	bne	mutex_vector_tryenter		@ strex failure, bail
	dmb					@ membar
	mov	r0, #1
	bx	lr
	SET_SIZE(mutex_tryenter)	

	ENTRY(mutex_adaptive_tryenter)
	mrc	CP15_TPIDRPRW(r1)		@ r1 = thread ptr
	ldrex	r2, [r0]
	cmp	r2, #0				@ check if unheld adaptive
	bne	1f				@ Already held, bail
	strex	r3, r1, [r0]			@ Grab attempt	
	cmp	r3, #0
	bne	1f				@ strex failure, bail
	dmb					@ membar
	mov	r0, #1				@ return success
	bx	lr
1:
	mov	r0, #0				@ return failure
	bx	lr
	SET_SIZE(mutex_adaptive_tryenter)

	ENTRY(mutex_exit)
mutex_exit_critical_start:			@ Interrupts restart here
	mrc	CP15_TPIDRPRW(r1)		@ r1 = thread ptr
	ldr	r2, [r0]			@ Get the owner field
	dmb
	cmp	r1, r2
	bne	mutex_vector_exit		@ wrong type/owner
	mov	r2, #0
	str	r2, [r0]
.mutex_exit_critical_end:
	bx lr
	SET_SIZE(mutex_exit)
	.globl	mutex_exit_critical_size
	.type	mutex_exit_critical_size, %object
	.align	CPTRSIZE
mutex_exit_critical_size:
	.long	.mutex_exit_critical_end - mutex_exit_critical_start
	SET_SIZE(mutex_exit_critical_size)

	ENTRY(mutex_owner_running)
mutex_owner_running_critical_start:
	ldr	r1, [r0]		@ Get owner field
	and	r1, r1, #MUTEX_THREAD	@ remove waiters
	cmp	r1, #0
	beq	1f			@ free, return NULL
	ldr	r2, [r1, #T_CPU]	@ get owner->t_cpu
	ldr	r3, [r2, #CPU_THREAD]	@ get t_cpu->cpu_thread
.mutex_owner_running_critical_end:
	cmp	r1, r3			/* owner == running thread ?*/
	beq	2f
1:
	mov	r0, #0			@ not running, return NULL
	bx	lr
2:
	mov	r0, r2			/* return cpu */
	bx	lr
	SET_SIZE(mutex_owner_running)
	.globl	mutex_owner_running_critical_size
	.type	mutex_owner_running_critical_size, %object
	.align	CPTRSIZE
mutex_owner_running_critical_size:
	.long	.mutex_owner_running_critical_end - mutex_owner_running_critical_start
	SET_SIZE(mutex_owner_running_critical_size)

/*
 * mutex_delay_default(void)
 * Spins for approx a few hundred processor cycles and returns to caller.
 */

#if defined(lint) || defined(__lint)

void
mutex_delay_default(void)
{}

#else	/* __lint */

	ENTRY(mutex_delay_default)
	mov	r0, #100
1:
	subs	r0, #1
	bne	1b
	bx	lr
	SET_SIZE(mutex_delay_default)

#endif	/* __lint */
