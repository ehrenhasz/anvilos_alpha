#ifndef PPC_H
#define PPC_H
#ifdef __cplusplus
extern "C" {
#endif
typedef uint64_t ppc_cpu_t;
struct powerpc_opcode
{
  const char *name;
  unsigned long opcode;
  unsigned long mask;
  ppc_cpu_t flags;
  ppc_cpu_t deprecated;
  unsigned char operands[8];
};
extern const struct powerpc_opcode powerpc_opcodes[];
extern const int powerpc_num_opcodes;
extern const struct powerpc_opcode vle_opcodes[];
extern const int vle_num_opcodes;
#define PPC_OPCODE_PPC			 1
#define PPC_OPCODE_POWER		 2
#define PPC_OPCODE_POWER2		 4
#define PPC_OPCODE_601			 8
#define PPC_OPCODE_COMMON	      0x10
#define PPC_OPCODE_ANY		      0x20
#define PPC_OPCODE_64		      0x40
#define PPC_OPCODE_64_BRIDGE	      0x80
#define PPC_OPCODE_ALTIVEC	     0x100
#define PPC_OPCODE_403		     0x200
#define PPC_OPCODE_BOOKE	     0x400
#define PPC_OPCODE_440		     0x800
#define PPC_OPCODE_POWER4	    0x1000
#define PPC_OPCODE_POWER7	    0x2000
#define PPC_OPCODE_SPE		    0x4000
#define PPC_OPCODE_ISEL		    0x8000
#define PPC_OPCODE_EFS		   0x10000
#define PPC_OPCODE_BRLOCK	   0x20000
#define PPC_OPCODE_PMR		   0x40000
#define PPC_OPCODE_CACHELCK	   0x80000
#define PPC_OPCODE_RFMCI	  0x100000
#define PPC_OPCODE_POWER5	  0x200000
#define PPC_OPCODE_E300           0x400000
#define PPC_OPCODE_POWER6	  0x800000
#define PPC_OPCODE_CELL		 0x1000000
#define PPC_OPCODE_PPCPS	 0x2000000
#define PPC_OPCODE_E500MC        0x4000000
#define PPC_OPCODE_405		 0x8000000
#define PPC_OPCODE_VSX		0x10000000
#define PPC_OPCODE_A2	 	0x20000000
#define PPC_OPCODE_476		0x40000000
#define PPC_OPCODE_TITAN        0x80000000
#define PPC_OPCODE_E500	       0x100000000ull
#define PPC_OPCODE_ALTIVEC2    0x200000000ull
#define PPC_OPCODE_E6500       0x400000000ull
#define PPC_OPCODE_TMR         0x800000000ull
#define PPC_OPCODE_VLE	      0x1000000000ull
#define PPC_OPCODE_POWER8     0x2000000000ull
#define PPC_OPCODE_HTM        PPC_OPCODE_POWER8
#define PPC_OPCODE_750	      0x4000000000ull
#define PPC_OPCODE_7450	      0x8000000000ull
#define PPC_OPCODE_860	      0x10000000000ull
#define PPC_OPCODE_POWER9     0x20000000000ull
#define PPC_OPCODE_VSX3       0x40000000000ull
#define PPC_OPCODE_E200Z4     0x80000000000ull
#define PPC_OP(i) (((i) >> 26) & 0x3f)
#define PPC_OP_SE_VLE(m) ((m) <= 0xffff)
#define VLE_OP(i,m) (((i) >> ((m) <= 0xffff ? 10 : 26)) & 0x3f)
#define VLE_OP_TO_SEG(i) ((i) >> 1)
struct powerpc_operand
{
  unsigned int bitm;
  int shift;
  unsigned long (*insert)
    (unsigned long instruction, long op, ppc_cpu_t dialect, const char **errmsg);
  long (*extract) (unsigned long instruction, ppc_cpu_t dialect, int *invalid);
  unsigned long flags;
};
extern const struct powerpc_operand powerpc_operands[];
extern const unsigned int num_powerpc_operands;
#define PPC_OPSHIFT_INV (-1U << 31)
#define PPC_OPERAND_SIGNED (0x1)
#define PPC_OPERAND_SIGNOPT (0x2)
#define PPC_OPERAND_FAKE (0x4)
#define PPC_OPERAND_PARENS (0x8)
#define PPC_OPERAND_CR_BIT (0x10)
#define PPC_OPERAND_GPR (0x20)
#define PPC_OPERAND_GPR_0 (0x40)
#define PPC_OPERAND_FPR (0x80)
#define PPC_OPERAND_RELATIVE (0x100)
#define PPC_OPERAND_ABSOLUTE (0x200)
#define PPC_OPERAND_OPTIONAL (0x400)
#define PPC_OPERAND_NEXT (0x800)
#define PPC_OPERAND_NEGATIVE (0x1000)
#define PPC_OPERAND_VR (0x2000)
#define PPC_OPERAND_DS (0x4000)
#define PPC_OPERAND_DQ (0x8000)
#define PPC_OPERAND_PLUS1 (0x10000)
#define PPC_OPERAND_FSL (0x20000)
#define PPC_OPERAND_FCR (0x40000)
#define PPC_OPERAND_UDI (0x80000)
#define PPC_OPERAND_VSR (0x100000)
#define PPC_OPERAND_CR_REG (0x200000)
#define PPC_OPERAND_OPTIONAL_VALUE (0x400000)
#define PPC_OPERAND_OPTIONAL32 (0x800000)
struct powerpc_macro
{
  const char *name;
  unsigned int operands;
  ppc_cpu_t flags;
  const char *format;
};
extern const struct powerpc_macro powerpc_macros[];
extern const int powerpc_num_macros;
static inline long
ppc_optional_operand_value (const struct powerpc_operand *operand)
{
  if ((operand->flags & PPC_OPERAND_OPTIONAL_VALUE) != 0)
    return (operand+1)->shift;
  return 0;
}
#ifdef __cplusplus
}
#endif
#endif  
