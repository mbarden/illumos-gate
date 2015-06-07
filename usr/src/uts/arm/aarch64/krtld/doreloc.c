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

#if defined(_KERNEL)
#include <sys/types.h>
#include "reloc.h"
#else
#define	ELF_TARGET_AARCH64
#if defined(DO_RELOC_LIBLD)
#undef DO_RELOC_LIBLD
#define	DO_RELOC_LIBLD_AARCH64
#endif
#include <stdio.h>
#include "sgs.h"
#include <arm/machdep_arm.h>
#include "libld.h"
#include "reloc.h"
#include "conv.h"
#include "msg.h"
#endif

/*
 * We need to build this code differently when it is used for
 * cross linking:
 *	- Data alignment requirements can differ from those
 *		of the running system, so we can't access data
 *		in units larger than a byte
 *	- We have to include code to do byte swapping when the
 *		target and linker host use different byte ordering,
 *		but such code is a waste when running natively.
 */
#if !defined(DO_RELOC_LIBLD) || defined(__aarch64__)
#define	DORELOC_NATIVE
#endif

const Rel_entry reloc_table[R_AARCH64_NUM] = {
	[R_AARCH64_NONE]		= { 0, FLG_RE_NOTREL, 0, 0, 0 },
	[R_AARCH64_NONE_2]		= { 0, FLG_RE_NOTREL, 0, 0, 0 },
	[R_AARCH64_ABS64]		= { 0, FLG_RE_NOTSUP, 0, 0, 0 },
	[R_AARCH64_ABS32]		= { 0, FLG_RE_NOTSUP, 0, 0, 0 },
	[R_AARCH64_ABS16]		= { 0, FLG_RE_NOTSUP, 0, 0, 0 },
	[R_AARCH64_PREL64]		= { 0, FLG_RE_NOTSUP, 0, 0, 0 },
	[R_AARCH64_PREL32]		= { 0, FLG_RE_NOTSUP, 0, 0, 0 },
	[R_AARCH64_PREL16]		= { 0, FLG_RE_NOTSUP, 0, 0, 0 },
	[R_AARCH64_MOVW_UABS_G0]	= { 0, FLG_RE_NOTSUP, 0, 0, 0 },
	[R_AARCH64_MOVW_UABS_G0_NC]	= { 0, FLG_RE_NOTSUP, 0, 0, 0 },
	[R_AARCH64_MOVW_UABS_G1]	= { 0, FLG_RE_NOTSUP, 0, 0, 0 },
	[R_AARCH64_MOVW_UABS_G1_NC]	= { 0, FLG_RE_NOTSUP, 0, 0, 0 },
	[R_AARCH64_MOVW_UABS_G2]	= { 0, FLG_RE_NOTSUP, 0, 0, 0 },
	[R_AARCH64_MOVW_UABS_G2_NC]	= { 0, FLG_RE_NOTSUP, 0, 0, 0 },
	[R_AARCH64_MOVW_UABS_G3]	= { 0, FLG_RE_NOTSUP, 0, 0, 0 },
	[R_AARCH64_MOVW_SABS_G0]	= { 0, FLG_RE_NOTSUP, 0, 0, 0 },
	[R_AARCH64_MOVW_SABS_G1]	= { 0, FLG_RE_NOTSUP, 0, 0, 0 },
	[R_AARCH64_MOVW_SABS_G2]	= { 0, FLG_RE_NOTSUP, 0, 0, 0 },
	[R_AARCH64_LD_PREL_LO19]	= { 0, FLG_RE_NOTSUP, 0, 0, 0 },
	[R_AARCH64_ADR_PREL_LO21]	= { 0, FLG_RE_NOTSUP, 0, 0, 0 },
	[R_AARCH64_ADR_PREL_PG_HI21]	= { 0, FLG_RE_NOTSUP, 0, 0, 0 },
	[R_AARCH64_ADR_PREL_PG_HI21_NC]	= { 0, FLG_RE_NOTSUP, 0, 0, 0 },
	[R_AARCH64_ADD_ABS_LO12_NC]	= { 0, FLG_RE_NOTSUP, 0, 0, 0 },
	[R_AARCH64_LDST8_ABS_LO12_NC]	= { 0, FLG_RE_NOTSUP, 0, 0, 0 },
	[R_AARCH64_TSTBR14]		= { 0, FLG_RE_NOTSUP, 0, 0, 0 },
	[R_AARCH64_CONDBR19]		= { 0, FLG_RE_NOTSUP, 0, 0, 0 },

	[R_AARCH64_JUMP26]		= { 0, FLG_RE_NOTSUP, 0, 0, 0 },
	[R_AARCH64_CALL26]		= { 0x03ffffff, FLG_RE_SIGN|FLG_RE_VERIFY|FLG_RE_PCREL, 4, 2, 0 },
	[R_AARCH64_LDST16_ABS_LO12_NC]	= { 0, FLG_RE_NOTSUP, 0, 0, 0 },
	[R_AARCH64_LDST32_ABS_LO12_NC]	= { 0, FLG_RE_NOTSUP, 0, 0, 0 },
	[R_AARCH64_LDST64_ABS_LO12_NC]	= { 0, FLG_RE_NOTSUP, 0, 0, 0 },
	[R_AARCH64_PREL_G0]		= { 0, FLG_RE_NOTSUP, 0, 0, 0 },
	[R_AARCH64_PREL_G0_NC]		= { 0, FLG_RE_NOTSUP, 0, 0, 0 },
	[R_AARCH64_PREL_G1]		= { 0, FLG_RE_NOTSUP, 0, 0, 0 },
	[R_AARCH64_PREL_G1_NC]		= { 0, FLG_RE_NOTSUP, 0, 0, 0 },
	[R_AARCH64_PREL_G2]		= { 0, FLG_RE_NOTSUP, 0, 0, 0 },
	[R_AARCH64_PREL_G2_NC]		= { 0, FLG_RE_NOTSUP, 0, 0, 0 },
	[R_AARCH64_PREL_G3]		= { 0, FLG_RE_NOTSUP, 0, 0, 0 },

	[R_AARCH64_LDST128_ABS_LO12_NC]	= { 0, FLG_RE_NOTSUP, 0, 0, 0 },
	[R_AARCH64_GOTOFF_G0]		= { 0, FLG_RE_NOTSUP, 0, 0, 0 },
	[R_AARCH64_GOTOFF_G0_NC]	= { 0, FLG_RE_NOTSUP, 0, 0, 0 },
	[R_AARCH64_GOTOFF_G1]		= { 0, FLG_RE_NOTSUP, 0, 0, 0 },
	[R_AARCH64_GOTOFF_G1_NC]	= { 0, FLG_RE_NOTSUP, 0, 0, 0 },
	[R_AARCH64_GOTOFF_G2]		= { 0, FLG_RE_NOTSUP, 0, 0, 0 },
	[R_AARCH64_GOTOFF_G2_NC]	= { 0, FLG_RE_NOTSUP, 0, 0, 0 },
	[R_AARCH64_GOTOFF_G3]		= { 0, FLG_RE_NOTSUP, 0, 0, 0 },
	[R_AARCH64_GOTREL64]		= { 0, FLG_RE_NOTSUP, 0, 0, 0 },
	[R_AARCH64_GOTREL32]		= { 0, FLG_RE_NOTSUP, 0, 0, 0 },
	[R_AARCH64_GOT_LD_PREL19]	= { 0, FLG_RE_NOTSUP, 0, 0, 0 },
	[R_AARCH64_LD64_GOTOFF_LO15]	= { 0, FLG_RE_NOTSUP, 0, 0, 0 },
	[R_AARCH64_ADR_GOT_PAGE]	= { 0, FLG_RE_NOTSUP, 0, 0, 0 },
	[R_AARCH64_LD64_GOT_LO12_NC]	= { 0, FLG_RE_NOTSUP, 0, 0, 0 },
	[R_AARCH64_LD64_GOTPAGE_LO15]	= { 0, FLG_RE_NOTSUP, 0, 0, 0 },

	[R_AARCH64_COPY]		= { 0, FLG_RE_NOTSUP, 0, 0, 0 },
	[R_AARCH64_GLOB_DAT]		= { 0, FLG_RE_NOTSUP, 0, 0, 0 },
	[R_AARCH64_JUMP_SLOT]		= { 0, FLG_RE_NOTSUP, 0, 0, 0 },
	[R_AARCH64_RELATIVE]		= { 0, FLG_RE_NOTSUP, 0, 0, 0 },
	[R_AARCH64_TLS_DTPREL64]	= { 0, FLG_RE_NOTSUP, 0, 0, 0 },
	[R_AARCH64_TLS_PTPMOD64]	= { 0, FLG_RE_NOTSUP, 0, 0, 0 },
	[R_AARCH64_TLS_TPREL64]		= { 0, FLG_RE_NOTSUP, 0, 0, 0 },
	[R_AARCH64_TLSDESC]		= { 0, FLG_RE_NOTSUP, 0, 0, 0 },
	[R_AARCH64_IRELATIVE]		= { 0, FLG_RE_NOTSUP, 0, 0, 0 },
};

