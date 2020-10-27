/* Target-dependent code for Infineon TriCore.

   Copyright (C) 2009-2020 Free Software Foundation, Inc.

   This file is part of GDB.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

#include "defs.h"
#include "arch-utils.h"
#include "dis-asm.h"
#include "frame.h"
#include "trad-frame.h"
#include "symtab.h"
#include "value.h"
#include "gdbcmd.h"
#include "gdbcore.h"
#include "breakpoint.h"
#include "inferior.h"
#include "regcache.h"
#include "target.h"
#include "frame-base.h"
#include "frame-unwind.h"
#include "dwarf2/frame.h"
#include "osabi.h"
#include "target-descriptions.h"
#include "opcodes/tricore-dis.h"
#include "tricore-tdep.h"
#include "remote.h"
#include "elf-bfd.h"
#include "elf/tricore.h"
#include "tricore.h"
#include "objfiles.h"
#include "gdbtypes.h"
#include "gdbarch.h"
#include "opcodes/disassemble.h"
#include "features/tricore.c"

#define TRICORE_BREAKPOINT      {0x00, 0xa0} /* debug */
constexpr gdb_byte tricore_break_insn[] = TRICORE_BREAKPOINT;
typedef BP_MANIPULATION (tricore_break_insn) tricore_breakpoint;

/* The registers of the Infineon TriCore processor.  */

static const char *tricore_register_names[] =
{
    "d0", "d1", "d2",  "d3",  "d4",  "d5",  "d6",  "d7",
    "d8", "d9", "d10", "d11", "d12", "d13", "d14", "d15",
    "a0", "a1", "a2",  "a3",  "a4",  "a5",  "a6",  "a7",
    "a8", "a9", "a10", "a11", "a12", "a13", "a14", "a15",
    "lcx", "fcx", "pcxi", "psw", "pc", "icr", "isp",
    "btv", "biv", "syscon", "pmucon0", "dmucon"
};

#define TRICORE_NUM_REGS ARRAY_SIZE (tricore_register_names)

static unsigned int tricore_debug_flag = 0;

/* This array describes which registers are stored in the context save
   area (CSA) upon execution of a call instruction or an exception.
   We use this to find the saved registers of a given frame.  */

static int csa_upper_regs_a[] =
{
  TRICORE_PCXI_REGNUM, TRICORE_PSW_REGNUM, TRICORE_SP_REGNUM, TRICORE_RA_REGNUM, TRICORE_A12_REGNUM, TRICORE_A13_REGNUM,
  TRICORE_A14_REGNUM, TRICORE_A15_REGNUM, TRICORE_D8_REGNUM, TRICORE_D9_REGNUM, TRICORE_D10_REGNUM, TRICORE_D11_REGNUM,
  TRICORE_D12_REGNUM, TRICORE_D13_REGNUM, TRICORE_D14_REGNUM, TRICORE_D15_REGNUM
};

static const int num_csa_upper_regs = sizeof (csa_upper_regs_a) / sizeof (int);

/* Same as above, but for the lower context.  */

static int csa_lower_regs_a[] =
{
  TRICORE_PCXI_REGNUM, TRICORE_PC_REGNUM, TRICORE_A2_REGNUM, TRICORE_A3_REGNUM, TRICORE_A4_REGNUM, TRICORE_A5_REGNUM,
  TRICORE_A6_REGNUM, TRICORE_A7_REGNUM, TRICORE_D0_REGNUM, TRICORE_D1_REGNUM, TRICORE_D2_REGNUM, TRICORE_D3_REGNUM,
  TRICORE_D4_REGNUM, TRICORE_D5_REGNUM, TRICORE_D6_REGNUM, TRICORE_D7_REGNUM
};

static const int num_csa_lower_regs = sizeof (csa_lower_regs_a) / sizeof (int);

/* Upper/lower contexts for Rider-B.  */

static int csa_upper_regs_b[] =
{
  TRICORE_PCXI_REGNUM, TRICORE_PSW_REGNUM, TRICORE_SP_REGNUM, TRICORE_RA_REGNUM, TRICORE_D8_REGNUM, TRICORE_D9_REGNUM,
  TRICORE_D10_REGNUM, TRICORE_D11_REGNUM, TRICORE_A12_REGNUM, TRICORE_A13_REGNUM, TRICORE_A14_REGNUM, TRICORE_A15_REGNUM,
  TRICORE_D12_REGNUM, TRICORE_D13_REGNUM, TRICORE_D14_REGNUM, TRICORE_D15_REGNUM
};

