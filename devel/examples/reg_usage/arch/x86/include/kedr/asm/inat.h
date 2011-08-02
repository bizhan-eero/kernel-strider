#ifndef _ASM_X86_INAT_H
#define _ASM_X86_INAT_H
/*
 * x86 instruction attributes
 *
 * Written by Masami Hiramatsu <mhiramat@redhat.com>
 *
 * Handling of register usage information was implemented by 
 *  Eugene A. Shatokhin <spectre@ispras.ru>, 2011
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 */
#include "inat_types.h"

/*
 * Internal bits. Don't use bitmasks directly, because these bits are
 * unstable. You should use checking functions.
 */

#define INAT_OPCODE_TABLE_SIZE 256
#define INAT_GROUP_TABLE_SIZE 8

/* Legacy last prefixes */
#define INAT_PFX_OPNDSZ	1	/* 0x66 */ /* LPFX1 */
#define INAT_PFX_REPE	2	/* 0xF3 */ /* LPFX2 */
#define INAT_PFX_REPNE	3	/* 0xF2 */ /* LPFX3 */
/* Other Legacy prefixes */
#define INAT_PFX_LOCK	4	/* 0xF0 */
#define INAT_PFX_CS	5	/* 0x2E */
#define INAT_PFX_DS	6	/* 0x3E */
#define INAT_PFX_ES	7	/* 0x26 */
#define INAT_PFX_FS	8	/* 0x64 */
#define INAT_PFX_GS	9	/* 0x65 */
#define INAT_PFX_SS	10	/* 0x36 */
#define INAT_PFX_ADDRSZ	11	/* 0x67 */
/* x86-64 REX prefix */
#define INAT_PFX_REX	12	/* 0x4X */
/* AVX VEX prefixes */
#define INAT_PFX_VEX2	13	/* 2-bytes VEX prefix */
#define INAT_PFX_VEX3	14	/* 3-bytes VEX prefix */

#define INAT_LSTPFX_MAX	3
#define INAT_LGCPFX_MAX	11

/* Immediate size */
#define INAT_IMM_BYTE		1
#define INAT_IMM_WORD		2
#define INAT_IMM_DWORD		3
#define INAT_IMM_QWORD		4
#define INAT_IMM_PTR		5
#define INAT_IMM_VWORD32	6
#define INAT_IMM_VWORD		7

/* Legacy prefix */
#define INAT_PFX_OFFS	0
#define INAT_PFX_BITS	4
#define INAT_PFX_MAX    ((1 << INAT_PFX_BITS) - 1)
#define INAT_PFX_MASK	(INAT_PFX_MAX << INAT_PFX_OFFS)
/* Escape opcodes */
#define INAT_ESC_OFFS	(INAT_PFX_OFFS + INAT_PFX_BITS)
#define INAT_ESC_BITS	2
#define INAT_ESC_MAX	((1 << INAT_ESC_BITS) - 1)
#define INAT_ESC_MASK	(INAT_ESC_MAX << INAT_ESC_OFFS)
/* Group opcodes (1-16) */
#define INAT_GRP_OFFS	(INAT_ESC_OFFS + INAT_ESC_BITS)
#define INAT_GRP_BITS	5
#define INAT_GRP_MAX	((1 << INAT_GRP_BITS) - 1)
#define INAT_GRP_MASK	(INAT_GRP_MAX << INAT_GRP_OFFS)
/* Immediates */
#define INAT_IMM_OFFS	(INAT_GRP_OFFS + INAT_GRP_BITS)
#define INAT_IMM_BITS	3
#define INAT_IMM_MASK	(((1 << INAT_IMM_BITS) - 1) << INAT_IMM_OFFS)
/* Flags */
#define INAT_FLAG_OFFS	(INAT_IMM_OFFS + INAT_IMM_BITS)
#define INAT_MODRM	(1 << (INAT_FLAG_OFFS))
#define INAT_FORCE64	(1 << (INAT_FLAG_OFFS + 1))
#define INAT_SCNDIMM	(1 << (INAT_FLAG_OFFS + 2))
#define INAT_MOFFSET	(1 << (INAT_FLAG_OFFS + 3))
#define INAT_VARIANT	(1 << (INAT_FLAG_OFFS + 4))
#define INAT_VEXOK	(1 << (INAT_FLAG_OFFS + 5))
#define INAT_VEXONLY	(1 << (INAT_FLAG_OFFS + 6))
#define INAT_FLAG_BITS	7

