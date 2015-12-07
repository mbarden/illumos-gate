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

.file	"trap.s"

#include <sys/asm_linkage.h>
#include <sys/cpu_asm.h>

/*
 * Create a section into which to place the exception vector, such that we can
 * force it to be mapped where it needs to be.
 *
 * Each instruction in the vector jumps to its own address + 24, which is the
 * matching entry in exception_table.  We do this to insure that we get
 * effectively infinite displacement, which we can't achieve in one
 * instruction otherwise (and gas complains).
 *
 * The handler for each exception type saves a trap frame (struct regs, though
 * with some bits unusual) and calls the associated handler from trap_table.
 *
 * XXX: On CPUs with the security extensions, there are actually 2 additional
 * exception vectors: secure, and monitor.  We ignore them.
 */
.pushsection ".exception_vector", "ax"
	ldr	pc, [pc, #24]
	ldr	pc, [pc, #24]
	ldr	pc, [pc, #24]
	ldr	pc, [pc, #24]
	ldr	pc, [pc, #24]
	ldr	pc, [pc, #24]
	ldr	pc, [pc, #24]
	ldr	pc, [pc, #24]
.exception_table:
	.word handle_reset		/* Reset */
	.word handle_undef		/* Undefined insn */
	.word handle_svc		/* Supervisor Call */
	.word handle_prefetchabt	/* Prefetch abort */
	.word handle_dataabt		/* Data abort */
	.word 0xffff0014		/* Reserved (infinite loops) */
	.word handle_irq		/* IRQ */
	.word handle_fiq		/* FIQ */
.popsection

/* Clobbers r0 */
#define	CALL_HANDLER(scratch, trapno)						\
	ldr	scratch, =(trap_table + (4 * trapno));				\
	ldr	scratch, [scratch];						\
	cmp	scratch, #0;							\
	beq	1f;								\
	mov	r0, sp;		/* Pass our saved frame to the handler */	\
	blx	scratch;	/* Call the handler */				\
1:

/*
 * XXX: Note that we go to some contortions here to save in 'struct regs' style.
 *
 * This includes saving our own lr/spsr, for our own return _and_ saving them
 * into the 'struct regs', and doing the register save unusually such that we
 * get the 'struct regs' in order.
 *
 * Depending on the exact nature of 'struct regs', perhaps we should be
 * bending it to our will, rather than letting it bend us?
 *
 * XXX: Note also that we're saving the current registers assuming that we
 * came from supervisor mode.  We should probably be saving the banked
 * registers based on the mode in the spsr.  (or we'll screw up when nested,
 * or when userland traps, or...)
 */
#define	PUSH_TRAP_FRAME(scratch)							\
	srsdb	sp!, #(CPU_MODE_SVC);		/* Save lr and spsr for ourselves */	\
	cps	#(CPU_MODE_SVC);							\
	ldr	lr, [sp];			/* Get lr back for the trap frame */	\
	sub	sp, sp, #(4 * 17);		/* Space for all our registers */	\
	stmia	sp, {r0-r14};			/* XXX: Note we don't save pc */	\
	ldr	scratch, [sp, #(4 * 18)];	/* skip 17 regs and the lr */		\
	str	scratch, [sp, #(4 * 16)];

#define	POP_TRAP_FRAME_AND_RET()						\
	ldmia	sp, {r0-r14};							\
	add	sp, sp, #(4 * 19);	/* 17 regs + 2 for lr and spsr */	\
	rfedb	sp;			/* Return */

/*
 * XXX: None of these handlers are even vaguely aware that usermode exists,
 * and make no effort to do the traditional things we would probably do on
 * returning to user mode.  They will need to be rethought at such a time
 */
.globl trap_table

#define	DEFINE_HANDLER(name, code)		\
	ENTRY(name)				\
		PUSH_TRAP_FRAME(r0)		\
		CALL_HANDLER(r1, code)		\
		POP_TRAP_FRAME_AND_RET()	\
	SET_SIZE(name)

DEFINE_HANDLER(handle_reset, ARM_EXCPT_RESET)

/*
 * XXX: Note that in practice, if we use this for emulation, things
 * might look pretty different, but I think that giving the handler the
 * entire real frame which we restore gives us the flexibility to do it
 * this way.
 */
DEFINE_HANDLER(handle_undef, ARM_EXCPT_UNDINS)

DEFINE_HANDLER(handle_svc, ARM_EXCPT_SVC)
DEFINE_HANDLER(handle_prefetchabt, ARM_EXCPT_PREFETCH)
DEFINE_HANDLER(handle_dataabt, ARM_EXCPT_DATA)

/*
 * XXX: These assume that we really want vectored interrupts, and thus that
 * we'll only ever see these if something went wrong (we took a
 * non-vectored interrupt
 *
 * They may need extension (to pull info from the PIC this early, or
 * whatever.) if we choose to use non-vectored interrupts.
 */
DEFINE_HANDLER(handle_irq, ARM_EXCPT_IRQ)
DEFINE_HANDLER(handle_fiq, ARM_EXCPT_FIQ)