/*
 * Write a single relocated value to its reference location. We assume we
 * wish to add the relocation amount, value, to the value of the adress
 * already present at the offset.
 *
 * NAME			VALUE	FIELD	CALCULATION
 * R_AARCH64_NONE	0	none	none
 * R_AARCH64_ABS64      257	?	S + A
 * R_AARCH64_CALL26	283	imm26	S + A - P
 *
 * This is from Tables 4-6 & 4-11, ELF for the ARM 64-bit architecture
 * (AArch64), ARM IHI 0056B.
 *
 * Relocation calculations:
 *
 * CALCULATION uses the following notation:
 *	A	the addend used
 *	P	the place being relocated
 *
 * The calculations in the CALCULATION column are assumed to have been
 * performed before calling this function except for the addition of the
 * addresses in the instructions.
 */
#if defined(_KERNEL)
#define	lml	0		/* Needed by arglist of REL_ERR_* macros */
int
do_reloc_krtld(uchar_t rtype, uchar_t *off, Xword *value, const char *sym,
    const char *file)
#elif defined(DO_RELOC_LIBLD)
int
do_reloc_ld(Rel_desc *rdesc, uchar_t *off, Xword *value,
    rel_desc_sname_func_t rel_desc_sname_func, const char *file,
    int bswap, void *lml)
