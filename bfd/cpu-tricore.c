/* BFD support for Infineon's TriCore architecture.
   Copyright (C) 1998-2003 Free Software Foundation, Inc.
   Contributed by Michael Schumacher (mike@hightec-rt.com).

This file is part of BFD, the Binary File Descriptor library.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#include "sysdep.h"
#include "bfd.h"
#include "libbfd.h"
#include "elf/tricore.h"
#include "opcode/tricore.h"

/* Opcode masks for TriCore's various instruction formats.  */

unsigned long tricore_mask_abs;
unsigned long tricore_mask_absb;
unsigned long tricore_mask_b;
unsigned long tricore_mask_bit;
unsigned long tricore_mask_bo;
unsigned long tricore_mask_bol;
unsigned long tricore_mask_brc;
unsigned long tricore_mask_brn;
unsigned long tricore_mask_brr;
unsigned long tricore_mask_rc;
unsigned long tricore_mask_rcpw;
unsigned long tricore_mask_rcr;
unsigned long tricore_mask_rcrr;
unsigned long tricore_mask_rcrw;
unsigned long tricore_mask_rlc;
unsigned long tricore_mask_rr;
unsigned long tricore_mask_rr1;
unsigned long tricore_mask_rr2;
unsigned long tricore_mask_rrpw;
unsigned long tricore_mask_rrr;
unsigned long tricore_mask_rrr1;
unsigned long tricore_mask_rrr2;
unsigned long tricore_mask_rrrr;
unsigned long tricore_mask_rrrw;
unsigned long tricore_mask_sys;
unsigned long tricore_mask_sb;
unsigned long tricore_mask_sbc;
unsigned long tricore_mask_sbr;
unsigned long tricore_mask_sbrn;
unsigned long tricore_mask_sc;
unsigned long tricore_mask_slr;
unsigned long tricore_mask_slro;
unsigned long tricore_mask_sr;
unsigned long tricore_mask_src;
unsigned long tricore_mask_sro;
unsigned long tricore_mask_srr;
unsigned long tricore_mask_srrs;
unsigned long tricore_mask_ssr;
unsigned long tricore_mask_ssro;
unsigned long tricore_opmask[TRICORE_FMT_MAX];

void tricore_init_arch_vars (unsigned long);

/* Describe the various flavours of the TriCore architecture.  */

