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
 * Just like in i86pc, we too get the joys of mimicking the SPARC boot system.
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/bootconf.h>
#include <sys/bootsvcs.h>
#include <sys/boot_console.h>
#include <sys/atag.h>
#include <sys/varargs.h>
#include <sys/cmn_err.h>
#include <sys/sysmacros.h>
#include <sys/systm.h>
#include <sys/ctype.h>
#include <sys/bootstat.h>
#include <sys/privregs.h>
#include <sys/cpu_asm.h>
#include <sys/boot_mmu.h>
#include <sys/elf.h>
#include <sys/archsystm.h>

static bootops_t bootop;

/*
 * Debugging help
 */
static int fakebop_prop_debug = 0;
static int fakebop_alloc_debug = 0;
static int fakebop_atag_debug = 0;

static uint_t kbm_debug = 1;
#define	DBG_MSG(x)	{ if (kbm_debug) bcons_puts(x); bcons_puts("\n"); }
#define	BUFFERSIZE	256
static char buffer[BUFFERSIZE];

/*
 * fakebop memory allocations scheme
 *
 * It's a virtual world out there. The loader thankfully tells us all the areas
 * that it has mapped for us and it also tells us about the page table arena --
 * a set of addresses that have already been set aside for us. We have two
 * different kinds of allocations to worry about:
 *
 *    o Those that specify a particular vaddr
 *    o Those that do not specify a particular vaddr
 *
 * Those that do not specify a particular vaddr will come out of our scratch
 * space which is a fixed size arena of 16 MB (FAKEBOP_ALLOC_SIZE) that we set
 * aside at the beginning of the allocator. If we end up running out of that
 * then we'll go ahead and figure out a slightly larger area to worry about.
 *
 * Now, for those that do specify a particular vaddr we'll allocate more
 * physical address space for it. The loader set aside enough L2 page tables for
 * us that we'll go ahead and use the next 4k aligned address.
 */
#define	FAKEBOP_ALLOC_SIZE	(16 * 1024 * 1024)

static size_t bop_alloc_scratch_size;
static uintptr_t bop_alloc_scratch_next;	/* Next scratch address */
static uintptr_t bop_alloc_scratch_last;	/* Last scratch address */

static uintptr_t bop_alloc_pnext;		/* Next paddr */
static uintptr_t bop_alloc_plast;		/* cross this paddr and panic */

#define	BI_HAS_RAMDISK	0x1

/*
 * TODO Generalize this
 * This is the set of information tha we want to gather from the various atag
 * headers. This is simple and naive and will need to evolve as we have
 * additional boards beyond just the RPi.
 */
typedef struct bootinfo {
	uint_t		bi_flags;
	char 		*bi_cmdline;
	uint32_t	bi_ramdisk;
	uint32_t	bi_ramsize;
} bootinfo_t;

static bootinfo_t bootinfo;	/* Simple set of boot information */

static struct boot_syscalls bop_sysp = {
	bcons_getchar,
	bcons_putchar,
	bcons_ischar,
};

/*
 * stuff to store/report/manipulate boot property settings.
 */
typedef struct bootprop {
	struct bootprop *bp_next;
	char *bp_name;
	uint_t bp_vlen;
	char *bp_value;
} bootprop_t;

static bootprop_t *bprops = NULL;

static void
hexdump_stack()
{
	extern char t0stack[];

	uint8_t *start = (uint8_t *)t0stack;
	uint8_t *end = start + DEFAULTSTKSZ;
	uint8_t *ptr;
	int i;

	bop_printf(NULL, "stack (fp = %x):\n", getfp());

	ptr = (uint8_t *)(getfp() & ~0xf);
	if (ptr <= start || ptr >= end)
		ptr = start;

	while (ptr < end) {
		uint32_t *tmp = (uint32_t *)ptr;

		bop_printf(NULL, "%p: %08x %08x %08x %08x\n", ptr,
		    tmp[0], tmp[1], tmp[2], tmp[3]);

		ptr += 16;
	}

}

