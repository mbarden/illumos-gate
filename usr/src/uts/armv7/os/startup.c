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
#include <sys/memlist_impl.h>
#include <sys/sysmacros.h>
#include <vm/page.h>
#include <vm/hat_pte.h>
#include <vm/vm_dep.h>
#include <vm/seg_kmem.h>
#include <sys/vm_machparam.h>

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

static pgcnt_t kphysm_init(page_t *pp, pgcnt_t npages);
static void layout_kernel_va(void);

extern void pcf_init(void);


struct bootops		*bootops = 0;	/* passed in from boot */
struct bootops		**bootopsp;
struct boot_syscalls	*sysp;		/* passed in from boot */

char kern_bootargs[OBP_MAXPATHLEN];
char kern_bootfile[OBP_MAXPATHLEN];

/* XXX: 1GB for now */
pgcnt_t physmem = mmu_btop(1ul << 30);

uintptr_t	kernelbase;
size_t		segmapsize;
uintptr_t	segmap_start;
pgcnt_t npages;
pgcnt_t orig_npages;
size_t		core_size;		/* size of "core" heap */
uintptr_t	core_base;		/* base address of "core" heap */

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
#define	PRM_POINT(q)	if (prom_debug) 	\
	prom_printf("%s:%d: %s\n", "startup.c", __LINE__, q);

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

/*
 * Copy in a memory list from boot to kernel, with a filter function
 * to remove pages. The filter function can increase the address and/or
 * decrease the size to filter out pages.  It will also align addresses and
 * sizes to PAGESIZE.
 */
void
copy_memlist_filter(
	struct memlist *src,
	struct memlist **dstp,
	void (*filter)(uint64_t *, uint64_t *))
{
	struct memlist *dst, *prev;
	uint64_t addr;
	uint64_t size;
	uint64_t eaddr;

	dst = *dstp;
	prev = dst;

	/*
	 * Move through the memlist applying a filter against
	 * each range of memory. Note that we may apply the
	 * filter multiple times against each memlist entry.
	 */
	for (; src; src = src->ml_next) {
		addr = P2ROUNDUP(src->ml_address, PAGESIZE);
		eaddr = P2ALIGN(src->ml_address + src->ml_size, PAGESIZE);
		while (addr < eaddr) {
			size = eaddr - addr;
			if (filter != NULL)
				filter(&addr, &size);
			if (size == 0)
				break;
			dst->ml_address = addr;
			dst->ml_size = size;
			dst->ml_next = 0;
			if (prev == dst) {
				dst->ml_prev = 0;
				dst++;
			} else {
				dst->ml_prev = prev;
				prev->ml_next = dst;
				dst++;
				prev++;
			}
			addr += size;
		}
	}

	*dstp = dst;
}

/*
 * Callback for copy_memlist_filter() to filter nucleus, kadb/kmdb, (ie.
 * everything mapped above KERNEL_TEXT) pages from phys_avail. Note it
 * also filters out physical page zero.  There is some reliance on the
 * boot loader allocating only a few contiguous physical memory chunks.
 */