static int csa_lower_regs_b[] =
{
  TRICORE_PCXI_REGNUM, TRICORE_PC_REGNUM, TRICORE_A2_REGNUM, TRICORE_A3_REGNUM, TRICORE_D0_REGNUM, TRICORE_D1_REGNUM,
  TRICORE_D2_REGNUM, TRICORE_D3_REGNUM, TRICORE_A4_REGNUM, TRICORE_A5_REGNUM, TRICORE_A6_REGNUM, TRICORE_A7_REGNUM,
  TRICORE_D4_REGNUM, TRICORE_D5_REGNUM, TRICORE_D6_REGNUM, TRICORE_D7_REGNUM
};

static int *csa_upper_regs = csa_upper_regs_b;
static int *csa_lower_regs = csa_lower_regs_b;

static const char *
tricore_register_name (struct gdbarch *gdbarch, int regnum)
{
  if (regnum >= 0 && regnum < TRICORE_NUM_REGS)
    return tricore_register_names[regnum];
  return NULL;
}

/* Which instruction set architecure do we use?  */

static tricore_isa current_isa = TRICORE_V1_2;
#define RIDER_A (current_isa == TRICORE_V1_1)
#define RIDER_B (current_isa == TRICORE_V1_2)

/* Check what ISA is actually in use.  */

static void
tricore_find_isa (struct gdbarch_info *info)
{
  unsigned long mask;

  if (info->abfd == NULL)
    return;

  // mask = tricore_elf32_convert_eflags (elf_elfheader (info->abfd)->e_flags);

  mask = EF_EABI_TRICORE_V1_2;
  
  switch (mask & EF_EABI_TRICORE_CORE_MASK)
    {
      case EF_EABI_TRICORE_V1_1:
	current_isa = TRICORE_V1_1;
	csa_upper_regs = csa_upper_regs_a;
	csa_lower_regs = csa_lower_regs_a;
	break;

      case EF_EABI_TRICORE_V1_2:
      case EF_EABI_TRICORE_V1_3:
      case EF_EABI_TRICORE_V1_3_1:
      case EF_EABI_TRICORE_V1_6:
      case EF_EABI_TRICORE_V1_6_1:
	current_isa = TRICORE_V1_2;
	csa_upper_regs = csa_upper_regs_b;
	csa_lower_regs = csa_lower_regs_b;
	break;

      default:
	error ("Unknown TriCore ISA in ELF header detected.");
    }
}

/* Find the first real insn of the function starting at PC.  On the
   TriCore, a prologue (as produced by gcc) looks like this:

   > If the workaround for Rider-D's cpu13 bug is enabled:
       dsync                    0480000d
     If additonally the workaround for Rider-B/D's cpu9 bug is enabled:
       nop                      0000
       nop                      0000
     or
       nop                      0000000d
       nop                      0000000d

   > If the frame pointer (%a1) is used:
       st.a [+%sp]-8,%ax        f5b8ax89

       mov.aa %ax,%sp           ax80      RIDER-A only
     or
       mov.aa %ax,%sp           ax40      RIDER-B/D only
     or
       mov.aa %ax,%sp           x000a001

   > If space is needed to store local variables on the stack:
       sub.a %sp,const8         xx40      RIDER-A only
     or
       sub.a %sp,const8         xx20      RIDER-B/D only
     or
       lea %sp,[%sp]const16     xxxxaad9
     or
       movh.a %a15,const16      fxxxx091
       (lea %a15,[%a15]const16  xxxxffd9) if const16 != 0
       sub.a %sp,%sp,%a15       a020fa01

   > If the TOC pointer (%a12) needs to be loaded:
       movh.a %a12,hi:toc       cxxxx091
       lea %a12,[%a12]lo:toc    xxxxccd9

   > If this is main, then __main is called:
       call __main              xxxxxx6d

   Note that registers are not saved explicitly, as this is done
   automatically by the call insn.  */