void
bop_panic(const char *msg)
{
	bop_printf(NULL, "ARM bop_panic:\n%s\n", msg);

	hexdump_stack();

	bop_printf(NULL, "Spinning Forever...\n", msg);
	for (;;)
		;
}

/*
 * XXX This is just a hack to let us see a bit more about what's going on.
 * Normally we'd use vsnprintf, but that includes sys/systm.h which requires
 * almost every header platform header in the world. Also, we're using hex,
 * because hex is cool. Actually, we're really using it because it means we can
 * bitshift instead of divide. Integer division is optional on ARMv7-A.
 * Oops.
 */
static char *
fakebop_hack_ultostr(unsigned long value, char *ptr)
{
	ulong_t t, val = (ulong_t)value;
	char c;

	do {
		c = (char)('0' + val - 16 * (t = (val >> 4)));
		if (c > '9')
			c += 'A' - '9' - 1;
		*--ptr = c;
	} while ((val = t) != 0);

	*--ptr = 'x';
	*--ptr = '0';

	return (ptr);
}

/*
 * We need to map and reserve the scratch arena. As a part of this we'll go
 * through and set up the right place for other paddrs.
 */
static void
fakebop_alloc_init(atag_header_t *chain)
{
	uintptr_t pstart, vstart, pmax;
	size_t len;
	atag_illumos_mapping_t *aimp;
	atag_illumos_status_t *aisp;

	aisp = (atag_illumos_status_t *)atag_find(chain, ATAG_ILLUMOS_STATUS);
	if (aisp == NULL)
		bop_panic("missing ATAG_ILLUMOS_STATUS!\n");
	pstart = aisp->ais_freemem + aisp->ais_freeused;
	/* Align to next 1 MB boundary */
	if (pstart & MMU_PAGEOFFSET1M) {
		pstart &= MMU_PAGEMASK1M;
		pstart += MMU_PAGESIZE1M;
	}
	len = FAKEBOP_ALLOC_SIZE;
	vstart = pstart;

	pmax = 0xffffffff;
	/* Make sure the paddrs and vaddrs don't overlap at all */
	for (aimp =
	    (atag_illumos_mapping_t *)atag_find(chain, ATAG_ILLUMOS_MAPPING);
	    aimp != NULL; aimp =
	    (atag_illumos_mapping_t *)atag_find(atag_next(&aimp->aim_header),
	    ATAG_ILLUMOS_MAPPING)) {
		if (aimp->aim_paddr < pstart &&
		    aimp->aim_paddr + aimp->aim_vlen > pstart)
			bop_panic("phys addresses overlap\n");
		if (pstart < aimp->aim_paddr && pstart + len > aimp->aim_paddr)
			bop_panic("phys addresses overlap\n");
		if (aimp->aim_vaddr < vstart && aimp->aim_vaddr +
		    aimp->aim_vlen > vstart)
			bop_panic("virt addreses overlap\n");
		if (vstart < aimp->aim_vaddr && vstart + len > aimp->aim_vaddr)
			bop_panic("virt addresses overlap\n");

		if (aimp->aim_paddr > pstart && aimp->aim_paddr < pmax)
			pmax = aimp->aim_paddr;
	}

	armboot_mmu_map(pstart, vstart, len, PF_R | PF_W | PF_X);
	bop_alloc_scratch_next = vstart;
	bop_alloc_scratch_last = vstart + len;
	bop_alloc_scratch_size = len;

	bop_alloc_pnext = pstart + len;
	bop_alloc_plast = pmax;
}

#define DUMP_ATAG_VAL(name, val)					\
	do {								\
		DBG_MSG("\t" name ":");					\
		bcons_puts("\t");					\
		DBG_MSG(fakebop_hack_ultostr((val),			\
		    &buffer[BUFFERSIZE-1]));				\
	} while (0)

