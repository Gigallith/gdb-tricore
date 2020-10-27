#
# Linker script template for TriCore ELF objects and executables
# (embedded version).
#
# Copyright (C) 1998-2005 Free Software Foundation, Inc.
# Contributed by Michael Schumacher (mike@hightec-rt.com).
#
# When adding sections, do note that the names of some sections are used
# when specifying the start address of the next.
#
test -z "$ENTRY" && ENTRY=_start
if [ -z "$MACHINE" ]; then OUTPUT_ARCH=${ARCH}; else OUTPUT_ARCH=${ARCH}:${MACHINE}; fi
test "$LD_FLAG" = "N" && DATA_ADDR=.

# if this is for an embedded system, don't add SIZEOF_HEADERS.
if [ -z "$EMBEDDED" ]; then
   test -z "${TEXT_BASE_ADDRESS}" && TEXT_BASE_ADDRESS="${TEXT_START_ADDR} + SIZEOF_HEADERS"
else
   test -z "${TEXT_BASE_ADDRESS}" && TEXT_BASE_ADDRESS="${TEXT_START_ADDR}"
fi
Q=\"

cat <<EOF
OUTPUT_FORMAT("${OUTPUT_FORMAT}")
OUTPUT_ARCH(${OUTPUT_ARCH})
${RELOCATING+ENTRY(${ENTRY})}
${RELOCATING+${EXECUTABLE_SYMBOLS}}
${RELOCATING+

/*
 * NB: The memory map below works for TriCore simulators and some
 * evaluation boards.  You may need to change the values to match
 * your actual hardware setup and pass the modified script to the
 * linker using its ${Q}-T${Q} option. Alternativly you can pass a 
 * text file with the following symbol definition and a MEMORY section
 * to the linker to override the default definitions
 * (look at memory.x in the compiler specific directories for an example)
 */

/* the external RAM description */
__EXT_CODE_RAM_BEGIN = DEFINED(__EXT_CODE_RAM_BEGIN)? __EXT_CODE_RAM_BEGIN : 0xa0000000;
__EXT_CODE_RAM_SIZE  = DEFINED(__EXT_CODE_RAM_SIZE)? __EXT_CODE_RAM_SIZE : 512K ;
__EXT_DATA_RAM_BEGIN = DEFINED(__EXT_DATA_RAM_BEGIN)? __EXT_DATA_RAM_BEGIN : 0xa0000000 + 512K;
__EXT_DATA_RAM_SIZE  = DEFINED(__EXT_DATA_RAM_SIZE)? __EXT_DATA_RAM_SIZE : 512K;
/*
 * used to check for HEAP_SIZE
 */
__RAM_END = __EXT_DATA_RAM_BEGIN + __EXT_DATA_RAM_SIZE;
/* the internal ram description */
__INT_CODE_RAM_BEGIN = DEFINED(__INT_CODE_RAM_BEGIN)? __INT_CODE_RAM_BEGIN : 0xd400000;
__INT_CODE_RAM_SIZE  = DEFINED(__INT_CODE_RAM_SIZE)? __INT_CODE_RAM_SIZE : 32K;
__INT_DATA_RAM_BEGIN = DEFINED(__INT_DATA_RAM_BEGIN)? __INT_DATA_RAM_BEGIN : 0xd0000000;
__INT_DATA_RAM_SIZE  = DEFINED(__INT_DATA_RAM_SIZE)? __INT_DATA_RAM_SIZE : 28K;
/* the pcp memory description */
__PCP_CODE_RAM_BEGIN = DEFINED(__PCP_CODE_RAM_BEGIN)? __PCP_CODE_RAM_BEGIN : 0xf0020000;
__PCP_CODE_RAM_SIZE  = DEFINED(__PCP_CODE_RAM_SIZE)? __PCP_CODE_RAM_SIZE : 32K;
__PCP_DATA_RAM_BEGIN = DEFINED(__PCP_DATA_RAM_BEGIN)? __PCP_DATA_RAM_BEGIN : 0xf0010000;
__PCP_DATA_RAM_SIZE  = DEFINED(__PCP_DATA_RAM_SIZE)? __PCP_DATA_RAM_SIZE : 16K;

MEMORY
{
  ext_cram (rx!p):	org = 0xa0000000, len = 512K
  ext_dram (w!xp):	org = 0xa0080000, len = 512K
  int_cram (rx!p):	org = 0xd4000000, len = 32K
  int_dram (w!xp):	org = 0xd0000000, len = 28K
  pcp_data (wp!x):	org = 0xf0010000, len = 32K
  pcp_text (rxp):	org = 0xf0020000, len = 16K
"}"


/*
 * Define the sizes of the user and system stacks.
 */
__ISTACK_SIZE = DEFINED (__ISTACK_SIZE) ? __ISTACK_SIZE : 4K ;
__USTACK_SIZE = DEFINED (__USTACK_SIZE) ? __USTACK_SIZE : 20K ;

/*
 * The heap is the memory between the top of the user stack and
 * __RAM_END (as defined above); programs can dynamically allocate
 * space in this area using malloc() and various other functions.
 * Below you can define the minimum amount of memory that the heap
 * should provide.
 */
__HEAP_MIN = DEFINED (__HEAP_MIN) ? __HEAP_MIN : 64K ;

}

SECTIONS
{
  ${RELOCATING+
  /*
   * The startup code should be placed where the CPU expects it after a reset,
   * so we try to locate it first, no matter where it appears in the list of
   * objects and libraries (note: because the wildcard pattern doesn't match
   * directories, we'll try to find crt0.o in various (sub)directories).
   */
  .startup :
  {
    KEEP (*(.startup_code))
	. = ALIGN(8);    
  "}" ${RELOCATING+> ext_cram =${NOP-0}}
  }
  ${RELOCATING+
  /*
   * Allocate space for absolute addressable sections; this requires that
   * ${Q}int_dram${Q} starts at a TriCore segment (256M) and points to
   * some RAM area!  If these conditions are not met by your particular
   * hardware setup, you should either not use absolute data, or you
   * must move .zdata*,.zbss*,.bdata*,.bbss* input sections to some appropriate
   * memory area.
   */
  }
  
  ${RELOCATING-
   .zrodata 0:
  {
    *(.zrodata)
    *(.zrodata.*)
  "}" 
  }
 
  ${RELOCATING-
  .bbss 0 :
  {
	  *(.bbss)
	  *(.bbss.*)
  "}"
  }

 .zbss ${RELOCATING+ (NOLOAD)}${RELOCATING-0 }:
  {
    ${RELOCATING+ZBSS_BASE = . ;}
    *(.zbss)
    ${RELOCATING+*(.zbss.*)}
    ${RELOCATING+*(.gnu.linkonce.zb.*)}
	${RELOCATING+*(.bbss)}
    ${RELOCATING+*(.bbss.*)}
	${RELOCATING+. = ALIGN(8);}
    ${RELOCATING+ZBSS_END = . ;}
  } ${RELOCATING+> int_dram}

  ${RELOCATING-
  .bbata 0 :
  {
	  *(.bbata)
	  *(.bbata.*)
  "}"
  }
  
  .zdata ${RELOCATING- 0 }:
  {
    ${RELOCATING+ZDATA_BASE = . ;}
 	${RELOCATING+*(.zrodata)}
	${RELOCATING+*(.zrodata.*)}
   *(.zdata)
	${RELOCATING+*(.zdata.*)}
    ${RELOCATING+*(.gnu.linkonce.z.*)}
    ${RELOCATING+*(.bdata)}
    ${RELOCATING+*(.bdata.*)}
    ${RELOCATING+. = ALIGN(8);}
    ${RELOCATING+ZDATA_END = . ;}
  } ${RELOCATING+> int_dram AT> ext_cram}


  ${RELOCATING+
	  
  /* define the CSA Memory area as an own section
   * this section will be allocated into the internal RAM
   * after the absolute addressable sections .zdata/.zbss
   * and allocate all internal memory not occupied by .zdata/.zbss
  */
  .csa (NOLOAD) :
  {
    . = ALIGN(64);
    __CSA_BEGIN = . ;
    . +=  __INT_DATA_RAM_BEGIN + __INT_DATA_RAM_SIZE - ABSOLUTE(__CSA_BEGIN);
    . = ALIGN(64);
    __CSA_END = .;
  "}" > int_dram
  __CSA_SIZE = __CSA_END - __CSA_BEGIN;

  /*
   * Allocate trap and interrupt vector tables.
   */
  .traptab  :
  {
    *(.traptab)
    . = ALIGN(8) ;
  "}" > ext_cram

  .inttab  :
  {
    *(.inttab)
    . = ALIGN(8) ;
  "}" > ext_cram
  }

  
  
  
  ${RELOCATING+
  /*
   * Allocate .text and other read-only sections.
   */
  }
  .text ${RELOCATING- 0 }:
  {
    ${RELOCATING+${TEXT_START_SYMBOLS}}
    *(.text)
    ${RELOCATING+
	*(.text.*)
    *(.pcp_c_ptr_init)
    *(.pcp_c_ptr_init.*)
    *(.gnu.linkonce.t.*)
    /*
     * .gnu.warning sections are handled specially by elf32.em.
     */
    *(.gnu.warning)
	}
	${RELOCATING+. = ALIGN(8);}
  } ${RELOCATING+> ext_cram =${NOP-0}}

  .rodata ${RELOCATING- 0 } :
  {
    *(.rodata)
    ${RELOCATING+
	*(.rodata.*)
	*(.gnu.linkonce.r.*)
    *(.rodata1)
    *(.toc)
    /*
     * Create the clear and copy tables that tell the startup code
     * which memory areas to clear and to copy, respectively.
     */
    . = ALIGN(4) ;
    PROVIDE(__clear_table = .) ;
    LONG(0 + ADDR(.bss));     LONG(SIZEOF(.bss));
    LONG(0 + ADDR(.sbss));    LONG(SIZEOF(.sbss));
    LONG(0 + ADDR(.zbss));    LONG(SIZEOF(.zbss));
    LONG(-1);                 LONG(-1);

    PROVIDE(__copy_table = .) ;
    LONG(LOADADDR(.data));    LONG(0 + ADDR(.data)); LONG(SIZEOF(.data));
    LONG(LOADADDR(.sdata));   LONG(0 + ADDR(.sdata));LONG(SIZEOF(.sdata));
    LONG(LOADADDR(.zdata));   LONG(0 + ADDR(.zdata));LONG(SIZEOF(.zdata));
    LONG(LOADADDR(.pcpdata)); LONG(0 + ADDR(.pcpdata)); LONG(SIZEOF(.pcpdata));
    LONG(LOADADDR(.pcptext)); LONG(0 + ADDR(.pcptext));LONG(SIZEOF(.pcptext));
    LONG(-1);                 LONG(-1);                  LONG(-1);
    . = ALIGN(8);
    }
  } ${RELOCATING+> ext_cram}

  .sdata2 ${RELOCATING- 0 }:
  {
    *(.sdata.rodata)
    *(.sdata.rodata.*)
    ${RELOCATING+. = ALIGN(8);}
  } ${RELOCATING+> ext_cram}


  ${RELOCATING+
  /*
   * C++ exception handling tables.  NOTE: gcc emits .eh_frame
   * sections when compiling C sources with debugging enabled (-g).
   * If you can be sure that your final application consists
   * exclusively of C objects (i.e., no C++ objects), you may use
   * the -R option of the ${Q}strip${Q} and ${Q}objcopy${Q} utilities to remove
   * the .eh_frame section from the executable.
   */
  }
  .eh_frame ${RELOCATING- 0 }:
  {
    ${RELOCATING+*(.gcc_except_table)}
    ${RELOCATING+__EH_FRAME_BEGIN__ = . ;}
    ${RELOCATING+KEEP (*(.eh_frame))}
    ${RELOCATING-*(.eh_frame)}
    ${RELOCATING+__EH_FRAME_END__ = . ;}
	${RELOCATING+. = ALIGN(8);}
  } ${RELOCATING+> ext_cram}

  ${RELOCATING+
  /*
   * Constructors and destructors.
   */
  .ctors :
  {
    __CTOR_LIST__ = . ;
    LONG((__CTOR_END__ - __CTOR_LIST__) / 4 - 2);
    *(.ctors)
    LONG(0) ;
    __CTOR_END__ = . ;
	. = ALIGN(8);
  "}" > ext_cram

  .dtors :
  {
    __DTOR_LIST__ = . ;
    LONG((__DTOR_END__ - __DTOR_LIST__) / 4 - 2);
    *(.dtors)
    LONG(0) ;
    __DTOR_END__ = . ;
	. = ALIGN(8);
  "}" > ext_cram

  /*
   * We"'"re done now with the text part of the executable.  The
   * following sections are special in that their initial code or
   * data (if any) must also be stored in said text part of an
   * executable, but they ${Q}live${Q} at completely different addresses
   * at runtime -- usually in RAM areas.  NOTE: This is not really
   * necessary if you use a special program loader (e.g., a debugger)
   * to load a complete executable consisting of code, data, BSS, etc.
   * into the RAM of some target hardware or a simulator, but it *is*
   * necessary if you want to burn your application into non-volatile
   * memories such as EPROM or FLASH.
   */
  }

  .pcptext${RELOCATING- 0 }:
  {
    ${RELOCATING+PCODE_BASE = . ;}
    *(.pcptext)
    ${RELOCATING+*(.pcptext.*)}
    ${RELOCATING+. = ALIGN(8) ;}
   ${RELOCATING+PCODE_END = . ;}
  } ${RELOCATING+> pcp_text AT> ext_cram}

  .pcpdata${RELOCATING- 0 }: 
  {
    ${RELOCATING+PRAM_BASE = . ;}
    *(.pcpdata)
    ${RELOCATING+*(.pcpdata.*)}
    ${RELOCATING+. = ALIGN(8) ;}
    ${RELOCATING+PRAM_END = . ;}
  } ${RELOCATING+> pcp_data AT> ext_cram}

  .data${RELOCATING- 0 }: 
  {
    ${RELOCATING+. = ALIGN(8) ;}
    ${RELOCATING+DATA_BASE = . ;}
    *(.data)
    ${RELOCATING+*(.data.*)}
    ${RELOCATING+*(.gnu.linkonce.d.*)}
    ${CONSTRUCTING+SORT(CONSTRUCTORS)}
    ${RELOCATING+. = ALIGN(8) ;}
    ${RELOCATING+DATA_END = . ;}
  } ${RELOCATING+> ext_dram AT> ext_cram}

  .sdata ${RELOCATING- 0 }: 
  {
	${RELOCATING+. = ALIGN(8) ;}
	${RELOCATING+SDATA_BASE = . ;}
    ${RELOCATING+PROVIDE(__sdata_start = .);}
    *(.sdata)
    ${RELOCATING+*(.sdata.*)}
    ${RELOCATING+*(.gnu.linkonce.s.*)}
    ${RELOCATING+. = ALIGN(8) ;}
  } ${RELOCATING+> ext_dram AT> ext_cram}

  .sbss ${RELOCATIING+ (NOLOAD)}${RELOCATING-0 }:
  {
    ${RELOCATING+PROVIDE(__sbss_start = .);}
    *(.sbss)
    ${RELOCATING+*(.sbss.*)}
    ${RELOCATING+*(.gnu.linkonce.sb.*)}
    ${RELOCATING+. = ALIGN(8) ;}
  } ${RELOCATING+> ext_dram}

  ${RELOCATING+
  /*
   * Allocate space for BSS sections.
   */
  }
  .bss ${RELOCATING+ (NOLOAD)}${RELOCATING-0 }:
  {
    ${RELOCATING+BSS_BASE = . ;}
    ${RELOCATING+${OTHER_BSS_SYMBOLS}}
    *(.bss)
    ${RELOCATING+
    *(.bss.*)
    *(.gnu.linkonce.b.*)
    *(COMMON)
    . = ALIGN(8) ;
    __ISTACK = . + __ISTACK_SIZE ;
    __USTACK = __ISTACK + __USTACK_SIZE ;
    __HEAP = __USTACK ;
    __HEAP_END = __RAM_END ;
    }
  } ${RELOCATING+> ext_dram}
  ${RELOCATING+_end = __HEAP_END ;}
  ${RELOCATING+PROVIDE(end = _end) ;}

  ${RELOCATING+

  /* Make sure CSA, stack and heap addresses are properly aligned.  */
  _. = ASSERT ((__CSA_BEGIN & 0x3f) == 0 , ${Q}illegal CSA start address${Q}) ;
  _. = ASSERT ((__CSA_SIZE & 0x3f) == 0 , ${Q}illegal CSA size${Q}) ;
  _. = ASSERT ((__ISTACK & 7) == 0 , ${Q}ISTACK not doubleword aligned${Q}) ;
  _. = ASSERT ((__USTACK & 7) == 0 , ${Q}USTACK not doubleword aligned${Q}) ;
  _. = ASSERT ((__HEAP_END & 7) == 0 , ${Q}HEAP not doubleword aligned${Q}) ;

  /* Make sure enough memory is available for stacks and heap.  */
  _. = ASSERT (__ISTACK <= __RAM_END , ${Q}not enough memory for ISTACK${Q}) ;
  _. = ASSERT (__USTACK <= __RAM_END , ${Q}not enough memory for USTACK${Q}) ;
  _. = ASSERT ((__HEAP_END - __HEAP) >= __HEAP_MIN ,
               ${Q}not enough memory for HEAP${Q}) ;

  /* Define a default symbol for address 0.  */
  NULL = DEFINED (NULL) ? NULL : 0 ;
  }

  /*
   * DWARF debug sections.
   * Symbols in the DWARF debugging sections are relative to the
   * beginning of the section, so we begin them at 0.
   */

  /*
   * DWARF 1
   */
  .comment         0 : { *(.comment) }
  .debug           0 : { *(.debug) }
  .line            0 : { *(.line) }

  /*
   * GNU DWARF 1 extensions
   */
  .debug_srcinfo   0 : { *(.debug_srcinfo) }
  .debug_sfnames   0 : { *(.debug_sfnames) }

  /*
   * DWARF 1.1 and DWARF 2
   */
  .debug_aranges   0 : { *(.debug_aranges) }
  .debug_pubnames  0 : { *(.debug_pubnames) }

  /*
   * DWARF 2
   */
  .debug_info      0 : { *(.debug_info) }
  .debug_abbrev    0 : { *(.debug_abbrev) }
  .debug_line      0 : { *(.debug_line) }
  .debug_frame     0 : { *(.debug_frame) }
  .debug_str       0 : { *(.debug_str) }
  .debug_loc       0 : { *(.debug_loc) }
  .debug_macinfo   0 : { *(.debug_macinfo) }
  .debug_ranges    0 : { *(.debug_ranges) }

  /*
   * SGI/MIPS DWARF 2 extensions
   */
  .debug_weaknames 0 : { *(.debug_weaknames) }
  .debug_funcnames 0 : { *(.debug_funcnames) }
  .debug_typenames 0 : { *(.debug_typenames) }
  .debug_varnames  0 : { *(.debug_varnames) }

  ${RELOCATING+
  /*
   * Optional sections that may only appear when relocating.
   */
  ${OTHER_RELOCATING_SECTIONS}
  }

  /*
   * Optional sections that may appear regardless of relocating.
   */
  ${OTHER_SECTIONS}
}
EOF
