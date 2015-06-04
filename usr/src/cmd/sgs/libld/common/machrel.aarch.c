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
 * Portions:
 * Copyright (c) 1989, 2010, Oracle and/or its affiliates. All rights reserved.
 */

/*
 * XXXAARCH64: Luckily, to some degree, a lot of the target-specific things in the
 * link-editor aren't _exactly_ target specific, and a reasonable
 * approximation of them can be derived from the implementations of the other
 * targets.  That is what we're doing in this file right now -- Large chunks
 * are directly derived from the intel implementation.
 *
 * It is possible, and in fact likely, that I have misunderstood the
 * commonality of various pieces of this with the intel implementation and in
 * doing so have introduced bugs.
 *
 * I should also state that the comments describing various functions are
 * actually describing my understanding thereof.  It is not unlikely that my
 * understanding is flawed.
 */

#define	DO_RELOC_LIBLD_AARCH64

#include	<sys/elf_aarch64.h>
#include	<stdio.h>
#include	<debug.h>
#include	<reloc.h>
#include	<arm/machdep_arm.h>
#include	"msg.h"
#include	"_libld.h"

/*
 * genunix needs these relocations:
 *     R_AARCH64_ADR_PREL_PG_HI21
 *     R_AARCH64_ADD_ABS_LO12_NC
 *     R_AARCH64_CALL26
 *     R_AARCH64_LDST64_ABS_LO12_NC
 *     R_AARCH64_ABS64
 */

static Word
ld_init_rel(Rel_desc *reld, Word *typedata, void *reloc)
{
	Rel	*rel = (Rel *)reloc;

	reld->rel_rtype = (Word)ELF_R_TYPE(rel->r_info, M_MACH);
	reld->rel_roffset = rel->r_offset;
	reld->rel_raddend = 0;
	*typedata = (Word)ELF_R_TYPE_DATA(rel->r_info);

	return ((Word)ELF_R_SYM(rel->r_info));
}

static void
ld_mach_eflags(Ehdr *ehdr, Ofl_desc *ofl)
{
	/*
	 * XXXAARCH64: We want some kind of compatibility checking for input
	 * flags here, and to bail if we it's wrong.
	 */
	ofl->ofl_dehdr->e_flags |= ehdr->e_flags;
}

static void
ld_mach_make_dynamic(Ofl_desc *ofl, size_t *cnt)
{
	if (!(ofl->ofl_flags & FLG_OF_RELOBJ)) {
		/* Create this entry if we are going to create a PLT. */
		if (ofl->ofl_pltcnt > 0)
			(*cnt)++; /* DT_PLTGOT */
	}
}

/* ARGSUSED */
static Gotndx *
ld_find_got_ndx(Alist *alp, Gotref gref, Ofl_desc *ofl, Rel_desc *rdesc)
{
	Aliste	indx;
	Gotndx	*gnp;

	if ((gref == GOT_REF_TLSLD) && ofl->ofl_tlsldgotndx)
		return (ofl->ofl_tlsldgotndx);

	for (ALIST_TRAVERSE(alp, indx, gnp)) {
		if (gnp->gn_gotref == gref)
			return (gnp);
	}

	return (NULL);
}

static Xword
ld_calc_got_offset(Rel_desc *rdesc, Ofl_desc *ofl)
{
	Os_desc		*osp = ofl->ofl_osgot;
	Sym_desc	*sdp = rdesc->rel_sym;
	Xword		gotndx;
	Gotref		gref;
	Gotndx		*gnp;

	if (rdesc->rel_flags & FLG_REL_DTLS)
		gref = GOT_REF_TLSGD;
	else if (rdesc->rel_flags & FLG_REL_MTLS)
		gref = GOT_REF_TLSLD;
	else if (rdesc->rel_flags & FLG_REL_STLS)
		gref = GOT_REF_TLSIE;
	else
		gref = GOT_REF_GENERIC;

	gnp = ld_find_got_ndx(sdp->sd_GOTndxs, gref, ofl, NULL);
	assert(gnp);

	gotndx = (Xword)gnp->gn_gotndx;

#if 0
	/* XXX AARCH64: this is from ARM, does it apply? */
	if ((rdesc->rel_flags & FLG_REL_DTLS) &&
	    (rdesc->rel_rtype == R_ARM_TLS_DTPOFF32))
		gotndx++;
#endif

	return ((Xword)(osp->os_shdr->sh_addr + (gotndx * M_GOT_ENTSIZE)));
}

