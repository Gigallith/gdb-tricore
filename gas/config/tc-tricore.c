/* tc-tricore.c -- Assemble code for Infineon's TriCore MPU/MCU/DSP
		   (incl. MMU, FPU) and its on-chip PCP coprocessor.
   Copyright (C) 1998-2005 Free Software Foundation.
   Contributed by Michael Schumacher (mike@hightec-rt.com).

   This file is part of GAS, the GNU Assembler.

   GAS is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   GAS is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with GAS; see the file COPYING.  If not, write to the Free
   Software Foundation, 59 Temple Place - Suite 330, Boston, MA
   02111-1307, USA.  */

#include "as.h"
#include "subsegs.h"
#include "obstack.h"
#include "dwarf2dbg.h"
#include "opcode/tricore.h"
#include "elf/tricore.h"
#include "bfd.h"
#include "elf-bfd.h"
#ifdef HTC_SUPPORT
#include "htc/htc_support.h"
#endif
#include <assert.h>

#define MAX_OPS 5
#define MAX_PCPOPS 5
#define MAX_RELAXED_INSN_SIZE 12
#define INITIAL_INSNLINELENGTH 1024
#define NUMOPCODES tricore_numopcodes
#define NUMPCPOPCODES pcp_numopcodes
#define NUMSFRS tricore_numsfrs
#define NO_RELOC _dummy_first_bfd_reloc_code_real
#define BFD_RELOC_TRICORE_PCPREL6 BFD_RELOC_NONE+1
#define BFD_RELOC_TRICORE_PCPREL10 BFD_RELOC_NONE+2
#define CHECK_RELOC(i) if ((reloc = find_relocation (i)) == NO_RELOC) return
#define VAL(i) the_insn.opexp[i].X_add_number
#define PCPVAL(i) pcp_insn.opexp[i].X_add_number
#define SP_REGNUM 10
#define OPC_ADDR(f) (valueT) ((f)->fr_opcode)
#define BFD_RELOC_TRICORE_24CALL BFD_RELOC_TRICORE_24REL
#define GET_RELOC_NAME(code) \
	  ((code <= BFD_RELOC_NONE) \
	   ? bfd_get_reloc_code_name (code) \
	   : pcp_internal_reloc_name[code - BFD_RELOC_NONE])
#define MATCHES_ISA(isa) \
	  (((isa) == TRICORE_GENERIC) \
	   || ((isa & TRICORE_ISA_MASK) & current_isa))

/* This array holds the chars that always start a comment.  If the
   pre-processor is disabled, these aren't very useful.  */

const char comment_chars[] = "#";

/* This array holds the chars that only start a comment at the beginning
   of a line.  If the line seems to have the form '# 123 filename',
   .line and .file directives will appear in the pre-processed output.
   Note that input_file.c hand checks for '#' at the beginning of the
   first line of the input file.  This is because the compiler outputs
   #NO_APP at the beginning of its output.  Also note that comments
   started like this one will always work, no matter if the pre-processor
   is enabled or not.  */

const char line_comment_chars[] = "#";

/* These chars may be used to separate lines (i.e., insns, labels and
   pseudo-ops).  We're using the semicolon for this purpose, as it
   cannot be part of any legal TriCore/PCP insn.  */

const char line_separator_chars[] = ";";

/* Chars that can be used to separate the mantissa from the exponent
   in floating point numbers.  */

const char EXP_CHARS[] = "eE";

/* Chars that mean this number is a floating point constant, as in
   0f12.456 or 0d1.2345e12.  */

const char FLT_CHARS[] = "fFdD";

/* Additional assembler flags.  */

const char *md_shortopts = "VYm:";

/* Additional assembler options.  */

#define OPTION_NOOPT		(OPTION_MD_BASE)
#define OPTION_INSN32_ONLY	(OPTION_MD_BASE + 1)
#define OPTION_INSN32_PREF	(OPTION_MD_BASE + 2)
#define OPTION_ENFORCE_ALIGN	(OPTION_MD_BASE + 3)

struct option md_longopts[] =
{
  {"dont-optimize", no_argument, NULL, OPTION_NOOPT},
  {"insn32-only", no_argument, NULL, OPTION_INSN32_ONLY},
  {"insn32-preferred", no_argument, NULL, OPTION_INSN32_PREF},
  {"enforce-aligned-data", no_argument, NULL, OPTION_ENFORCE_ALIGN},
  {NULL, no_argument, NULL, 0}
};

size_t md_longopts_size = sizeof md_longopts;

/* Additional and overridden pseudo opcodes.  */

static void set_16bit_code_flag (int);
static void set_32bit_code_flag (int);
static void set_optimize_flag (int);
static void make_section (int);
static void pcp_init_word (int);
static void create_bit (int);
static void bpcons (int);
static void tricore_uacons (int);
#define TOC_SECTION	0
#define RODATA_SECTION	1
#define SDATA_SECTION	2
#define SBSS_SECTION	3
#define ZDATA_SECTION	4
#define ZBSS_SECTION	5
#define PCPTEXT_SECTION	6
#define PCPDATA_SECTION 7
const pseudo_typeS md_pseudo_table[] =
{
  {"code16", set_16bit_code_flag, 1},
  {"code32", set_32bit_code_flag, 1},
  {"optim", set_optimize_flag, 1},
  {"noopt", set_optimize_flag, 0},
  {"word", cons, 4},
  {"blcomm", s_lcomm_bytes, 1},
  {"toc", make_section, TOC_SECTION},
  {"rodata", make_section, RODATA_SECTION},
  {"sdata", make_section, SDATA_SECTION},
  {"sbss", make_section, SBSS_SECTION},
  {"zdata", make_section, ZDATA_SECTION},
  {"zbss", make_section, ZBSS_SECTION},
  {"pcptext", make_section, PCPTEXT_SECTION},
  {"pcpdata", make_section, PCPDATA_SECTION},
  {"pcpinitword", pcp_init_word, 0},
  {"bit", create_bit, 1},
  {"lbit", create_bit, 0},
  {"bpos", bpcons, 1},
  {"bposb", bpcons, 1},
  {"bposh", bpcons, 2},
  {"bposw", bpcons, 4},
  {"uahalf", tricore_uacons, 2},
  {"uaword", tricore_uacons, 4},
  {"uaxword", tricore_uacons, 8},
  {"file", (void (*) (int)) dwarf2_directive_file, 0},
  {"loc", dwarf2_directive_loc, 0},
  {NULL, 0, 0}
};

/* 1 means be extra verbose (for debugging only).  */

static int show_internals = 0;

/* 1 means use a 32-bit insn even if there's a 16-bit equivalent.  */

static int big_insns = 0;

/* 1 means use only 32-bit insns and ignore .code16 pseudo-ops.  */

static int big_insns_only = 0;

/* 1 means the next insn should be assembled into a 16-bit insn.  */

static int use_insn16 = 0;

/* 1 means the next insn should be assembled into a 32-bit insn.  */

static int use_insn32 = 0;

/* 1 means try to find the shortest opcode that matches the given mnemonic.  */

static int optimize = 1;

/* 1 means the next insn should be optimized, even if optimize == 0.  */

static int optimize_next;

/* 1 means always auto-align data created by .hword/.word/... pseudo-ops.  */

static int enforce_aligned_data = 0;

/* 1 means don't auto-align data created by .uahalf/.uaword/... pseudo-ops.  */

static int tricore_no_align_cons = 0;

/* 1 means automatically insert two NOPs after a DSYNC instruction.  */

static int workaround_cpu9 = 0;

/* 1 means automatically insert a NOP after a DSYNC instruction.  */

static int workaround_cpu34 = 0;

/* 1 means automatically insert a NOP between a LD.[A,DA] and an
   indirect jump or call instruction immediately following it.  */

static int workaround_cpu48 = 0;

/* 1 means automatically insert a NOP after some multi-cycle instructions
   when followed by a load instruction.  */

static int workaround_cpu50 = 0;

/* 1 means automatically insert a NOP between a LD.[A,DA] and a
   LD.[D,W] instruction immediately following it.  */

static int workaround_cpu60 = 0;

/* 1 means automatically insert a NOP after a RSLCX instruction.  */

static int workaround_cpu69 = 0;

/* 1 means automatically insert 1-2 NOPs between a conditional jump
   instruction and a LOOP instruction immediately following it.  */

static int workaround_cpu70 = 0;

/* 1 means automatically insert a NOP between a LD.[A,DA] and a
   LOOP instruction immediately following it.  */

static int workaround_cpu72 = 0;

/* 1 means gripe when seeing a LD.[A,DA] %A10,... instruction.  */

static int workaround_cpu81 = 0;

/* 1 means automatically insert a NOP between a ST[LU]CX and a
   LD.x instruction immediately following it.  */

static int workaround_cpu82 = 0;

/* 1 means automatically insert a NOP after a DISABLE instruction.  */

static int workaround_cpu83 = 0;

/* 1 means automatically insert 1 NOP between a conditional jump
   in the IP (the jump uses a data register) and a context sensitive
   instruction CALL, CALLA, CALLI, SYSCALL, RET, RFE */

static int workaround_cpu94 = 0;

/* 1 means automatically insert 1 NOP between a sat.b/sat.h and a following
   addsc.a, addsc.at, mov.a, mtcr which use a DGPR as a source */

static int workaround_cpu95 = 0;

/* 1 means automatically insert a NOP as required by DMI_TC.012 HW bug. */

static int workaround_dmi12 = 0;

/* Names of additional PCP relocations handling PC-relative jumps.
   These relocs are only used (and resolved) internally, meaning
   they will never appear in object files.  For debugging only.  */

static const char *pcp_internal_reloc_name[] =
{
  "BFD_RELOC_NONE (unused)",
  "BFD_RELOC_TRICORE_PCPREL6 (internal)",
  "BFD_RELOC_TRICORE_PCPREL10 (internal)"
};

/* Additional sections that can be defined using pseudo-ops.  */

static segT toc_section;	/* Table-of-contents section.  */
static segT rodata_section;	/* Read-only data section.  */
static segT sdata_section;	/* Small data section.  */
static segT sbss_section;	/* Small bss section.  */
static segT zdata_section;	/* Absolute addressable data section.  */
static segT zbss_section;	/* Absolute addressable bss section.  */
static segT pcptext_section;	/* PCP text section.  */
static segT pcpdata_section;	/* PCP data section.  */

#define AR_FLAGS		SEC_ALLOC | SEC_RELOC
#define ARLD_FLAGS		AR_FLAGS | SEC_LOAD | SEC_DATA
//#define PCP_SEG			SEC_ARCH_BIT_0

typedef struct _asec_t
{
  int sec_exists;		/* 1 if this section has been created.  */
  int sec_align;		/* The alignment for this section.  */
  const char *sec_name;		/* The name of this section.  */
  segT *sec_ptr;		/* Pointer to section descriptor.  */
  const int sec_flags;		/* Section flags (BFD style).  */
} asec_t;

static asec_t addsecs[] =
{
  {0, 3, ".toc",    &toc_section,    ARLD_FLAGS | SEC_READONLY},
  {0, 3, ".rodata", &rodata_section, ARLD_FLAGS | SEC_READONLY},
  {0, 3, ".sdata",  &sdata_section,  ARLD_FLAGS},
  {0, 3, ".sbss",   &sbss_section,   AR_FLAGS},
  {0, 3, ".zdata",  &zdata_section,  ARLD_FLAGS},
  {0, 3, ".zbss",   &zbss_section,   AR_FLAGS},
  {0, 1, ".pcptext",&pcptext_section, /* PCP_SEG | */ AR_FLAGS | SEC_LOAD | SEC_CODE},
  {0, 6, ".pcpdata",&pcpdata_section, /* PCP_SEG | */ ARLD_FLAGS}
};

static const int nr_addsecs = sizeof addsecs / sizeof addsecs[0];

/* Some handy shortcuts.  */

typedef struct tricore_opcode opcode_t;
typedef const struct tricore_core_register sfr_t;
typedef struct pcp_opcode pcp_opcode_t;

/* These are used to hash opcodes and SFRs.  */

static struct htab *hash_ops = NULL;
static struct htab *hash_sfr = NULL;
static struct htab *hash_pcp = NULL;

/* This is used to quickly find compatible operand types.  */

static char *pseudo_codes[256];

/* Names of TriCore instruction formats (same order as in tricore.h).  */

static const char *fmt_name[] =
{
  "ABS", "ABSB", "B", "BIT", "BO", "BOL", "BRC", "BRN", "BRR",
  "RC", "RCPW", "RCR", "RCRR", "RCRW", "RLC", "RR", "RR1", "RR2",
  "RRPW", "RRR", "RRR1", "RRR2", "RRRR", "RRRW", "SYS",
  "SB", "SBC", "SBR", "SBRN", "SC", "SLR", "SLRO", "SR",
  "SRC", "SRO", "SRR", "SRRS", "SSR", "SSRO"
};

/* This is used to sort relocations in ascending order, while preserving
   the order of multiple relocations for the same instruction.  */

typedef struct _rel_entry
{
  arelent *reloc;
  unsigned int index;
} rel_entry;

/* Forward declarations.  */

int md_estimate_size_before_relax (register fragS *, register segT);
void md_apply_fix3 (fixS *, valueT *, segT);
long md_pcrel_from_section (fixS *, segT);
static void check_insn_tables (void);
static char classify_const (long);
static int read_regno (char **str);
static char read_regsuffix (char **str);
static int get_expression (const char *, char **, int);
static void tricore_ip (const char *str);
static void insert_reg_or_const (int, int, char, int);
static opcode_t *insn16_to_insn32 (void);
static void delete_operand (int);
static opcode_t *optimize_insn (opcode_t *);
static void optimize_16bit_jumps (void);
static opcode_t *find_opcode (void);
static void print_the_insn (void);
static int insert_abs18 (unsigned long *, int);
static enum bfd_reloc_code_real find_relocation (int);
static void emit_code (void);
static int compare_relocs (const void *, const void *);
//static void pcp_ip (const char *);
//static int get_pcp_expression (const char *, char *, int, int);
//static pcp_opcode_t *find_pcp_opcode (void);
//static void print_pcp_insn (void);
//static int pcp_encode (void);
//static void pcp_emit_code (void);
//static void pcp_assemble (const char *);
static void md_pcp_apply_fix (fixS *, valueT *,
				      enum bfd_reloc_code_real,
				      char *);
static void encode_abs (void);
static void encode_absb (void);
static void encode_b (void);
static void encode_bit (void);
static void encode_bo (void);
static void encode_bol (void);
static void encode_brc (void);
static void encode_brn (void);
static void encode_brr (void);
static void encode_rc (void);
static void encode_rcpw (void);
static void encode_rcr (void);
static void encode_rcrr (void);
static void encode_rcrw (void);
static void encode_rlc (void);
static void encode_rr (void);
static void encode_rr1 (void);
static void encode_rr2 (void);
static void encode_rrpw (void);
static void encode_rrr (void);
static void encode_rrr1 (void);
static void encode_rrr2 (void);
static void encode_rrrr (void);
static void encode_rrrw (void);
static void encode_sys (void);
static void encode_sb (void);
static void encode_sbc (void);
static void encode_sbr (void);
static void encode_sbrn (void);
static void encode_sc (void);
static void encode_slr (void);
static void encode_slro (void);
static void encode_sr (void);
static void encode_src (void);
static void encode_sro (void);
static void encode_srr (void);
static void encode_srrs (void);
static void encode_ssr (void);
static void encode_ssro (void);

/* Array of pointers to encoding functions.  */

static void (*encode[]) (void) =
{
  encode_abs, encode_absb, encode_b, encode_bit, encode_bo, encode_bol,
  encode_brc, encode_brn, encode_brr, encode_rc, encode_rcpw, encode_rcr,
  encode_rcrr, encode_rcrw, encode_rlc, encode_rr, encode_rr1, encode_rr2,
  encode_rrpw, encode_rrr, encode_rrr1, encode_rrr2, encode_rrrr,
  encode_rrrw, encode_sys,
  encode_sb, encode_sbc, encode_sbr, encode_sbrn, encode_sc, encode_slr,
  encode_slro, encode_sr, encode_src, encode_sro, encode_srr,
  encode_srrs, encode_ssr, encode_ssro
};

/* To select between several possible relocation types, memory operands
   may be prefixed with "hi:", "lo:", "sm:", "up:", "got:", "gothi:",
   "gotlo:", "gotup:", "gotoff:", "gotoffhi:", "gotofflo:", "gotoffup:",
   "gotpc:", "gotpchi:", "gotpclo:", "gotpcup:", "plt:", or "bpos:".  */

typedef enum _prefix_t
{
  PREFIX_NONE = 0,
  PREFIX_HI = 1,
  PREFIX_LO = 2,
  PREFIX_SM = 3,
  PREFIX_UP = 4,
  PREFIX_GOT = 5,
  PREFIX_GOTHI = 6,
  PREFIX_GOTLO = 7,
  PREFIX_GOTUP = 8,
  PREFIX_GOTOFF = 9,
  PREFIX_GOTOFFHI = 10,
  PREFIX_GOTOFFLO = 11,
  PREFIX_GOTOFFUP = 12,
  PREFIX_GOTPC = 13,
  PREFIX_GOTPCHI = 14,
  PREFIX_GOTPCLO = 15,
  PREFIX_GOTPCUP = 16,
  PREFIX_PLT = 17,
  PREFIX_BITPOS = 18
} prefix_t;

/* The internal representation of a TriCore insn.  */

typedef struct _tricore_insn_t
{
  const char *error;		/* Error message, or NULL on M'kay. :-)  */
  opcode_t *code;		/* Pointer to matching opcode (hashed).  */
  unsigned long opcode;		/* This insn's (preliminary) opcode.  */
  unsigned long opc_address;	/* The address at which `opcode' is stored.  */
  int nops;			/* Number of successfully parsed operands.  */
  prefix_t prefix[MAX_OPS];	/* Operand prefixes (as defined above).  */
  expressionS opexp[MAX_OPS];	/* Operand expressions.  */
  unsigned char ops[MAX_OPS];	/* Operand types (as defined in tricore.h).  */
  int regs[MAX_OPS];		/* Register number, if used as/by operand.  */
  int matches_v[MAX_OPS];	/* 1 for operands matching type 'v'.  */
  int matches_6[MAX_OPS];	/* 1 for operands matching type '6'.  */
  int matches_k[MAX_OPS];	/* 1 for operands matching type 'k'.  */
  int pcrel[MAX_OPS];		/* 1 for PC-relative operands.  */
  int is_odd[MAX_OPS];		/* 1 for non-even constant operands.  */
  int is_call;			/* 1 if this is a "call" or "fcall" insn.  */
  int is_loop;			/* 1 if this is a "loop" insn.  */
  int needs_prefix;		/* 1 if at least one operand is prefixed.  */
  int bitpos[MAX_OPS];		/* 1 if operand is prefixed with "bpos:".  */
} tricore_insn_t;

static tricore_insn_t the_insn;

/* To select between several possible PCP relocation types, memory operands
   may be prefixed with "dptr:", "doff:", or "cptr:".  */

typedef enum _pcp_pfx_t
{
  PCP_PREFIX_NONE = 0,
  PCP_PREFIX_DPTR = 1,
  PCP_PREFIX_DOFF = 2,
  PCP_PREFIX_CPTR = 3
} pcp_pfx_t;

/* The internal representation of a PCP insn.  */

typedef struct _pcp_insn_t
{
  const char *error;		/* Error message, or NULL on M'kay. :-)  */
  pcp_opcode_t *code;		/* Pointer to matching opcode (hashed).  */
  unsigned long opcode;		/* This insn's (preliminary) opcode.  */
  int nops;			/* Number of successfully parsed operands.  */
  pcp_pfx_t prefix[MAX_PCPOPS]; /* Operand prefixes (as defined above).  */
  expressionS opexp[MAX_PCPOPS];/* Operand expressions.  */
  unsigned char ops[MAX_PCPOPS];/* Operand types (as defined in tricore.h).  */
} pcp_insn_t;

//static pcp_insn_t pcp_insn;

/* Names of PCP condition codes.  For debugging only.  */

// static const char *pcp_ccodes[] =
// {
//   "uc", "z", "nz", "v", "c/ult", "ugt", "slt", "sgt",    /* CONDCA  */
//   "n", "nn", "nv", "nc/uge", "sge", "sle", "cnz", "cnn"  /* CONDCB  */
// };

/* Which instruction set architecture are we assembling for?  */

static tricore_isa current_isa = TRICORE_V1_2;

/* This is used to convert 16-bit to 32-bit insns.  */

typedef struct _insn16_t
{
  const char *name;		/* Name and operand types of 16-bit insn.  */
  int neg_val1;			/* 1 if "VAL (1)" needs being negated.  */
  int nr_inserts;		/* Number of reg/const insertions needed.  */
  int reg_or_const1;		/* 1: insert register, 0: insert const.  */
  int regno_or_val1;		/* Operand's reg number or value of const.  */
  unsigned char op_type1;	/* Operand type of inserted reg/const.  */
  int op_pos1;			/* Where to insert the reg/const.  */
  int reg_or_const2;		/* Same as above, but for 2nd insertion.  */
  int regno_or_val2;
  unsigned char op_type2;
  int op_pos2;
  const char *new_name;		/* Name of equivalent 32-bit insn.  */
  const char *new_args;		/* Operand types of equivalent 32-bit insn.  */
  tricore_isa isa;		/* Applicable instruction set architecture.  */
  opcode_t *new_code;		/* Pointer to equivalent 32-bit insn.  */
} insn16_t;

static insn16_t insn16_table[] =
{
  /* add %dn,%dx -> add %dn,%dn,%dx  */
  {"add-dd",      0, 1,  1, 0, 'd', 0,  0, 0, 'X', 0,  "add",      "ddd",
		  TRICORE_GENERIC, NULL},
  /* add %dn,sconst4 -> add %dn,%dn,sconst9  */
  {"add-d4",      0, 1,  1, 0, 'd', 0,  0, 0, 'X', 0,  "add",      "dd9",
		  TRICORE_GENERIC, NULL},
  /* add.a %an,%am -> add.a %an,%an,%am  */
  {"add.a-aa",    0, 1,  1, 0, 'a', 0,  0, 0, 'X', 0,  "add.a",    "aaa",
		  TRICORE_V1_2_UP, NULL},
  /* add.a %an,sconst4 -> lea %an,[%an]sconst10  */
  {"add.a-a4",    0, 1,  1, 0, '@', 0,  0, 0, 'X', 0,  "lea",      "a@0",
		  TRICORE_V1_2_UP, NULL},
  /* adds %dn,%dx -> adds %dn,%dn,%dx  */
  {"adds-dd",     0, 1,  1, 0, 'd', 0,  0, 0, 'X', 0,  "adds",     "ddd",
		  TRICORE_GENERIC, NULL},
  /* addsc.a %an,%dn,zconst2 -> addsc.a %an,%an,%dn,zconst2  */
  {"addsc.a-ad2", 0, 1,  1, 0, 'a', 0,  0, 0, 'X', 0,  "addsc.a",  "aad2",
  		  TRICORE_V1_1, NULL},
  /* addsc.a %an,%am,%d15,zconst2 -> addsc.a %an,%am,%d15,zconst2  */
  {"addsc.a-aai2",0, 0,  0, 0, 'X', 0,  0, 0, 'X', 0,  "addsc.a",  "aad2",
  		  TRICORE_V1_2_UP, NULL},
  /* and %dn,%dx -> and %dn,%dn,%dx  */
  {"and-dd",      0, 1,  1, 0, 'd', 0,  0, 0, 'X', 0,  "and",      "ddd",
		  TRICORE_GENERIC, NULL},
  /* and %d15,zconst8 -> and %d15,%d15,zconst9  */
  {"and-i8",      0, 1,  1, 0, 'i', 0,  0, 0, 'X', 0,  "and",      "ddn",
		  TRICORE_GENERIC, NULL},
  /* cadd %dn,%d15,%dx -> cadd %dn,%d15,%dn,%dx  */
  {"cadd-did",    0, 1,  1, 0, 'd', 2,  0, 0, 'X', 0,  "cadd",     "dddd",
		  TRICORE_V1_1, NULL},
  /* cadd %dn,%d15,sconst4 -> cadd %dn,%d15,%dn,sconst9  */
  {"cadd-di4",    0, 1,  1, 0, 'd', 2,  0, 0, 'X', 0,  "cadd",     "ddd9",
		  TRICORE_GENERIC, NULL},
  /* caddn %dn,%d15,%dx -> caddn %dn,%d15,%dn,%dx  */
  {"caddn-did",   0, 1,  1, 0, 'd', 2,  0, 0, 'X', 0,  "caddn",    "dddd",
		  TRICORE_V1_1, NULL},
  /* caddn %dn,%d15,sconst4 -> caddn %dn,%d15,%dn,sconst9  */
  {"caddn-di4",   0, 1,  1, 0, 'd', 2,  0, 0, 'X', 0,  "caddn",    "ddd9",
		  TRICORE_GENERIC, NULL},
  /* call sdisp8 -> call sdisp24  */
  {"call-R",      0, 0,  0, 0, 'X', 0,  0, 0, 'X', 0,  "call",     "O",
		  TRICORE_V1_2_UP, NULL},
  /* cmov %dn,%d15,%dx -> sel %dn,%d15,%dx,%dn  */
  {"cmov-did",    0, 1,  1, 0, 'd', 3,  0, 0, 'X', 0,  "sel",      "dddd",
		  TRICORE_GENERIC, NULL},
  /* cmov %dn,%d15,sconst4 -> seln %dn,%d15,%dn,sconst9  */
  {"cmov-di4",    0, 1,  1, 0, 'd', 2,  0, 0, 'X', 0,  "seln",     "ddd9",
		  TRICORE_GENERIC, NULL},
  /* cmovn %dn,%d15,%dx -> seln %dn,%d15,%dx,%dn  */
  {"cmovn-did",   0, 1,  1, 0, 'd', 3,  0, 0, 'X', 0,  "seln",     "dddd",
		  TRICORE_GENERIC, NULL},
  /* cmovn %dn,%d15,sconst4 -> sel %dn,%d15,%dn,sconst9  */
  {"cmovn-di4",   0, 1,  1, 0, 'd', 2,  0, 0, 'X', 0,  "sel",      "ddd9",
		  TRICORE_GENERIC, NULL},
  /* dvadj %en,%dx -> dvadj %en,%en,%dx  */
  {"dvadj-Dd",    0, 1,  1, 0, 'D', 1,  0, 0, 'X', 0,  "dvadj",    "DDd",
		  TRICORE_V1_1, NULL},
  /* dvstep %en,%dx -> dvstep %en,%en,%dx  */
  {"dvstep-Dd",   0, 1,  1, 0, 'D', 1,  0, 0, 'X', 0,  "dvstep",   "DDd",
		  TRICORE_V1_1, NULL},
  /* dvstep.u %en,%dx -> dvstep.u %en,%en,%dx  */
  {"dvstep.u-Dd", 0, 1,  1, 0, 'D', 1,  0, 0, 'X', 0,  "dvstep.u", "DDd",
		  TRICORE_V1_1, NULL},
  /* j sdisp8 -> j sdisp24  */
  {"j-R",         0, 0,  0, 0, 'X', 0,  0, 0, 'X', 0,  "j",        "O",
		  TRICORE_GENERIC, NULL},
  /* jeq %d15,%dn,zdisp4 -> jeq %d15,%dn,sdisp15  */
  {"jeq-idm",     0, 0,  0, 0, 'X', 0,  0, 0, 'X', 0,  "jeq",      "ddo",
		  TRICORE_GENERIC, NULL},
  /* jeq %d15,%dn,zdisp5 -> jeq %d15,%dn,sdisp15  */
  {"jeq-idx",     0, 0,  0, 0, 'X', 0,  0, 0, 'X', 0,  "jeq",      "ddo",
  		  TRICORE_V1_3_UP, NULL},
  /* jeq %d15,sconst4,zdisp4 -> jeq %d15,sconst4,sdisp15  */
  {"jeq-i4m",     0, 0,  0, 0, 'X', 0,  0, 0, 'X', 0,  "jeq",      "d4o",
		  TRICORE_GENERIC, NULL},
  /* jeq %d15,sconst4,zdisp5 -> jeq %d15,sconst4,sdisp15  */
  {"jeq-i4x",     0, 0,  0, 0, 'X', 0,  0, 0, 'X', 0,  "jeq",      "d4o",
		  TRICORE_V1_3_UP, NULL},
  /* jgez %dn,zdisp4 -> jge %dn,0,sdisp15  */
  {"jgez-dm",     0, 1,  0, 0, '1', 1,  0, 0, 'X', 0,  "jge",      "d4o",
		  TRICORE_GENERIC, NULL},
  /* jgtz %dn,zdisp4 -> jge %dn,1,sdisp15  */
  {"jgtz-dm",     0, 1,  0, 1, '1', 1,  0, 0, 'X', 0,  "jge",      "d4o",
		  TRICORE_GENERIC, NULL},
  /* jlez %dn,zdisp4 -> jlt %dn,1,sdisp15  */
  {"jlez-dm",     0, 1,  0, 1, '1', 1,  0, 0, 'X', 0,  "jlt",      "d4o",
		  TRICORE_GENERIC, NULL},
  /* jltz %dn,zdisp4 -> jlt %dn,0,sdisp15  */
  {"jltz-dm",     0, 1,  0, 0, '1', 1,  0, 0, 'X', 0,  "jlt",      "d4o",
		  TRICORE_GENERIC, NULL},
  /* jne %d15,%dn,zdisp4 -> jne %d15,%dn,sdisp15  */
  {"jne-idm",     0, 0,  0, 0, 'X', 0,  0, 0, 'X', 0,  "jne",      "ddo",
		  TRICORE_GENERIC, NULL},
  /* jne %d15,%dn,zdisp5 -> jne %d15,%dn,sdisp15  */
  {"jne-idx",     0, 0,  0, 0, 'X', 0,  0, 0, 'X', 0,  "jne",      "ddo",
  		  TRICORE_V1_3_UP, NULL},
  /* jne %d15,sconst4,zdisp4 -> jne %d15,sconst4,sdisp15  */
  {"jne-i4m",     0, 0,  0, 0, 'X', 0,  0, 0, 'X', 0,  "jne",      "d4o",
		  TRICORE_GENERIC, NULL},
  /* jne %d15,sconst4,zdisp5 -> jne %d15,sconst4,sdisp15  */
  {"jne-i4x",     0, 0,  0, 0, 'X', 0,  0, 0, 'X', 0,  "jne",      "d4o",
  		  TRICORE_V1_3_UP, NULL},
  /* jnz %dn,zdisp4 -> jne %dn,0,sdisp15  */
  {"jnz-dm",      0, 1,  0, 0, '1', 1,  0, 0, 'X', 0,  "jne",      "d4o",
		  TRICORE_GENERIC, NULL},
  /* jnz %d15,sdisp8 -> jne %d15,0,sdisp15  */
  {"jnz-iR",      0, 1,  0, 0, '1', 1,  0, 0, 'X', 0,  "jne",      "d4o",
		  TRICORE_GENERIC, NULL},
  /* jnz.a %an,zdisp4 -> jnz.a %an,sdisp15  */
  {"jnz.a-am",    0, 0,  0, 0, 'X', 0,  0, 0, 'X', 0,  "jnz.a",    "ao",
		  TRICORE_GENERIC, NULL},
  /* jnz.t %d15,zconst5,zdisp4 -> jnz.t %d15,zconst5,sdisp15  */
  {"jnz.t-i5m",   0, 0,  0, 0, 'X', 0,  0, 0, 'X', 0,  "jnz.t",    "d5o",
		  TRICORE_V1_1, NULL},
  /* jnz.t %d15,zconst4,zdisp4 -> jnz.t %d15,zconst5,sdisp15  */
  {"jnz.t-ifm",   0, 0,  0, 0, 'X', 0,  0, 0, 'X', 0,  "jnz.t",    "d5o",
		  TRICORE_V1_2_UP, NULL},
  /* jz %dn,zdisp4 -> jeq %dn,0,sdisp15  */
  {"jz-dm",       0, 1,  0, 0, '1', 1,  0, 0, 'X', 0,  "jeq",      "d4o",
		  TRICORE_GENERIC, NULL},
  /* jz %d15,sdisp8 -> jeq %d15,0,sdisp15  */
  {"jz-iR",       0, 1,  0, 0, '1', 1,  0, 0, 'X', 0,  "jeq",      "d4o",
		  TRICORE_GENERIC, NULL},
  /* jz.a %an,zdisp4 -> jz.a %an,sdisp15  */
  {"jz.a-am",     0, 0,  0, 0, 'X', 0,  0, 0, 'X', 0,  "jz.a",     "ao",
		  TRICORE_GENERIC, NULL},
  /* jz.t %d15,zconst5,zdisp4 -> jz.t %d15,zconst5,sdisp15  */
  {"jz.t-i5m",    0, 0,  0, 0, 'X', 0,  0, 0, 'X', 0,  "jz.t",     "d5o",
		  TRICORE_V1_1, NULL},
  /* jz.t %d15,zconst4,zdisp4 -> jz.t %d15,zconst5,sdisp15  */
  {"jz.t-ifm",    0, 0,  0, 0, 'X', 0,  0, 0, 'X', 0,  "jz.t",     "d5o",
		  TRICORE_V1_2_UP, NULL},
  /* ld.a %an,[%am+] -> ld.a %an,[%am+]4  */
  {"ld.a-a>",     0, 1,  0, 4, '6', 2,  0, 0, 'X', 0,  "ld.a",     "a>0",
		  TRICORE_GENERIC, NULL},
  /* ld.a %an,[%am] -> ld.a %an,[%am]0  */
  {"ld.a-a@",     0, 1,  0, 0, '1', 2,  0, 0, 'X', 0,  "ld.a",     "a@w",
		  TRICORE_GENERIC, NULL},
  /* ld.b %dn,[%an+] -> ld.b %dn,[%an+]1  */
  {"ld.b-d>",     0, 1,  0, 1, '1', 2,  0, 0, 'X', 0,  "ld.b",     "d>0",
		  TRICORE_V1_1, NULL},
  /* ld.b %dn,[%an] -> ld.b %dn,[%an]0  */
  {"ld.b-d@",     0, 1,  0, 0, '1', 2,  0, 0, 'X', 0,  "ld.b",     "d@0",
		  TRICORE_V1_1, NULL},
  /* ld.bu %dn,[%an+] -> ld.bu %dn,[%an+]1  */
  {"ld.bu-d>",    0, 1,  0, 1, '1', 2,  0, 0, 'X', 0,  "ld.bu",    "d>0",
		  TRICORE_GENERIC, NULL},
  /* ld.bu %dn,[%an] -> ld.bu %dn,[%an]0  */
  {"ld.bu-d@",    0, 1,  0, 0, '1', 2,  0, 0, 'X', 0,  "ld.bu",    "d@0",
		  TRICORE_GENERIC, NULL},
  /* ld.h %dn,[%an+] -> ld.h %dn,[%an+]2  */
  {"ld.h-d>",     0, 1,  0, 2, '6', 2,  0, 0, 'X', 0,  "ld.h",     "d>0",
		  TRICORE_GENERIC, NULL},
  /* ld.h %dn,[%an] -> ld.h %dn,[%an]0  */
  {"ld.h-d@",     0, 1,  0, 0, '1', 2,  0, 0, 'X', 0,  "ld.h",     "d@0",
		  TRICORE_GENERIC, NULL},
  /* ld.w %dn,[%an+] -> ld.w %dn,[%an+]4  */
  {"ld.w-d>",     0, 1,  0, 4, '6', 2,  0, 0, 'X', 0,  "ld.w",     "d>0",
		  TRICORE_GENERIC, NULL},
  /* ld.w %dn,[an] -> ld.w %dn,[%an]0  */
  {"ld.w-d@",     0, 1,  0, 0, '1', 2,  0, 0, 'X', 0,  "ld.w",     "d@w",
		  TRICORE_GENERIC, NULL},
  /* loop %an,odisp4 -> loop %an,sdisp15  */
  {"loop-ar",     0, 0,  0, 0, 'X', 0,  0, 0, 'X', 0,  "loop",     "ao",
		  TRICORE_GENERIC, NULL},
  /* mov %en,sconst4 -> mov %en,sconst16  */
  {"mov-D4",      0, 0,  0, 0, 'X', 0,  0, 0, 'X', 0,  "mov",      "Dw",
  		  TRICORE_V1_3_UP, NULL},
  /* mov.a %an,zconst4 -> lea %an,abs10  */
  {"mov.a-af",    0, 0,  0, 0, 'X', 0,  0, 0, 'X', 0,  "lea",      "at",
		  TRICORE_V1_2_UP, NULL},
  /* movz.a %an -> lea %an,0  */
  {"movz.a-a",    0, 1,  0, 0, 't', 1,  0, 0, 'X', 0,  "lea",      "at",
		  TRICORE_V1_1, NULL},
  /* mul %dn,%dm -> mul %dn,%dn,%dm  */
  {"mul-dd",      0, 1,  1, 0, 'd', 1,  0, 0, 'X', 0,  "mul",      "ddd",
		  TRICORE_GENERIC, NULL},
  /* nor/not %dn -> nor %dn,%dn,0  */
  {"nor-d",       0, 2,  1, 0, 'd', 1,  0, 0, '1', 2,  "nor",      "ddn",
		  TRICORE_GENERIC, NULL},
  /* or %dn,%dm -> or %dn,%dn,%dx  */
  {"or-dd",       0, 1,  1, 0, 'd', 1,  0, 0, 'X', 0,  "or",       "ddd",
		  TRICORE_GENERIC, NULL},
  /* or %d15,zconst8 -> or %d15,%d15,zconst9  */
  {"or-i8",       0, 1,  1, 0, 'i', 1,  0, 0, 'X', 0,  "or",       "ddn",
		  TRICORE_GENERIC, NULL},
  /* rsub %dn -> rsub %dn,%dn,0  */
  {"rsub-d",      0, 2,  1, 0, 'd', 1,  0, 0, '1', 2,  "rsub",     "dd9",
		  TRICORE_GENERIC, NULL},
  /* sat.b %dn -> sat.b %dn,%dn  */
  {"sat.b-d",     0, 1,  1, 0, 'd', 1,  0, 0, 'X', 0,  "sat.b",    "dd",
		  TRICORE_GENERIC, NULL},
  /* sat.bu %dn -> sat.bu %dn,%dn  */
  {"sat.bu-d",    0, 1,  1, 0, 'd', 1,  0, 0, 'X', 0,  "sat.bu",   "dd",
		  TRICORE_GENERIC, NULL},
  /* sat.h %dn -> sat.h %dn,%dn  */
  {"sat.h-d",     0, 1,  1, 0, 'd', 1,  0, 0, 'X', 0,  "sat.h",    "dd",
		  TRICORE_GENERIC, NULL},
  /* sat.hu %dn -> sat.hu %dn,%dn  */
  {"sat.hu-d",    0, 1,  1, 0, 'd', 1,  0, 0, 'X', 0,  "sat.hu",   "dd",
		  TRICORE_GENERIC, NULL},
  /* sh %dn,sconst4 -> sh %dn,%dn,sconst9  */
  {"sh-d4",       0, 1,  1, 0, 'd', 1,  0, 0, 'X', 0,  "sh",       "dd9",
		  TRICORE_GENERIC, NULL},
  /* sha %dn,sconst4 -> sha %dn,%dn,sconst9  */
  {"sha-d4",      0, 1,  1, 0, 'd', 1,  0, 0, 'X', 0,  "sha",      "dd9",
		  TRICORE_GENERIC, NULL},
  /* st.a [%an+],%am -> st.a [%an+]4,%am  */
  {"st.a->a",     0, 1,  0, 4, '6', 1,  0, 0, 'X', 0,  "st.a",     ">0a",
		  TRICORE_GENERIC, NULL},
  /* st.a [%an],%am -> st.a [%an]0,%am  */
  {"st.a-@a",     0, 1,  0, 0, '1', 1,  0, 0, 'X', 0,  "st.a",     "@0a",
		  TRICORE_GENERIC, NULL},
  /* st.b [%an+],%dn -> st.b [%an+]1,%dn  */
  {"st.b->d",     0, 1,  0, 1, '1', 1,  0, 0, 'X', 0,  "st.b",     ">0d",
		  TRICORE_GENERIC, NULL},
  /* st.b [%an],%dn -> st.b [%an]0,%dn  */
  {"st.b-@d",     0, 1,  0, 0, '1', 1,  0, 0, 'X', 0,  "st.b",     "@0d",
		  TRICORE_GENERIC, NULL},
  /* st.h [%an+],%dn -> st.h [%an+]2,%dn  */
  {"st.h->d",     0, 1,  0, 2, '6', 1,  0, 0, 'X', 0,  "st.h",     ">0d",
		  TRICORE_GENERIC, NULL},
  /* st.h [%an],%dn -> st.h [%an]0,%dn  */
  {"st.h-@d",     0, 1,  0, 0, '1', 1,  0, 0, 'X', 0,  "st.h",     "@0d",
		  TRICORE_GENERIC, NULL},
  /* st.w [%an+],%dn -> st.w [%an+]4,%dn  */
  {"st.w->d",     0, 1,  0, 4, '6', 1,  0, 0, 'X', 0,  "st.w",     ">0d",
		  TRICORE_GENERIC, NULL},
  /* st.w [%an],%dn -> st.w [%an]0,%dn  */
  {"st.w-@d",     0, 1,  0, 0, '1', 1,  0, 0, 'X', 0,  "st.w",     "@wd",
		  TRICORE_GENERIC, NULL},
  /* sub %dn,%dm -> sub %dn,%dn,%dm  */
  {"sub-dd",      0, 1,  1, 0, 'd', 1,  0, 0, 'X', 0,  "sub",      "ddd",
		  TRICORE_GENERIC, NULL},
  /* sub.a %sp,zconst8 -> lea %sp,[%sp]-sconst16  */
  {"sub.a-P8",    1, 1,  1, 0, '@', 1,  0, 0, 'X', 0,  "lea",      "a@w",
		  TRICORE_GENERIC, NULL},
  /* subs %dn,%dm -> subs %dn,%dn,%dm  */
  {"subs-dd",     0, 1,  1, 0, 'd', 1,  0, 0, 'X', 0,  "subs",     "ddd",
		  TRICORE_GENERIC, NULL},
  /* xor %dn,%dm -> xor %dn,%dn,%dm  */
  {"xor-dd",      0, 1,  1, 0, 'd', 1,  0, 0, 'X', 0,  "xor",      "ddd",
		  TRICORE_V1_2_UP, NULL}
};

static const int num_insn16 = sizeof insn16_table / sizeof insn16_table[0];
static struct htab *hash_insn16 = NULL;

/* This is used to convert 32-bit to 16-bit insns (aka optimizing).  */

typedef struct _conv_t
{
  int test;			/* Number of test to apply to this insn.  */
  const char *new_name;		/* Name of equivalent 16-bit insn.  */
  const char *new_args;		/* Operand types of equivalent 16-bit insn.  */
  opcode_t *new_code;		/* Pointer to equivalent 16-bit insn.  */
} conv_t;

typedef struct _insn32_t
{
  const char *name;		/* Name and operand types of 32-bit insn.  */
  int nr_alt;			/* Number of alternatives for this insn.  */
  conv_t alt[2];		/* Description of the 16-bit alternatives.  */
  tricore_isa isa;		/* Applicable instruction set architecture.  */
} insn32_t;

static insn32_t insn32_table[] =
{
#define NO_ALTERNATIVE { 0, NULL,       NULL,  NULL }
  {"add-ddd",      1, {{ 1, "add",      "dd",  NULL },
		       NO_ALTERNATIVE		    },  TRICORE_GENERIC},
  {"add-dd9",      1, {{ 2, "add",      "d4",  NULL },
		       NO_ALTERNATIVE		    },  TRICORE_GENERIC},
  {"add.a-aaa",    1, {{ 1, "add.a",    "aa",  NULL },
		       NO_ALTERNATIVE		    },  TRICORE_V1_2_UP},
  {"adds-ddd",     1, {{ 1, "adds",     "dd",  NULL },
		       NO_ALTERNATIVE		    },  TRICORE_GENERIC},
  {"addsc.a-aad2", 1, {{ 1, "addsc.a",  "ad2", NULL },
		       NO_ALTERNATIVE		    },  TRICORE_V1_1},
  {"and-ddd",      1, {{ 1, "and",      "dd",  NULL },
		       NO_ALTERNATIVE		    },  TRICORE_GENERIC},
  {"and-ddn",      1, {{ 3, "and",      "i8",  NULL },
		       NO_ALTERNATIVE		    },  TRICORE_GENERIC},
  {"cadd-dddd",    1, {{ 4, "cadd",     "did", NULL },
		       NO_ALTERNATIVE		    },  TRICORE_V1_1},
  {"cadd-ddd9",    1, {{ 5, "cadd",     "di4", NULL },
		       NO_ALTERNATIVE		    },  TRICORE_GENERIC},
  {"caddn-dddd",   1, {{ 4, "caddn",    "did", NULL },
		       NO_ALTERNATIVE		    },  TRICORE_V1_1},
  {"caddn-ddd9",   1, {{ 5, "caddn",    "di4", NULL },
		       NO_ALTERNATIVE		    },  TRICORE_GENERIC},
  {"dvadj-DDd",    1, {{ 1, "dvadj",    "Dd",  NULL },
		       NO_ALTERNATIVE		    },  TRICORE_V1_1},
  {"dvstep-DDd",   1, {{ 1, "dvstep",   "Dd",  NULL },
		       NO_ALTERNATIVE		    },  TRICORE_V1_1},
  {"dvstep.u-DDd", 1, {{ 1, "dvstep.u", "Dd",  NULL },
		       NO_ALTERNATIVE		    },  TRICORE_V1_1},
  {"jeq-d4o",      2, {{ 6, "jz",       "iR",  NULL },
		       { 7, "jz",       "dm",  NULL }}, TRICORE_GENERIC},
  {"jge-d4o",      2, {{ 8, "jgtz",     "dm",  NULL },
		       { 7, "jgez",     "dm",  NULL }}, TRICORE_GENERIC},
  {"jlt-d4o",      2, {{ 8, "jlez",     "dm",  NULL },
		       { 7, "jltz",     "dm",  NULL }}, TRICORE_GENERIC},
  {"jne-d4o",      2, {{ 6, "jnz",      "iR",  NULL },
		       { 7, "jnz",      "dm",  NULL }}, TRICORE_GENERIC},
  {"ld.a-a>0",     1, {{ 9, "ld.a",     "a>",  NULL },
		       NO_ALTERNATIVE		    },  TRICORE_GENERIC},
  {"ld.a-a@w",     1, {{10, "ld.a",     "a@",  NULL },
		       NO_ALTERNATIVE		    },  TRICORE_GENERIC},
  {"ld.b-d>0",     1, {{11, "ld.b",     "d>",  NULL },
		       NO_ALTERNATIVE		    },  TRICORE_V1_1},
  {"ld.b-d@0",     1, {{10, "ld.b",     "d@",  NULL },
		       NO_ALTERNATIVE		    },  TRICORE_V1_1},
  {"ld.bu-d>0",    1, {{11, "ld.bu",    "d>",  NULL },
		       NO_ALTERNATIVE		    },  TRICORE_GENERIC},
  {"ld.bu-d@0",    1, {{10, "ld.bu",    "d@",  NULL },
		       NO_ALTERNATIVE		    },  TRICORE_GENERIC},
  {"ld.h-d>0",     1, {{12, "ld.h",     "d>",  NULL },
		       NO_ALTERNATIVE		    },  TRICORE_GENERIC},
  {"ld.h-d@0",     1, {{10, "ld.h",     "d@",  NULL },
		       NO_ALTERNATIVE		    },  TRICORE_GENERIC},
  {"ld.w-d>0",     1, {{ 9, "ld.w",     "d>",  NULL },
		       NO_ALTERNATIVE		    },  TRICORE_GENERIC},
  {"ld.w-d@w",     1, {{10, "ld.w",     "d@",  NULL },
		       NO_ALTERNATIVE		    },  TRICORE_GENERIC},
  {"lea-at",       1, {{13, "movz.a",   "a",   NULL },
		       NO_ALTERNATIVE		    },  TRICORE_V1_1},
  {"lea-at",       1, {{21, "mov.a",    "af",  NULL },
		       NO_ALTERNATIVE		    },  TRICORE_V1_2_UP},
  {"lea-a@w",      1, {{14, "sub.a",    "P8",  NULL },
		       NO_ALTERNATIVE		    },  TRICORE_V1_1},
  {"lea-a@w",      2, {{14, "sub.a",    "P8",  NULL },
		       {22, "add.a",    "a4",  NULL }}, TRICORE_V1_2_UP},
  {"mul-ddd",      1, {{ 1, "mul",      "dd",  NULL },
		       NO_ALTERNATIVE		    },  TRICORE_GENERIC},
  {"nor-ddn",      1, {{15, "nor",      "d",   NULL },
		       NO_ALTERNATIVE		    },  TRICORE_GENERIC},
  {"or-ddd",       1, {{ 1, "or",       "dd",  NULL },
		       NO_ALTERNATIVE		    },  TRICORE_GENERIC},
  {"or-ddn",       1, {{ 3, "or",       "i8",  NULL },
		       NO_ALTERNATIVE		    },  TRICORE_GENERIC},
  {"rsub-dd9",     1, {{15, "rsub",     "d",   NULL },
		       NO_ALTERNATIVE		    },  TRICORE_GENERIC},
  {"sat.b-dd",     1, {{ 1, "sat.b",    "d",   NULL },
		       NO_ALTERNATIVE		    },  TRICORE_GENERIC},
  {"sat.h-dd",     1, {{ 1, "sat.h",    "d",   NULL },
		       NO_ALTERNATIVE		    },  TRICORE_GENERIC},
  {"sat.bu-dd",    1, {{ 1, "sat.bu",   "d",   NULL },
		       NO_ALTERNATIVE		    },  TRICORE_GENERIC},
  {"sat.hu-dd",    1, {{ 1, "sat.hu",   "d",   NULL },
		       NO_ALTERNATIVE		    },  TRICORE_GENERIC},
  {"sel-dddd",     1, {{16, "cmov",     "did", NULL },
		       NO_ALTERNATIVE		    },  TRICORE_GENERIC},
  {"sel-ddd9",     1, {{ 5, "cmovn",    "di4", NULL },
		       NO_ALTERNATIVE		    },  TRICORE_GENERIC},
  {"seln-dddd",    1, {{16, "cmovn",    "did", NULL },
		       NO_ALTERNATIVE		    },  TRICORE_GENERIC},
  {"seln-ddd9",    1, {{ 5, "cmov",     "di4", NULL },
		       NO_ALTERNATIVE		    },  TRICORE_GENERIC},
  {"sh-dd9",       1, {{ 2, "sh",       "d4",  NULL },
		       NO_ALTERNATIVE		    },  TRICORE_GENERIC},
  {"sha-dd9",      1, {{ 2, "sha",      "d4",  NULL },
		       NO_ALTERNATIVE		    },  TRICORE_GENERIC},
  {"st.a->0a",     1, {{17, "st.a",     ">a",  NULL },
		       NO_ALTERNATIVE		    },  TRICORE_GENERIC},
  {"st.a-@0a",     1, {{18, "st.a",     "@a",  NULL },
		       NO_ALTERNATIVE		    },  TRICORE_GENERIC},
  {"st.b->0d",     1, {{19, "st.b",     ">d",  NULL },
		       NO_ALTERNATIVE		    },  TRICORE_GENERIC},
  {"st.b-@0d",     1, {{18, "st.b",     "@d",  NULL },
		       NO_ALTERNATIVE		    },  TRICORE_GENERIC},
  {"st.h->0d",     1, {{20, "st.h",     ">d",  NULL },
		       NO_ALTERNATIVE		    },  TRICORE_GENERIC},
  {"st.h-@0d",     1, {{18, "st.h",     "@d",  NULL },
		       NO_ALTERNATIVE		    },  TRICORE_GENERIC},
  {"st.w->0d",     1, {{17, "st.w",     ">d",  NULL },
		       NO_ALTERNATIVE		    },  TRICORE_GENERIC},
  {"st.w-@wd",     1, {{18, "st.w",     "@d",  NULL },
		       NO_ALTERNATIVE		    },  TRICORE_GENERIC},
  {"sub-ddd",      1, {{ 1, "sub",      "dd",  NULL },
		       NO_ALTERNATIVE		    },  TRICORE_GENERIC},
  {"subs-ddd",     1, {{ 1, "subs",     "dd",  NULL },
		       NO_ALTERNATIVE		    },  TRICORE_GENERIC},
  {"xor-ddd",      1, {{ 1, "xor",      "dd",  NULL },
		       NO_ALTERNATIVE		    },  TRICORE_V1_2_UP}
#undef NO_ALTERNATIVE
};

static const int num_insn32 = sizeof insn32_table / sizeof insn32_table[0];
static struct htab *hash_insn32 = NULL;

/* This table is used for relaxing.  Each entry consists of 4 numbers:

   - the highest displacement reachable from the end of the base insn,
   - the lowest displacement reachable from the end of the base insn,
   - the number of bytes this displacement adds to the base insn, and
   - the entry number to use if the target address is out of reach for
     this insn (0 indicates that this is the last entry for this mode).

   If a 16-bit PC-relative call or jump instruction can't reach its
   target due to its limited range, we're converting it to its 32-bit
   equivalent, which can reach targets within a +/-32KB range.  If that's
   still not sufficient, we're reversing the condition code (if any) and
   modify the instruction to branch over the 32-bit unconditional jump
   instruction we're inserting right after it, which extends the range
   to the maximum possible on the TriCore (+/-16MB).  However, that's
   only the maximum possible range for calls and jumps to _local_ labels
   (which thus restricts the size of an object's text segment), as the
   linker (i.e., tricore-ld) is also able to relax "j", "jl", "fcall" and
   "call" instructions targeting global labels, enabling them to reach
   any valid address within TriCore's entire 4GB address space.

   We're also relaxing PCP's "jc" and "jl" instructions; if necessary,
   they'll be turned into "jc.a" instructions, which can reach any valid
   address within PCP's 128KB code address space ("PCODE").  */

const relax_typeS md_relax_table[] =
{
  /* SB -> BRC  */
#define RELAX_SB     (relax_substateT) 0
  {  252,   -258, 0, RELAX_SB + 1},
  {32764, -32770, 2, RELAX_SB + 2},
  {    1,      1, 4, 0},

  /* SB -> B  */
#define RELAX_SB2    (relax_substateT) 3
  {  252,   -258, 0, RELAX_SB2 + 1},
  {    1,      1, 2, 0},

  /* SBC -> BRC  */
#define RELAX_SBC    (relax_substateT) 5
  {   28,     -2, 0, RELAX_SBC + 1},
  {32764, -32770, 2, RELAX_SBC + 2},
  {    1,      1, 4, 0},

  /* SBC2 -> BRC  */
#define RELAX_SBC2   (relax_substateT) 8
  {   60,     -2, 0, RELAX_SBC2 + 1},
  {32764, -32770, 2, RELAX_SBC2 + 2},
  {    1,      1, 4, 0},

  /* SBR -> BRR/BRC  */
#define RELAX_SBR    (relax_substateT) 11
  {   28,     -2, 0, RELAX_SBR + 1},
  {32764, -32770, 2, RELAX_SBR + 2},
  {    1,      1, 4, 0},

  /* SBR2 -> BRR  */
#define RELAX_SBR2   (relax_substateT) 14
  {   60,     -2, 0, RELAX_SBR2 + 1},
  {32764, -32770, 2, RELAX_SBR2 + 2},
  {    1,      1, 4, 0},

  /* SBRN -> BRN  */
#define RELAX_SBRN   (relax_substateT) 17
  {   28,     -2, 0, RELAX_SBRN + 1},
  {32764, -32770, 2, RELAX_SBRN + 2},
  {    1,      1, 4, 0},

  /* SBR special case: loop  */
#define RELAX_LOOP   (relax_substateT) 20
  {   -4,    -34, 0, RELAX_LOOP + 1},
  {32764, -32770, 2, RELAX_LOOP + 2},
  {    1,      1, 8, 0},

  /* BRR/BRC -> additional j instruction  */
#define RELAX_BRX    (relax_substateT) 23
  {32762, -32772, 0, RELAX_BRX + 1},
  {    1,      1, 4, 0},

  /* BRN -> additional j instruction  */
#define RELAX_BRN    (relax_substateT) 25
  {32762, -32772, 0, RELAX_BRN + 1},
  {    1,      1, 4, 0},

  /* BRR/BRC special cases: jned/jnei, additional 16-bit j instruction  */
#define RELAX_JNEX   (relax_substateT) 27
  {32762, -32772, 0, RELAX_JNEX + 1},
  {    1,      1, 6, 0},

  /* BRN/BRC special cases: jned/jnei, additional 32-bit j instruction  */
#define RELAX_JNEX2  (relax_substateT) 29
  {32762, -32772, 0, RELAX_JNEX2 + 1},
  {    1,      1, 8, 0},

  /* BRR special case: loop, additional 16-bit j instruction  */
#define RELAX_BLOOP  (relax_substateT) 31
  {32762, -32772, 0, RELAX_BLOOP + 1},
  {    1,      1, 6, 0},
 
  /* BRR special case: loop, additional 32-bit j instruction  */
#define RELAX_BLOOP2 (relax_substateT) 33
  {32762, -32772, 0, RELAX_BLOOP2 + 1},
  {    1,      1, 8, 0},
 
  /* BRR special case: loopu, replaced by a j instruction  */
#define RELAX_LOOPU  (relax_substateT) 35
  {32762, -32772, 0, RELAX_LOOPU + 1},
  {    1,      1, 0, 0},

  /* PCP: jc -> jc.a  */
#define RELAX_JC     (relax_substateT) 37
  {   62,    -64, 0, RELAX_JC + 1},
  {    1,      1, 2, 0},

  /* PCP: jl -> jc.a  */
#define RELAX_JL     (relax_substateT) 39
  { 1022,  -1024, 0, RELAX_JL + 1},
  {    1,      1, 2, 0}
};

/* Relaxing of "loop" and "loopu" instructions needs special treatment:
   the 16-bit "loop" instruction can only reach targets that are between
   -2 and -32 bytes away, so we need to switch to the 32-bit variant if
   the displacement is zero.  Second, "loopu" needs being replaced by a
   "j" instruction if the target is more than +/- 32KB away, but since both
   are 32-bit insns, the frag size won't grow, which also means that
   write.c:relax_frag won't switch to the next state.  Both special cases
   are handled by the macro tc-tricore.h:md_prepare_relax_scan, but we need
   to export the initial relaxation states for these instructions, as the
   macro will be evaluated in the context of write.c:relax_frag, and thus
   can't access local variables defined in this module.  */

relax_substateT tricore_relax_loop_state = RELAX_LOOP;
relax_substateT tricore_relax_loopu_state = RELAX_LOOPU;

/* These variables hold the opcode of relaxable 16-bit instructions.  */

static unsigned char tricore_insn_j_R = 0x3c;
static unsigned char tricore_insn_call_R = 0x5c;
static unsigned char tricore_insn_jnz_iR = 0xee;
static unsigned char tricore_insn_jz_iR = 0x6e;
static unsigned char tricore_insn_loop_ar = 0xfc;
static unsigned char tricore_insn_jeq_idm = 0x3e;
static unsigned char tricore_insn_jeq_idx = 0xbe;
static unsigned char tricore_insn_jne_idm = 0x7e;
static unsigned char tricore_insn_jne_idx = 0xfe;
static unsigned char tricore_insn_jgez_dm = 0xce;
static unsigned char tricore_insn_jgtz_dm = 0x4e;
static unsigned char tricore_insn_jlez_dm = 0x8e;
static unsigned char tricore_insn_jltz_dm = 0x0e;
static unsigned char tricore_insn_jnz_dm = 0xf6;
static unsigned char tricore_insn_jnza_am = 0x7c;
static unsigned char tricore_insn_jz_dm = 0x76;
static unsigned char tricore_insn_jza_am = 0xbc;
static unsigned char tricore_insn_jeq_i4m = 0x1e;
static unsigned char tricore_insn_jne_i4m = 0x5e;
static unsigned char tricore_insn_jeq_i4x = 0x9e;
static unsigned char tricore_insn_jne_i4x = 0xde;
static unsigned char tricore_insn_jnzt_i5m = 0x00;
static unsigned char tricore_insn_jnzt_ifm = 0xae;
static unsigned char tricore_insn_jzt_i5m = 0x00;
static unsigned char tricore_insn_jzt_ifm = 0x2e;

/* These variables hold the opcode of relaxable 32-bit instructions.  */

static unsigned char tricore_insn_j_O = 0x1d;
static unsigned char tricore_insn_jge_ddo = 0x7f;
static unsigned char tricore_insn_jgeu_ddo = 0x7f;
static unsigned char tricore_insn_jge_d4o = 0xff;
static unsigned char tricore_insn_jgeu_dfo = 0xff;
static unsigned char tricore_insn_jlt_ddo = 0x3f;
static unsigned char tricore_insn_jltu_ddo = 0x3f;
static unsigned char tricore_insn_jlt_d4o = 0xbf;
static unsigned char tricore_insn_jltu_dfo = 0xbf;
static unsigned char tricore_insn_jeq_ddo = 0x5f;
static unsigned char tricore_insn_jne_ddo = 0x5f;
static unsigned char tricore_insn_jeq_d4o = 0xdf;
static unsigned char tricore_insn_jne_d4o = 0xdf;
static unsigned char tricore_insn_jeqa_aao = 0x7d;
static unsigned char tricore_insn_jnea_aao = 0x7d;
static unsigned char tricore_insn_jnei_ddo = 0x1f;
static unsigned char tricore_insn_jned_ddo = 0x1f;
static unsigned char tricore_insn_jnei_d4o = 0x9f;
static unsigned char tricore_insn_jned_d4o = 0x9f;
static unsigned char tricore_insn_jza_ao = 0xbd;
static unsigned char tricore_insn_jnza_ao = 0xbd;
static unsigned char tricore_insn_jzt_d5o = 0x6f;
static unsigned char tricore_insn_jnzt_d5o = 0x6f;
static unsigned char tricore_insn_loop_ao = 0xfd;

/* Temporary line buffer (malloc()ed), and its current length.  */

static char *insnline;
static size_t insnline_len;

/* md_begin initializes the TriCore-specific part of gas.  It is
   called after md_parse_option, but prior to any other function
   of this module.  */

void
md_begin()
{
  register int opind, hopind, i;
  register void **retval;
  register opcode_t *pop, *hpop;
  register pcp_opcode_t *ppop, *hppop;
  register sfr_t *psfr;
  register insn16_t *pinsn16;
  register insn32_t *pinsn32;
  register conv_t *palt;
  unsigned long mach;

  if (show_internals)
    printf ("*** md_begin()\n");

  /* Initialize ISA-specific variables.  */

  switch (current_isa & TRICORE_ISA_MASK)
    {
    case TRICORE_V1_1:
      mach = bfd_mach_tricore_v1_1;
      tricore_insn_j_R = 0x5c;
      tricore_insn_call_R = 0x00;
      tricore_insn_jnz_iR = 0xae;
      tricore_insn_jz_iR = 0x2e;
      tricore_insn_loop_ar = 0xfc;
      tricore_insn_jeq_idm = 0x1e;
      tricore_insn_jne_idm = 0x9e;
      tricore_insn_jgez_dm = 0xfe;
      tricore_insn_jgtz_dm = 0x7e;
      tricore_insn_jlez_dm = 0xbe;
      tricore_insn_jltz_dm = 0x3e;
      tricore_insn_jnz_dm = 0xde;
      tricore_insn_jnza_am = 0x7c;
      tricore_insn_jz_dm = 0x5e;
      tricore_insn_jza_am = 0xbc;
      tricore_insn_jeq_i4m = 0x6e;
      tricore_insn_jne_i4m = 0xee;
      tricore_insn_jnzt_i5m = 0x4e;
      tricore_insn_jnzt_ifm = 0x00;
      tricore_insn_jzt_i5m = 0x0e;
      tricore_insn_jzt_ifm = 0x00;
      break;

    case TRICORE_V1_2:  
      mach = bfd_mach_tricore_v1_2;
      break;

    case TRICORE_V1_3:
      mach = bfd_mach_tricore_v1_3;
      tricore_insn_jeq_idx = 0xbe;
      tricore_insn_jne_idx = 0xfe;
      tricore_insn_jeq_i4x = 0x9e;
      tricore_insn_jne_i4x = 0xde;
      break;

    default:
      as_fatal (_("Illegal architecture %d"),
      		(current_isa & TRICORE_ISA_MASK));
    }
  tricore_init_arch_vars (mach);

  /* Create the various hash tables.  */

  hash_ops = htab_create_alloc (NUMOPCODES, hash_string_tuple, eq_string_tuple, NULL, xcalloc, xfree);
  if (hash_ops == NULL)
	as_fatal (_("Cannot create hash tables"));

  hash_sfr = htab_create_alloc (NUMSFRS, hash_string_tuple, eq_string_tuple, NULL, xcalloc, xfree);
  if (hash_sfr == NULL)
	as_fatal (_("Cannot create hash tables"));

  hash_pcp = htab_create_alloc (NUMPCPOPCODES, hash_string_tuple, eq_string_tuple, NULL, xcalloc, xfree);
  if (hash_pcp == NULL)
	as_fatal (_("Cannot create hash tables"));

  hash_insn16 = htab_create_alloc (num_insn16, hash_string_tuple, eq_string_tuple, NULL, xcalloc, xfree);
  if (hash_insn16 == NULL)
	as_fatal (_("Cannot create hash tables"));

  hash_insn32 = htab_create_alloc (num_insn32, hash_string_tuple, eq_string_tuple, NULL, xcalloc, xfree);
  if (hash_insn32 == NULL)
	as_fatal (_("Cannot create hash tables"));

  /* Populate the opcode hash table.  */

  pop = tricore_opcodes;
  for (opind = 0; opind < NUMOPCODES; ++opind)
    {
      hpop = pop; hopind = opind;
      retval = str_hash_insert (hash_ops, pop->name, pop, 0);

      if (retval && *retval)
        as_fatal (_("Cannot hash %s"), pop->name);
      while ((++hopind != NUMOPCODES) && !strcmp ((++hpop)->name, pop->name))
        /* Empty.  */;
      pop->insind = opind;
      pop->inslast = --hopind;
      pop = hpop;
      opind = hopind;
    }

  /* Populate the PCP opcode hash table.  */

  ppop = pcp_opcodes;
  for (opind = 0; opind < NUMPCPOPCODES; ++opind)
    {
      hppop = ppop; hopind = opind;
	  retval = str_hash_insert (hash_pcp, ppop->name, ppop, 0);
      if (retval && *retval)
        as_fatal (_("Cannot hash %s"), ppop->name);
      while ((++hopind != NUMPCPOPCODES)
             && !strcmp ((++hppop)->name, ppop->name))
	/* Empty.  */;
      ppop->insind = opind;
      ppop->inslast = --hopind;
      ppop = hppop;
      opind = hopind;
    }

  /* Populate the SFR hash table.  */

  psfr = tricore_sfrs;
  for (opind = 0; opind < NUMSFRS; ++opind, ++psfr)
    if (MATCHES_ISA (psfr->isa))
      {
		retval = str_hash_insert (hash_sfr, psfr->name, psfr, 0);
        if (retval && *retval)
          as_fatal (_("Cannot hash %s"), psfr->name);
      }

  /* Populate the 16->32-bit-insn-conversion hash table.  */

  pinsn16 = insn16_table;
  for (opind = 0; opind < num_insn16; ++opind, ++pinsn16)
    {
      if (!MATCHES_ISA (pinsn16->isa))
        continue;
	  retval = str_hash_insert (hash_insn16, pinsn16->name, pinsn16, 0);

      if (retval && *retval)
        as_fatal (_("Cannot hash %s"), pinsn16->name);
      pop = (opcode_t *) str_hash_find (hash_ops, pinsn16->new_name);
      if ((hpop = pop) == NULL)
        as_fatal (_("Internal error: couldn't find `%s'"), pinsn16->new_name);
      for (hopind = hpop->insind; hopind <= hpop->inslast; ++hopind, ++pop)
        if (pop->len32 && MATCHES_ISA (pop->isa)
	    && !strcmp (pop->args, pinsn16->new_args))
	  {
	    pinsn16->new_code = pop;
	    break;
	  }
      if (pinsn16->new_code == NULL)
        as_fatal (_("Internal error: couldn't init `%s'"), pinsn16->name);
    }

  /* Populate the 32->16-bit-insn-conversion hash table.  */

  pinsn32 = insn32_table;
  for (opind = 0; opind < num_insn32; ++opind, ++pinsn32)
    {
      if (!MATCHES_ISA (pinsn32->isa))
        continue;
	  
	  retval = str_hash_insert (hash_insn32, pinsn32->name, pinsn32, 0);

      if (retval && *retval)
        as_fatal (_("Cannot hash %s"), pinsn32->name);
      for (i = 0; i < pinsn32->nr_alt; ++i)
        {
	  palt = &pinsn32->alt[i];
	  pop = (opcode_t *) str_hash_find (hash_ops, palt->new_name);
	  if ((hpop = pop) == NULL)
	    as_fatal (_("Internal error: couldn't find `%s'"), palt->new_name);
	  for (hopind = hpop->insind; hopind <= hpop->inslast; ++hopind, ++pop)
	    if (!pop->len32 && MATCHES_ISA (pop->isa)
	        && !strcmp (pop->args, palt->new_args))
	      {
	        palt->new_code = pop;
		break;
	      }
	  if (palt->new_code == NULL)
	    as_fatal (_("Internal error: couldn't init `%s'"), pinsn32->name);
	}
    }

  /* Perform sanity checks if in debug mode.  */

  if (show_internals)
    check_insn_tables ();
 
  /* Initialize the "operand compatibility matrix".  */

  for (opind = 0; opind < 256; ++opind)
    pseudo_codes[opind] = NULL;
	pseudo_codes['d'] = (char *) "di";
	pseudo_codes['g'] = (char *) "g";
	pseudo_codes['G'] = (char *) "G";
	pseudo_codes['-'] = (char *) "-";
	pseudo_codes['+'] = (char *) "+";
	pseudo_codes['l'] = (char *) "l";
	pseudo_codes['L'] = (char *) "L";
	pseudo_codes['D'] = (char *) "D";
	pseudo_codes['i'] = (char *) "i";
	pseudo_codes['a'] = (char *) "aAIP";
	pseudo_codes['A'] = (char *) "AP";
	pseudo_codes['I'] = (char *) "I";
	pseudo_codes['P'] = (char *) "P";
	pseudo_codes['c'] = (char *) "cU123f5v68nqW";
	pseudo_codes['1'] = (char *) "1U";
	pseudo_codes['2'] = (char *) "2U1";
	pseudo_codes['3'] = (char *) "3U12";
	pseudo_codes['4'] = (char *) "4U123";
	pseudo_codes['f'] = (char *) "fU123";
	pseudo_codes['5'] = (char *) "5U123fvmx";
	pseudo_codes['F'] = (char *) "FU1234f";
	pseudo_codes['v'] = (char *) "vU123fm";
	pseudo_codes['6'] = (char *) "6U123f5vmx";
	pseudo_codes['8'] = (char *) "8U123f5v6mx";
	pseudo_codes['9'] = (char *) "9U1234f5Fv68mxrR";
	pseudo_codes['n'] = (char *) "nU123f5v68mx";
	pseudo_codes['k'] = (char *) "kU123f5v68nmx";
	pseudo_codes['0'] = (char *) "0U1234f5Fv689nmxrR";
	pseudo_codes['q'] = (char *) "qU123f5v68nk";
	pseudo_codes['w'] = (char *) "wU1234f5Fv689nk0qmxrRo";
	pseudo_codes['W'] = (char *) "WU123f5v68nkqmx";
	pseudo_codes['M'] = (char *) "MU1234f5Fv689nk0qwWmxrRoO";
	pseudo_codes['m'] = (char *) "mU123fv";
	pseudo_codes['x'] = (char *) "xU123m";
	pseudo_codes['r'] = (char *) "rUF4";
	pseudo_codes['R'] = (char *) "RU1234f5Fv689mxr";
	pseudo_codes['o'] = (char *) "oU1234f5Fv689nk0qwmxrR";
	pseudo_codes['O'] = (char *) "OU1234f5Fv689nk0qwWmxrRo";
	pseudo_codes['t'] = (char *) "tU1234f5Fv689nk0qwWMmxrRoO";
	pseudo_codes['T'] = (char *) "TU1234f5Fv689nk0qwWMmxrRoOt";
	pseudo_codes['U'] = (char *) "U";
	pseudo_codes['@'] = (char *) "@S&";
	pseudo_codes['&'] = (char *) "&";
	pseudo_codes['<'] = (char *) "<";
	pseudo_codes['>'] = (char *) ">";
	pseudo_codes['*'] = (char *) "*";
	pseudo_codes['#'] = (char *) "#";
	pseudo_codes['?'] = (char *) "?";
	pseudo_codes['S'] = (char *) "S";

  /* Allocate memory for the temporary line buffer.  */

  insnline = xmalloc (INITIAL_INSNLINELENGTH);
  insnline_len = INITIAL_INSNLINELENGTH;

  /* Set optimization flag.  */

  optimize_next = optimize;

  /* Make sure the text, data and bss sections are properly aligned.  */

  (void) bfd_set_section_alignment (text_section, 1);
  if (current_isa == TRICORE_V1_3)
    {
      /* FIXME: quad-word objects must be aligned to 2**4, which probably
         means that we should dedicate one or more separate sections to
	 them -- just to avoid wasting precious memory due to alignment
	 restrictions when linking the final program.  However, I'm not
	 sure whether this can (or should) be handled automatically by
	 the assembler.  Come to think of it, I'm rather against it.  */
      (void) bfd_set_section_alignment ( data_section, 1);
      (void) bfd_set_section_alignment ( bss_section, 1);
    }
  else
    {
      (void) bfd_set_section_alignment ( data_section, 3);
      (void) bfd_set_section_alignment ( bss_section, 3);
    }
}

/* Check whether the tables for 16/32-bit insn conversions are OK.
   However, we can only test the correctness of the name-operand
   combo of the source insns (the correctness of the target insns
   is tested in md_begin, though).  This function is only called
   if show_internals is set, and thus only used for debugging.  */ 

static void
check_insn_tables ()
{
  insn16_t *pinsn16;
  insn32_t *pinsn32;
  opcode_t *pop, *hpop;
  int opind, hopind;
  char name[30], *cp;

  pinsn16 = insn16_table;
  for (opind = 0; opind < num_insn16; ++opind, ++pinsn16)
    {
      if (!MATCHES_ISA (pinsn16->isa))
        continue;

      strcpy (name, pinsn16->name);
      if ((cp = strchr (name, '-')) == NULL)
        as_fatal (_("Internal error: insn16_table is broken"));
      *cp++ = '\0';
      pop = (opcode_t *) str_hash_find (hash_ops, name);
      if ((hpop = pop) == NULL)
        as_fatal (_("Internal error: can't find `%s' (insn16_table)"), name);
      for (hopind = hpop->insind; hopind <= hpop->inslast; ++hopind, ++pop)
        if (!pop->len32 && MATCHES_ISA (pop->isa) && !strcmp (pop->args, cp))
	  break;
      if (hopind > hpop->inslast)
        as_fatal (_("Internal error: can't find `%s' (insn16_table)"),
		  pinsn16->name);
      if ((pinsn16->nr_inserts < 0) || (pinsn16->nr_inserts > 2))
        as_fatal (_("Internal error: too many inserts for `%s'"),
		  pinsn16->name);
    }

  pinsn32 = insn32_table;
  for (opind = 0; opind < num_insn32; ++opind, ++pinsn32)
    {
      if (!MATCHES_ISA (pinsn32->isa))
        continue;

      strcpy (name, pinsn32->name);
      if ((cp = strchr (name, '-')) == NULL)
        as_fatal (_("Internal error: insn32_table is broken"));
      *cp++ = '\0';
      pop = (opcode_t *) str_hash_find (hash_ops, name);
      if ((hpop = pop) == NULL)
        as_fatal (_("Internal error: can't find `%s' (insn32_table)"), name);
      for (hopind = hpop->insind; hopind <= hpop->inslast; ++hopind, ++pop)
        if (pop->len32 && MATCHES_ISA (pop->isa) && !strcmp (pop->args, cp))
	  break;
      if (hopind > hpop->inslast)
        as_fatal (_("Internal error: can't find `%s' (insn32_table)"),
		  pinsn32->name);
    }
}

/* Handle TriCore-specific assembler options and flags passed on
   the command line.  */

int
md_parse_option (int c, const char *arg)
{
  switch (c)
    {
    case 'a': /* listing option(s); apply standard treatment.  */
      return 0;

    case 'Y':
      show_internals = 1;
      break;

    case 'V':
      /* -V: SVR4 argument to print version ID.  */
      print_version_id ();
      break;

    case 'm':
      if (!strcmp (arg, "tc10")
          || !strcmp (arg, "rider-a")
          || !strcmp (arg, "TriCore:Rider-A"))
        current_isa = (current_isa & ~TRICORE_ISA_MASK) | TRICORE_V1_1;
      else if (!strcmp (arg, "tc12")
      	       || !strcmp (arg, "rider-b")
      	       || !strcmp (arg, "TriCore:Rider-B"))
        current_isa = (current_isa & ~TRICORE_ISA_MASK) | TRICORE_V1_2;
      else if (!strcmp (arg, "tc13")
      	       || !strcmp (arg, "rider-d")
      	       || !strcmp (arg, "TriCore:Rider-D"))
        current_isa = (current_isa & ~TRICORE_ISA_MASK) | TRICORE_V1_2;
      else if (!strcmp (arg, "tc2")
      	       || !strcmp (arg, "rider-2")
      	       || !strcmp (arg, "TriCore:V2"))
        current_isa = (current_isa & ~TRICORE_ISA_MASK) | TRICORE_V1_3;
      else if (!strcmp (arg, "cpu9"))
        {
	  workaround_cpu9 = 1;
	  workaround_cpu34 = 0;
	}
      else if (!strcmp (arg, "cpu34"))
        {
	  workaround_cpu34 = 1;
	  workaround_cpu9 = 0;
	}
      else if (!strcmp (arg, "cpu48"))
        workaround_cpu48 = 1;
      else if (!strcmp (arg, "cpu50"))
        workaround_cpu50 = 1;
      else if (!strcmp (arg, "cpu60"))
        workaround_cpu60 = 1;
      else if (!strcmp (arg, "cpu69"))
        workaround_cpu69 = 1;
      else if (!strcmp (arg, "cpu70"))
        workaround_cpu70 = 1;
      else if (!strcmp (arg, "cpu72"))
        workaround_cpu72 = 1;
      else if (!strcmp (arg, "cpu81"))
        workaround_cpu81 = 1;
      else if (!strcmp (arg, "cpu82"))
        workaround_cpu82 = 1;
      else if (!strcmp (arg, "cpu83"))
        workaround_cpu83 = 1;
      else if (!strcmp (arg, "cpu094"))
        workaround_cpu94 = 1;
      else if (!strcmp (arg, "cpu095"))
        workaround_cpu95 = 1;
      else if (!strcmp (arg, "dmi12"))
        workaround_dmi12 = 1;
      else
        {
          as_bad (_("unrecognized option `-%c%s'"), c, arg ? arg : "");
          return 0;
        }
      break;

    /* Handle long options.  */

    case OPTION_NOOPT:
      optimize = optimize_next = 0;
      break;

    case OPTION_INSN32_ONLY:
      big_insns = big_insns_only = 1;
      break;

    case OPTION_INSN32_PREF:
      big_insns = 1;
      break;

    case OPTION_ENFORCE_ALIGN:
      enforce_aligned_data = 1;
      break;
    default:
      as_bad (_("unrecognized option `-%c%s'"), c, arg ? arg : "");
      return 0;
    }

  return 1;
}

/* This is the callback function for the .code16 pseudo opcode.  */

static void
set_16bit_code_flag (flag)
     int flag;
{
  use_insn16 = (big_insns_only) ? 0 : flag;
  use_insn32 = 0;
}

/* This is the callback function for the .code32 pseudo opcode.  */

static void
set_32bit_code_flag (flag)
     int flag;
{
  use_insn32 = flag;
  use_insn16 = 0;
}

/* This is the callback function for the .optim/.noopt pseudo opcodes.  */

static void
set_optimize_flag (flag)
     int flag;
{
  optimize_next = flag;
}

/* This is the callback function for the .pcpinitword pseudo opcode.  */

static void
pcp_init_word (flag)
     int flag ATTRIBUTE_UNUSED;
{
  expressionS init_pc, init_page, init_flags;
  unsigned long flags = 0;
  char *p;
#define BAD(reason)		\
  {				\
    as_bad (reason);		\
    ignore_rest_of_line ();	\
    return;			\
  }

  BAD("PCP_SEG TO BE IMPLEMENTED");
  return;
//   if (!(now_seg->flags & PCP_SEG) || (now_seg->flags & SEC_CODE))
//     BAD (_(".pcpinitword must only be used within PCP data sections"));

  /* Get expression for initial PC.  */
  if (is_it_end_of_statement ())
    BAD (_("Expecting expression for initial PC"));
  expression (&init_pc);
  if ((init_pc.X_op == O_absent)
      || ((init_pc.X_op == O_constant)
          && (init_pc.X_add_number & ~0xffff)))
    BAD (_("Illegal expression for initial PC"));

  /* Get expression for initial data page.  */
  SKIP_WHITESPACE ();
  if (*input_line_pointer != ',')
    BAD (_("Expecting expression for initial data page"));
  ++input_line_pointer;
  expression (&init_page);
  if ((init_page.X_op == O_absent)
      || ((init_page.X_op == O_constant)
          && (init_page.X_add_number & ~0xff)))
    BAD (_("Illegal expression for initial data page"));

  /* Get expression for initial flags.  */
  SKIP_WHITESPACE ();
  if (*input_line_pointer != ',')
    BAD (_("Expecting expression for initial flags"));
  ++input_line_pointer;
  expression (&init_flags);
  if ((init_flags.X_op == O_absent)
      || ((init_flags.X_op == O_constant)
	  && (init_flags.X_add_number & ~0xff)))
    BAD (_("Illegal expression for initial flags"));
  demand_empty_rest_of_line ();

  /* Now emit the initialization word and all required relocs.  */
  p = frag_more (4);

  if (init_pc.X_op == O_constant)
    flags |= (init_pc.X_add_number << 16);
  else
    fix_new_exp (frag_now, (p + 2 - frag_now->fr_literal), 0, &init_pc, 0,
    		 BFD_RELOC_TRICORE_PCPTEXT);

  if (init_page.X_op == O_constant)
    flags |= (init_page.X_add_number << 8);
  else
    fix_new_exp (frag_now, (p - frag_now->fr_literal), 0, &init_page, 0,
    		 BFD_RELOC_TRICORE_PCPPAGE);

  if (init_flags.X_op == O_constant)
    flags |= init_flags.X_add_number;
  else
    fix_new_exp (frag_now, (p - frag_now->fr_literal), 0, &init_flags, 0,
    		 BFD_RELOC_TRICORE_8ABS);

  md_number_to_chars (p, flags, 4);

  if (show_internals)
    printf ("*** pcpinitword: word = 0x%08lx, relocs = %s, %s, %s\n", flags,
      	    (init_pc.X_op != O_constant) ? "PCPTEXT" : "none",
	    (init_page.X_op != O_constant) ? "PCPPAGE" : "none",
	    (init_flags.X_op != O_constant) ? "8ABS": "none");
#undef BAD
}

/* This is the callback function for the .bit/.lbit pseudo opcodes.  */
#pragma GCC diagnostic ignored "-Wstack-usage="
static void
create_bit (int global)

{
  char *bname, *pname, *sname = NULL, *p, c;
  symbolS *bsym, *psym;
  elf_symbol_type *elfsym;
  segT current_seg = now_seg;
  subsegT current_subseg = now_subseg;
  segT bdata_sec, bpos_sec;
  int val = -1;

  /* Get the name of the bit symbol.  */
  bname = input_line_pointer;  
  p = input_line_pointer;
  char **tmp = &p; //todo understand what should be tmp
  c = get_symbol_name(tmp);
  if (bname == p)
    {
      as_bad (_("Expected symbol name for bit variable"));
      *p = c;
      discard_rest_of_line ();
      return;
    }

  /* Make sure that local label names aren't used as local bit names, as
     they'll usually be stripped from the symbol table.  */
  if (!global && bfd_is_local_label_name (stdoutput, bname))
    {
      as_bad (_("Can't use local label name <%s> as local bit name"), bname);
      *p = c;
      discard_rest_of_line ();
      return;
    }

  /* Gripe if this symbol is already defined, or else create a symbol
     table entry for it.  */
  if (((bsym = symbol_find (bname)) != NULL)
      && S_IS_DEFINED (bsym))
    {
      as_bad (_("Symbol <%s> already defined"), bname);
      *p = c;
      discard_rest_of_line ();
      return;
    }
  else if (bsym == NULL)
    bsym = symbol_find_or_make (bname);

  /* This prevents error messages like "Symbol <foo.pos> already defined" if
     some error occurs before the actual section is set for "foo" at the end
     of this function, and another .bit/.lbit pseudo-op refers to "foo" again;
     by tentatively setting "foo"'s section to .boffs (and thus "defining" the
     bit symbol), we'll get the correct error message "Symbol <foo> already
     defined" in these cases.  */
  bpos_sec = subseg_new (".boffs", 0);
  S_SET_SEGMENT (bsym, bpos_sec);

  /* Create another symbol for it in section .boffs that holds its bit offset
     within the containing byte; gripe if this symbol already exists (note
     that it's irrelevant whether the symbol already exists because it has
     been actually defined, or merely because it was referenced: bit position
     symbols may only be created by .bit/.lbit pseudo-ops (both handled by
     this function), and the only legal way to refer to them is by using the
     "bpos:" prefix -- and even this prefix refers only to the bit symbol
     itself, _not_ to the corresponding bit offset symbol, so either a defined
     or an undefined bit offset symbol spells trouble).  */
  pname = alloca (strlen (bname) + 5);
  sprintf (pname, "%s.pos", bname);
  *p = c;
  if ((psym = symbol_find (pname)) != NULL)
    {
      as_bad (_("Bit position symbol <%s> already exists"), pname);
      discard_rest_of_line ();
      return;
    }
  else
    psym = symbol_find_or_make (pname);
  seg_info (bpos_sec)->bss = 1;
  bfd_set_section_flags ( bpos_sec, SEC_ALLOC | SEC_READONLY);
  S_SET_SEGMENT (psym, bpos_sec);
  if (global)
    S_SET_EXTERNAL (psym);
  else
    S_CLEAR_EXTERNAL (psym);
  subseg_set (current_seg, current_subseg);

  /* Get initialization value and section name, if specified.  */
  SKIP_WHITESPACE ();
  if (*input_line_pointer == ',')
    {
      int nr_errors = had_errors ();

      /* Get initialization value, if specified.  */
      ++input_line_pointer;
      SKIP_WHITESPACE ();
      if (*input_line_pointer != ',')
	{
	  if (is_end_of_line[(unsigned char) *input_line_pointer])
	    {
	      as_bad (_("Missing initialization value for bit variable"));
	      return;
	    }
	  val = get_absolute_expression ();
	  if (nr_errors != had_errors ())
	    {
	      ignore_rest_of_line ();
	      return;
	    }
	  if (val & ~1)
	    {
	      as_bad (_("Illegal bit value; must be 0 or 1"));
	      ignore_rest_of_line ();
	      return;
	    }
	  SKIP_WHITESPACE ();
	}

      /* Get the section name, if specified.  */
      if (*input_line_pointer == ',')
	{
	  char *secname;
	  asection *sec;

	  ++input_line_pointer;
	  SKIP_WHITESPACE ();
	  secname = input_line_pointer;
	  c = get_symbol_name(&input_line_pointer); //TODO understand what should be the arg here
	  p = input_line_pointer;
	  if (secname == p)
	    {
	      as_bad (_("Expected bit section name"));
	      *p = c;
	      discard_rest_of_line ();
	      return;
	    }

	  /* Make sure we're dealing with a valid bit section name.  */
	  if (strcmp (secname, ".bdata") && strncmp (secname, ".bdata.", 7)
	      && strcmp (secname, ".bbss") && strncmp (secname, ".bbss.", 6))
	    {
	      as_bad (_("Illegal bit section name \"%s\""), secname);
	      *p = c;
	      discard_rest_of_line ();
	      return;
	    }

	  /* If it's a new bit section, make a permanent copy of its name,
	     or else re-use its already existing name.  */
	  if ((sec = bfd_get_section_by_name (stdoutput, secname)) == NULL)
	    sname = xstrdup (secname);
	  else
	    sname = (char *) sec->name;
	  *p = c;
	}
    }
  demand_empty_rest_of_line ();

  /* If no section name was explicitly specified, default to .bdata or .bbss,
     depending on whether an initialization value was given or not.  */
  if (sname == NULL)
    {
	  
      sname = (char* ) ((val == -1) ? ".bbss" : ".bdata");
    }

  if (val == -1)
    {
      /* Try to assign the symbol to a bit BSS section.  */
      if (strcmp (sname, ".bbss") && strncmp (sname, ".bbss.", 6))
        {
	  as_bad (_("Illegal bit BSS section name \"%s\""), sname);
	  return;
	}
      bdata_sec = subseg_new (sname, 0);
      seg_info (bdata_sec)->bss = 1;
      bfd_set_section_flags (bdata_sec, SEC_ALLOC);
    }
  else
    {
      /* Try to assign the symbol to a bit data section.  */
      if (strcmp (sname, ".bdata") && strncmp (sname, ".bdata.", 7))
        {
	  as_bad (_("Illegal bit data section name \"%s\""), sname);
	  return;
	}
      bdata_sec = subseg_new (sname, 0);
      bfd_set_section_flags (bdata_sec, SEC_ALLOC | SEC_LOAD | SEC_DATA);
    }

  /* Finally, define the bit symbol in the requested section.  */
  S_SET_SEGMENT (bsym, bdata_sec);
  S_SET_VALUE (bsym, (valueT) frag_now_fix ());
  S_SET_SIZE (bsym, 1);
  symbol_set_frag (bsym, frag_now);
  
  elfsym = (elf_symbol_type *) symbol_get_bfdsym (bsym);
  elfsym->symbol.flags |= BSF_OBJECT;
  if (global)
    S_SET_EXTERNAL (bsym);
  else
    S_CLEAR_EXTERNAL (bsym);
  p = frag_more (1);
  *p = (val == -1) ? 0 : val;
  subseg_set (current_seg, current_subseg);

  if (show_internals)
    {
      pname[strlen (pname) - 4] = '\0';
      printf ("*** .%s <%s>,", global ? "bit" : "lbit", pname);
      if (val == -1)
        printf (",<%s>\n", sname);
      else
        printf ("%d,<%s>\n", val, sname);
    }
}

/* This is the callback function for .bpos and friends.  */

static void
bpcons (nbytes)
     int nbytes;
{
  expressionS bexp;
  char *p;

  /* Get expression for the bit symbol.  */
  if (is_it_end_of_statement ())
    {
      as_bad (_("Expected symbol name for bit variable"));
      discard_rest_of_line ();
      return;
    }
  expression (&bexp);
  if ((bexp.X_op == O_absent)
      || ((bexp.X_op == O_constant)
          && (bexp.X_add_number & ~0x7))
      || ((bexp.X_op != O_constant)
          && (bexp.X_add_number != 0))
      || (bexp.X_add_symbol == NULL)
      || (bexp.X_op_symbol != NULL))
    {
      as_bad (_("Illegal expression for bit variable"));
      discard_rest_of_line ();
      return;
    }
  demand_empty_rest_of_line ();

  /* Now emit the byte(s) and required relocs.  */
  frag_grow (nbytes);
  p = frag_more (nbytes);
  if (bexp.X_op == O_constant)
    {
      md_number_to_chars (p, bexp.X_add_number, nbytes);
      if (show_internals)
        printf ("*** bpcons(%d): val = %ld\n", nbytes, bexp.X_add_number);
    }
  else
    {
      enum bfd_reloc_code_real reloc = NO_RELOC;

      if (nbytes == 1)
        reloc = BFD_RELOC_8;
      else if (nbytes == 2)
        reloc = BFD_RELOC_16;
      else if (nbytes == 4)
        reloc = BFD_RELOC_32;

      md_number_to_chars (p, 0, nbytes);
      fix_new_exp (frag_now, (p - frag_now->fr_literal), 0, &bexp, 0,
      		   BFD_RELOC_TRICORE_BITPOS);
      fix_new_exp (frag_now, (p - frag_now->fr_literal), 0, &bexp, 0, reloc);

      if (show_internals)
        printf ("*** bpcons(%d): sym = <%s>, relocs = %s, %s\n",
	        nbytes, S_GET_NAME (bexp.X_add_symbol),
		"BFD_RELOC_TRICORE_BITPOS", GET_RELOC_NAME (reloc));
    }
}

/* This is the callback function for .toc and friends.  */

static void
make_section (sec)
     int sec;
{
  asec_t *aptr;

  if ((sec < 0) || (sec >= nr_addsecs))
    {
      as_bad (_("Illegal section %d in make_section"), sec);
      return;
    }

  aptr = &addsecs[sec];
  *aptr->sec_ptr = subseg_new (aptr->sec_name, 0);
  if (!aptr->sec_exists)
    {
      bfd_set_section_flags (*aptr->sec_ptr, aptr->sec_flags);
      bfd_set_section_alignment (*aptr->sec_ptr, aptr->sec_align);
      aptr->sec_exists = 1;
      if (show_internals)
        printf ("*** Created new section \"%s\".\n", aptr->sec_name);
    }
}

/* Map ELF section letter 'p' to SHF_TRICORE_PCP.  */

int
tricore_elf_section_letter (letter, ptr_msg)
     int letter;
     char **ptr_msg;
{
  if (letter == 'p')
    return SHF_TRICORE_PCP;

  *ptr_msg = _("Bad .section directive: want a,p,w,x,M,S,G in string");
  return 0;
}

/* This is called to convert special ELF section flags to their
   BFD equivalent.  */

flagword
tricore_elf_section_flags (flagword flags, int attr ATTRIBUTE_UNUSED, int type ATTRIBUTE_UNUSED)
 {
  //if (attr & SHF_TRICORE_PCP)
    //flags |= SEC_ARCH_BIT_0;

  return flags;
}

/* This is called upon entering a new section.  If the new section's PCP_SEG
   flag is set and the section has just been created, we'll set a reasonable
   default alignment for it (NB: it's not wise to do this for every section
   that appears to contain code or data, because some sections (notably debug
   sections) must be byte-aligned to function correctly).  We're also making
   sure that the user can't create or enter bit data sections via ".section"
   and similar pseudo-ops.  */

void
tricore_elf_section_change_hook ()
{
  //int /*align,*/ old_align;

  if ((now_seg->name[0] == '.') && (now_seg->name[1] == 'b')
      && (!strcmp (now_seg->name, ".boffs")
	  || !strcmp (now_seg->name, ".bdata")
	  || !strncmp (now_seg->name, ".bdata.", 7)
	  || !strcmp (now_seg->name, ".bbss")
	  || !strncmp (now_seg->name, ".bbss.", 6)))
    {
      as_bad (_("Illegal attempt to create or enter bit data section \"%s\""),
      	      now_seg->name);
      return;
    }

  //   Since PCP_SEG isn't used, this section is useless 
  // old_align = (now_seg->alignment_power + 0);

  //if (old_align > 0)
  //  return;  /* Alignment already set; we're done.  */


//   if (now_seg->flags & PCP_SEG)
//     {
//       /* Align PCODE sections to 2**1 and PRAM sections to 2**6.  */
//       align = (now_seg->flags & SEC_CODE) ? 1 : 6;
//       if (show_internals)
//         printf ("*** Setting alignment of %s section \"%s\" to 2**%d\n",
//     	        align == 1 ? "PCODE" : "PRAM", now_seg->name, align);
//       bfd_set_section_alignment (now_seg, align);
//     }  
}

/* This is the callback function for .uahalf and friends.  */

static void
tricore_uacons (bytes)
     int bytes;
{
  /* Tell tricore_cons_align to not align this value.  */
  tricore_no_align_cons = 1;
  cons (bytes);
}

/* This is called back by cons () and aligns constants if required.  */

void
tricore_cons_align (nbytes)
     int nbytes;
{
  int nalign;
  //char *p;

  /* Only do this if we are enforcing aligned data.  */
  if (!enforce_aligned_data)
    return;

  if (tricore_no_align_cons)
    {
      /* This is an unaligned pseudo-op.  */
      tricore_no_align_cons = 0;
      return;
    }

  nalign = 0;
  while ((nbytes & 1) == 0)
    {
      ++nalign;
      nbytes >>= 1;
    }

  if (nalign == 0)
    return;

  if (now_seg == absolute_section)
    {
      if ((abs_section_offset & ((1 << nalign) - 1)) != 0)
        as_bad (_("Misaligned data in absolute section"));
      return;
    }
  //unused variable
  /*p = frag_var (rs_align_code, 1, 1, (relax_substateT) 0,
  		(symbolS *) NULL, (offsetT) nalign, (char *) NULL);*/

  record_alignment (now_seg, nalign);
}

/* Show which TriCore-specific options we can offer.  */

void
md_show_usage (stream)
     FILE *stream;
{
  fprintf (stream, _("\
TriCore-specific options:\n\
  -V			  print assembler version number\n\
  -Y			  print internal information (helps debugging `as')\n\
  -mtc10		  assemble for the TC1V1.0 instruction set (deprecated)\n\
  -mtc12		  assemble for the TC1V1.[23] instruction set (default)\n\
  -mtc13		  assemble for the TC1V1.[23] instruction set (default)\n\
  -mtc2			  assemble for the TC2 instruction set\n\
  -mcpu9		  insert 2 NOPs after DSYNC (workaround for CPU.9 bug)\n\
  -mcpu34		  insert ISYNC after DSYNC (workaround for CPU_TC.034/\n\
			    COR17 bug)\n\
  -mcpu48		  insert a NOP between LD.[A,DA] and an immediately\n\
  			    following indirect jump or call instruction\n\
			    (workaround for CPU_TC.048 bug)\n\
  -mcpu50		  insert a NOP after certain multi-cycle instructions\n\
			    followed by LD.x (workaround for CPU_TC.050 bug)\n\
  -mcpu60		  insert a NOP between LD.[A,DA] and an immediately\n\
  			    following LD.[D,W] instruction (workaround for\n\
			    CPU_TC.060 bug)\n\
  -mcpu69		  insert a NOP after a RSLCX instruction (workaround\n\
			    for CPU_TC.069 bug)\n\
  -mcpu70		  insert 1-2 NOPs after a conditional jump and a LOOP\n\
  			    instruction following it (workaround for CPU_TC.070\n\
			    bug)\n\
  -mcpu72		  insert a NOP between LD.[A,DA] and an immediately\n\
  			    following LOOP instruction (workaround for\n\
			    CPU_TC.072 bug)\n\
  -mcpu81		  forbid loading of %%A10 from memory (workaround\n\
			    for CPU_TC.081 bug)\n\
  -mcpu82		  insert a NOP between ST[LU]CX and an immediately\n\
			    following LD.x instruction (workaround for\n\
			    CPU_TC.082 bug)\n\
  -mcpu83		  insert a NOP after a DISABLE instruction (workaround\n\
			    for CPU_TC.083 bug)\n\
  -mcpu094		  insert a NOP between an integer pipeline jump instruction\n\
				and a CSA instruction (workaround for CPU_TC.094 bug)\n\
  -mcpu095		  insert a NOP between a sat instruction a load and store\n\
				pipeline instruction (addsc, mov.a, mtcr) \n\
				(workaround for CPU_TC.095 bug)\n\
  -mdmi12		  insert a NOP before or after certain load/store and\n\
  			    jump instructions (workaround for DMI_TC.012 bug)\n\
  --dont-optimize	  don't try to find the shortest matching opcode\n\
			    (.optim, .code16 and .code32 are honored)\n\
  --insn32-only		  use only 32-bit instructions (.code16 is ignored)\n\
  --insn32-preferred	  use only 32-bit instructions (.code16 is honored)\n\
  --enforce-aligned-data  force .hword/.word/etc. to be aligned correctly\n"));
}

/* See if we need to do something special with an undefined symbol.
   This is true if NAME represents a TriCore core register.  */

symbolS *
md_undefined_symbol (char *name)
{
  char *src, *dst, *cname;
  sfr_t *creg;
  symbolS *sym;

  assert (name);
  if (*name == '$')
    {
      cname = xmalloc (strlen (name) + 1);
      for (src = name, dst = cname; *src; /* Empty.  */)
        if ((*src >= 'A') && (*src <= 'Z'))
          *dst++ = (*src++) - 'A' + 'a';
        else
          *dst++ = *src++;
      *dst = '\0';
      creg = str_hash_find (hash_sfr, cname);
      free (cname);
      if (creg != NULL)
        {
	  if ((sym = symbol_find (creg->name)) != NULL)
	    return sym;

	  return symbol_new (creg->name, absolute_section,
	  		     &zero_address_frag, (valueT) creg->addr);
	}
    }

  return (symbolS *) 0;
}

/* Turn the floating point constant pointed to by input_line_pointer into
   its binary equivalent of type TYPE.  Write the bytes in the correct
   byte order to the buffer pointed to by LITP and set *SIZEP to the
   number of bytes written.  Return an error message, or NULL on OK.  */

const char *
md_atof (int type, char* litP, int* sizeP)
{
  int i, prec;
  char *t;
  LITTLENUM_TYPE words[6]; /* atof-ieee.c wants 6 words.  */
  static const int wordsize = sizeof (LITTLENUM_TYPE);

  if ((type == 'f') || (type == 'F'))
    prec = 2;
  else if ((type == 'd') || (type == 'D'))
    prec = 4;
  else
    {
      *sizeP = 0;
      return "Illegal precision specification";
    }

  if ((t = atof_ieee (input_line_pointer, type, words)) != NULL)
    input_line_pointer = t;
  *sizeP = prec * wordsize;

  /* TriCore is little-endian.  */
  for (i = prec - 1; i >= 0; --i)
    {
      md_number_to_chars (litP, (valueT) words[i], wordsize);
      litP += wordsize;
    }

  return NULL;
}

/* Return the closest matching operand type for the constant value C.  */

static char
classify_const (c)
     long c;
{
  if (c < 0)
    {
      if (c >= -8)
        return '4';

      if (c >= -16)
        return 'F';

      if ((c >= -32) && !(c & 1))
	return 'r';

      if (c >= -256)
        return (c & 1) ? '9' : 'R';

      if (c >= -512)
        return '0';

      if (c >= -32768)
        return (c & 1) ? 'w' : 'o';

      if ((c >= -16777216) && !(c & 1))
        return 'O';
    }
  else
    {
      if (c < 2)
        return '1';

      if (c < 4)
        return '2';

      if (c < 8)
        return '3';

      if (c < 16)
        return 'f';

      if (c < 31)
        return (c & 1) ? '5' : 'v';

      if (c < 32)
        return '5';

      if ((c < 61) && !(c & 3))
        return '6';

      if ((c < 63) && !(c & 1))
        return 'x';

      if (c < 256)
        return '8';

      if (c < 512)
        return 'n';

      if ((c < 1024) && !(c & 3))
        return 'k';

      if (c < 32768)
        return 'q';

      if (c < 65536)
        return 'W';

      if ((c < 16777215) && !(c & 1))
        return 'O';
    }

  if (!(c & 0x0fffc000))
    return 't';
  else if (!(c & 0x0fe00001))
    return 'T';

  return 'M';
}

/* Return the register number starting at *STR and advance *STR
   to the first character after the number.  If no valid number
   can be found, -1 is returned and the_insn.error is set to an
   appropriate error message.  Valid regnos range from 00 to 15.  */

static int
read_regno (str)
     char **str;
{
  char *pos = *str;
  int no = 0, digits_seen = 0;

  while ((*pos >= '0') && (*pos <= '9'))
    {
      ++digits_seen;
      no = no * 10 + (*pos - '0');
      if ((no > 15) || (digits_seen > 2))
	{
	  the_insn.error = _("Invalid register number");
	  return -1;
	}
      else
	++pos;
    }

  if (!digits_seen)
    {
      the_insn.error = _("Missing register number");
      return -1;
    }

  *str = pos;
  return no;
}

/* *STR points to the first character after a data register specification
   and **STR is not '\0'.  This may mean there's a register suffix; if so,
   return the appropriate mode and advance *STR to the first character
   after the suffix.  Valid suffixes are u, l, ll, lu, ul and uu.  */

static char
read_regsuffix (str)
     char **str;
{
  char *pos = *str;

  if (*pos == 'l')
    {
      if (*(pos + 1) == 'l')
        {
	  *str += 2;
	  return '-';
	}
      else if (*(pos + 1) == 'u')
        {
	  *str += 2;
	  return 'l';
	}

      *str += 1;
      return 'g';
    }
  else if (*pos == 'u')
    {
      if (*(pos + 1) == 'l')
        {
	  *str += 2;
	  return 'L';
	}
      else if (*(pos + 1) == 'u')
        {
	  *str += 2;
	  return '+';
	}

      *str += 1;
      return 'G';
    }

  return 'd';  /* Will immediately lead to an error, as **str != '\0'.  */
}

/* Parse an expression for operand OPNR starting at *STR (SRC points
   to the original input line).  Advance *STR to the first character
   after the expression.  If no valid expression can be found, 0 is
   returned and the_insn.error is set to an appropriate error message,
   otherwise 1 is returned.  */

static int
get_expression (const char *src, char** str, int opnr)
{
  char *cp = *str, *save_in, *save_oldpos, *colon, *pfx;
  
  const char *pfxs = " hi: lo: sm: up: got: gothi: gotlo: gotup:"
  		     " gotoff: gotoffhi: gotofflo: gotoffup:"
		     " gotpc: gotpchi: gotpclo: gotpcup: plt: bpos:";
  const char *plen = " 3   3   3   3   4    6      6      6     "
  		     " 7       9         9         9        "
		     " 6      8        8        8        4    5";
  const char *pcod = " 1   2   3   4   5    6      7      8     "
  		     " 9       10        11        12       "
		     " 13     14       15       16       17   18";

  /* Filter out prefixes.  */
  if ((colon = strchr (cp, ':')) != NULL)
    {
      char save = colon[1];

      colon[1] = '\0';
      if (((pfx = strstr (pfxs, cp)) != NULL) && (pfx[-1] == ' '))
        {
	  int i, diff;

	  diff = pfx - pfxs;
	  sscanf (&pcod[diff], "%d", &i);
	  if ((prefix_t) i == PREFIX_BITPOS)
	    the_insn.bitpos[opnr] = 1;
	  else
	    {
	      the_insn.prefix[opnr] = (prefix_t) i;
	      the_insn.needs_prefix = 1;
	    }
	  sscanf (&plen[diff], "%d", &i);
	  cp += i;
	}
      colon[1] = save;
    }

  /* See if this expression references the special GOT symbol.  */  
  if (strstr (src + (cp - insnline), "_GLOBAL_OFFSET_TABLE_"))
    {
      switch (the_insn.prefix[opnr])
	{
	case PREFIX_NONE:
	  the_insn.prefix[opnr] = PREFIX_GOTPC;
	  break;

	case PREFIX_HI:
	  the_insn.prefix[opnr] = PREFIX_GOTPCHI;
	  break;

	case PREFIX_LO:
	  the_insn.prefix[opnr] = PREFIX_GOTPCLO;
	  break;

	case PREFIX_UP:
	  the_insn.prefix[opnr] = PREFIX_GOTPCUP;
	  break;

	case PREFIX_GOTPC:
	case PREFIX_GOTPCHI:
	case PREFIX_GOTPCLO:
	case PREFIX_GOTPCUP:
	  break;

	default:
	  as_bad (_("Illegal prefix for GOT expression"));
	  break;
	}
    }

  save_in = input_line_pointer;
  save_oldpos = input_line_pointer = (char*) src + (cp - insnline);
  expression (&the_insn.opexp[opnr]);
  *str = cp + (input_line_pointer - save_oldpos);
  input_line_pointer = save_in;

  if (**str != '\0')
    {
      the_insn.error = _("Trailing chars after expression");
      return 0;
    }

  if (the_insn.opexp[opnr].X_op == O_absent)
    {
      the_insn.error = _("Illegal expression");
      return 0;
    }

  if (the_insn.opexp[opnr].X_op == O_constant)
    {
      switch (the_insn.prefix[opnr])
        {
	case PREFIX_NONE:
	case PREFIX_HI:
	case PREFIX_LO:
	case PREFIX_UP:
	  break;

	default:
	  the_insn.error = _("Illegal prefix for constant expression");
	  return 0;
	}
      if (the_insn.bitpos[opnr] && ((VAL (opnr) < 0) || (VAL (opnr) > 7)))
	{
	  the_insn.error = _("Illegal constant bit position");
	  return 0;
        }
      the_insn.ops[opnr] = classify_const (VAL (opnr));
      the_insn.is_odd[opnr] = (VAL (opnr) & 1);
      if (the_insn.prefix[opnr] != PREFIX_NONE)
        the_insn.ops[opnr] = 'q'; /* Matches both 'w' and 'W'!  */
      else if (the_insn.ops[opnr] == 'k')
        the_insn.matches_k[opnr] = 1;
      else if (the_insn.ops[opnr] == '6')
	{
	  the_insn.matches_6[opnr] = 1;
	  the_insn.matches_k[opnr] = 1;
	}
      else if (strchr ("123fmxv", the_insn.ops[opnr]))
	{
	  if (!(VAL (opnr) & 1))
	    the_insn.matches_v[opnr] = 1;
	  if (!(VAL (opnr) & 3))
	    {
	      the_insn.matches_6[opnr] = 1;
	      the_insn.matches_k[opnr] = 1;
	    }
	}
      else if (strchr ("58n", the_insn.ops[opnr]))
        if (!(VAL (opnr) & 3))
	  the_insn.matches_k[opnr] = 1;
    }
  else
    {
      the_insn.ops[opnr] = 'U';
      the_insn.matches_v[opnr] = 1;
      the_insn.matches_6[opnr] = 1;
      the_insn.matches_k[opnr] = 1;
    }

  return 1;
}

/* Parse the TriCore instruction starting at STR and fill the_insn.  */

static void
tricore_ip (const char* str)
{
  char *src, *dst, mode;
  int numops = -1;
  int preinc, postinc;
  size_t str_len;
  int not_hack = 0;

  if ((str_len = strlen (str)) >= insnline_len)
    {
      insnline_len = str_len + 1;
      insnline = xrealloc (insnline, insnline_len);
    }

  /* Make a lower-case-only copy of the input line.  */
  for (src = (char *) str, dst = insnline; *src; /* Empty.  */)
    if ((*src >= 'A') && (*src <= 'Z'))
      *dst++ = (*src++) - 'A' + 'a';
    else
      *dst++ = *src++;
  *dst = '\0';

  /* Lookup the instruction name.  */
  dst = strtok (insnline, ", ");
  if ((*insnline == 'n') && !strcmp (insnline, "not"))
    {
      /* Pseudo-insn "not" is actually a "nor".  */
      insnline[2] = 'r';
      not_hack = 1;
    }
  else if ((*insnline == 'c') && !strcmp (insnline, "call"))
    the_insn.is_call = 1;
  else if ((*insnline == 'l') && !strcmp (insnline, "loop"))
    the_insn.is_loop = 1;
  else if ((*insnline == 'f') && !strcmp (insnline, "fcall"))
    the_insn.is_call = 1;

  if ((the_insn.code = (opcode_t *) str_hash_find (hash_ops, insnline)) == NULL)
    {
      the_insn.error = _("Unknown instruction");
      return;
    }


  /* Parse the operands.  */
  while ((dst = strtok (NULL, ", ")) != NULL)
    {
      /* Offsets after address mode specifications are not necessarily
         comma-separated, but they need to be treated as operands on
	 their own right.  The label below serves this purpose.  */
restart_scan:
      if (++numops == MAX_OPS)
        {
	  the_insn.error = _("Too many operands");
	  return;
	}
      preinc = postinc = 0;
      switch (*dst)
        {
	case '%': /* A register.  */
	  mode = *++dst;
	  if ((mode == 's') && (dst[1] == 'p') && (dst[2] == '\0'))
	    {
	      /* %sp -> %aSP_REGNUM  */
	      the_insn.regs[numops] = SP_REGNUM;
	      the_insn.ops[numops] = 'P';
	      break;
	    }
	  if ((mode != 'd') && (mode != 'e') && (mode != 'a'))
	    {
	      the_insn.error = _("Invalid register specification");
	      return;
	    }
	  ++dst;
	  if ((the_insn.regs[numops] = read_regno (&dst)) == -1)
	    return;

	  if ((mode == 'd') && (*dst == '+') && (*(dst + 1) == '\0'))
	    {
	      mode = 'e';
	      ++dst;
	    }

	  if ((mode == 'd') && (*dst != '\0'))
	    mode = the_insn.ops[numops] = read_regsuffix (&dst);

	  if (*dst != '\0')
	    {
	      the_insn.error = _("Trailing chars after register specification");
	      return;
	    }
	  if (mode == 'd')
	    {
	      /* A data register.  */
	      if (the_insn.regs[numops] == 15)
	        the_insn.ops[numops] = 'i';
	      else
	        the_insn.ops[numops] = 'd';
	    }
	  else if (mode == 'e')
	    {
	      /* An extended data register.  */
	      if (the_insn.regs[numops] & 1)
	        {
		  the_insn.error = _("Invalid extended register specification");
		  return;
		}
	      the_insn.ops[numops] = 'D';
	    }
	  else if (mode == 'a')
	    {
	      /* An address register.  */
	      if (the_insn.regs[numops] == SP_REGNUM)
	        the_insn.ops[numops] = 'P';
	      else if (the_insn.regs[numops] == 15)
	        the_insn.ops[numops] = 'I';
	      else if (the_insn.regs[numops] & 1)
	        the_insn.ops[numops] = 'a';
	      else
	        the_insn.ops[numops] = 'A';
	    }
	  break;

	case '[': /* An address mode.  */
	  switch (*++dst)
	    {
	    case '\0':
	      the_insn.error = _("Missing address register");
	      return;

	    case '+':
	      ++dst;
	      preinc = 1;
	      break;
	    }
	  if (*dst++ != '%')
	    {
	      the_insn.error = _("Missing address register");
	      return;
	    }
	  if ((*dst == 's') && (dst[1] == 'p'))
	    {
	      /* %sp -> %aSP_REGNUM  */
	      the_insn.regs[numops] = SP_REGNUM;
	      dst += 2;
	    }
	  else if (*dst == 'a')
	    {
	      ++dst;
	      if ((the_insn.regs[numops] = read_regno (&dst)) == -1)
	        return;
	    }
	  else
	    {
	      the_insn.error = _("Invalid or missing address register");
	      return;
	    }
	  if (*dst == '\0')
	    {
	      the_insn.error = _("Missing ']'");
	      return;
	    }
	  else if (*dst == ']')
	    {
	      if (preinc)
	        the_insn.ops[numops] = '<';
	      else
	        if (the_insn.regs[numops] == 15)
		  the_insn.ops[numops] = 'S';
		else if (the_insn.regs[numops] == 10)
		  the_insn.ops[numops] = '&';
		else
		  the_insn.ops[numops] = '@';
	      if (*++dst != '\0')
	        goto restart_scan;
	      else
	        break;
	    }
	  else if (*dst == '+')
	    {
	      if (preinc)
	        {
		  the_insn.error = _("Invalid address mode");
		  return;
		}
	      postinc = 1;
	      if (*++dst == '\0')
	        {
		  the_insn.error = _("Missing ']'");
		  return;
		}
	      if (*dst == ']')
	        {
		  the_insn.ops[numops] = '>';
		  if (*++dst != '\0')
		    goto restart_scan;
		  else
		    break;
		}
	      mode = *dst;
	      if ((mode == 'c') || (mode == 'r') || (mode == 'i'))
	        {
		  if (the_insn.regs[numops] & 1)
		    {
		      the_insn.error = _("Even address register required");
		      return;
		    }
		  if (*++dst != ']')
		    {
		      the_insn.error = _("Missing ']'");
		      return;
		    }
		  if (mode == 'c')
		    {
		      the_insn.ops[numops] = '*';
		      if (*++dst != '\0')
		        goto restart_scan;
		    }
		  else
		    {
		      the_insn.ops[numops] = (mode == 'r') ? '#' : '?';
		      if (*++dst != '\0')
		        {
			  the_insn.error = _("No offset allowed for this mode");
			  return;
			}
		    }
		  break;
		}
	      else
	        {
		  the_insn.error = _("Invalid address mode");
		  return;
		}
	    }
	  else
	    {
	      the_insn.error = _("Invalid address mode");
	      return;
	    }

	default: /* A constant, a symbol or a complex expression.  */
	  if (!get_expression (str, &dst, numops))
	    return;
	  break;
	}
    }

  /* This is because we started with numops = -1.  */
  the_insn.nops = ++numops;

  /* If we've seen a 'not' pseudo-insn, make sure the operands
     are valid: either %dn or %dn,%dn,0.  */
  if (not_hack
      && !(((numops == 1) && strchr ("dDi", the_insn.ops[0]))
           || ((numops == 3)
	       && strchr ("dDi", the_insn.ops[0])
	       && strchr ("dDi", the_insn.ops[1])
	       && (the_insn.regs[0] == the_insn.regs[1])
	       && (the_insn.ops[2] == '1')
	       && (VAL (2) == 0))))
    the_insn.error = _("Invalid operands for pseudo-insn `not'");
}

/* If INSERT_REG is non-zero, insert the register of operand REGNO
   of type MODE at position WHERE in the_insn, otherwise insert a
   constant value REGNO of type MODE at position WHERE.  */

static void
insert_reg_or_const (insert_reg, regno, mode, where)
     int insert_reg, regno, where;
     char mode;
{
  int new, old;

  if (insert_reg)
    regno = the_insn.regs[regno];

  new = the_insn.nops;
  for (old = new - 1; old >= where; --old, --new)
    {
      the_insn.prefix[new] = the_insn.prefix[old];
      the_insn.opexp[new] = the_insn.opexp[old];
      the_insn.ops[new] = the_insn.ops[old];
      the_insn.regs[new] = the_insn.regs[old];
      the_insn.matches_v[new] = the_insn.matches_v[old];
      the_insn.matches_6[new] = the_insn.matches_6[old];
      the_insn.matches_k[new] = the_insn.matches_k[old];
      the_insn.pcrel[new] = the_insn.pcrel[old];
      the_insn.is_odd[new] = the_insn.is_odd[old];
      the_insn.bitpos[new] = the_insn.bitpos[old];
    }

  the_insn.prefix[where] = PREFIX_NONE;
  the_insn.bitpos[where] = 0;
  the_insn.ops[where] = mode;
  memset ((char *) &the_insn.opexp[where], 0, sizeof (expressionS));
  if (insert_reg)
    {
      the_insn.regs[where] = regno;
      the_insn.matches_v[where] = 0;
      the_insn.matches_6[where] = 0;
      the_insn.matches_k[where] = 0;
      the_insn.pcrel[where] = 0;
      the_insn.is_odd[where] = 0;
    }
  else
    {
      the_insn.regs[where] = 0;
      the_insn.matches_v[where] = !(regno & 1);
      the_insn.matches_6[where] = !(regno & 3);
      the_insn.matches_k[where] = !(regno & 3);
      the_insn.pcrel[where] = 0;
      the_insn.is_odd[where] = !(regno & 1);
      VAL (where) = regno;
    }
  ++the_insn.nops;
}

/* If the_insn represents a valid 16-bit opcode, convert it to its
   32-bit equivalent.  We only care about those opcodes that can't be
   found automatically by find_opcode, plus those that are vital for
   the relaxing pass (e.g. j, jeq and jne).  */

static opcode_t *
insn16_to_insn32 ()
{
  opcode_t *pop;
  int i, op;
  insn16_t *pinsn16;
  char name[30];

  /* 16-bit insns can't have prefixes.  */
  if (the_insn.needs_prefix)
    return (opcode_t *) 0;

  /* Try to find a 16-bit opcode that matches the_insn.  */
  pop = the_insn.code;
  for (i = the_insn.code->insind; i <= the_insn.code->inslast; ++pop, ++i)
    {
      if ((pop->nr_operands != the_insn.nops)
          || pop->len32 || !MATCHES_ISA (pop->isa))
        continue;

      for (op = 0; op < the_insn.nops; ++op)
        {
	  if (!strchr (pseudo_codes[(int) pop->args[op]], the_insn.ops[op]))
	    break;
	  if ((pop->args[op] == 'v') && !the_insn.matches_v[op])
	    break;
	  else if ((pop->args[op] == '6') && !the_insn.matches_6[op])
	    break;
	  else if ((pop->args[op] == 'k') && !the_insn.matches_k[op])
	    break;
	}
      if (op == the_insn.nops)
	break;
    }

  if (i > the_insn.code->inslast)
    return (opcode_t *) 0;

  /* Lookup the appropriate entry in insn16_table.  */
  sprintf (name, "%s-%s", pop->name, pop->args);
  if ((pinsn16 = (insn16_t *) str_hash_find (hash_insn16, name)) == NULL)
    as_fatal (_("Internal error: couldn't find insn16 `%s'"), name);

  if (pinsn16->neg_val1)
    {
      if (the_insn.ops[1] == 'U')
        {
	  as_bad (_("Can't convert to 32-bit insn: zconst8 operand unknown"));
	  return (opcode_t *) 0;
	}
      VAL (1) = -VAL (1);
      the_insn.ops[1] = '9';
    }

  /* Insert the necessary registers and/or constants.  */
  if (pinsn16->nr_inserts >= 1)
    insert_reg_or_const (pinsn16->reg_or_const1, pinsn16->regno_or_val1,
		         pinsn16->op_type1, pinsn16->op_pos1);
  if (pinsn16->nr_inserts == 2)
    insert_reg_or_const (pinsn16->reg_or_const2, pinsn16->regno_or_val2,
			 pinsn16->op_type2, pinsn16->op_pos2);

  pop = pinsn16->new_code;
  if (show_internals)
    printf ("*** %s -> %s-%s (EXP)\n", name, pop->name, pop->args);
#if EXT_LISTING
  if (listing & LISTING_LISTING)
    {
      snprintf (EXT_BUF, _("Expanding %s -> %s-%s"),
      		name, pop->name, pop->args);
      ASM_NOTICE ();
    }
#endif /* EXT_LISTING  */

  return pop;
}

/* Delete operand NR from the_insn.  */

static void
delete_operand (nr)
     int nr;
{
  int new, old;

  --the_insn.nops;
  for (new = nr, old = nr + 1; new < the_insn.nops; ++new, ++old)
    {
      the_insn.prefix[new] = the_insn.prefix[old];
      the_insn.opexp[new] = the_insn.opexp[old];
      the_insn.ops[new] = the_insn.ops[old];
      the_insn.regs[new] = the_insn.regs[old];
      the_insn.matches_v[new] = the_insn.matches_v[old];
      the_insn.matches_6[new] = the_insn.matches_6[old];
      the_insn.matches_k[new] = the_insn.matches_k[old];
      the_insn.pcrel[new] = the_insn.pcrel[old];
      the_insn.is_odd[new] = the_insn.is_odd[old];
      the_insn.bitpos[new] = the_insn.bitpos[old];
    }
}

/* Try to optimize the_insn, i.e. try to convert it to a 16-bit
   insn.  POP points to the 32-bit opcode that was found by
   find_opcode, so we know that the_insn contains a valid insn.  */

static opcode_t *
optimize_insn (pop)
     opcode_t *pop;
{
  insn32_t *pinsn32;
  conv_t *palt;
  int i, del_opnr;
  char name[30];

  sprintf (name, "%s-%s", pop->name, pop->args);
  pinsn32 = (insn32_t *) str_hash_find (hash_insn32, name);
  if (pinsn32 == NULL)
    return pop;

  for (i = 0; i < pinsn32->nr_alt; ++i)
    {
      palt = &pinsn32->alt[i];
      del_opnr = -1;

      switch (palt->test)
        {
	case 1:
	  if (the_insn.regs[0] == the_insn.regs[1])
	    del_opnr = 0;
	  break;

	case 2:
	  if ((the_insn.regs[0] == the_insn.regs[1])
	      && (the_insn.ops[2] != 'U')
	      && strchr (pseudo_codes['4'], the_insn.ops[2]))
	    del_opnr = 0;
	  break;

	case 3:
	  if ((the_insn.regs[0] == 15) && (the_insn.regs[1] == 15)
	      && (the_insn.ops[2] != 'U')
	      && strchr (pseudo_codes['8'], the_insn.ops[2]))
	    del_opnr = 0;
	  break;

	case 4:
	  if ((the_insn.regs[0] == the_insn.regs[2])
	      && (the_insn.regs[1] == 15))
	    del_opnr = 2;
	  break;

	case 5:
	  if ((the_insn.regs[0] == the_insn.regs[2])
	      && (the_insn.regs[1] == 15)
	      && (the_insn.ops[3] != 'U')
	      && strchr (pseudo_codes['4'], the_insn.ops[3]))
	    del_opnr = 2;
	  break;

	case 6:
	  if ((the_insn.regs[0] == 15)
	      && ((the_insn.ops[1] != 'U') && (VAL (1) == 0))
	      && strchr (pseudo_codes['R'], the_insn.ops[2]))
	    del_opnr = 1;
	  break;

	case 7:
	  if (((the_insn.ops[1] != 'U') && (VAL (1) == 0))
	      && strchr (pseudo_codes['m'], the_insn.ops[2]))
	    del_opnr = 1;
	  break;

	case 8:
	  if (((the_insn.ops[1] != 'U') && (VAL (1) == 1))
	      && strchr (pseudo_codes['m'], the_insn.ops[2]))
	    del_opnr = 1;
	  break;

	case 9:
	  if ((the_insn.ops[2] != 'U') && (VAL (2) == 4))
	    del_opnr = 2;
	  break;

	case 10:
	  if ((the_insn.ops[2] != 'U') && (VAL (2) == 0))
	    del_opnr = 2;
	  break;

	case 11:
	  if ((the_insn.ops[2] != 'U') && (VAL (2) == 1))
	    del_opnr = 2;
	  break;

	case 12:
	  if ((the_insn.ops[2] != 'U') && (VAL (2) == 2))
	    del_opnr = 2;
	  break;

	case 13:
	  if ((the_insn.ops[1] != 'U') && (VAL (1) == 0))
	    del_opnr = 1;
	  break;

	case 14:
	  if ((the_insn.regs[0] == SP_REGNUM)
	      && (the_insn.regs[1] == SP_REGNUM)
	      && (the_insn.ops[2] != 'U')
	      && ((VAL (2) >= -255) && (VAL (2) <= 0)))
	    {
	      VAL (2) = -VAL (2);
	      del_opnr = 1;
	    }
	  break;

	case 15:
	  if ((the_insn.regs[0] == the_insn.regs[1])
	      && ((the_insn.ops[2] != 'U') && (VAL (2) == 0)))
	    {
	      delete_operand (1);
	      del_opnr = 2;
	    }
	  break;

	case 16:
	  if ((the_insn.regs[0] == the_insn.regs[3])
	      && (the_insn.regs[1] == 15))
	    del_opnr = 3;
	  break;

	case 17:
	  if ((the_insn.ops[1] != 'U') && (VAL (1) == 4))
	    del_opnr = 1;
	  break;

	case 18:
	  if ((the_insn.ops[1] != 'U') && (VAL (1) == 0))
	    del_opnr = 1;
	  break;

	case 19:
	  if ((the_insn.ops[1] != 'U') && (VAL (1) == 1))
	    del_opnr = 1;
	  break;

	case 20:
	  if ((the_insn.ops[1] != 'U') && (VAL (1) == 2))
	    del_opnr = 1;
	  break;

	case 21:
	  if ((the_insn.ops[1] != 'U')
	      && (VAL (1) >= 0) && (VAL (1) < 16))
	    {
	      /* No need to delete an operand, just set new opcode.  */
	      pop = palt->new_code;
	      if (show_internals)
	        printf ("*** %s -> %s-%s (OPT)\n", name, pop->name, pop->args);
#if EXT_LISTING
	      if (listing & LISTING_LISTING)
	        {
		  snprintf (EXT_BUF, _("Optimizing %s -> %s-%s"),
		  	    name, pop->name, pop->args);
		  ASM_NOTICE ();
		}
#endif /* EXT_LISTING  */
	    }
	  break;

	case 22:
	  if ((the_insn.regs[0] == the_insn.regs[1])
	      && (the_insn.ops[2] != 'U')
	      && (VAL (2) >= -8) && (VAL (2) < 8))
	    del_opnr = 1;
	  break;

	default:
	  as_fatal (_("Internal error: illegal test code %d"), palt->test);
	  break;
	}

      if (del_opnr >= 0)
        {
          delete_operand (del_opnr);
	  pop = palt->new_code;
	  if (show_internals)
	    printf ("*** %s -> %s-%s (OPT)\n", name, pop->name, pop->args);
#if EXT_LISTING
	  if (listing & LISTING_LISTING)
	    {
	      snprintf (EXT_BUF, _("Optimizing %s -> %s-%s"),
		  	name, pop->name, pop->args);
	      ASM_NOTICE ();
	    }
#endif /* EXT_LISTING  */
	  break;
	}
    }

  return pop;
}

/* This is called before the (preliminary) opcode is determined for
   the_insn.  We use this to perform special 16-bit jump optimizations.  */

static void
optimize_16bit_jumps ()
{
  opcode_t *pop;

  if ((the_insn.code->name[0] != 'j')
      || (the_insn.nops != 3)
      || (the_insn.ops[0] != 'i')
      || (the_insn.ops[1] != '1')
      || (VAL (1) != 0)
      || !strchr (pseudo_codes['R'], the_insn.ops[2]))
    return;

  if (!strcmp (the_insn.code->name, "jeq"))
    /* jeq %d15,0,m -> jz %d15,R  */
    pop = (opcode_t *) str_hash_find (hash_ops, "jz");
  else if (!strcmp (the_insn.code->name, "jne"))
    /* jne %d15,0,m -> jnz %d15,R  */
    pop = (opcode_t *) str_hash_find (hash_ops, "jnz");
  else
    return;

  if (pop == NULL)
    return; /* Famous last words: "Can't happen"...  */

  if (show_internals)
    {
      const char *no = the_insn.code->name, *nn = pop->name;
      const char *ao = the_insn.code->args, *an = pop->args;

      printf ("*** %s-%s -> %s-%s (OPT)\n", no, ao, nn, an);
    }
#if EXT_LISTING
  if (listing & LISTING_LISTING)
    {
      snprintf (EXT_BUF, _("Optimizing %s-%s -> %s-%s"),
	  	the_insn.code->name, the_insn.code->args,
		pop->name, pop->args);
      ASM_NOTICE ();
    }
#endif /* EXT_LISTING  */

  delete_operand (1);
  the_insn.code = pop;
}

/* Find an opcode that matches the_insn's opcode/operands.  */

static opcode_t *
find_opcode ()
{
  opcode_t *pop;
  int i, op, nops;

  if (the_insn.needs_prefix || big_insns_only)
    use_insn32 = 1;
  else if (big_insns)
    use_insn32 = !use_insn16;

  if (!use_insn32 && optimize_next)
    optimize_16bit_jumps ();

  pop = the_insn.code;
  nops = the_insn.nops;

  for (i = the_insn.code->insind; i <= the_insn.code->inslast; ++pop, ++i)
    {
      if ((pop->nr_operands != nops)
          || !MATCHES_ISA (pop->isa)
          || (use_insn32 && !pop->len32)
          || (use_insn16 && pop->len32))
        continue;

      for (op = 0; op < nops; ++op)
        {
          if (!strchr (pseudo_codes[(int) pop->args[op]], the_insn.ops[op])
              || ((pop->args[op] == 'v') && !the_insn.matches_v[op])
              || ((pop->args[op] == '6') && !the_insn.matches_6[op])
	      || ((pop->args[op] == 'k') && !the_insn.matches_k[op]))
            break;

	  /* Unless explicitly requested by a .code16 pseudo-opcode,
	     don't accept a 16-bit insn if a non-PC-relative operand
	     is unknown.  This reduces the relaxation pass to 16-bit
	     PC-relative insns, which is a Good Thing[tm].  */
	  if (!pop->len32 && (the_insn.ops[op] == 'U'))
	    if (!use_insn16 && !strchr ("mxrRoO", pop->args[op]))
	      break;
	}

      if (op == nops)
	{
	  if (!optimize_next || use_insn32 || !pop->len32)
	    return pop;
	  else
	    return optimize_insn (pop);
	}
    }

  /* We couldn't find a valid opcode so far, which could mean that
     the user requested a 32-bit version of a 16-bit-only opcode,
     or that a valid 16-bit insn has an unknown, non-PC-relative
     operand.  If so, convert it to a 32-bit insn, or fail otherwise.  */

  return insn16_to_insn32 ();
}

/* Print the contents of the_insn.  Used for debugging.  */

static void
print_the_insn ()
{
  int i;

  printf ("%lx ", the_insn.opc_address);
  printf ("<%s ", the_insn.code->name);
  i = the_insn.code->len32 ? 32 : 16;
  printf ("%s%d ", fmt_name[the_insn.code->format], i);
  printf ("\"%s\"> ", the_insn.code->args);
  if (the_insn.code->len32)
    printf ("%08lx ", the_insn.opcode);
  else
    printf ("%04lx ", the_insn.opcode);
  for (i = 0; i < the_insn.nops; ++i)
    {
      switch (the_insn.prefix[i])
        {
	case PREFIX_NONE:
	  if (the_insn.bitpos[i])
	    printf ("bpos:");
	  break;

	case PREFIX_HI:
	  printf ("hi:");
	  break;

	case PREFIX_LO:
	  printf ("lo:");
	  break;

	case PREFIX_SM:
	  printf ("sm:");
	  break;

	case PREFIX_UP:
	  printf ("up:");
	  break;

	case PREFIX_GOT:
	  printf ("got:");
	  break;

	case PREFIX_GOTHI:
	  printf ("gothi:");
	  break;

	case PREFIX_GOTLO:
	  printf ("gotlo:");
	  break;

	case PREFIX_GOTUP:
	  printf ("gotup:");
	  break;

	case PREFIX_GOTOFF:
	  printf ("gotoff:");
	  break;

	case PREFIX_GOTOFFHI:
	  printf ("gotoffhi:");
	  break;

	case PREFIX_GOTOFFLO:
	  printf ("gotofflo:");
	  break;

	case PREFIX_GOTOFFUP:
	  printf ("gotoffup:");
	  break;

	case PREFIX_GOTPC:
	  printf ("gotpc:");
	  break;

	case PREFIX_GOTPCHI:
	  printf ("gotpchi:");
	  break;

	case PREFIX_GOTPCLO:
	  printf ("gotpclo:");
	  break;

	case PREFIX_GOTPCUP:
	  printf ("gotpcup:");
	  break;

	case PREFIX_PLT:
	  printf ("plt:");
	  break;

	case PREFIX_BITPOS:
	  /* Already handled in PREFIX_NONE.  */
	  break;
	}

      switch (the_insn.ops[i])
        {
	case 'd':
	  printf ("d%d", the_insn.regs[i]);
	  break;

	case 'g':
	  printf ("d%dl", the_insn.regs[i]);
	  break;

	case 'G':
	  printf ("d%du", the_insn.regs[i]);
	  break;

	case '-':
	  printf ("d%dll", the_insn.regs[i]);
	  break;

	case '+':
	  printf ("d%duu", the_insn.regs[i]);
	  break;

	case 'l':
	  printf ("d%dlu", the_insn.regs[i]);
	  break;

	case 'L':
	  printf ("d%dul", the_insn.regs[i]);
	  break;

	case 'D':
	  printf ("E%d", the_insn.regs[i]);
	  break;

	case 'i':
	  printf ("d15");
	  break;

	case 'a':
	  printf ("a%d", the_insn.regs[i]);
	  break;

	case 'A':
	  printf ("A%d", the_insn.regs[i]);
	  break;

	case 'I':
	  printf ("a15");
	  break;

	case 'P':
	  printf ("sp");
	  break;

	case 'c':
	  printf ("core reg");
	  break;

	case '1':
	case '2':
	case '3':
	case 'f':
	case '5':
	case 'v':
	case '6':
	case '8':
	case 'n':
	case 'k':
	case 'W':
	case 'm':
	case 'x':
	case 'M':
	case 't':
	case 'T':
	  printf ("%lu=0x%08lx", VAL (i), VAL (i));
	  break;

	case '4':
	case 'F':
	case '9':
	case '0':
	case 'w':
	case 'r':
	case 'R':
	case 'o':
	case 'O':
	  printf ("%ld=0x%08lx", VAL (i), VAL (i));
	  break;

	case 'U':
	  printf ("symbol(%s)", S_GET_NAME (the_insn.opexp[i].X_add_symbol));
	  break;

	case '@':
	  printf ("[a%d]", the_insn.regs[i]);
	  break;

	case '&':
	  printf ("[sp]");
	  break;

	case '<':
	  printf ("[+a%d]", the_insn.regs[i]);
	  break;

	case '>':
	  printf ("[a%d+]", the_insn.regs[i]);
	  break;

	case '*':
	  printf ("[a%d+c]", the_insn.regs[i]);
	  break;

	case '#':
	  printf ("[a%d+r]", the_insn.regs[i]);
	  break;

	case '?':
	  printf ("[a%d+i]", the_insn.regs[i]);
	  break;

	case 'S':
	  printf ("[a15]");
	  break;
	}

      printf ("(%c)", the_insn.ops[i]);
      if (the_insn.pcrel[i])
        printf("(PCREL)");
      if (i < (the_insn.nops - 1))
        printf (", ");
    }
  printf ("\n");
}

/* Produce binary code for the_insn according to its format.
   There are almost 40 instruction formats (each with up to
   five operand fields), and each one has its own encoding
   function.  Operation "Code Desert" starts here.  :-)  */

/* Encode 18-bit absolute addresses (ABS and ABSB formats).  */

static int
insert_abs18 (code, i)
     unsigned long *code;
     int i;
{
  unsigned long val = VAL (i);

  if (val & 0x0fffc000)
    {
      as_bad (_("Illegal 18-bit absolute address 0x%08lx"), val);
      return 0;
    }
  *code |= ((val & 0x3f) << 16);
  *code |= ((val & 0x3c0) << 22);
  *code |= ((val & 0x3c00) << 12);
  *code |= ((val & 0xf0000000) >> 16);
  return 1;
}

static void
encode_abs ()
{
  int i;
  enum bfd_reloc_code_real reloc;
  unsigned long *code = &the_insn.opcode;

  for (i = 0; i < the_insn.nops; ++i)
    {
      if (the_insn.ops[i] == 'U')
        continue;

      switch (the_insn.code->fields[i])
        {
	case FMT_ABS_NONE:
	  continue;

	case FMT_ABS_OFF18:
	  CHECK_RELOC (i);
	  if (!insert_abs18 (code, i))
	    return;
	  break;

	case FMT_ABS_S1_D:
	  *code |= (the_insn.regs[i] << 8);
	  break;

	default:
	  as_fatal (_("Illegal insn field '%c'"), the_insn.code->fields[i]);
	}
    }
}

static void
encode_absb ()
{
  int i;
  enum bfd_reloc_code_real reloc;
  unsigned long *code = &the_insn.opcode;

  for (i = 0; i < the_insn.nops; ++i)
    {
      if (the_insn.ops[i] == 'U')
        continue;

      switch (the_insn.code->fields[i])
        {
	case FMT_ABSB_NONE:
	  continue;
	
	case FMT_ABSB_OFF18:
	  CHECK_RELOC (i);
	  if (!insert_abs18 (code, i))
	    return;
	  break;
	
	case FMT_ABSB_B:
	  CHECK_RELOC (i);
	  *code |= ((VAL (i) & 1) << 11);
	  break;

	case FMT_ABSB_BPOS3:
	  CHECK_RELOC (i);
	  *code |= ((VAL (i) & 7) << 8);
	  break;
	
	default:
	  as_fatal (_("Illegal insn field '%c'"), the_insn.code->fields[i]);
	}
    }
}

static void
encode_b ()
{
  int i;
  enum bfd_reloc_code_real reloc;
  unsigned long *code = &the_insn.opcode;
  unsigned long val;

  for (i = 0; i < the_insn.nops; ++i)
    {
      if (the_insn.ops[i] == 'U')
        continue;

      switch (the_insn.code->fields[i])
        {
	case FMT_B_NONE:
	  continue;
	
	case FMT_B_DISP24:
	  CHECK_RELOC (i);
	  val = VAL (i);
	  if (val & 1)
	    {
	      as_bad (_("Offset or absolute address 0x%lx is not even"), val);
	      return;
	    }
	  if (the_insn.code->args[i] == 'O')
	    val >>= 1;
	  else if (the_insn.code->args[i] == 'T')
	    {
	      if (val & 0x0fe00001)
	        {
		  as_bad (_("Illegal absolute address 0x%lx"), val);
		  return;
		}
	      val >>= 1;
	      val |= ((val & 0x78000000) >> 7);
	    }
	  *code |= ((val & 0xffff) << 16);
	  *code |= ((val & 0xff0000) >> 8);
	  break;
	
	default:
	  as_fatal (_("Illegal insn field '%c'"), the_insn.code->fields[i]);
	}
    }
}

static void
encode_bit ()
{
  int i;
  enum bfd_reloc_code_real reloc;
  unsigned long *code = &the_insn.opcode;

  for (i = 0; i < the_insn.nops; ++i)
    {
      if (the_insn.ops[i] == 'U')
        continue;

      switch (the_insn.code->fields[i])
        {
	case FMT_BIT_NONE:
	  continue;
	
	case FMT_BIT_D:
	  *code |= (the_insn.regs[i] << 28);
	  break;
	
	case FMT_BIT_P2:
	  CHECK_RELOC (i);
	  *code |= ((VAL (i) & 0x1f) << 23);
	  break;

	case FMT_BIT_P1:
	  CHECK_RELOC (i);
	  *code |= ((VAL (i) & 0x1f) << 16);
	  break;

	case FMT_BIT_S2:
	  *code |= (the_insn.regs[i] << 12);
	  break;

	case FMT_BIT_S1:
	  *code |= (the_insn.regs[i] << 8);
	  break;
	
	default:
	  as_fatal (_("Illegal insn field '%c'"), the_insn.code->fields[i]);
	}
    }
}

static void
encode_bo ()
{
  int i;
  enum bfd_reloc_code_real reloc;
  unsigned long *code = &the_insn.opcode;

  for (i = 0; i < the_insn.nops; ++i)
    {
      if (the_insn.ops[i] == 'U')
        continue;

      switch (the_insn.code->fields[i])
        {
	case FMT_BO_NONE:
	  continue;
	
	case FMT_BO_OFF10:
	  if (the_insn.prefix[i] != PREFIX_NONE)
	    {
	      as_bad (_("Illegal prefix for absolute offset"));
	      return;
	    }
	  CHECK_RELOC (i);
	  *code |= ((VAL (i) & 0x3f) << 16);
	  *code |= ((VAL (i) & 0x3c0) << 22);
	  break;
	
	case FMT_BO_S2:
	  *code |= (the_insn.regs[i] << 12);
	  break;

	case FMT_BO_S1_D:
	  *code |= (the_insn.regs[i] << 8);
	  break;
	
	default:
	  as_fatal (_("Illegal insn field '%c'"), the_insn.code->fields[i]);
	}
    }
}

static void
encode_bol ()
{
  int i;
  enum bfd_reloc_code_real reloc;
  unsigned long *code = &the_insn.opcode, val = 0;

  for (i = 0; i < the_insn.nops; ++i)
    {
      if (the_insn.ops[i] == 'U')
        continue;

      switch (the_insn.code->fields[i])
        {
	case FMT_BOL_NONE:
	  continue;
	
	case FMT_BOL_OFF16:
	  CHECK_RELOC (i);
	  switch (the_insn.prefix[i])
	    {
	    case PREFIX_NONE:
	      val = VAL (i);
	      break;

	    case PREFIX_LO:
	      val = (VAL (i) & 0xffff);
	      break;

	    default:
	      as_bad (_("Illegal prefix for absolute offset"));
	      return;
	    }
	  *code |= ((val & 0x3f) << 16);
	  *code |= ((val & 0x3c0) << 22);
	  *code |= ((val & 0xfc00) << 12);
	  break;

	case FMT_BOL_S2:
	  *code |= (the_insn.regs[i] << 12);
	  break;

	case FMT_BOL_S1_D:
	  *code |= (the_insn.regs[i] << 8);
	  break;

	default:
	  as_fatal (_("Illegal insn field '%c'"), the_insn.code->fields[i]);
	}
    }
}

static void
encode_brc ()
{
  int i;
  enum bfd_reloc_code_real reloc;
  unsigned long *code = &the_insn.opcode;

  for (i = 0; i < the_insn.nops; ++i)
    {
      if (the_insn.ops[i] == 'U')
        continue;

      switch (the_insn.code->fields[i])
        {
	case FMT_BRC_NONE:
	  continue;
	
	case FMT_BRC_DISP15:
	  CHECK_RELOC (i);
	  if (VAL (i) & 1)
	    {
	      as_bad (_("Offset 0x%lx is not even"), VAL (i));
	      return;
	    }
	  *code |= (((VAL (i) >> 1) & 0x7fff) << 16);
	  break;

	case FMT_BRC_CONST4:
	  CHECK_RELOC (i);
	  *code |= ((VAL (i) & 0xf) << 12);
	  break;

	case FMT_BRC_S1:
	  *code |= (the_insn.regs[i] << 8);
	  break;
	
	default:
	  as_fatal (_("Illegal insn field '%c'"), the_insn.code->fields[i]);
	}
    }
}

static void
encode_brn ()
{
  int i;
  enum bfd_reloc_code_real reloc;
  unsigned long *code = &the_insn.opcode;

  for (i = 0; i < the_insn.nops; ++i)
    {
      if (the_insn.ops[i] == 'U')
        continue;

      switch (the_insn.code->fields[i])
        {
	case FMT_BRN_NONE:
	  continue;
	
	case FMT_BRN_DISP15:
	  CHECK_RELOC (i);
	  if (VAL (i) & 1)
	    {
	      as_bad (_("Offset 0x%lx is not even"), VAL (i));
	      return;
	    }
	  *code |= (((VAL (i) >> 1) & 0x7fff) << 16);
	  break;
	
	case FMT_BRN_N:
	  CHECK_RELOC (i);
	  *code |= ((VAL (i) & 0xf) << 12);
	  *code |= ((VAL (i) & 0x10) << 3);
	  break;

	case FMT_BRN_S1:
	  *code |= (the_insn.regs[i] << 8);
	  break;

	default:
	  as_fatal (_("Illegal insn field '%c'"), the_insn.code->fields[i]);
	}
    }
}

static void
encode_brr ()
{
  int i;
  enum bfd_reloc_code_real reloc;
  unsigned long *code = &the_insn.opcode;

  for (i = 0; i < the_insn.nops; ++i)
    {
      if (the_insn.ops[i] == 'U')
        continue;

      switch (the_insn.code->fields[i])
        {
	case FMT_BRR_NONE:
	  continue;
	
	case FMT_BRR_DISP15:
	  CHECK_RELOC (i);
	  if (VAL (i) & 1)
	    {
	      as_bad (_("Offset 0x%lx is not even"), VAL (i));
	      return;
	    }
	  *code |= (((VAL (i) >> 1) & 0x7fff) << 16);
	  break;
	
	case FMT_BRR_S2:
	  *code |= (the_insn.regs[i] << 12);
	  break;

	case FMT_BRR_S1:
	  *code |= (the_insn.regs[i] << 8);
	  break;

	default:
	  as_fatal (_("Illegal insn field '%c'"), the_insn.code->fields[i]);
	}
    }
}

static void
encode_rc ()
{
  int i;
  enum bfd_reloc_code_real reloc;
  unsigned long *code = &the_insn.opcode;

  for (i = 0; i < the_insn.nops; ++i)
    {
      if (the_insn.ops[i] == 'U')
        continue;

      switch (the_insn.code->fields[i])
        {
	case FMT_RC_NONE:
	  continue;
	
	case FMT_RC_D:
	  *code |= (the_insn.regs[i] << 28);
	  break;

	case FMT_RC_CONST9:
	  CHECK_RELOC (i);
	  *code |= ((VAL (i) & 0x1ff) << 12);
	  break;

	case FMT_RC_S1:
	  *code |= (the_insn.regs[i] << 8);
	  break;
	
	default:
	  as_fatal (_("Illegal insn field '%c'"), the_insn.code->fields[i]);
	}
    }
}

static void
encode_rcpw ()
{
  int i;
  enum bfd_reloc_code_real reloc;
  unsigned long *code = &the_insn.opcode;

  for (i = 0; i < the_insn.nops; ++i)
    {
      if (the_insn.ops[i] == 'U')
        continue;

      switch (the_insn.code->fields[i])
        {
	case FMT_RCPW_NONE:
	  continue;
	
	case FMT_RCPW_D:
	  *code |= (the_insn.regs[i] << 28);
	  break;

	case FMT_RCPW_P:
	  CHECK_RELOC (i);
	  *code |= ((VAL (i) & 0x1f) << 23);
	  break;

	case FMT_RCPW_W:
	  CHECK_RELOC (i);
	  *code |= ((VAL (i) & 0x1f) << 16);
	  break;

	case FMT_RCPW_CONST4:
	  CHECK_RELOC (i);
	  *code |= ((VAL (i) & 0xf) << 12);
	  break;

	case FMT_RCPW_S1:
	  *code |= (the_insn.regs[i] << 8);
	  break;
	
	default:
	  as_fatal (_("Illegal insn field '%c'"), the_insn.code->fields[i]);
	}
    }
}

static void
encode_rcr ()
{
  int i;
  enum bfd_reloc_code_real reloc;
  unsigned long *code = &the_insn.opcode;

  for (i = 0; i < the_insn.nops; ++i)
    {
      if (the_insn.ops[i] == 'U')
        continue;

      switch (the_insn.code->fields[i])
        {
	case FMT_RCR_NONE:
	  continue;
	
	case FMT_RCR_D:
	  *code |= (the_insn.regs[i] << 28);
	  break;

	case FMT_RCR_S3:
	  *code |= (the_insn.regs[i] << 24);
	  break;

	case FMT_RCR_CONST9:
	  CHECK_RELOC (i);
	  *code |= ((VAL (i) & 0x1ff) << 12);
	  break;

	case FMT_RCR_S1:
	  *code |= (the_insn.regs[i] << 8);
	  break;
	
	default:
	  as_fatal (_("Illegal insn field '%c'"), the_insn.code->fields[i]);
	}
    }
}

static void
encode_rcrr ()
{
  int i;
  enum bfd_reloc_code_real reloc;
  unsigned long *code = &the_insn.opcode;

  for (i = 0; i < the_insn.nops; ++i)
    {
      if (the_insn.ops[i] == 'U')
        continue;

      switch (the_insn.code->fields[i])
        {
	case FMT_RCRR_NONE:
	  continue;
	
	case FMT_RCRR_D:
	  *code |= (the_insn.regs[i] << 28);
	  break;

	case FMT_RCRR_S3:
	  *code |= (the_insn.regs[i] << 24);
	  break;

	case FMT_RCRR_CONST4:
	  CHECK_RELOC (i);
	  *code |= ((VAL (i) & 0xf) << 12);
	  break;

	case FMT_RCRR_S1:
	  *code |= (the_insn.regs[i] << 8);
	  break;
	
	default:
	  as_fatal (_("Illegal insn field '%c'"), the_insn.code->fields[i]);
	}
    }
}

static void
encode_rcrw ()
{
  int i;
  enum bfd_reloc_code_real reloc;
  unsigned long *code = &the_insn.opcode;

  for (i = 0; i < the_insn.nops; ++i)
    {
      if (the_insn.ops[i] == 'U')
        continue;

      switch (the_insn.code->fields[i])
        {
	case FMT_RCRW_NONE:
	  continue;
	
	case FMT_RCRW_D:
	  *code |= (the_insn.regs[i] << 28);
	  break;

	case FMT_RCRW_S3:
	  *code |= (the_insn.regs[i] << 24);
	  break;

	case FMT_RCRW_W:
	  CHECK_RELOC (i);
	  *code |= ((VAL (i) & 0x1f) << 16);
	  break;

	case FMT_RCRW_CONST4:
	  CHECK_RELOC (i);
	  *code |= ((VAL (i) & 0xf) << 12);
	  break;

	case FMT_RCRW_S1:
	  *code |= (the_insn.regs[i] << 8);
	  break;

	default:
	  as_fatal (_("Illegal insn field '%c'"), the_insn.code->fields[i]);
	}
    }
}

static void
encode_rlc ()
{
  int i;
  enum bfd_reloc_code_real reloc;
  unsigned long *code = &the_insn.opcode;

  for (i = 0; i < the_insn.nops; ++i)
    {
      if (the_insn.ops[i] == 'U')
        continue;

      switch (the_insn.code->fields[i])
        {
	case FMT_RLC_NONE:
	  continue;
	
	case FMT_RLC_D:
	  *code |= (the_insn.regs[i] << 28);
	  break;

	case FMT_RLC_CONST16:
	  CHECK_RELOC (i);
	  switch (the_insn.prefix[i])
	    {
	    case PREFIX_NONE:
	    case PREFIX_LO:
	      *code |= ((VAL (i) & 0xffff) << 12);
	      break;

	    case PREFIX_HI:  
	      *code |= ((((VAL (i) + 0x8000) >> 16) & 0xffff) << 12);
	      break;

	    case PREFIX_UP:
	      *code |= (((VAL (i) >> 16) & 0xffff) << 12);
	      break;

	    default:  
	      as_bad (_("Illegal prefix for absolute value"));
	      break;
	    }  
	  break;
	
	case FMT_RLC_S1:
	  *code |= (the_insn.regs[i] << 8);
	  break;

	default:
	  as_fatal (_("Illegal insn field '%c'"), the_insn.code->fields[i]);
	}
    }
}

static void
encode_rr ()
{
  int i;
  enum bfd_reloc_code_real reloc;
  unsigned long *code = &the_insn.opcode;

  for (i = 0; i < the_insn.nops; ++i)
    {
      if (the_insn.ops[i] == 'U')
        continue;

      switch (the_insn.code->fields[i])
        {
	case FMT_RR_NONE:
	  continue;
	
	case FMT_RR_D:
	  *code |= (the_insn.regs[i] << 28);
	  break;

	case FMT_RR_N:
	  CHECK_RELOC (i);
	  *code |= ((VAL (i) & 3) << 16);
	  break;

	case FMT_RR_S2:
	  *code |= (the_insn.regs[i] << 12);
	  break;

	case FMT_RR_S1:
	  *code |= (the_insn.regs[i] << 8);
	  break;
	
	default:
	  as_fatal (_("Illegal insn field '%c'"), the_insn.code->fields[i]);
	}
    }
}

static void
encode_rr1 ()
{
  int i;
  enum bfd_reloc_code_real reloc;
  unsigned long *code = &the_insn.opcode;

  for (i = 0; i < the_insn.nops; ++i)
    {
      if (the_insn.ops[i] == 'U')
        continue;

      switch (the_insn.code->fields[i])
        {
	case FMT_RR1_NONE:
	  continue;
	
	case FMT_RR1_D:
	  *code |= (the_insn.regs[i] << 28);
	  break;

	case FMT_RR1_N:
	  CHECK_RELOC (i);
	  *code |= ((VAL (i) & 3) << 16);
	  break;

	case FMT_RR1_S2:
	  *code |= (the_insn.regs[i] << 12);
	  break;

	case FMT_RR1_S1:
	  *code |= (the_insn.regs[i] << 8);
	  break;
	
	default:
	  as_fatal (_("Illegal insn field '%c'"), the_insn.code->fields[i]);
	}
    }
}

static void
encode_rr2 ()
{
  int i;
  enum bfd_reloc_code_real reloc ATTRIBUTE_UNUSED;
  unsigned long *code = &the_insn.opcode;

  for (i = 0; i < the_insn.nops; ++i)
    {
      if (the_insn.ops[i] == 'U')
        continue;

      switch (the_insn.code->fields[i])
        {
	case FMT_RR2_NONE:
	  continue;
	
	case FMT_RR2_D:
	  *code |= (the_insn.regs[i] << 28);
	  break;

	case FMT_RR2_S2:
	  *code |= (the_insn.regs[i] << 12);
	  break;

	case FMT_RR2_S1:
	  *code |= (the_insn.regs[i] << 8);
	  break;
	
	default:
	  as_fatal (_("Illegal insn field '%c'"), the_insn.code->fields[i]);
	}
    }
}

static void
encode_rrpw ()
{
  int i;
  enum bfd_reloc_code_real reloc;
  unsigned long *code = &the_insn.opcode;

  for (i = 0; i < the_insn.nops; ++i)
    {
      if (the_insn.ops[i] == 'U')
        continue;

      switch (the_insn.code->fields[i])
        {
	case FMT_RRPW_NONE:
	  continue;
	
	case FMT_RRPW_D:
	  *code |= (the_insn.regs[i] << 28);
	  break;

	case FMT_RRPW_P:
	  CHECK_RELOC (i);
	  *code |= ((VAL (i) & 0x1f) << 23);
	  break;

	case FMT_RRPW_W:
	  CHECK_RELOC (i);
	  *code |= ((VAL (i) & 0x1f) << 16);
	  break;

	case FMT_RRPW_S2:
	  *code |= (the_insn.regs[i] << 12);
	  break;

	case FMT_RRPW_S1:
	  *code |= (the_insn.regs[i] << 8);
	  break;

	default:
	  as_fatal (_("Illegal insn field '%c'"), the_insn.code->fields[i]);
	}
    }
}

static void
encode_rrr ()
{
  int i;
  enum bfd_reloc_code_real reloc;
  unsigned long *code = &the_insn.opcode;

  for (i = 0; i < the_insn.nops; ++i)
    {
      if (the_insn.ops[i] == 'U')
        continue;

      switch (the_insn.code->fields[i])
        {
	case FMT_RRR_NONE:
	  continue;
	
	case FMT_RRR_D:
	  *code |= (the_insn.regs[i] << 28);
	  break;

	case FMT_RRR_S3:
	  *code |= (the_insn.regs[i] << 24);
	  break;

	case FMT_RRR_N:
	  CHECK_RELOC (i);
	  *code |= ((VAL (i) & 3) << 16);
	  break;

	case FMT_RRR_S2:
	  *code |= (the_insn.regs[i] << 12);
	  break;

	case FMT_RRR_S1:
	  *code |= (the_insn.regs[i] << 8);
	  break;
	
	default:
	  as_fatal (_("Illegal insn field '%c'"), the_insn.code->fields[i]);
	}
    }
}

static void
encode_rrr1 ()
{
  int i;
  enum bfd_reloc_code_real reloc;
  unsigned long *code = &the_insn.opcode;

  for (i = 0; i < the_insn.nops; ++i)
    {
      if (the_insn.ops[i] == 'U')
        continue;

      switch (the_insn.code->fields[i])
        {
	case FMT_RRR1_NONE:
	  continue;
	
	case FMT_RRR1_D:
	  *code |= (the_insn.regs[i] << 28);
	  break;

	case FMT_RRR1_S3:
	  *code |= (the_insn.regs[i] << 24);
	  break;

	case FMT_RRR1_N:
	  CHECK_RELOC (i);
	  *code |= ((VAL (i) & 3) << 16);
	  break;

	case FMT_RRR1_S2:
	  *code |= (the_insn.regs[i] << 12);
	  break;

	case FMT_RRR1_S1:
	  *code |= (the_insn.regs[i] << 8);
	  break;
	
	default:
	  as_fatal (_("Illegal insn field '%c'"), the_insn.code->fields[i]);
	}
    }
}

static void
encode_rrr2 ()
{
  int i;
  unsigned long *code = &the_insn.opcode;

  for (i = 0; i < the_insn.nops; ++i)
    {
      if (the_insn.ops[i] == 'U')
        continue;

      switch (the_insn.code->fields[i])
        {
	case FMT_RRR2_NONE:
	  continue;
	
	case FMT_RRR2_D:
	  *code |= (the_insn.regs[i] << 28);
	  break;

	case FMT_RRR2_S3:
	  *code |= (the_insn.regs[i] << 24);
	  break;

	case FMT_RRR2_S2:
	  *code |= (the_insn.regs[i] << 12);
	  break;

	case FMT_RRR2_S1:
	  *code |= (the_insn.regs[i] << 8);
	  break;

	default:
	  as_fatal (_("Illegal insn field '%c'"), the_insn.code->fields[i]);
	}
    }
}

static void
encode_rrrr ()
{
  int i;
  unsigned long *code = &the_insn.opcode;

  for (i = 0; i < the_insn.nops; ++i)
    {
      if (the_insn.ops[i] == 'U')
        continue;

      switch (the_insn.code->fields[i])
        {
	case FMT_RRRR_NONE:
	  continue;
	
	case FMT_RRRR_D:
	  *code |= (the_insn.regs[i] << 28);
	  break;

	case FMT_RRRR_S3:
	  *code |= (the_insn.regs[i] << 24);
	  break;

	case FMT_RRRR_S2:
	  *code |= (the_insn.regs[i] << 12);
	  break;

	case FMT_RRRR_S1:
	  *code |= (the_insn.regs[i] << 8);
	  break;
	
	default:
	  as_fatal (_("Illegal insn field '%c'"), the_insn.code->fields[i]);
	}
    }
}

static void
encode_rrrw ()
{
  int i;
  enum bfd_reloc_code_real reloc;
  unsigned long *code = &the_insn.opcode;

  for (i = 0; i < the_insn.nops; ++i)
    {
      if (the_insn.ops[i] == 'U')
        continue;

      switch (the_insn.code->fields[i])
        {
	case FMT_RRRW_NONE:
	  continue;
	
	case FMT_RRRW_D:
	  *code |= (the_insn.regs[i] << 28);
	  break;

	case FMT_RRRW_S3:
	  *code |= (the_insn.regs[i] << 24);
	  break;

	case FMT_RRRW_W:
	  CHECK_RELOC (i);
	  *code |= ((VAL (i) & 0x1f) << 16);
	  break;

	case FMT_RRRW_S2:
	  *code |= (the_insn.regs[i] << 12);
	  break;

	case FMT_RRRW_S1:
	  *code |= (the_insn.regs[i] << 8);
	  break;
	
	default:
	  as_fatal (_("Illegal insn field '%c'"), the_insn.code->fields[i]);
	}
    }
}

static void
encode_sys ()
{
  int i;
  unsigned long *code = &the_insn.opcode;

  for (i = 0; i < the_insn.nops; ++i)
    {
      if (the_insn.ops[i] == 'U')
        continue;

      switch (the_insn.code->fields[i])
        {
	case FMT_SYS_NONE:
	  continue;
	
	case FMT_SYS_S1_D:
	  *code |= (the_insn.regs[i] << 8);
	  break;

	default:
	  as_fatal (_("Illegal insn field '%c'"), the_insn.code->fields[i]);
	}
    }
}

static void
encode_sb ()
{
  int i;
  enum bfd_reloc_code_real reloc;
  unsigned long *code = &the_insn.opcode;

  for (i = 0; i < the_insn.nops; ++i)
    {
      if (the_insn.ops[i] == 'U')
        continue;

      switch (the_insn.code->fields[i])
        {
	case FMT_SB_NONE:
	  continue;

	case FMT_SB_DISP8:
	  CHECK_RELOC (i);
	  if (VAL (i) & 1)
	    {
	      as_bad (_("Offset 0x%lx is not even"), VAL (i));
	      return;
	    }
	  *code |= (((VAL (i) >> 1) & 0xff) << 8);
	  break;

	default:
	  as_fatal (_("Illegal insn field '%c'"), the_insn.code->fields[i]);
	}
    }
}

static void
encode_sbc ()
{
  int i;
  enum bfd_reloc_code_real reloc;
  unsigned long *code = &the_insn.opcode;

  for (i = 0; i < the_insn.nops; ++i)
    {
      if (the_insn.ops[i] == 'U')
        continue;

      switch (the_insn.code->fields[i])
        {
	case FMT_SBC_NONE:
	  continue;
	
	case FMT_SBC_CONST4:
	  CHECK_RELOC (i);
	  *code |= ((VAL (i) & 0xf) << 12);
	  break;

	case FMT_SBC_DISP4:
	  CHECK_RELOC (i);
	  if (VAL (i) & 1)
	    {
	      as_bad (_("Offset 0x%lx is not even"), VAL (i));
	      return;
	    }
	  *code |= (((VAL (i) >> 1) & 0xf) << 8);
	  break;
	
	default:
	  as_fatal (_("Illegal insn field '%c'"), the_insn.code->fields[i]);
	}
    }
}

static void
encode_sbr ()
{
  int i;
  enum bfd_reloc_code_real reloc;
  unsigned long *code = &the_insn.opcode;

  for (i = 0; i < the_insn.nops; ++i)
    {
      if (the_insn.ops[i] == 'U')
        continue;

      switch (the_insn.code->fields[i])
        {
	case FMT_SBR_NONE:
	  continue;
	
	case FMT_SBR_S2:
	  *code |= (the_insn.regs[i] << 12);
	  break;
	
	case FMT_SBR_DISP4:
	  CHECK_RELOC (i);
	  if (VAL (i) & 1)
	    {
	      as_bad (_("Offset 0x%lx is not even"), VAL (i));
	      return;
	    }
	  *code |= (((VAL (i) >> 1) & 0xf) << 8);
	  break;
	
	default:
	  as_fatal (_("Illegal insn field '%c'"), the_insn.code->fields[i]);
	}
    }
}

static void
encode_sbrn ()
{
  int i;
  enum bfd_reloc_code_real reloc;
  unsigned long *code = &the_insn.opcode;

  for (i = 0; i < the_insn.nops; ++i)
    {
      if (the_insn.ops[i] == 'U')
        continue;

      switch (the_insn.code->fields[i])
        {
	case FMT_SBRN_NONE:
	  continue;
	
	case FMT_SBRN_N:
	  CHECK_RELOC (i);
	  *code |= ((VAL (i) & 0xf) << 12);
	  if (the_insn.code->args[i] == '5')
	    *code |= ((VAL (i) & 0x10) << 3);
	  break;

	case FMT_SBRN_DISP4:
	  CHECK_RELOC (i);
	  if (VAL (i) & 1)
	    {
	      as_bad (_("Offset 0x%lx is not even"), VAL (i));
	      return;
	    }
	  *code |= (((VAL (i) >> 1) & 0xf) << 8);
	  break;
	
	default:
	  as_fatal (_("Illegal insn field '%c'"), the_insn.code->fields[i]);
	}
    }
}

static void
encode_sc ()
{
  int i;
  enum bfd_reloc_code_real reloc;
  unsigned long *code = &the_insn.opcode;

  for (i = 0; i < the_insn.nops; ++i)
    {
      if (the_insn.ops[i] == 'U')
        continue;

      switch (the_insn.code->fields[i])
        {
	case FMT_SC_NONE:
	  continue;
	
	case FMT_SC_CONST8:
	  CHECK_RELOC (i);
	  if (the_insn.code->args[i] == 'k')
	    *code |= (((VAL (i) >> 2) & 0xff) << 8);
	  else
	    *code |= ((VAL (i) & 0xff) << 8);
	  break;
	
	default:
	  as_fatal (_("Illegal insn field '%c'"), the_insn.code->fields[i]);
	}
    }
}

static void
encode_slr ()
{
  int i;
  unsigned long *code = &the_insn.opcode;

  for (i = 0; i < the_insn.nops; ++i)
    {
      if (the_insn.ops[i] == 'U')
        continue;

      switch (the_insn.code->fields[i])
        {
	case FMT_SLR_NONE:
	  continue;
	
	case FMT_SLR_S2:
	  *code |= (the_insn.regs[i] << 12);
	  break;

	case FMT_SLR_D:
	  *code |= (the_insn.regs[i] << 8);
	  break;
	
	default:
	  as_fatal (_("Illegal insn field '%c'"), the_insn.code->fields[i]);
	}
    }
}

static void
encode_slro ()
{
  int i;
  enum bfd_reloc_code_real reloc;
  unsigned long *code = &the_insn.opcode;

  for (i = 0; i < the_insn.nops; ++i)
    {
      if (the_insn.ops[i] == 'U')
        continue;

      switch (the_insn.code->fields[i])
        {
	case FMT_SLRO_NONE:
	  continue;
	
	case FMT_SLRO_OFF4:
	  CHECK_RELOC (i);
	  if (the_insn.code->args[i] == 'v')
	    {
	      if (VAL (i) & 1)
	        {
	          as_bad (_("Offset %d is not even"), (int) VAL (i));
		  return;
	        }
	      *code |= (((VAL (i) >> 1) & 0xf) << 12);
	    }
	  else if (the_insn.code->args[i] == '6')
	    {
	      if (VAL (i) & 3)
	        {
		  as_bad (_("Offset %d is not a multiple of 4"), (int) VAL (i));
		  return;
		}
	      *code |= (((VAL (i) >> 2) & 0xf) << 12);
	    }
	  else
	    *code |= ((VAL (i) & 0xf) << 12);
	  break;
	
	case FMT_SLRO_D:
	  *code |= (the_insn.regs[i] << 8);
	  break;
	
	default:
	  as_fatal (_("Illegal insn field '%c'"), the_insn.code->fields[i]);
	}
    }
}

static void
encode_sr ()
{
  int i;
  unsigned long *code = &the_insn.opcode;

  for (i = 0; i < the_insn.nops; ++i)
    {
      if (the_insn.ops[i] == 'U')
        continue;

      switch (the_insn.code->fields[i])
        {
	case FMT_SR_NONE:
	  continue;
	
	case FMT_SR_S1_D:
	  *code |= (the_insn.regs[i] << 8);
	  break;
	
	default:
	  as_fatal (_("Illegal insn field '%c'"), the_insn.code->fields[i]);
	}
    }
}

static void
encode_src ()
{
  int i;
  enum bfd_reloc_code_real reloc;
  unsigned long *code = &the_insn.opcode;

  for (i = 0; i < the_insn.nops; ++i)
    {
      if (the_insn.ops[i] == 'U')
        continue;

      switch (the_insn.code->fields[i])
        {
	case FMT_SRC_NONE:
	  continue;
	
	case FMT_SRC_CONST4:
	  CHECK_RELOC (i);
	  *code |= ((VAL (i) & 0xf) << 12);
	  break;

	case FMT_SRC_S1_D:
	  *code |= (the_insn.regs[i] << 8);
	  break;

	default:
	  as_fatal (_("Illegal insn field '%c'"), the_insn.code->fields[i]);
	}
    }
}

static void
encode_sro ()
{
  int i;
  enum bfd_reloc_code_real reloc;
  unsigned long *code = &the_insn.opcode;

  for (i = 0; i < the_insn.nops; ++i)
    {
      if (the_insn.ops[i] == 'U')
        continue;

      switch (the_insn.code->fields[i])
        {
	case FMT_SRO_NONE:
	  continue;
	
	case FMT_SRO_S2:
	  *code |= (the_insn.regs[i] << 12);
	  break;

	case FMT_SRO_OFF4:
	  CHECK_RELOC (i);
	  if (the_insn.code->args[i] == 'v')
	    {
	      if (VAL (i) & 1)
	        {
	          as_bad (_("Offset %d is not even"), (int) VAL (i));
		  return;
	        }
	      *code |= (((VAL (i) >> 1) & 0xf) << 8);
	    }
	  else if (the_insn.code->args[i] == '6')
	    {
	      if (VAL (i) & 3)
	        {
		  as_bad (_("Offset %d is not a multiple of 4"), (int) VAL (i));
		  return;
		}
	      *code |= (((VAL (i) >> 2) & 0xf) << 8);
	    }
	  else
	    *code |= ((VAL (i) & 0xf) << 8);
	  break;
	  
	default:
	  as_fatal (_("Illegal insn field '%c'"), the_insn.code->fields[i]);
	}
    }
}

static void
encode_srr ()
{
  int i;
  unsigned long *code = &the_insn.opcode;

  for (i = 0; i < the_insn.nops; ++i)
    {
      if (the_insn.ops[i] == 'U')
        continue;

      switch (the_insn.code->fields[i])
        {
	case FMT_SRR_NONE:
	  continue;
	
	case FMT_SRR_S2:
	  *code |= (the_insn.regs[i] << 12);
	  break;

	case FMT_SRR_S1_D:
	  *code |= (the_insn.regs[i] << 8);
	  break;
	
	default:
	  as_fatal (_("Illegal insn field '%c'"), the_insn.code->fields[i]);
	}
    }
}

static void
encode_srrs ()
{
  int i;
  enum bfd_reloc_code_real reloc;
  unsigned long *code = &the_insn.opcode;

  for (i = 0; i < the_insn.nops; ++i)
    {
      if (the_insn.ops[i] == 'U')
        continue;

      switch (the_insn.code->fields[i])
        {
	case FMT_SRRS_NONE:
	  continue;
	
	case FMT_SRRS_S2:
	  *code |= (the_insn.regs[i] << 12);
	  break;

	case FMT_SRRS_S1_D:
	  *code |= (the_insn.regs[i] << 8);
	  break;

	case FMT_SRRS_N:
	  CHECK_RELOC (i);
	  *code |= ((VAL (i) & 3) << 6);
	  break;
	
	default:
	  as_fatal (_("Illegal insn field '%c'"), the_insn.code->fields[i]);
	}
    }
}

static void
encode_ssr ()
{
  int i;
  unsigned long *code = &the_insn.opcode;

  for (i = 0; i < the_insn.nops; ++i)
    {
      if (the_insn.ops[i] == 'U')
        continue;

      switch (the_insn.code->fields[i])
        {
	case FMT_SSR_NONE:
	  continue;
	
	case FMT_SSR_S2:
	  *code |= (the_insn.regs[i] << 12);
	  break;

	case FMT_SSR_S1:
	  *code |= (the_insn.regs[i] << 8);
	  break;

	default:
	  as_fatal (_("Illegal insn field '%c'"), the_insn.code->fields[i]);
	}
    }
}

static void
encode_ssro ()
{
  int i;
  enum bfd_reloc_code_real reloc;
  unsigned long *code = &the_insn.opcode;

  for (i = 0; i < the_insn.nops; ++i)
    {
      if (the_insn.ops[i] == 'U')
        continue;

      switch (the_insn.code->fields[i])
        {
	case FMT_SSRO_NONE:
	  continue;
	
	case FMT_SSRO_OFF4:
	  CHECK_RELOC (i);
	  if (the_insn.code->args[i] == 'v')
	    {
	      if (VAL (i) & 1)
	        {
	          as_bad (_("Offset %d is not even"), (int) VAL (i));
		  return;
	        }
	      *code |= (((VAL (i) >> 1) & 0xf) << 12);
	    }
	  else if (the_insn.code->args[i] == '6')
	    {
	      if (VAL (i) & 3)
	        {
		  as_bad (_("Offset %d is not a multiple of 4"), (int) VAL (i));
		  return;
		}
	      *code |= (((VAL (i) >> 2) & 0xf) << 12);
	    }
	  else
	    *code |= ((VAL (i) & 0xf) << 12);
	  break;

	case FMT_SSRO_S1:
	  *code |= (the_insn.regs[i] << 8);
	  break;
	
	default:
	  as_fatal (_("Illegal insn field '%c'"), the_insn.code->fields[i]);
	}
    }
}

/* Return the relocation type for operand OPNR of the_insn.  This
   also catches cases where a prefix was specified, but either the
   operand may not be prefixed at all, or the given prefix is not
   allowed for this particular operand.  */

static enum bfd_reloc_code_real
find_relocation (opnr)
     int opnr;
{
  int pfx_allowed = 0;
  char place = the_insn.code->fields[opnr];
  char type = the_insn.code->args[opnr];
  enum bfd_reloc_code_real reloc = NO_RELOC;

  switch (the_insn.code->format)
    {
    case TRICORE_FMT_ABS:
      if (place == FMT_ABS_OFF18)
        reloc = BFD_RELOC_TRICORE_18ABS;
      break;

    case TRICORE_FMT_ABSB:
      if (place == FMT_ABSB_OFF18)
        reloc = BFD_RELOC_TRICORE_18ABS;
      else if (place == FMT_ABSB_B)
        reloc = BFD_RELOC_TRICORE_1BIT;
      else if (place == FMT_ABSB_BPOS3)
        reloc = BFD_RELOC_TRICORE_3POS;
      break;

    case TRICORE_FMT_B:
      if (place == FMT_B_DISP24)
        {
          if (type == 'T')
            reloc = BFD_RELOC_TRICORE_24ABS;
          else if (type == 'O')
	    {
	      if (the_insn.prefix[opnr] == PREFIX_PLT)
                {
	          pfx_allowed = 1;
	          reloc = BFD_RELOC_TRICORE_PLT;
	        }	
	      else if (the_insn.is_call)
	        reloc = BFD_RELOC_TRICORE_24CALL;
	      else
                reloc = BFD_RELOC_TRICORE_24REL;
	    }
	}
      break;

    case TRICORE_FMT_BIT:
      if (place == FMT_BIT_P1)
        reloc = BFD_RELOC_TRICORE_5POS;
      else if (place == FMT_BIT_P2)
        reloc = BFD_RELOC_TRICORE_5POS2;
      break;

    case TRICORE_FMT_BO:
      if (place == FMT_BO_OFF10)
        {
          if (the_insn.prefix[opnr] == PREFIX_SM)
            {
	      pfx_allowed = 1;
	      reloc = BFD_RELOC_TRICORE_10SM;
	    }  
	  else  
	    reloc = BFD_RELOC_TRICORE_10OFF;
        }
      break;

    case TRICORE_FMT_BOL:
      if (place == FMT_BOL_OFF16)
	{
	  pfx_allowed = 1;
          switch (the_insn.prefix[opnr])
	    {
	    case PREFIX_SM:
	      reloc = BFD_RELOC_TRICORE_16SM;
	      break;

	    case PREFIX_LO:
	      reloc = BFD_RELOC_TRICORE_LO2;
	      break;

	    case PREFIX_GOT:
	      reloc = BFD_RELOC_TRICORE_GOT2;
	      break;

	    case PREFIX_GOTLO:
	      reloc = BFD_RELOC_TRICORE_GOTLO2;
	      break;

	    case PREFIX_GOTOFF:
	      reloc = BFD_RELOC_TRICORE_GOTOFF2;
	      break;

	    case PREFIX_GOTOFFLO:
	      reloc = BFD_RELOC_TRICORE_GOTOFFLO2;
	      break;

	    case PREFIX_GOTPC:
	      reloc = BFD_RELOC_TRICORE_GOTPC2;
	      break;

	    case PREFIX_GOTPCLO:
	      reloc = BFD_RELOC_TRICORE_GOTPCLO2;
	      break;

	    default:  
	      reloc = BFD_RELOC_TRICORE_16OFF;
	      pfx_allowed = 0;
	      break;
	    }
	}    
      break;

    case TRICORE_FMT_BRC:
      if (place == FMT_BRC_DISP15)
        reloc = BFD_RELOC_TRICORE_15REL;
      else if (place == FMT_BRC_CONST4)
        {
          if (type == '4')
	    reloc = BFD_RELOC_TRICORE_BRCC;
	  else if (type == 'f')
	    reloc = BFD_RELOC_TRICORE_BRCZ;
        }
      break;

    case TRICORE_FMT_BRN:
      if (place == FMT_BRN_DISP15)
        reloc = BFD_RELOC_TRICORE_15REL;
      else if (place == FMT_BRN_N)
        reloc = BFD_RELOC_TRICORE_BRNN;
      break;

    case TRICORE_FMT_BRR:
      if (place == FMT_BRR_DISP15)
        reloc = BFD_RELOC_TRICORE_15REL;
      break;

    case TRICORE_FMT_RC:
      if (place == FMT_RC_CONST9)
        {
          if (type == '9')
	    reloc = BFD_RELOC_TRICORE_9SCONST;
	  else if (type == 'n')
	    reloc = BFD_RELOC_TRICORE_9ZCONST;
        }
      break;

    case TRICORE_FMT_RCPW:
      if (place == FMT_RCPW_P)
        reloc = BFD_RELOC_TRICORE_5POS2;
      else if (place == FMT_RCPW_W)
        reloc = BFD_RELOC_TRICORE_5POS;
      else if (place == FMT_RCPW_CONST4)
        reloc = BFD_RELOC_TRICORE_BRCZ;
      break;

    case TRICORE_FMT_RCR:
      if (place == FMT_RCR_CONST9)
        {
          if (type == '9')
	    reloc = BFD_RELOC_TRICORE_9SCONST;
	  else if (type == 'n')
	    reloc = BFD_RELOC_TRICORE_9ZCONST;
	}    
      break;

    case TRICORE_FMT_RCRR:
      if (place == FMT_RCRR_CONST4)
        reloc = BFD_RELOC_TRICORE_BRCZ;
      break;

    case TRICORE_FMT_RCRW:
      if (place == FMT_RCRW_W)
        reloc = BFD_RELOC_TRICORE_5POS;
      else if (place == FMT_RCRW_CONST4)
        reloc = BFD_RELOC_TRICORE_BRCZ;
      break;

    case TRICORE_FMT_RLC:
      if (place == FMT_RLC_CONST16)
	{
          pfx_allowed = 1;
	  switch (the_insn.prefix[opnr])
	    {
	    case PREFIX_HI:
	      reloc = BFD_RELOC_TRICORE_HIADJ;
	      break;

	    case PREFIX_LO:
	      reloc = BFD_RELOC_TRICORE_LO;
	      break;

	    case PREFIX_SM:
	      reloc = BFD_RELOC_TRICORE_16SM2;
	      break;

	    case PREFIX_UP:
	      reloc = BFD_RELOC_TRICORE_HI;
	      break;

	    case PREFIX_GOT:
	      reloc = BFD_RELOC_TRICORE_GOT;
	      break;

	    case PREFIX_GOTHI:
	      reloc = BFD_RELOC_TRICORE_GOTHI;
	      break;

	    case PREFIX_GOTLO:
	      reloc = BFD_RELOC_TRICORE_GOTLO;
	      break;

	    case PREFIX_GOTUP:
	      reloc = BFD_RELOC_TRICORE_GOTUP;
	      break;

	    case PREFIX_GOTOFF:
	      reloc = BFD_RELOC_TRICORE_GOTOFF;
	      break;

	    case PREFIX_GOTOFFHI:
	      reloc = BFD_RELOC_TRICORE_GOTOFFHI;
	      break;

	    case PREFIX_GOTOFFLO:
	      reloc = BFD_RELOC_TRICORE_GOTOFFLO;
	      break;

	    case PREFIX_GOTOFFUP:
	      reloc = BFD_RELOC_TRICORE_GOTOFFUP;
	      break;

	    case PREFIX_GOTPC:
	      reloc = BFD_RELOC_TRICORE_GOTPC;
	      break;

	    case PREFIX_GOTPCHI:
	      reloc = BFD_RELOC_TRICORE_GOTPCHI;
	      break;

	    case PREFIX_GOTPCLO:
	      reloc = BFD_RELOC_TRICORE_GOTPCLO;
	      break;

	    case PREFIX_GOTPCUP:
	      reloc = BFD_RELOC_TRICORE_GOTPCUP;
	      break;

	    case PREFIX_PLT:
	      as_bad (_("Prefix plt: not allowed here"));
	      break;

	    default:
	      pfx_allowed = 0;
              if (type == 'w')
	        reloc = BFD_RELOC_TRICORE_16CONST;
	      else
	        reloc = BFD_RELOC_TRICORE_LO;
	      break;
	    }  
	}
      break;

    case TRICORE_FMT_RR:
      if (place == FMT_RR_N)
        reloc = BFD_RELOC_TRICORE_RRN;
      break;

    case TRICORE_FMT_RR1:
      if (place == FMT_RR1_N)
        reloc = BFD_RELOC_TRICORE_RRN;
      break;

    case TRICORE_FMT_RR2:
      /* No relocateable fields.  */
      break;

    case TRICORE_FMT_RRPW:
      if (place == FMT_RRPW_P)
        reloc = BFD_RELOC_TRICORE_5POS2;
      else if (place == FMT_RRPW_W)
        reloc = BFD_RELOC_TRICORE_5POS;
      break;

    case TRICORE_FMT_RRR:
      if (place == FMT_RRR_N)
        reloc = BFD_RELOC_TRICORE_RRN;
      break;

    case TRICORE_FMT_RRR1:
      if (place == FMT_RRR1_N)
        reloc = BFD_RELOC_TRICORE_RRN;
      break;

    case TRICORE_FMT_RRR2:
      /* No relocateable fields.  */
      break;

    case TRICORE_FMT_RRRR:
      /* No relocateable fields.  */
      break;

    case TRICORE_FMT_RRRW:
      if (place == FMT_RRRW_W)
        reloc = BFD_RELOC_TRICORE_5POS;
      break;

    case TRICORE_FMT_SYS:
      /* No relocateable fields.  */
      break;

    case TRICORE_FMT_SB:
      if (place == FMT_SB_DISP8)
        reloc = BFD_RELOC_TRICORE_8REL;
      break;

    case TRICORE_FMT_SBC:
      if (place == FMT_SBC_CONST4)
        reloc = BFD_RELOC_TRICORE_4CONST;
      else if (place == FMT_SBC_DISP4)
	{
	  if (type == 'm')
            reloc = BFD_RELOC_TRICORE_4REL;
	  else
	    reloc = BFD_RELOC_TRICORE_5REL;
	}    
      break;

    case TRICORE_FMT_SBR:
      if (place == FMT_SBR_DISP4)
        {
	  if (the_insn.is_loop)
            reloc = BFD_RELOC_TRICORE_4REL2;
	  else if (type == 'm')
            reloc = BFD_RELOC_TRICORE_4REL;
	  else  
            reloc = BFD_RELOC_TRICORE_5REL;
        }
      break;

    case TRICORE_FMT_SBRN:
      if (place == FMT_SBRN_N)
        {
          if (type == 'f')
            reloc = BFD_RELOC_TRICORE_4POS;
	  else
            reloc = BFD_RELOC_TRICORE_5POS3;
	}    
      else if (place == FMT_SBRN_DISP4)
        reloc = BFD_RELOC_TRICORE_4REL;
      break;

    case TRICORE_FMT_SC:
      if (place == FMT_SC_CONST8)
        {
          if (type == 'k')
            reloc = BFD_RELOC_TRICORE_8CONST2;
	  else
            reloc = BFD_RELOC_TRICORE_8CONST;
        }
      break;

    case TRICORE_FMT_SLR:
      /* No relocateable fields.  */
      break;

    case TRICORE_FMT_SLRO:
      if (place == FMT_SLRO_OFF4)
        {
          if (type == 'f')
	    reloc = BFD_RELOC_TRICORE_42OFF;
	  else if (type == 'v')
	    reloc = BFD_RELOC_TRICORE_42OFF2;
	  else if (type == '6')
	    reloc = BFD_RELOC_TRICORE_42OFF4;
	}    
      break;

    case TRICORE_FMT_SR:
      /* No relocateable fields.  */
      break;

    case TRICORE_FMT_SRC:
      if (place == FMT_SRC_CONST4)
        {
          if (type == '4')
	    reloc = BFD_RELOC_TRICORE_4CONST;
	  else if (type == 'f')
	    reloc = BFD_RELOC_TRICORE_4OFF;
        }
      break;

    case TRICORE_FMT_SRO:
      if (place == FMT_SRO_OFF4)
        {
          if (type == 'f')
	    reloc = BFD_RELOC_TRICORE_42OFF;
	  else if (type == 'v')
	    reloc = BFD_RELOC_TRICORE_42OFF2;
	  else if (type == '6')
	    reloc = BFD_RELOC_TRICORE_42OFF4;
	}    
      break;

    case TRICORE_FMT_SRR:
      /* No relocateable fields.  */
      break;

    case TRICORE_FMT_SRRS:
      if (place == FMT_SRRS_N)
        reloc = BFD_RELOC_TRICORE_2OFF;
      break;

    case TRICORE_FMT_SSR:
      /* No relocateable fields.  */
      break;

    case TRICORE_FMT_SSRO:
      if (place == FMT_SSRO_OFF4)
        {
          if (type == 'f')
	    reloc = BFD_RELOC_TRICORE_4OFF;
	  else if (type == 'v')
	    reloc = BFD_RELOC_TRICORE_4OFF2;
	  else if (type == '6')
	    reloc = BFD_RELOC_TRICORE_4OFF4;
	}    
      break;

    default:
      as_fatal (_("Internal error: illegal instruction format %d\n"),
		the_insn.code->format);
    }

  if (reloc == NO_RELOC)
    as_fatal (_("No reloc found for operand %d\n"), opnr);

  if (!pfx_allowed && (the_insn.prefix[opnr] != PREFIX_NONE))
    {
      as_bad (_("Illegal prefix for operand %d\n"), opnr);
      return NO_RELOC;
    }

  return reloc;
}

/* If a TriCore insn could be successfully parsed and encoded
   (hooray!), we finally come here to celebrate the frag hack.
   Party on, insn!  Resistance is futile -- you'll be emitted!  */

static void
emit_code ()
{
  int i, oplen = the_insn.code->len32 ? 4 : 2;
  enum bfd_reloc_code_real reloc;
  char *pfrag;
  int dont_relax, add_bytes; /* Extra k3wl description of this function.  */
  relax_substateT state;

  /* Make sure the current frag is large enough to hold this
     insn, even if it is subject to relaxation.  This is
     necessary because we always call frag_more for the fix
     part and frag_var for the variable part.  frag_var will
     start a new frag if there's not enough room for the
     variable part, so it could happen that the insn's fix
     and variable parts live in different frags.  Uh-oh...  */
  frag_grow (MAX_RELAXED_INSN_SIZE);

  /* Handle CPU.048 workaround, if so requested.  */
  if (workaround_cpu48)
    {
      static segT current_seg = (segT) 0;
      static subsegT current_subseg = (subsegT) 0;
      static int workaround_cpu48_pending = 0;
      static int addressregno = 0;
      static int double_addressreg = 0;

      if (workaround_cpu48_pending)
        {
	  if ((current_seg == now_seg) && (current_subseg == now_subseg))
	    {
	      const char *name = the_insn.code->name;

	      if ((!strcmp (name, "ji")
	           || !strcmp (name, "jli")
		   || !strcmp (name, "calli"))
		  && ((the_insn.regs[0] == addressregno)
		      || (double_addressreg
		          ? (the_insn.regs[0] == (addressregno + 1))
		          : 0)))
		{
		  if (show_internals)
		    printf ("*** Inserting NOP (workaround "
		    	    "for CPU_TC.048 bug).\n");
#if EXT_LISTING
		  ASM_NOTICE_WORKAROUND (_("inserting NOP as a workaround "
		      			   "for bug CPU_TC.048"));
#endif

		  if (use_insn32)
		    {
		      pfrag = frag_more (4);
		      md_number_to_chars (pfrag, 0x0d, 4);
		    }
		  else
		    {
		      pfrag = frag_more (2);
		      md_number_to_chars (pfrag, 0x00, 2);
		    }

		  frag_grow (MAX_RELAXED_INSN_SIZE);
	          workaround_cpu48_pending = 0;
		}
	      else
	        {
	          workaround_cpu48_pending = 0;
		  goto recheck_cpu48;
		}
	    }
	}
      else
        {
	  const char *name;

 recheck_cpu48:
	  name = the_insn.code->name;
	  if ((name[0] == 'l') && (name[1] == 'd') && (name[2] == '.')
	      && (!strcmp (name, "ld.a") || !strcmp (name, "ld.da")))
	    {
	      workaround_cpu48_pending = 1;
	      current_seg = now_seg;
	      current_subseg = now_subseg;
	      addressregno = the_insn.regs[0];
	      double_addressreg = (name[3] == 'd');
	    }
	}
    }

  /* Handle CPU.050 workaround, if so requested.  */
  if (workaround_cpu50)
    {
      static segT current_seg = (segT) 0;
      static subsegT current_subseg = (subsegT) 0;
      static int workaround_cpu50_pending = 0;

      if (workaround_cpu50_pending)
        {
	  if ((current_seg == now_seg) && (current_subseg == now_subseg))
	    {
	      const char *name = the_insn.code->name;

	      if ((name[0] == 'l') && (name[1] == 'd') && (name[2] == '.'))
		{
		  if (show_internals)
		    printf ("*** Inserting NOP (workaround "
		    	    "for CPU_TC.050 bug).\n");
#if EXT_LISTING
		  ASM_NOTICE_WORKAROUND (_("inserting NOP as a workaround "
		      			   "for bug CPU_TC.050"));
#endif

		  if (use_insn32)
		    {
		      pfrag = frag_more (4);
		      md_number_to_chars (pfrag, 0x0d, 4);
		    }
		  else
		    {
		      pfrag = frag_more (2);
		      md_number_to_chars (pfrag, 0x00, 2);
		    }

		  frag_grow (MAX_RELAXED_INSN_SIZE);
	          workaround_cpu50_pending = 0;
		}
	      else
	        {
	          workaround_cpu50_pending = 0;
		  goto recheck_cpu50;
		}
	    }
	}
      else
        {
	  const char *name;

 recheck_cpu50:
	  name = the_insn.code->name;
	  if (((name[0] == 'd') && (name[1] == 'v')
	       && (!strcmp (name, "dvadj")
	           || !strcmp (name, "dvstep")
		   || !strcmp (name, "dvstep.u")))
	      || ((name[0] == 'm') && (name[1] == 'a')
	          && (!strcmp (name, "madd")
		      || !strcmp (name, "madds")
		      || !strcmp (name, "madds.u")))
	      || ((name[0] == 'm') && (name[1] == 's')
	          && (!strcmp (name, "msub")
		      || !strcmp (name, "msubs")
		      || !strcmp (name, "msubs.u")))
	      || ((name[0] == 'm') && (name[1] == 'u')
	          && (!strcmp (name, "mul")
		      || !strcmp (name, "muls")
		      || !strcmp (name, "muls.u"))))
	    {
	      workaround_cpu50_pending = 1;
	      current_seg = now_seg;
	      current_subseg = now_subseg;
	    }
	}
    }

  /* Handle CPU.060 workaround, if so requested.  */
  if (workaround_cpu60)
    {
      static segT current_seg = (segT) 0;
      static subsegT current_subseg = (subsegT) 0;
      static int workaround_cpu60_pending = 0;
      static int addressregno = 0;
      static int double_addressreg = 0;

      if (workaround_cpu60_pending)
        {
	  if ((current_seg == now_seg) && (current_subseg == now_subseg))
	    {
	      const char *name = the_insn.code->name;

	      if ((name[0] == 'l') && (name[1] == 'd') && (name[2] == '.')
	          && ((name[3] == 'd') || (name[3] == 'w'))
		  && (name[4] == '\0')
		  && (the_insn.nops >= 2)
		  && (the_insn.code->args[1] != 't')
		  && ((the_insn.regs[1] == addressregno)
		      || (double_addressreg
		          ? (the_insn.regs[1] == (addressregno + 1))
		          : 0)))
		{
		  if (show_internals)
		    printf ("*** Inserting NOP (workaround "
		    	    "for CPU_TC.060 bug).\n");
#if EXT_LISTING
		  ASM_NOTICE_WORKAROUND (_("inserting NOP as a workaround "
		      			   "for bug CPU_TC.060"));
#endif
		  if (use_insn32)
		    {
		      pfrag = frag_more (4);
		      md_number_to_chars (pfrag, 0x0d, 4);
		    }
		  else
		    {
		      pfrag = frag_more (2);
		      md_number_to_chars (pfrag, 0x00, 2);
		    }

		  frag_grow (MAX_RELAXED_INSN_SIZE);
	          workaround_cpu60_pending = 0;
		}
	      else
	        {
	          workaround_cpu60_pending = 0;
		  goto recheck_cpu60;
		}
	    }
	}
      else
        {
	  const char *name;

 recheck_cpu60:
	  name = the_insn.code->name;
	  if ((name[0] == 'l') && (name[1] == 'd') && (name[2] == '.')
	      && (!strcmp (name, "ld.a") || !strcmp (name, "ld.da")))
	    {
	      workaround_cpu60_pending = 1;
	      current_seg = now_seg;
	      current_subseg = now_subseg;
	      addressregno = the_insn.regs[0];
	      double_addressreg = (name[3] == 'd');
	    }
	}
    }

  /* Handle CPU.070 workaround, if so requested.  */
  if (workaround_cpu70)
    {
      static segT current_seg = (segT) 0;
      static subsegT current_subseg = (subsegT) 0;
      static int workaround_cpu70_pending = 0;
      static int addressreg = 0;

      if (workaround_cpu70_pending)
        {
	  if ((current_seg == now_seg) && (current_subseg == now_subseg))
	    {
	      const char *name = the_insn.code->name;

	      if (!strncmp (name, "loop", 4))
	        {
		  if (show_internals)
		    printf ("*** Inserting %s (workaround "
		    	    "for CPU_TC.070 bug).\n",
			    addressreg ? "NOP" : "2 NOPs");
#if EXT_LISTING
		  ASM_NOTICE_WORKAROUND (_("inserting NOP as a workaround "
		      			   "for bug CPU_TC.070"));
#endif

		  if (use_insn32)
		    {
		      if (addressreg)
		        {
			  pfrag = frag_more (4);
		          md_number_to_chars (pfrag, 0x0d, 4);
			}
		      else
		        {
			  pfrag = frag_more (8);
		          md_number_to_chars (pfrag, 0x0d, 4);
		          md_number_to_chars (pfrag + 4, 0x0d, 4);
			}
		    }
		  else
		    {
		      pfrag = frag_more (addressreg ? 2 : 4);
		      md_number_to_chars (pfrag, 0x00, addressreg ? 2 : 4);
		    }

		  frag_grow (MAX_RELAXED_INSN_SIZE);
	          workaround_cpu70_pending = 0;
		}
	      else
	        {
	          workaround_cpu70_pending = 0;
		  goto recheck_cpu70;
		}
	    }
	}
      else
        {
	  const char *name;

 recheck_cpu70:
	  name = the_insn.code->name;
          if ((name[0] == 'j') && (name[1] != '\0')
	      && (the_insn.code->nr_operands > 1)
	      && (strpbrk (the_insn.code->args, "mxrRoOMtT") != NULL))
	    {
	      size_t ilen = strlen (name);

	      workaround_cpu70_pending = 1;
	      current_seg = now_seg;
	      current_subseg = now_subseg;
	      addressreg = ((ilen > 2)
	      		    && (name[ilen - 2] == '.')
			    && (name[ilen - 1] == 'a'));
	    }
	}
    }
  /* Handle CPU.072 workaround, if so requested.  */
  if (workaround_cpu72)
    {
      static segT current_seg = (segT) 0;
      static subsegT current_subseg = (subsegT) 0;
      static int workaround_cpu72_pending = 0;
      static int addressregno = 0;
      static int double_addressreg = 0;

      if (workaround_cpu72_pending)
        {
	  if ((current_seg == now_seg) && (current_subseg == now_subseg))
	    {
	      if (the_insn.is_loop
		  && ((the_insn.regs[0] == addressregno)
		      || (double_addressreg
		          ? (the_insn.regs[0] == (addressregno + 1))
		          : 0)))
		{
		  if (show_internals)
		    printf ("*** Inserting NOP (workaround "
		    	    "for CPU_TC.072 bug).\n");
#if EXT_LISTING
		  ASM_NOTICE_WORKAROUND (_("inserting NOP as a workaround "
		      			   "for bug CPU_TC.072"));
#endif

		  if (use_insn32)
		    {
		      pfrag = frag_more (4);
		      md_number_to_chars (pfrag, 0x0d, 4);
		    }
		  else
		    {
		      pfrag = frag_more (2);
		      md_number_to_chars (pfrag, 0x00, 2);
		    }

		  frag_grow (MAX_RELAXED_INSN_SIZE);
	          workaround_cpu72_pending = 0;
		}
	      else
	        {
	          workaround_cpu72_pending = 0;
		  goto recheck_cpu72;
		}
	    }
	}
      else
        {
	  const char *name;

 recheck_cpu72:
	  name = the_insn.code->name;
	  if ((name[0] == 'l') && (name[1] == 'd') && (name[2] == '.')
	      && (!strcmp (name, "ld.a") || !strcmp (name, "ld.da")))
	    {
	      workaround_cpu72_pending = 1;
	      current_seg = now_seg;
	      current_subseg = now_subseg;
	      addressregno = the_insn.regs[0];
	      double_addressreg = (name[3] == 'd');
	    }
	}
    }

  /* Handle CPU.082 workaround, if so requested.  */
  if (workaround_cpu82)
    {
      static segT current_seg = (segT) 0;
      static subsegT current_subseg = (subsegT) 0;
      static int workaround_cpu82_pending = 0;

      if (workaround_cpu82_pending)
        {
	  if ((current_seg == now_seg) && (current_subseg == now_subseg))
	    {
	      const char *name = the_insn.code->name;

	      if ((name[0] == 'l') && (name[1] == 'd') && (name[2] == '.'))
		{
		  if (show_internals)
		    printf ("*** Inserting NOP (workaround "
		    	    "for CPU_TC.082 bug).\n");
#if EXT_LISTING
		  ASM_NOTICE_WORKAROUND (_("inserting NOP as a workaround "
		      			   "for bug CPU_TC.082"));
#endif

		  if (use_insn32)
		    {
		      pfrag = frag_more (4);
		      md_number_to_chars (pfrag, 0x0d, 4);
		    }
		  else
		    {
		      pfrag = frag_more (2);
		      md_number_to_chars (pfrag, 0x00, 2);
		    }

		  frag_grow (MAX_RELAXED_INSN_SIZE);
	          workaround_cpu82_pending = 0;
		}
	      else
	        {
	          workaround_cpu82_pending = 0;
		  goto recheck_cpu82;
		}
	    }
	}
      else
        {
	  const char *name;

 recheck_cpu82:
	  name = the_insn.code->name;
	  if ((name[0] == 's') && (name[1] == 't')
	      && (!strcmp (name, "stlcx") || !strcmp (name, "stucx")))
	    {
	      workaround_cpu82_pending = 1;
	      current_seg = now_seg;
	      current_subseg = now_subseg;
	    }
	}
    }

  /* Handle CPU.094 workaround, if so requested.  */
  if (workaround_cpu94)
    {
      static segT current_seg = (segT) 0;
      static subsegT current_subseg = (subsegT) 0;
      static int workaround_cpu94_pending = 0;

      if (workaround_cpu94_pending)
        {
	  if ((current_seg == now_seg) && (current_subseg == now_subseg))
	    {
	      const char *name = the_insn.code->name;

	      if (!strncmp (name, "call", 4)
			|| !strncmp(name,"syscall",7)
			|| !strncmp(name,"ret",3)
			|| !strncmp(name,"rfe",3))
	        {
		  if (show_internals)
		    printf ("*** Inserting %s (workaround "
		    	    "for CPU_TC.094 bug).\n", "NOP");
#if EXT_LISTING
		  ASM_NOTICE_WORKAROUND (_("inserting NOP as a workaround "
		      			   "for bug CPU_TC.094"));
#endif

		  if (use_insn32)
		    {
			  pfrag = frag_more (4);
		          md_number_to_chars (pfrag, 0x0d, 4);
		    }
		  else
		    {
		      pfrag = frag_more (2);
		      md_number_to_chars (pfrag, 0x00, 2);
		    }

		  frag_grow (MAX_RELAXED_INSN_SIZE);
	          workaround_cpu94_pending = 0;
		}
	      else
	        {
	          workaround_cpu94_pending = 0;
		  goto recheck_cpu94;
		}
	    }
	}
      else
        {
	  const char *name;

 recheck_cpu94:
	  name = the_insn.code->name;
          if ((name[0] == 'j') && (name[1] != '\0')
	      && (the_insn.code->nr_operands > 1)
			/* check only IP jump instructions which use data registers */
	      && (strpbrk (the_insn.code->args, "di") != NULL))
	    {
	      // size_t ilen = strlen (name);

	      workaround_cpu94_pending = 1;
	      current_seg = now_seg;
	      current_subseg = now_subseg;
	    }
	}
    }

  /* Handle CPU.095 workaround, if so requested.  */
  if (workaround_cpu95)
    {
      static segT current_seg = (segT) 0;
      static subsegT current_subseg = (subsegT) 0;
      static int workaround_cpu95_pending = 0;

      if (workaround_cpu95_pending)
        {
	  if ((current_seg == now_seg) && (current_subseg == now_subseg))
	    {
	      const char *name = the_insn.code->name;

	      if (!strncmp (name, "addsc", 5)
			|| !strncmp(name,"mov.a",5)
			|| !strncmp(name,"mtcr",4))
	        {
		  if (show_internals)
		    printf ("*** Inserting %s (workaround "
		    	    "for CPU_TC.095 bug).\n", "NOP");
#if EXT_LISTING
		  ASM_NOTICE_WORKAROUND (_("inserting NOP as a workaround "
		      			   "for bug CPU_TC.095"));
#endif

		  if (use_insn32)
		    {
			  pfrag = frag_more (4);
		          md_number_to_chars (pfrag, 0x0d, 4);
		    }
		  else
		    {
		      pfrag = frag_more (2);
		      md_number_to_chars (pfrag, 0x00, 2);
		    }

		  frag_grow (MAX_RELAXED_INSN_SIZE);
	          workaround_cpu95_pending = 0;
		}
	      else
	        {
	          workaround_cpu95_pending = 0;
		  goto recheck_cpu95;
		}
	    }
	}
      else
        {
	  const char *name;

 recheck_cpu95:
	  name = the_insn.code->name;
        if ((name[0] == 's') && (name[1] == 'a') && (name[2] == 't'))
	    {
	      //size_t ilen = strlen (name);

	      workaround_cpu95_pending = 1;
	      current_seg = now_seg;
	      current_subseg = now_subseg;
	    }
	}
    }



  /* Handle DMI.012 workaround, if so requested.  Boy, this one's nasty!  */
  if (workaround_dmi12)
    {
      static segT current_seg = (segT) 0;
      static subsegT current_subseg = (subsegT) 0;
      static int workaround_dmi12_2ab_pending = 0;
      static int workaround_dmi12_2c_pending = 0;
      static int workaround_dmi12_2d_pending = 0;
      static int workaround_dmi12_2e_pending = 0;
      static int addressregno = -1;
      static int double_addressreg = 0;
      static int dataregno = -1;
      static int double_datareg = 0;
#if EXT_LISTING
      static fragS *pending_dmi12_2e_frag = (fragS *) NULL;
#endif

      /* If the current insn is a NOP, it needs special treatment wrt.
         (possibly pending) DMI12 workarounds -- see comment below.  */
      if (!strcmp (the_insn.code->name, "nop")
          && (workaround_dmi12_2ab_pending
	      || workaround_dmi12_2c_pending
	      || workaround_dmi12_2d_pending
	      || workaround_dmi12_2e_pending)
          && ((current_seg == now_seg) && (current_subseg == now_subseg)))
        {
	  /* The insn we're going to emit right now is a NOP, which is a
	     "dual-pipeline" insn, meaning it stalls both, TriCore's integer
	     (I) and load/store (LS) pipelines.  This means we can safely
	     reset all potentially pending DMI12 workarounds, because no
	     instruction sequence affected by this cache bug can be started
	     with a NOP, and even if such a sequence is pending, a NOP insn
	     will immediately "disarm" it.  This is even true for case 2.c
	     where an address register, after being loaded, must not be used
	     within the next two insns to load data from memory, unless the
	     insn following the initial load insn stalls the LS pipeline: if
	     the current NOP is the first insn after the initial load, it'll
	     stall the LS pipeline, so even a possibly following load insn
	     won't trigger the bug; and if this NOP is the second insn after
	     the initial load, it would also terminate the critical sequence
	     by merely not being a potentially "dangerous" load insn.  */

 reset_dmi12:
	  workaround_dmi12_2ab_pending = 0;
	  workaround_dmi12_2c_pending = 0;
	  workaround_dmi12_2d_pending = 0;
	  workaround_dmi12_2e_pending = 0;
	  addressregno = -1;
	  double_addressreg = 0;
	  dataregno = -1;
	  double_datareg = 0;
	  goto recheck_dmi12;
	}

      /* Do we have to insert a NOP instruction due to a previous "store"
         instruction within the same (sub-) section?  */
      if (workaround_dmi12_2e_pending
          && ((current_seg == now_seg) && (current_subseg == now_subseg)))
        {
	  /* Workaround for bug DMI_TC.012, case 2.e, part 1: the previous
	     insn was a "store" instruction, so we have to insert a NOP insn
	     before emitting the current insn.  */
	  if (show_internals)
	    printf ("*** Inserting NOP (workaround for DMI_TC.012 "
	            "bug, case 2.e (\"store\")).\n");
#if EXT_LISTING
	  /* Attach the notice to the frag that belongs to the instruction
	     which started the faulting instruction sequence (see below).  */
          ext_listing_frag = pending_dmi12_2e_frag;
	  ASM_NOTICE_WORKAROUND (_("inserting NOP as a workaround for "
	  			   "bug DMI_TC.012, case 2.e (\"store\")"));
	  ext_listing_frag = pending_dmi12_2e_frag = (fragS *) NULL;
#endif

	  if (use_insn32)
	    {
	      pfrag = frag_more (4);
	      md_number_to_chars (pfrag, 0x0d, 4);
	    }
	  else
	    {
	      pfrag = frag_more (2);
	      md_number_to_chars (pfrag, 0x00, 2);
	    }

	  frag_grow (MAX_RELAXED_INSN_SIZE);

	  /* Now that we've inserted a NOP, we can safely reset all possibly
	     pending DMI12 workarounds (see the comment for "NOP" above for
	     details).  Note, however, that we can't simply jump to the
	     "reset_dmi12" label above, because the current insn might be
	     some "load" instruction -- in which case we must even insert
	     insert another NOP, but this is handled by the code below!  */
	  workaround_dmi12_2ab_pending = 0;
	  workaround_dmi12_2c_pending = 0;
	  workaround_dmi12_2d_pending = 0;
	  workaround_dmi12_2e_pending = 0;
	  addressregno = -1;
	  double_addressreg = 0;
	  dataregno = -1;
	  double_datareg = 0;
	}

      /* It's a shame that there's no easier way to identify the following
         insns, but then again, this is only necessary to implement some
	 workaround, so it's not really okay, but at least acceptable.  */
      if (!strncmp (the_insn.code->name, "ld.", 3)
	  || !strcmp (the_insn.code->name, "call")
	  || !strcmp (the_insn.code->name, "calla")
	  || !strcmp (the_insn.code->name, "calli")
	  || !strcmp (the_insn.code->name, "ret")
	  || !strcmp (the_insn.code->name, "ldlcx")
	  || !strcmp (the_insn.code->name, "lducx")
	  || !strcmp (the_insn.code->name, "ldmst")
	  || !strcmp (the_insn.code->name, "swap.w")
	  || !strcmp (the_insn.code->name, "st.t")
	  || !strcmp (the_insn.code->name, "rfe")
	  || !strcmp (the_insn.code->name, "bisr")
	  || !strcmp (the_insn.code->name, "svlcx")
	  || !strcmp (the_insn.code->name, "rslcx")
	  || !strncmp (the_insn.code->name, "cachea.", 7))
	{
	  /* Workaround for bug DMI_TC.012, case 2.e, part 2: the current
	     insn qualifies as a "load" instruction (according to the
	     DMI12 bug and workaround description, aka. Errata Sheet), so
	     we have to insert a NOP instruction before emitting the current
	     insn.  And after that, we can also safely reset all possibly
	     pending DMI12 workarounds for the reasons already explained
	     above.  Note: case 2.e eclipses case 2.c, but the code for 2.c
	     was already implemented, tested and released at the time case
	     2.e was even reported, so it's still there for documentation
	     purposes, or "just in case"...  ;-)  */
	  if (show_internals)
	    printf ("*** Inserting NOP (workaround for DMI_TC.012 "
	            "bug, case 2.e (\"load\")).\n");
#if EXT_LISTING
	  ASM_NOTICE_WORKAROUND (_("inserting NOP as a workaround for "
	  			   "bug DMI_TC.012, case 2.e (\"load\")"));
#endif

	  if (use_insn32)
	    {
	      pfrag = frag_more (4);
	      md_number_to_chars (pfrag, 0x0d, 4);
	    }
	  else
	    {
	      pfrag = frag_more (2);
	      md_number_to_chars (pfrag, 0x00, 2);
	    }

	  frag_grow (MAX_RELAXED_INSN_SIZE);

	  /* Now that we've inserted a NOP, we can safely reset all possibly
	     pending DMI12 workarounds (see the comment for "NOP" above for
	     details).  */
	  goto reset_dmi12;
	}

      /* Is there another instruction sequence pending that might trigger
         some DMI12 bug?  */
      if (workaround_dmi12_2ab_pending
          || workaround_dmi12_2c_pending
          || workaround_dmi12_2d_pending)
        {
	  if ((current_seg == now_seg) && (current_subseg == now_subseg))
	    {
	      const char *name = the_insn.code->name;

	      if (!strncmp (name, "ld.", 3)
	      	  || !strcmp (name, "st.t")
		  || !strcmp (name, "ldmst")
		  || !strcmp (name, "swap.w")
		  || !strcmp (name, "ldlcx")
		  || !strcmp (name, "lducx")
	      	  || !strncmp (name, "cachea.", 7))
	        {
		  if (workaround_dmi12_2ab_pending)
		    {
		      if (show_internals)
		    	printf ("*** Inserting NOP (workaround "
		    	    	"for DMI_TC.012 bug, cases 2.a/b).\n");

#if EXT_LISTING
		      ASM_NOTICE_WORKAROUND (_("inserting NOP as a workaround "
		      			       "for bug DMI_TC.012, cases "
					       "2.a/b"));
#endif

		      if (use_insn32)
		    	{
		      	  pfrag = frag_more (4);
		      	  md_number_to_chars (pfrag, 0x0d, 4);
		    	}
		      else
		    	{
		      	  pfrag = frag_more (2);
		      	  md_number_to_chars (pfrag, 0x00, 2);
		    	}

		      frag_grow (MAX_RELAXED_INSN_SIZE);
		      workaround_dmi12_2ab_pending = 0;
		      workaround_dmi12_2c_pending = 0;
		      workaround_dmi12_2d_pending = 0;
		      workaround_dmi12_2e_pending = 0;
		      addressregno = -1;
		      double_addressreg = 0;
		      dataregno = -1;
		      double_datareg = 0;
		      goto recheck_dmi12;
		    }
		  workaround_dmi12_2ab_pending = 0;

	          if (workaround_dmi12_2d_pending
		      && ((!strncmp (name, "ld.", 3)
		           && (strchr ("diD", the_insn.code->args[0]) != NULL)
			   && ((the_insn.regs[0] == dataregno)
			       || (double_datareg
			           ? (the_insn.regs[0] == (dataregno + 1))
				   : 0)))
			  || (!strcmp (name, "ldmst")
			      && ((the_insn.regs[the_insn.nops - 1]
			           == dataregno)
			          || ((the_insn.regs[the_insn.nops - 1] + 1)
				      == dataregno)))
			  || (!strcmp (name, "swap.w")
			      && ((the_insn.regs[the_insn.nops - 1]
			           == dataregno)
			          || (double_datareg
				      ? (the_insn.regs[the_insn.nops - 1]
				         == (dataregno + 1))
				      : 0)))
			  || (!strcmp (name, "ldlcx") && (dataregno < 8))
			  || (!strcmp (name, "lducx") && (dataregno > 7))))
	            {
		      /* If the previous insn was an arithmetic insn and
		         used a data register as its target operand, and
			 the current insn is a "load" insn that uses the very
			 same data register as its target operand, we need
			 to insert a "nop".  This won't happen in real apps,
			 though, as it doesn't make any sense to overwrite
			 a data register that has just been used to compute
			 some result.  A compiler might produce such code,
			 but only if the source code itself doesn't make
			 much sense, and if optimizations are turned off.  */
		      if (show_internals)
		    	printf ("*** Inserting NOP (workaround "
		    	    	"for DMI_TC.012 bug, case 2.d).\n");
#if EXT_LISTING
		      ASM_NOTICE_WORKAROUND (_("inserting NOP as a workaround "
		      			       "for bug DMI_TC.012, case 2.d"));
#endif

		      if (use_insn32)
		    	{
		      	  pfrag = frag_more (4);
		      	  md_number_to_chars (pfrag, 0x0d, 4);
		    	}
		      else
		    	{
		      	  pfrag = frag_more (2);
		      	  md_number_to_chars (pfrag, 0x00, 2);
		    	}

		      frag_grow (MAX_RELAXED_INSN_SIZE);
		      workaround_dmi12_2c_pending = 0;
		      addressregno = -1;
		      double_addressreg = 0;
		      workaround_dmi12_2d_pending = 0;
		      dataregno = -1;
		      double_datareg = 0;
		      workaround_dmi12_2e_pending = 0;
		      goto recheck_dmi12;
		    }
	          workaround_dmi12_2d_pending = 0;
		  dataregno = -1;
		  double_datareg = 0;

	      	  if (workaround_dmi12_2c_pending)
		    {
		      /* The previous insn was either "ld.a" or "ld.da", so
		         we have to insert a "nop" in case the current insn
		         uses the just-loaded address register(s) to access
			 one of its source operands.  */
	              if ((the_insn.code->nr_operands > 1)
		  	  && strchr ("@&<>*#S", the_insn.code->args[1])
		  	  && ((the_insn.regs[1] == addressregno)
		      	      || (double_addressreg
		          	  ? (the_insn.regs[1] == (addressregno + 1))
		          	  : 0)))
			{
		  	  if (show_internals)
		    	    printf ("*** Inserting NOP (workaround "
		    	    	    "for DMI_TC.012 bug, case 2.c).\n");

#if EXT_LISTING
		          ASM_NOTICE_WORKAROUND (_("inserting NOP as a "
			  			   "workaround for bug "
						   "DMI_TC.012, case 2.c"));
#endif

		  	  if (use_insn32)
		    	    {
		      	      pfrag = frag_more (4);
		      	      md_number_to_chars (pfrag, 0x0d, 4);
		    	    }
		  	  else
		    	    {
		      	      pfrag = frag_more (2);
		      	      md_number_to_chars (pfrag, 0x00, 2);
		    	    }

		  	  frag_grow (MAX_RELAXED_INSN_SIZE);
			}
		      else if (workaround_dmi12_2c_pending == 2)
		        {
		          /* The previous insn was "ld.[d]a", and the
		             current insn doesn't use the just loaded
		             address register to read some operand.
		             Unfortunately, if the current insn is some
		             integer pipeline insn and the next insn
		             uses the address register loaded by the
		             previous insn, then the previous and the
		             next insn will still be executed in parallel,
		             which may trigger the cache bug.  Since we
		             cannot easily know the next insn at this point
		             in time, we extend the pending workaround to
		             also include the next insn.  */
			  workaround_dmi12_2c_pending = 1;
			  goto recheck_dmi12;
			}

		      workaround_dmi12_2c_pending = 0;
		      addressregno = -1;
		      double_addressreg = 0;
		    }

		  goto recheck_dmi12;
		}
	      else
	        {
	          if (workaround_dmi12_2c_pending)
		    {
		      if ((workaround_dmi12_2c_pending -= 1) == 0)
		        {
			  addressregno = -1;
		          double_addressreg = 0;
			}
		    }
		  workaround_dmi12_2ab_pending = 0;
		  workaround_dmi12_2d_pending = 0;
		  goto recheck_dmi12;
		}
	    }
	}
      else
        {
	  const char *name;

 recheck_dmi12:
 	  name = the_insn.code->name;
	  /* All insns using TriCore's integer pipeline and which use
	     a data register as their target (which excludes all jump
	     insns) may trigger case 2d of the DMI12 cache bug.  */
	  if ((the_insn.opcode & 2)
	      && (the_insn.code->nr_operands > 1)
	      && (strchr ("diD", the_insn.code->args[0]) != NULL)
	      && !((name[0] == 'j')
	           && (strpbrk (the_insn.code->args, "mxrRoOMtT") != NULL)))
	    {
	      workaround_dmi12_2d_pending = 1;
	      dataregno = the_insn.regs[0];
	      double_datareg = (the_insn.code->args[0] == 'D');
	      current_seg = now_seg;
	      current_subseg = now_subseg;
	    }

	  /* All store instructions must be followed by a NOP, as requested
	     by case 2e of the DMI12 cache bug workaround description; this
	     NOP will be emitted right before the next instruction that
	     goes into the current segment and subsegment.  Note that the
	     second part of the 2e workaround, where load instructions must
	     be preceded by a NOP insns, is already handled above.  */
	  if (!strncmp (name, "st.", 3)
	      || !strcmp (name, "stlcx")
	      || !strcmp (name, "stucx"))
	    {
	      workaround_dmi12_2e_pending = 1;
	      current_seg = now_seg;
	      current_subseg = now_subseg;
#if EXT_LISTING
	      /* Remember the frag that starts a critical insn sequence, so
	         we can attach an appropriate message to it in case it
		 actually requires a workaround (see above for details).  */
              pending_dmi12_2e_frag = frag_now;
#endif
	    }

	  /* Conditional jumps may trigger case 2b of the DMI12 cache bug,
	     and various other insns may trigger case 2a (as described in
	     the relevant Errata Sheet).  Since both cases require the same
	     set of insns following them in order to potentially cause
	     problems, we're combining them in a single "2ab" case.  */
	  if (((name[0] == 'j') && (name[1] != '\0')
	       && (the_insn.code->nr_operands > 1)
	       && (strpbrk (the_insn.code->args, "mxrRoOMtT") != NULL))
	      || (!strncmp (name, "madd", 4) && strcmp (name, "madd.h"))
	      || (!strncmp (name, "msub", 4) && strcmp (name, "msub.h"))
	      || (!strncmp (name, "mul", 3) && strcmp (name, "mul.h"))
	      || !strncmp (name, "dvstep", 6)
	      || !strncmp (name, "fto", 3)
	      || !strcmp (name, "add.f")
	      || !strcmp (name, "sub.f")
	      || !strcmp (name, "div.f")
	      || !strcmp (name, "itof")
	      || !strcmp (name, "q31tof")
	      || !strcmp (name, "utof"))
	    {
	      workaround_dmi12_2ab_pending = 1;
	      current_seg = now_seg;
	      current_subseg = now_subseg;
	    }
	  else if (!strcmp (name, "ld.a") || !strcmp (name, "ld.da"))
	    {
	      /* These insns may trigger case 2c of the DMI12 cache bug.
	         Note that case 2e (see above) supersedes this case.  */
	      workaround_dmi12_2c_pending = 2;
	      addressregno = the_insn.regs[0];
	      double_addressreg = (name[3] == 'd');
	      current_seg = now_seg;
	      current_subseg = now_subseg;
	    }
	}
    }

  /* Allocate frag memory to hold the opcode.  */
  pfrag = frag_more (oplen);

  /* Emit the instruction code.  */
  md_number_to_chars (pfrag, the_insn.opcode, oplen);

  /* Emit DWARF2 line debug info (if requested).  */
  dwarf2_emit_insn (oplen);

  /* Emit fixups and relaxation infos if necessary.  */
  dont_relax = use_insn16 || (the_insn.code->format == TRICORE_FMT_B);
  for (i = 0; i < the_insn.nops; ++i)
    {
      if (the_insn.ops[i] != 'U')
        continue;

      CHECK_RELOC (i);
      if (dont_relax || !the_insn.pcrel[i])
        {
	  if (the_insn.bitpos[i])
	    {
	      fix_new_exp (frag_now, (pfrag - frag_now->fr_literal), 0,
	      		   &the_insn.opexp[i], the_insn.pcrel[i],
			   BFD_RELOC_TRICORE_BITPOS);
	      if (show_internals)
	        printf ("*** fix_new_exp (%d, BFD_RELOC_TRICORE_BITPOS)\n", i);
	    }
          fix_new_exp (frag_now, (pfrag - frag_now->fr_literal), 0,
	               &the_insn.opexp[i], the_insn.pcrel[i], reloc);
          if (show_internals)
    	    printf ("*** fix_new_exp (%d, %s)\n", i, GET_RELOC_NAME (reloc));
	  continue;
	}

      /* This is a PC-relative jump/call insn; prepare it for relaxing.  */
      switch (the_insn.code->format)
	{
	case TRICORE_FMT_SB:
	  if (the_insn.code->nr_operands == 1)
	    state = RELAX_SB2;
	  else
	    state = RELAX_SB;
	  break;

	case TRICORE_FMT_SBR:
	  if (the_insn.is_loop)
	    state = RELAX_LOOP;
	  else if (the_insn.code->args[i] == 'x')
	    state = RELAX_SBR2;
	  else  
	    state = RELAX_SBR;
	  break;

	case TRICORE_FMT_SBC:
	  if (the_insn.code->args[i] == 'x')
	    state = RELAX_SBC2;
	  else  
	    state = RELAX_SBC;
	  break;

	case TRICORE_FMT_SBRN:
	  state = RELAX_SBRN;
	  break;

	case TRICORE_FMT_BRN:
	  state = RELAX_BRN;
	  break;

	case TRICORE_FMT_BRR:
	case TRICORE_FMT_BRC:
	  if (the_insn.is_loop)
	    {
	      if (use_insn32 || big_insns || big_insns_only)
	        state = RELAX_BLOOP2;
	      else
	        state = RELAX_BLOOP;
	    }
	  else if (!strcmp (the_insn.code->name, "jned")
	      || !strcmp (the_insn.code->name, "jnei"))
	    {
	      if (use_insn32 || big_insns || big_insns_only)
	        state = RELAX_JNEX2;
	      else
	        state = RELAX_JNEX;
	    }
	  else if (!strcmp (the_insn.code->name, "loopu"))
	    state = RELAX_LOOPU;
	  else
	    state = RELAX_BRX;
	  break;

	default:
	  as_fatal (_("Internal error: attempt to relax format <%s> "
	  	      "in emit_code"),
	      	    fmt_name[the_insn.code->format]);
	  break;
	}

      /* We already have allocated space in the current frag to hold
         this insn's opcode (this was done by calling frag_more above).
	 Now we call frag_var to reserve space for add_bytes more
	 bytes in case the insn needs being expanded.  */
      add_bytes = MAX_RELAXED_INSN_SIZE - oplen;
      (void) frag_var (rs_machine_dependent, add_bytes, add_bytes, state,
		       the_insn.opexp[i].X_add_symbol, VAL (i), pfrag);
      if (show_internals)
    	printf ("*** frag_var (%d, add_bytes=%d)\n", i, add_bytes);
    }

  /* Remember the address where the opcode is stored (for debugging).  */
  the_insn.opc_address = (unsigned long) pfrag;
}

/* This is called for each rs_machine_dependent frag (pointed to by
   FRAGP), right before SEGMENT is relaxed.  Any symbol that is
   still undefined will not become defined, so we need to switch to
   the biggest variant of this insn.  Return the estimated length in
   bytes by which this frag must grow to reach its destination.  */

int
md_estimate_size_before_relax (fragP, segment)
     register fragS *fragP;
     register segT segment;
{
  if (show_internals)
    printf ("*** md_estimate_size_before_relax (%lx): ", OPC_ADDR (fragP));

  if ((fragP->fr_symbol != (symbolS *) 0)
      && ((S_GET_SEGMENT (fragP->fr_symbol) == segment)
          || (S_GET_SEGMENT (fragP->fr_symbol) == absolute_section))
      && S_IS_DEFINED (fragP->fr_symbol)
      && !S_IS_EXTERNAL (fragP->fr_symbol)
      && !S_IS_WEAK (fragP->fr_symbol))
    {
      /* If a symbol is defined and lives in the same segment as the
         insn that references it (or in the absolute segment), then
	 relax_segment will automatically find out if the displacement
	 fits in the original insn, or if this insn needs being
	 expanded (which is eventually done in md_convert_frag).  */
      if (show_internals)
        {
	  if (md_relax_table[fragP->fr_subtype].rlx_length)
	    printf ("+ %d bytes (-> state %d)\n",
	  	    md_relax_table[fragP->fr_subtype].rlx_length,
		    fragP->fr_subtype);
	  else if (fragP->fr_subtype == (tricore_relax_loopu_state + 1))
	    printf ("loopu -> j\n");
	  else
            printf ("currently keeping state %d\n", fragP->fr_subtype);
	}
    }
  else
    {
      relax_substateT old_state, state;
      const relax_typeS *type;

      /* The symbol lives in a different segment (or is global or weak,
         so it can be overridden by a non-SO/non-weak definition), which
	 means that it can't be reached by a single "short" insn in all
	 cases.  Only a relaxing linker could find this out, but then this
	 function would be unnecessary and never be called, because we
	 would never call frag_var, and pass all PC-relative fixups as
	 relocations to the linker.  The only reasonable thing to do here
	 is to switch to the last valid entry in md_relax_table for this
	 relaxation type.  */
      old_state = state = fragP->fr_subtype;
      type = md_relax_table + old_state;
      while (type->rlx_more)
        {
	  state = type->rlx_more;
          type = md_relax_table + state;
	}
      fragP->fr_subtype = state;
      if (show_internals)
        {
	  if (state != old_state)
	    printf ("+ %d bytes (-> max. state %d)\n",
	  	    type->rlx_length, state);
	  else
	    printf ("[+ %d bytes] (max. state %d kept)\n",
	    	    type->rlx_length, state);
	}
    }

  return md_relax_table[fragP->fr_subtype].rlx_length;
}

/* This is called for each rs_machine_dependent frag (pointed to by
   FRAGP), right after SEGMENT is relaxed.  If necessary, convert
   the insn so that it can reach its target.  Set the final size of
   this frag, and reset its variable part (FRAGP->fr_var).  Note that
   the value of FRAGP->fr_symbol might be undefined, so we can't
   use it to determine the target address; we emit fixups instead.  */

void
md_convert_frag (abfd, segment, fragP)
     bfd *abfd ATTRIBUTE_UNUSED;
     segT segment ATTRIBUTE_UNUSED;
     register fragS *fragP;
{
  char *buf = fragP->fr_opcode, bytecode;
  int convert_insn = 0, pcrel = 1, regno;
  char optype, regtype, *opc16 = NULL, *opc_name = NULL;
  enum bfd_reloc_code_real reloc = BFD_RELOC_TRICORE_NONE;
  offsetT fix_where = fragP->fr_fix - 2;
  opcode_t *code;
  fixS *new_fix;

  if (show_internals)
    printf ("*** md_convert_frag (%lx): ", OPC_ADDR (fragP));

#if EXT_LISTING
  if (listing & LISTING_LISTING)
    ext_listing_frag = fragP;
#endif

  switch (fragP->fr_subtype)
    {
    case RELAX_SB:
    case RELAX_SB2:
      reloc = BFD_RELOC_TRICORE_8REL;
      break;

    case RELAX_LOOP:
      reloc = BFD_RELOC_TRICORE_4REL2;
      break;

    case RELAX_SBR:
    case RELAX_SBC:
    case RELAX_SBRN:
      reloc = BFD_RELOC_TRICORE_4REL;
      break;

    case RELAX_SBR2:
    case RELAX_SBC2:
      reloc = BFD_RELOC_TRICORE_5REL;
      break;

    case RELAX_BRX:
    case RELAX_BRN:
    case RELAX_JNEX:
    case RELAX_JNEX2:
    case RELAX_BLOOP:
    case RELAX_BLOOP2:
    case RELAX_LOOPU:
      fix_where -= 2;
      reloc = BFD_RELOC_TRICORE_15REL;
      break;

    case RELAX_JC:
      reloc = BFD_RELOC_TRICORE_PCPREL6;
      break;

    case RELAX_JL:
      reloc = BFD_RELOC_TRICORE_PCPREL10;
      break;

    case RELAX_SB + 1:
    case RELAX_SB2 + 1:
    case RELAX_LOOP + 1:
    case RELAX_SBR + 1:
    case RELAX_SBC + 1:
    case RELAX_SBR2 + 1:
    case RELAX_SBC2 + 1:
    case RELAX_SBRN + 1:
      convert_insn = 1;
      break; 

    case RELAX_BRX + 1:
    case RELAX_BRN + 1:
    case RELAX_JNEX + 1:
    case RELAX_JNEX2 + 1:
    case RELAX_BLOOP + 1:
    case RELAX_BLOOP2 + 1:
    case RELAX_LOOPU + 1:
      fix_where -= 2;
      reloc = BFD_RELOC_TRICORE_24REL;
      convert_insn = 2;
      break;

    case RELAX_JC + 1:
    case RELAX_JL + 1:
      fix_where += 2;
      reloc = BFD_RELOC_TRICORE_PCPTEXT;
      convert_insn = 3;
      pcrel = 0;
      break; 

    case RELAX_SB + 2:
    case RELAX_SBC + 2:
    case RELAX_SBC2 + 2:
    case RELAX_SBR + 2:
    case RELAX_SBR2 + 2:
    case RELAX_SBRN + 2:
    case RELAX_LOOP + 2:
      reloc = BFD_RELOC_TRICORE_24REL;
      convert_insn = 4;
      break;

    default:
      as_fatal (_("Internal error: illegal relax type %d"), fragP->fr_subtype);
      break;
    }

  if (convert_insn == 0)
    {
      /* There is obviously no reason to convert the insn, so all
         we need to do is to generate a fixup for the symbol.  */
      if (show_internals)
        printf ("keeping original insn\n");
    }
  else if (convert_insn == 1)
    {
      /* Converting 16-bit to 32-bit TriCore-insns isn't fun at all:
         it's not enough to just toggle a bit (or to replace the opcode)
         and appending the displacement, because 32-bit insns not only
         have different opcodes, but also different numbers and different
         (though upward-compatible) kinds of operands.  Even the names
         of 16-bit and 32-bit insns can be different.  What a mess...
         But, hey -- Don't Panic!  We can easily re-use the 16->32-bit
         insn conversion functions (and a few others that are helpful in
         this regard), so all we have to do is to reconstruct the_insn,
         and then call the appropriate functions to do the real work.  */
      if (show_internals)
        printf ("converting 16-bit to 32-bit insn\n");

      memset ((char *) &the_insn, 0, sizeof (the_insn));
      if (S_GET_SEGMENT (fragP->fr_symbol) == undefined_section)
        optype = 'U';
      else
        optype = 'X';

      switch (fragP->fr_subtype)
        {
        case RELAX_SB + 1:		/* Convert insns of type SB.  */
        case RELAX_SB2 + 1:
          bytecode = *buf & tricore_opmask[TRICORE_FMT_SB];
          if (optype != 'U')
            optype = 'R';
          if (bytecode == tricore_insn_j_R)
            {
	      opc_name = (char *)"j";
              opc16 = (char *)"j-R";
	    }
          else if (bytecode == tricore_insn_call_R)
            {
	      opc_name = (char *)"call";
	      opc16 = (char *)"call-R";
	    }
          else if (bytecode == tricore_insn_jnz_iR)
            {
	      opc_name = (char *)"jnz";
              opc16 = (char *)"jnz-iR";
	      the_insn.regs[0] = 15;
	      the_insn.ops[0] = 'i';
	      the_insn.nops = 1;
	    }
          else if (bytecode == tricore_insn_jz_iR)
            {
	      opc_name = (char *)"jz";
              opc16 = (char *)"jz-iR";
	      the_insn.regs[0] = 15;
	      the_insn.ops[0] = 'i';
	      the_insn.nops = 1;
	    }
          else
            {
              as_fatal (_("Internal error: unknown SB opcode 0x%02x"),
	      		bytecode);
	      return;
	    }
          break;

        case RELAX_LOOP + 1:		/* Convert loop insns (SBR).  */
          bytecode = *buf & tricore_opmask[TRICORE_FMT_SBR];
          if (optype != 'U')
            optype = 'r';
          if (bytecode == tricore_insn_loop_ar)
            {
	      opc_name = (char *)"loop";
	      opc16 = (char *)"loop-ar";
	      the_insn.regs[0] = ((buf[1] >> 4) & 0xf);
	      the_insn.ops[0] = 'a';
	      the_insn.nops = 1;
	      the_insn.is_loop = 1;
	    }
          else
            {
              as_fatal (_("Internal error: unknown loop insn 0x%02x"),
	      		bytecode);
	      return;
	    }
          break;

        case RELAX_SBR + 1:		/* Convert insns of type SBR.  */
          bytecode = *buf & tricore_opmask[TRICORE_FMT_SBR];
          if (optype != 'U')
            optype = 'm';
          regno = ((buf[1] >> 4) & 0xf);
          regtype = 'd';
          if (bytecode == tricore_insn_jeq_idm)
            {
	      opc_name = (char *)"jeq";
              opc16 = (char *)"jeq-idm";
	      the_insn.regs[0] = 15;
              the_insn.ops[0] = 'i';
	      the_insn.nops = 1;
	    }
          else if (bytecode == tricore_insn_jne_idm)
            {
	      opc_name = (char *)"jne";
              opc16 = (char *)"jne-idm";
	      the_insn.regs[0] = 15;
              the_insn.ops[0] = 'i';
	      the_insn.nops = 1;
	    }
          else if (bytecode == tricore_insn_jgez_dm)
            {
	      opc_name = (char *)"jgez";
	      opc16 = (char *)"jgez-dm";
	    }
          else if (bytecode == tricore_insn_jgtz_dm)
            {
	      opc_name = (char *)"jgtz";
	      opc16 = (char *)"jgtz-dm";
	    }
          else if (bytecode == tricore_insn_jlez_dm)
            {
	      opc_name = (char *)"jlez";
	      opc16 = (char *)"jlez-dm";
	    }
          else if (bytecode == tricore_insn_jltz_dm)
            {
	      opc_name = (char *)"jltz";
	      opc16 = (char *)"jltz-dm";
	    }
          else if (bytecode == tricore_insn_jnz_dm)
            {
	      opc_name = (char *)"jnz";
	      opc16 = (char *)"jnz-dm";
	    }
          else if (bytecode == tricore_insn_jnza_am)
            {
	      opc_name = (char *)"jnz.a";
	      opc16 = (char *)"jnz.a-am";
	      regtype = 'a';
	    }
          else if (bytecode == tricore_insn_jz_dm)
            {
	      opc_name = (char *)"jz";
	      opc16 = (char *)"jz-dm";
	    }
          else if (bytecode == tricore_insn_jza_am)
            {
	      opc_name = (char *)"jz.a";
	      opc16 = (char *)"jz.a-am";
	      regtype = 'a';
	    }
          else
            {
              as_fatal (_("Internal error: unknown SBR opcode 0x%02x"),
	      		bytecode);
	      return;
	    }
          the_insn.regs[the_insn.nops] = regno;
          the_insn.ops[the_insn.nops] = regtype;
          ++the_insn.nops;
          break;

        case RELAX_SBR2 + 1:		/* Convert insns of type SBR2.  */
          bytecode = *buf & tricore_opmask[TRICORE_FMT_SBR];
          if (optype != 'U')
            optype = 'x';
          regno = ((buf[1] >> 4) & 0xf);
          regtype = 'd';
          if (bytecode == tricore_insn_jeq_idx)
            {
	      opc_name = (char *)"jeq";
              opc16 = (char *)"jeq-idx";
	      the_insn.regs[0] = 15;
              the_insn.ops[0] = 'i';
	      the_insn.nops = 1;
	    }
          else if (bytecode == tricore_insn_jne_idx)
            {
	      opc_name = (char *)"jne";
              opc16 = (char *)"jne-idx";
	      the_insn.regs[0] = 15;
              the_insn.ops[0] = 'i';
	      the_insn.nops = 1;
	    }
          else
            {
              as_fatal (_("Internal error: unknown SBR2 opcode 0x%02x"),
	      		bytecode);
	      return;
	    }
          the_insn.regs[the_insn.nops] = regno;
          the_insn.ops[the_insn.nops] = regtype;
          ++the_insn.nops;
          break;

        case RELAX_SBC + 1:		/* Convert insns of type SBC.  */
          bytecode = *buf & tricore_opmask[TRICORE_FMT_SBC];
          if (optype != 'U')
            optype = 'm';
          the_insn.regs[0] = 15;
          the_insn.ops[0] = 'i';
          VAL (1) = ((buf[1] >> 4) & 0xf);
          /* Sign-extend if necessary.  */
          if (VAL (1) & 0x8)
            VAL (1) |= ~0xf;
          the_insn.ops[1] = '4';
          the_insn.nops = 2;
          if (bytecode == tricore_insn_jeq_i4m)
	    {
	      opc_name = (char *)"jeq";
              opc16 = (char *)"jeq-i4m";
	    }
          else if (bytecode == tricore_insn_jne_i4m)
            {
	      opc_name = (char *)"jne";
              opc16 = (char *)"jne-i4m";
	    }
          else
            {
              as_fatal (_("Internal error: unknown SBC opcode 0x%02x"),
	      		bytecode);
	      return;
	    }
          break;

        case RELAX_SBC2 + 1:		/* Convert insns of type SBC2.  */
          bytecode = *buf & tricore_opmask[TRICORE_FMT_SBC];
          if (optype != 'U')
            optype = 'x';
          the_insn.regs[0] = 15;
          the_insn.ops[0] = 'i';
          VAL (1) = ((buf[1] >> 4) & 0xf);
          /* Sign-extend if necessary.  */
          if (VAL (1) & 0x8)
            VAL (1) |= ~0xf;
          the_insn.ops[1] = '4';
          the_insn.nops = 2;
          if (bytecode == tricore_insn_jeq_i4x)
	    {
	      opc_name = (char *)"jeq";
              opc16 = (char *)"jeq-i4x";
	    }
          else if (bytecode == tricore_insn_jne_i4x)
            {
	      opc_name = (char *)"jne";
              opc16 = (char *)"jne-i4x";
	    }
          else
            {
              as_fatal (_("Internal error: unknown SBC2 opcode 0x%02x"),
	      		bytecode);
	      return;
	    }
          break;

        case RELAX_SBRN + 1:		/* Convert insns of type SBRN.  */
          bytecode = *buf & tricore_opmask[TRICORE_FMT_SBRN];
          if (optype != 'U')
            optype = 'm';
          the_insn.regs[0] = 15;
          the_insn.ops[0] = 'i';
          VAL (1) = ((buf[1] >> 4) & 0x0f);
          if (current_isa == TRICORE_V1_1)
            {
              VAL (1) |= ((buf[0] & 0x80) >> 3);
              the_insn.ops[1] = '5';
	    }
          else
            the_insn.ops[1] = 'f';
          the_insn.nops = 2;
          if (bytecode == tricore_insn_jnzt_i5m)
            {
	      opc_name = (char *)"jnz.t";
              opc16 = (char *)"jnz.t-i5m";
	    }
          else if (bytecode == tricore_insn_jnzt_ifm)
            {
	      opc_name = (char *)"jnz.t";
              opc16 = (char *)"jnz.t-ifm";
	    }
          else if (bytecode == tricore_insn_jzt_i5m)
            {
	      opc_name = (char *)"jz.t";
              opc16 = (char *)"jz.t-i5m";
	    }
          else if (bytecode == tricore_insn_jzt_ifm)
	    {
	      opc_name = (char *)"jz.t";
              opc16 = (char *)"jz.t-ifm";
	    }
          else
            {
              as_fatal (_("Internal error: unknown SBRN opcode 0x%02x"),
	      		bytecode);
	      return;
	    }
          break;
        }

      /* Complete the_insn.  */
      the_insn.opexp[the_insn.nops].X_add_symbol = fragP->fr_symbol;
      the_insn.opexp[the_insn.nops].X_add_number = fragP->fr_offset;
      the_insn.opexp[the_insn.nops].X_op = O_symbol;
      the_insn.pcrel[the_insn.nops] = 1;
      the_insn.ops[the_insn.nops] = optype;
      the_insn.opc_address = (unsigned long) buf;
      ++the_insn.nops;

      /* Find the first opcode whose name is opc_name.  */
      code = (opcode_t *) str_hash_find (hash_ops, opc_name);
      if (code == (opcode_t *) 0)
        {
          as_fatal (_("Internal error: couldn't lookup `%s'"), opc_name);
          return;
        }
      the_insn.code = code;

      /* Find its 32-bit equivalent.  */
      code = insn16_to_insn32 ();
      if (code == (opcode_t *) 0)
        {
          as_fatal (_("Internal error: couldn't convert `%s'"), opc16);
          return;
        }
      the_insn.code = code;
      the_insn.opcode = code->opcode;

      /* Find the relocation type.  */
      CHECK_RELOC (the_insn.nops - 1);

      /* Encode the new insn.  */
      encode[code->format] ();
      if (show_internals)
        print_the_insn ();

      /* Emit the new opcode.  */
      md_number_to_chars (buf, the_insn.opcode, 4);
    }
  else if (convert_insn == 2)
    {
      unsigned long opmask, opcode = 0, jump_insn = tricore_insn_j_O;

      /* Expand 32-bit instructions; this is done by inverting the condition
         code (if any) of the instruction and inserting a jump instruction.  */
      if (show_internals)
        printf ("expanding 32-bit insn\n");

#if EXT_LISTING
      if (listing & LISTING_LISTING)
        {
          snprintf (EXT_BUF, _("Inverting condition code, adding \"j\" insn"));
          ASM_NOTICE ();
	}
#endif /* EXT_LISTING  */

      switch (fragP->fr_subtype)
        {
        case RELAX_BRX + 1:
	  opmask = tricore_opmask[TRICORE_FMT_BRR];
          bytecode = *buf & opmask;
	  if ((bytecode == tricore_insn_jge_ddo)
	      || (bytecode == tricore_insn_jgeu_ddo))
	    {
	      *buf &= ~opmask;
	      *buf |= (tricore_insn_jlt_ddo & opmask);
	      if (tricore_insn_jlt_ddo != tricore_insn_jltu_ddo)
	        {
		  as_fatal (_("Internal error: jlt_ddo != jltu_ddo"));
		  return;
		}
	    }
	  else if ((bytecode == tricore_insn_jge_d4o)
	  	   || (bytecode == tricore_insn_jgeu_dfo))
	    {
	      *buf &= ~opmask;
	      *buf |= (tricore_insn_jlt_d4o & opmask);
	      if (tricore_insn_jlt_d4o != tricore_insn_jltu_dfo)
	        {
		  as_fatal (_("Internal error: jlt_d4o != jltu_dfo"));
		  return;
		}
	    }
	  else if ((bytecode == tricore_insn_jlt_ddo)
	  	   || (bytecode == tricore_insn_jltu_ddo))
	    {
	      *buf &= ~opmask;
	      *buf |= (tricore_insn_jge_ddo & opmask);
	      if (tricore_insn_jge_ddo != tricore_insn_jgeu_ddo)
	        {
		  as_fatal (_("Internal error: jge_ddo != jgeu_ddo"));
		  return;
		}
	    }
	  else if ((bytecode == tricore_insn_jlt_d4o)
	  	   || (bytecode == tricore_insn_jltu_dfo))
	    {
	      *buf &= ~opmask;
	      *buf |= (tricore_insn_jge_d4o & opmask);
	      if (tricore_insn_jge_d4o != tricore_insn_jgeu_dfo)
	        {
		  as_fatal (_("Internal error: jge_d4o != jgeu_dfo"));
		  return;
		}
	    }
	  else if ((bytecode == tricore_insn_jeq_ddo)
	  	   || (bytecode == tricore_insn_jne_ddo)
	  	   || (bytecode == tricore_insn_jeq_d4o)
	  	   || (bytecode == tricore_insn_jne_d4o)
		   || (bytecode == tricore_insn_jeqa_aao)
		   || (bytecode == tricore_insn_jnea_aao)
		   || (bytecode == tricore_insn_jza_ao)
		   || (bytecode == tricore_insn_jnza_ao))
	    {
	      buf[3] ^= 0x80;
	    }
	  else
	    {
	      as_fatal (_("Internal error: unknown BRX opcode 0x%02x"),
	      		bytecode);
	      return;
	    }
	  buf[2] = 0x04;
	  md_number_to_chars (buf + 4, jump_insn, 4);
	  fix_where += 4;
	  break;

	case RELAX_BRN + 1:
	  opmask = tricore_opmask[TRICORE_FMT_BRN];
	  bytecode = *buf & opmask;
	  if ((bytecode == tricore_insn_jzt_d5o)
	      || (bytecode == tricore_insn_jnzt_d5o))
	    {
	      buf[2] = 0x04;
	      buf[3] ^= 0x80;
	      md_number_to_chars (buf + 4, jump_insn, 4);
	      fix_where += 4;
	    }
	  else
	    {
	      as_fatal (_("Internal error: unknown BRN opcode 0x%02x"),
	      		bytecode);
	      return;
	    }
	  break;

	case RELAX_JNEX + 1:
	  opmask = tricore_opmask[TRICORE_FMT_BRR];
	  bytecode = *buf & opmask;
	  if ((bytecode == tricore_insn_jnei_ddo)
	      || (bytecode == tricore_insn_jnei_d4o)
	      || (bytecode == tricore_insn_jned_ddo)
	      || (bytecode == tricore_insn_jned_d4o))
	    {
	      buf[2] = 0x03;
	      opcode = tricore_insn_j_R | 0x0300;
	      md_number_to_chars (buf + 4, opcode, 2);
	      md_number_to_chars (buf + 6, jump_insn, 4);
	      fix_where += 6;
	    }
	  else
	    {
	      as_fatal (_("Internal error: unknown BRN opcode 0x%02x"),
	      		bytecode);
	      return;
	    }
	  break;

	case RELAX_JNEX2 + 1:
	  opmask = tricore_opmask[TRICORE_FMT_BRR];
	  bytecode = *buf & opmask;
	  if ((bytecode == tricore_insn_jnei_ddo)
	      || (bytecode == tricore_insn_jnei_d4o)
	      || (bytecode == tricore_insn_jned_ddo)
	      || (bytecode == tricore_insn_jned_d4o))
	    {
	      buf[2] = 0x04;
	      opcode = jump_insn | 0x40000;
	      md_number_to_chars (buf + 4, opcode, 4);
	      md_number_to_chars (buf + 8, jump_insn, 4);
	      fix_where += 8;
	    }
	  else
	    {
	      as_fatal (_("Internal error: unknown BRN opcode 0x%02x"),
	      		bytecode);
	      return;
	    }
	  break;

	case RELAX_BLOOP + 1:
	  buf[2] = 0x03;
	  opcode = tricore_insn_j_R | 0x0300;
	  md_number_to_chars (buf + 4, opcode, 2);
	  md_number_to_chars (buf + 6, jump_insn, 4);
	  fix_where += 6;
	  break;

	case RELAX_BLOOP2 + 1:
	  buf[2] = 0x04;
	  opcode = jump_insn | 0x40000;
	  md_number_to_chars (buf + 4, opcode, 4);
	  md_number_to_chars (buf + 8, jump_insn, 4);
	  fix_where += 8;
	  break;

	case RELAX_LOOPU + 1:
	  md_number_to_chars (buf, jump_insn, 4);
	  break;
	}
    }
  else if (convert_insn == 3)
    {
      unsigned long opcode = 0;

      /* Convert PCP's jc and jl insns to a jc.a insn.  */
      if (show_internals)
        printf ("converting jc/jl to jc.a insn (PCP)\n");

#if EXT_LISTING
      if (listing & LISTING_LISTING)
        {
          snprintf (EXT_BUF, _("Converting \"jc\"/\"jl\" to \"jc.a\" insn"));
          ASM_NOTICE ();
	}
#endif /* EXT_LISTING  */

      opcode = bfd_getl16 (buf);
      opcode = (opcode & 0xf3ff) | 0x0800;
      md_number_to_chars (buf, opcode, 4);
    }
  else if (convert_insn == 4)
    {
      unsigned long opmask, opcode = 0, jump_insn = tricore_insn_j_O;

      /* Expand 16-bit instructions; this is done by inverting the condition
         code (if any) of the instruction and inserting a jump instruction.  */
      if (show_internals)
        printf ("expanding 16-bit insn\n");

#if EXT_LISTING
      if (listing & LISTING_LISTING)
        {
          snprintf (EXT_BUF, _("Inverting condition code, adding \"j\" insn"));
          ASM_NOTICE ();
	}
#endif /* EXT_LISTING  */

      switch (fragP->fr_subtype)
        {
        case RELAX_SB + 2:
	  opmask = tricore_opmask[TRICORE_FMT_SB];
          bytecode = *buf & opmask;
	  if (bytecode == tricore_insn_jnz_iR)
	    {
	      *buf &= ~opmask;
	      *buf |= (tricore_insn_jz_iR & opmask);
	    }
	  else if (bytecode == tricore_insn_jz_iR)
	    {
	      *buf &= ~opmask;
	      *buf |= (tricore_insn_jnz_iR & opmask);
	    }
	  else
	    {
	      as_fatal (_("Internal error: unknown SB opcode 0x%02x"),
	      		bytecode);
	      return;
	    }
	  buf[1] = 0x03;
	  md_number_to_chars (buf + 2, jump_insn, 4);
	  fix_where += 2;
	  break;

	case RELAX_SBC + 2:
	case RELAX_SBC2 + 2:
	  opmask = tricore_opmask[TRICORE_FMT_SBC];
          bytecode = *buf & opmask;
	  if ((bytecode == tricore_insn_jeq_i4m)
	      || (bytecode == tricore_insn_jeq_i4x))
	    {
	      *buf &= ~opmask;
	      *buf |= (tricore_insn_jne_i4m & opmask);
	    }
	  else if ((bytecode == tricore_insn_jne_i4m)
	  	   || (bytecode == tricore_insn_jne_i4x))
	    {
	      *buf &= ~opmask;
	      *buf |= (tricore_insn_jeq_i4m & opmask);
	    }
	  else
	    {
	      as_fatal (_("Internal error: unknown SBC opcode 0x%02x"),
	      		bytecode);
	      return;
	    }
	  buf[1] &= 0xf0;
	  buf[1] |= 0x03;
	  md_number_to_chars (buf + 2, jump_insn, 4);
	  fix_where += 2;
	  break;

	case RELAX_SBR + 2:
	case RELAX_SBR2 + 2:
	  opmask = tricore_opmask[TRICORE_FMT_SBR];
          bytecode = *buf & opmask;
	  if ((bytecode == tricore_insn_jeq_idm)
	      || (bytecode == tricore_insn_jeq_idx))
	    {
	      *buf &= ~opmask;
	      *buf |= (tricore_insn_jne_idm & opmask);
	    }
	  else if ((bytecode == tricore_insn_jne_idm)
	  	   || (bytecode == tricore_insn_jne_idx))
	    {
	      *buf &= ~opmask;
	      *buf |= (tricore_insn_jeq_idm & opmask);
	    }
	  else if (bytecode == tricore_insn_jgez_dm)
	    {
	      *buf &= ~opmask;
	      *buf |= (tricore_insn_jltz_dm & opmask);
	    }
	  else if (bytecode == tricore_insn_jltz_dm)
	    {
	      *buf &= ~opmask;
	      *buf |= (tricore_insn_jgez_dm & opmask);
	    }
	  else if (bytecode == tricore_insn_jgtz_dm)
	    {
	      *buf &= ~opmask;
	      *buf |= (tricore_insn_jlez_dm & opmask);
	    }
	  else if (bytecode == tricore_insn_jlez_dm)
	    {
	      *buf &= ~opmask;
	      *buf |= (tricore_insn_jgtz_dm & opmask);
	    }
	  else if (bytecode == tricore_insn_jnz_dm)
	    {
	      *buf &= ~opmask;
	      *buf |= (tricore_insn_jz_dm & opmask);
	    }
	  else if (bytecode == tricore_insn_jz_dm)
	    {
	      *buf &= ~opmask;
	      *buf |= (tricore_insn_jnz_dm & opmask);
	    }
	  else if (bytecode == tricore_insn_jnza_am)
	    {
	      *buf &= ~opmask;
	      *buf |= (tricore_insn_jza_am & opmask);
	    }
	  else if (bytecode == tricore_insn_jza_am)
	    {
	      *buf &= ~opmask;
	      *buf |= (tricore_insn_jnza_am & opmask);
	    }
	  else
	    {
	      as_fatal (_("Internal error: unknown SBR opcode 0x%02x"),
	      		bytecode);
	      return;
	    }
	  buf[1] &= 0xf0;
	  buf[1] |= 0x03;
	  md_number_to_chars (buf + 2, jump_insn, 4);
	  fix_where += 2;
	  break;

        case RELAX_SBRN + 2:
	  opmask = tricore_opmask[TRICORE_FMT_SBRN];
          bytecode = *buf & opmask;
	  if (bytecode == tricore_insn_jnzt_ifm)
	    {
	      *buf &= ~opmask;
	      *buf |= (tricore_insn_jzt_ifm & opmask);
	    }
	  else if (bytecode == tricore_insn_jzt_ifm)
	    {
	      *buf &= ~opmask;
	      *buf |= (tricore_insn_jnzt_ifm & opmask);
	    }
	  else if (bytecode == tricore_insn_jnzt_i5m)
	    {
	      *buf &= ~opmask;
	      *buf |= (tricore_insn_jzt_i5m & opmask);
	    }
	  else if (bytecode == tricore_insn_jzt_i5m)
	    {
	      *buf &= ~opmask;
	      *buf |= (tricore_insn_jnzt_i5m & opmask);
	    }
	  else
	    {
	      as_fatal (_("Internal error: unknown SBRN opcode 0x%02x"),
	      		bytecode);
	      return;
	    }
	  buf[1] &= 0xf0;
	  buf[1] |= 0x03;
	  md_number_to_chars (buf + 2, jump_insn, 4);
	  fix_where += 2;
	  break;

        case RELAX_LOOP + 2:
	  opcode = tricore_insn_loop_ao;
	  opcode |= (0x30000 | ((buf[1] & 0xf0) << 8));
	  md_number_to_chars (buf, opcode, 4);
	  opcode = tricore_insn_j_R | 0x0300;
	  md_number_to_chars (buf + 4, opcode, 2);
	  md_number_to_chars (buf + 6, jump_insn, 4);
	  fix_where += 6;
	  break;
        }
    }
  else
    {
      as_fatal (_("Internal error: convert_insn = %d"), convert_insn);
      return;
    }

  /* Set the frag's new fixed size, reset its variable size, and emit a
     fixup for the symbol, which will either be resolved in md_apply_fix3,
     or converted to a relocation in tc_gen_reloc.  */
  fragP->fr_fix += md_relax_table[fragP->fr_subtype].rlx_length;
  fragP->fr_var = 0;
  new_fix = fix_new (fragP, fix_where, 0, fragP->fr_symbol,
		     fragP->fr_offset, pcrel, reloc);
  new_fix->fx_file = fragP->fr_file;
  new_fix->fx_line = fragP->fr_line;
  if (show_internals)
    printf ("*** fix_new (%s, %ld)\n", GET_RELOC_NAME (reloc), fix_where);
}

/* Check whether a fixup for a PCP instruction can be resolved; if so, the
   opcode will be patched appropriately and the fixup will be removed
   afterwards, otherwise it'll be turned into a relocation by tc_gen_reloc.  */

static void
md_pcp_apply_fix (fixS * fixP, valueT *valP, enum bfd_reloc_code_real reloc, char *buf)
{
  unsigned long code = 0;
  long val;
#define BAD as_bad_where
#define WHERE fixP->fx_file, fixP->fx_line

  if (show_internals)
    printf ("*** md_pcp_apply_fix (%lx, %s#%d, %s)\n", (valueT) buf,
    	    fixP->fx_file, fixP->fx_line, GET_RELOC_NAME (reloc));

  if (fixP->fx_addsy)
    {
      if (fixP->fx_pcrel
	  && (S_GET_SEGMENT (fixP->fx_addsy) == absolute_section))
	val = fixP->fx_offset + S_GET_VALUE (fixP->fx_addsy);
      else
	{
	  /* Relocation needed; remember *valP.  */
	  fixP->fx_addnumber = *valP;
	  return;
	}
    }
  else
    val = *valP;

  code = bfd_getl16 (buf);

  switch (reloc)
    {
    case BFD_RELOC_TRICORE_PCPHI:
      code = ((val >> 16) & 0xffff);
      break;

    case BFD_RELOC_TRICORE_PCPLO:
      code = (val & 0xffff);
      break;

    case BFD_RELOC_TRICORE_PCPPAGE:
      if (val & 0x3)
        {
          BAD (WHERE, _("PRAM target address is not word-aligned"));
	  break;
	}
      if (val & 0xff)
	as_warn (_("PRAM address (0x%08lx) should be "
		   "aligned to a 256-byte boundary"),
		 val);
      code |= (val & 0xff00);
      break;

    case BFD_RELOC_TRICORE_PCPOFF:
      if (val & 0x3)
        {
          BAD (WHERE, _("PRAM target address (0x%08lx) is not word-aligned"),
	       val);
	  break;
	}
      code |= ((val >> 2) & 0x3f);
      break;

    case BFD_RELOC_TRICORE_PCPTEXT:
      if ((val < 0) || (val > 65535))
        BAD (WHERE, _("PCP code address 0x%08lx out of range [0..65535]"), val);
      else
        code |= val;
      break;

    case BFD_RELOC_TRICORE_PCPREL10:
      if (val & 1)
        {
          BAD (WHERE, _("Displacement %ld is not even"), val);
	  break;
	}
      val >>= 1;
      if ((val < -512) || (val > 511))
        BAD (WHERE, _("Displacement %ld out of range [-512..511]"), val);
      else
        code |= (val & 0x3ff);
      break;

    case BFD_RELOC_TRICORE_PCPREL6:
      if (val & 1)
        {
          BAD (WHERE, _("Displacement %ld is not even"), val);
	  break;
	}
      val >>= 1;
      if ((val < -32) || (val > 31))
        BAD (WHERE, _("Displacement %ld out of range [-32..31]"), val);
      else
        code |= (val & 0x3f);
      break;

    default:
      BAD (WHERE, _("Internal error: unimplemented PCP relocation %d"), reloc);
      break;
    }

  /* Write back the fixed-up insn.  */
  md_number_to_chars ((char *)buf, code, 2);
  fixP->fx_done = 1;

  if (show_internals)
    printf ("*** new fixed opcode: 0x%04lx\n", code);

#undef BAD
#undef WHERE
}

/* Check whether a fixup for a TriCore instruction can be resolved; if so,
   the opcode will be patched appropriately and the fixup will be removed
   afterwards, otherwise it'll be turned into a relocation by tc_gen_reloc.  */

void
md_apply_fix (fixS *fixP, valueT *valP, segT segment ATTRIBUTE_UNUSED)
{
  char *buf = fixP->fx_where + fixP->fx_frag->fr_literal;
  unsigned long opcode;
  enum bfd_reloc_code_real reloc = fixP->fx_r_type;
  long val;
  int len32;
#define BAD as_bad_where
#define WHERE fixP->fx_file, fixP->fx_line
#define CHECK_DISPLACEMENT(off,min,max)					\
  if ((off) & 1)							\
    {									\
      BAD (WHERE, _("Displacement 0x%08lx is not even"), (off));	\
      break;								\
    }									\
  else if (((off) < (min)) || ((off) > (max)))				\
    {									\
      BAD (WHERE, _("Displacement 0x%08lx out of range"), (off));	\
      break;								\
    }

  switch (reloc)
    {
    case BFD_RELOC_TRICORE_PCPHI:
    case BFD_RELOC_TRICORE_PCPLO:
    case BFD_RELOC_TRICORE_PCPPAGE:
    case BFD_RELOC_TRICORE_PCPOFF:
    case BFD_RELOC_TRICORE_PCPTEXT:
    case BFD_RELOC_TRICORE_PCPREL6:
    case BFD_RELOC_TRICORE_PCPREL10:
      return md_pcp_apply_fix (fixP, valP, reloc, buf);

    default:
      break;  /* TriCore relocations are handled below.  */
    }

  //if (show_internals)
  //  printf ("*** md_apply_fix3 (%lx, %s#%d, %s)\n", (valueT) buf,
  //          fixP->fx_file, fixP->fx_line, GET_RELOC_NAME (reloc));

  if (fixP->fx_addsy)
    {
      if (fixP->fx_pcrel
          && (S_GET_SEGMENT (fixP->fx_addsy) == absolute_section))
	{
	  /* This happens if we have something like:
	        call gaga
	        .set gaga,42
	     In this case, the relocation value is that of
	     the symbol plus the optional offset, and the luser
	     who committed this sequence is, well, gaga.  :-)  */
	  val = fixP->fx_offset + S_GET_VALUE (fixP->fx_addsy);
	}
      else if (fixP->fx_subsy
      	       && (S_GET_SEGMENT (fixP->fx_subsy)
	           == S_GET_SEGMENT (fixP->fx_addsy))
	       && SEG_NORMAL (S_GET_SEGMENT (fixP->fx_addsy)))
	{
	  /* This happens when differences between two labels within
	     the same section (usually some text section) are
	     computed, and the "add" symbol is global -- in which case
	     tricore_force_relocation() will have returned 1 and thus
	     prevented write.c:fixup_segment() from resolving this
	     fixup.  However, fixup_segment() has already computed the
	     final value in *valP, so we can safely use this value and
	     immediately resolve the fixup instead of forcing a reloc.  */
	  val = *valP;
	}
      else
        {
          /* Relocation needed; remember *valP.  */
          fixP->fx_addnumber = *valP;

	  /* If this is a relocation against an absolute symbol,
	     we must subtract the symbol's value from fx_addnumber.  */
	  if (S_GET_SEGMENT (fixP->fx_addsy) == absolute_section)
	    fixP->fx_addnumber -= S_GET_VALUE (fixP->fx_addsy);

	  /* Turn absolute data relocations into pc-relative ones.  */
          if (fixP->fx_pcrel)
	    {
	      switch (reloc)
	        {
		case BFD_RELOC_8:
		case BFD_RELOC_TRICORE_8ABS:
                  fixP->fx_r_type = BFD_RELOC_8_PCREL;
		  break;

                case BFD_RELOC_16:
		case BFD_RELOC_TRICORE_16ABS:
                  fixP->fx_r_type = BFD_RELOC_16_PCREL;
		  break;

	        case BFD_RELOC_32:
		case BFD_RELOC_TRICORE_32ABS:
                  fixP->fx_r_type = BFD_RELOC_32_PCREL;
		  break;

		default:
		  break;
		}
	    }
          return;
	}
    }
  else
    val = *valP;

  if (reloc == BFD_RELOC_TRICORE_BITPOS)
    {
      /* This fixup can be resolved, which means that there was an
         operand like "bpos: foo", and "foo" eventually turned out to
	 be an absolute symbol (defined by a statement like "foo=42").
	 We can safely throw away this fixup, because the next fixup
	 will be for the same instruction and against the same symbol,
	 so it can also be resolved (and applied to this instruction,
	 according to its relocation type).  */
      fixP->fx_done = 1;
      return;
    }

  /* Handle simple data relocations.  */
  if (reloc == BFD_RELOC_8)
    {
      md_number_to_chars (buf, val, 1);
      fixP->fx_done = 1;
      return;
    }
  else if (reloc == BFD_RELOC_16)
    {
      md_number_to_chars (buf, val, 2);
      fixP->fx_done = 1;
      return;
    }
  else if (reloc == BFD_RELOC_32)
    {
      md_number_to_chars (buf, val, 4);
      fixP->fx_done = 1;
      return;
    }

  /* VTABLE relocations must be handled by the linker.  */
  if ((reloc == BFD_RELOC_VTABLE_ENTRY) || (reloc == BFD_RELOC_VTABLE_INHERIT))
    {
      fixP->fx_done = 0;
      return;
    }

  /* It's a fixup/relocation against an instruction.  */
  opcode = 0;
  len32 = (*buf & 1);
  if (len32)
    opcode = bfd_getl32 (buf);
  else
    opcode = bfd_getl16 (buf);

  switch (reloc)
    {
    case BFD_RELOC_TRICORE_24REL:
      CHECK_DISPLACEMENT (val, -16777216, 16777214);
      val >>= 1;
      opcode |= ((val & 0xffff) << 16);
      opcode |= ((val & 0xff0000) >> 8);
      break;

    case BFD_RELOC_TRICORE_24ABS:
      if (val & 0x0fe00001)
        {
          BAD (WHERE, _("Illegal 24-bit absolute address 0x%08lx"), val);
	  break;
	}
      val >>= 1;
      val |= ((val & 0x78000000) >> 7);
      opcode |= ((val & 0xffff) << 16);
      opcode |= ((val & 0xff0000) >> 8);
      break;

    case BFD_RELOC_TRICORE_18ABS:
      if (val & 0x0fffc000)
        {
	  BAD (WHERE, _("Illegal 18-bit absolute address 0x%08lx"), val);
	  break;
	}
      opcode |= ((val & 0x3f) << 16);
      opcode |= ((val & 0x3c0) << 22);
      opcode |= ((val & 0x3c00) << 12);
      opcode |= ((val & 0xf0000000) >> 16);
      break;

    case BFD_RELOC_TRICORE_HI:
      opcode |= (((val >> 16) & 0xffff) << 12);
      break;

    case BFD_RELOC_TRICORE_HIADJ:
      opcode |= ((((val + 0x8000) >> 16) & 0xffff) << 12);
      break;

    case BFD_RELOC_TRICORE_LO:
      opcode |= ((val & 0xffff) << 12);
      break;

    case BFD_RELOC_TRICORE_LO2:
      opcode |= ((val & 0x3f) << 16);
      opcode |= ((val & 0x3c0) << 22);
      opcode |= ((val & 0xfc00) << 12);
      break;

    case BFD_RELOC_TRICORE_16SM:
    case BFD_RELOC_TRICORE_10SM:
    case BFD_RELOC_TRICORE_16SM2:
      BAD (WHERE, _("Illegal prefix (sm:) for constant offset"));
      break;

    case BFD_RELOC_TRICORE_16CONST:
      if ((val < -32768) || (val > 32767))
        {
	  BAD (WHERE, _("16-bit signed value overflow (0x%08lx)"), val);
	  break;
	}
      opcode |= ((val & 0xffff) << 12);
      break;

    case BFD_RELOC_TRICORE_15REL:
      CHECK_DISPLACEMENT (val, -32768, 32766);
      opcode |= (((val >> 1) & 0x7fff) << 16);
      break;

    case BFD_RELOC_TRICORE_9ZCONST:
      if (val & ~511)
        {
	  BAD (WHERE, _("9-bit unsigned value overflow (0x%08lx)"), val);
	  break;
	}
      opcode |= (val << 12);
      break;

    case BFD_RELOC_TRICORE_9SCONST:
      if ((val < -256) || (val > 255))
        {
	  BAD (WHERE, _("9-bit signed value overflow (%ld)"), val);
	  break;
	}
      opcode |= ((val & 0x1ff) << 12);
      break;

    case BFD_RELOC_TRICORE_8REL:
      CHECK_DISPLACEMENT (val, -256, 254);
      val >>= 1;
      opcode |= ((val & 0xff) << 8);
      break;

    case BFD_RELOC_TRICORE_8CONST:
      if (val & ~255)
        {
          BAD (WHERE, _("8-bit unsigned value overflow (0x%08lx)"), val);
	  break;
	}
      opcode |= (val << 8);
      break;

    case BFD_RELOC_TRICORE_10OFF:
      if ((val < -512) || (val > 511))
        {
          BAD (WHERE, _("10-bit signed offset overflow (%ld)"), val);
	  break;
	}
      opcode |= ((val & 0x3f) << 16);
      opcode |= ((val & 0x3c0) << 22);
      break;

    case BFD_RELOC_TRICORE_16OFF:
      if ((val < -32768) || (val > 32767))
        {
          BAD (WHERE, _("16-bit signed offset overflow (%ld)"), val);
	  break;
	}
      opcode |= ((val & 0x3f) << 16);
      opcode |= ((val & 0x3c0) << 22);
      opcode |= ((val & 0xfc00) << 12);
      break;

    case BFD_RELOC_TRICORE_1BIT:
      if (val & ~1)
        {
	  BAD (WHERE, _("Invalid bit value (%ld)"), val);
	  break;
	}
      opcode |= (val << 11);
      break;

    case BFD_RELOC_TRICORE_3POS:
      if (val & ~7)
        {
	  BAD (WHERE, _("Invalid 3-bit bit position (%ld)"), val);
	  break;
	}
      opcode |= (val << 8);
      break;

    case BFD_RELOC_TRICORE_5POS:
      if (val & ~31)
        {
	  BAD (WHERE, _("Invalid 5-bit bit position (%ld)"), val);
	  break;
	}
      opcode |= (val << 16);
      break;

    case BFD_RELOC_TRICORE_5POS2:
      if (val & ~31)
        {
	  BAD (WHERE, _("Invalid 5-bit bit position (%ld)"), val);
	  break;
	}
      opcode |= (val << 23);
      break;

    case BFD_RELOC_TRICORE_BRCC:
      if ((val < -8) || (val > 7))
	{
	  BAD (WHERE, _("4-bit signed value overflow (%ld)"), val);
	  break;
	}
      opcode |= ((val & 0xf) << 12);
      break;

    case BFD_RELOC_TRICORE_BRCZ:
      if (val & ~15)
	{
	  BAD (WHERE, _("4-bit unsigned value overflow (0x%08lx)"), val);
	  break;
	}
      opcode |= (val << 12);
      break;

    case BFD_RELOC_TRICORE_BRNN:
      if (val & ~31)
        {
	  BAD (WHERE, _("Invalid 5-bit bit position (%ld)"), val);
	  break;
	}
      opcode |= ((val & 0xf) << 12);
      opcode |= ((val & 0x10) << 3);
      break;

    case BFD_RELOC_TRICORE_RRN:
      if (val & ~3)
        {
	  BAD (WHERE, _("2-bit unsigned value overflow (0x%08lx)"), val);
	  break;
	}
      opcode |= (val << 16);
      break;

    case BFD_RELOC_TRICORE_4CONST:
      if ((val < -8) || (val > 7))
        {
	  BAD (WHERE, _("4-bit signed value overflow (%ld)"), val);
	  break;
	}
      opcode |= ((val & 0xf) << 12);
      break;

    case BFD_RELOC_TRICORE_4REL:
      CHECK_DISPLACEMENT (val, 0, 30);
      val >>= 1;
      opcode |= ((val & 0xf) << 8);
      break;

    case BFD_RELOC_TRICORE_4REL2:
      CHECK_DISPLACEMENT (val, -32, -2);
      val >>= 1;
      opcode |= ((val & 0xf) << 8);
      break;

    case BFD_RELOC_TRICORE_5REL:
      CHECK_DISPLACEMENT (val, 0, 62);
      val >>= 1;
      opcode &= ~0x0080;
      opcode |= ((val & 0xf) << 8);
      opcode |= ((val & 0x10) << 3);
      break;

    case BFD_RELOC_TRICORE_4POS:
      if (val & ~15)
        {
	  BAD (WHERE, _("Invalid 4-bit bit position (%ld)"), val);
	  break;
	}
      opcode |= ((val & 0xf) << 12);	
      break;

    case BFD_RELOC_TRICORE_5POS3:
      if (val & ~31)
        {
	  BAD (WHERE, _("Invalid 5-bit bit position (%ld)"), val);
	  break;
	}
      opcode |= ((val & 0xf) << 12);
      opcode |= ((val & 0x10) << 3);
      break;

    case BFD_RELOC_TRICORE_4OFF:
      if (val & ~15)
        {
          BAD (WHERE, _("4-bit unsigned offset overflow (0x%08lx)"), val);
	  break;
	}
      opcode |= (val << 12);
      break;

    case BFD_RELOC_TRICORE_4OFF2:
      if (val & ~31)
        {
          BAD (WHERE, _("5-bit unsigned offset overflow (0x%08lx)"), val);
	  break;
	}
      else if (val & 1)
        {
          BAD (WHERE, _("5-bit unsigned offset is not even (0x%08lx)"), val);
	  break;
	}
      opcode |= (val << 11);
      break;

    case BFD_RELOC_TRICORE_4OFF4:
      if (val & ~63)
        {
          BAD (WHERE, _("6-bit unsigned offset overflow (0x%08lx)"), val);
	  break;
	}
      else if (val & 3)
        {
          BAD (WHERE,
	       _("6-bit unsigned offset is not a multiple of 4 (0x%08lx)"),
	       val);
	  break;
	}
      opcode |= (val << 10);
      break;

    case BFD_RELOC_TRICORE_42OFF:
      if (val & ~15)
        {
          BAD (WHERE, _("4-bit unsigned offset overflow (0x%08lx)"), val);
	  break;
	}
      opcode |= (val << 8);
      break;

    case BFD_RELOC_TRICORE_42OFF2:
      if (val & ~31)
        {
          BAD (WHERE, _("5-bit unsigned offset overflow (0x%08lx)"), val);
	  break;
	}
      else if (val & 1)
        {
          BAD (WHERE, _("5-bit unsigned offset is not even (0x%08lx)"), val);
	  break;
	}
      opcode |= (val << 7);
      break;

    case BFD_RELOC_TRICORE_42OFF4:
      if (val & ~63)
        {
          BAD (WHERE, _("6-bit unsigned offset overflow (0x%08lx)"), val);
	  break;
	}
      else if (val & 3)
        {
          BAD (WHERE,
	       _("6-bit unsigned offset is not a multiple of 4 (0x%08lx)"),
	       val);
	  break;
	}
      opcode |= (val << 6);
      break;

    case BFD_RELOC_TRICORE_2OFF:
      if (val & ~3)
        {
          BAD (WHERE, _("2-bit unsigned value overflow (0x%08lx)"), val);
	  break;
	}
      opcode |= (val << 6);
      break;

    case BFD_RELOC_TRICORE_8CONST2:
      if (val & ~1023)
        {
	  BAD (WHERE, _("10-bit unsigned offset overflow (0x%08lx)"), val);
	  break;
	}
      else if (val & 3)
        {
	  BAD (WHERE,
	       _("10-bit unsigned offset is not a multiple of 4 (0x%08lx)"),
	       val);
	  break;
	}
      opcode |= (val << 6);
      break;

    case BFD_RELOC_TRICORE_GOT:
    case BFD_RELOC_TRICORE_GOT2:
    case BFD_RELOC_TRICORE_GOTHI:
    case BFD_RELOC_TRICORE_GOTLO:
    case BFD_RELOC_TRICORE_GOTLO2:
    case BFD_RELOC_TRICORE_GOTUP:
    case BFD_RELOC_TRICORE_GOTOFF:
    case BFD_RELOC_TRICORE_GOTOFF2:
    case BFD_RELOC_TRICORE_GOTOFFHI:
    case BFD_RELOC_TRICORE_GOTOFFLO:
    case BFD_RELOC_TRICORE_GOTOFFLO2:
    case BFD_RELOC_TRICORE_GOTOFFUP:
    case BFD_RELOC_TRICORE_GOTPC:
    case BFD_RELOC_TRICORE_GOTPC2:
    case BFD_RELOC_TRICORE_GOTPCHI:
    case BFD_RELOC_TRICORE_GOTPCLO:
    case BFD_RELOC_TRICORE_GOTPCLO2:
    case BFD_RELOC_TRICORE_GOTPCUP:
      BAD (WHERE, _("GOT-relocation not allowed for constants"));
      break;

    case BFD_RELOC_TRICORE_PLT:
      BAD (WHERE, _("Illegal prefix (plt:) for constant offset"));
      break;

    default:
      BAD (WHERE, _("Internal error: can't handle reloc %d"), reloc);
      break;
    }

  /* Write back the fixed-up insn.  */
  md_number_to_chars (buf, opcode, len32 ? 4 : 2);
  fixP->fx_done = 1;

  if (show_internals)
    {
      if (len32)
        printf ("*** new fixed opcode: 0x%08lx\n", opcode);
      else
        printf ("*** new fixed opcode: 0x%04lx\n", opcode);
    }

#undef BAD
#undef WHERE
#undef CHECK_DISPLACEMENT
}

/* Translate the internal representation of a relocation info (pointed to
   by FIXP) to the BFD target format; SECTION points to the section which
   contains the relocation.  */

arelent *
tc_gen_reloc (section, fixP)
     asection *section ATTRIBUTE_UNUSED;
     fixS *fixP;
{
  arelent *reloc;
  enum bfd_reloc_code_real code = fixP->fx_r_type;

  if (show_internals)
    {
      char *buf = fixP->fx_where + fixP->fx_frag->fr_literal;

      printf ("*** tc_gen_reloc (%lx, %s#%d, %s)\n", (valueT) buf,
              fixP->fx_file, fixP->fx_line, GET_RELOC_NAME (code));
    }

  reloc = (arelent *) xmalloc (sizeof (arelent));
  reloc->sym_ptr_ptr = (asymbol **) xmalloc (sizeof (asymbol *));
  *reloc->sym_ptr_ptr = symbol_get_bfdsym (fixP->fx_addsy);
  reloc->address = fixP->fx_frag->fr_address + fixP->fx_where;
  reloc->addend = fixP->fx_addnumber;
  reloc->howto = bfd_reloc_type_lookup (stdoutput, code);
  if (!reloc->howto)
    {
      as_bad_where (fixP->fx_file, fixP->fx_line,
      		    "Internal error: don't know howto reloc type %d", code);
      return (arelent *) 0;
    }

  return reloc;
}

/* Helper function for tricore_sort_relocs: given any two relocations R1
   and R2 for the same section, return -1 or 1, depending on whether R1
   should appear before or after R2 in the final list of relocations for
   this section.  Note that we must preserve the order in case of several
   relocation entries for the same instruction (and thus the same address
   within the section) because of "pseudo" relocs like R_TRICORE_BITPOS,
   which must precede an "actual" reloc that effects an instruction fixup;
   so if R1 and R2 address the same instruction, the one appearing first
   in the list of fixups is considered "smaller" than the other one.  */

static int
compare_relocs (r1, r2)
     const void *r1, *r2;
{
  const rel_entry *rel1 = r1, *rel2 = r2;
  int retval = -1;

  if ((rel1->reloc->address > rel2->reloc->address)
      || ((rel1->reloc->address == rel2->reloc->address)
          && (rel1->index > rel2->index)))
    retval = 1;

#ifdef REVERSE_SORT_RELOCS
  return -retval;
#else
  return retval;
#endif
}

/* RELOCS is an array containing N pointers to relocation entries for SECTION;
   sort them in ascending (or descending, #ifdef REVERSE_SORT_RELOCS) order,
   just in case some utility expects them to be sorted by addresses.  Usually,
   this sorting is done automatically, because GAS emits relocations in the
   same order as the fixups requested by calls to fix_new or fix_new_exp.  
   However, this doesn't work here, because for relaxable instructions, we
   cannot create fixups until we know the final instruction -- which is only
   determined in md_convert_frag --, and since fixups for non-relaxable insns
   will be created immediately in emit_code, the resulting relocs will appear
   before the relocs for relaxable insns, thus breaking the "natural" order.  */

void
tricore_sort_relocs (section, relocs, n)
     asection *section;
     arelent **relocs;
     unsigned int n;
{
  rel_entry *srelocs;
  unsigned int i;

  /* Since the "natural" sort order can only be spoiled by relaxable
     instructions, we only need to consider actual code sections.  */
  if (!(section->flags & SEC_CODE) || (n < 2))
    return;

  if (show_internals)
    printf ("*** Sorting %u relocs for section \"%s\"\n", n, section->name);

  /* If there are several relocs for the same instruction, their order
     is significant (see compare_relocs above for an explanation), so
     we need to remember their original position.  */
  srelocs = (rel_entry *) xmalloc (n * sizeof (rel_entry));
  for (i = 0; i < n; ++i)
    {
      srelocs[i].reloc = relocs[i];
      srelocs[i].index = i;
    }

  /* Now sort the entries...  */
  qsort (srelocs, n, sizeof (rel_entry), compare_relocs);

  /* ...and put them back into the relocs array in their new order.  */
  for (i = 0; i < n; ++i)
    relocs[i] = srelocs[i].reloc;

  free (srelocs);
}

/* Round up a section size to the appropriate boundary.  */

valueT
md_section_align (segment, size)
     segT segment;
     valueT size;
{
  unsigned int align = bfd_section_alignment(segment);

  return ((size + (1 << align) - 1) & (~0U << align));
}

/* Exactly what point is a PC-relative offset relative to?  On the
   TriCore, it's relative to the start address of the insn itself.
   However, if the fixup references a symbol living in a different
   section, we just return zero, as the linker (or more precisely,
   the BFD function elf32-tricore.c:tricore_elf32_relocate_section)
   will subtract the offset from the insn to the beginning of the
   section from the relocation value.  That way, the r_addend value
   of a reloc entry produced by the assembler actually contains the
   fixup's fx_offset value, and not fx_offset - "seg_off (insn)".
   For the PCP, however, the offset is always relative to the next insn.  */

long
md_pcrel_from_section(fixS* fixP, segT segment)
{
//  if (segment->flags & PCP_SEG)
//    return fixP->fx_frag->fr_address + fixP->fx_where + 2;

  if ((fixP->fx_subsy != (symbolS *) 0)
      || ((fixP->fx_addsy != (symbolS *) 0)
          && (((S_GET_SEGMENT (fixP->fx_addsy) == segment))
              || (S_GET_SEGMENT (fixP->fx_addsy) == absolute_section))
	  && S_IS_DEFINED (fixP->fx_addsy)
	  && !S_IS_EXTERNAL (fixP->fx_addsy)
	  && !S_IS_WEAK (fixP->fx_addsy)))
    /* Return "seg_off (insn)", i.e., the start address of the insn
       relative to the beginning of this segment.  */
    return fixP->fx_frag->fr_address + fixP->fx_where;

  return 0;
}

/* See if a fix can be adjusted (i.e., locally resolved by using the
   offset relative to the beginning of the section in which a symbol
   is defined).  */

int
tricore_fix_adjustable (fixP)
     fixS *fixP;
{
  if (fixP->fx_addsy == NULL)
    return 1;

  /* Prevent all adjustments to global or weak symbols, or symbols in
     merge sections.  */
  if ((S_IS_EXTERNAL (fixP->fx_addsy)
       || (S_IS_WEAK (fixP->fx_addsy))
       || (S_GET_SEGMENT (fixP->fx_addsy)->flags & SEC_MERGE) != 0))
    return 0;

  /* We need the symbol name for the VTABLE entries.  */
  if ((fixP->fx_r_type == BFD_RELOC_VTABLE_INHERIT)
      || (fixP->fx_r_type == BFD_RELOC_VTABLE_ENTRY))
    return 0;

  /* GOT/PLT relocations can't be adjusted, either.  */
  if ((fixP->fx_r_type == BFD_RELOC_TRICORE_GOT)
      || (fixP->fx_r_type == BFD_RELOC_TRICORE_GOT2)
      || (fixP->fx_r_type == BFD_RELOC_TRICORE_GOTHI)
      || (fixP->fx_r_type == BFD_RELOC_TRICORE_GOTLO)
      || (fixP->fx_r_type == BFD_RELOC_TRICORE_GOTLO2)
      || (fixP->fx_r_type == BFD_RELOC_TRICORE_GOTUP)
      || (fixP->fx_r_type == BFD_RELOC_TRICORE_GOTOFF)
      || (fixP->fx_r_type == BFD_RELOC_TRICORE_GOTOFF2)
      || (fixP->fx_r_type == BFD_RELOC_TRICORE_GOTOFFHI)
      || (fixP->fx_r_type == BFD_RELOC_TRICORE_GOTOFFLO)
      || (fixP->fx_r_type == BFD_RELOC_TRICORE_GOTOFFLO2)
      || (fixP->fx_r_type == BFD_RELOC_TRICORE_GOTOFFUP)
      || (fixP->fx_r_type == BFD_RELOC_TRICORE_GOTPC)
      || (fixP->fx_r_type == BFD_RELOC_TRICORE_GOTPC2)
      || (fixP->fx_r_type == BFD_RELOC_TRICORE_GOTPCLO)
      || (fixP->fx_r_type == BFD_RELOC_TRICORE_GOTPCLO2)
      || (fixP->fx_r_type == BFD_RELOC_TRICORE_GOTPCHI)
      || (fixP->fx_r_type == BFD_RELOC_TRICORE_GOTPCUP)
      || (fixP->fx_r_type == BFD_RELOC_TRICORE_PLT))
    return 0;

  return 1;
}

/* Emit relocations for references to VTABLE entries and global or
   weak symbols.  */

int
tricore_force_relocation (fixP)
     fixS *fixP;
{
  if ((fixP->fx_r_type == BFD_RELOC_VTABLE_INHERIT)
      || (fixP->fx_r_type == BFD_RELOC_VTABLE_ENTRY))
    return 1;

  if ((fixP->fx_addsy != NULL)
      && (S_IS_EXTERNAL (fixP->fx_addsy) || S_IS_WEAK (fixP->fx_addsy)))
    return 1;

  return 0;
}

/* Set the machine type in the header of a TriCore ELF file.  */

void
tricore_elf_final_processing ()
{
  elf_elfheader (stdoutput)->e_flags = current_isa;
}

/* Parse the PCP instruction starting at STR and fill pcp_insn.  */

// static void
// pcp_ip (const char* str)
// {
//   char *src, *dst, *equ, *pos, *cp;
//   int numops = -1;
//   size_t str_len;
//   int indirect;
//   const char *flags = " cnc cnt0 size st ec int ep dac rta rna eda sdb";
//   const char *flops = ".c4 .n4  .f5  .g3.h3.i4 .j3.m4 .o4 .o4 .p4 .q4 ";

//   if ((str_len = strlen (str)) >= insnline_len)
//     {
//       insnline_len = str_len + 1;
//       insnline = xrealloc (insnline, insnline_len);
//     }

//   /* Make a lower-case-only copy of the input line.  */
//   for (src = (char*) str, dst = insnline; *src; /* Empty.  */)
//     if ((*src >= 'A') && (*src <= 'Z'))
//       *dst++ = (*src++) - 'A' + 'a';
//     else
//       *dst++ = *src++;
//   *dst = '\0';

//   /* Lookup the instruction name.  */
//   dst = strtok (insnline, ", ");

//   if ((pcp_insn.code = (pcp_opcode_t *) str_hash_find (hash_pcp, insnline)) == NULL)
//     {
//       pcp_insn.error = _("Unknown PCP instruction");
//       return;
//     }

//   /* Parse the operands.  */
//   while ((dst = strtok (NULL, ", ")) != NULL)
//     {
//       if (++numops == MAX_PCPOPS)
//         {
// 	  pcp_insn.error = _("Too many operands");
// 	  return;
// 	}

//       /* Check if indirection mode.  */
//       indirect = 0;
//       if (*dst == '[')
//         { ++dst;
// 	  indirect = 1;
// 	}

//       /* Check if it's a register.  */
//       if ((*dst == 'r') && ((dst[1] >= '0') && (dst[1] <= '7')))
//         {
// 	  if (indirect && (dst[2] == ']'))
// 	    {
// 	      if (dst[3] == '\0')
// 	        {
// 		  pcp_insn.ops[numops] = 'R';
// 		  PCPVAL (numops) = dst[1] - '0';
// 		  continue;
// 		}
// 	      else
// 	        {
// 		  pcp_insn.error = _("Trailing chars after register indirect");
// 		  return;
// 		}
// 	    }
// 	  else if (!indirect && (dst[2] == '\0'))
// 	    {
// 	      pcp_insn.ops[numops] = 'r';
// 	      PCPVAL (numops) = dst[1] - '0';
// 	      continue;
// 	    }
// 	}

//       /* Check if it's src{+-} or dst{+-}.  */
//       if ((*dst == 's') && (dst[1] == 'r') && (dst[2] == 'c'))
//         {
// 	  if (dst[3] == '\0')
// 	    {
// 	      if (indirect)
// 	        {
// invalid_indmode:
// 		  pcp_insn.error = _("Invalid indirect address mode");
// 		  return;
// 		}
// 	      pcp_insn.ops[numops] = 's';
// 	      PCPVAL (numops) = 0;
// 	      continue;
// 	    }
// 	  else if ((dst[3] == ']') || (dst[4] == ']'))
// 	    {
// 	      if (indirect)
// 	        goto invalid_indmode;
// 	      else
// 	        {
// 		  pcp_insn.error = _("Illegal char ']' after SRC{+-}");
// 		  return;
// 		}
// 	    }
// 	  else if ((dst[3] == '+') && (dst[4] == '\0'))
// 	    {
// 	      if (indirect)
// 	        goto invalid_indmode;

// 	      pcp_insn.ops[numops] = 's';
// 	      PCPVAL (numops) = 1;
// 	      continue;
// 	    }
// 	  else if ((dst[3] == '-') && (dst[4] == '\0'))
// 	    {
// 	      if (indirect)
// 	        goto invalid_indmode;

// 	      pcp_insn.ops[numops] = 's';
// 	      PCPVAL (numops) = 2;
// 	      continue;
// 	    }
// 	}
//       else if ((*dst == 'd') && (dst[1] == 's') && (dst[2] == 't'))
//         {
// 	  if (dst[3] == '\0')
// 	    {
// 	      if (indirect)
// 	        goto invalid_indmode;

// 	      pcp_insn.ops[numops] = 'd';
// 	      PCPVAL (numops) = 0;
// 	      continue;
// 	    }
// 	  else if ((dst[3] == ']') || (dst[4] == ']'))
// 	    {
// 	      if (indirect)
// 	        goto invalid_indmode;
// 	      else
// 	        {
// 		  pcp_insn.error = _("Illegal char ']' after DST{+-}");
// 		  return;
// 		}
// 	    }
// 	  else if ((dst[3] == '+') && (dst[4] == '\0'))
// 	    {
// 	      if (indirect)
// 	        goto invalid_indmode;

// 	      pcp_insn.ops[numops] = 'd';
// 	      PCPVAL (numops) = 1;
// 	      continue;
// 	    }
// 	  else if ((dst[3] == '-') && (dst[4] == '\0'))
// 	    {
// 	      if (indirect)
// 	        goto invalid_indmode;

// 	      pcp_insn.ops[numops] = 'd';
// 	      PCPVAL (numops) = 2;
// 	      continue;
// 	    }
// 	}

//       /* Check if it's a condition code.  */
//       if ((*dst == 'c') && (dst[1] == 'c') && (dst[2] == '_')
//           && (dst[3] != '\0'))
//         {
// 	  if ((dst[4] == '\0') || (dst[4] == ']'))
// 	    {
// 	      if (dst[3] == 'z')
// 	        {
// 		  if (indirect || (dst[4] == ']'))
// 		    goto invalid_indmode;

// 		  pcp_insn.ops[numops] = 'a';
// 		  PCPVAL (numops) = 1;
// 		  continue;
// 		}
// 	      if (dst[3] == 'c')
// 	        {
// 		  if (indirect || (dst[4] == ']'))
// 		    goto invalid_indmode;

// 		  pcp_insn.ops[numops] = 'a';
// 		  PCPVAL (numops) = 4;
// 		  continue;
// 		}
// 	      if (dst[3] == 'v')
// 	        {
// 		  if (indirect || (dst[4] == ']'))
// 		    goto invalid_indmode;

// 		  pcp_insn.ops[numops] = 'a';
// 		  PCPVAL (numops) = 3;
// 		  continue;
// 		}
// 	      if (dst[3] == 'n')
// 	        {
// 		  if (indirect || (dst[4] == ']'))
// 		    goto invalid_indmode;

// 		  pcp_insn.ops[numops] = 'b';
// 		  PCPVAL (numops) = 8;
// 		  continue;
// 		}
// 	    }
// 	  else if ((dst[5] == '\0') || (dst[5] == ']'))
// 	    {
// 	      if ((dst[3] == 'u') && (dst[4] == 'c'))
// 	        {
// 		  if (indirect || (dst[5] == ']'))
// 		    goto invalid_indmode;

// 		  pcp_insn.ops[numops] = 'a';
// 		  PCPVAL (numops) = 0;
// 		  continue;
// 		}
// 	      if (dst[3] == 'n')
// 	        {
// 	          if (dst[4] == 'z')
// 		    {
// 		      if (indirect || (dst[5] == ']'))
// 		        goto invalid_indmode;

// 		      pcp_insn.ops[numops] = 'a';
// 		      PCPVAL (numops) = 2;
// 		      continue;
// 		    }
// 	          if (dst[4] == 'c')
// 		    {
// 		      if (indirect || (dst[5] == ']'))
// 		        goto invalid_indmode;

// 		      pcp_insn.ops[numops] = 'b';
// 		      PCPVAL (numops) = 11;
// 		      continue;
// 		    }
// 	          if (dst[4] == 'n')
// 		    {
// 		      if (indirect || (dst[5] == ']'))
// 		        goto invalid_indmode;

// 		      pcp_insn.ops[numops] = 'b';
// 		      PCPVAL (numops) = 9;
// 		      continue;
// 		    }
// 	          if (dst[4] == 'v')
// 		    {
// 		      if (indirect || (dst[5] == ']'))
// 		        goto invalid_indmode;

// 		      pcp_insn.ops[numops] = 'b';
// 		      PCPVAL (numops) = 10;
// 		      continue;
// 		    }
// 		}
// 	    }
// 	  else if ((dst[6] == '\0') || (dst[6] == ']'))
// 	    {
// 	      if (dst[3] == 'u')
// 	        {
// 		  if ((dst[4] == 'l') && (dst[5] == 't'))
// 		    {
// 		      if (indirect || (dst[6] == ']'))
// 		        goto invalid_indmode;

// 		      pcp_insn.ops[numops] = 'a';
// 		      PCPVAL (numops) = 4;
// 		      continue;
// 		    }
// 		  if (dst[4] == 'g')
// 		    {
// 		      if (dst[5] == 't')
// 		        {
// 			  if (indirect || (dst[6] == ']'))
// 			    goto invalid_indmode;

// 			  pcp_insn.ops[numops] = 'a';
// 			  PCPVAL (numops) = 5;
// 			  continue;
// 			}
// 		      if (dst[5] == 'e')
// 		        {
// 			  if (indirect || (dst[6] == ']'))
// 			    goto invalid_indmode;

// 			  pcp_insn.ops[numops] = 'b';
// 			  PCPVAL (numops) = 11;
// 			  continue;
// 			}
// 		    }
// 		}
// 	      else if (dst[3] == 's')
// 	        {
// 		  if (dst[4] == 'l')
// 		    {
// 		      if (dst[5] == 't')
// 		        {
// 			  if (indirect || (dst[6] == ']'))
// 			    goto invalid_indmode;

// 			  pcp_insn.ops[numops] = 'a';
// 			  PCPVAL (numops) = 6;
// 			  continue;
// 			}
// 		      if (dst[5] == 'e')
// 		        {
// 			  if (indirect || (dst[6] == ']'))
// 			    goto invalid_indmode;

// 			  pcp_insn.ops[numops] = 'b';
// 			  PCPVAL (numops) = 13;
// 			  continue;
// 			}
// 		    }
// 		  else if (dst[4] == 'g')
// 		    {
// 		      if (dst[5] == 't')
// 		        {
// 			  if (indirect || (dst[6] == ']'))
// 			    goto invalid_indmode;

// 			  pcp_insn.ops[numops] = 'a';
// 			  PCPVAL (numops) = 7;
// 			  continue;
// 			}
// 		      if (dst[5] == 'e')
// 		        {
// 			  if (indirect || (dst[6] == ']'))
// 			    goto invalid_indmode;

// 			  pcp_insn.ops[numops] = 'b';
// 			  PCPVAL (numops) = 12;
// 			  continue;
// 			}
// 		    }
// 		}
// 	      else if ((dst[3] == 'c') && (dst[4] == 'n'))
// 	        {
// 		  if (dst[5] == 'z')
// 		    {
// 		      if (indirect || (dst[6] == ']'))
// 		        goto invalid_indmode;

// 		      pcp_insn.ops[numops] = 'b';
// 		      PCPVAL (numops) = 14;
// 		      continue;
// 		    }
// 		  if (dst[5] == 'n')
// 		    {
// 		      if (indirect || (dst[6] == ']'))
// 		        goto invalid_indmode;

// 		      pcp_insn.ops[numops] = 'b';
// 		      PCPVAL (numops) = 15;
// 		      continue;
// 		    }
// 		}
// 	    }
// 	}

//       /* Check if it's set or clr.  */
//       if ((*dst == 's') && (dst[1] == 'e') && (dst[2] == 't'))
//         {
// 	  if (dst[3] == '\0')
// 	    {
// 	      if (indirect)
// 	        goto invalid_indmode;

// 	      pcp_insn.ops[numops] = 'k';
// 	      PCPVAL (numops) = 1;
// 	      continue;
// 	    }
// 	  else if (dst[3] == ']')
// 	    {
// 	      if (indirect)
// 	        goto invalid_indmode;
// 	      else
// 	        {
// 		  pcp_insn.error = _("Illegal char ']' after SET");
// 		  return;
// 		}
// 	    }
// 	}
//       else if ((*dst == 'c') && (dst[1] == 'l') && (dst[2] == 'r'))
//         {
// 	  if (dst[3] == '\0')
// 	    {
// 	      if (indirect)
// 	        goto invalid_indmode;

// 	      pcp_insn.ops[numops] = 'l';
// 	      PCPVAL (numops) = 0;
// 	      continue;
// 	    }
// 	  else if (dst[3] == ']')
// 	    {
// 	      if (indirect)
// 	        goto invalid_indmode;
// 	      else
// 	        {
// 		  pcp_insn.error = _("Illegal char ']' after CLR");
// 		  return;
// 		}
// 	    }
// 	}

//       /* Check if it's a flag=value pair.  */
//       if ((dst[1] != '\0')
//           && ((dst[2] == '=')
// 	      || ((dst[2] != '\0')
// 	          && ((dst[3] == '=')
// 		      || ((dst[3] != '\0')
// 		          && (dst[4] == '='))))))
//         {
// 	  if (dst[2] == '=')
// 	    equ = &dst[2];
// 	  else if (dst[3] == '=')
// 	    equ = &dst[3];
// 	  else
// 	    equ = &dst[4];
// 	  *equ = '\0';
//           if (((pos = strstr (flags, dst)) != NULL) && (pos[-1] == ' '))
//             {
// 	      cp = (char *) &flops[(int) (pos - flags) - 1];
// 	      if (*cp++ == '.')
// 	        {
// 	          if (indirect)
// 	            goto invalid_indmode;

// 	          pcp_insn.ops[numops] = *cp++;
// 		  if (get_pcp_expression (str, equ + 1, numops, 0))
// 		    continue;
// 		  else
// 		    return;
// 	        }
// 	    }
// 	  *equ = '=';
// 	}

//       /* Check if it's a constant, a symbol or a complex expression.  */
//       if (!get_pcp_expression (str, dst, numops, indirect))
//         return;
//     }

//   /* This is because we started with numops = -1.  */
//   pcp_insn.nops = ++numops;

//   /* Convert "jc offset6,cc_uc" to "jl offset10".  */
//   if (optimize_next
//       && (numops == 2)
//       && !strcmp (pcp_insn.code->name, "jc")
//       && (((pcp_insn.ops[0] == 'a') && (PCPVAL (0) == 0))
//           || ((pcp_insn.ops[1] == 'a') && (PCPVAL (1) == 0))))
//     {
//       if ((pcp_insn.code = (pcp_opcode_t *) str_hash_find (hash_pcp, "jl")) == NULL)
//         {
//           pcp_insn.error = _("Internal error: cannot lookup PCP insn \"jl\"");
//           return;
//         }
//       pcp_insn.nops = 1;
//       if (pcp_insn.ops[0] == 'a')
//         {
//   	  pcp_insn.ops[0] = pcp_insn.ops[1];
// 	  pcp_insn.prefix[0] = pcp_insn.prefix[1];
// 	  pcp_insn.opexp[0] = pcp_insn.opexp[1];
// 	}

//       if (show_internals)
//         printf ("*** jc cc_uc,soff6 -> jl soff10 (OPT)\n");
// #if EXT_LISTING
//       if (listing & LISTING_LISTING)
//         {
//           snprintf (EXT_BUF, _("Optimizing jc cc_uc,soff6 -> jl soff10"));
//           ASM_NOTICE ();
//         }
// #endif /* EXT_LISTING  */
//     }
// }

/* Parse an expression for PCP operand OPNR starting at STR (SRC points
   to the original input line).  If IND is non-zero, the expression must
   be followed by "]\0", otherwise there must be no trailing chars.  If
   no valid expression can be found, or if the numerical value of the
   expression exceeds an operand-specific range, 0 is returned and
   pcp_insn.error is set to an appropriate error message, otherwise 1 is
   returned.  */

// static int
// get_pcp_expression (const char* src, char* str, int opnr, int ind)
// {
//   char *save_in, *save_oldpos;
  
//   /* Filter out prefixes.  */
//   if (!strncmp (str, "dptr:", 5))
//     {
//       pcp_insn.prefix[opnr] = PCP_PREFIX_DPTR;
//       str += 5;
//     }
//   else if (!strncmp (str, "doff:", 5))
//     {
//       pcp_insn.prefix[opnr] = PCP_PREFIX_DOFF;
//       str += 5;
//     }
//   else if (!strncmp (str, "cptr:", 5))
//     {
//       pcp_insn.prefix[opnr] = PCP_PREFIX_CPTR;
//       str += 5;
//     }

//   save_in = input_line_pointer;
//   save_oldpos = input_line_pointer = (char *)src + (str - insnline);
//   expression (&pcp_insn.opexp[opnr]);
//   str += (input_line_pointer - save_oldpos);
//   input_line_pointer = save_in;

//   if (ind)
//     {
//       if (*str != ']')
//         {
// 	  pcp_insn.error = _("Missing ']'");
// 	  return 0;
// 	}
//       if (str[1] != '\0')
//         {
// 	  pcp_insn.error = _("Trailing chars after indirect expression");
// 	  return 0;
// 	}
//     }
//   else if (*str != '\0')
//     {
//       pcp_insn.error = _("Trailing chars after expression");
//       return 0;
//     }

//   if (pcp_insn.opexp[opnr].X_op == O_absent)
//     {
//       pcp_insn.error = _("Illegal expression");
//       return 0;
//     }

//   if (pcp_insn.ops[opnr] != '\0')
//     {
//       offsetT val = PCPVAL (opnr);

//       if (pcp_insn.opexp[opnr].X_op != O_constant)
//         {
// 	  pcp_insn.error = _("Non-constant expression");
// 	  return 0;
// 	}
//       switch (pcp_insn.ops[opnr])
//         {
// 	case 'c':
// 	  if ((val < 0) || (val > 2))
// 	    {
// 	      pcp_insn.error = _("CNC value out of range [0..2]");
// 	      return 0;
// 	    }
// 	  break;

// 	case 'f':
// 	  if ((val != 8) && (val != 16) && (val != 32))
// 	    {
// 	      pcp_insn.error = _("Illegal SIZE value (must be 8, 16, or 32)");
// 	      return 0;
// 	    }
// 	  else if (val == 8)
// 	    PCPVAL (opnr) = 0;
// 	  else if (val == 16)
// 	    PCPVAL (opnr) = 1;
// 	  else
// 	    PCPVAL (opnr) = 2;
// 	  break;

// 	case 'g':
// 	  if ((val != 0) && (val != 1))
// 	    {
// 	      pcp_insn.error = _("Illegal ST value (must be 0 or 1)");
// 	      return 0;
// 	    }
// 	  break;

// 	case 'h':
// 	  if ((val != 0) && (val != 1))
// 	    {
// 	      pcp_insn.error = _("Illegal EC value (must be 0 or 1)");
// 	      return 0;
// 	    }
// 	  break;

// 	case 'i':
// 	  if ((val != 0) && (val != 1))
// 	    {
// 	      pcp_insn.error = _("Illegal INT value (must be 0 or 1)");
// 	      return 0;
// 	    }
// 	  break;

// 	case 'j':
// 	  if ((val != 0) && (val != 1))
// 	    {
// 	      pcp_insn.error = _("Illegal EP value (must be 0 or 1)");
// 	      return 0;
// 	    }
// 	  break;

// 	case 'm':
// 	  if ((val != 0) && (val != 1))
// 	    {
// 	      pcp_insn.error = _("Illegal DAC value (must be 0 or 1)");
// 	      return 0;
// 	    }
// 	  break;

// 	case 'n':
// 	  if (!strcmp (pcp_insn.code->name, "copy"))
// 	    {
// 	      if ((val < 1) || (val > 8))
// 	        {
// 	          pcp_insn.error = _("CNT0 value out of range [1..8] for COPY");
// 	          return 0;
// 		}
// 	      else
// 	        PCPVAL (opnr) = val & 0x7;
// 	    }
// 	  else if (!strcmp (pcp_insn.code->name, "bcopy"))
// 	    {
// 	      if ((val != 2) && (val != 4) && (val != 8))
// 	        {
// 		  pcp_insn.error = _("Illegal CNT0 value (must be 2, 4, or 8)");
// 		  return 0;
// 		}
// 	      else if (val == 4)
// 	        PCPVAL (opnr) = 3;
// 	      else if (val == 8)
// 	        PCPVAL (opnr) = 0;
// 	    }
// 	  else
// 	    {
// 	      pcp_insn.error = _("CNT0 only valid for [B]COPY instructions");
// 	      return 0;
// 	    }
// 	  break;

// 	case 'o':
// 	  if ((val != 0) && (val != 1))
// 	    {
// 	      pcp_insn.error = _("Illegal RTA value (must be 0 or 1)");
// 	      return 0;
// 	    }
// 	  break;

// 	case 'p':
// 	  if ((val != 0) && (val != 1))
// 	    {
// 	      pcp_insn.error = _("Illegal EDA value (must be 0 or 1)");
// 	      return 0;
// 	    }
// 	  break;

// 	case 'q':
// 	  if ((val != 0) && (val != 1))
// 	    {
// 	      pcp_insn.error = _("Illegal SDB value (must be 0 or 1)");
// 	      return 0;
// 	    }
// 	  break;
// 	}
//     }
//   else
//     {
//       if (pcp_insn.opexp[opnr].X_op == O_constant)
// 	pcp_insn.ops[opnr] = ind ? 'E' : 'e';
//       else
//         pcp_insn.ops[opnr] = ind ? 'U' : 'u';
//     }

//   return 1;
// }

/* Find an opcode that matches pcp_insn's opcode/operands.  */

// static pcp_opcode_t *
// find_pcp_opcode ()
// {
//   pcp_opcode_t *pop;
//   int i, op, nops;
//   char ops[MAX_PCPOPS + 1], *pos;

//   pop = pcp_insn.code;
//   nops = pcp_insn.nops;

//   for (i = pcp_insn.code->insind; i <= pcp_insn.code->inslast; ++pop, ++i)
//     {
//       if (pop->nr_operands != nops)
//         continue;

//       if (pop->ooo)
//         {
// 	  for (op = 0; op < nops; ++op)
// 	    ops[op] = pcp_insn.ops[op];
// 	  ops[nops] = '\0';

//           for (op = 0; op < nops; ++op)
// 	    if (((pos = strchr (ops, pop->args[op])) == NULL)
// 	        && !((pop->args[op] == 'e') && (pos = strchr (ops, 'u')))
// 		&& !((pop->args[op] == 'E') && (pos = strchr (ops, 'U'))))
// 	      break;
// 	    else
// 	      strcpy (pos, pos + 1);
// 	}
//       else
//         {
// 	  for (op = 0; op < nops; ++op)
// 	    if ((pcp_insn.ops[op] != pop->args[op])
// 	        && !((pop->args[op] == 'e') && (pcp_insn.ops[op] == 'u'))
// 		&& !((pop->args[op] == 'E') && (pcp_insn.ops[op] == 'U')))
// 	      break;
// 	}

//       if (op == nops)
//         return pop;
//     }

//   /* We couldn't find a valid opcode.  */
//   return (pcp_opcode_t *) NULL;
// }

/* Print the contents of pcp_insn.  Used for debugging.  */

// static void
// print_pcp_insn ()
// {
//   int i;

//   printf ("<%s \"%s\"> ", pcp_insn.code->name, pcp_insn.code->args);
//   if (pcp_insn.code->len32)
//     printf ("0x%08lx ", pcp_insn.opcode);
//   else
//     printf ("0x%04lx ", pcp_insn.opcode);

//   for (i = 0; i < pcp_insn.nops; ++i)
//     {
//       switch (pcp_insn.prefix[i])
//         {
// 	case PCP_PREFIX_NONE:
// 	  break;

// 	case PCP_PREFIX_DPTR:
// 	  printf ("dptr:");
// 	  break;

// 	case PCP_PREFIX_DOFF:
// 	  printf ("doff:");
// 	  break;

// 	case PCP_PREFIX_CPTR:
// 	  printf ("cptr:");
// 	  break;
// 	}

//       switch (pcp_insn.ops[i])
//         {
// 	case 'a':
// 	  printf ("CONDCA (%ld, cc_%s)", PCPVAL (i), pcp_ccodes[PCPVAL (i)]);
// 	  break;

// 	case 'b':
// 	  printf ("CONDCB (%ld, cc_%s)", PCPVAL (i), pcp_ccodes[PCPVAL (i)]);
// 	  break;

// 	case 'c':
// 	  printf ("CNC=(%ld)", PCPVAL (i));
// 	  break;

// 	case 'd':
// 	  printf ("DST");
// 	  if (PCPVAL (i) == 1)
// 	    printf ("+");
// 	  else if (PCPVAL (i) == 2)
// 	    printf ("-");
// 	  break;

// 	case 'e':
// 	  printf ("EXPR (%ld,0x%08lx)", PCPVAL (i), PCPVAL (i));
// 	  break;

// 	case 'E':
// 	  printf ("[EXPR] (%ld,0x%08lx)", PCPVAL (i), PCPVAL (i));
// 	  break;

// 	case 'f':
// 	  printf ("SIZE=(%ld)", PCPVAL (i));
// 	  break;

// 	case 'g':
// 	  printf ("ST=(%ld)", PCPVAL (i));
// 	  break;

// 	case 'h':
// 	  printf ("EC=(%ld)", PCPVAL (i));
// 	  break;

// 	case 'i':
// 	  printf ("INT=(%ld)", PCPVAL (i));
// 	  break;

// 	case 'j':
// 	  printf ("EP=(%ld)", PCPVAL (i));
// 	  break;

// 	case 'k':
// 	  printf ("SET (%ld)", PCPVAL (i));
// 	  break;

// 	case 'l':
// 	  printf ("CLR (%ld)", PCPVAL (i));
// 	  break;

// 	case 'm':
// 	  printf ("DAC=(%ld)", PCPVAL (i));
// 	  break;

// 	case 'n':
// 	  printf ("CNT0=(%ld)", PCPVAL (i));
// 	  break;

// 	case 'o':
// 	  printf ("RTA=(%ld)", PCPVAL (i));
// 	  break;

// 	case 'p':
// 	  printf ("EDA=(%ld)", PCPVAL (i));
// 	  break;

// 	case 'q':
// 	  printf ("SDB=(%ld)", PCPVAL (i));
// 	  break;

// 	case 'r':
// 	  printf ("REG (%ld)", PCPVAL (i));
// 	  break;

// 	case 'R':
// 	  printf ("[REG] (%ld)", PCPVAL (i));
// 	  break;

// 	case 's':
// 	  printf ("SRC");
// 	  if (PCPVAL (i) == 1)
// 	    printf ("+");
// 	  else if (PCPVAL (i) == 2)
// 	    printf ("-");
// 	  break;

// 	case 'u':
// 	  printf ("UNDF (%s)", S_GET_NAME (pcp_insn.opexp[i].X_add_symbol));
// 	  break;

// 	case 'U':
// 	  printf ("[UNDF] (%s)", S_GET_NAME (pcp_insn.opexp[i].X_add_symbol));
// 	  break;
// 	}
      
//       printf ("(%c)", pcp_insn.ops[i]);
//       if (!strcmp (pcp_insn.code->name, "jl")
//           || !strcmp (pcp_insn.code->name, "jc"))
//         printf ("(PCREL)");
//       if (i < (pcp_insn.nops - 1))
//         printf (", ");
//     }
//   printf ("\n");
// }

/* Encode the PCP instruction contained in pcp_insn.  Return 1 on
   error, or 0 on OK.  */

// static int
// pcp_encode ()
// {
//   int op;
//   pcp_opcode_t *pop = pcp_insn.code;
//   unsigned long *code = &pcp_insn.opcode;

//   switch (pop->fmt_group)
//     {
//     case 0:
//       for (op = 0; op < pcp_insn.nops; ++op)
//         switch (pcp_insn.ops[op])
// 	  {
// 	  case 'd': /* DST{+-}  */
// 	    *code |= (PCPVAL (op) << 9);
// 	    break;

// 	  case 's': /* SRC{+-}  */
// 	    *code |= (PCPVAL (op) << 7);
// 	    break;

// 	  case 'c': /* CNC=  */
// 	    *code |= (PCPVAL (op) << 5);
// 	    break;

// 	  case 'n': /* CNT0=  */
// 	    *code |= (PCPVAL (op) << 2);
// 	    break;

// 	  case 'f': /* SIZE=  */
// 	  case 'a': /* CONDCA  */
// 	  case 'b': /* CONDCB  */
// 	    *code |= PCPVAL (op);
// 	    break;

// 	  case 'g': /* ST=  */
// 	    *code |= (PCPVAL (op) << 10);
// 	    break;

// 	  case 'i': /* INT=  */
// 	    *code |= (PCPVAL (op) << 9);
// 	    break;

// 	  case 'j': /* EP=  */
// 	    *code |= (PCPVAL (op) << 8);
// 	    break;

// 	  case 'h': /* EC=  */
// 	    *code |= (PCPVAL (op) << 7);
// 	    break;

// 	  default:
// 	    as_fatal (_("Internal error: operand #%d is unknown"), op);
// 	    return 1;
// 	  }
//       break;

//     case 1:
//       *code |= ((PCPVAL (0) << 6) | (PCPVAL (1) << 3) | PCPVAL (2));
//       break;

//     case 2:
//       if ((PCPVAL (0) == 7)
//           && (!strcmp (pop->name, "mclr.pi") || !strcmp (pop->name, "mset.pi")))
//         {
// 	  as_bad (_("Register R7 not allowed for this instruction"));
// 	  return 1;
// 	}
//       if ((pcp_insn.ops[1] == 'u') || (pcp_insn.ops[1] == 'U'))
//         {
// 	  /* This is catched elsewhere (in pcp_emit_code), as we now permit
// 	     undefined operands for these insns and emit relocs for them.

// 	  as_bad (_("Unknown offset; must be a constant between 0 and 63"));
// 	  return 1;  */
// 	}
//       else if ((PCPVAL (1) < 0) || (PCPVAL (1) > 63))
//         {
// 	  as_bad (_("Offset %ld out of range [0..63]"), PCPVAL (1));
// 	  return 1;
// 	}
//       *code |= ((PCPVAL (0) << 6) | PCPVAL (1));
//       break;

//     case 3:
//       if (pcp_insn.ops[0] == 'r')
//         *code |= ((PCPVAL (0) << 6) | (PCPVAL (1) << 3) | PCPVAL (2));
//       else
//         *code |= ((PCPVAL (1) << 6) | (PCPVAL (2) << 3) | PCPVAL (0));
//       break;

//     case 4:
//       for (op = 0; op < pcp_insn.nops; ++op)
//         switch (pcp_insn.ops[op])
// 	  {
// 	  case 'r': /* R0-R7  */
// 	    *code |= (PCPVAL (op) << 6);
// 	    break;

// 	  case 'k': /* SET  */
// 	  case 'l': /* CLR  */
// 	    *code |= (PCPVAL (op) << 5);
// 	    break;

// 	  case 'e': /* const expr  */
// 	    if (!strcmp (pop->name, "ldl.iu") || !strcmp (pop->name, "ldl.il"))
// 	      break;  /* These are handled in pcp_emit code.  */
// 	    if (!strcmp (pop->name, "shr")
// 	        || !strcmp (pop->name, "shl")
// 		|| !strcmp (pop->name, "rr")
// 		|| !strcmp (pop->name, "rl"))
// 	      {
// 	        if ((PCPVAL (op) != 1) && (PCPVAL (op) != 2)
// 		    && (PCPVAL (op) != 4) && (PCPVAL (op) != 8))
// 		  {
// 		    as_bad (_("Illegal constant %ld; must be 1, 2, 4, or 8"),
// 		            PCPVAL (op));
// 		    return 1;
// 		  }
// 	      }
// 	    else if (!strcmp (pop->name, "set")
// 	    	     || !strcmp (pop->name, "clr")
// 		     || !strcmp (pop->name, "inb.i")
// 		     || !strcmp (pop->name, "chkb")
// 		     || !strcmp (pop->name, "exb")
// 		     || !strcmp (pop->name, "exib"))
// 	      {
// 	        if ((PCPVAL (op) < 0) || (PCPVAL (op) > 31))
// 		  {
// 		    as_bad (_("Constant %ld out of range [0..31]"),
// 		    	    PCPVAL (op));
// 		    return 1;
// 		  }
// 	      }
// 	    else if ((PCPVAL (op) < 0) || (PCPVAL (op) > 63))
// 	      {
// 	        as_bad (_("Constant %ld out of range [0..63]"), PCPVAL (op));
// 		return 1;
//               }
// 	    *code |= PCPVAL (op);
// 	    break;

// 	  case 'u': /* unknown  */
// 	    if (strcmp (pop->name, "ldl.iu") && strcmp (pop->name, "ldl.il"))
// 	      {
// 	        as_bad (_("Invalid expression; must be a constant"));
// 		return 1;
// 	      }
// 	    break;

// 	  default:
// 	    as_fatal (_("Internal error: operand #%d is unknown"), op);
// 	    return 1;
// 	  }
//       break;

//     case 5:
//       if (pcp_insn.ops[1] == 'u')
//         {
// 	  as_bad (_("Invalid expression; must be a constant"));
// 	  return 1;
// 	}
//       if (!strcmp (pop->name, "set.f") || !strcmp (pop->name, "clr.f"))
//         {
// 	  if ((PCPVAL (1) < 0) || (PCPVAL (1) >= (8 << PCPVAL (2))))
// 	    {
// 	      as_bad (_("Constant %ld out of range [0..%d]"), PCPVAL (1),
// 	      	      (8 << PCPVAL (2)) - 1);
// 	      return 1;
// 	    }
// 	}
//       else if ((PCPVAL (1) < 0) || (PCPVAL (1) > 31))
//         {
// 	  as_bad (_("Constant %ld out of range [0..31]"), PCPVAL (1));
// 	  return 1;
// 	}
//       *code |= (PCPVAL (0) << 6);
//       *code |= PCPVAL (1);
//       *code |= (((PCPVAL (2) & 1) << 5) | ((PCPVAL (2) & 2) << 8));
//       break;

//     case 6:
//       if ((PCPVAL (0) == 0) || (PCPVAL (0) == 7))
//         {
// 	  as_bad (_("R%ld must not be used as destination register"),
// 	  	  PCPVAL (0));
// 	  return 1;
// 	}
//       else if ((PCPVAL (1) == 0) || (PCPVAL (1) == 7))
//         {
// 	  as_bad (_("R%ld must not be used as source register"), PCPVAL (1));
// 	  return 1;
// 	}
//       else if (PCPVAL (0) == PCPVAL (1))
//         {
// 	  as_bad (_("Source and destination registers must not be the same"));
// 	  return 1;
// 	}
//       *code |= ((PCPVAL (0) << 6) | (PCPVAL (1) << 3));
//       break;

//     case 7:
//       for (op = 0; op < pcp_insn.nops; ++op)
//         switch (pcp_insn.ops[op])
// 	  {
// 	  case 'r': /* R0-R7  */
// 	  case 'R': /* [R0]-[R7]  */
// 	  case 'm': /* DAC=  */
// 	    *code |= (PCPVAL (op) << 3);
// 	    break;

// 	  case 'a': /* CONDCA  */
// 	  case 'b': /* CONDCB  */
// 	    *code |= (PCPVAL (op) << 6);
// 	    break;

// 	  case 'o': /* RTA/RNA=  */
// 	    *code |= (PCPVAL (op) << 2);
// 	    break;

// 	  case 'p': /* EDA=  */
// 	    *code |= (PCPVAL (op) << 1);
// 	    break;

// 	  case 'q': /* SDB=  */
// 	    *code |= PCPVAL (op);
// 	    break;

// 	  case 'e': /* expr  */
// 	    if (!strcmp (pop->name, "jl"))
// 	      {
// 	        if ((PCPVAL (op) < -512) || (PCPVAL (op) > 511))
// 		  {
// 		    as_bad (_("Offset %ld out of range [-512..511]"),
// 		    	    PCPVAL (op));
// 		    return 1;
// 		  }
// 		*code |= (PCPVAL (op) & 0x3ff);
// 	      }
// 	    else if (!strcmp (pop->name, "jc"))
// 	      {
// 	        if ((PCPVAL (op) < -32) || (PCPVAL (op) > 31))
// 		  {
// 		    as_bad (_("Offset %ld out of range [-32..31]"),
// 		    	    PCPVAL (op));
// 		    return 1;
// 		  }
// 		*code |= (PCPVAL (op) & 0x3f);
// 	      }
// 	    else if (!strcmp (pop->name, "jc.a"))
// 	      {
// 	        if (PCPVAL (op) & ~0xffff)
// 		  {
// 		    as_bad (_("PCODE address 0x%08lx out of range [0..65535]"),
// 		    	    PCPVAL (op));
// 		    return 1;
// 		  }
// 		/* Note: the code will be set in pcp_emit_code.  */
// 	      }
// 	    else
// 	      {
// 	        as_fatal (_("Unknown instruction \"%s\""), pop->name);
// 		return 1;
// 	      }
// 	    break;

// 	  case 'u': /* unknown  */
// 	    break;

// 	  default:
// 	    as_fatal (_("Operand #%d is unknown"), op);
// 	    return 1;
// 	  }
//       break;

//     default:
//       as_fatal (_("Illegal instruction format group %d"), pop->fmt_group);
//       return 1;
//     }

//   return 0;
// }

/* If a PCP insn could be successfully parsed and encoded, this function
   will take care of writing out the opcode bytes and relocation info, if
   needed.  */

// static void
// pcp_emit_code ()
// {
//   int opnum = -1, oplen = pcp_insn.code->len32 ? 4 : 2;
//   char *pfrag;
//   unsigned long code = pcp_insn.opcode;
//   enum bfd_reloc_code_real reloc = NO_RELOC;
//   relax_substateT state = RELAX_JC;
//   int add_bytes;

//   /* Make sure the current frag is large enough to hold a 32-bit insn.  */
//   frag_grow (4);
//   pfrag = frag_more (2);

//   /* Emit the 16-bit base code.  */
//   md_number_to_chars (pfrag, code, 2);

//   /* Emit DWARF2 line debug info (if requested).  */
//   dwarf2_emit_insn (2);

//   /* Handle class 2 instructions (*.pi) with undefined operand.  */
//   if ((pcp_insn.code->fmt_group == 2)
//       && ((pcp_insn.ops[1] == 'u') || (pcp_insn.ops[1] == 'U')))
//     {
//       if ((pcp_insn.prefix[1] == PCP_PREFIX_NONE)
//           || (pcp_insn.prefix[1] == PCP_PREFIX_DOFF))
// 	{
// 	  fix_new_exp (frag_now, (pfrag - frag_now->fr_literal), 0,
// 	  	       &pcp_insn.opexp[1], 0, BFD_RELOC_TRICORE_PCPOFF);
// 	  if (show_internals)
// 	    printf ("*** fix_new_exp (1, BFD_RELOC_TRICORE_PCPOFF)\n");
// 	}
//       else
// 	as_bad (_("Invalid prefix for operand #1"));

//       return;
//     }

//   /* Handle 32-bit instructions.  */
//   if (oplen == 4)
//     {
//       int is_lower = !strcmp (pcp_insn.code->name, "ldl.il");

//       pfrag = frag_more (2);
//       code <<= 16;
//       if (is_lower || !strcmp (pcp_insn.code->name, "ldl.iu"))
//         {
// 	  if (pcp_insn.ops[1] == 'e')
// 	    {
// 	      if (pcp_insn.prefix[1] == PCP_PREFIX_NONE)
// 	        {
// 		  if (is_lower)
// 	            code |= (PCPVAL (1) & 0xffff);
// 		  else
// 	            code |= ((PCPVAL (1) >> 16) & 0xffff);
// 		}    
// 	      else if (pcp_insn.prefix[1] == PCP_PREFIX_DPTR)
// 		{
// 		  if (PCPVAL (1) & 0x3)
// 		    {
// 		      as_bad (_("PRAM address 0x%08lx is not word-aligned"),
// 		      	      PCPVAL (1));
// 		      return;
// 		    }  
// 		  else if (PCPVAL (1) & 0xff)
// 		    as_warn (_("PRAM address 0x%08lx should be "
// 		    	       "aligned to a 256-byte boundary"),
// 			     PCPVAL (1));
// 	          code |= (PCPVAL (1) & 0xff00);
// 	        }
// 	      else if (pcp_insn.prefix[1] == PCP_PREFIX_DOFF)
// 	        {
// 		  if (PCPVAL (1) & 0x3)
// 		    {
// 		      as_bad (_("PRAM address 0x%08lx is not word-aligned"),
// 		      	      PCPVAL (1));
// 		      return;
// 		    }
// 		  code |= ((PCPVAL (1) >> 2) & 0x3f);
// 		}
// 	      else if (pcp_insn.prefix[1] == PCP_PREFIX_CPTR)
// 	        {
// 	          if (PCPVAL (1) & ~0xffff)
// 		    {
// 		      as_bad (_("PCODE address 0x%08lx out "
// 		      		"of range [0..65535]"),
// 		    	      PCPVAL (1));
// 		      return;
// 		    }
// 		  code |= (PCPVAL (1) & 0xffff);  
// 		}
// 	    }
// 	  else
// 	    {
// 	      opnum = 1;
// 	      if (pcp_insn.prefix[1] == PCP_PREFIX_NONE)
// 		{
// 		  if (is_lower)
// 	            reloc = BFD_RELOC_TRICORE_PCPLO;
// 		  else  
// 	            reloc = BFD_RELOC_TRICORE_PCPHI;
// 		}    
// 	      else if (pcp_insn.prefix[1] == PCP_PREFIX_DPTR)
// 	        reloc = BFD_RELOC_TRICORE_PCPPAGE;
// 	      else if (pcp_insn.prefix[1] == PCP_PREFIX_DOFF)
// 	        reloc = BFD_RELOC_TRICORE_PCPOFF;
// 	      else if (pcp_insn.prefix[1] == PCP_PREFIX_CPTR)
// 	        reloc = BFD_RELOC_TRICORE_PCPTEXT;
// 	      else
// 	        {
// ill_pfx_error:
// 		  as_bad (_("Illegal operand prefix"));
// 		  return;
// 		}  
// 	    }
// 	}
//       else if (!strcmp (pcp_insn.code->name, "jc.a"))
//         {
// 	  if (pcp_insn.ops[0] == 'e')
// 	    {
// 	      if ((pcp_insn.prefix[0] != PCP_PREFIX_NONE)
// 	          && (pcp_insn.prefix[0] != PCP_PREFIX_CPTR))
// 		goto ill_pfx_error;

// 	      /* If the target address has been specified as an absolute
// 	         expression (e.g., "0x1234"), we must assume the address
// 		 to already be in PCP-format, so we must not translate
// 		 it into TriCore-format (i.e., into a byte address).  */
// 	      code |= (PCPVAL (0) & 0xffff);
// 	    }  
// 	  else if (pcp_insn.ops[1] == 'e')
// 	    {  
// 	      if ((pcp_insn.prefix[1] != PCP_PREFIX_NONE)
// 	          && (pcp_insn.prefix[1] != PCP_PREFIX_CPTR))
// 		goto ill_pfx_error;

// 	      /* If the target address has been specified as an absolute
// 	         expression (e.g., "0x1234"), we must assume the address
// 		 to already be in PCP-format, so we must not translate
// 		 it into TriCore-format (i.e., into a byte address).  */
// 	      code |= (PCPVAL (1) & 0xffff);
// 	    }  
// 	  else
// 	    {
// 	      opnum = (pcp_insn.ops[0] == 'u') ? 0 : 1;
// 	      if ((pcp_insn.prefix[opnum] != PCP_PREFIX_NONE)
// 	          && (pcp_insn.prefix[opnum] != PCP_PREFIX_CPTR))
// 		goto ill_pfx_error;

// 	      reloc = BFD_RELOC_TRICORE_PCPTEXT;
// 	    }
// 	}
//       else
//         {
// 	  as_fatal (_("Internal error: unknown 32-bit PCP instruction \"%s\""),
// 	  	    pcp_insn.code->name);
// 	  return;
// 	}

//       /* Emit the additional code.  */
//       md_number_to_chars (pfrag, code & 0xffff, 2);

//       /* Handle relocations.  */
//       if (reloc != NO_RELOC)
//         {
// 	  fix_new_exp (frag_now, (pfrag - frag_now->fr_literal), 0,
// 	  	       &pcp_insn.opexp[opnum], 0, reloc);
// 	  if (show_internals)
// 	    printf ("*** fix_new_exp (%d, %s)\n", opnum,
// 	    	    GET_RELOC_NAME (reloc));
// 	}

//       pcp_insn.opcode = code;
//       return;
//     }

//   /* Handle PC-relative relocations for jl and jc.  */
//   if (!strcmp (pcp_insn.code->name, "jl"))
//     {
//       if (pcp_insn.ops[0] == 'u')
//         {
// 	  opnum = 0;
//           reloc = BFD_RELOC_TRICORE_PCPREL10;
// 	  state = RELAX_JL;
// 	}
//     }
//   else if (!strcmp (pcp_insn.code->name, "jc"))
//     {
//       if (pcp_insn.ops[0] == 'u')
//         opnum = 0;
//       else if (pcp_insn.ops[1] == 'u')
//         opnum = 1;
//       if (opnum != -1)
//         {
// 	  reloc = BFD_RELOC_TRICORE_PCPREL6;
// 	  state = RELAX_JC;
// 	}
//     }
//   if (reloc != NO_RELOC)
//     {
//       /* Prepare relaxing.  */
//       add_bytes = md_relax_table[state + 1].rlx_length;
//       (void) frag_var (rs_machine_dependent, add_bytes, add_bytes, state,
//       		       pcp_insn.opexp[opnum].X_add_symbol,
// 		       pcp_insn.opexp[opnum].X_add_number, pfrag);
//       if (show_internals)
//         printf ("*** frag_var (%d, add_bytes=%d)\n", opnum, add_bytes);
//     }
//   pcp_insn.opcode = code;
// }

/* Assemble the PCP instruction starting at STR.  */

// static void
// pcp_assemble (const char* str)
// {
//   pcp_opcode_t *insn;

//   if (show_internals)
//     printf ("*** pcp_assemble (\"%s\")\n", str);

//   /* Find out the insn's name and operands.  */
//   memset ((char *) &pcp_insn, 0, sizeof (pcp_insn));
//   pcp_ip (str);
//   if (pcp_insn.error)
//     {
//       as_bad ("%s: %s", str, pcp_insn.error);
//       return;
//     }

//   /* Find an opcode that matches pcp_insn.  */
//   if ((insn = find_pcp_opcode ()) == NULL)
//     {
//       if (show_internals)
//         print_pcp_insn ();
//       as_bad (_("Opcode/operand mismatch: %s"), str);
//       return;
//     }
//   pcp_insn.code = insn;
//   pcp_insn.opcode = insn->opcode;

//   /* Encode pcp_insn.  */
//   if (pcp_encode ())
//     {
//       if (show_internals)
//         print_pcp_insn ();
//       return;
//     }

//   /* Emit its opcode and fixup/relaxation infos.  */
//   pcp_emit_code ();

//   if (show_internals)
//     print_pcp_insn ();
// }

/* Assemble the TriCore instruction starting at STR.  */

void
md_assemble (char* str)
{
  opcode_t *insn;
  int i;

  assert (str);

//   Until now, PCP_seg isn't used so this part is useless
//   if (now_seg->flags & PCP_SEG)
//     {
//       pcp_assemble (str);
//       optimize_next = optimize;
//       return;
//     }

  if (show_internals)
    printf ("*** md_assemble (\"%s\")\n", str);

  /* Find out the insn's name and operands.  */
  memset ((char *) &the_insn, 0, sizeof (the_insn));
  tricore_ip (str);
  if (the_insn.error)
    {
      as_bad ("%s: %s", str, the_insn.error);
      goto ich_habe_fertig;
    }

  /* Find an opcode that matches the_insn.  */
  if ((insn = find_opcode ()) == NULL)
    {
      if (show_internals)
        print_the_insn ();
      as_bad (_("Opcode/operand mismatch: %s"), str);
      goto ich_habe_fertig;
    }
  the_insn.code = insn;
  the_insn.opcode = insn->opcode;

  /* Mark and check PC-relative operands.  */
  for (i = 0; i < insn->nr_operands; ++i)
    if (strchr ("mxrRoO", insn->args[i]))
      {
        the_insn.pcrel[i] = 1;
	if (the_insn.is_odd[i])
	  {
	    if (show_internals)
	      print_the_insn ();
	    as_bad (_("Displacement 0x%08lx is not even"), VAL (i));
	    goto ich_habe_fertig;
	  }
      }

  /* Encode the_insn.  */
  encode[insn->format] ();

  /* Emit its opcode and fixup/relaxation infos.  */
  emit_code ();

  if (show_internals)
    print_the_insn ();

ich_habe_fertig:
  /* Perform more workarounds, if requested and applicable.  */
  if (workaround_cpu9 && !the_insn.error
      && !strcmp (the_insn.code->name, "dsync"))
    {
      int tmp_use_insn32 = use_insn32;
      int tmp_use_insn16 = use_insn16;
      int tmp_optimize_next = optimize_next;
      int tmp_show_internals = show_internals;

      if (show_internals)
        printf ("*** Inserting 2 NOPs (workaround for CPU.9 bug).\n");
#if EXT_LISTING
      ASM_NOTICE_WORKAROUND (_("inserting 2 NOPs as a workaround "
		      	       "for bug CPU.9"));
#endif

      show_internals = 0;
      md_assemble ((char*)"nop");

      /* Restore desired insn size and optimization settings before
         assembling the next "nop", as they might have been changed
	 by md_assemble.  */
      use_insn32 = tmp_use_insn32;
      use_insn16 = tmp_use_insn16;
      optimize_next = tmp_optimize_next;
      md_assemble ((char*)"nop");
      show_internals = tmp_show_internals;
    }
  else if (workaround_cpu34 && !the_insn.error
	   && !strcmp (the_insn.code->name, "dsync"))
    {
      int tmp_show_internals = show_internals;

      if (show_internals)
        printf ("*** Inserting ISYNC (workaround for CPU_TC.034 bug).\n");
#if EXT_LISTING
      ASM_NOTICE_WORKAROUND (_("inserting ISYNC as a workaround "
		      	       "for bug CPU_TC.034"));
#endif

      show_internals = 0;
      md_assemble ((char*)"isync");
      show_internals = tmp_show_internals;
    }
  else if (workaround_cpu69 && !the_insn.error
	   && !strcmp (the_insn.code->name, "rslcx"))
    {
      int tmp_show_internals = show_internals;

      if (show_internals)
	printf ("*** Inserting NOP (workaround for CPU_TC.069 bug).\n");
#if EXT_LISTING
      ASM_NOTICE_WORKAROUND (_("inserting NOP as a workaround "
		     	       "for bug CPU_TC.069"));
#endif

      show_internals = 0;
      md_assemble ((char*)"nop");
      show_internals = tmp_show_internals;
    }
  else if (workaround_cpu81 && !the_insn.error
	   && (the_insn.regs[0] == 10)
  	   && (!strcmp (the_insn.code->name, "ld.a")
	       || !strcmp (the_insn.code->name, "ld.da")))
    {
      if (show_internals)
        printf ("*** Issuing error message (workaround for CPU_TC.081 bug).\n");
#if EXT_LISTING
      ASM_NOTICE_WORKAROUND (_("Issuing an error message as a workaround "
		     	       "for bug CPU_TC.081"));
#endif
      as_bad (_("%%A10 must not be loaded from memory due to bug CPU_TC.081"));
    }
  else if (workaround_cpu83 && !the_insn.error
  	   && (the_insn.code->nr_operands == 0)
	   && !strcmp (the_insn.code->name, "disable"))
    {
      int tmp_show_internals = show_internals;

      if (show_internals)
	printf ("*** Inserting NOP (workaround for CPU_TC.083 bug).\n");
#if EXT_LISTING
      ASM_NOTICE_WORKAROUND (_("inserting NOP as a workaround "
		     	       "for bug CPU_TC.083"));
#endif

      show_internals = 0;
      md_assemble ((char*)"nop");
      show_internals = tmp_show_internals;
    }

  /* Reset the .code16/32, .optim and .noopt selectors.  */
  use_insn32 = 0;
  use_insn16 = 0;
  optimize_next = optimize;
}

/* End of tc-tricore.c.  */

/* Next week: Assemble code for Deep Thought II.  Stay tuned!  */
