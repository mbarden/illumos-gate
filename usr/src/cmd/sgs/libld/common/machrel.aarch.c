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

static void
ld_mach_eflags(Ehdr *ehdr, Ofl_desc *ofl)
{
	/*
	 * XXXAARCH64: We want some kind of compatibility checking for input
	 * flags here, and to bail if we it's wrong.
	 */
	ofl->ofl_dehdr->e_flags |= ehdr->e_flags;
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
			.mr_reloc_table			= NULL,
			.mr_init_rel			= NULL,
			.mr_mach_eflags			= ld_mach_eflags,
			.mr_mach_make_dynamic		= NULL,
			.mr_mach_update_odynamic	= NULL,
			.mr_calc_plt_addr		= NULL,
			.mr_perform_outreloc		= NULL,
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