static void
avail_filter(uint64_t *addr, uint64_t *size)
{
	uintptr_t va;
	uintptr_t next_va;
	pfn_t pfn;
	uint64_t pfn_addr;
	uint64_t pfn_eaddr;
	uint_t prot;
	size_t len;
	uint_t change;

	if (prom_debug)
		prom_printf("\tFilter: in: a=%" PRIx64 ", s=%" PRIx64 "\n",
		    *addr, *size);

	/*
	 * page zero is required for BIOS.. never make it available
	 */
	if (*addr == 0) {
		*addr += MMU_PAGESIZE;
		*size -= MMU_PAGESIZE;
	}

	/*
	 * First we trim from the front of the range. Since kbm_probe()
	 * walks ranges in virtual order, but addr/size are physical, we need
	 * to the list until no changes are seen.  This deals with the case
	 * where page "p" is mapped at v, page "p + PAGESIZE" is mapped at w
	 * but w < v.
	 */
	do {
		change = 0;
		for (va = KERNEL_TEXT;
		    *size > 0;
		    va = next_va) {

			len = MMU_PAGESIZE;
			next_va = va + len;
			/* if (prom_debug) */
			/* 	prom_printf("armv7_va_to_pa 0x%lx\n", va); */

			pfn_addr = armv7_va_to_pa(va);
			/* if (prom_debug) */
			/* 	prom_printf("armv7_va_to_pa returned 0x%" PRIx64 "\n", pfn_addr); */

			if ((pfn_addr & 1) != 0)
				break;
			pfn_addr &= MMU_STD_PAGEMASK;

			pfn_eaddr = pfn_addr + len;

			if (pfn_addr <= *addr && pfn_eaddr > *addr) {
				change = 1;
				while (*size > 0 && len > 0) {
					*addr += MMU_PAGESIZE;
					*size -= MMU_PAGESIZE;
					len -= MMU_PAGESIZE;
				}
			}
		}
		if (change && prom_debug)
			prom_printf("\t\ttrim: a=%" PRIx64 ", s=%" PRIx64 "\n",
			    *addr, *size);
	} while (change);

	/*
	 * Trim pages from the end of the range.
	 */
	for (va = KERNEL_TEXT;
	    *size > 0;
	    va = next_va) {

		len = MMU_PAGESIZE;
		next_va = va + len;
		pfn_addr = armv7_va_to_pa(va);
		if ((pfn_addr & 1) != 0)
			break;
		pfn_addr &= MMU_STD_PAGEMASK;

		if (pfn_addr >= *addr && pfn_addr < *addr + *size)
			*size = pfn_addr - *addr;
	}

	if (prom_debug)
		prom_printf("\tFilter out: a=%" PRIx64 ", s=%" PRIx64 "\n",
		    *addr, *size);
}

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
	copy_memlist_filter(&bootops->boot_mem.physinstalled, &current, NULL);
	if ((caddr_t)current > (caddr_t)memlist + memlist_sz)
		panic("physinstalled was too big!");
	if (prom_debug)
		print_memlist("phys_install", phys_install);

	phys_avail = current;
	PRM_POINT("Building phys_avail:\n");
	copy_memlist_filter(&bootops->boot_mem.physinstalled, &current,
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

	page_coloring_setup(pagecolor_mem);
	page_lock_init();

	(void) page_ctrs_alloc(page_ctrs_mem);

	pcf_init();

	/*
	 * Initialize the page structures from the memory lists.
	 */
	availrmem_initial = availrmem = freemem = 0;
	PRM_POINT("Calling kphysm_init()...");
	npages = kphysm_init(pp_base, npages);
	PRM_POINT("kphysm_init() done");
	PRM_DEBUG(npages);

	/* XXX init_debug_info(); */

	/*
	 * Now that page_t's have been initialized, remove all the
	 * initial allocation pages from the kernel free page lists.
	 */
	boot_mapin((caddr_t)valloc_base, valloc_sz);
	boot_mapin((caddr_t)MISC_VA_BASE, MISC_VA_SIZE);
	PRM_POINT("startup_memlist() done");

	PRM_DEBUG(valloc_sz);
}

static void
startup_kmem()
{
	extern void page_set_colorequiv_arr(void);

  	PRM_POINT("startup_kmem() starting...");

	kernelbase = (uintptr_t)KERNELBASE;
	kernelbase -= ROUND_UP_PAGE(2 * valloc_sz);
	PRM_DEBUG(kernelbase);

	core_base = valloc_base;
	core_size = 0;

	PRM_DEBUG(core_base);
	PRM_DEBUG(core_size);
	PRM_DEBUG(kernelbase);

	ekernelheap = (char *)core_base;
	PRM_DEBUG(ekernelheap);

	*(uintptr_t *)&_kernelbase = kernelbase;
	*(uintptr_t *)&_userlimit = kernelbase;
	*(uintptr_t *)&_userlimit32 = _userlimit;

	PRM_DEBUG(_kernelbase);
	PRM_DEBUG(_userlimit);
	PRM_DEBUG(_userlimit32);

	layout_kernel_va();

	/*
	 * If segmap is too large we can push the bottom of the kernel heap
	 * higher than the base.  Or worse, it could exceed the top of the
	 * VA space entirely, causing it to wrap around.
	 */
	if (kernelheap >= ekernelheap || (uintptr_t)kernelheap < kernelbase)
		panic("too little address space available for kernelheap");

	PRM_POINT("kernelheap_init");
	/*
	 * Initialize the kernel heap. Note 3rd argument must be > 1st.
	 */
	kernelheap_init(kernelheap, ekernelheap,
	    kernelheap + MMU_PAGESIZE,
	    (void *)core_base, (void *)(core_base + core_size));
	PRM_POINT("kmem_init");
	/*
	 * Initialize kernel memory allocator.
	 */
	kmem_init();

	/*
	 * Factor in colorequiv to check additional 'equivalent' bins
	 */
	page_set_colorequiv_arr();

	/*
	 * Initialize bp_mapin().
	 */
	bp_init(MMU_PAGESIZE, HAT_STORECACHING_OK);
}