static void
fakebop_dump_tags(void *tagstart)
{
	atag_header_t *h = tagstart;
	atag_core_t *acp;
	atag_mem_t *amp;
	atag_cmdline_t *alp;
	atag_initrd_t *aip;
	atag_illumos_status_t *aisp;
	atag_illumos_mapping_t *aimp;
	const char *tname;
	int i;
	char *c;

	DBG_MSG("starting point:");
	DBG_MSG(fakebop_hack_ultostr((uintptr_t)h, &buffer[BUFFERSIZE-1]));
	DBG_MSG("first atag size:");
	DBG_MSG(fakebop_hack_ultostr(h->ah_size, &buffer[BUFFERSIZE-1]));
	DBG_MSG("first atag tag:");
	DBG_MSG(fakebop_hack_ultostr(h->ah_tag, &buffer[BUFFERSIZE-1]));
	while (h != NULL) {
		switch (h->ah_tag) {
		case ATAG_CORE:
			tname = "ATAG_CORE";
			break;
		case ATAG_MEM:
			tname = "ATAG_MEM";
			break;
		case ATAG_VIDEOTEXT:
			tname = "ATAG_VIDEOTEXT";
			break;
		case ATAG_RAMDISK:
			tname = "ATAG_RAMDISK";
			break;
		case ATAG_INITRD2:
			tname = "ATAG_INITRD2";
			break;
		case ATAG_SERIAL:
			tname = "ATAG_SERIAL";
			break;
		case ATAG_REVISION:
			tname = "ATAG_REVISION";
			break;
		case ATAG_VIDEOLFB:
			tname = "ATAG_VIDEOLFB";
			break;
		case ATAG_CMDLINE:
			tname = "ATAG_CMDLINE";
			break;
		case ATAG_ILLUMOS_STATUS:
			tname = "ATAG_ILLUMOS_STATUS";
			break;
		case ATAG_ILLUMOS_MAPPING:
			tname = "ATAG_ILLUMOS_MAPPING";
			break;
		default:
			tname = fakebop_hack_ultostr(h->ah_tag,
			    &buffer[BUFFERSIZE-1]);
			break;
		}
		DBG_MSG("tag:");
		DBG_MSG(tname);
		DBG_MSG("size:");
		DBG_MSG(fakebop_hack_ultostr(h->ah_size,
		    &buffer[BUFFERSIZE-1]));
		/* Extended information */
		switch (h->ah_tag) {
		case ATAG_CORE:
			if (h->ah_size == 2) {
				DBG_MSG("ATAG_CORE has no extra information");
			} else {
				acp = (atag_core_t *)h;
				DUMP_ATAG_VAL("flags", acp->ac_flags);
				DUMP_ATAG_VAL("pagesize", acp->ac_pagesize);
				DUMP_ATAG_VAL("rootdev", acp->ac_rootdev);
			}
			break;
		case ATAG_MEM:
			amp = (atag_mem_t *)h;
			DUMP_ATAG_VAL("size", amp->am_size);
			DUMP_ATAG_VAL("start", amp->am_start);
			break;
		case ATAG_INITRD2:
			aip = (atag_initrd_t *)h;
			DUMP_ATAG_VAL("size", aip->ai_size);
			DUMP_ATAG_VAL("start", aip->ai_start);
			break;
		case ATAG_CMDLINE:
			alp = (atag_cmdline_t *)h;
			DBG_MSG("\tcmdline:");
			/*
			 * We have no intelligent thing to wrap our tty at 80
			 * chars so we just do this a bit more manually for now.
			 */
			i = 0;
			c = alp->al_cmdline;
			while (*c != '\0') {
				bcons_putchar(*c++);
				if (++i == 72) {
					bcons_puts("\n");
					i = 0;
				}
			}
			bcons_puts("\n");
			break;
		case ATAG_ILLUMOS_STATUS:
			aisp = (atag_illumos_status_t *)h;
			DUMP_ATAG_VAL("version", aisp->ais_version);
			DUMP_ATAG_VAL("ptbase", aisp->ais_ptbase);
			DUMP_ATAG_VAL("freemem", aisp->ais_freemem);
			DUMP_ATAG_VAL("freeused", aisp->ais_freeused);
			DUMP_ATAG_VAL("archive", aisp->ais_archive);
			DUMP_ATAG_VAL("archivelen", aisp->ais_archivelen);
			DUMP_ATAG_VAL("pt_arena", aisp->ais_pt_arena);
			DUMP_ATAG_VAL("pt_arena_max", aisp->ais_pt_arena_max);
			DUMP_ATAG_VAL("stext", aisp->ais_stext);
			DUMP_ATAG_VAL("etext", aisp->ais_etext);
			DUMP_ATAG_VAL("sdata", aisp->ais_sdata);
			DUMP_ATAG_VAL("edata", aisp->ais_edata);
			break;
		case ATAG_ILLUMOS_MAPPING:
			aimp = (atag_illumos_mapping_t *)h;
			DUMP_ATAG_VAL("paddr", aimp->aim_paddr);
			DUMP_ATAG_VAL("plen", aimp->aim_plen);
			DUMP_ATAG_VAL("vaddr", aimp->aim_vaddr);
			DUMP_ATAG_VAL("vlen", aimp->aim_vlen);
			DUMP_ATAG_VAL("mapflags", aimp->aim_mapflags);
			break;
		default:
			break;
		}
		h = atag_next(h);
	}
}

