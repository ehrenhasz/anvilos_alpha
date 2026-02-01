 
 

#include "sme-inst.h"

.arch_extension sve

.globl rdvl_sve
rdvl_sve:
	hint	34			 
	rdvl	x0, #1
	ret

.globl rdvl_sme
rdvl_sme:
	hint	34			 

	rdsvl	0, 1

	ret
