/* TriCore ELF support for BFD.
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

/* This file holds definitions specific to the TriCore EABI.  */
   

#ifndef _ELF_TRICORE_H
#define _ELF_TRICORE_H

/* Executable-only code section.  */

#define SHF_TRICORE_NOREAD	0x00000800

/* Absolute section.  */

#define SHF_TRICORE_ABS		0x00000400

/* PCP section (code or data).  */

#define SHF_TRICORE_PCP		0x10000000


/* Instruction set architecture V1.1.  */
#define EF_TRICORE_V1_1				0x00000001
#define EF_EABI_TRICORE_V1_1		0x80000000

/* Instruction set architecture V1.2.  */
#define EF_TRICORE_V1_2				0x00000002
#define EF_EABI_TRICORE_V1_2		0x40000000

/* Instruction set architecture V1.3.  */
#define EF_TRICORE_V1_3				0x00000004
#define EF_EABI_TRICORE_V1_3		0x20000000


/* Instruction set architecture V1.3.1  */

#define EF_TRICORE_V1_3_1			0x00000100
#define EF_EABI_TRICORE_V1_3_1		0x00800000

/* Instruction set architecture V1.6.  */

#define EF_TRICORE_V1_6				0x00000200
#define EF_EABI_TRICORE_V1_6		0x00400000

/* Instruction set architecture V1.6.1.  */

#define EF_TRICORE_V1_6_1			0x00000400
#define EF_EABI_TRICORE_V1_6_1		0x00200000


/* PCP co-processor.  */

#define EF_TRICORE_PCP				0x00000010
#define EF_EABI_TRICORE_PCP			0x01000000

/* PCP co-processor, version 2.  */

#define EF_TRICORE_PCP2				0x00000020
#define EF_EABI_TRICORE_PCP2		0x02000000

#define EF_TRICORE_CORE_MASK		0x00000f0f
#define EF_EABI_TRICORE_CORE_MASK	0xf0f00000

/* function to convert old to new eflags definition */
//extern unsigned long tricore_elf32_convert_eflags(unsigned long eflags);
/* convert old eflags to EABI conforming eflags */
#define tricore_elf32_convert_eflags(eflags) (\
{                                             \
  unsigned long new_flags = 0;                \
  int i;                                      \
                                              \
  if (eflags & 0xffff0000)                    \
    new_flags = eflags;                       \
  else                                        \
    for (i = 0; i < 16; i++)                  \
      {                                       \
        if (eflags & (1 << i))                \
          new_flags |= 1 << (31-i);           \
      }                                       \
  (eflags) = new_flags;                       \
} ) 
/* Relocations.  */

#include "elf/reloc-macros.h"