static void
fakebop_getatags(void *tagstart)
{
	atag_mem_t *amp;
	atag_cmdline_t *alp;
	atag_header_t *ahp = tagstart;
	atag_illumos_status_t *aisp;
	bootinfo_t *bp = &bootinfo;
	boolean_t got_mem = B_FALSE;

	bp->bi_flags = 0;
	while (ahp != NULL) {
		switch (ahp->ah_tag) {
		case ATAG_MEM:
			amp = (atag_mem_t *)ahp;
			/*
			 * We may actually get more than one ATAG_MEM if the
			 * system has discontiguous physical memory
			 */
			if (got_mem) {
				bop_printf(NULL, "found multiple ATAG_MEM\n");
				bop_printf(NULL, "ignoring: %#x - %#x\n",
				    amp->am_start, amp->am_start +
				    amp->am_size - 1);
				break;
			}

			bootop.boot_mem.physinstalled.ml_address = amp->am_start;
			bootop.boot_mem.physinstalled.ml_size = amp->am_size;
			bootop.boot_mem.physinstalled.ml_prev = NULL;
			bootop.boot_mem.physinstalled.ml_next = NULL;
			got_mem = B_TRUE;
			break;
		case ATAG_CMDLINE:
			alp = (atag_cmdline_t *)ahp;
			bp->bi_cmdline = alp->al_cmdline;
			break;
		case ATAG_ILLUMOS_STATUS:
			aisp = (atag_illumos_status_t *)ahp;
			bp->bi_ramdisk = aisp->ais_archive;
			bp->bi_ramsize = aisp->ais_archivelen;
			bp->bi_flags |= BI_HAS_RAMDISK;
			break;
		default:
			break;
		}
		ahp = atag_next(ahp);
	}
}

/*
 * We've been asked to allocate at a specific VA. Allocate the next ragne of
 * physical addresses and go from there.
 */
static caddr_t
fakebop_alloc_hint(caddr_t virt, size_t size, int align)
{
	uintptr_t start = P2ROUNDUP(bop_alloc_pnext, align);
	if (fakebop_alloc_debug != 0)
		bop_printf(NULL, "asked to allocate %d bytes at v/p %p/%p\n",
		    size, virt, start);
	if (start + size > bop_alloc_plast)
		bop_panic("fakebop_alloc_hint: No more physical address -_-\n");

	armboot_mmu_map(start, (uintptr_t)virt, size, PF_R | PF_W | PF_X);
	bop_alloc_pnext = start + size;
	return (virt);
}