static const bfd_arch_info_type arch_info_struct[] =
{
  /* V1.1 ISA.  */
  {
    32,				/* 32 bits per word.  */
    32,				/* 32 bits per address.  */
    8,				/* 8 bits per byte.  */
    bfd_arch_tricore,		/* Architecture type.  */
    EF_EABI_TRICORE_V1_1,	/* Machine type.  */
    "tricore",			/* Name of architecture (internal use).  */
    "TriCore:V1.1",		/* Name of architecture to print.  */
    3,				/* Align sections on 8 byte boundaries.  */
    FALSE,			/* No, this is ain't the default arch type.  */
    bfd_default_compatible,	/* We're compatible with ourselves.  */
    bfd_default_scan,		/* Let BFD find the default arch.  */
    bfd_arch_default_fill, /* Default fill.  */
    &arch_info_struct[1],	/* Next TriCore architecture.  */
    0 /* Maximum offset of a reloc from the start of an insn.  */
  },

  /* V1.3 ISA.  */
  {
    32,				/* 32 bits per word.  */
    32,				/* 32 bits per address.  */
    8,				/* 8 bits per byte.  */
    bfd_arch_tricore,		/* Architecture type.  */
    EF_EABI_TRICORE_V1_3,	/* Machine type.  */
    "tricore",			/* Name of architecture (internal use).  */
    "TriCore:V1.3",		/* Name of architecture to print.  */
    3,				/* Align sections on 8 byte boundaries.  */
    FALSE,			/* No, this is ain't the default arch type.  */
    bfd_default_compatible,	/* We're compatible with ourselves.  */
    bfd_default_scan,		/* Let BFD find the default arch.  */
    bfd_arch_default_fill, /* Default fill.  */
    &arch_info_struct[2],	/* Next TriCore architecture.  */
    0 /* Maximum offset of a reloc from the start of an insn.  */
  },

  /* TC V1.3.1 ISA.  */
  {
    32,				/* 32 bits per word.  */
    32,				/* 32 bits per address.  */
    8,				/* 8 bits per byte.  */
    bfd_arch_tricore,		/* Architecture type.  */
    EF_EABI_TRICORE_V1_3_1,	/* Machine type.  */
    "tricore",			/* Name of architecture (internal use).  */
    "TriCore:V1.3.1",		/* Name of architecture to print.  */
    3,				/* Align sections on 8 byte boundaries.  */
    FALSE,			/* No, this is ain't the default arch type.  */
    bfd_default_compatible,	/* We're compatible with ourselves.  */
    bfd_default_scan,		/* Let BFD find the default arch.  */
    bfd_arch_default_fill, /* Default fill.  */
    &arch_info_struct[3],	/* Next TriCore architecture.  */
    0 /* Maximum offset of a reloc from the start of an insn.  */
  },

  /* TriCore V1_6 ISA.  */
  {
    32,				/* 32 bits per word.  */
    32,				/* 32 bits per address.  */
    8,				/* 8 bits per byte.  */
    bfd_arch_tricore,		/* Architecture type.  */
    EF_EABI_TRICORE_V1_6,		/* Machine type.  */
    "tricore",			/* Name of architecture (internal use).  */
    "TriCore:V1_6",		/* Name of architecture to print.  */
    3,				/* Align sections on 8 byte boundaries.  */
    FALSE,			/* No, this is ain't the default arch type.  */
    bfd_default_compatible,	/* We're compatible with ourselves.  */
    bfd_default_scan,		/* Let BFD find the default arch.  */
    bfd_arch_default_fill, /* Default fill.  */
    &arch_info_struct[4],	/* Next TriCore architecture.  */
    0 /* Maximum offset of a reloc from the start of an insn.  */
  },

  /* TriCore V1.6.1 ISA.  */
  {
    32,				/* 32 bits per word.  */
    32,				/* 32 bits per address.  */
    8,				/* 8 bits per byte.  */
    bfd_arch_tricore,		/* Architecture type.  */
    EF_EABI_TRICORE_V1_6_1,	/* Machine type.  */
    "tricore",			/* Name of architecture (internal use).  */
    "TriCore:V1_6_1",		/* Name of architecture to print.  */
    3,				/* Align sections on 8 byte boundaries.  */
    FALSE,			/* No, this is ain't the default arch type.  */
    bfd_default_compatible,	/* We're compatible with ourselves.  */
    bfd_default_scan,		/* Let BFD find the default arch.  */
    bfd_arch_default_fill, /* Default fill.  */
    NULL,	/* No more arch types for TriCore.  */
    0 /* Maximum offset of a reloc from the start of an insn.  */
  }
};

const bfd_arch_info_type bfd_tricore_arch =
{
  /* V1.2 ISA.  */
  32,				/* 32 bits per word.  */
  32,				/* 32 bits per address.  */
  8,				/* 8 bits per byte.  */
  bfd_arch_tricore,		/* Architecture type.  */
  EF_EABI_TRICORE_V1_2,		/* Machine type.  */
  "tricore",			/* Name of architecture (internal use).  */
  "TriCore:V1.2",		/* Name of architecture to print.  */
  3,				/* Align sections on 8 byte boundaries.  */
  TRUE,				/* Yes, this is the default arch type.  */
  bfd_default_compatible,	/* We're compatible with ourselves.  */
  bfd_default_scan,		/* Let BFD find the default arch.  */
  bfd_arch_default_fill,	/* Default fill.  */
  &arch_info_struct[0],		/* Next arch type for TriCore.  */
  0 /* Maximum offset of a reloc from the start of an insn.  */
};

/* Initialize the architecture-specific variables.  This must be called
   by the assembler and disassembler prior to encoding/decoding any
   TriCore instructions;  the linker (or more precisely, the specific
   back-end, bfd/elf32-tricore.c:tricore_elf32_relocate_section) will
   also have to call this if it ever accesses the variables below, but
   it currently doesn't.  */

