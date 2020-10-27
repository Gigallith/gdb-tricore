/* THIS FILE IS GENERATED.  -*- buffer-read-only: t -*- vi:set ro:
  Original: tricore.xml */

#include "defs.h"
#include "osabi.h"
#include "target-descriptions.h"

struct target_desc *tdesc_tricore;
static void
initialize_tdesc_tricore (void)
{
  struct target_desc *result = allocate_target_description ();
  struct tdesc_feature *feature;

  feature = tdesc_create_feature (result, "org.gnu.gdb.tricore.core");
  tdesc_create_reg (feature, "d0", 0, 1, "data", 32, "int");
  tdesc_create_reg (feature, "d1", 1, 1, "data", 32, "int");
  tdesc_create_reg (feature, "d2", 2, 1, "data", 32, "int");
  tdesc_create_reg (feature, "d3", 3, 1, "data", 32, "int");
  tdesc_create_reg (feature, "d4", 4, 1, "data", 32, "int");
  tdesc_create_reg (feature, "d5", 5, 1, "data", 32, "int");
  tdesc_create_reg (feature, "d6", 6, 1, "data", 32, "int");
  tdesc_create_reg (feature, "d7", 7, 1, "data", 32, "int");
  tdesc_create_reg (feature, "d8", 8, 1, "data", 32, "int");
  tdesc_create_reg (feature, "d9", 9, 1, "data", 32, "int");
  tdesc_create_reg (feature, "d10", 10, 1, "data", 32, "int");
  tdesc_create_reg (feature, "d11", 11, 1, "data", 32, "int");
  tdesc_create_reg (feature, "d12", 12, 1, "data", 32, "int");
  tdesc_create_reg (feature, "d13", 13, 1, "data", 32, "int");
  tdesc_create_reg (feature, "d14", 14, 1, "data", 32, "int");
  tdesc_create_reg (feature, "d15", 15, 1, "data", 32, "int");
  tdesc_create_reg (feature, "a0", 16, 1, "address", 32, "int");
  tdesc_create_reg (feature, "a1", 17, 1, "address", 32, "int");
  tdesc_create_reg (feature, "a2", 18, 1, "address", 32, "int");
  tdesc_create_reg (feature, "a3", 19, 1, "address", 32, "int");
  tdesc_create_reg (feature, "a4", 20, 1, "address", 32, "int");
  tdesc_create_reg (feature, "a5", 21, 1, "address", 32, "int");
  tdesc_create_reg (feature, "a6", 22, 1, "address", 32, "int");
  tdesc_create_reg (feature, "a7", 23, 1, "address", 32, "int");
  tdesc_create_reg (feature, "a8", 24, 1, "address", 32, "int");
  tdesc_create_reg (feature, "a9", 25, 1, "address", 32, "int");
  tdesc_create_reg (feature, "a10", 26, 1, "address", 32, "data_ptr");
  tdesc_create_reg (feature, "a11", 27, 1, "address", 32, "int");
  tdesc_create_reg (feature, "a12", 28, 1, "address", 32, "int");
  tdesc_create_reg (feature, "a13", 29, 1, "address", 32, "int");
  tdesc_create_reg (feature, "a14", 30, 1, "address", 32, "int");
  tdesc_create_reg (feature, "a15", 31, 1, "address", 32, "int");
  tdesc_create_reg (feature, "lcx", 32, 1, NULL, 32, "int");
  tdesc_create_reg (feature, "fcx", 33, 1, NULL, 32, "int");
  tdesc_create_reg (feature, "pcxi", 34, 1, NULL, 32, "int");
  tdesc_create_reg (feature, "psw", 35, 1, NULL, 32, "int");
  tdesc_create_reg (feature, "pc", 36, 1, NULL, 32, "code_ptr");
  tdesc_create_reg (feature, "icr", 37, 1, NULL, 32, "int");
  tdesc_create_reg (feature, "isp", 38, 1, NULL, 32, "int");
  tdesc_create_reg (feature, "btv", 39, 1, NULL, 32, "int");
  tdesc_create_reg (feature, "biv", 40, 1, NULL, 32, "int");
  tdesc_create_reg (feature, "syscon", 41, 1, NULL, 32, "int");
  tdesc_create_reg (feature, "pmucon0", 42, 1, NULL, 32, "int");
  tdesc_create_reg (feature, "dmucon", 43, 1, NULL, 32, "int");
  tdesc_create_reg (feature, "lcx", 44, 1, NULL, 32, "int");
  
  tdesc_tricore = result;
}