static caddr_t
fakebop_alloc(struct bootops *bops, caddr_t virthint, size_t size, int align)
{
	caddr_t start;

	if (virthint != NULL)
		return (fakebop_alloc_hint(virthint, size, align));
	if (fakebop_alloc_debug != 0)
		bop_printf(bops, "asked to allocate %d bytes\n", size);
	if (bop_alloc_scratch_next == 0)
		bop_panic("fakebop_alloc_init not called");

	if (align == BO_ALIGN_DONTCARE || align == 0)
		align = 4;

	start = (caddr_t)P2ROUNDUP(bop_alloc_scratch_next, align);
	if ((uintptr_t)start + size > bop_alloc_scratch_last)
		bop_panic("fakebop_alloc: ran out of scratch space!\n");
	if (fakebop_alloc_debug != 0)
		bop_printf(bops, "returning address: %p\n", start);
	bop_alloc_scratch_next = (uintptr_t)start + size;

	return (start);
}

static void
fakebop_free(struct bootops *bops, caddr_t virt, size_t size)
{
	bop_panic("Called into fakebop_free");
}

static int
fakebop_getproplen(struct bootops *bops, const char *pname)
{
	bootprop_t *p;

	if (fakebop_prop_debug)
		bop_printf(NULL, "fakebop_getproplen: asked for %s\n", pname);
	for (p = bprops; p != NULL; p = p->bp_next) {
		if (strcmp(pname, p->bp_name) == 0)
			return (p->bp_vlen);
	}
	if (fakebop_prop_debug != 0)
		bop_printf(NULL, "prop %s not found\n", pname);
	return (-1);
}

static int
fakebop_getprop(struct bootops *bops, const char *pname, void *value)
{
	bootprop_t *p;

	if (fakebop_prop_debug)
		bop_printf(NULL, "fakebop_getprop: asked for %s\n", pname);
	for (p = bprops; p != NULL; p = p->bp_next) {
		if (strcmp(pname, p->bp_name) == 0)
			break;
	}
	if (p == NULL) {
		if (fakebop_prop_debug)
			bop_printf(NULL, "fakebop_getprop: ENOPROP %s\n",
			    pname);
		return (-1);
	}
	if (fakebop_prop_debug)
		bop_printf(NULL, "fakebop_getprop: copying %d bytes to 0x%x\n",
		    p->bp_vlen, value);
	bcopy(p->bp_value, value, p->bp_vlen);
	return (0);
}

void
bop_printf(bootops_t *bop, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	(void) vsnprintf(buffer, BUFFERSIZE, fmt, ap);
	va_end(ap);
	bcons_puts(buffer);
}

static void
fakebop_setprop(char *name, int nlen, void *value, int vlen)
{
	size_t size;
	bootprop_t *bp;
	caddr_t cur;

	size = sizeof (bootprop_t) + nlen + 1 + vlen;
	cur = fakebop_alloc(NULL, NULL, size, BO_ALIGN_DONTCARE);
	bp = (bootprop_t *)cur;
	if (bprops == NULL) {
		bprops = bp;
		bp->bp_next = NULL;
	} else {
		bp->bp_next = bprops;
		bprops = bp;
	}
	cur += sizeof (bootprop_t);
	bp->bp_name = cur;
	bcopy(name, cur, nlen);
	cur += nlen;
	*cur = '\0';
	cur++;
	bp->bp_vlen = vlen;
	bp->bp_value = cur;
	if (vlen > 0)
		bcopy(value, cur, vlen);

	if (fakebop_prop_debug)
		bop_printf(NULL, "setprop - name: %s, nlen: %d, vlen: %d\n",
		    bp->bp_name, nlen, bp->bp_vlen);
}

static void
fakebop_setprop_string(char *name, char *value)
{
	if (fakebop_prop_debug)
		bop_printf(NULL, "setprop_string: %s->[%s]\n", name, value);
	fakebop_setprop(name, strlen(name), value, strlen(value) + 1);
}

static void
fakebop_setprop_32(char *name, uint32_t value)
{
	if (fakebop_prop_debug)
		bop_printf(NULL, "setprop_32: %s->[%d]\n", name, value);
	fakebop_setprop(name, strlen(name), (void *)&value, sizeof (value));
}

static void
fakebop_setprop_64(char *name, uint64_t value)
{
	if (fakebop_prop_debug)
		bop_printf(NULL, "setprop_64: %s->[%lld]\n", name, value);
	fakebop_setprop(name, strlen(name), (void *)&value, sizeof (value));
}

