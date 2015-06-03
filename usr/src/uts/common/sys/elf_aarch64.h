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
 * Copyright 2015, Josef 'Jeff' Sipek <jeffpc@josefsipek.net>
 */

#ifndef _SYS_ELF_AARCH64_H
#define	_SYS_ELF_AARCH64_H

#ifdef __cplusplus
extern "C" {
#endif

#define	R_AARCH64_NONE			0
#define	R_AARCH64_NONE_2		256	/* withdrawn, treat as NONE */
#define	R_AARCH64_ABS64			257
#define	R_AARCH64_ABS32			258
#define	R_AARCH64_ABS16			259
#define	R_AARCH64_PREL64		260
#define	R_AARCH64_PREL32		261
#define	R_AARCH64_PREL16		262
#define	R_AARCH64_MOVW_UABS_G0		263
#define	R_AARCH64_MOVW_UABS_G0_NC	264
#define	R_AARCH64_MOVW_UABS_G1		265
#define	R_AARCH64_MOVW_UABS_G1_NC	266
#define	R_AARCH64_MOVW_UABS_G2		267
#define	R_AARCH64_MOVW_UABS_G2_NC	268
#define	R_AARCH64_MOVW_UABS_G3		269
#define	R_AARCH64_MOVW_SABS_G0		270
#define	R_AARCH64_MOVW_SABS_G1		271
#define	R_AARCH64_MOVW_SABS_G2		272
#define	R_AARCH64_LD_PREL_LO19		273
#define	R_AARCH64_ADR_PREL_LO21		274
#define	R_AARCH64_ADR_PREL_PG_HI21	275
#define	R_AARCH64_ADR_PREL_PG_HI21_NC	276
#define	R_AARCH64_ADD_ABS_LO12_NC	277
#define	R_AARCH64_LDST8_ABS_LO12_NC	278
#define	R_AARCH64_TSTBR14		279
#define	R_AARCH64_CONDBR19		280
/* 281 unallocated */
#define	R_AARCH64_JUMP26		282
#define	R_AARCH64_CALL26		283
#define	R_AARCH64_LDST16_ABS_LO12_NC	284
#define	R_AARCH64_LDST32_ABS_LO12_NC	285
#define	R_AARCH64_LDST64_ABS_LO12_NC	286
#define	R_AARCH64_PREL_G0		287
#define	R_AARCH64_PREL_G0_NC		288
#define	R_AARCH64_PREL_G1		289
#define	R_AARCH64_PREL_G1_NC		290
#define	R_AARCH64_PREL_G2		291
#define	R_AARCH64_PREL_G2_NC		292
#define	R_AARCH64_PREL_G3		293
/* 294-298 unallocated */
#define	R_AARCH64_LDST128_ABS_LO12_NC	299
#define	R_AARCH64_GOTOFF_G0		300
#define	R_AARCH64_GOTOFF_G0_NC		301
#define	R_AARCH64_GOTOFF_G1		302
#define	R_AARCH64_GOTOFF_G1_NC		303
#define	R_AARCH64_GOTOFF_G2		304
#define	R_AARCH64_GOTOFF_G2_NC		305
#define	R_AARCH64_GOTOFF_G3		306
#define	R_AARCH64_GOTREL64		307
#define	R_AARCH64_GOTREL32		308
#define	R_AARCH64_GOT_LD_PREL19		309
#define	R_AARCH64_LD64_GOTOFF_LO15	310
#define	R_AARCH64_ADR_GOT_PAGE		311
#define	R_AARCH64_LD64_GOT_LO12_NC	312
#define	R_AARCH64_LD64_GOTPAGE_LO15	313
/* XXX: there are more */
#define	R_AARCH64_COPY			1024
#define	R_AARCH64_GLOB_DAT		1025
#define	R_AARCH64_JUMP_SLOT		1026
#define	R_AARCH64_RELATIVE		1027
#define	R_AARCH64_TLS_DTPREL64		1028
#define	R_AARCH64_TLS_PTPMOD64		1029
#define	R_AARCH64_TLS_TPREL64		1030
#define	R_AARCH64_TLSDESC		1031
#define	R_AARCH64_IRELATIVE		1032

#define	R_AARCH64_NUM			1033

#define	ELF_AARCH64_MAXPGSZ	0x08000 /* XXX: guess */

/* aarch64-specific section types */
#define	SHT_AARCH64_ATTRIBUTES	0x70000003 /* Object compatibility attributes */

#ifdef __cplusplus
}
#endif

#endif /* _SYS_ELF_AARCH64_H */