static CORE_ADDR
tricore_skip_prologue (struct gdbarch *gdbarch, CORE_ADDR pc)
{
  CORE_ADDR insn;
  CORE_ADDR main_pc, __main_pc, offset;
  struct symtab_and_line sal;
  struct bound_minimal_symbol sym;
  struct gdbarch_tdep *tdep = gdbarch_tdep (gdbarch);
  struct gdbarch_info *info =tdep->info;

  if (tricore_debug_flag)
    fprintf_unfiltered (gdb_stdlog, "*** tricore_skip_prologue (0x%08lx) = ", pc);

  tricore_find_isa (info);

  /* Check if PC points to main ().  */
  main_pc = __main_pc = (CORE_ADDR) 0;
  sym = lookup_minimal_symbol_text ("main", (struct objfile *) NULL);
  if (sym.minsym)
    {
      if (BMSYMBOL_VALUE_ADDRESS (sym) == pc)
        {
          main_pc = pc;
          sym = lookup_minimal_symbol_text ("__main",
                                            (struct objfile *) NULL);
          if (sym.minsym)
            __main_pc = BMSYMBOL_VALUE_ADDRESS (sym);
        }
    }

  insn = read_memory_integer (pc, 4, gdbarch_byte_order (gdbarch));

  /* Handle Rider-B/D workarounds.  */
  if (RIDER_B && (insn == 0x0480000d)) /* dsync  */
    {
      pc += 4;
      insn = read_memory_integer (pc, 4, gdbarch_byte_order (gdbarch));

      /* Skip 1 or 2 16- or 32-bit NOPs, if present.  */
      if (insn == 0x00000000) /* 2 16-bit NOPs.  */
        {
          pc += 4;
          insn = read_memory_integer (pc, 4, gdbarch_byte_order (gdbarch));
        }
      else if (insn == 0x0000000d) /* 1st 32-bit NOP.  */
        {
          pc += 4;
          insn = read_memory_integer (pc, 4, gdbarch_byte_order (gdbarch));
          if (insn == 0x0000000d) /* 2nd 32-bit NOP.  */
            {
              pc += 4;
              insn = read_memory_integer (pc, 4, gdbarch_byte_order (gdbarch));
            }
        }
      else if ((insn & 0xffff0000) == 0x00000000) /* Only 1 16-bit NOP.  */
        {
          pc += 2;
          insn = read_memory_integer (pc, 4, gdbarch_byte_order (gdbarch));
        }
    }

  /* Handle stack and frame pointer manipulation.  */
  if (RIDER_A && ((insn & 0xff) == 0x40)) /* sub.a %sp,const8  */
    pc += 2;
  else if (RIDER_B && ((insn & 0xff) == 0x20)) /* sub.a %sp,const8  */
    pc += 2;
  else
    {
      if ((insn & 0xfffff0ff) == 0xf5b8a089) /* st.a [+%sp]-8,%ax  */
        {
          pc += 4;
          insn = read_memory_integer (pc, 4, gdbarch_byte_order (gdbarch));

          if (RIDER_A && ((insn & 0xf0ff) == 0xa080)) /* mov.aa %an,%sp  */
            {
              pc += 2;
              insn = read_memory_integer (pc, 4, gdbarch_byte_order (gdbarch));
            }
          else if (RIDER_B && ((insn & 0xf0ff) == 0xa040)) /* mov.aa %an,%sp  */
            {
              pc += 2;
              insn = read_memory_integer (pc, 4, gdbarch_byte_order (gdbarch));
            }
          else if ((insn & 0x0fffffff) == 0x0000a001) /* mov.aa %an,%sp  */
            {
              pc += 4;
              insn = read_memory_integer (pc, 4, gdbarch_byte_order (gdbarch));
            }
        }

      if (RIDER_A && ((insn & 0xff) == 0x40)) /* sub.a %sp,const8  */
        pc += 2;
      else if (RIDER_B && ((insn & 0xff) == 0x20)) /* sub.a %sp,const8  */
        pc += 2;
      else if ((insn & 0xffff) == 0xaad9) /* lea %sp,[%sp]const16  */
        pc += 4;
      else if ((insn & 0xf0000fff) == 0xf0000091) /* movh.a %a15,const16  */
        {
          CORE_ADDR old_pc = pc;

          pc += 4;
          insn = read_memory_integer (pc, 4, gdbarch_byte_order (gdbarch));
          if ((insn & 0x0000ffff) == 0x0000ffd9) /* lea %a15,[%a15]const16  */
            {
              pc += 4;
              insn = read_memory_integer (pc, 4, gdbarch_byte_order (gdbarch));
            }
          if (insn == 0xa020fa01) /* sub.a %sp,%sp,%a15  */
            pc += 4;
          else
            pc = old_pc;
        }
    }

  /* Handle TOC pointer.  */
  insn = read_memory_integer (pc, 4, gdbarch_byte_order (gdbarch));
  if ((insn & 0xf0000fff) == 0xc0000091)
    {
      pc += 8;
      if (main_pc != (CORE_ADDR) 0)
        insn = read_memory_integer (pc, 4, gdbarch_byte_order (gdbarch));
    }

  /* Check for "call __main".  FIXME: Should also check for CALLA etc.  */
  if (main_pc != (CORE_ADDR) 0)
    {
      if ((insn & 0x000000ff) == 0x0000006d)
        {
          offset = (insn & 0xffff0000) >> 16;
          offset |= (insn & 0x0000ff00) << 8;
          if (offset & 0x800000)
            offset |= ~0xffffff;
          offset <<= 1;
          if ((pc + offset) == __main_pc)
            pc += 4;
        }
      else if (RIDER_B && ((insn & 0xff) == 0x5c))
        {
          offset = (insn & 0x0000ff00) >> 8;
          if (offset & 0x80)
            offset |= ~0xff;
          offset <<= 1;
          if ((pc + offset) == __main_pc)
            pc += 2;
        }
    }

  if (tricore_debug_flag)
    fprintf_unfiltered (gdb_stdlog, "0x%08lx\n", pc);

  return pc;
}

