 

#include <asm/asm.h>
#include <asm/regdef.h>

 

   .text
   .set noreorder

    
LEAF(setjmp)
    

   REG_S sp, 0(a0)		 
   REG_S ra, 1*SZREG(a0)
   REG_S gp, 2*SZREG(a0)
   REG_S s0, 3*SZREG(a0)
   REG_S s1, 4*SZREG(a0)
   REG_S s2, 5*SZREG(a0)
   REG_S s3, 6*SZREG(a0)
   REG_S s4, 7*SZREG(a0)
   REG_S s5, 8*SZREG(a0)
   REG_S s6, 9*SZREG(a0)
   REG_S s7, 10*SZREG(a0)
   REG_S s8, 11*SZREG(a0)

   jr ra		 
    move v0, zero	 
END(setjmp)


    
LEAF(longjmp)
    

   sltiu t0, a1, 1	 
   addu a1, a1, t0	 

   REG_L sp, 0(a0)		 
   REG_L ra, 1*SZREG(a0)
   REG_L gp, 2*SZREG(a0)
   REG_L s0, 3*SZREG(a0)
   REG_L s1, 4*SZREG(a0)
   REG_L s2, 5*SZREG(a0)
   REG_L s3, 6*SZREG(a0)
   REG_L s4, 7*SZREG(a0)
   REG_L s5, 8*SZREG(a0)
   REG_L s6, 9*SZREG(a0)
   REG_L s7, 10*SZREG(a0)
   REG_L s8, 11*SZREG(a0)

   jr ra		 
    move v0, a1		 
END(longjmp)

#ifdef __ELF__
.section .note.GNU-stack,"",%progbits
#endif
