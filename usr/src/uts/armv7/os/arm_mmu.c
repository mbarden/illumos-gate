/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License (the "License").
 * You may not use this file except in compliance with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 */
/*
 * Copyright 2010 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#include <sys/types.h>
#include <vm/vm_dep.h>
#include <sys/cmn_err.h>

extern uintptr_t armv7_va_to_pa(uintptr_t va); /* XXX where to put this? */

/*
 * Flag is not set early in boot. Once it is set we are no longer
 * using boot's page tables.
 */
uint_t khat_running = 0;

/*
 * This procedure is callable only while the boot loader is in charge of the
 * MMU. It assumes that PA == VA for page table pointers.  It doesn't live in
 * kboot_mmu.c since it's used from common code.
 */
pfn_t
va_to_pfn(void *vaddr)
{
	uintptr_t pa;

	if (khat_running)
		panic("va_to_pfn(): called too late\n");

	pa = armv7_va_to_pa((uintptr_t)vaddr);

	if ((pa & 1) != 0)
		return (PFN_INVALID);


	return (pa >> MMU_PAGESHIFT);
}