/*
 * Build a single PLT entry.  See the comment for ld_fillin_pltgot() for a
 * more complete description.
 */
/* ARGSUSED */
static void
plt_entry(Ofl_desc *ofl, Sym_desc *sdp)
{
	uchar_t	*pltent, *gotent;
	Word	plt_off;
	Word	got_off;
	Word	got_disp;
	Boolean	bswap = (ofl->ofl_flags1 & FLG_OF1_ENCDIFF) != 0;
	Addr	got_addr, plt_addr;

	got_off = sdp->sd_aux->sa_PLTGOTndx * M_GOT_ENTSIZE;
	plt_off = M_PLT_RESERVSZ + ((sdp->sd_aux->sa_PLTndx - 1) *
	    M_PLT_ENTSIZE);

	pltent = (uchar_t *)(ofl->ofl_osplt->os_outdata->d_buf) + plt_off;
	gotent = (uchar_t *)(ofl->ofl_osgot->os_outdata->d_buf) + got_off;

	/*
	 * XXX AARCH64: update to spit out aarch64 instructions
	 */
	got_addr = ofl->ofl_osgot->os_shdr->sh_addr + got_off;
	plt_addr = ofl->ofl_osplt->os_shdr->sh_addr + plt_off;
	got_disp = got_addr - (plt_addr + 8); /* adjusted for %pc offset */

	/* LINTED */
	*(Word *)gotent = ofl->ofl_osplt->os_shdr->sh_addr;
	if (bswap)
		/* LINTED */
		*(Word *)gotent = ld_bswap_Word(*(Word *)gotent);

	/* add ip, pc, #0 | ...  */
	/* LINTED */
	*(Word *)pltent = 0xe28fc600 | ((got_disp & 0xfff00000) >> 20);
	if (bswap)
		/* LINTED */
		*(Word *)pltent = ld_bswap_Word(*(Word *)pltent);
	pltent += M_PLT_INSSIZE;

	/* add ip, ip, #0 | ... */
	/* LINTED */
	*(Word *)pltent = 0xe28cca00 | ((got_disp & 0x000ff000) >> 12);
	if (bswap)
		/* LINTED */
		*(Word *)pltent = ld_bswap_Word(*(Word *)pltent);
	pltent += M_PLT_INSSIZE;

	/* ldr pc, [ip, #0]! | ...  */
	/* LINTED */
	*(Word *)pltent = 0xe5bcf000 | (got_disp & 0x00000fff);
	if (bswap)
		/* LINTED */
		*(Word *)pltent = ld_bswap_Word(*(Word *)pltent);
	pltent += M_PLT_INSSIZE;
}

/*
 * Insert an appropriate dynamic relocation into the output image in the
 * appropriate relocation section.
 *
 * Primarily, this is not particularly target-specific, and involves
 * calculating the correct offset for the relocation entry to be written, and
 * accounting for some complicated edge cases.
 *
 * Heavily taken from the Intel implementation.
 */