/* Register usage info that can be deduced from the opcode. 
 * To obtain the complete information, other parts of instruction may be 
 * necessary to investigate (REX prefix, Mod R/M and SIB bytes) .
 * 
 * For register numbers (codes), see Table 3-1 "Register Codes Associated 
 * With +rb, +rw, +rd, +ro" in Intel Software Developer's Manual Vol2A. 
 * 
 * [NB] INAT_USES_REG_* constants are only for internal use in the decoder.
 * The external components should use X86_REG_MASK(INAT_REG_CODE<N>) 
 * instead.*/
#define INAT_REG_CODE_AX	0  /* 0000(b) */
#define INAT_REG_CODE_CX	1  /* 0001(b) */
#define INAT_REG_CODE_DX	2  /* 0010(b) */
#define INAT_REG_CODE_BX	3  /* 0011(b) */
#define INAT_REG_CODE_SP	4  /* 0100(b) */
#define INAT_REG_CODE_BP	5  /* 0101(b) */
#define INAT_REG_CODE_SI	6  /* 0110(b) */
#define INAT_REG_CODE_DI	7  /* 0111(b) */
#define INAT_REG_CODE_8		8  /* 1000(b) */
#define INAT_REG_CODE_9		9  /* 1001(b) */
#define INAT_REG_CODE_10	10 /* 1010(b) */
#define INAT_REG_CODE_11	11 /* 1011(b) */
#define INAT_REG_CODE_12	12 /* 1100(b) */
#define INAT_REG_CODE_13	13 /* 1101(b) */
#define INAT_REG_CODE_14	14 /* 1110(b) */
#define INAT_REG_CODE_15	15 /* 1111(b) */

/* Whether the instruction uses byte registers or not - it is needed to 
 * determine which register exactly is used, AH or RSP/ESP/SP/SPL, etc. */
/* ??? */ #define INAT_BYTE_REG_OFFS  (INAT_FLAG_OFFS + INAT_FLAG_BITS)
/* ??? */ #define INAT_BYTE_REG       (1 << (INAT_BYTE_REG_OFFS))

/* ??? */ #define INAT_USES_REG_OFFS  (INAT_BYTE_REG_OFFS + 1)
#define INAT_USES_REG_AX    (1 << (INAT_USES_REG_OFFS + INAT_REG_CODE_AX))
#define INAT_USES_REG_CX    (1 << (INAT_USES_REG_OFFS + INAT_REG_CODE_CX))
#define INAT_USES_REG_DX    (1 << (INAT_USES_REG_OFFS + INAT_REG_CODE_DX))
#define INAT_USES_REG_BX    (1 << (INAT_USES_REG_OFFS + INAT_REG_CODE_BX))
#define INAT_USES_REG_SP    (1 << (INAT_USES_REG_OFFS + INAT_REG_CODE_SP))
#define INAT_USES_REG_BP    (1 << (INAT_USES_REG_OFFS + INAT_REG_CODE_BP))
#define INAT_USES_REG_SI    (1 << (INAT_USES_REG_OFFS + INAT_REG_CODE_SI))
#define INAT_USES_REG_DI    (1 << (INAT_USES_REG_OFFS + INAT_REG_CODE_DI))
#define INAT_USES_REG_BITS  8
#define INAT_USES_REG_MASK  (0xFF << (INAT_USES_REG_OFFS))

#if (INAT_USES_REG_OFFS + INAT_USES_REG_BITS > 32)
# error Not enough space in insn_attr_t::attributes left
#endif

/* Attribute making macros for attribute tables */
#define INAT_MAKE_PREFIX(pfx)	(pfx << INAT_PFX_OFFS)
#define INAT_MAKE_ESCAPE(esc)	(esc << INAT_ESC_OFFS)
#define INAT_MAKE_GROUP(grp)	((grp << INAT_GRP_OFFS) | INAT_MODRM)
#define INAT_MAKE_IMM(imm)	(imm << INAT_IMM_OFFS)

/* Attribute search APIs.
 * The functions will store the attributes in '*attr'. The caller must 
 * ensure 'attr' points to a insn_attr_t instance. */
extern void
inat_get_opcode_attribute(insn_attr_t *attr, insn_byte_t opcode);

extern void
inat_get_escape_attribute(insn_attr_t *attr, insn_byte_t opcode, 
	insn_byte_t last_pfx, const insn_attr_t *esc_attr);
		          
extern void
inat_get_group_attribute(insn_attr_t *attr, insn_byte_t modrm, 
	insn_byte_t last_pfx, const insn_attr_t *esc_attr);
	
extern void
inat_get_avx_attribute(insn_attr_t *attr, insn_byte_t opcode, 
	insn_byte_t vex_m, insn_byte_t vex_pp);

