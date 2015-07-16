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

#include <sys/machparam.h>
#include <vm/page.h>
#include <vm/vm_dep.h>

/* XXX for panic */
#include <sys/cmn_err.h>

/*
 * UNIX machine dependent virtual memory support.
 */

/*
 * ARMv7 has PIPT data/unified caches and VIVT instruction caches.
 *
 * In addition, we always set vac_colors to one to indicate that we need virtual
 * address caching.
 *
 * XXX This isn't all wired up at this time, make it so.
 */

#define	ARMv6_PAGE_COLORS	4
uint_t mmu_page_colors = ARMv6_PAGE_COLORS;

uint_t vac_colors = 1;

uint_t mmu_page_sizes = MMU_PAGE_SIZES;
uint_t mmu_exported_page_sizes = MMU_PAGE_SIZES;
uint_t mmu_legacy_page_sizes = MMU_PAGE_SIZES;

page_t ***page_freelists;
page_t **page_cachelists;

/*
 * initialized by page_coloring_init().
 */
uint_t	page_colors;
uint_t	page_colors_mask;
uint_t	page_coloring_shift;
int	cpu_page_colors;

/*
 * The page layer uses this information.
 */
hw_pagesize_t hw_page_array[] = {
	{ MMU_PAGESIZE, MMU_PAGESHIFT, ARMv6_PAGE_COLORS,
		MMU_PAGESIZE >> MMU_PAGESHIFT },
	{ MMU_PAGESIZE64K, MMU_PAGESHIFT64K, ARMv6_PAGE_COLORS,
		MMU_PAGESIZE64K >> MMU_PAGESHIFT },
	{ MMU_PAGESIZE1M, MMU_PAGESHIFT1M, ARMv6_PAGE_COLORS,
		MMU_PAGESIZE1M >> MMU_PAGESHIFT },
	{ MMU_PAGESIZE16M, MMU_PAGESHIFT16M, ARMv6_PAGE_COLORS,
		MMU_PAGESIZE16M >> MMU_PAGESHIFT }
};

kmutex_t	*fpc_mutex[NPC_MUTEX];
kmutex_t	*cpc_mutex[NPC_MUTEX];

void
plcnt_modify_max(pfn_t startpfn, long cnt)
{
	panic("plcnt_modify_max");
}
