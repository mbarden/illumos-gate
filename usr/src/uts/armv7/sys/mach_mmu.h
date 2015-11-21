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
 * Copyright 2007 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#ifndef _SYS_MACH_MMU_H
#define	_SYS_MACH_MMU_H

#pragma ident	"%Z%%M%	%I%	%E% SMI"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef _ASM

#include <sys/types.h>
#include <sys/systm.h>

/*
 * Platform-dependent MMU routines and types.
 *
 * WARNING: this header file is used by both dboot and i86pc, so don't go using
 * normal kernel headers.
 */

#define	TWO_MEG		(2 * 1024 * 1024)

/*
 * This is:
 *	The kernel nucleus pagesizes, ie: bi->bi_kseg_size
 *	The grub 64 bit file load address (see multiboot header in dboot_grub.s)
 *	The grub 32 bit and hypervisor physical load addresses of
 *	    the kernel text/data (see Mapfile.unix)
 */
#define	FOUR_MEG	(4 * 1024 * 1024)

#define	ONE_GIG		(1024 * 1024 * 1024)
#define	FOUR_GIG	((uint64_t)4 * ONE_GIG)

#define	MMU_STD_PAGESIZE	4096
#define	MMU_STD_PAGEMASK	0xFFFFF000UL
#define	MMU_STD_PTMASK		0xFFFFFC00UL

/*
 * Defines for the bits in ARM Page Tables
 *
 * Notes:
 *
 * In Solaris the PAT/PWT/PCD values are set up so that:
 *
 * PAT & PWT -> Write Protected
 * PAT & PCD -> Write Combining
 * PAT by itself (PWT == 0 && PCD == 0) yields uncacheable (same as PCD == 1)
 */

#define	PT_DESC		(0x003)	/* entry type bits */

/*
  First Level Page Table:
  31:10 - Page Table Addr
  9 - Implementation Defined
  8:5 - Domain
  4 - Zero
  3 - Non-Secure Region
  2 - Privileged Execute-Never
  1:0 - Descriptor
  - 00 Invalid
  - 01 Page Table
*/

#define	PT_VALID	(0x001) /* Valid bit */
#define	PT_PXN		(0x004)	/* Privileged Execute Never */
#define	PT_NS		(0x008)	/* Non-Secure memory region */
#define	PT_DOMAIN	(0x1E0)	/* Memory Domain */
#define	PT_IMPDEF	(0x200)	/* Implementation Defined */

#define PT_BITS		(PT_VALID)

/*
  Second Level Small Page:
  31:12 - Page Table Addr
  11 - NonGlobal
  10 - Shareable
  9 - AP[2] Access Permissions 2
  - Disable Write
  8:6 - TEX[2:0] Type Extension
  5:4 - AP[1:0] Access Permissions (AP[0] can be Access flag)
  - 5 - User
  3 - Cacheable
  2 - Bufferable
  1:0 - Descriptor
  - 00 Invalid
  - 01 Large Page
  - 1x Small Page
  0 - XN
*/

#define	SP_XN		(0x001) /* Execute Never (small only) */
#define	SP_VALID	(0x002) /* 00 is valid */
#define	SP_BUF		(0x004) /* Bufferable */
#define	SP_CACHE	(0x008) /* Cacheable */
#define	SP_AP		(0x230) /* Access Permisions */
#define	SP_NOWRITE	(0x200) /* Disable Write */
#define	SP_USER		(0x020) /* User Accessible */
#define	SP_ACCESS	(0x010) /* Access Flag */
#define	SP_TEX		(0x1C0) /* Type Extension */
#define	SP_SHARE	(0x400) /* Shareable */
#define	SP_NOTGLOBAL	(0x800) /* Non Global */

/*
 * The software bits are used by the HAT to track attributes.
 * Note that the attributes are inclusive as the values increase.
 *
 * PT_NOSYNC - The PT_REF/PT_MOD bits are not sync'd to page_t.
 *             The hat will install them as always set.
 *
 * PT_NOCONSIST - There is no hment entry for this mapping.
 *
 * PT_FOREIGN - used for the hypervisor, check via
 *		(pte & PT_SOFTWARE) >= PT_FOREIGN
 *		as it might set	0x800 for foreign grant table mappings.
 */
#define	PT_NOSYNC	(0x200)	/* PTE was created with HAT_NOSYNC */
#define	PT_NOCONSIST	(0x400)	/* PTE was created with HAT_LOAD_NOCONSIST */
#define	PT_FOREIGN	(0x600)	/* MFN mapped on the hypervisor has no PFN */

/*
 * The software extraction for a single Page Table Entry will always
 * be a 64 bit unsigned int. If running a non-PAE hat, the page table
 * access routines know to extend/shorten it to 32 bits.
 */
typedef uint64_t arm_lpte_t;
typedef uint32_t arm_pte_t;

arm_lpte_t get_pteval(paddr_t, uint_t);
void set_pteval(paddr_t, uint_t, uint_t, arm_lpte_t);
paddr_t make_ptable(arm_lpte_t *, uint_t);
arm_lpte_t *find_pte(uint64_t, paddr_t *, uint_t, uint_t);
arm_lpte_t *map_pte(paddr_t, uint_t);

extern uint_t *shift_amt;
extern uint_t ptes_per_table;
extern paddr_t top_page_table;
extern uint_t top_level;
extern uint_t pte_size;
extern uint_t shift_amt_nopae[];
extern uint_t shift_amt_pae[];
extern uint32_t lpagesize;

#ifdef __cplusplus
}
#endif

#endif /* _ASM */

#endif	/* _SYS_MACH_MMU_H */