/* Copy one insn_attr_t struct ('src') to another ('dest'). */
extern void 
inat_copy_insn_attr(insn_attr_t *dest, const insn_attr_t *src);

/* Zero out the insn_attr_t instance */
extern void
inat_zero_insn_attr(insn_attr_t *attr);

/* Attribute checking functions */
static inline int inat_is_legacy_prefix(const insn_attr_t *ia)
{
	unsigned int attr = ia->attributes;
	attr &= INAT_PFX_MASK;
	return attr && attr <= INAT_LGCPFX_MAX;
}

static inline int inat_is_address_size_prefix(const insn_attr_t *ia)
{
	unsigned int attr = ia->attributes;
	return (attr & INAT_PFX_MASK) == INAT_PFX_ADDRSZ;
}

static inline int inat_is_operand_size_prefix(const insn_attr_t *ia)
{
	unsigned int attr = ia->attributes;
	return (attr & INAT_PFX_MASK) == INAT_PFX_OPNDSZ;
}

static inline int inat_is_rex_prefix(const insn_attr_t *ia)
{
	unsigned int attr = ia->attributes;
	return (attr & INAT_PFX_MASK) == INAT_PFX_REX;
}

static inline int inat_last_prefix_id(const insn_attr_t *ia)
{
	unsigned int attr = ia->attributes;
	if ((attr & INAT_PFX_MASK) > INAT_LSTPFX_MAX)
		return 0;
	else
		return attr & INAT_PFX_MASK;
}

static inline int inat_is_vex_prefix(const insn_attr_t *ia)
{
	unsigned int attr = ia->attributes;
	attr &= INAT_PFX_MASK;
	return attr == INAT_PFX_VEX2 || attr == INAT_PFX_VEX3;
}

static inline int inat_is_vex3_prefix(const insn_attr_t *ia)
{
	unsigned int attr = ia->attributes;
	return (attr & INAT_PFX_MASK) == INAT_PFX_VEX3;
}

static inline int inat_is_escape(const insn_attr_t *ia)
{
	unsigned int attr = ia->attributes;
	return attr & INAT_ESC_MASK;
}

static inline int inat_escape_id(const insn_attr_t *ia)
{
	unsigned int attr = ia->attributes;
	return (attr & INAT_ESC_MASK) >> INAT_ESC_OFFS;
}

static inline int inat_is_group(const insn_attr_t *ia)
{
	unsigned int attr = ia->attributes;
	return attr & INAT_GRP_MASK;
}

static inline int inat_group_id(const insn_attr_t *ia)
{
	unsigned int attr = ia->attributes;
	return (attr & INAT_GRP_MASK) >> INAT_GRP_OFFS;
}

static inline void 
inat_group_copy_common_attribute(insn_attr_t *attr /* out */, 
	const insn_attr_t *ia /* in */)
{
	inat_copy_insn_attr(attr, ia);
	attr->attributes &= ~INAT_GRP_MASK;
}

static inline int inat_has_immediate(const insn_attr_t *ia)
{
	unsigned int attr = ia->attributes;
	return attr & INAT_IMM_MASK;
}

static inline int inat_immediate_size(const insn_attr_t *ia)
{
	unsigned int attr = ia->attributes;
	return (attr & INAT_IMM_MASK) >> INAT_IMM_OFFS;
}

static inline int inat_has_modrm(const insn_attr_t *ia)
{
	unsigned int attr = ia->attributes;
	return attr & INAT_MODRM;
}

static inline int inat_is_force64(const insn_attr_t *ia)
{
	unsigned int attr = ia->attributes;
	return attr & INAT_FORCE64;
}

static inline int inat_has_second_immediate(const insn_attr_t *ia)
{
	unsigned int attr = ia->attributes;
	return attr & INAT_SCNDIMM;
}

static inline int inat_has_moffset(const insn_attr_t *ia)
{
	unsigned int attr = ia->attributes;
	return attr & INAT_MOFFSET;
}

static inline int inat_has_variant(const insn_attr_t *ia)
{
	unsigned int attr = ia->attributes;
	return attr & INAT_VARIANT;
}

static inline int inat_accept_vex(const insn_attr_t *ia)
{
	unsigned int attr = ia->attributes;
	return attr & INAT_VEXOK;
}

static inline int inat_must_vex(const insn_attr_t *ia)
{
	unsigned int attr = ia->attributes;
	return attr & INAT_VEXONLY;
}

static inline unsigned int 
inat_reg_usage_attribute(const insn_attr_t *ia)
{
	unsigned int attr = ia->attributes;
	return (attr & INAT_USES_REG_MASK) >> INAT_USES_REG_OFFS;
}
#endif /* _ASM_X86_INAT_H */