static uintptr_t
ld_perform_outreloc(Rel_desc *orsp, Ofl_desc *ofl, Boolean *remain_seen)
{
	Os_desc		*relosp, *osp = NULL;
	Word		ndx, roffset, value;
	Rel		rea;
	char		*relbits;
	Sym_desc	*sdp, *psym = NULL;
	Boolean		sectmoved = FALSE;

	sdp = orsp->rel_sym;

	/*
	 * If the section this relocation is against has been dicarded
	 * (-zignore), then also discard the relocation itself.
	 */
	if ((orsp->rel_isdesc != NULL) && ((orsp->rel_flags &
	    (FLG_REL_GOT | FLG_REL_BSS | FLG_REL_PLT | FLG_REL_NOINFO)) == 0) &&
	    (orsp->rel_isdesc->is_flags & FLG_IS_DISCARD)) {
		DBG_CALL(Dbg_reloc_discard(ofl->ofl_lml, M_MACH, orsp));
		return (1);
	}

	/*
	 * If this is a relocation against a move table, or expanded move
	 * table, adjust the relocation entries.
	 */
	if (RELAUX_GET_MOVE(orsp) != NULL)
		ld_adj_movereloc(ofl, orsp);

	/*
	 * If this is a relocation against a section using a partially
	 * initialized symbol, adjust the embedded symbol info.
	 *
	 * The second argument of the am_I_partial() is the value stored at
	 * the target address to which the relocation is going to be
	 * applied.
	 */
	if (ELF_ST_TYPE(sdp->sd_sym->st_info) == STT_SECTION) {
		if (ofl->ofl_parsyms &&
		    (sdp->sd_isc->is_flags & FLG_IS_RELUPD) &&
		    /* LINTED */
		    (psym = ld_am_I_partial(orsp, *(Xword *)((uchar_t *)
		    (orsp->rel_isdesc->is_indata->d_buf) +
		    orsp->rel_roffset)))) {
			DBG_CALL(Dbg_move_outsctadj(ofl->ofl_lml, psym));
			sectmoved = TRUE;
		}
	}

	value = sdp->sd_sym->st_value;

	if (orsp->rel_flags & FLG_REL_GOT) {
		osp = ofl->ofl_osgot;
		roffset = (Word)ld_calc_got_offset(orsp, ofl);
	} else if (orsp->rel_flags & FLG_REL_PLT) {
		/*
		 * Note that relocations for PLTs actually cause a relocation
		 * against the GOT
		 */
		osp = ofl->ofl_osplt;
		roffset = (Word) (ofl->ofl_osgot->os_shdr->sh_addr) +
		    sdp->sd_aux->sa_PLTGOTndx * M_GOT_ENTSIZE;

		plt_entry(ofl, sdp);
	} else if (orsp->rel_flags & FLG_REL_BSS) {
		/*
		 * This must be an R_AARCH64_COPY.  For these set the
		 * roffset to point to the new symbol's location.
		 */
		osp = ofl->ofl_isbss->is_osdesc;
		roffset = (Word)value;
	} else {
		osp = RELAUX_GET_OSDESC(orsp);

		/*
		 * Calculate virtual offset of reference point; equals offset
		 * into section + vaddr of section for loadable sections, or
		 * offset plus section displacement for nonloadable
		 * sections.
		 */
		roffset = orsp->rel_roffset +
		    (Off)_elf_getxoff(orsp->rel_isdesc->is_indata);
		if (!(ofl->ofl_flags & FLG_OF_RELOBJ))
			roffset += orsp->rel_isdesc->is_osdesc->
			    os_shdr->sh_addr;
	}

	if ((osp == NULL) || ((relosp = osp->os_relosdesc) == NULL)) {
		relosp = ofl->ofl_osrel;
	}

	/*
	 * Assign the symbols index for the output relocation.  If the
	 * relocation refers to a SECTION symbol then it's index is based upon
	 * the output sections symbols index.  Otherwise the index can be
	 * derived from the symbols index itself.
	 */
	if (orsp->rel_rtype == R_AARCH64_RELATIVE) {
		ndx = STN_UNDEF;
	} else if ((orsp->rel_flags & FLG_REL_SCNNDX) ||
	    (ELF_ST_TYPE(sdp->sd_sym->st_info) == STT_SECTION)) {
		if (sectmoved == FALSE) {
			/*
			 * Check for a null input section.  This can occur if
			 * this relocation references a symbol generated by
			 * sym_add_sym()
			 */
			if ((sdp->sd_isc != NULL) &&
			    (sdp->sd_isc->is_osdesc != NULL)) {
				ndx = sdp->sd_isc->is_osdesc->os_identndx;
			} else {
				ndx = sdp->sd_shndx;
			}
		} else {
			ndx = ofl->ofl_parexpnndx;
		}
	} else {
		ndx = sdp->sd_symndx;
	}

	/*
	 * If we have a replacement value for the relocation target, put it in
	 * place now.
	 */
	if (orsp->rel_flags & FLG_REL_NADDEND) {
		Xword	addend = orsp->rel_raddend;
		uchar_t	*addr;

		/*
		 * Get the address of the data item we nede to modify.
		 */
		addr = (uchar_t *)((uintptr_t)orsp->rel_roffset +
		    (uintptr_t)_elf_getxoff(orsp->rel_isdesc->is_indata));
		addr += (uintptr_t)RELAUX_GET_OSDESC(orsp)->os_outdata->d_buf;
		if (ld_reloc_targval_set(ofl, orsp, addr, addend) == 0) {
			return (S_ERROR);
		}
	}

	relbits = (char *)relosp->os_outdata->d_buf;

	rea.r_info = ELF_R_INFO(ndx, orsp->rel_rtype);
	rea.r_offset = roffset;
	DBG_CALL(Dbg_reloc_out(ofl, ELF_DBG_LD, SHT_REL, &rea, relosp->os_name,
	    ld_reloc_sym_name(orsp)));

	/* Assert we haven't walked off the end of our relocation table. */
	assert(relosp->os_szoutrels <= relosp->os_shdr->sh_size);

	(void) memcpy((relbits + relosp->os_szoutrels),
	    (char *)&rea, sizeof (Rel));
	relosp->os_szoutrels += sizeof (Rel);

	/*
	 * Determine if this relocation is against a non-writable, allocatable
	 * section.  If so we may need to provide a text relocation
	 * diagnostic.
	 *
	 * Note that relocations against the .plt (R_AARCH64_JUMP_SLOT)
	 * actually result in modifications to the .got
	 */
	if (orsp->rel_rtype == R_AARCH64_JUMP_SLOT)
		osp = ofl->ofl_osgot;

	ld_reloc_remain_entry(orsp, osp, ofl, remain_seen);
	return (1);
}