/*
 * Here we create a bunch of the initial boot properties. This includes what
 * we've been passed in via the command line. It also includes a few values that
 * we compute ourselves.
 */
static void
fakebop_bootprops_init(void)
{
	int i = 0, proplen = 0, cmdline_len = 0, quote, cont;
	static int stdout_val = 0;
	char *c, *prop, *cmdline, *pname;
	bootinfo_t *bp = &bootinfo;

	/*
	 * Set the ramdisk properties for kobj_boot_mountroot() can succeed.
	 */
	if ((bp->bi_flags & BI_HAS_RAMDISK) != 0) {
		fakebop_setprop_64("ramdisk_start",
		    (uint64_t)(uintptr_t)bp->bi_ramdisk);
		fakebop_setprop_64("ramdisk_end",
		    (uint64_t)(uintptr_t)bp->bi_ramdisk + bp->bi_ramsize);
	}

	/*
	 * TODO Various arm devices may spit properties at the front just like
	 * i86xpv. We should do something about them at some point.
	 */

	/*
	 * Our boot parameters always wil start with kernel /platform/..., but
	 * the bootloader may in fact stick other properties in front of us. To
	 * deal with that we go ahead and we include them. We'll find the kernel
	 * and our properties set via -B when we finally find something without
	 * an equals sign, mainly kernel.
	 */
	c = bp->bi_cmdline;
	prop = strstr(c, "kernel");
	if (prop == NULL)
		bop_panic("failed to find kernel string in boot params!");
	/* Get us past the first kernel string */
	prop += 6;
	while (ISSPACE(prop[0]))
		prop++;
	proplen = 0;
	while (prop[proplen] != '\0' && !ISSPACE(prop[proplen]))
		proplen++;
	c = prop + proplen + 1;
	if (proplen > 0) {
		prop[proplen] = '\0';
		fakebop_setprop_string("boot-file", prop);
		/*
		 * We strip the leading path from whoami so no matter what
		 * environment we enter into here from it is consistent and
		 * makes some amount of sense.
		 */
		if (strstr(prop, "/platform") != NULL)
			prop = strstr(prop, "/platform");
		fakebop_setprop_string("whoami", prop);
	} else {
		bop_panic("no kernel string in boot params!");
	}

	/*
	 * At this point we have two different sets of properties. Anything that
	 * involves -B is a boot property, otherwise it becomes part of the
	 * kernel command line and must be saved in its own property.
	 */
	cmdline = fakebop_alloc(NULL, NULL, strlen(c), BO_ALIGN_DONTCARE);
	cmdline[0] = '\0';
	while (*c != '\0') {

		/*
		 * Just blindly copy it to the commadline if we don't find it.
		 */
		if (c[0] != '-' || c[1] != 'B') {
			cmdline[cmdline_len++] = *c;
			cmdline[cmdline_len] = '\0';
			c++;
			continue;
		}

		/* Get past "-B" */
		c += 2;
		while (ISSPACE(*c))
			c++;

		/*
		 * We have a series of comma separated key-value pairs to sift
		 * through here. The key and value are separated by an equals
		 * sign. The value may quoted with either a ' or ". Note, white
		 * space will also end the value (as it indicates that we have
		 * moved on from the -B argument.
		 */
		for (;;) {
			if (*c == '\0' || ISSPACE(*c))
				break;
			prop = strchr(c, '=');
			if (prop == NULL)
				break;
			pname = c;
			*prop = '\0';
			prop++;
			proplen = 0;
			quote = '\0';
			for (;;) {
				if (prop[proplen] == '\0')
					break;

				if (proplen == 0 && (prop[0] == '\'' ||
				    prop[0] == '"')) {
					quote = prop[0];
					proplen++;
					continue;
				}

				if (quote != '\0') {
					if (prop[proplen] == quote)
						quote = '\0';
					proplen++;
					continue;
				}

				if (prop[proplen] == ',' ||
				    ISSPACE(prop[proplen]))
					break;

				/* We just have a normal character */
				proplen++;
			}

			/*
			 * Save whether we should continue or not and update 'c'
			 * now as we will most likely clobber the string when we
			 * are done.
			 */
			cont = (prop[proplen] == ',');
			if (prop[proplen] != '\0')
				c = prop + proplen + 1;
			else
				c = prop + proplen;

			if (proplen == 0) {
				fakebop_setprop_string(pname, "true");
			} else {
				/*
				 * When we copy the prop, do not include the
				 * quote.
				 */
				if (prop[0] == prop[proplen - 1] &&
				    (prop[0] == '\'' || prop[0] == '"')) {
					prop++;
					proplen -= 2;
				}
				prop[proplen] = '\0';
				fakebop_setprop_string(pname, prop);
			}

			if (cont == 0)
				break;
		}
	}

	/*
	 * Yes, we actually set both names here. The latter is set because of
	 * 1275.
	 */
	fakebop_setprop_string("boot-args", cmdline);
	fakebop_setprop_string("bootargs", cmdline);

	/*
	 * Here are some things that we make up, just like our i86pc brethren.
	 */
	fakebop_setprop_32("stdout", 0);
	fakebop_setprop_string("mfg-name", "ARMv7");
	fakebop_setprop_string("impl-arch-name", "ARMv7");
}