static void
tricore_frame_this_id (struct frame_info *this_frame,
		     void **prologue_cache,
		     struct frame_id *this_id)
{

  try
    {
      // HAVE TO THINK HERE
    }
  catch (const gdb_exception_error &ex)
    {
      /* Ignore errors, this leaves the frame id as the predefined outer
         frame id which terminates the backtrace at this point.  */
    }
}


static const struct frame_unwind tricore_frame_unwind =
{
  NORMAL_FRAME,
  default_frame_unwind_stop_reason,
  tricore_frame_this_id,
  (value* (*)(frame_info*, void**, int)) 0XB,
  (const frame_data*) 0XC,
  default_frame_sniffer
};

/* Return number of args passed to a frame.
   Can return -1, meaning no way to tell. */

static int
tricore_frame_num_args (struct frame_info *fi)
{
#if 0 /* WT 2006-04-19 */
  return -1;
#else
  return 0;
#endif /* WT 2006-04-19 */
}

/* TriCore uses CSAs (Context Save Area) in a linked list.
   The normal stack concept cannot be used for TriCore. */
static int
tricore_inner_than (CORE_ADDR lhs, CORE_ADDR rhs)
{
  if (tricore_debug_flag)
    fprintf_unfiltered (gdb_stdlog, "*** tricore_inner_than: lhs=0x%08lx, rhs=0x%08lx\n", lhs, rhs);
  return 0;
}

/* Write the return value in SRC with type TYPE into the
   appropriate register(s).  This is called when making the
   current frame returning using "ret value_to_be_returned".  */

static void
tricore_store_return_value (struct type *type, struct regcache *regs,
			    const gdb_byte *src)
{
  int regno, len;

  if ((type->code () == TYPE_CODE_PTR) ||
      (type->code () == TYPE_CODE_REF))
    regno = TRICORE_A2_REGNUM;
  else
    regno = TRICORE_D2_REGNUM;

  /* Sanity checking... */
  if ((len = TYPE_LENGTH (type)) > 8)
    len = 8;

  regs->cooked_write_part (regno, 0, len, src);

}

/* Copy the return value in REGS with type TYPE to DST.  This is
   used to find out the return value of a function after a "finish"
   command has been issued, and after a call dummy has returned.  */

static void
tricore_extract_return_value (struct type *type,
                              struct regcache *regs,
                              gdb_byte *dst)
{
  int regno, len;

  if ((type->code () == TYPE_CODE_PTR) ||
      (type->code () == TYPE_CODE_REF))
    regno = TRICORE_A2_REGNUM;
  else
    regno = TRICORE_D2_REGNUM;

  /* Sanity checking... */
  if ((len = TYPE_LENGTH (type)) > 8)
    len = 8;

  regs->cooked_read_part (regno, 0, len, dst); 

}


