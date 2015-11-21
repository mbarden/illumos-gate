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
#include <sys/sysmacros.h>
#include <vm/page.h>
#include <vm/hat_pte.h>
#include <vm/vm_dep.h>

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

struct memlist *phys_avail;	/* Total available physical memory */

struct memseg *memseg_base;
struct memlist *memlist;

/*
 * Some CPUs have holes in the middle of the 64-bit virtual address range.
 */
uintptr_t hole_start, hole_end;

/*
 * PROM debugging facilities
 */
int prom_debug = 1;


/*
 * Simple boot time debug facilities
 */
static char *prm_dbg_str[] = {
	"%s:%d: '%s' is 0x%x\n",
	"%s:%d: '%s' is 0x%llx\n"
};

#define panic bop_panic
#define	PRM_DEBUG(q)	if (prom_debug) 	\
	prom_printf(prm_dbg_str[sizeof (q) >> 3], "startup.c", __LINE__, #q, q);


#define	ROUND_UP_PAGE(x)	\
	((uintptr_t)P2ROUNDUP((uintptr_t)(x), (uintptr_t)MMU_PAGESIZE))


/*
 * new memory fragmentations are possible in startup() due to BOP_ALLOCs. this
 * depends on number of BOP_ALLOC calls made and requested size, memory size
 * combination and whether boot.bin memory needs to be freed.
 XXX what on EARTH is this value?
 */
#define	POSS_NEW_FRAGMENTS	12

/*
 * This structure is used to keep track of the intial allocations
 * done in startup_memlist(). The value of NUM_ALLOCATIONS needs to
 * be >= the number of ADD_TO_ALLOCATIONS() executed in the code.
 */
#define	NUM_ALLOCATIONS 8
int num_allocations = 0;
struct {
	void **al_ptr;
	size_t al_size;
} allocations[NUM_ALLOCATIONS];
size_t valloc_sz = 0;
uintptr_t valloc_base;

#define	ADD_TO_ALLOCATIONS(ptr, size) {					\
		size = ROUND_UP_PAGE(size);		 		\
		if (num_allocations == NUM_ALLOCATIONS)			\
			panic("too many ADD_TO_ALLOCATIONS()");		\
		allocations[num_allocations].al_ptr = (void**)&ptr;	\
		allocations[num_allocations].al_size = size;		\
		valloc_sz += size;					\
		++num_allocations;				 	\
	}

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

/*
 * Allocate all the initial memory needed by the page allocator.
 */
static void
perform_allocations(void)
{
	caddr_t mem;
	int i;
	int valloc_align;

	PRM_DEBUG(valloc_base);
	PRM_DEBUG(valloc_sz);
	valloc_align = mmu.level_size[mmu.max_page_level > 0];
	mem = BOP_ALLOC(bootops, (caddr_t)valloc_base, valloc_sz, valloc_align);
	if (mem != (caddr_t)valloc_base)
		panic("BOP_ALLOC() failed");
	bzero(mem, valloc_sz);
	for (i = 0; i < num_allocations; ++i) {
		*allocations[i].al_ptr = (void *)mem;
		mem += allocations[i].al_size;
	}
}

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

extern void mmu_init(void);
extern int size_pse_array(pgcnt_t, int);