/*
 * Nominally this should try and look for bootenv.rc, but seriously, let's not.
 * Instead for now all we're going to to do is look and make sure that console
 * is set. We *should* do something with it, but we're not.
 */
void
boot_prop_finish(void)
{
	int ret;

	if (fakebop_getproplen(NULL, "console") <= 0)
		bop_panic("console not set");
}

/*ARGSUSED*/
int
boot_compinfo(int fd, struct compinfo *cbp)
{
	cbp->iscmp = 0;
	cbp->blksize = MAXBSIZE;
	return (0);
}

extern void (*exception_table[])(void);

void
primordial_handler(void)
{
	bop_panic("TRAP");
}

void
primordial_svc(void *arg)
{
	struct regs *r = (struct regs *)arg;
	uint32_t *insaddr = (uint32_t *)(r->r_lr - 4);

	bop_printf(NULL, "TRAP: svc #%d from %p\n", *insaddr & 0x00ffffff,
	    insaddr);
}

void
primordial_undef(void *arg)
{
	struct regs *r = (struct regs *)arg;
	uint32_t *insaddr = (uint32_t *)(r->r_lr - 4);

	bop_printf(NULL, "TRAP: undefined instruction %x at %p\n",
	    *insaddr, insaddr);
}

void
primordial_reset(void *arg)
{
	struct regs *r = (struct regs *)arg;
	uint32_t *insaddr = (uint32_t *)(r->r_lr - 4);

	bop_printf(NULL, "TRAP: reset from %p\n",
	    insaddr);
	bop_panic("cannot recover from reset\n");
}

void
primordial_prefetchabt(void *arg)
{
	struct regs *r = (struct regs *)arg;
	uint32_t *insaddr = (uint32_t *)(r->r_lr - 4);
	uint32_t bkptno = ((*insaddr & 0xfff00) >> 4) | (*insaddr & 0xf);

	bop_printf(NULL, "TRAP: prefetch (or bkpt #%d) at %p\n",
	    bkptno, insaddr);
}

/*
 * XXX: This can't be tested without the MMU on, I don't think
 *
 * So while I'm sure (surely) we can decode this into a useful "what went
 * wrong and why", I have no idea how, and couldn't test it if I did.
 *
 * I will say that I currently have the awful feeling that it would require an
 * instruction decoder to decode *insaddr and find the memory refs...
 */
void
primordial_dataabt(void *arg)
{
	struct regs *r = (struct regs *)arg;
	/* XXX: Yes, really +8 see ARM A2.6.6 */
	uint32_t *insaddr = (uint32_t *)(r->r_lr - 8);

	bop_printf(NULL, "TRAP: data abort at (insn) %p\n", insaddr);
	bop_printf(NULL, "r0: %08x\tr1: %08x\n", r->r_r0, r->r_r1);
	bop_printf(NULL, "r2: %08x\tr3: %08x\n", r->r_r2, r->r_r3);
	bop_printf(NULL, "r4: %08x\tr5: %08x\n", r->r_r4, r->r_r5);
	bop_printf(NULL, "r6: %08x\tr7: %08x\n", r->r_r6, r->r_r7);
	bop_printf(NULL, "r8: %08x\tr9: %08x\n", r->r_r8, r->r_fp);
	bop_panic("Page Fault! Go fix it\n");
}

