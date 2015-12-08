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
 * Copyright (c) 2014 Joyent, Inc.  All rights reserved.
 */

#include <sys/types.h>
#include <sys/sysmacros.h>
#include <sys/machsystm.h>
#include <sys/sunddi.h>
#include <sys/promif.h>
#include <sys/privregs.h>
#include <sys/cpuvar.h>
#include <sys/stack.h>
#include <sys/vmparam.h>
#include <sys/pg.h>
#include <sys/disp.h>
#include <sys/cpupart.h>
#include <sys/reboot.h>
#include <sys/arm_archext.h>

#include <sys/bootconf.h>

/*
 * We've been given the name of the kernel. From this we should construct the
 * module path.
 *
 * XXX At this time we aren't really handlin the fact that the there are
 * different machine implementations on ARMv6. When we do we need to come back
 * and revisit this and make sure that we properly set impl-arch-name. That
 * means that for now we basically want to return /platform/armv6/kernel for
 * now. Eventually this will become /platform/<mumble>/kernel
 * /platform/armv6/kernel. See uts/sun4/os/mlsetup.c for an example.
 */
void
mach_modpath(char *path, const char *filename)
{
	char *p;

	if ((p = strrchr(filename, '/')) == NULL)
		return;

	while (p > filename && *(p - 1) == '/')
		p--;	/* remove trailing '/' characters */
	if (p == filename)
		p++;	/* so "/" -is- the modpath in this case */

	/*
	 * If we ever support AARCH64 in this file, we must go through and
	 * remove its suffix from the file name if it is there.
	 */
	(void) strncpy(path, filename, p - filename);
}

extern void *romp;
extern struct bootops *ops;
extern struct bootops *bootops;
extern struct boot_syscalls *sysp;

void
mlsetup(struct regs *rp)
{
	extern struct classfuncs sys_classfuncs;
	extern disp_t cpu0_disp;
	extern char t0stack[];

	ASSERT_STACK_ALIGNED();

	/* Verify that we correctly set up curthread */
	ASSERT((uintptr_t)&t0 == (uintptr_t)threadp());

	/*
	 * Pass through cpuid data now and make sure that we have everything
	 * that we expect from our CPU in addition to making sure that we gather
	 * everything that we might want later on.
	 */
	cpuid_setup();

	cpu[0]->cpu_self = cpu[0];
	t0.t_stk = (caddr_t)rp - MINFRAME;
	t0.t_stkbase = t0stack;
	t0.t_pri = maxclsyspri - 3;
	t0.t_schedflag = TS_LOAD | TS_DONT_SWAP;
	t0.t_procp = &p0;
	t0.t_plockp = &p0lock.pl_lock;
	t0.t_lwp = &lwp0;
	t0.t_forw = &t0;
	t0.t_back = &t0;
	t0.t_next = &t0;
	t0.t_prev = &t0;
	t0.t_cpu = cpu[0];
	t0.t_disp_queue = &cpu0_disp;
	t0.t_bind_cpu = PBIND_NONE;
	t0.t_bind_pset = PS_NONE;
	t0.t_bindflag = (uchar_t)default_binding_mode;
	t0.t_cpupart = &cp_default;
	t0.t_clfuncs = &sys_classfuncs.thread;
	t0.t_copyops = NULL;
	THREAD_ONPROC(&t0, CPU);

	lwp0.lwp_thread = &t0;
	lwp0.lwp_regs = (void *)rp;
	lwp0.lwp_procp = &p0;
	t0.t_tid = p0.p_lwpcnt = p0.p_lwprcnt = p0.p_lwpid = 1;

	p0.p_exec = NULL;
	p0.p_stat = SRUN;
	p0.p_flag = SSYS;
	p0.p_tlist = &t0;
	p0.p_stksize = 2*PAGESIZE;
	p0.p_stkpageszc = 0;
	p0.p_as = &kas;
	p0.p_lockp = &p0lock;
	p0.p_brkpageszc = 0;
	p0.p_t1_lgrpid = LGRP_NONE;
	p0.p_tr_lgrpid = LGRP_NONE;
	sigorset(&p0.p_ignore, &ignoredefault);

	CPU->cpu_thread = &t0;
	bzero(&cpu0_disp, sizeof (disp_t));
	CPU->cpu_disp = &cpu0_disp;
	CPU->cpu_disp->disp_cpu = CPU;
	CPU->cpu_dispthread = &t0;
	CPU->cpu_idle_thread = &t0;
	CPU->cpu_flags = CPU_READY | CPU_RUNNING | CPU_EXISTS | CPU_ENABLE;
	CPU->cpu_dispatch_pri = t0.t_pri;

	CPU->cpu_id = 0;

	/* TODO Set CPU priority if we ever have a notion of one? */

	/*
	 * Initialize thread/cpu microstate accounting
	 */
	init_mstate(&t0, LMS_SYSTEM);
	init_cpu_mstate(CPU, CMS_SYSTEM);

	/*
	 * Initialize lists of available and active CPUs.
	 */
	cpu_list_init(CPU);

	pg_cpu_bootstrap(CPU);

	/* XXX why was this XXX'd? */
	cpu_vm_data_init(CPU);

	/*
	 * XXX This is where we should be entering kmdb
	 */

	rp->r_fp = 0;	/* terminate kernel stack traces! */

	prom_init("kernel", (void *)NULL);

	/*
	 * We support zero CPU DR at this time.
	 */
	/* XXX boot_ncpus hasn't been initialized, so things are breaking.
	   where is it supposed to be initialized?
	*/
	max_ncpus = boot_max_ncpus = boot_ncpus = NCPU;

	/*
	 * Initialize the lgrp framework
	 */
	lgrp_init(LGRP_INIT_STAGE1);

	if (boothowto & RB_HALT) {
		prom_printf("unix: kernel halted by -h flag\n");
		prom_enter_mon();
	}

	ASSERT_STACK_ALIGNED();
}
