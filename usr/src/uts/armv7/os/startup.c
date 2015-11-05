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

#include <sys/types.h>
#include <sys/bootconf.h>
#include <sys/obpdefs.h>
#include <sys/promif.h>
#include <sys/systm.h>
#include <sys/memnode.h>
#include <vm/page.h>

/*
 *	32-bit Kernel's Virtual memory layout.
 *		+-----------------------+
 *		|    exception table    |
 * 0xFFFF0000  -|-----------------------|- EXCEPTION_ADDRESS
 *		|                       |
 * 0xFFC00000  -|-----------------------|- ARGSBASE
 *		|   XXX  debugger?      |
 * 0xFF800000  -|-----------------------|- XXX SEGDEBUBBASE?+
 *		|      Kernel Data	|
 * 0xFEC00000  -|-----------------------|
 *              |      Kernel Text	|
 * 0xFE800000  -|-----------------------|- KERNEL_TEXT
 *		|                       |
 *		|    XXX No idea yet    |
 *		|                       |
 * 0xC8002000  -|-----------------------|- XXX segmap_start?
 *		|       Red Zone        |
 * 0xC8000000  -|-----------------------|- kernelbase / userlimit (floating)
 *		|      User Stack       |
 *		|			|
 *		|                       |
 *
 *		:                       :
 *		|    shared objects     |
 *		:                       :
 *
 *		:                       :
 *		|       user data       |
 *             -|-----------------------|-
 *		|       user text       |
 * 0x00002000  -|-----------------------|-  XXX Not necessairily truetoday
 *		|       invalid         |
 * 0x00000000  -|-----------------------|-
 *
 * + Item does not exist at this time.
 */

extern void pcf_init(void);


struct bootops		*bootops = 0;	/* passed in from boot */
struct bootops		**bootopsp;
struct boot_syscalls	*sysp;		/* passed in from boot */

char kern_bootargs[OBP_MAXPATHLEN];
char kern_bootfile[OBP_MAXPATHLEN];

/* XXX: 1GB for now */
pgcnt_t physmem = mmu_btop(1ul << 30);

pgcnt_t npages;
pgcnt_t orig_npages;

caddr_t s_text;		/* start of kernel text segment */
caddr_t e_text;		/* end of kernel text segment */
caddr_t s_data;		/* start of kernel data segment */
caddr_t e_data;		/* end of kernel data segment */
caddr_t modtext;	/* start of loadable module text reserved */
caddr_t e_modtext;	/* end of loadable module text reserved */
caddr_t moddata;	/* start of loadable module data reserved */
caddr_t e_moddata;	/* end of loadable module data reserved */

/*
 * Some CPUs have holes in the middle of the 64-bit virtual address range.
 */
uintptr_t hole_start, hole_end;

/*
 * PROM debugging facilities
 */
int prom_debug = 1;

/*
 * VM related data
 */
long page_hashsz;		/* Size of page hash table (power of two) */
unsigned int page_hashsz_shift;	/* log2(page_hashsz) */
struct page *pp_base;		/* Base of initial system page struct array */
struct page **page_hash;	/* Page hash table */
pad_mutex_t *pse_mutex;		/* Locks protecting pp->p_selock */
size_t pse_table_size;		/* Number of mutexes in pse_mutex[] */
int pse_shift;			/* log2(pse_table_size) */

/*
 * Cache size information filled in via cpuid.
 */
int l2cache_sz;
int l2cache_linesz;
int l2cache_assoc;

static void
print_memlist(char *title, struct memlist *mp)
{
	prom_printf("MEMLIST: %s:\n", title);
	while (mp != NULL)  {
		prom_printf("\tAddress 0x%" PRIx64 ", size 0x%" PRIx64 "\n",
		    mp->ml_address, mp->ml_size);
		mp = mp->ml_next;
	}
}

/*
 * Do basic set up.
 */
static void
startup_init()
{
	if (BOP_GETPROPLEN(bootops, "prom_debug") >= 0) {
		++prom_debug;
		prom_printf("prom_debug found in boot enviroment");
	}
}

static void
startup_memlist()
{
	struct memlist *ml;
	int memblocks;

	ml = &bootops->boot_mem.physinstalled;
	if (prom_debug)
		print_memlist("boot physinstalled", ml);

	/* XXX: assuming we have only one element in our memlist */
	physmax = (ml->ml_address + ml->ml_size - 1) >> PAGESHIFT;
	physinstalled = btop(ml->ml_size);
	memblocks = 1;

	if (prom_debug)
		prom_printf("physmax = %lu, physinstalled = %lu, "
		    "memblocks = %d\n", physmax, physinstalled, memblocks);

	// XXX: mmu_init();
	startup_build_mem_nodes(ml);

	/*
	 * We will need page_t's for every page in the system.
	 *
	 * XXX: Don't count pages we'll never get to use - e.g., pages
	 * mapped at or above the start of kernel .text.
	 */
	npages = physinstalled - 1;

	/*
	 * If physmem is patched to be non-zero, use it instead of the
	 * computed value unless it is larger than the actual amount of
	 * memory on hand.
	 */
	if (physmem == 0 || physmem > npages) {
		physmem = npages;
	} else if (physmem < npages) {
		orig_npages = npages;
		npages = physmem;
	}

	if (prom_debug)
		prom_printf("npages = %lu, orig_npages = %lu\n", npages,
		    orig_npages);

	// XXX: allocate pse_mutex
	// XXX: set valloc_base
	// XXX: set up page coloring
	//	page_coloring_setup(pagecolor_mem);

	page_lock_init();

	// XXX: page_ctrs_alloc(page_ctrs_mem);

	pcf_init();

	/*
	 * Initialize the page structures from the memory lists.
	 */
	availrmem_initial = availrmem = freemem = 0;

	bop_panic("startup_memlist");
}

static void
startup_kmem()
{
	bop_panic("startup_kmem");
}

static void
startup_vm()
{
	bop_panic("startup_vm");
}

static void
startup_modules()
{
	bop_panic("startup_modules");
}

static void
startup_end()
{
	bop_panic("startup_end");
}

/*
 * Our kernel text is at 0xfe800000, data at 0xfec000000, and exception vector
 * at 0xffff0000. These addresses are all determined and set in stone at link
 * time.
 *
 * This is the ARM machine-dependent startup code. Let's startup the world!
 */
void
startup(void)
{
	startup_init();
	/* TODO if we ever need a board specific startup, it goes here */
	startup_memlist();
	startup_kmem();
	startup_vm();
	startup_modules();
	startup_end();
}
