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
 * Copyright (c) 2013 Joyent, Inc.  All rights reserved.
 * Copyright (c) 2015 Josef 'Jeff' Sipek <jeffpc@josefsipek.net>
 */

#ifndef _SYS_MEMNODE_H
#define	_SYS_MEMNODE_H

/*
 * Mappings between physical addresses and memory nodes.
 */

#ifdef __cplusplus
extern "C" {
#endif

#ifdef	_KERNEL

#include <sys/lgrp.h>

/*
 * In the world of ARMv7, we shouldn't have to worry about separate logical
 * memory nodes. The macro MAX_MEM_NODES determines the static sizing of a lot
 * of data structures in the unix binary. The variable max_mem_nodes will
 * reflect the actual size. For now, we just give ourselves two memory nodes;
 * though, realistically, there isn't likely an ARMv7 platform that is using it.
 * However, we allow an individual platform to override this in their own
 * Makefile.
 */

#ifndef	MAX_MEM_NODES
#define	MAX_MEM_NODES	2
#endif	/* MAX_MEM_NODES */

extern int max_mem_nodes;


#define	PFN_2_MEM_NODE(pfn)			\
	((max_mem_nodes > 1) ? plat_pfn_to_mem_node(pfn) : 0)

#define	MEM_NODE_2_LGRPHAND(mnode)		\
	((max_mem_nodes > 1) ? plat_mem_node_to_lgrphand(mnode) : \
	    LGRP_DEFAULT_HANDLE)

/*
 * Platmod hooks
 */

extern int plat_pfn_to_mem_node(pfn_t);
extern void plat_assign_lgrphand_to_mem_node(lgrp_handle_t, int);
extern lgrp_handle_t plat_mem_node_to_lgrphand(int);
extern void plat_slice_del(pfn_t, pfn_t);

struct mem_node_conf {
	int	exists;		/* only try if set, list may still be empty */
	pfn_t	physbase;	/* lowest PFN in this memnode */
	pfn_t	physmax;	/* highest PFN in this memnode */
};

struct memlist;

extern void startup_build_mem_nodes(struct memlist *);

extern struct mem_node_conf	mem_node_config[];

#endif	/* _KERNEL */

#ifdef __cplusplus
}
#endif

#endif /* _SYS_MEMNODE_H */
