#include <sys/cpuvar.h>
#include <sys/systm.h>

static int current_priority; // TODO: per-cpu

#define	intr_clear()	0 // XXX
#define	intr_restore(x)	do {} while (0) // XXX

#define GPIO_BASE       0x3f200000

/*
 * The BCM2836 (as well as its predecessor BCM2835) does not have any
 * support for hardware interrupt priority.  ("An interrupt vector module
 * has NOT been implemented.")  It presents the OS with three sets of
 * registers which represent interrupt line masks and pending bits.  It is
 * up to the OS to service these interrupts in sequence such that important
 * interrupts are handled sooner than less important interrupts.
 *
 * Fast Interrupts (FIQ) are supported.  If an interrupt is selected for FIQ
 * processing, its normal interrupt enable bit should be cleared.  Note that
 * only one interrupt may be selected for FIQ processing.  ("The FIQ
 * register control which interrupt source can generate a FIQ to the ARM.
 * Only a single interrupt can be selected.")
 *
 * See page 112 of BCM2835 data sheet for register addresses.
 *
 * See page 113 of BCM2835 data sheet for individual interrupt line
 * assignments.
 *
 * SCTLR.VE must be 0.
 *
 * The easiest way to emulate the 16 SPLs that Illumos relies on is to
 * define a set of 16 interrupt-enabled masks (e.g., uint32_t masks[16])
 * where SPL 0 enables all interrupts, LOCK_LEVEL enables anything more
 * important than the timer, etc.  splr/splx can do the usual checking and
 * eventually just loading up the interrupt mask register with the value of
 * masks[new_spl].
 *
 * While it may be tempting to use FIQ for some interrupt (e.g., the clock),
 * it may be easier to handle all interrupts the same way for now.  We will
 * need the same dispatch logic anyway, and the few instructions won't hurt
 * that much.  If they do, we can revisit this assumption then.  Note that
 * it is not possible to select more than one interrupt for FIQ processing.
 *
 * We should probably follow the lead (see page 116 of BCM2835) and number
 * our interrupts using the FIQ scheme.
 *
 * Random data-point: Linux wires up the USB controller to the FIQ
 * interrupt.
 *
 * Since we have three sets of interrupts (ARM-core basic interrupts, and
 * two sets of GPU interrupts) we end up with three sets of masks:
 * basic_IRQ_masks[16], gpu_IRQ_masks1[16], and gpu_IRQ_masks2[16].  They
 * are all used at the same time.  See setspl().
 */

/* IRQs 0-31 */
#define SYS_TIME0	(1 << 0)	/* cmp reg 0: used by GPU */
#define SYS_TIME1	(1 << 1)	/* cmp reg 1 */
#define SYS_TIME2	(1 << 2)	/* cmp reg 2: used by GPU */
#define SYS_TIME3	(1 << 3)	/* cmp reg 3 */
#define USB		(1 << 9)
#define AUX		(1 << 29)	/* mini-UART */

static const uint32_t gpu_IRQ_masks1[16] = {
#if 0
	/* TODO: we currently don't have a timer or usb */
	[0]  = SYS_TIME1 | USB,
	[1]  = SYS_TIME1 | USB,
	[2]  = SYS_TIME1 | USB,
	[3]  = SYS_TIME1 | USB,
	[4]  = SYS_TIME1 | USB,
	[5]  = SYS_TIME1 | USB,
	[6]  = SYS_TIME1,
	[7]  = SYS_TIME1,
	[8]  = SYS_TIME1,
	[9]  = SYS_TIME1,
	[10] = SYS_TIME1,	/* LOCK_LEVEL */
#endif
	[11] = 0,
	[12] = 0,
	[13] = 0,
	[14] = 0,
	[15] = 0,
};

