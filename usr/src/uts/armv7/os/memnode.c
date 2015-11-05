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
 * Copyright (c) 2015 Josef 'Jeff' Sipek <jeffpc@josefsipek.net>
 */

/*
 * ARMv6 mappings between memory nodes and physical addressese.
 *
 * XXX Fill this in when we know much more about what it looks like.
 */

#include <sys/systm.h>
#include <sys/memlist.h>
#include <sys/memnode.h>
/* This is only used for panic and can be removed when we implement the file */
#include <sys/cmn_err.h>
#include <sys/sysmacros.h>

int max_mem_nodes = 1;

struct mem_node_conf mem_node_config[MAX_MEM_NODES];

uint16_t num_memnodes;
mnodeset_t memnodes_mask; /* assumes 8*(sizeof(mnodeset_t)) >= MAX_MEM_NODES */


int
plat_pfn_to_mem_node(pfn_t pfn)
{
	panic("plat_pfn_to_mem_nodek");
}

void
plat_assign_lgrphand_to_mem_node(lgrp_handle_t hdl, int mnode)
{
	panic("plat_assign_lgrphand_to_mem_node");
}

lgrp_handle_t
plat_mem_node_to_lgrphand(int mnode)
{
	panic("plat_mem_node_to_lgrphand");
}

static void
mem_node_add_slice(pfn_t start, pfn_t end)
{
	int mnode;

	mnode = PFN_2_MEM_NODE(start);
	ASSERT(mnode >= 0 && mnode < max_mem_nodes);

	if (atomic_cas_32((uint32_t *)&mem_node_config[mnode].exists, 0, 1)) {
		/*
		 * Add slice to existing node.
		 */
		if (start < mem_node_config[mnode].physbase)
			mem_node_config[mnode].physbase = start;
		if (end > mem_node_config[mnode].physmax)
			mem_node_config[mnode].physmax = end;
	} else {
		mem_node_config[mnode].physbase = start;
		mem_node_config[mnode].physmax = end;
		atomic_inc_16(&num_memnodes);
		atomic_or_64(&memnodes_mask, 1ull << mnode);
	}

	lgrp_config(LGRP_CONFIG_MEM_ADD, mnode, MEM_NODE_2_LGRPHAND(mnode));
}
void
plat_slice_del(pfn_t start, pfn_t end)
{
	panic("plat_slice_del");
}

void
startup_build_mem_nodes(struct memlist *list)
{
	pfn_t start, end;

	/* LINTED: ASSERT will always true or false */
	ASSERT(NBBY * sizeof (mnodeset_t) >= max_mem_nodes);

	while (list) {
		start = list->ml_address >> PAGESHIFT;
		end = (list->ml_address + list->ml_size - 1) >> PAGESHIFT;
		end = MIN(end, physmax);
		mem_node_add_slice(start, end);
		list = list->ml_next;
	}
}
