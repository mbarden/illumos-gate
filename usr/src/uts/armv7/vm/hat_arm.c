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
 * Copyright (c) 1992, 2010, Oracle and/or its affiliates. All rights reserved.
 */
/*
 * Copyright (c) 2010, Intel Corporation.
 * All rights reserved.
 */
/*
 * Copyright 2011 Nexenta Systems, Inc.  All rights reserved.
 * Copyright (c) 2014, 2015 by Delphix. All rights reserved.
 */

#include <sys/types.h>
#include <vm/hat_pte.h>
#include <sys/machsystm.h>
#include <vm/vm_dep.h>
#include <vm/htable.h>

/*
 * Basic parameters for hat operation.
 */
struct hat_mmu_info mmu;

static void
set_max_page_level()
{
	level_t lvl = 0;

	/* if (!kbm_largepage_support) { */
	/* 	lvl = 0; */
	/* } else { */
	/* 	if (is_x86_feature(x86_featureset, X86FSET_1GPG)) { */
	/* 		lvl = 2; */
	/* 		if (chk_optimal_1gtlb && */
	/* 		    cpuid_opteron_erratum(CPU, 6671130)) { */
	/* 			lvl = 1; */
	/* 		} */
	/* 		if (plat_mnode_xcheck(LEVEL_SIZE(2) >> */
	/* 		    LEVEL_SHIFT(0))) { */
	/* 			lvl = 1; */
	/* 		} */
	/* 	} else { */
	/* 		lvl = 1; */
	/* 	} */
	/* } */
	mmu.max_page_level = lvl;

	/* if ((lvl == 2) && (enable_1gpg == 0)) */
	/* 	mmu.umax_page_level = 1; */
	/* else */
	mmu.umax_page_level = lvl;
}

/*
 * Initialize hat data structures based on processor MMU information.
 */
void
mmu_init(void)
{
	uint_t max_htables;
	uint_t pa_bits;
	uint_t va_bits;
	int i;

	mmu.pt_global = SP_NOTGLOBAL;

	/* nx, LPAE support */

	pa_bits = 32; /* I think LPAE makes this 40? */
	va_bits = 32;

	if (va_bits < sizeof (void *) * NBBY) {
		mmu.hole_start = (1ul << (va_bits - 1));
		mmu.hole_end = 0ul - mmu.hole_start - 1;
	} else {
		mmu.hole_end = 0;
		mmu.hole_start = mmu.hole_end - 1;
	}
	hole_start = mmu.hole_start;
	hole_end = mmu.hole_end;

	mmu.highest_pfn = mmu_btop((1ull << pa_bits) - 1);
	if (mmu.pae_hat == 0 && pa_bits > 32)
		mmu.highest_pfn = PFN_4G - 1;

	if (mmu.pae_hat) {
		mmu.pte_size = 8;	/* 8 byte PTEs */
		mmu.pte_size_shift = 3;
	} else {
		mmu.pte_size = 4;	/* 4 byte PTEs */
		mmu.pte_size_shift = 2;
	}

	if (mmu.pae_hat) {
		mmu.num_level = 3;
		mmu.max_level = 2;
		/* mmu.ptes_per_table = 512; */
		/* mmu.top_level_count = 4; */

		/* mmu.level_shift[0] = 12; */
		/* mmu.level_shift[1] = 21; */
		/* mmu.level_shift[2] = 30; */

	} else {
		mmu.num_level = 2;
		mmu.max_level = 1;
		mmu.ptes_per_table = 4096;
		mmu.top_level_count = 256;

		mmu.level_shift[0] = 12;
		mmu.level_shift[1] = 20;
	}

	for (i = 0; i < mmu.num_level; ++i) {
		mmu.level_size[i] = 1UL << mmu.level_shift[i];
		mmu.level_offset[i] = mmu.level_size[i] - 1;
		mmu.level_mask[i] = ~mmu.level_offset[i];
	}

	set_max_page_level();

	mmu_page_sizes = mmu.max_page_level + 1;
	mmu_exported_page_sizes = mmu.umax_page_level + 1;

	mmu.pte_bits[1] = SP_VALID;
	for (i = 0; i <= mmu.max_page_level; ++i) {
		mmu.pte_bits[i] = SP_VALID;
	}

	/*
	 * NOTE Legacy 32 bit PAE mode only has the P_VALID bit at top level.
	 */
	for (i = 1; i < mmu.num_level; ++i)
		mmu.ptp_bits[i] = PT_BITS;

	mmu.ptp_bits[2] = PT_VALID;

	/*
	 * Compute how many hash table entries to have per process for htables.
	 * We start with 1 page's worth of entries.
	 *
	 * If physical memory is small, reduce the amount need to cover it.
	 */
	max_htables = physmax / mmu.ptes_per_table;
	mmu.hash_cnt = MMU_PAGESIZE / sizeof (htable_t *);
	while (mmu.hash_cnt > 16 && mmu.hash_cnt >= max_htables)
		mmu.hash_cnt >>= 1;
	mmu.vlp_hash_cnt = mmu.hash_cnt;

#if defined(__amd64)
	/*
	 * If running in 64 bits and physical memory is large,
	 * increase the size of the cache to cover all of memory for
	 * a 64 bit process.
	 */
#define	HASH_MAX_LENGTH 4
	while (mmu.hash_cnt * HASH_MAX_LENGTH < max_htables)
		mmu.hash_cnt <<= 1;
#endif
}

