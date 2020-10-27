/* TriCore-specific support for 32-bit ELF.
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
#include "bfdlink.h"
#include "genlink.h"
#include "elf-bfd.h"
#include "elf/tricore.h"
#include <stdbool.h>

/* The full name of the default instruction set architecture.  */

#define DEFAULT_ISA "TriCore:V1.2"		/* Name of default architecture to print.  */

/* The full name of the dynamic interpreter; put in the .interp section.  */

#define ELF_DYNAMIC_INTERPRETER "/lib/ld-tricore.so.1"

/* The number of reserved entries at the beginning of the PLT.  */

#define PLT_RESERVED_SLOTS 2

/* The size (in bytes) of a PLT entry.  */

#define PLT_ENTRY_SIZE 12

/* Section flag for PCP sections.  */

#define PCP_SEG SEC_ARCH_BIT_0

/* Section flags for dynamic relocation sections.  */

#define RELGOTSECFLAGS	(SEC_ALLOC | SEC_LOAD | SEC_HAS_CONTENTS \
			 | SEC_IN_MEMORY | SEC_LINKER_CREATED | SEC_READONLY)
#define DYNOBJSECFLAGS  (SEC_HAS_CONTENTS | SEC_IN_MEMORY \
			 | SEC_LINKER_CREATED | SEC_READONLY)

/* Will references to this symbol always reference the symbol in this obj?  */

/*
#define SYMBOL_REFERENCES_LOCAL(INFO, H)				\
  ((!INFO->shared							\
    || INFO->symbolic							\
    || (H->dynindx == -1)						\
    || (ELF_ST_VISIBILITY (H->other) == STV_INTERNAL)			\
    || (ELF_ST_VISIBILITY (H->other) == STV_HIDDEN))			\
   && ((H->elf_link_hash_flags & ELF_LINK_HASH_DEF_REGULAR) != 0))
*/
/* Will _calls_ to this symbol always call the version in this object?  */
/*
#define SYMBOL_CALLS_LOCAL(INFO, H)					\
  ((!INFO->shared							\
    || INFO->symbolic							\
    || (H->dynindx == -1)						\
    || (ELF_ST_VISIBILITY (H->other) != STV_DEFAULT))			\
   && ((H->elf_link_hash_flags & ELF_LINK_HASH_DEF_REGULAR) != 0))
*/
/* Describe "short addressable" memory areas (SDAs, Small Data Areas).  */

typedef struct _sda_t
{
  /* Name of this SDA's short addressable data section.  */
  const char *data_section_name;

  /* Name of this SDA's short addressable BSS section.  */
  const char *bss_section_name;

  /* Name of the symbol that contains the base address of this SDA.  */
  const char *sda_symbol_name;

  /* Pointers to the BFD representations of the above data/BSS sections.  */
  asection *data_section;
  asection *bss_section;

  /* The base address of this SDA; usually points to the middle of a 64k
     memory area to provide full access via a 16-bit signed offset; this
     can, however, be overridden by the user (via "--defsym" on the command
     line, or in the linker script with an assignment statement such as
     "_SMALL_DATA_ = ABSOLUTE (.);" in output section ".sbss" or ".sdata").  */
  bfd_vma gp_value;

  /* The number of the address register that contains the base address
     of this SDA.  This is 0 for .sdata/.sbss (or 12 if this is a dynamic
     executable, because in this case the SDA follows immediately after the
     GOT, and both are accessed via the GOT pointer), 1 for .sdata2/.sbss2,
     8 for .sdata3/.sbss3, and 9 for .sdata4/.sbss4.  */
  int areg;

  /* TRUE if this SDA has been specified as an output section in the
     linker script; it suffices if either the data or BSS section of
     this SDA has been specified (e.g., just ".sbss2", but not ".sdata2"
     for SDA1, or just ".sdata", but not ".sbss" for SDA0).  */
  bfd_boolean valid;
} sda_t;

/* We allow up to four independent SDAs in executables.  For instance,
   if you need 128k of initialized short addressable data, and 128k of
   uninitialized short addressable data, you could specify .sdata, .sdata2,
   .sbss3, and .sbss4 as output sections in your linker script.  Note,
   however, that according to the EABI only the first SDA must be supported,
   while support for the second SDA (called "literal section") is optional.
   The other two SDAs are GNU extensions and can only be used in standalone
   applications, or if an underlying OS doesn't use %a8 and %a9 for its own
   purposes.  Also note that shared objects may only use the first SDA,
   which will be addressed via the GOT pointer (%a12), so it can't exceed
   32k, and may only use it for static variables.  That's because if a
   program references a global variable defined in a shared object, the
   linker reserves space for it in the program's ".dynbss" section and emits
   a COPY reloc that will be resolved by the dynamic linker.  If, however,
   the variable would be defined in the SDA of a SO, then this would lead
   to different accesses to this variable, as the program expects it to live
   in its ".dynbss" section, while the SO was compiled to access it in its
   SDA -- clearly a situation that must be avoided.  */

#define NR_SDAS 4

sda_t small_data_areas[NR_SDAS] =
{
  { ".sdata",
    ".sbss",
    "_SMALL_DATA_",
    (asection *) NULL,
    (asection *) NULL,
    0,
    0,
    FALSE
  },

  { ".sdata2",
    ".sbss2",
    "_SMALL_DATA2_",
    (asection *) NULL,
    (asection *) NULL,
    0,
    1,
    FALSE
  },

  { ".sdata3",
    ".sbss3",
    "_SMALL_DATA3_",
    (asection *) NULL,
    (asection *) NULL,
    0,
    8,
    FALSE
  },

  { ".sdata4",
    ".sbss4",
    "_SMALL_DATA4_",
    (asection *) NULL,
    (asection *) NULL,
    0,
    9,
    FALSE
  }
};

/* If the user requested an extended map file, we might need to keep a list
   of global (and possibly static) symbols.  */

typedef struct _symbol_t
{
  /* Name of symbol/variable.  */
  const char *name;

  /* Memory location of this variable, or value if it's an absolute symbol.  */
  bfd_vma address;

  /* Alignment of this variable (in output section).  */
  int align;

  /* Name of memory region this variable lives in.  */
  char *region_name;

  /* TRUE if this is a bit variable.  */
  bfd_boolean is_bit;

  /* Bit position if this is a bit variable.  */
  int bitpos;

  /* TRUE if this is a static variable.  */
  bfd_boolean is_static;

  /* Size of this variable.  */
  bfd_vma size;

  /* Pointer to the section in which this symbol is defined.  */
  asection *section;

  /* Name of module in which this symbol is defined.  */
  const char *module_name;
} symbol_t;

/* Symbols to be listed are stored in a dynamically allocated array.  */

// static symbol_t *symbol_list;
// static int symbol_list_idx = -1;
// static int symbol_list_max = 512;

/* This describes memory regions defined by the user; must be kept in
   sync with ld/emultempl/tricoreelf.em.  */

typedef struct _memreg
{
  /* Name of region.  */
  char *name;

  /* Start of region.  */
  bfd_vma start;

  /* Length of region.  */
  bfd_size_type length;

  /* Number of allocated (used) bytes.  */
  bfd_size_type used;
} memreg_t;

/* This array describes TriCore relocations.  */