static void
startup_vm()
{

	bop_panic("startup_vm");
	hat_init();
	hat_kern_alloc();
	hat_kern_setup();
	/* no more bop_alloc() */
	kvm_init();

	/*
	 * When printing memory, show the total as physmem less
	 * that stolen by a debugger.
	 */
	cmn_err(CE_CONT, "?mem = %ldK (0x%lx000)\n",
	    (ulong_t)(physinstalled) << (PAGESHIFT - 10),
	    (ulong_t)(physinstalled) << (PAGESHIFT - 12));

	/*
	 * disable automatic large pages for small memory systems or
	 * when the disable flag is set.
	 *
	 * Do not yet consider page sizes larger than 2m/4m.
	 */
	if (!auto_lpg_disable && mmu.max_page_level > 0) {
		max_uheap_lpsize = LEVEL_SIZE(1);
		max_ustack_lpsize = LEVEL_SIZE(1);
		max_privmap_lpsize = LEVEL_SIZE(1);
		max_uidata_lpsize = LEVEL_SIZE(1);
		max_utext_lpsize = LEVEL_SIZE(1);
		max_shm_lpsize = LEVEL_SIZE(1);
	}
	if (physmem < privm_lpg_min_physmem || mmu.max_page_level == 0 ||
	    auto_lpg_disable) {
		use_brk_lpg = 0;
		use_stk_lpg = 0;
	}


	i = ptob(MIN(segkpsize, max_phys_segkp));

	rw_enter(&kas.a_lock, RW_WRITER);
	if (segkp_fromheap) {
		segkp->s_as = &kas;
	} else if (seg_attach(&kas, va, i, segkp) < 0)
		cmn_err(CE_PANIC, "startup: cannot attach segkp");
	if (segkp_create(segkp) != 0)
		cmn_err(CE_PANIC, "startup: segkp_create failed");
	rw_exit(&kas.a_lock);


	/*
	 * kpm segment
	 */
	segmap_kpm = kpm_enable &&
	    segmap_kpm && PAGESIZE == MAXBSIZE;

	if (kpm_enable) {
		rw_enter(&kas.a_lock, RW_WRITER);

		/*
		 * The segkpm virtual range range is larger than the
		 * actual physical memory size and also covers gaps in
		 * the physical address range for the following reasons:
		 * . keep conversion between segkpm and physical addresses
		 *   simple, cheap and unambiguous.
		 * . avoid extension/shrink of the the segkpm in case of DR.
		 * . avoid complexity for handling of virtual addressed
		 *   caches, segkpm and the regular mapping scheme must be
		 *   kept in sync wrt. the virtual color of mapped pages.
		 * Any accesses to virtual segkpm ranges not backed by
		 * physical memory will fall through the memseg pfn hash
		 * and will be handled in segkpm_fault.
		 * Additional kpm_size spaces needed for vac alias prevention.
		 */
		if (seg_attach(&kas, kpm_vbase, kpm_size * vac_colors,
		    segkpm) < 0)
			cmn_err(CE_PANIC, "cannot attach segkpm");

		b.prot = PROT_READ | PROT_WRITE;
		b.nvcolors = shm_alignment >> MMU_PAGESHIFT;

		if (segkpm_create(segkpm, (caddr_t)&b) != 0)
			panic("segkpm_create segkpm");

		rw_exit(&kas.a_lock);

		mach_kpm_init();
	}

	segdev_init();
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
extern int stack_test1(void);
extern int stack_test2(void);
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

/*
 * Initialize the platform-specific parts of a page_t.
 */
void
add_physmem_cb(page_t *pp, pfn_t pnum)
{
	pp->p_pagenum = pnum;
}

/*
 * kphysm_init() initializes physical memory.
 */
static pgcnt_t
kphysm_init(
	page_t *pp,
	pgcnt_t npages)
{
	struct memlist	*pmem;
	struct memseg	*cur_memseg;
	pfn_t		base_pfn;
	pfn_t		end_pfn;
	pgcnt_t		num;
	pgcnt_t		pages_done = 0;
	uint64_t	addr;
	uint64_t	size;

	/* XXX do we need ddiphysmin? */

	ASSERT(page_hash != NULL && page_hashsz != 0);

	cur_memseg = memseg_base;
	for (pmem = phys_avail; pmem && npages; pmem = pmem->ml_next) {
		/*
		 * In a 32 bit kernel can't use higher memory if we're
		 * not booting in PAE mode. This check takes care of that.
		 */
		addr = pmem->ml_address;
		size = pmem->ml_size;
		if (btop(addr) > physmax)
			continue;

		/*
		 * align addr and size - they may not be at page boundaries
		 */
		if ((addr & MMU_PAGEOFFSET) != 0) {
			addr += MMU_PAGEOFFSET;
			addr &= ~(uint64_t)MMU_PAGEOFFSET;
			size -= addr - pmem->ml_address;
		}

		/* only process pages below or equal to physmax */
		if ((btop(addr + size) - 1) > physmax)
			size = ptob(physmax - btop(addr) + 1);

		num = btop(size);
		if (num == 0)
			continue;

		if (num > npages)
			num = npages;

		npages -= num;
		pages_done += num;
		base_pfn = btop(addr);

		if (prom_debug)
			prom_printf("MEMSEG addr=0x%" PRIx64
			    " pgs=0x%lx pfn 0x%lx-0x%lx\n",
			    addr, num, base_pfn, base_pfn + num);

		/*
		 * Build the memsegs entry
		 */
		cur_memseg->pages = pp;
		cur_memseg->epages = pp + num;
		cur_memseg->pages_base = base_pfn;
		cur_memseg->pages_end = base_pfn + num;

		/*
		 * Insert into memseg list in decreasing pfn range
		 * order. Low memory is typically more fragmented such
		 * that this ordering keeps the larger ranges at the
		 * front of the list for code that searches memseg.
		 * This ASSERTS that the memsegs coming in from boot
		 * are in increasing physical address order and not
		 * contiguous.
		 */
		if (memsegs != NULL) {
			ASSERT(cur_memseg->pages_base >=
			    memsegs->pages_end);
			cur_memseg->next = memsegs;
		}
		memsegs = cur_memseg;

		/*
		 * add_physmem() initializes the PSM part of the page
		 * struct by calling the PSM back with add_physmem_cb().
		 * In addition it coalesces pages into larger pages as
		 * it initializes them.
		 */
		prom_printf("add_physmem\n");
		add_physmem(pp, num, base_pfn);
		prom_printf("add_physmem done\n");
		cur_memseg++;
		availrmem_initial += num;
		availrmem += num;

		pp += num;
	}

	PRM_DEBUG(availrmem_initial);
	PRM_DEBUG(availrmem);
	PRM_DEBUG(freemem);
	build_pfn_hash();
	return (pages_done);
}

static void
layout_kernel_va(void)
{
	PRM_POINT("layout_kernel_va() starting...");
	/*
	 * Establish the final size of the kernel's heap, size of segmap,
	 * segkp, etc.
	 */

	segmap_start = ROUND_UP_PAGE(kernelbase);
	PRM_DEBUG(segmap_start);

	segmapsize = SEGMAPDEFAULT;

	/*
	 * 32-bit systems don't have segkpm or segkp, so segmap appears at
	 * the bottom of the kernel's address range.  Set aside space for a
	 * small red zone just below the start of segmap.
	 */
	segmap_start += KERNEL_REDZONE_SIZE;
	segmapsize -= KERNEL_REDZONE_SIZE;

	PRM_DEBUG(segmap_start);
	PRM_DEBUG(segmapsize);
	kernelheap = (caddr_t)ROUND_UP_PAGE(segmap_start + segmapsize);
	PRM_DEBUG(kernelheap);
	PRM_POINT("layout_kernel_va() done...");
}