START_RELOC_NUMBERS (elf_tricore_reloc_type)
  RELOC_NUMBER (R_TRICORE_NONE, 0)
  RELOC_NUMBER (R_TRICORE_32REL, 1)
  RELOC_NUMBER (R_TRICORE_32ABS, 2)
  RELOC_NUMBER (R_TRICORE_24REL, 3)
  RELOC_NUMBER (R_TRICORE_24ABS, 4)
  RELOC_NUMBER (R_TRICORE_16SM, 5)
  RELOC_NUMBER (R_TRICORE_HIADJ, 6)
  RELOC_NUMBER (R_TRICORE_LO, 7)
  RELOC_NUMBER (R_TRICORE_LO2, 8)
  RELOC_NUMBER (R_TRICORE_18ABS, 9)
  RELOC_NUMBER (R_TRICORE_10SM, 10)
  RELOC_NUMBER (R_TRICORE_15REL, 11)

  /* Below are GNU extensions.  */

  RELOC_NUMBER (R_TRICORE_HI, 12)
  RELOC_NUMBER (R_TRICORE_16CONST, 13)
  RELOC_NUMBER (R_TRICORE_9ZCONST, 14)
  RELOC_NUMBER (R_TRICORE_9SCONST, 15)
  RELOC_NUMBER (R_TRICORE_8REL, 16)
  RELOC_NUMBER (R_TRICORE_8CONST, 17)
  RELOC_NUMBER (R_TRICORE_10OFF, 18)
  RELOC_NUMBER (R_TRICORE_16OFF, 19)
  RELOC_NUMBER (R_TRICORE_8ABS, 20)
  RELOC_NUMBER (R_TRICORE_16ABS, 21)
  RELOC_NUMBER (R_TRICORE_1BIT, 22)
  RELOC_NUMBER (R_TRICORE_3POS, 23)
  RELOC_NUMBER (R_TRICORE_5POS, 24)

  /* PCP relocations.  */

  RELOC_NUMBER (R_TRICORE_PCPHI, 25)
  RELOC_NUMBER (R_TRICORE_PCPLO, 26)
  RELOC_NUMBER (R_TRICORE_PCPPAGE, 27)
  RELOC_NUMBER (R_TRICORE_PCPOFF, 28)
  RELOC_NUMBER (R_TRICORE_PCPTEXT, 29)

  /* More GNU extensions.  */

  RELOC_NUMBER (R_TRICORE_5POS2, 30)
  RELOC_NUMBER (R_TRICORE_BRCC, 31)
  RELOC_NUMBER (R_TRICORE_BRCZ, 32)
  RELOC_NUMBER (R_TRICORE_BRNN, 33)
  RELOC_NUMBER (R_TRICORE_RRN, 34)
  RELOC_NUMBER (R_TRICORE_4CONST, 35)
  RELOC_NUMBER (R_TRICORE_4REL, 36)
  RELOC_NUMBER (R_TRICORE_4REL2, 37)
  RELOC_NUMBER (R_TRICORE_5POS3, 38)
  RELOC_NUMBER (R_TRICORE_4OFF, 39)
  RELOC_NUMBER (R_TRICORE_4OFF2, 40)
  RELOC_NUMBER (R_TRICORE_4OFF4, 41)
  RELOC_NUMBER (R_TRICORE_42OFF, 42)
  RELOC_NUMBER (R_TRICORE_42OFF2, 43)
  RELOC_NUMBER (R_TRICORE_42OFF4, 44)
  RELOC_NUMBER (R_TRICORE_2OFF, 45)
  RELOC_NUMBER (R_TRICORE_8CONST2, 46)
  RELOC_NUMBER (R_TRICORE_4POS, 47)
  RELOC_NUMBER (R_TRICORE_16SM2, 48)
  RELOC_NUMBER (R_TRICORE_5REL, 49)

  /* GNU extensions to help optimizing virtual tables (C++).  */

  RELOC_NUMBER (R_TRICORE_GNU_VTENTRY, 50)
  RELOC_NUMBER (R_TRICORE_GNU_VTINHERIT, 51)

  /* Support for shared objects.  */

  RELOC_NUMBER (R_TRICORE_PCREL16, 52)
  RELOC_NUMBER (R_TRICORE_PCREL8, 53)
  RELOC_NUMBER (R_TRICORE_GOT, 54)
  RELOC_NUMBER (R_TRICORE_GOT2, 55)
  RELOC_NUMBER (R_TRICORE_GOTHI, 56)
  RELOC_NUMBER (R_TRICORE_GOTLO, 57)
  RELOC_NUMBER (R_TRICORE_GOTLO2, 58)
  RELOC_NUMBER (R_TRICORE_GOTUP, 59)
  RELOC_NUMBER (R_TRICORE_GOTOFF, 60)
  RELOC_NUMBER (R_TRICORE_GOTOFF2, 61)
  RELOC_NUMBER (R_TRICORE_GOTOFFHI, 62)
  RELOC_NUMBER (R_TRICORE_GOTOFFLO, 63)
  RELOC_NUMBER (R_TRICORE_GOTOFFLO2, 64)
  RELOC_NUMBER (R_TRICORE_GOTOFFUP, 65)
  RELOC_NUMBER (R_TRICORE_GOTPC, 66)
  RELOC_NUMBER (R_TRICORE_GOTPC2, 67)
  RELOC_NUMBER (R_TRICORE_GOTPCHI, 68)
  RELOC_NUMBER (R_TRICORE_GOTPCLO, 69)
  RELOC_NUMBER (R_TRICORE_GOTPCLO2, 70)
  RELOC_NUMBER (R_TRICORE_GOTPCUP, 71)
  RELOC_NUMBER (R_TRICORE_PLT, 72)
  RELOC_NUMBER (R_TRICORE_COPY, 73)
  RELOC_NUMBER (R_TRICORE_GLOB_DAT, 74)
  RELOC_NUMBER (R_TRICORE_JMP_SLOT, 75)
  RELOC_NUMBER (R_TRICORE_RELATIVE, 76)

  /* Support for single bit objects.  */

  RELOC_NUMBER (R_TRICORE_BITPOS, 77)

  /* Support for small data addressing areas */
  /* get the base address of a small data symbol */

  RELOC_NUMBER (R_TRICORE_SBREG_S2, 78)
  RELOC_NUMBER (R_TRICORE_SBREG_S1, 79)
  RELOC_NUMBER (R_TRICORE_SBREG_D,  80)
END_RELOC_NUMBERS (R_TRICORE_max)

#endif /* _ELF_TRICORE_H  */