void
tricore_init_arch_vars (unsigned long mach)
{
  switch (mach & EF_EABI_TRICORE_CORE_MASK)
    {
    case EF_EABI_TRICORE_V1_1:
      tricore_mask_abs =  0x0c0000ff;
      tricore_mask_absb = 0x0c0000ff;
      tricore_mask_b =    0x000000ff;
      tricore_mask_bit =  0x006000ff;
      tricore_mask_bo =   0x0fc000ff;
      tricore_mask_bol =  0x000000ff;
      tricore_mask_brc =  0x800000ff;
      tricore_mask_brn =  0x8000007f;
      tricore_mask_brr =  0x800000ff;
      tricore_mask_rc =   0x0fe000ff;
      tricore_mask_rcpw = 0x006000ff;
      tricore_mask_rcr =  0x00e000ff;
      tricore_mask_rcrr = 0x00e000ff;
      tricore_mask_rcrw = 0x00e000ff;
      tricore_mask_rlc =  0x000000ff;
      tricore_mask_rr =   0x0ff000ff;
      tricore_mask_rrpw = 0x006000ff;
      tricore_mask_rrr =  0x00f000ff;
      tricore_mask_rrr1 = 0x00fc00ff;
      tricore_mask_rrr2 = 0x00ff00ff;
      tricore_mask_rrrr = 0x00e000ff;
      tricore_mask_rrrw = 0x00e000ff;
      tricore_mask_sys =  0x07c000ff;
      tricore_mask_sb =       0x00ff;
      tricore_mask_sbc =      0x00ff;
      tricore_mask_sbr =      0x00ff;
      tricore_mask_sbrn =     0x007f;
      tricore_mask_sc =       0x00ff;
      tricore_mask_slr =      0x00ff;
      tricore_mask_slro =     0x00ff;
      tricore_mask_sr =       0xf0ff;
      tricore_mask_src =      0x00ff;
      tricore_mask_sro =      0x00ff;
      tricore_mask_srr =      0x00ff;
      tricore_mask_srrs =     0x003f;
      tricore_mask_ssr =      0x00ff;
      tricore_mask_ssro =     0x00ff;
      break;

    case EF_EABI_TRICORE_V1_2:
    case EF_EABI_TRICORE_V1_3:
    case EF_EABI_TRICORE_V1_3_1:
    case EF_EABI_TRICORE_V1_6:
    case EF_EABI_TRICORE_V1_6_1:
      tricore_mask_abs =  0x0c0000ff;
      tricore_mask_absb = 0x0c0000ff;
      tricore_mask_b =    0x000000ff;
      tricore_mask_bit =  0x006000ff;
      tricore_mask_bo =   0x0fc000ff;
      tricore_mask_bol =  0x000000ff;
      tricore_mask_brc =  0x800000ff;
      tricore_mask_brn =  0x8000007f;
      tricore_mask_brr =  0x800000ff;
      tricore_mask_rc =   0x0fe000ff;
      tricore_mask_rcpw = 0x006000ff;
      tricore_mask_rcr =  0x00e000ff;
      tricore_mask_rcrr = 0x00e000ff;
      tricore_mask_rcrw = 0x00e000ff;
      tricore_mask_rlc =  0x000000ff;
      tricore_mask_rr =   0x0ff300ff;
      tricore_mask_rr1 =  0x0ffc00ff;
      tricore_mask_rr2 =  0x0fff00ff;
      tricore_mask_rrpw = 0x006000ff;
      tricore_mask_rrr =  0x00f300ff;
      tricore_mask_rrr1 = 0x00fc00ff;
      tricore_mask_rrr2 = 0x00ff00ff;
      tricore_mask_rrrr = 0x00e000ff;
      tricore_mask_rrrw = 0x00e000ff;
      tricore_mask_sys =  0x0fc000ff;
      tricore_mask_sb =       0x00ff;
      tricore_mask_sbc =      0x00ff;
      tricore_mask_sbr =      0x00ff;
      tricore_mask_sbrn =     0x00ff;
      tricore_mask_sc =       0x00ff;
      tricore_mask_slr =      0x00ff;
      tricore_mask_slro =     0x00ff;
      tricore_mask_sr =       0xf0ff;
      tricore_mask_src =      0x00ff;
      tricore_mask_sro =      0x00ff;
      tricore_mask_srr =      0x00ff;
      tricore_mask_srrs =     0x003f;
      tricore_mask_ssr =      0x00ff;
      tricore_mask_ssro =     0x00ff;
      break;
    }

  /* Now fill in tricore_opmask[].  */

  tricore_opmask[TRICORE_FMT_ABS] = tricore_mask_abs;
  tricore_opmask[TRICORE_FMT_ABSB] = tricore_mask_absb;
  tricore_opmask[TRICORE_FMT_B] = tricore_mask_b;
  tricore_opmask[TRICORE_FMT_BIT] = tricore_mask_bit;
  tricore_opmask[TRICORE_FMT_BO] = tricore_mask_bo;
  tricore_opmask[TRICORE_FMT_BOL] = tricore_mask_bol;
  tricore_opmask[TRICORE_FMT_BRC] = tricore_mask_brc;
  tricore_opmask[TRICORE_FMT_BRN] = tricore_mask_brn;
  tricore_opmask[TRICORE_FMT_BRR] = tricore_mask_brr;
  tricore_opmask[TRICORE_FMT_RC] = tricore_mask_rc;
  tricore_opmask[TRICORE_FMT_RCPW] = tricore_mask_rcpw;
  tricore_opmask[TRICORE_FMT_RCR] = tricore_mask_rcr;
  tricore_opmask[TRICORE_FMT_RCRR] = tricore_mask_rcrr;
  tricore_opmask[TRICORE_FMT_RCRW] = tricore_mask_rcrw;
  tricore_opmask[TRICORE_FMT_RLC] = tricore_mask_rlc;
  tricore_opmask[TRICORE_FMT_RR] = tricore_mask_rr;
  tricore_opmask[TRICORE_FMT_RR1] = tricore_mask_rr1;
  tricore_opmask[TRICORE_FMT_RR2] = tricore_mask_rr2;
  tricore_opmask[TRICORE_FMT_RRPW] = tricore_mask_rrpw;
  tricore_opmask[TRICORE_FMT_RRR] = tricore_mask_rrr;
  tricore_opmask[TRICORE_FMT_RRR1] = tricore_mask_rrr1;
  tricore_opmask[TRICORE_FMT_RRR2] = tricore_mask_rrr2;
  tricore_opmask[TRICORE_FMT_RRRR] = tricore_mask_rrrr;
  tricore_opmask[TRICORE_FMT_RRRW] = tricore_mask_rrrw;
  tricore_opmask[TRICORE_FMT_SYS] = tricore_mask_sys;
  tricore_opmask[TRICORE_FMT_SB] = tricore_mask_sb;
  tricore_opmask[TRICORE_FMT_SBC] = tricore_mask_sbc;
  tricore_opmask[TRICORE_FMT_SBR] = tricore_mask_sbr;
  tricore_opmask[TRICORE_FMT_SBRN] = tricore_mask_sbrn;
  tricore_opmask[TRICORE_FMT_SC] = tricore_mask_sc;
  tricore_opmask[TRICORE_FMT_SLR] = tricore_mask_slr;
  tricore_opmask[TRICORE_FMT_SLRO] = tricore_mask_slro;
  tricore_opmask[TRICORE_FMT_SR] = tricore_mask_sr;
  tricore_opmask[TRICORE_FMT_SRC] = tricore_mask_src;
  tricore_opmask[TRICORE_FMT_SRO] = tricore_mask_sro;
  tricore_opmask[TRICORE_FMT_SRR] = tricore_mask_srr;
  tricore_opmask[TRICORE_FMT_SRRS] = tricore_mask_srrs;
  tricore_opmask[TRICORE_FMT_SSR] = tricore_mask_ssr;
  tricore_opmask[TRICORE_FMT_SSRO] = tricore_mask_ssro;
}

/* End of cpu-tricore.c.  */
