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
 * Copyright 2009 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */
#include <vm/seg_kmem.h>

static vmem_t *ka_arena;

static void *
segkmem_alloc_ka(vmem_t *vmp, size_t size, int flag)
{
	return (segkmem_xalloc(vmp, NULL, size, flag, HAT_STRUCTURE_LE,
	    segkmem_page_create, NULL));
}

/* XXX following sparc, not sure what's best here, x86 does something weird */
void
ka_init(void)
{
	ka_arena = vmem_create("kalloc_arena", NULL, 0, 1,
	    segkmem_alloc_ka, segkmem_free, heap_arena, 0, VM_SLEEP);
}