static void
startup_memlist()
{
	struct memlist *ml;
	int memblocks;

	size_t memlist_sz;
	size_t memseg_sz;
	size_t pagehash_sz;
	size_t pp_sz;
	size_t pagecolor_memsz;
	size_t page_ctrs_size;
	size_t pse_table_alloc_size;

	caddr_t pagecolor_mem;
	caddr_t page_ctrs_mem;

	struct memlist *current;

	prom_printf("s_text = %p, e_text = %p, s_data = %p, e_data = %p\n", s_text, e_text, s_data, e_data);
	/* moddata/modtext vars */
	/* moddata = (caddr_t)ROUND_UP_PAGE(e_data); */
	/* e_moddata = moddata + MODDATA; */
	/* modtext = (caddr_t)ROUND_UP_PAGE(e_text); */
	/* e_modtext = modtext + MODTEXT; */
	/* econtig = e_moddata; */

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

	/* boot reserved mem */

	mmu_init();
	startup_build_mem_nodes(ml);

	/*
	 * We will need page_t's for every page in the system.
	 *
	 * XXX: Don't count pages we'll never get to use - e.g., pages
	 * mapped at or above the start of kernel .text.
	 */
	npages = physinstalled - 1;

	/* kbm_probe(...) */

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

	/*
	 * We now compute the sizes of all the  initial allocations for
	 * structures the kernel needs in order do kmem_alloc(). These
	 * include:
	 *	memsegs
	 *	memlists
	 *	page hash table
	 *	page_t's
	 *	page coloring data structs
	 */
	memseg_sz = sizeof (struct memseg) * (memblocks + POSS_NEW_FRAGMENTS);
	ADD_TO_ALLOCATIONS(memseg_base, memseg_sz);
	PRM_DEBUG(memseg_sz);

	/*
	 * Reserve space for memlists. There's no real good way to know exactly
	 * how much room we'll need, but this should be a good upper bound.
	 */
	memlist_sz = ROUND_UP_PAGE(2 * sizeof (struct memlist) *
	    (memblocks + POSS_NEW_FRAGMENTS));
	ADD_TO_ALLOCATIONS(memlist, memlist_sz);
	PRM_DEBUG(memlist_sz);

	/*
	 * Reserve space for bios reserved memlists.
	 */
	/* rsvdmemlist_sz = ROUND_UP_PAGE(2 * sizeof (struct memlist) * */
	/*     (rsvdmemblocks + POSS_NEW_FRAGMENTS)); */
	/* ADD_TO_ALLOCATIONS(bios_rsvd, rsvdmemlist_sz); */
	/* PRM_DEBUG(rsvdmemlist_sz); */

	/* /\* LINTED *\/ */
	/* ASSERT(P2SAMEHIGHBIT((1 << PP_SHIFT), sizeof (struct page))); */

	/*
	 * The page structure hash table size is a power of 2
	 * such that the average hash chain length is PAGE_HASHAVELEN.
	 */
	page_hashsz = npages / PAGE_HASHAVELEN;
	page_hashsz_shift = highbit(page_hashsz);
	page_hashsz = 1 << page_hashsz_shift;
	pagehash_sz = sizeof (struct page *) * page_hashsz;
	ADD_TO_ALLOCATIONS(page_hash, pagehash_sz);
	PRM_DEBUG(pagehash_sz);

	/*
	 * Set aside room for the page structures themselves.
	 */
	PRM_DEBUG(npages);
	pp_sz = sizeof (struct page) * npages;
	ADD_TO_ALLOCATIONS(pp_base, pp_sz);
	PRM_DEBUG(pp_sz);

	/*
	 * determine l2 cache info and memory size for page coloring
	 */
	pagecolor_memsz =
	    page_coloring_init(l2cache_sz, l2cache_linesz, l2cache_assoc);
	ADD_TO_ALLOCATIONS(pagecolor_mem, pagecolor_memsz);
	PRM_DEBUG(pagecolor_memsz);

	page_ctrs_size = page_ctrs_sz();
	ADD_TO_ALLOCATIONS(page_ctrs_mem, page_ctrs_size);
	PRM_DEBUG(page_ctrs_size);

	/*
	 * Allocate the array that protects pp->p_selock.
	 */
	pse_shift = size_pse_array(physmem, max_ncpus);
	pse_table_size = 1 << pse_shift;
	pse_table_alloc_size = pse_table_size * sizeof (pad_mutex_t);
	ADD_TO_ALLOCATIONS(pse_mutex, pse_table_alloc_size);

	valloc_base = (uintptr_t)(MISC_VA_BASE - valloc_sz);
	valloc_base = P2ALIGN(valloc_base, mmu.level_size[1]);
	PRM_DEBUG(valloc_base);

	/*
	 * do all the initial allocations
	 */
	perform_allocations();

	/*
	 * Build phys_install and phys_avail in kernel memspace.
	 * - phys_install should be all memory in the system.
	 * - phys_avail is phys_install minus any memory mapped before this
	 *    point above KERNEL_TEXT.
	 */
	current = phys_install = memlist;
	copy_memlist_filter(bootops->boot_mem.physinstalled, &current, NULL);
	if ((caddr_t)current > (caddr_t)memlist + memlist_sz)
		panic("physinstalled was too big!");
	if (prom_debug)
		print_memlist("phys_install", phys_install);

	phys_avail = current;
	PRM_POINT("Building phys_avail:\n");
	copy_memlist_filter(bootops->boot_mem.physinstalled, &current,
	    avail_filter);
	if ((caddr_t)current > (caddr_t)memlist + memlist_sz)
		panic("physavail was too big!");
	if (prom_debug)
		print_memlist("phys_avail", phys_avail);

	/*
	 * Free unused memlist items, which may be used by memory DR driver
	 * at runtime.
	 */
	if ((caddr_t)current < (caddr_t)memlist + memlist_sz) {
		memlist_free_block((caddr_t)current,
		    (caddr_t)memlist + memlist_sz - (caddr_t)current);
	}

	/* ADD_TO_ALLOCATIONS ? */
	/* valloc_base */
	/* perform_allocations() */
	/* phys_install phys_avail */
	/* bios reserved */
	/* page_coloring_setup() */
	/* page_lock_init() */
	/* page_ctrs_alloc() */
	/* pcf_init() */
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