#else
int
do_reloc_rtld(uchar_t rtype, uchar_t *off, Xword *value,
    const char *sym, const char *file, void *lml)
#endif
{
	const Rel_entry *rep;
#ifdef DO_RELOC_LIBLD
#define	sym (*rel_desc_sname_func)(rdesc)
	uint_t		rtype = rdesc->rel_rtype;
#endif
	Xword		base = 0, uvalue = 0;

	rep = &reloc_table[rtype];

	switch (rep->re_fsize) {
	case 4:
#if defined(DORELOC_NATIVE)
		base = *(Xword *)off;
#else
		if (bswap) {
			uchar_t *b_bytes = (uchar_t *)&base;
			UL_ASSIGN_BSWAP_WORD(b_bytes, off);
		} else {
			uchar_t *b_bytes = (uchar_t *)&base;
			UL_ASSIGN_WORD(b_bytes, off);
		}
#endif
		break;
	default:
		REL_ERR_UNSUPSZ(lml, file, sym, rtype, rep->re_fsize);
		abort();
		return (0);
	}

	uvalue = *value;
	uvalue >>= rep->re_bshift;

	/*
	 * Masked values are masked both in and out
	 * for convenience, base is trunced.
	 *
	 * XXXARM: This of course means that any masked relocation is a
	 * replacement not masked addition.  I sure _hope_ that's true...
	 */
	if (rep->re_mask != 0) {
		uvalue &= rep->re_mask;
		base &= ~rep->re_mask;
	}

	/* Write the result */
	switch (rep->re_fsize) {
	case 4:
#if defined(DORELOC_NATIVE)
		*(Xword *)off = (Xword)(base + uvalue);
#else
		if (bswap) {
			uchar_t *b_bytes = (uchar_t *)&base;
			base += uvalue;
			UL_ASSIGN_BSWAP_WORD(off, b_bytes);
		} else {
			uchar_t *b_bytes = (uchar_t *)&base;
			base += uvalue;
			UL_ASSIGN_WORD(off, b_bytes);
		}
#endif
		break;
	}


	return (1);
#ifdef DO_RELOC_LIBLD
#undef sym
#endif
}