static reloc_howto_type tricore_elf32_howto_table[] =
{
      /* No relocation (ignored).  */
  HOWTO (R_TRICORE_NONE,	/* type */
	 0,			/* rightshift */
	 0,			/* size (0 = byte, 1 = short, 2 = long) */
	 0,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont,/* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_TRICORE_NONE",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0,			/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* 32 bit PC-relative relocation.  */
  HOWTO (R_TRICORE_32REL,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 TRUE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont,/* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_TRICORE_32REL",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffffffff,		/* dst_mask */
	 TRUE),			/* pcrel_offset */

  /* 32 bit absolute relocation.  */
  HOWTO (R_TRICORE_32ABS,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont,/* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_TRICORE_32ABS",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffffffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* relB 25 bit PC-relative relocation.  */
  HOWTO (R_TRICORE_24REL,	/* type */
	 1,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 25,			/* bitsize */
	 TRUE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont,/* complain_on_overflow */
	 bfd_elf_generic_reloc, /* special_function */
	 "R_TRICORE_24REL",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffffff00,		/* dst_mask */
	 TRUE),			/* pcrel_offset */

  /* absB 24 bit absolute address relocation.  */
  HOWTO (R_TRICORE_24ABS,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont,/* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_TRICORE_24ABS",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffffff00,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* bolC 16 bit small data section relocation.  */
  HOWTO (R_TRICORE_16SM,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont,/* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_TRICORE_16SM",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffff0000,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* RLC High 16 bits of symbol value, adjusted.  */
  HOWTO (R_TRICORE_HIADJ,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont,/* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_TRICORE_HIADJ",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0x0ffff000,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* RLC Low 16 bits of symbol value.  */
  HOWTO (R_TRICORE_LO,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 FALSE,			/* pc_relative */
	 12,			/* bitpos */
	 complain_overflow_dont,/* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_TRICORE_LO",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0x0ffff000,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* BOL Low 16 bits of symbol value.  */
  HOWTO (R_TRICORE_LO2,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont,/* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_TRICORE_LO2",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffff0000,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* ABS 18 bit absolute address relocation.  */
  HOWTO (R_TRICORE_18ABS,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 18,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont,/* complain_on_overflow */
	 bfd_elf_generic_reloc, /* special_function */
	 "R_TRICORE_18ABS",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0xf3fff000,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* BO 10 bit relative small data relocation.  */
  HOWTO (R_TRICORE_10SM,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 10,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont,/* complain_on_overflow */
	 bfd_elf_generic_reloc, /* special_function */
	 "R_TRICORE_10SM",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0xf03f0000,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* BR 15 bit PC-relative relocation.  */
  HOWTO (R_TRICORE_15REL,	/* type */
	 1,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 TRUE,			/* pc_relative */
	 16,			/* bitpos */
	 complain_overflow_dont,/* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_TRICORE_15REL",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0x7fff0000,		/* dst_mask */
	 TRUE),			/* pcrel_offset */

  /* RLC High 16 bits of symbol value.  */
  HOWTO (R_TRICORE_HI,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont,/* complain_on_overflow */
	 bfd_elf_generic_reloc, /* special_function */
	 "R_TRICORE_HI",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0x0ffff000,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* rlcC 16 bit signed constant.  */
  HOWTO (R_TRICORE_16CONST,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 FALSE,			/* pc_relative */
	 12,			/* bitpos */
	 complain_overflow_dont,/* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_TRICORE_16CONST",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0x0ffff000,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* rcC2 9 bit unsigned constant.  */
  HOWTO (R_TRICORE_9ZCONST,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 9,			/* bitsize */
	 FALSE,			/* pc_relative */
	 12,			/* bitpos */
	 complain_overflow_dont,/* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_TRICORE_9ZCONST",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0x001ff000,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* RcC 9 bit signed constant.  */
  HOWTO (R_TRICORE_9SCONST,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 9,			/* bitsize */
	 FALSE,			/* pc_relative */
	 12,			/* bitpos */
	 complain_overflow_dont,/* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_TRICORE_9SCONST",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0x001ff000,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* sbD 9 bit PC-relative displacement.  */
  HOWTO (R_TRICORE_8REL,	/* type */
	 1,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 9,			/* bitsize */
	 TRUE,			/* pc_relative */
	 8,			/* bitpos */
	 complain_overflow_dont,/* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_TRICORE_8REL",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0xff00,		/* dst_mask */
	 TRUE),			/* pcrel_offset */

  /* scC 8 bit unsigned constant.  */
  HOWTO (R_TRICORE_8CONST,	/* type */
	 0,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 8,			/* bitsize */
	 FALSE,			/* pc_relative */
	 8,			/* bitpos */
	 complain_overflow_dont,/* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_TRICORE_8CONST",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0xff00,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* BO 10 bit data offset.  */
  HOWTO (R_TRICORE_10OFF,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 10,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont,/* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_TRICORE_10OFF",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0xf03f0000,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* bolC 16 bit data offset.  */
  HOWTO (R_TRICORE_16OFF,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont,/* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_TRICORE_16OFF",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffff0000,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* 8 bit absolute data relocation.  */
  HOWTO (R_TRICORE_8ABS,	/* type */
	 0,			/* rightshift */
	 0,			/* size (0 = byte, 1 = short, 2 = long) */
	 8,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont,/* complain_on_overflow */
	 bfd_elf_generic_reloc, /* special_function */
	 "R_TRICORE_8ABS",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0xff,			/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* 16 bit absolute data relocation.  */
  HOWTO (R_TRICORE_16ABS,	/* type */
	 0,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont,/* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_TRICORE_16ABS",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* absBb 1 bit relocation.  */
  HOWTO (R_TRICORE_1BIT,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 1,			/* bitsize */
	 FALSE,			/* pc_relative */
	 11,			/* bitpos */
	 complain_overflow_dont,/* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_TRICORE_1BIT",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0x00000800,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* absBp 3 bit bit position.  */
  HOWTO (R_TRICORE_3POS,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 3,			/* bitsize */
	 FALSE,			/* pc_relative */
	 8,			/* bitpos */
	 complain_overflow_dont,/* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_TRICORE_3POS",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0x00000700,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* bitP1 5 bit bit position.  */
  HOWTO (R_TRICORE_5POS,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 5,			/* bitsize */
	 FALSE,			/* pc_relative */
	 16,			/* bitpos */
	 complain_overflow_dont,/* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_TRICORE_5POS",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0x001f0000,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* PCP HI relocation.  */
  HOWTO (R_TRICORE_PCPHI,	/* type */
	 1,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont,/* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_TRICORE_PCPHI",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* PCP LO relocation.  */
  HOWTO (R_TRICORE_PCPLO,	/* type */
	 0,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont,/* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_TRICORE_PCPLO",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* PCP PAGE relocation.  */
  HOWTO (R_TRICORE_PCPPAGE,	/* type */
	 0,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont,/* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_TRICORE_PCPPAGE",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0xff00,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* PCP OFF relocation.  */
  HOWTO (R_TRICORE_PCPOFF,	/* type */
	 0,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont,/* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_TRICORE_PCPOFF",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0x003f,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* PCP TEXT relocation.  */
  HOWTO (R_TRICORE_PCPTEXT,	/* type */
	 0,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont,/* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_TRICORE_PCPTEXT",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* bitP2 5 bit bit position.  */
  HOWTO (R_TRICORE_5POS2,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 5,			/* bitsize */
	 FALSE,			/* pc_relative */
	 23,			/* bitpos */
	 complain_overflow_dont,/* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_TRICORE_5POS2",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0x0f800000,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* brcC 4 bit signed offset.  */
  HOWTO (R_TRICORE_BRCC,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 4,			/* bitsize */
	 FALSE,			/* pc_relative */
	 12,			/* bitpos */
	 complain_overflow_dont,/* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_TRICORE_BRCC",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0x0000f000,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* brcC2 4 bit unsigned offset.  */
  HOWTO (R_TRICORE_BRCZ,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 4,			/* bitsize */
	 FALSE,			/* pc_relative */
	 12,			/* bitpos */
	 complain_overflow_dont,/* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_TRICORE_BRCZ",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0x0000f000,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* brnN 5 bit bit position.  */
  HOWTO (R_TRICORE_BRNN,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 5,			/* bitsize */
	 FALSE,			/* pc_relative */
	 12,			/* bitpos */
	 complain_overflow_dont,/* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_TRICORE_BRNN",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0x0000f080,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* rrN 2 bit unsigned constant.  */
  HOWTO (R_TRICORE_RRN,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 2,			/* bitsize */
	 FALSE,			/* pc_relative */
	 16,			/* bitpos */
	 complain_overflow_dont,/* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_TRICORE_RRN",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0x00030000,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* sbcC 4 bit signed constant.  */
  HOWTO (R_TRICORE_4CONST,	/* type */
	 0,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 4,			/* bitsize */
	 FALSE,			/* pc_relative */
	 12,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_TRICORE_4CONST",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0xf000,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* sbcD/sbrD 5 bit PC-relative, zero-extended displacement.  */
  HOWTO (R_TRICORE_4REL,	/* type */
	 1,			/* rightshift */
	 0,			/* size (0 = byte, 1 = short, 2 = long) */
	 5,			/* bitsize */
	 TRUE,			/* pc_relative */
	 8,			/* bitpos */
	 complain_overflow_dont,/* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_TRICORE_4REL",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0x0f00,		/* dst_mask */
	 TRUE),			/* pcrel_offset */

  /* sbrD 5 bit PC-relative, one-extended displacement.  */
  HOWTO (R_TRICORE_4REL2,	/* type */
	 1,			/* rightshift */
	 0,			/* size (0 = byte, 1 = short, 2 = long) */
	 5,			/* bitsize */
	 TRUE,			/* pc_relative */
	 8,			/* bitpos */
	 complain_overflow_dont,/* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_TRICORE_4REL2",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0x0f00,		/* dst_mask */
	 TRUE),			/* pcrel_offset */

  /* sbrN 5 bit bit position.  */
  HOWTO (R_TRICORE_5POS3,	/* type */
	 0,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 5,			/* bitsize */
	 FALSE,			/* pc_relative */
	 12,			/* bitpos */
	 complain_overflow_dont,/* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_TRICORE_5POS3",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0xf080,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* slroO 4 bit zero-extended offset.  */
  HOWTO (R_TRICORE_4OFF,	/* type */
	 0,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 4,			/* bitsize */
	 FALSE,			/* pc_relative */
	 12,			/* bitpos */
	 complain_overflow_dont,/* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_TRICORE_4OFF",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0xf000,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* slroO2 5 bit zero-extended offset.  */
  HOWTO (R_TRICORE_4OFF2,	/* type */
	 1,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 5,			/* bitsize */
	 FALSE,			/* pc_relative */
	 12,			/* bitpos */
	 complain_overflow_dont,/* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_TRICORE_4OFF2",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0xf000,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* slroO4 6 bit zero-extended offset.  */
  HOWTO (R_TRICORE_4OFF4,	/* type */
	 2,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 6,			/* bitsize */
	 FALSE,			/* pc_relative */
	 12,			/* bitpos */
	 complain_overflow_dont,/* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_TRICORE_4OFF4",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0xf000,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* sroO 4 bit zero-extended offset.  */
  HOWTO (R_TRICORE_42OFF,	/* type */
	 0,			/* rightshift */
	 0,			/* size (0 = byte, 1 = short, 2 = long) */
	 4,			/* bitsize */
	 FALSE,			/* pc_relative */
	 8,			/* bitpos */
	 complain_overflow_dont,/* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_TRICORE_42OFF",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0x0f00,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* sroO2 5 bit zero-extended offset.  */
  HOWTO (R_TRICORE_42OFF2,	/* type */
	 1,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 5,			/* bitsize */
	 FALSE,			/* pc_relative */
	 8,			/* bitpos */
	 complain_overflow_dont,/* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_TRICORE_42OFF2",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0x0f00,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* slroO4 6 bit zero-extended offset.  */
  HOWTO (R_TRICORE_42OFF4,	/* type */
	 2,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 6,			/* bitsize */
	 FALSE,			/* pc_relative */
	 8,			/* bitpos */
	 complain_overflow_dont,/* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_TRICORE_42OFF4",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0x0f00,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* srrsN 2 bit zero-extended constant.  */
  HOWTO (R_TRICORE_2OFF,	/* type */
	 0,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 2,			/* bitsize */
	 FALSE,			/* pc_relative */
	 6,			/* bitpos */
	 complain_overflow_dont,/* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_TRICORE_2OFF",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0x00c0,		/* dst_mask */
	 FALSE), 		/* pcrel_offset */

  /* scC 8 bit zero-extended offset.  */
  HOWTO (R_TRICORE_8CONST2,	/* type */
	 0,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 8,			/* bitsize */
	 FALSE,			/* pc_relative */
	 8,			/* bitpos */
	 complain_overflow_dont,/* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_TRICORE_8CONST2",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0xff00,		/* dst_mask */
	 FALSE), 		/* pcrel_offset */

  /* sbrnN 4 bit zero-extended constant.  */
  HOWTO (R_TRICORE_4POS,	/* type */
	 0,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 4,			/* bitsize */
	 FALSE,			/* pc_relative */
	 12,			/* bitpos */
	 complain_overflow_dont,/* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_TRICORE_4POS",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0xf000,		/* dst_mask */
	 FALSE), 		/* pcrel_offset */

  /* rlcC 16 bit small data section relocation.  */
  HOWTO (R_TRICORE_16SM2,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 FALSE,			/* pc_relative */
	 12,			/* bitpos */
	 complain_overflow_dont,/* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_TRICORE_16SM2",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0x0ffff000,		/* dst_mask */
	 FALSE), 		/* pcrel_offset */

  /* sbcD/sbrD 6 bit PC-relative, zero-extended displacement.  */
  HOWTO (R_TRICORE_5REL,	/* type */
	 1,			/* rightshift */
	 0,			/* size (0 = byte, 1 = short, 2 = long) */
	 5,			/* bitsize */
	 TRUE,			/* pc_relative */
	 8,			/* bitpos */
	 complain_overflow_dont,/* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_TRICORE_5REL",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0x0f00,		/* dst_mask */
	 TRUE),			/* pcrel_offset */

  /* Special reloc for optimizing virtual tables.  */
  HOWTO (R_TRICORE_GNU_VTENTRY,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 0,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont,/* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_TRICORE_GNU_VTENTRY",/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0,			/* dst_mask */
	 FALSE), 		/* pcrel_offset */

  /* Special reloc for optimizing virtual tables.  */
  HOWTO (R_TRICORE_GNU_VTINHERIT,/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 0,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont,/* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_TRICORE_GNU_VTINHERIT",/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0,			/* dst_mask */
	 FALSE),  		/* pcrel_offset */

  /* 16 bit PC-relative relocation.  */
  HOWTO (R_TRICORE_PCREL16,	/* type */
	 0,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 TRUE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont,/* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_TRICORE_PCREL16",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffff,		/* dst_mask */
	 TRUE),			/* pcrel_offset */

  /* 8 bit PC-relative relocation.  */
  HOWTO (R_TRICORE_PCREL8,	/* type */
	 0,			/* rightshift */
	 0,			/* size (0 = byte, 1 = short, 2 = long) */
	 8,			/* bitsize */
	 TRUE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont,/* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_TRICORE_PCREL8",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0xff,			/* dst_mask */
	 TRUE),			/* pcrel_offset */

  /* rlcC 16 bit GOT symbol entry.  */
  HOWTO (R_TRICORE_GOT,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 FALSE,			/* pc_relative */
	 12,			/* bitpos */
	 complain_overflow_dont,/* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_TRICORE_GOT",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0x0ffff000,		/* dst_mask */
	 FALSE), 		/* pcrel_offset */

  /* bolC 16 bit GOT symbol entry.  */
  HOWTO (R_TRICORE_GOT2,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont,/* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_TRICORE_GOT2",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffff0000,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* rlcC 16 bit GOTHI symbol entry.  */
  HOWTO (R_TRICORE_GOTHI,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 FALSE,			/* pc_relative */
	 12,			/* bitpos */
	 complain_overflow_dont,/* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_TRICORE_GOTHI",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0x0ffff000,		/* dst_mask */
	 FALSE), 		/* pcrel_offset */

  /* rlcC 16 bit GOTLO symbol entry.  */
  HOWTO (R_TRICORE_GOTLO,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 FALSE,			/* pc_relative */
	 12,			/* bitpos */
	 complain_overflow_dont,/* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_TRICORE_GOTLO",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0x0ffff000,		/* dst_mask */
	 FALSE), 		/* pcrel_offset */

  /* bolC 16 bit GOTLO symbol entry.  */
  HOWTO (R_TRICORE_GOTLO2,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont,/* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_TRICORE_GOTLO2",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffff0000,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* rlcC 16 bit GOTUP symbol entry.  */
  HOWTO (R_TRICORE_GOTUP,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 FALSE,			/* pc_relative */
	 12,			/* bitpos */
	 complain_overflow_dont,/* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_TRICORE_GOTUP",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0x0ffff000,		/* dst_mask */
	 FALSE), 		/* pcrel_offset */

  /* rlcC 16 bit GOTOFF symbol entry.  */
  HOWTO (R_TRICORE_GOTOFF,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 FALSE,			/* pc_relative */
	 12,			/* bitpos */
	 complain_overflow_dont,/* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_TRICORE_GOTOFF",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0x0ffff000,		/* dst_mask */
	 FALSE), 		/* pcrel_offset */

  /* bolC 16 bit GOTOFF symbol entry.  */
  HOWTO (R_TRICORE_GOTOFF2,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont,/* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_TRICORE_GOTOFF2",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffff0000,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* rlcC 16 bit GOTOFFHI symbol entry.  */
  HOWTO (R_TRICORE_GOTOFFHI,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 FALSE,			/* pc_relative */
	 12,			/* bitpos */
	 complain_overflow_dont,/* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_TRICORE_GOTOFFHI",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0x0ffff000,		/* dst_mask */
	 FALSE), 		/* pcrel_offset */

  /* rlcC 16 bit GOTOFFLO symbol entry.  */
  HOWTO (R_TRICORE_GOTOFFLO,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 FALSE,			/* pc_relative */
	 12,			/* bitpos */
	 complain_overflow_dont,/* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_TRICORE_GOTOFFLO",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0x0ffff000,		/* dst_mask */
	 FALSE), 		/* pcrel_offset */

  /* bolC 16 bit GOTOFFLO symbol entry.  */
  HOWTO (R_TRICORE_GOTOFFLO2,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont,/* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_TRICORE_GOTOFFLO2",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffff0000,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* rlcC 16 bit GOTOFFUP symbol entry.  */
  HOWTO (R_TRICORE_GOTOFFUP,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 FALSE,			/* pc_relative */
	 12,			/* bitpos */
	 complain_overflow_dont,/* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_TRICORE_GOTOFFUP",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0x0ffff000,		/* dst_mask */
	 FALSE), 		/* pcrel_offset */

  /* rlcC 16 bit GOTPC symbol entry.  */
  HOWTO (R_TRICORE_GOTPC,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 FALSE,			/* pc_relative */
	 12,			/* bitpos */
	 complain_overflow_dont,/* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_TRICORE_GOTPC",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0x0ffff000,		/* dst_mask */
	 FALSE), 		/* pcrel_offset */

  /* bolC 16 bit GOTPC symbol entry.  */
  HOWTO (R_TRICORE_GOTPC2,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont,/* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_TRICORE_GOTPC2",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffff0000,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* rlcC 16 bit GOTPCHI symbol entry.  */
  HOWTO (R_TRICORE_GOTPCHI,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 FALSE,			/* pc_relative */
	 12,			/* bitpos */
	 complain_overflow_dont,/* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_TRICORE_GOTPCHI",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0x0ffff000,		/* dst_mask */
	 FALSE), 		/* pcrel_offset */

  /* rlcC 16 bit GOTPCLO symbol entry.  */
  HOWTO (R_TRICORE_GOTPCLO,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 FALSE,			/* pc_relative */
	 12,			/* bitpos */
	 complain_overflow_dont,/* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_TRICORE_GOTPCLO",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0x0ffff000,		/* dst_mask */
	 FALSE), 		/* pcrel_offset */

  /* bolC 16 bit GOTPCLO symbol entry.  */
  HOWTO (R_TRICORE_GOTPCLO2,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont,/* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_TRICORE_GOTPCLO2",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffff0000,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* rlcC 16 bit GOTPCUP symbol entry.  */
  HOWTO (R_TRICORE_GOTPCUP,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 FALSE,			/* pc_relative */
	 12,			/* bitpos */
	 complain_overflow_dont,/* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_TRICORE_GOTPCUP",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0x0ffff000,		/* dst_mask */
	 FALSE), 		/* pcrel_offset */

  /* relB PLT entry.  */
  HOWTO (R_TRICORE_PLT,		/* type */
	 1,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 25,			/* bitsize */
	 TRUE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont,/* complain_on_overflow */
	 bfd_elf_generic_reloc, /* special_function */
	 "R_TRICORE_PLT",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffffff00,		/* dst_mask */
	 TRUE),			/* pcrel_offset */

  /* COPY.  */
  HOWTO (R_TRICORE_COPY,	/* type */
	 0,			/* rightshift */
	 0,			/* size (0 = byte, 1 = short, 2 = long) */
	 0,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont,/* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_TRICORE_COPY",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0,			/* dst_mask */
	 TRUE), 		/* pcrel_offset */

  /* GLOB_DAT.  */
  HOWTO (R_TRICORE_GLOB_DAT,	/* type */
	 0,			/* rightshift */
	 0,			/* size (0 = byte, 1 = short, 2 = long) */
	 0,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont,/* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_TRICORE_GLOB_DAT",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0,			/* dst_mask */
	 TRUE), 		/* pcrel_offset */

  /* JMP_SLOT.  */
  HOWTO (R_TRICORE_JMP_SLOT,	/* type */
	 0,			/* rightshift */
	 0,			/* size (0 = byte, 1 = short, 2 = long) */
	 0,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont,/* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_TRICORE_JMP_SLOT",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0,			/* dst_mask */
	 TRUE), 		/* pcrel_offset */

  /* RELATIVE.  */
  HOWTO (R_TRICORE_RELATIVE,	/* type */
	 0,			/* rightshift */
	 0,			/* size (0 = byte, 1 = short, 2 = long) */
	 0,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont,/* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_TRICORE_RELATIVE",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0,			/* dst_mask */
	 TRUE),	 		/* pcrel_offset */

  /* BITPOS.  */
  HOWTO (R_TRICORE_BITPOS,	/* type */
	 0,			/* rightshift */
	 0,			/* size (0 = byte, 1 = short, 2 = long) */
	 0,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont,/* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_TRICORE_BITPOS",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0,			/* dst_mask */
	 FALSE)	 		/* pcrel_offset */
#if 0
// TO BE REVIEW
    ,
  /* SMALL DATA Baseregister operand 2.  */
  HOWTO (R_TRICORE_SBREG_S2,	/* type */
	 0,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 4,			/* bitsize */
	 FALSE,			/* pc_relative */
	 12,			/* bitpos */
	 complain_overflow_dont,/* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_TRICORE_SBREG_S2",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0xf000,	/* dst_mask */
	 FALSE), 		/* pcrel_offset */

  /* SMALL DATA Baseregister operand 1.  */
  HOWTO (R_TRICORE_SBREG_S1,	/* type */
	 0,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 4,			/* bitsize */
	 FALSE,			/* pc_relative */
	 8,			/* bitpos */
	 complain_overflow_dont,/* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_TRICORE_SBREG_S1",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0x0f00,	/* dst_mask */
	 FALSE),	 		/* pcrel_offset */

  /* SMALL DATA Baseregister destination.  */
  HOWTO (R_TRICORE_SBREG_D,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 4,			/* bitsize */
	 FALSE,			/* pc_relative */
	 28,			/* bitpos */
	 complain_overflow_dont,/* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_TRICORE_SBREG_D",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0xf0000000,/* dst_mask */
	 FALSE)	 		/* pcrel_offset */
#endif
};


/* Describe the mapping between BFD and TriCore relocs.  */

struct elf_reloc_map {
  bfd_reloc_code_real_type bfd_reloc_val;
  enum elf_tricore_reloc_type tricore_val;
};

static const struct elf_reloc_map tricore_reloc_map[] =
{
  {BFD_RELOC_NONE,              R_TRICORE_NONE},
  {BFD_RELOC_TRICORE_32REL,     R_TRICORE_32REL},
  {BFD_RELOC_TRICORE_32ABS,     R_TRICORE_32ABS},
  {BFD_RELOC_TRICORE_24REL,     R_TRICORE_24REL},
  {BFD_RELOC_TRICORE_24ABS,     R_TRICORE_24ABS},
  {BFD_RELOC_TRICORE_16SM,      R_TRICORE_16SM},
  {BFD_RELOC_TRICORE_HIADJ,     R_TRICORE_HIADJ},
  {BFD_RELOC_TRICORE_LO,        R_TRICORE_LO},
  {BFD_RELOC_TRICORE_LO2,       R_TRICORE_LO2},
  {BFD_RELOC_TRICORE_18ABS,     R_TRICORE_18ABS},
  {BFD_RELOC_TRICORE_10SM,      R_TRICORE_10SM},
  {BFD_RELOC_TRICORE_15REL,     R_TRICORE_15REL},
  {BFD_RELOC_TRICORE_HI,        R_TRICORE_HI},
  {BFD_RELOC_TRICORE_16CONST,   R_TRICORE_16CONST},
  {BFD_RELOC_TRICORE_9ZCONST,   R_TRICORE_9ZCONST},
  {BFD_RELOC_TRICORE_9SCONST,   R_TRICORE_9SCONST},
  {BFD_RELOC_TRICORE_8REL,      R_TRICORE_8REL},
  {BFD_RELOC_TRICORE_8CONST,    R_TRICORE_8CONST},
  {BFD_RELOC_TRICORE_10OFF,     R_TRICORE_10OFF},
  {BFD_RELOC_TRICORE_16OFF,     R_TRICORE_16OFF},
  {BFD_RELOC_TRICORE_8ABS,      R_TRICORE_8ABS},
  {BFD_RELOC_TRICORE_16ABS,     R_TRICORE_16ABS},
  {BFD_RELOC_TRICORE_1BIT,      R_TRICORE_1BIT},
  {BFD_RELOC_TRICORE_3POS,      R_TRICORE_3POS},
  {BFD_RELOC_TRICORE_5POS,      R_TRICORE_5POS},
  {BFD_RELOC_TRICORE_PCPHI,     R_TRICORE_PCPHI},
  {BFD_RELOC_TRICORE_PCPLO,     R_TRICORE_PCPLO},
  {BFD_RELOC_TRICORE_PCPPAGE,   R_TRICORE_PCPPAGE},
  {BFD_RELOC_TRICORE_PCPOFF,    R_TRICORE_PCPOFF},
  {BFD_RELOC_TRICORE_PCPTEXT,   R_TRICORE_PCPTEXT},
  {BFD_RELOC_TRICORE_5POS2,     R_TRICORE_5POS2},
  {BFD_RELOC_TRICORE_BRCC,      R_TRICORE_BRCC},
  {BFD_RELOC_TRICORE_BRCZ,      R_TRICORE_BRCZ},
  {BFD_RELOC_TRICORE_BRNN,      R_TRICORE_BRNN},
  {BFD_RELOC_TRICORE_RRN,       R_TRICORE_RRN},
  {BFD_RELOC_TRICORE_4CONST,    R_TRICORE_4CONST},
  {BFD_RELOC_TRICORE_4REL,      R_TRICORE_4REL},
  {BFD_RELOC_TRICORE_4REL2,     R_TRICORE_4REL2},
  {BFD_RELOC_TRICORE_5POS3,     R_TRICORE_5POS3},
  {BFD_RELOC_TRICORE_4OFF,      R_TRICORE_4OFF},
  {BFD_RELOC_TRICORE_4OFF2,     R_TRICORE_4OFF2},
  {BFD_RELOC_TRICORE_4OFF4,     R_TRICORE_4OFF4},
  {BFD_RELOC_TRICORE_42OFF,     R_TRICORE_42OFF},
  {BFD_RELOC_TRICORE_42OFF2,    R_TRICORE_42OFF2},
  {BFD_RELOC_TRICORE_42OFF4,    R_TRICORE_42OFF4},
  {BFD_RELOC_TRICORE_2OFF,      R_TRICORE_2OFF},
  {BFD_RELOC_TRICORE_8CONST2,   R_TRICORE_8CONST2},
  {BFD_RELOC_TRICORE_4POS,      R_TRICORE_4POS},
  {BFD_RELOC_TRICORE_16SM2,     R_TRICORE_16SM2},
  {BFD_RELOC_TRICORE_5REL,      R_TRICORE_5REL},
  {BFD_RELOC_VTABLE_ENTRY,      R_TRICORE_GNU_VTENTRY},
  {BFD_RELOC_VTABLE_INHERIT,    R_TRICORE_GNU_VTINHERIT},
  {BFD_RELOC_TRICORE_PCREL16,	R_TRICORE_PCREL16},
  {BFD_RELOC_TRICORE_PCREL8,	R_TRICORE_PCREL8},
  {BFD_RELOC_TRICORE_GOT,       R_TRICORE_GOT},
  {BFD_RELOC_TRICORE_GOT2,      R_TRICORE_GOT2},
  {BFD_RELOC_TRICORE_GOTHI,     R_TRICORE_GOTHI},
  {BFD_RELOC_TRICORE_GOTLO,     R_TRICORE_GOTLO},
  {BFD_RELOC_TRICORE_GOTLO2,    R_TRICORE_GOTLO2},
  {BFD_RELOC_TRICORE_GOTUP,     R_TRICORE_GOTUP},
  {BFD_RELOC_TRICORE_GOTOFF,    R_TRICORE_GOTOFF},
  {BFD_RELOC_TRICORE_GOTOFF2,   R_TRICORE_GOTOFF2},
  {BFD_RELOC_TRICORE_GOTOFFHI,  R_TRICORE_GOTOFFHI},
  {BFD_RELOC_TRICORE_GOTOFFLO,  R_TRICORE_GOTOFFLO},
  {BFD_RELOC_TRICORE_GOTOFFLO2, R_TRICORE_GOTOFFLO2},
  {BFD_RELOC_TRICORE_GOTOFFUP,  R_TRICORE_GOTOFFUP},
  {BFD_RELOC_TRICORE_GOTPC,     R_TRICORE_GOTPC},
  {BFD_RELOC_TRICORE_GOTPC2,    R_TRICORE_GOTPC2},
  {BFD_RELOC_TRICORE_GOTPCHI,   R_TRICORE_GOTPCHI},
  {BFD_RELOC_TRICORE_GOTPCLO,   R_TRICORE_GOTPCLO},
  {BFD_RELOC_TRICORE_GOTPCLO2,  R_TRICORE_GOTPCLO2},
  {BFD_RELOC_TRICORE_GOTPCUP,   R_TRICORE_GOTPCUP},
  {BFD_RELOC_TRICORE_PLT,       R_TRICORE_PLT},
  {BFD_RELOC_TRICORE_COPY,      R_TRICORE_COPY},
  {BFD_RELOC_TRICORE_GLOB_DAT,  R_TRICORE_GLOB_DAT},
  {BFD_RELOC_TRICORE_JMP_SLOT,  R_TRICORE_JMP_SLOT},
  {BFD_RELOC_TRICORE_RELATIVE,  R_TRICORE_RELATIVE},
  {BFD_RELOC_TRICORE_BITPOS,    R_TRICORE_BITPOS},
#if 0
// TO BE REVIEW
  {BFD_RELOC_TRICORE_SBREG_S2, R_TRICORE_SBREG_S2},
  {BFD_RELOC_TRICORE_SBREG_S1, R_TRICORE_SBREG_S1},
  {BFD_RELOC_TRICORE_SBREG_D,  R_TRICORE_SBREG_D}
#endif
};

static unsigned int nr_maps = sizeof tricore_reloc_map / sizeof tricore_reloc_map[0];

/* TRUE if we should compress bit objects during the relaxation pass.  */

bfd_boolean tricore_elf32_relax_bdata = FALSE;

/* TRUE if we should relax call and jump instructions whose target
   addresses are out of reach.  */

bfd_boolean tricore_elf32_relax_24rel = FALSE;

/* TRUE if we should output diagnostic messages when relaxing sections.  */

bfd_boolean tricore_elf32_debug_relax = FALSE;

/* If the linker was invoked with -M or -Map, we save the pointer to
   the map file in this variable; used to list allocated bit objects
   and other fancy extensions.  */

FILE *tricore_elf32_map_file = (FILE *) NULL;

/* If the linker was invoked with --extmap in addition to -M/-Map, we
   also save the filename of the map file (NULL means stdout).  */

char *tricore_elf32_map_filename = (char *) NULL;

/* TRUE if an extended map file should be produced.  */

bfd_boolean tricore_elf32_extmap_enabled = FALSE;

/* TRUE if the map file should include the version of the linker, the
   date of the link run, and the name of the map file.  */

bfd_boolean tricore_elf32_extmap_header = FALSE;

/* TRUE if the map file should contain an augmented memory segment map.  */

bfd_boolean tricore_elf32_extmap_memory_segments = FALSE;

/* 1 if global symbols should be listed in the map file, 2 if all symbols
   should be listed; symbols are sorted by name.  */

int tricore_elf32_extmap_syms_by_name = 0;

/* 1 if global symbols should be listed in the map file, 2 if all symbols
   should be listed; symbols are sorted by address.  */

int tricore_elf32_extmap_syms_by_addr = 0;

/* Name of the linker; only valid if tricore_elf32_extmap_enabled.  */

char *tricore_elf32_extmap_ld_name = (char *) NULL;

/* Pointer to a function that prints the linker version to a file;
   only valid if tricore_elf32_extmap_enabled.  */

void (*tricore_elf32_extmap_ld_version) (FILE *);

/* Pointer to a function that returns a list of defined memory regions;
   only valid if tricore_elf32_extmap_enabled.  */

memreg_t *(*tricore_elf32_extmap_get_memregs) (int *) = NULL;

/* If >= 0, describes the address mapping scheme for PCP sections.  */

int tricore_elf32_pcpmap = -1;

/* TRUE if PCP address mappings should be printed (for debugging only).  */

bfd_boolean tricore_elf32_debug_pcpmap = FALSE;

/* TRUE if small data accesses should be checked for non-small accesses.  */

bfd_boolean tricore_elf32_check_sdata = FALSE;

/* the core architecture of the executable set in the eflags of the ELF header*/
unsigned long tricore_core_arch = EF_EABI_TRICORE_V1_2;

/* Forward declarations.  */

static reloc_howto_type *tricore_elf32_reloc_type_lookup
      (bfd *, bfd_reloc_code_real_type);

static reloc_howto_type *tricore_elf_reloc_name_lookup
      (bfd *, const char *);

static bfd_boolean tricore_elf32_info_to_howto
      (bfd *, arelent *, Elf_Internal_Rela *);

// static void tricore_elf32_final_sda_bases
//  (bfd *, struct bfd_link_info *);

// static bfd_reloc_status_type tricore_elf32_final_sda_base
//      (asection *, bfd_vma *, int *);

static bfd_boolean tricore_elf32_merge_private_bfd_data (bfd *, struct bfd_link_info *);

static bfd_boolean tricore_elf32_copy_private_bfd_data (bfd *, bfd *);

const bfd_target *tricore_elf32_object_p (bfd *);

// static bfd_boolean tricore_elf32_fake_sections
//      (bfd *, Elf_Internal_Shdr *, asection *);

// static bfd_boolean tricore_elf32_section_flags
//      (flagword *, Elf_Internal_Shdr *);

// static bfd_boolean tricore_elf32_final_gp
//      (bfd *, struct bfd_link_info *);

// static void tricore_elf32_set_arch_mach
//      (bfd *, enum bfd_architecture);

// static bfd_boolean tricore_elf32_size_dynamic_sections
//      (bfd *, struct bfd_link_info *);

// static bfd_boolean tricore_elf32_adjust_dynamic_symbol
//      (struct bfd_link_info *, struct elf_link_hash_entry *);


// static unsigned long tricore_elf32_get_bitpos
//      (bfd *, struct bfd_link_info *, Elf_Internal_Rela *,
//              Elf_Internal_Shdr *, Elf_Internal_Sym *,
//              struct elf_link_hash_entry **, asection *, bfd_boolean *);

// static bfd_boolean tricore_elf32_adjust_bit_relocs
//      (bfd *, struct bfd_link_info *, unsigned long,
//              bfd_vma, bfd_vma, int, unsigned int);


// static void tricore_elf32_list_bit_objects (struct bfd_link_info *);

// static symbol_t *tricore_elf32_new_symentry (void);

// static void tricore_elf32_do_extmap (struct bfd_link_info *);

// static int tricore_elf32_extmap_sort_addr (const void *, const void *);

// static int tricore_elf32_extmap_sort_name (const void *, const void *);

// static int tricore_elf32_extmap_sort_memregs
//      (const void *, const void *);

// static bfd_boolean tricore_elf32_extmap_add_sym
//      (struct bfd_link_hash_entry *, PTR);

// static bfd_boolean tricore_elf32_finish_dynamic_symbol
//      (bfd *, struct bfd_link_info *,
//              struct elf_link_hash_entry *, Elf_Internal_Sym *sym);

// static bfd_boolean tricore_elf32_finish_dynamic_sections
//      (bfd *, struct bfd_link_info *);

// static enum elf_reloc_type_class tricore_elf32_reloc_type_class
//      (const Elf_Internal_Rela *);

// static asection *tricore_elf32_gc_mark_hook
//      (asection *, struct bfd_link_info *, Elf_Internal_Rela *,
//              struct elf_link_hash_entry *, Elf_Internal_Sym *);

// static bfd_boolean tricore_elf32_gc_sweep_hook
//      (bfd *, struct bfd_link_info *, asection *,
//              const Elf_Internal_Rela *);


/* Given a BFD reloc type CODE, return the corresponding howto structure.  */

static reloc_howto_type *
tricore_elf32_reloc_type_lookup (bfd *abfd ATTRIBUTE_UNUSED,
                                 bfd_reloc_code_real_type code)
{
  unsigned int i;

  if (code == BFD_RELOC_8)
    code = BFD_RELOC_TRICORE_8ABS;
  else if (code == BFD_RELOC_16)
    code = BFD_RELOC_TRICORE_16ABS;
  else if (code == BFD_RELOC_32)
    code = BFD_RELOC_TRICORE_32ABS;
  else if (code == BFD_RELOC_32_PCREL)
    code = BFD_RELOC_TRICORE_32REL;
  else if (code == BFD_RELOC_16_PCREL)
    code = BFD_RELOC_TRICORE_PCREL16;
  else if (code == BFD_RELOC_8_PCREL)
    code = BFD_RELOC_TRICORE_PCREL8;

  for (i = 0; i < nr_maps; ++i)
    if (tricore_reloc_map[i].bfd_reloc_val == code)
      return &tricore_elf32_howto_table[(tricore_reloc_map[i].tricore_val)];

  bfd_set_error (bfd_error_bad_value);
  return (reloc_howto_type *) 0;
}

/* Given a BFD reloc name, return the corresponding howto structure.  */

static reloc_howto_type *
tricore_elf_reloc_name_lookup (bfd *abfd ATTRIBUTE_UNUSED,
				  const char *r_name)
{
  unsigned int i;

  for (i = 0; i < nr_maps; i++)
    if (tricore_elf32_howto_table[i].name != NULL
	&& strcasecmp (tricore_elf32_howto_table[i].name, r_name) == 0)
      return &tricore_elf32_howto_table[i];

  return NULL;
}

/* Set CACHE_PTR->howto to the howto entry for the relocation DST.  */

static bfd_boolean
tricore_elf32_info_to_howto (bfd *abfd ATTRIBUTE_UNUSED,
                             arelent *cache_ptr,
                             Elf_Internal_Rela *dst)
{
	unsigned int r_type;

	r_type = ELF32_R_TYPE (dst->r_info);
	if (r_type >= R_TRICORE_max)
		{
		/* xgettext:c-format */
		_bfd_error_handler (_("%pB: unsupported relocation type %#x"),
				abfd, r_type);
		bfd_set_error (bfd_error_bad_value);
		return FALSE;
		}
	
	cache_ptr->howto = &tricore_elf32_howto_table[r_type];

	return TRUE;
}


#if 0
/* This is called once from tricore_elf32_relocate_section, before any
   relocation has been performed.  We need to determine the final offset
   for the GOT pointer, which is currently zero, meaning that the symbol
   "_GLOBAL_OFFSET_TABLE_" will point to the beginning of the ".got"
   section.  There are, however, two cases in which this is undesirable:

      1. If the GOT contains more than 8192 entries, single TriCore
	 instructions can't address the excessive entries with their
	 16-bit signed offset.  Of course, that's only a problem
	 when there are modules compiled with "-fpic".

      2. In a shared object, the GOT pointer is also used to address
         variables in SDA0, so the combined size of the ".got", ".sbss"
	 and ".sdata" sections must not exceed 32k (well, of course the
	 combined size of these sections can be 64k, but not if the
	 GOT pointer offset is zero).

   To address these problems, we use the following algorithm to determine
   the final GOT offset:

      1. If the combined size of the ".got", ".sbss" and ".sdata"
	 sections is <= 32k, we'll keep the zero offset.

      2. If the GOT contains more than 8192 entries, we'll
	 set the offset to 0x8000, unless doing that would
	 render any SDA entries unaccessible.

      3. In all other cases, we'll set the offset to the size
	 of the ".got" section minus 4 (because of the _DYNAMIC
	 entry at _GLOBAL_OFFSET_TABLE_[0]).

   In any case, if either ".sdata" or ".sbss" is non-empty, we're adjusting
   the symbol "_SMALL_DATA_" to have the same value as "_GLOBAL_OFFSET_TABLE_",
   as both the GOT and the SDA are addressed via the same register (%a12).
   Note that the algorithm described above won't guarantee that all GOT
   and SDA entries are reachable using a 16-bit offset -- it's just
   increasing the probability for this to happen.  */

static boolean
tricore_elf32_final_gp (bfd *output_bfd, struct bfd_link_info *info)
{
  asection *sgot;

  sgot = bfd_get_section_by_name (output_bfd, ".got");
  if (sgot && (sgot->_raw_size > 0))
    {
      struct elf_link_hash_entry *h;
      bfd_vma gp, sda;
      asection *sdata = NULL, *sbss = NULL;
      long got_size = sgot->_raw_size, sda_size = 0;
      long gp_offset;

      if (info->shared)
        {
	  sdata = bfd_get_section_by_name (output_bfd, ".sdata");
          sbss = bfd_get_section_by_name (output_bfd, ".sbss");
          if (sdata != NULL)
            {
              if (sdata->_cooked_size != 0)
	        sda_size += sdata->_cooked_size;
	      else
	        sda_size += sdata->_raw_size;
	    }

          if (sbss != NULL)
            {
              if (sbss->_cooked_size != 0)
	        sda_size += sbss->_cooked_size;
	      else
	        sda_size += sbss->_raw_size;
	    }

          if (sda_size > (0x8000 - 4))
            {
	      (*_bfd_error_handler) (_("%s: Too many SDA entries (%ld bytes)"),
	  			     bfd_archive_filename (output_bfd),
				     sda_size);
	      return false;
	    }
        }

      if ((got_size + sda_size) <= 0x8000)
        gp_offset = 0;
      else if ((got_size > 0x8000)
	       && ((sda_size + got_size - 0x8000) <= 0x8000))
	gp_offset = 0x8000;
      else
        gp_offset = got_size - 4;

      if (gp_offset != 0)
	elf_hash_table (info)->hgot->root.u.def.value = gp_offset;

      /* If there's any data in ".sdata"/".sbss", set the value of
         _SMALL_DATA_ to that of the GOT pointer.  */
      if (((sdata != NULL) && (sdata->_raw_size > 0))
          || ((sbss != NULL) && (sbss->_raw_size > 0)))
        {
	  h = (struct elf_link_hash_entry *)
	       bfd_link_hash_lookup (info->hash, "_SMALL_DATA_",
	  			     false, false, false);
	  if (h == NULL)
	    {
	      /* This can't possibly happen, as we're always creating the
	         ".sdata"/".sbss" output sections and the "_SMALL_DATA_"
		 symbol in tricore_elf32_check_relocs.  */
	      (*_bfd_error_handler)
	       (_("%s: SDA entries, but _SMALL_DATA_ undefined"),
	       bfd_archive_filename (output_bfd));
	      return false;
	    }
	  gp = gp_offset + sgot->output_section->vma + sgot->output_offset;
	  sdata = h->root.u.def.section;
	  sda = sdata->output_section->vma + sdata->output_offset;
	  h->root.u.def.value = gp - sda;
	}
    }

  return TRUE;
}
#endif

                                        

/* Check whether it's okay to merge objects IBFD and OBFD.  */

static bfd_boolean
tricore_elf32_merge_private_bfd_data (bfd *ibfd, struct bfd_link_info *info)
{
  bfd_boolean error = FALSE;
  unsigned long mask;

  if (bfd_get_flavour (ibfd) != bfd_target_elf_flavour
      || bfd_get_flavour (info->output_bfd) != bfd_target_elf_flavour)
    return TRUE;

  if ((bfd_get_arch (ibfd) != bfd_arch_tricore) ||
      (bfd_get_arch (info->output_bfd) != bfd_arch_tricore))
    {
      error = TRUE;
      (*_bfd_error_handler)
       (_("%s and/or %s don't use the TriCore architecture."),
        bfd_get_filename (ibfd), bfd_get_filename (info->output_bfd));
    }
  else
    {
      unsigned long new_isa;

      mask = tricore_elf32_convert_eflags(elf_elfheader(ibfd)->e_flags);
      new_isa = mask & EF_EABI_TRICORE_CORE_MASK;
      // old_isa = tricore_core_arch & EF_EABI_TRICORE_CORE_MASK;
      switch (tricore_core_arch & EF_EABI_TRICORE_CORE_MASK)
        {
	case EF_EABI_TRICORE_V1_1:
	  if (new_isa != EF_EABI_TRICORE_V1_1)
	    {
	      error = TRUE;
	    }
	  break;
	case EF_EABI_TRICORE_V1_2:
	case EF_EABI_TRICORE_V1_3:
	  switch (new_isa)
	    {
	    case EF_EABI_TRICORE_V1_1:
	    case EF_EABI_TRICORE_V1_3_1:
	    case EF_EABI_TRICORE_V1_6:
	    case EF_EABI_TRICORE_V1_6_1:
	      error = TRUE;
	      break;
	    }
	  break;
	case EF_EABI_TRICORE_V1_3_1:
	  switch (new_isa)
	    {
	    case EF_EABI_TRICORE_V1_1:
	    case EF_EABI_TRICORE_V1_6:
	    case EF_EABI_TRICORE_V1_6_1:
	      error = TRUE;
	      break;
	    }
	  break;
	case EF_EABI_TRICORE_V1_6:
	  switch (new_isa)
	    {
	    case EF_EABI_TRICORE_V1_1:
	    case EF_EABI_TRICORE_V1_6_1:
	      error = TRUE;
	      break;
	    }
	  break;
	case EF_EABI_TRICORE_V1_6_1:
	  switch (new_isa)
	    {
	    case EF_EABI_TRICORE_V1_1:
	      error = TRUE;
	      break;
	    }
	  break;
	}


      if (error == TRUE)
	{
          (*_bfd_error_handler)
           ("%s uses an incompatible TriCore instruction set architecture.",
            bfd_get_filename (ibfd));
	}
      else
        {
	  elf_elfheader (info->output_bfd)->e_flags = tricore_core_arch;
	}
    }

  if (error)
    {
      bfd_set_error (bfd_error_wrong_format);
      return FALSE;
    }

  return TRUE;
}

/* Copy e_flags from IBFD to OBFD.  */

static bfd_boolean
tricore_elf32_copy_private_bfd_data (bfd *ibfd, bfd *obfd)
{
  bfd_boolean error = FALSE;

  if (bfd_get_flavour (ibfd) != bfd_target_elf_flavour
      || bfd_get_flavour (obfd) != bfd_target_elf_flavour)
    return TRUE;

  if (bfd_get_arch (ibfd) != bfd_arch_tricore)
    {
      error = TRUE;
      (*_bfd_error_handler)
       (_("%s doesn't use the TriCore architecture."), bfd_get_filename (ibfd));
    }
  else
    elf_elfheader (obfd)->e_flags = elf_elfheader (ibfd)->e_flags;

  if (error)
    {
      bfd_set_error (bfd_error_bad_value);
      return FALSE;
    }

  return TRUE;
}

/* Set the correct machine number (i.e., the ID for the instruction set
   architecture) for a TriCore ELF file.  */

// static void
// tricore_elf32_set_arch_mach (bfd *abfd, enum bfd_architecture arch)
// {
//   bfd_arch_info_type *ap, *def_ap;
//   unsigned long mach;

//   if (arch != bfd_arch_tricore)
//     return; /* Case already handled by bfd_default_set_arch_mach.  */

//   mach = tricore_elf32_convert_eflags (elf_elfheader (abfd)->e_flags) & EF_EABI_TRICORE_CORE_MASK;

//   /* Find the default arch_info.  */
//   def_ap = (bfd_arch_info_type *) bfd_scan_arch (DEFAULT_ISA);

//   /* Scan all sub-targets of the default architecture until we find
//      the one that matches "mach".  If we find a target that is not
//      the current default, we're making it the new default.  */
//   for (ap = def_ap; ap != NULL; ap = (bfd_arch_info_type *) ap->next)
//     if (ap->mach == mach)
//       {
// 	abfd->arch_info = ap;
// 	return;
//       }

//   abfd->arch_info = &bfd_default_arch_struct;
//   bfd_set_error (bfd_error_bad_value);
// }

/* This hack is needed because it's not possible to redefine the
   function bfd_default_set_arch_mach.  Since we need to set the
   correct instruction set architecture, we're redefining
   bfd_elf32_object_p below (but calling it here to do the real work)
   and then we're calling tricore_elf32_set_arch_mach to set the
   correct ISA.  */

// const bfd_target *
// tricore_elf32_object_p (bfd *abfd)
// {
//   const bfd_target *bt;
//   struct elf_backend_data *ebd;
//   extern const bfd_target *elf_object_p (bfd *);

//   if ((bt = bfd_elf32_object_p (abfd)) != NULL)
//     {
//       ebd = abfd->xvec->backend_data;
//       tricore_elf32_set_arch_mach (abfd, ebd->arch);
//     }

//   return bt;
// }


/* Now #define all necessary stuff to describe this target.  */

#define USE_RELA			1
#define ELF_ARCH			bfd_arch_tricore
#define ELF_MACHINE_CODE		EM_TRICORE
#define ELF_MAXPAGESIZE			0x4000
#define TARGET_LITTLE_SYM		tricore_elf32_le_vec
#define TARGET_LITTLE_NAME		"elf32-tricore"
#define bfd_elf32_bfd_reloc_type_lookup	tricore_elf32_reloc_type_lookup
#define bfd_elf32_bfd_reloc_name_lookup tricore_elf_reloc_name_lookup
//#define bfd_elf32_object_p		tricore_elf32_object_p
#define bfd_elf32_bfd_merge_private_bfd_data tricore_elf32_merge_private_bfd_data
#define bfd_elf32_bfd_copy_private_bfd_data tricore_elf32_copy_private_bfd_data
#define elf_info_to_howto		tricore_elf32_info_to_howto
#define elf_info_to_howto_rel		0

#include "elf32-target.h"