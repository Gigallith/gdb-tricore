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

#ifndef TRICORE_TDEP_H
#define TRICORE_TDEP_H 1

/* TriCore architecture-specific information.  */
struct gdbarch_tdep
{
    // We need struct gdbarch_info
    struct gdbarch_info *info;
    
};

#ifdef __VIRTUAL_IO__
#include <sys/types.h>
#include <fcntl.h>
#include "gdb_stat.h"
#endif /* __VIRTUAL_IO__ */

#ifdef TSIM_CYCLES
#define CYCLES_REGNUM 44
#define INSTR_REGNUM 45
#define TIME_REGNUM 46
//#define TRICORE_NUM_REGS 47
#else
//#define TRICORE_NUM_REGS 44
#endif

/* Register numbers.  */
enum tricore_regnum
{
    TRICORE_D0_REGNUM,
    TRICORE_D1_REGNUM,
    TRICORE_D2_REGNUM,
    TRICORE_D3_REGNUM,
    TRICORE_D4_REGNUM, TRICORE_DARG0_REGNUM = TRICORE_D4_REGNUM,
    TRICORE_D5_REGNUM,
    TRICORE_D6_REGNUM,
    TRICORE_D7_REGNUM, TRICORE_DARGLAST_REGNUM = TRICORE_D7_REGNUM,
    TRICORE_D8_REGNUM,
    TRICORE_D9_REGNUM,
    TRICORE_D10_REGNUM,
    TRICORE_D11_REGNUM,
    TRICORE_D12_REGNUM,
    TRICORE_D13_REGNUM,
    TRICORE_D14_REGNUM,
    TRICORE_D15_REGNUM,
    TRICORE_A0_REGNUM,
    TRICORE_A1_REGNUM,
    TRICORE_A2_REGNUM,
    TRICORE_A3_REGNUM,
    TRICORE_A4_REGNUM, TRICORE_STRUCT_RETURN_REGNUM = TRICORE_A4_REGNUM, TRICORE_AARG0_REGNUM = TRICORE_A4_REGNUM,
    TRICORE_A5_REGNUM,
    TRICORE_A6_REGNUM,
    TRICORE_A7_REGNUM, TRICORE_AARGLAST_REGNUM = TRICORE_A7_REGNUM,
    TRICORE_A8_REGNUM,
    TRICORE_A9_REGNUM,
    TRICORE_A10_REGNUM, TRICORE_FP_REGNUM = TRICORE_A10_REGNUM, TRICORE_SP_REGNUM = TRICORE_A10_REGNUM,
    TRICORE_A11_REGNUM, TRICORE_RA_REGNUM = TRICORE_A11_REGNUM,
    TRICORE_A12_REGNUM,
    TRICORE_A13_REGNUM,
    TRICORE_A14_REGNUM,
    TRICORE_A15_REGNUM,
    TRICORE_LCX_REGNUM,
    TRICORE_FCX_REGNUM,
    TRICORE_PCXI_REGNUM,
    TRICORE_PSW_REGNUM,
    TRICORE_PC_REGNUM,
    TRICORE_ICR_REGNUM,
    TRICORE_ISP_REGNUM,
    TRICORE_BTV_REGNUM,
    TRICORE_BIV_REGNUM,
    TRICORE_SYSCON_REGNUM,
    TRICORE_PMUCON0_REGNUM,
    TRICORE_DMUCON_REGNUM,
    TRICORE_NUM_REGS
};

#define INT_REGISTER_SIZE 4

#endif /* tricore-tdep.h */