/* Setting/getting return values from functions.

   If USE_STRUCT_CONVENTION returns 0, then gdb uses STORE_RETURN_VALUE
   and EXTRACT_RETURN_VALUE to store/fetch the functions return value. */

/* Will a function return an aggregate type in memory or in a
   register?  Return 0 if an aggregate type can be returned in a
   register, 1 if it must be returned in memory.  */

static enum return_value_convention
tricore_return_value (struct gdbarch *gdbarch, struct value *function,
		    struct type *type, struct regcache *regcache,
		    gdb_byte *readbuf, const gdb_byte *writebuf)
{
  int struct_return = type->code () == TYPE_CODE_STRUCT
		      || type->code () == TYPE_CODE_UNION
		      || type->code () == TYPE_CODE_ARRAY;

  if (writebuf != NULL)
    {
      gdb_assert (!struct_return);
      tricore_store_return_value (type, regcache, writebuf);
    }

  if (readbuf != NULL)
    {
      gdb_assert (!struct_return);
      tricore_extract_return_value (type, regcache, readbuf);
    }

  /* All aggregate types larger than double word are returned in memory */
  if (TYPE_LENGTH (type) > 8)
    return RETURN_VALUE_STRUCT_CONVENTION;
  return RETURN_VALUE_REGISTER_CONVENTION;
}

/* Figure out where the longjmp will land.  We expect the first arg (%a4)
   to be a pointer to the jmp_buf structure from which we extract the PC
   that we will land at.  The PC is copied into *PC.  This routine returns
   true on success.  */

static int
tricore_get_longjmp_target (struct frame_info *frame, CORE_ADDR *pc)
{
  CORE_ADDR a4;
  struct gdbarch *gdbarch = get_frame_arch (frame);
  //a4 = read_register (TRICORE_A4_REGNUM); deprecated, we should use FRAME
  a4 = get_frame_register_unsigned(frame, TRICORE_A4_REGNUM);
  *pc = read_memory_integer (a4, 4, gdbarch_byte_order (gdbarch));

  return 1;
}

/* Caveat: Writing to TriCore's scratch pad RAM (SPRAM) is only allowed
   in chunks of 32 bits and only at 32-bit-aligned addresses.  Since a
   breakpoint instruction ("debug") only takes 16 bits, we need to be
   careful when inserting/removing breakpoints.  */


static int
tricore_memory_insert_breakpoint (struct gdbarch *gdbarch, struct bp_target_info *bp_tgt)
{
  CORE_ADDR addr = bp_tgt->placed_address = bp_tgt->reqstd_address;
  int val, offs;
  gdb_byte bp[] = TRICORE_BREAKPOINT;
  gdb_byte contents_cache[4];

  /* Save the memory contents.  */
  val = target_read_memory (addr & ~3, contents_cache, 4);
  if (val != 0)
    return val;			/* return error */

  memcpy (bp_tgt->shadow_contents, contents_cache, 4);
  bp_tgt->shadow_len = 4;


  /* Write the breakpoint.  */
  /* check word alignment */
  offs = ((addr & 3) ? 2 : 0);
  memcpy(contents_cache + offs, bp, 2);
  val = target_write_memory (addr & ~3, contents_cache, 4);

  return val;
}

static int
tricore_memory_remove_breakpoint (struct gdbarch *gdbarch,
			       struct bp_target_info *bp_tgt)
{
  CORE_ADDR addr = bp_tgt->placed_address;
  gdb_byte *contents_cache = bp_tgt->shadow_contents;
  gdb_byte mem_cache[4];
  int val, offs;

  val = target_read_memory (addr & ~3, mem_cache, 4);
  if (val != 0)
    return val;			/* return error */
  
  offs = ((addr & 3) ? 2 : 0);
  memcpy(mem_cache + offs, contents_cache + offs , 2);  

  val = target_write_memory (addr & ~3, mem_cache, 4);

  return val;
}