static const uchar_t nullfunc_tmpl[] = {
	0x00, 0x00, 0x00, 0x00,
};

const Target *
ld_targ_init_aarch64(void)
{
	static const Target _ld_targ = {
		.t_m = {
			.m_mach			= M_MACH,
			.m_machplus		= M_MACHPLUS,
			.m_flagsplus		= M_FLAGSPLUS,
			.m_class		= M_CLASS,
			.m_data			= M_DATA,

			.m_segm_align		= M_SEGM_ALIGN,
			.m_segm_origin		= M_SEGM_ORIGIN,
			.m_segm_aorigin		= M_SEGM_AORIGIN,
			.m_dataseg_perm		= M_DATASEG_PERM,
			.m_stack_perm		= M_STACK_PERM,
			.m_word_align		= M_WORD_ALIGN,
			.m_def_interp		= MSG_ORIG(MSG_PTH_RTLD),

			.m_r_arrayaddr		= M_R_ARRAYADDR,
			.m_r_copy		= M_R_COPY,
			.m_r_glob_dat		= M_R_GLOB_DAT,
			.m_r_jmp_slot		= M_R_JMP_SLOT,
			.m_r_num		= M_R_NUM,
			.m_r_none		= M_R_NONE,
			.m_r_relative		= M_R_RELATIVE,
			.m_r_register		= M_R_REGISTER,

			.m_rel_dt_count		= M_REL_DT_COUNT,
			.m_rel_dt_ent		= M_REL_DT_ENT,
			.m_rel_dt_size		= M_REL_DT_SIZE,
			.m_rel_dt_type		= M_REL_DT_TYPE,
			.m_rel_sht_type		= M_REL_SHT_TYPE,

			.m_got_entsize		= M_GOT_ENTSIZE,
			.m_got_xnumber		= M_GOT_XNumber,

			.m_plt_align		= M_PLT_ALIGN,
			.m_plt_entsize		= M_PLT_ENTSIZE,
			.m_plt_reservsz		= M_PLT_RESERVSZ,
			.m_plt_shf_flags	= M_PLT_SHF_FLAGS,

			.m_sht_unwind		= SHT_PROGBITS,

			.m_dt_register		= M_DT_REGISTER
		},
		.t_id = {
			.id_array	= M_ID_ARRAY,
			.id_bss		= M_ID_BSS,
			.id_cap		= M_ID_CAP,
			.id_capinfo	= M_ID_CAPINFO,
			.id_capchain	= M_ID_CAPCHAIN,
			.id_data	= M_ID_DATA,
			.id_dynamic	= M_ID_DYNAMIC,
			.id_dynsort	= M_ID_DYNSORT,
			.id_dynstr	= M_ID_DYNSTR,
			.id_dynsym	= M_ID_DYNSYM,
			.id_dynsym_ndx	= M_ID_DYNSYM_NDX,
			.id_got		= M_ID_GOT,
			.id_gotdata	= M_ID_UNKNOWN,
			.id_hash	= M_ID_HASH,
			.id_interp	= M_ID_INTERP,
			.id_lbss	= M_ID_UNKNOWN,
			.id_ldynsym	= M_ID_LDYNSYM,
			.id_note	= M_ID_NOTE,
			.id_null	= M_ID_NULL,
			.id_plt		= M_ID_PLT,
			.id_rel		= M_ID_REL,
			.id_strtab	= M_ID_STRTAB,
			.id_syminfo	= M_ID_SYMINFO,
			.id_symtab	= M_ID_SYMTAB,
			.id_symtab_ndx	= M_ID_SYMTAB_NDX,
			.id_text	= M_ID_TEXT,
			.id_tls		= M_ID_TLS,
			.id_tlsbss	= M_ID_TLSBSS,
			.id_unknown	= M_ID_UNKNOWN,
			.id_unwind	= M_ID_UNWIND,
			.id_unwindhdr	= M_ID_UNWINDHDR,
			.id_user	= M_ID_USER,
			.id_version	= M_ID_VERSION,
		},
		.t_nf = {
			.nf_template	= nullfunc_tmpl,
			.nf_size	= sizeof (nullfunc_tmpl),
		},
		.t_ff = {
			/*
			 * XXXARM: This will use 0x0, which is a nop-ish
			 * andeq.  The _preferred_ nop is mov r0, r0
			 */
			.ff_execfill	= NULL,
		},
		.t_mr = {
			.mr_reloc_table			= reloc_table,
			.mr_init_rel			= ld_init_rel,
			.mr_mach_eflags			= ld_mach_eflags,
			.mr_mach_make_dynamic		= ld_mach_make_dynamic,
			.mr_mach_update_odynamic	= NULL,
			.mr_calc_plt_addr		= NULL,
			.mr_perform_outreloc		= ld_perform_outreloc,
			.mr_do_activerelocs		= NULL,
			.mr_add_outrel			= NULL,
			.mr_reloc_register		= NULL,
			.mr_reloc_local			= NULL,
			.mr_reloc_GOTOP			= NULL,
			.mr_reloc_TLS			= NULL,
			.mr_assign_got			= NULL,
			.mr_find_got_ndx		= NULL,
			.mr_calc_got_offset		= NULL,
			.mr_assign_got_ndx		= NULL,
			.mr_assign_plt_ndx		= NULL,
			.mr_allocate_got		= NULL,
			.mr_fillin_gotplt		= NULL,
		},
		.t_ms = {
			.ms_reg_check		= NULL,
			.ms_mach_sym_typecheck	= NULL,
			.ms_is_regsym		= NULL,
			.ms_reg_find		= NULL,
			.ms_reg_enter		= NULL,
		}
	};

	return (&_ld_targ);
}