/* IRQs 32-63 */
#define I2C_SPI_SLV	(1 << (43 - 32))
#define PWA0		(1 << (45 - 32))
#define PWA1		(1 << (46 - 32))
#define SMI		(1 << (48 - 32))
#define GPIO0		(1 << (49 - 32))
#define GPIO1		(1 << (50 - 32))
#define GPIO2		(1 << (51 - 32))
#define GPIO3		(1 << (52 - 32))
#define I2C		(1 << (53 - 32))
#define SPI		(1 << (54 - 32))
#define PCM		(1 << (55 - 32))
#define UART		(1 << (57 - 32))

static const uint32_t gpu_IRQ_masks2[16] = {
#if 0
	/* TODO: we currently don't have an interrupt-driven UART */
	[0]  = UART,
	[1]  = UART,
	[2]  = UART,
	[3]  = UART,
	[4]  = UART,
	[5]  = UART,
	[6]  = UART,
	[7]  = UART,
	[8]  = UART,
	[9]  = UART,
	[10] = UART,	/* LOCK_LEVEL */
	[11] = UART,
	[12] = UART,
	[13] = UART,
	[14] = UART,
#endif
	[15] = 0,
};

/* basic IRQs (we call them 64-71) */
#define ACCESS_ERR_0	(1 << 7)
#define ACCESS_ERR_1	(1 << 6)
#define GPU_HALTED_1	(1 << 5)
#define GPU_HALTED_0	(1 << 4)
#define ARM_DOORBELL_1	(1 << 3)
#define ARM_DOORBELL_0	(1 << 2)
#define ARM_MBOX	(1 << 1)
#define ARM_TIMER	(1 << 0)

static const uint32_t basic_IRQ_masks[16] = {
	[0]  = 0,
	[1]  = 0,
	[2]  = 0,
	[3]  = 0,
	[4]  = 0,
	[5]  = 0,
	[6]  = 0,
	[7]  = 0,
	[8]  = 0,
	[9]  = 0,
	[10] = 0,	/* LOCK_LEVEL */
	[11] = 0,
	[12] = 0,
	[13] = 0,
	[14] = 0,
	[15] = 0,
};

static void
setspl(int newpri)
{
	volatile uint32_t *gpu_irqs1 = (void *)(uintptr_t)(GPIO_BASE + 0x210);
	volatile uint32_t *gpu_irqs2 = (void *)(uintptr_t)(GPIO_BASE + 0x214);
	volatile uint32_t *basic_irqs = (void *)(uintptr_t)(GPIO_BASE + 0x218);

	ASSERT3S(newpri, >=, 0);
	ASSERT3S(newpri, <=, 15);

	/* XXX: we shouldn't be enabling any interrupts just yet */
	ASSERT0(gpu_IRQ_masks1[newpri]);
	ASSERT0(gpu_IRQ_masks2[newpri]);
	ASSERT0(basic_IRQ_masks[newpri]);

	ASSERT0(basic_IRQ_masks[newpri] & 0xffffff00);

	*gpu_irqs1 = gpu_IRQ_masks1[newpri];
	*gpu_irqs2 = gpu_IRQ_masks2[newpri];
	*basic_irqs = basic_IRQ_masks[newpri];
}

int
do_splx(int newpri)
{
	cpu_t *cpu;
	ulong_t flag;
	int curpri, basepri;

	flag = intr_clear();
	cpu = CPU;
	curpri = current_priority;
	basepri = cpu->cpu_base_spl;
	if (newpri < basepri)
		newpri = basepri;
	current_priority = newpri;
	setspl(newpri);

	// XXX: softint?

	intr_restore(flag);

	return (curpri);
}

int
splr(int newpri)
{
	cpu_t *cpu;
	ulong_t flag;
	int curpri, basepri;

	flag = intr_clear();
	cpu = CPU;
	curpri = current_priority;

	if (newpri > curpri) {
		basepri = cpu->cpu_base_spl;
		if (newpri < basepri)
			newpri = basepri;
		current_priority = newpri;
		setspl(newpri);

		// XXX: softint?
	}

	intr_restore(flag);
	return (curpri);
}
