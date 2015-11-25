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

#include <sys/types.h>
#include <sys/sysmacros.h>
#include <sys/kmem.h>
#include <sys/atomic.h>
#include <sys/bitmap.h>
#include <sys/systm.h>
#include <vm/vm_dep.h>
#include <vm/htable.h>
#include <vm/hment.h>
#include <sys/avl.h>

/*
 * When pages are shared by more than one mapping, a list of these
 * structs hangs off of the page_t connected by the hm_next and hm_prev
 * fields.  Every hment is also indexed by a system-wide hash table, using
 * hm_hashlink to connect the hments within each hash bucket.
 */
struct hment {
	avl_node_t	hm_hashlink;	/* links for hash table */
	struct hment	*hm_next;	/* next mapping of same page */
	struct hment	*hm_prev;	/* previous mapping of same page */
	htable_t	*hm_htable;	/* corresponding htable_t */
	pfn_t		hm_pfn;		/* mapping page frame number */
	uint16_t	hm_entry;	/* index of pte in htable */
	uint16_t	hm_pad;		/* explicitly expose compiler padding */
#ifdef __amd64
	uint32_t	hm_pad2;	/* explicitly expose compiler padding */
#endif
};

/*
 * "mlist_lock" is a hashed mutex lock for protecting per-page mapping
 * lists and "hash_lock" is a similar lock protecting the hment hash
 * table.  The hashed approach is taken to avoid the spatial overhead of
 * maintaining a separate lock for each page, while still achieving better
 * scalability than a single lock would allow.
 */
#define	MLIST_NUM_LOCK	2048		/* must be power of two */
static kmutex_t *mlist_lock;

/*
 * the shift by 9 is so that all large pages don't use the same hash bucket
 */
#define	MLIST_MUTEX(pp) \
	&mlist_lock[((pp)->p_pagenum + ((pp)->p_pagenum >> 9)) & \
	(MLIST_NUM_LOCK - 1)]

/*
 * These must test for mlist_lock not having been allocated yet.
 * We just ignore locking in that case, as it means were in early
 * single threaded startup.
 */
int
arm_hm_held(page_t *pp)
{
	ASSERT(pp != NULL);
	if (mlist_lock == NULL)
		return (1);
	return (MUTEX_HELD(MLIST_MUTEX(pp)));
}

void
arm_hm_enter(page_t *pp)
{
	ASSERT(pp != NULL);
	if (mlist_lock != NULL)
		mutex_enter(MLIST_MUTEX(pp));
}

void
arm_hm_exit(page_t *pp)
{
	ASSERT(pp != NULL);
	if (mlist_lock != NULL)
		mutex_exit(MLIST_MUTEX(pp));
}

/*
 * return the number of mappings to a page
 *
 * Note there is no ASSERT() that the MUTEX is held for this.
 * Hence the return value might be inaccurate if this is called without
 * doing an arm_hm_enter().
 */
uint_t
hment_mapcnt(page_t *pp)
{
	uint_t cnt;
	uint_t szc;
	page_t *larger;
	hment_t	*hm;

	arm_hm_enter(pp);
	if (pp->p_mapping == NULL)
		cnt = 0;
	else if (pp->p_embed)
		cnt = 1;
	else
		cnt = pp->p_share;
	arm_hm_exit(pp);

	/*
	 * walk through all larger mapping sizes counting mappings
	 */
	for (szc = 1; szc <= pp->p_szc; ++szc) {
		larger = PP_GROUPLEADER(pp, szc);
		if (larger == pp)	/* don't double count large mappings */
			continue;

		arm_hm_enter(larger);
		if (larger->p_mapping != NULL) {
			if (larger->p_embed &&
			    ((htable_t *)larger->p_mapping)->ht_level == szc) {
				++cnt;
			} else if (!larger->p_embed) {
				for (hm = larger->p_mapping; hm;
				    hm = hm->hm_next) {
					if (hm->hm_htable->ht_level == szc)
						++cnt;
				}
			}
		}
		arm_hm_exit(larger);
	}
	return (cnt);
}