void
bad_interrupt(void *arg)
{
	bop_panic("Interrupt with VE == 0 (non-vectored)\n");
}

/* XXX: This should probably be somewhere else */
void (*trap_table[ARM_EXCPT_NUM])(void *) = {
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL
};

/*
 * Welcome to the kernel. We need to make a fake version of the boot_ops and the
 * boot_syscalls and then jump our way to _kobj_boot(). Here, we're borrowing
 * the Linux bootloader expectations, mostly because a lot of bootloaders and
 * boards already do this. If it turns out that we want to abstract this in the
 * future, then we should have locore.s do that before we get here.
 */
void
_fakebop_start(void *zeros, uint32_t machid, void *tagstart)
{
	atag_illumos_status_t *aisp;
	bootinfo_t *bip = &bootinfo;
	bootops_t *bops = &bootop;
	extern void _kobj_boot();

	fakebop_getatags(tagstart);
	bcons_init(bip->bi_cmdline);

	/* Now that we have a console, we can usefully handle traps */
	trap_table[ARM_EXCPT_RESET] = primordial_reset;
	trap_table[ARM_EXCPT_UNDINS] = primordial_undef;
	trap_table[ARM_EXCPT_SVC] = primordial_svc;
	trap_table[ARM_EXCPT_PREFETCH] = primordial_prefetchabt;
	trap_table[ARM_EXCPT_DATA] = primordial_dataabt;
	trap_table[ARM_EXCPT_IRQ] = bad_interrupt;
	trap_table[ARM_EXCPT_FIQ] = bad_interrupt;

	bop_printf(NULL, "Testing some exceptions\n");
	__asm__ __volatile__(".word 0xffffffff");
	__asm__ __volatile__("svc #14");
	/* the high and low bit in each field, so we can check the decode */
	__asm__ __volatile__("bkpt #32793");

	/* Clear some lines from the bootloader */
	bop_printf(NULL, "\nWelcome to fakebop -- ARM edition\n");
	if (fakebop_atag_debug != 0)
		fakebop_dump_tags(tagstart);

	aisp = (atag_illumos_status_t *)atag_find(tagstart,
	    ATAG_ILLUMOS_STATUS);
	if (aisp == NULL)
		bop_panic("missing ATAG_ILLUMOS_STATUS!\n");

	/*
	 * Fill in the bootops vector
	 */
	bops->bsys_version = BO_VERSION;
	bops->bsys_alloc = fakebop_alloc;
	bops->bsys_free = fakebop_free;
	bops->bsys_getproplen = fakebop_getproplen;
	bops->bsys_getprop = fakebop_getprop;
	bops->bsys_printf = bop_printf;

	armboot_mmu_init(tagstart);
	fakebop_alloc_init(tagstart);
	fakebop_bootprops_init();
	bop_printf(NULL, "booting into _kobj\n");

	/*
	 * krtld uses _stext, _etext, _sdata, and _edata as part of its segment
	 * brks. ARM is a bit different from other versions of unix because we
	 * have our exception vector in the binary at the top of our address
	 * space. As such, we basically pass in values to krtld that represent
	 * what our _etext and _edata would have looked like if not for the
	 * exception vector.
	 */
	_kobj_boot(&bop_sysp, NULL, bops, aisp->ais_stext, aisp->ais_etext,
	    aisp->ais_sdata, aisp->ais_edata, EXCEPTION_ADDRESS);

	bop_panic("Returned from kobj_init\n");
}

void
_fakebop_locore_start(struct boot_syscalls *sysp, struct bootops *bops)
{
	bop_panic("Somehow made it back to fakebop_locore_start...");
}