static const unsigned char *
tricore_breakpoint_from_pc (struct gdbarch *gdbarch, CORE_ADDR *pcptr, int *lenptr)
{
  static const unsigned char breakpoint_insn[] = TRICORE_BREAKPOINT;

  *lenptr = sizeof (breakpoint_insn);
  return breakpoint_insn;
}

static struct gdbarch *
tricore_gdbarch_init (struct gdbarch_info info, struct gdbarch_list *arches)
{
  struct gdbarch_tdep *tdep;
  struct gdbarch *gdbarch;
  struct tdesc_arch_data *tdesc_data = NULL;
  const struct target_desc *tdesc = info.target_desc;

  /* If there is already a candidate, use it.  */
  arches = gdbarch_list_lookup_by_info (arches, &info);
  if (arches != NULL)
    return arches->gdbarch;
  if (tdesc == NULL) {
    initialize_tdesc_tricore ();
    tdesc = tdesc_tricore;
  }
    

  /* Check any target description for validity.  */
  if (tdesc_has_registers (tdesc))
    {
      const struct tdesc_feature *feature;
      int valid_p;
      int i;

      feature = tdesc_find_feature (tdesc,
                                    "org.gnu.gdb.tricore.core");
      if (feature == NULL)
        return NULL;
      tdesc_data = tdesc_data_alloc ();

      valid_p = 1;
      for (i = 0; i < TRICORE_NUM_REGS; i++)
        valid_p &= tdesc_numbered_register (feature, tdesc_data, i,
                                            tricore_register_names[i]);
      if (!valid_p)
        {
          tdesc_data_cleanup (tdesc_data);
          return NULL;
        }
    }

  /* Allocate space for the new architecture.  */
  tdep = XCNEW (struct gdbarch_tdep);
  gdbarch = gdbarch_alloc (&info, tdep);
  tdep->info = &info;

  /* Register info */
  set_gdbarch_num_regs (gdbarch, TRICORE_NUM_REGS);
  set_gdbarch_pc_regnum (gdbarch, TRICORE_PC_REGNUM);
  set_gdbarch_sp_regnum (gdbarch, TRICORE_SP_REGNUM);
  set_gdbarch_deprecated_fp_regnum (gdbarch, TRICORE_FP_REGNUM);
  set_gdbarch_ps_regnum (gdbarch, TRICORE_PSW_REGNUM);

  set_gdbarch_num_pseudo_regs (gdbarch, 0);
  set_gdbarch_register_name (gdbarch, tricore_register_name);
  // set_gdbarch_deprecated_register_size (gdbarch, 4);
  // set_gdbarch_deprecated_register_bytes (gdbarch, TRICORE_NUM_REGS * 4);
  // set_gdbarch_deprecated_register_byte (gdbarch, tricore_register_byte);
  // set_gdbarch_deprecated_register_raw_size (gdbarch, tricore_register_raw_size);
  // set_gdbarch_deprecated_max_register_raw_size (gdbarch, 4);
  // set_gdbarch_deprecated_register_virtual_size (gdbarch, tricore_register_virtual_size);
  // set_gdbarch_deprecated_max_register_virtual_size (gdbarch, 4);
  // set_gdbarch_deprecated_register_virtual_type (gdbarch, tricore_register_virtual_type);
  // set_gdbarch_deprecated_register_convertible (gdbarch, tricore_register_convertible);

/* Frame and stack info */
  set_gdbarch_skip_prologue (gdbarch, tricore_skip_prologue);
  //set_gdbarch_deprecated_saved_pc_after_call (gdbarch, tricore_saved_pc_after_call);

  set_gdbarch_frame_num_args (gdbarch, tricore_frame_num_args);

  // set_gdbarch_deprecated_frame_chain (gdbarch, tricore_frame_chain);
  // set_gdbarch_deprecated_frame_saved_pc (gdbarch, tricore_frame_saved_pc);

  // set_gdbarch_deprecated_frame_args_address (gdbarch, tricore_frame_args_address);
  // set_gdbarch_deprecated_frame_locals_address (gdbarch, tricore_frame_locals_address);

  // set_gdbarch_deprecated_frame_init_saved_regs (gdbarch, tricore_frame_init_saved_regs);
  // set_gdbarch_deprecated_get_saved_register (gdbarch, tricore_get_saved_register);
  // set_gdbarch_deprecated_init_extra_frame_info (gdbarch, tricore_init_extra_frame_info);

  /* Unwind the frame.  */
  dwarf2_append_unwinders (gdbarch);
  frame_unwind_append_unwinder (gdbarch, &tricore_frame_unwind);
  frame_base_append_sniffer (gdbarch, dwarf2_frame_base_sniffer);

  set_gdbarch_inner_than (gdbarch, tricore_inner_than);

  set_gdbarch_decr_pc_after_break (gdbarch, 0);
  //set_gdbarch_deprecated_function_start_offset (gdbarch, 0);
  set_gdbarch_frame_args_skip (gdbarch, 0);

  /* Return value info */
  //set_gdbarch_deprecated_extract_struct_value_address (gdbarch, tricore_extract_struct_value_address);
  set_gdbarch_return_value (gdbarch, tricore_return_value);
   
 
  //set_gdbarch_deprecated_push_return_address (gdbarch, tricore_push_return_address);
  //set_gdbarch_deprecated_reg_struct_has_addr (gdbarch, tricore_reg_struct_has_addr);
  //set_gdbarch_extract_return_value (gdbarch, tricore_extract_return_value);
  //set_gdbarch_store_return_value (gdbarch, tricore_store_return_value);

  /* Call dummy info */
  //set_gdbarch_deprecated_push_dummy_frame (gdbarch, tricore_push_dummy_frame);
  // set_gdbarch_deprecated_pop_frame (gdbarch, tricore_pop_frame);
  // set_gdbarch_deprecated_fix_call_dummy (gdbarch, tricore_fix_call_dummy);
  // set_gdbarch_deprecated_pc_in_call_dummy (gdbarch, tricore_pc_in_call_dummy);
  set_gdbarch_call_dummy_location (gdbarch, ON_STACK);
  // set_gdbarch_deprecated_call_dummy_words (gdbarch, tricore_call_dummy_words);
  // set_gdbarch_deprecated_call_dummy_length (gdbarch, 0);
  // set_gdbarch_deprecated_sizeof_call_dummy_words (gdbarch, sizeof_tricore_call_dummy_words);
  // set_gdbarch_deprecated_call_dummy_start_offset (gdbarch, 0);
  // set_gdbarch_deprecated_call_dummy_breakpoint_offset (gdbarch, 8);

  /* we build our own dummy frames: tricore_push_dummy_frame */
  //set_gdbarch_deprecated_use_generic_dummy_frames (gdbarch, 0);



  //set_gdbarch_deprecated_push_arguments (gdbarch, tricore_push_arguments);
  set_gdbarch_get_longjmp_target (gdbarch, tricore_get_longjmp_target);

  /* Breakpoint support */
  set_gdbarch_memory_insert_breakpoint (gdbarch, tricore_memory_insert_breakpoint);
  set_gdbarch_memory_remove_breakpoint (gdbarch, tricore_memory_remove_breakpoint);
  set_gdbarch_breakpoint_from_pc (gdbarch, tricore_breakpoint_from_pc);
    set_gdbarch_breakpoint_kind_from_pc (gdbarch,
				       tricore_breakpoint::kind_from_pc);
  set_gdbarch_sw_breakpoint_from_kind (gdbarch,
				       tricore_breakpoint::bp_from_kind);
#ifdef USE_NEXTPC
//  set_gdbarch_software_single_step (gdbarch, tricore_software_single_step);
#endif /* USE_NEXTPC */

  set_gdbarch_print_insn (gdbarch, default_print_insn);

  /* Hook in OS ABI-specific overrides, if they have been registered.  */
  gdbarch_init_osabi (info, gdbarch);
  
  if (tdesc_data != NULL)
    tdesc_use_registers (gdbarch, tdesc, tdesc_data);

  return gdbarch;
}

void _initialize_tricore_tdep ();
void
_initialize_tricore_tdep ()
{
  register_gdbarch_init (bfd_arch_tricore, tricore_gdbarch_init);

  /* Debug this files internals.  */
  add_setshow_zuinteger_cmd ("tricore", class_maintenance,
			     &tricore_debug_flag, _("\
Set tricore debugging."), _("\
Show tricore debugging."), _("\
When non-zero, tricore specific debugging is enabled."),
			     NULL,
			     NULL,
			     &setdebuglist, &showdebuglist);

}