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
#include <sys/memnode.h>
#include <sys/systm.h>
#include <vm/hat_pte.h>

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

#define	ARMv7_PAGE_COLORS	4
uint_t mmu_page_colors = ARMv7_PAGE_COLORS;

uint_t vac_colors = 1;

uint_t mmu_page_sizes = MMU_PAGE_SIZES;
uint_t mmu_exported_page_sizes = MMU_PAGE_SIZES;
uint_t mmu_legacy_page_sizes = MMU_PAGE_SIZES;

page_t ***page_freelists;
page_t **page_cachelists;

/*
 * initialized by page_coloring_init().
1 */
uint_t	page_colors;
uint_t	page_colors_mask;
uint_t	page_coloring_shift;
int	cpu_page_colors;
static uint_t	l2_colors;
/*
 * The page layer uses this information.
 */
hw_pagesize_t hw_page_array[] = {
	{ MMU_PAGESIZE, MMU_PAGESHIFT, ARMv7_PAGE_COLORS,
		MMU_PAGESIZE >> MMU_PAGESHIFT },
	{ MMU_PAGESIZE64K, MMU_PAGESHIFT64K, ARMv7_PAGE_COLORS,
		MMU_PAGESIZE64K >> MMU_PAGESHIFT },
	{ MMU_PAGESIZE1M, MMU_PAGESHIFT1M, ARMv7_PAGE_COLORS,
		MMU_PAGESIZE1M >> MMU_PAGESHIFT },
	{ MMU_PAGESIZE16M, MMU_PAGESHIFT16M, ARMv7_PAGE_COLORS,
		MMU_PAGESIZE16M >> MMU_PAGESHIFT }
};

kmutex_t	*fpc_mutex[NPC_MUTEX];
kmutex_t	*cpc_mutex[NPC_MUTEX];

void
plcnt_modify_max(pfn_t startpfn, long cnt)
{
	panic("plcnt_modify_max");
}

/*
 * Initialize page coloring variables based on the l2 cache parameters.
 * Calculate and return memory needed for page coloring data structures.
 */
size_t
page_coloring_init(uint_t l2_sz, int l2_linesz, int l2_assoc)
{
	_NOTE(ARGUNUSED(l2_linesz));
	size_t	colorsz = 0;
	int	i;
	int	colors;

	if (l2_assoc && l2_sz > (l2_assoc * MMU_PAGESIZE))
		l2_colors = l2_sz / (l2_assoc * MMU_PAGESIZE);
	else
		l2_colors = 1;

	/* for scalability, configure at least PAGE_COLORS_MIN color bins */
	page_colors = l2_colors;

	/*
	 * cpu_page_colors is non-zero when a page color may be spread across
	 * multiple bins.
	 */
	if (l2_colors < page_colors)
		cpu_page_colors = l2_colors;

	page_colors_mask = page_colors - 1;

	page_coloring_shift = lowbit(CPUSETSIZE());

	/* initialize number of colors per page size */
	for (i = 0; i <= mmu.max_page_level; i++) {
		hw_page_array[i].hp_size = LEVEL_SIZE(i);
		hw_page_array[i].hp_shift = LEVEL_SHIFT(i);
		hw_page_array[i].hp_pgcnt = LEVEL_SIZE(i) >> LEVEL_SHIFT(0);
		hw_page_array[i].hp_colors = (page_colors_mask >>
		    (hw_page_array[i].hp_shift - hw_page_array[0].hp_shift))
		    + 1;
		colorequivszc[i] = 0;
	}

	/*
	 * The value of cpu_page_colors determines if additional color bins
	 * need to be checked for a particular color in the page_get routines.
	 */
	if (cpu_page_colors != 0) {

		int a = lowbit(page_colors) - lowbit(cpu_page_colors);
		ASSERT(a > 0);
		ASSERT(a < 16);

		for (i = 0; i <= mmu.max_page_level; i++) {
			if ((colors = hw_page_array[i].hp_colors) <= 1) {
				colorequivszc[i] = 0;
				continue;
			}
			while ((colors >> a) == 0)
				a--;
			ASSERT(a >= 0);

			/* higher 4 bits encodes color equiv mask */
			colorequivszc[i] = (a << 4);
		}
	}

#if 0
	/* factor in colorequiv to check additional 'equivalent' bins. */
	if (colorequiv > 1) {

		int a = lowbit(colorequiv) - 1;
		if (a > 15)
			a = 15;

		for (i = 0; i <= mmu.max_page_level; i++) {
			if ((colors = hw_page_array[i].hp_colors) <= 1) {
				continue;
			}
			while ((colors >> a) == 0)
				a--;
			if ((a << 4) > colorequivszc[i]) {
				colorequivszc[i] = (a << 4);
			}
		}
	}
#endif

	/* size for fpc_mutex and cpc_mutex */
	colorsz += (2 * max_mem_nodes * sizeof (kmutex_t) * NPC_MUTEX);

	/* size of page_freelists */
	colorsz += sizeof (page_t ***);
	colorsz += mmu_page_sizes * sizeof (page_t **);

	for (i = 0; i < mmu_page_sizes; i++) {
		colors = page_get_pagecolors(i);
		colorsz += colors * sizeof (page_t *);
	}

	/* size of page_cachelists */
	colorsz += sizeof (page_t **);
	colorsz += page_colors * sizeof (page_t *);

	return (colorsz);
}
