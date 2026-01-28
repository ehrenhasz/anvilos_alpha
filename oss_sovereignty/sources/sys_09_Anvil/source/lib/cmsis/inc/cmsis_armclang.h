


 

#ifndef __CMSIS_ARMCLANG_H
#define __CMSIS_ARMCLANG_H

#pragma clang system_header   


#ifndef   __ASM
  #define __ASM                                  __asm
#endif
#ifndef   __INLINE
  #define __INLINE                               __inline
#endif
#ifndef   __STATIC_INLINE
  #define __STATIC_INLINE                        static __inline
#endif
#ifndef   __STATIC_FORCEINLINE
  #define __STATIC_FORCEINLINE                   __attribute__((always_inline)) static __inline
#endif
#ifndef   __NO_RETURN
  #define __NO_RETURN                            __attribute__((__noreturn__))
#endif
#ifndef   __USED
  #define __USED                                 __attribute__((used))
#endif
#ifndef   __WEAK
  #define __WEAK                                 __attribute__((weak))
#endif
#ifndef   __PACKED
  #define __PACKED                               __attribute__((packed, aligned(1)))
#endif
#ifndef   __PACKED_STRUCT
  #define __PACKED_STRUCT                        struct __attribute__((packed, aligned(1)))
#endif
#ifndef   __PACKED_UNION
  #define __PACKED_UNION                         union __attribute__((packed, aligned(1)))
#endif
#ifndef   __UNALIGNED_UINT32        
  #pragma clang diagnostic push
  #pragma clang diagnostic ignored "-Wpacked"
 
  struct __attribute__((packed)) T_UINT32 { uint32_t v; };
  #pragma clang diagnostic pop
  #define __UNALIGNED_UINT32(x)                  (((struct T_UINT32 *)(x))->v)
#endif
#ifndef   __UNALIGNED_UINT16_WRITE
  #pragma clang diagnostic push
  #pragma clang diagnostic ignored "-Wpacked"
 
  __PACKED_STRUCT T_UINT16_WRITE { uint16_t v; };
  #pragma clang diagnostic pop
  #define __UNALIGNED_UINT16_WRITE(addr, val)    (void)((((struct T_UINT16_WRITE *)(void *)(addr))->v) = (val))
#endif
#ifndef   __UNALIGNED_UINT16_READ
  #pragma clang diagnostic push
  #pragma clang diagnostic ignored "-Wpacked"
 
  __PACKED_STRUCT T_UINT16_READ { uint16_t v; };
  #pragma clang diagnostic pop
  #define __UNALIGNED_UINT16_READ(addr)          (((const struct T_UINT16_READ *)(const void *)(addr))->v)
#endif
#ifndef   __UNALIGNED_UINT32_WRITE
  #pragma clang diagnostic push
  #pragma clang diagnostic ignored "-Wpacked"
 
  __PACKED_STRUCT T_UINT32_WRITE { uint32_t v; };
  #pragma clang diagnostic pop
  #define __UNALIGNED_UINT32_WRITE(addr, val)    (void)((((struct T_UINT32_WRITE *)(void *)(addr))->v) = (val))
#endif
#ifndef   __UNALIGNED_UINT32_READ
  #pragma clang diagnostic push
  #pragma clang diagnostic ignored "-Wpacked"
 
  __PACKED_STRUCT T_UINT32_READ { uint32_t v; };
  #pragma clang diagnostic pop
  #define __UNALIGNED_UINT32_READ(addr)          (((const struct T_UINT32_READ *)(const void *)(addr))->v)
#endif
#ifndef   __ALIGNED
  #define __ALIGNED(x)                           __attribute__((aligned(x)))
#endif
#ifndef   __RESTRICT
  #define __RESTRICT                             __restrict
#endif
#ifndef   __COMPILER_BARRIER
  #define __COMPILER_BARRIER()                   __ASM volatile("":::"memory")
#endif



#ifndef __PROGRAM_START
#define __PROGRAM_START           __main
#endif

#ifndef __INITIAL_SP
#define __INITIAL_SP              Image$$ARM_LIB_STACK$$ZI$$Limit
#endif

#ifndef __STACK_LIMIT
#define __STACK_LIMIT             Image$$ARM_LIB_STACK$$ZI$$Base
#endif

#ifndef __VECTOR_TABLE
#define __VECTOR_TABLE            __Vectors
#endif

#ifndef __VECTOR_TABLE_ATTRIBUTE
#define __VECTOR_TABLE_ATTRIBUTE  __attribute__((used, section("RESET")))
#endif

#if defined (__ARM_FEATURE_CMSE) && (__ARM_FEATURE_CMSE == 3U)
#ifndef __STACK_SEAL
#define __STACK_SEAL              Image$$STACKSEAL$$ZI$$Base
#endif

#ifndef __TZ_STACK_SEAL_SIZE
#define __TZ_STACK_SEAL_SIZE      8U
#endif

#ifndef __TZ_STACK_SEAL_VALUE
#define __TZ_STACK_SEAL_VALUE     0xFEF5EDA5FEF5EDA5ULL
#endif


__STATIC_FORCEINLINE void __TZ_set_STACKSEAL_S (uint32_t* stackTop) {
  *((uint64_t *)stackTop) = __TZ_STACK_SEAL_VALUE;
}
#endif






#if defined (__thumb__) && !defined (__thumb2__)
#define __CMSIS_GCC_OUT_REG(r) "=l" (r)
#define __CMSIS_GCC_RW_REG(r) "+l" (r)
#define __CMSIS_GCC_USE_REG(r) "l" (r)
#else
#define __CMSIS_GCC_OUT_REG(r) "=r" (r)
#define __CMSIS_GCC_RW_REG(r) "+r" (r)
#define __CMSIS_GCC_USE_REG(r) "r" (r)
#endif


#define __NOP          __builtin_arm_nop


#define __WFI          __builtin_arm_wfi



#define __WFE          __builtin_arm_wfe



#define __SEV          __builtin_arm_sev



#define __ISB()        __builtin_arm_isb(0xF)


#define __DSB()        __builtin_arm_dsb(0xF)



#define __DMB()        __builtin_arm_dmb(0xF)



#define __REV(value)   __builtin_bswap32(value)



#define __REV16(value) __ROR(__REV(value), 16)



#define __REVSH(value) (int16_t)__builtin_bswap16(value)



__STATIC_FORCEINLINE uint32_t __ROR(uint32_t op1, uint32_t op2)
{
  op2 %= 32U;
  if (op2 == 0U)
  {
    return op1;
  }
  return (op1 >> op2) | (op1 << (32U - op2));
}



#define __BKPT(value)     __ASM volatile ("bkpt "#value)



#define __RBIT            __builtin_arm_rbit


__STATIC_FORCEINLINE uint8_t __CLZ(uint32_t value)
{
  
  if (value == 0U)
  {
    return 32U;
  }
  return __builtin_clz(value);
}


#if ((defined (__ARM_ARCH_7M__       ) && (__ARM_ARCH_7M__        == 1)) || \
     (defined (__ARM_ARCH_7EM__      ) && (__ARM_ARCH_7EM__       == 1)) || \
     (defined (__ARM_ARCH_8M_MAIN__  ) && (__ARM_ARCH_8M_MAIN__   == 1)) || \
     (defined (__ARM_ARCH_8M_BASE__  ) && (__ARM_ARCH_8M_BASE__   == 1)) || \
     (defined (__ARM_ARCH_8_1M_MAIN__) && (__ARM_ARCH_8_1M_MAIN__ == 1))     )


#define __LDREXB        (uint8_t)__builtin_arm_ldrex



#define __LDREXH        (uint16_t)__builtin_arm_ldrex



#define __LDREXW        (uint32_t)__builtin_arm_ldrex



#define __STREXB        (uint32_t)__builtin_arm_strex



#define __STREXH        (uint32_t)__builtin_arm_strex



#define __STREXW        (uint32_t)__builtin_arm_strex



#define __CLREX             __builtin_arm_clrex

#endif 


#if ((defined (__ARM_ARCH_7M__       ) && (__ARM_ARCH_7M__        == 1)) || \
     (defined (__ARM_ARCH_7EM__      ) && (__ARM_ARCH_7EM__       == 1)) || \
     (defined (__ARM_ARCH_8M_MAIN__  ) && (__ARM_ARCH_8M_MAIN__   == 1)) || \
     (defined (__ARM_ARCH_8_1M_MAIN__) && (__ARM_ARCH_8_1M_MAIN__ == 1))     )


#define __SSAT             __builtin_arm_ssat



#define __USAT             __builtin_arm_usat



__STATIC_FORCEINLINE uint32_t __RRX(uint32_t value)
{
  uint32_t result;

  __ASM volatile ("rrx %0, %1" : __CMSIS_GCC_OUT_REG (result) : __CMSIS_GCC_USE_REG (value) );
  return(result);
}



__STATIC_FORCEINLINE uint8_t __LDRBT(volatile uint8_t *ptr)
{
  uint32_t result;

  __ASM volatile ("ldrbt %0, %1" : "=r" (result) : "Q" (*ptr) );
  return ((uint8_t) result);    
}



__STATIC_FORCEINLINE uint16_t __LDRHT(volatile uint16_t *ptr)
{
  uint32_t result;

  __ASM volatile ("ldrht %0, %1" : "=r" (result) : "Q" (*ptr) );
  return ((uint16_t) result);    
}



__STATIC_FORCEINLINE uint32_t __LDRT(volatile uint32_t *ptr)
{
  uint32_t result;

  __ASM volatile ("ldrt %0, %1" : "=r" (result) : "Q" (*ptr) );
  return(result);
}



__STATIC_FORCEINLINE void __STRBT(uint8_t value, volatile uint8_t *ptr)
{
  __ASM volatile ("strbt %1, %0" : "=Q" (*ptr) : "r" ((uint32_t)value) );
}



__STATIC_FORCEINLINE void __STRHT(uint16_t value, volatile uint16_t *ptr)
{
  __ASM volatile ("strht %1, %0" : "=Q" (*ptr) : "r" ((uint32_t)value) );
}



__STATIC_FORCEINLINE void __STRT(uint32_t value, volatile uint32_t *ptr)
{
  __ASM volatile ("strt %1, %0" : "=Q" (*ptr) : "r" (value) );
}

#else 


__STATIC_FORCEINLINE int32_t __SSAT(int32_t val, uint32_t sat)
{
  if ((sat >= 1U) && (sat <= 32U))
  {
    const int32_t max = (int32_t)((1U << (sat - 1U)) - 1U);
    const int32_t min = -1 - max ;
    if (val > max)
    {
      return max;
    }
    else if (val < min)
    {
      return min;
    }
  }
  return val;
}


__STATIC_FORCEINLINE uint32_t __USAT(int32_t val, uint32_t sat)
{
  if (sat <= 31U)
  {
    const uint32_t max = ((1U << sat) - 1U);
    if (val > (int32_t)max)
    {
      return max;
    }
    else if (val < 0)
    {
      return 0U;
    }
  }
  return (uint32_t)val;
}

#endif 


#if ((defined (__ARM_ARCH_8M_MAIN__  ) && (__ARM_ARCH_8M_MAIN__   == 1)) || \
     (defined (__ARM_ARCH_8M_BASE__  ) && (__ARM_ARCH_8M_BASE__   == 1)) || \
     (defined (__ARM_ARCH_8_1M_MAIN__) && (__ARM_ARCH_8_1M_MAIN__ == 1))     )


__STATIC_FORCEINLINE uint8_t __LDAB(volatile uint8_t *ptr)
{
  uint32_t result;

  __ASM volatile ("ldab %0, %1" : "=r" (result) : "Q" (*ptr) : "memory" );
  return ((uint8_t) result);
}



__STATIC_FORCEINLINE uint16_t __LDAH(volatile uint16_t *ptr)
{
  uint32_t result;

  __ASM volatile ("ldah %0, %1" : "=r" (result) : "Q" (*ptr) : "memory" );
  return ((uint16_t) result);
}



__STATIC_FORCEINLINE uint32_t __LDA(volatile uint32_t *ptr)
{
  uint32_t result;

  __ASM volatile ("lda %0, %1" : "=r" (result) : "Q" (*ptr) : "memory" );
  return(result);
}



__STATIC_FORCEINLINE void __STLB(uint8_t value, volatile uint8_t *ptr)
{
  __ASM volatile ("stlb %1, %0" : "=Q" (*ptr) : "r" ((uint32_t)value) : "memory" );
}



__STATIC_FORCEINLINE void __STLH(uint16_t value, volatile uint16_t *ptr)
{
  __ASM volatile ("stlh %1, %0" : "=Q" (*ptr) : "r" ((uint32_t)value) : "memory" );
}



__STATIC_FORCEINLINE void __STL(uint32_t value, volatile uint32_t *ptr)
{
  __ASM volatile ("stl %1, %0" : "=Q" (*ptr) : "r" ((uint32_t)value) : "memory" );
}



#define     __LDAEXB                 (uint8_t)__builtin_arm_ldaex



#define     __LDAEXH                 (uint16_t)__builtin_arm_ldaex



#define     __LDAEX                  (uint32_t)__builtin_arm_ldaex



#define     __STLEXB                 (uint32_t)__builtin_arm_stlex



#define     __STLEXH                 (uint32_t)__builtin_arm_stlex



#define     __STLEX                  (uint32_t)__builtin_arm_stlex

#endif 

 






#ifndef __ARM_COMPAT_H
__STATIC_FORCEINLINE void __enable_irq(void)
{
  __ASM volatile ("cpsie i" : : : "memory");
}
#endif



#ifndef __ARM_COMPAT_H
__STATIC_FORCEINLINE void __disable_irq(void)
{
  __ASM volatile ("cpsid i" : : : "memory");
}
#endif



__STATIC_FORCEINLINE uint32_t __get_CONTROL(void)
{
  uint32_t result;

  __ASM volatile ("MRS %0, control" : "=r" (result) );
  return(result);
}


#if (defined (__ARM_FEATURE_CMSE ) && (__ARM_FEATURE_CMSE == 3))

__STATIC_FORCEINLINE uint32_t __TZ_get_CONTROL_NS(void)
{
  uint32_t result;

  __ASM volatile ("MRS %0, control_ns" : "=r" (result) );
  return(result);
}
#endif



__STATIC_FORCEINLINE void __set_CONTROL(uint32_t control)
{
  __ASM volatile ("MSR control, %0" : : "r" (control) : "memory");
  __ISB();
}


#if (defined (__ARM_FEATURE_CMSE ) && (__ARM_FEATURE_CMSE == 3))

__STATIC_FORCEINLINE void __TZ_set_CONTROL_NS(uint32_t control)
{
  __ASM volatile ("MSR control_ns, %0" : : "r" (control) : "memory");
  __ISB();
}
#endif



__STATIC_FORCEINLINE uint32_t __get_IPSR(void)
{
  uint32_t result;

  __ASM volatile ("MRS %0, ipsr" : "=r" (result) );
  return(result);
}



__STATIC_FORCEINLINE uint32_t __get_APSR(void)
{
  uint32_t result;

  __ASM volatile ("MRS %0, apsr" : "=r" (result) );
  return(result);
}



__STATIC_FORCEINLINE uint32_t __get_xPSR(void)
{
  uint32_t result;

  __ASM volatile ("MRS %0, xpsr" : "=r" (result) );
  return(result);
}



__STATIC_FORCEINLINE uint32_t __get_PSP(void)
{
  uint32_t result;

  __ASM volatile ("MRS %0, psp"  : "=r" (result) );
  return(result);
}


#if (defined (__ARM_FEATURE_CMSE ) && (__ARM_FEATURE_CMSE == 3))

__STATIC_FORCEINLINE uint32_t __TZ_get_PSP_NS(void)
{
  uint32_t result;

  __ASM volatile ("MRS %0, psp_ns"  : "=r" (result) );
  return(result);
}
#endif



__STATIC_FORCEINLINE void __set_PSP(uint32_t topOfProcStack)
{
  __ASM volatile ("MSR psp, %0" : : "r" (topOfProcStack) : );
}


#if (defined (__ARM_FEATURE_CMSE ) && (__ARM_FEATURE_CMSE == 3))

__STATIC_FORCEINLINE void __TZ_set_PSP_NS(uint32_t topOfProcStack)
{
  __ASM volatile ("MSR psp_ns, %0" : : "r" (topOfProcStack) : );
}
#endif



__STATIC_FORCEINLINE uint32_t __get_MSP(void)
{
  uint32_t result;

  __ASM volatile ("MRS %0, msp" : "=r" (result) );
  return(result);
}


#if (defined (__ARM_FEATURE_CMSE ) && (__ARM_FEATURE_CMSE == 3))

__STATIC_FORCEINLINE uint32_t __TZ_get_MSP_NS(void)
{
  uint32_t result;

  __ASM volatile ("MRS %0, msp_ns" : "=r" (result) );
  return(result);
}
#endif



__STATIC_FORCEINLINE void __set_MSP(uint32_t topOfMainStack)
{
  __ASM volatile ("MSR msp, %0" : : "r" (topOfMainStack) : );
}


#if (defined (__ARM_FEATURE_CMSE ) && (__ARM_FEATURE_CMSE == 3))

__STATIC_FORCEINLINE void __TZ_set_MSP_NS(uint32_t topOfMainStack)
{
  __ASM volatile ("MSR msp_ns, %0" : : "r" (topOfMainStack) : );
}
#endif


#if (defined (__ARM_FEATURE_CMSE ) && (__ARM_FEATURE_CMSE == 3))

__STATIC_FORCEINLINE uint32_t __TZ_get_SP_NS(void)
{
  uint32_t result;

  __ASM volatile ("MRS %0, sp_ns" : "=r" (result) );
  return(result);
}



__STATIC_FORCEINLINE void __TZ_set_SP_NS(uint32_t topOfStack)
{
  __ASM volatile ("MSR sp_ns, %0" : : "r" (topOfStack) : );
}
#endif



__STATIC_FORCEINLINE uint32_t __get_PRIMASK(void)
{
  uint32_t result;

  __ASM volatile ("MRS %0, primask" : "=r" (result) );
  return(result);
}


#if (defined (__ARM_FEATURE_CMSE ) && (__ARM_FEATURE_CMSE == 3))

__STATIC_FORCEINLINE uint32_t __TZ_get_PRIMASK_NS(void)
{
  uint32_t result;

  __ASM volatile ("MRS %0, primask_ns" : "=r" (result) );
  return(result);
}
#endif



__STATIC_FORCEINLINE void __set_PRIMASK(uint32_t priMask)
{
  __ASM volatile ("MSR primask, %0" : : "r" (priMask) : "memory");
}


#if (defined (__ARM_FEATURE_CMSE ) && (__ARM_FEATURE_CMSE == 3))

__STATIC_FORCEINLINE void __TZ_set_PRIMASK_NS(uint32_t priMask)
{
  __ASM volatile ("MSR primask_ns, %0" : : "r" (priMask) : "memory");
}
#endif


#if ((defined (__ARM_ARCH_7M__       ) && (__ARM_ARCH_7M__        == 1)) || \
     (defined (__ARM_ARCH_7EM__      ) && (__ARM_ARCH_7EM__       == 1)) || \
     (defined (__ARM_ARCH_8M_MAIN__  ) && (__ARM_ARCH_8M_MAIN__   == 1)) || \
     (defined (__ARM_ARCH_8_1M_MAIN__) && (__ARM_ARCH_8_1M_MAIN__ == 1))     )

__STATIC_FORCEINLINE void __enable_fault_irq(void)
{
  __ASM volatile ("cpsie f" : : : "memory");
}



__STATIC_FORCEINLINE void __disable_fault_irq(void)
{
  __ASM volatile ("cpsid f" : : : "memory");
}



__STATIC_FORCEINLINE uint32_t __get_BASEPRI(void)
{
  uint32_t result;

  __ASM volatile ("MRS %0, basepri" : "=r" (result) );
  return(result);
}


#if (defined (__ARM_FEATURE_CMSE ) && (__ARM_FEATURE_CMSE == 3))

__STATIC_FORCEINLINE uint32_t __TZ_get_BASEPRI_NS(void)
{
  uint32_t result;

  __ASM volatile ("MRS %0, basepri_ns" : "=r" (result) );
  return(result);
}
#endif



__STATIC_FORCEINLINE void __set_BASEPRI(uint32_t basePri)
{
  __ASM volatile ("MSR basepri, %0" : : "r" (basePri) : "memory");
}


#if (defined (__ARM_FEATURE_CMSE ) && (__ARM_FEATURE_CMSE == 3))

__STATIC_FORCEINLINE void __TZ_set_BASEPRI_NS(uint32_t basePri)
{
  __ASM volatile ("MSR basepri_ns, %0" : : "r" (basePri) : "memory");
}
#endif



__STATIC_FORCEINLINE void __set_BASEPRI_MAX(uint32_t basePri)
{
  __ASM volatile ("MSR basepri_max, %0" : : "r" (basePri) : "memory");
}



__STATIC_FORCEINLINE uint32_t __get_FAULTMASK(void)
{
  uint32_t result;

  __ASM volatile ("MRS %0, faultmask" : "=r" (result) );
  return(result);
}


#if (defined (__ARM_FEATURE_CMSE ) && (__ARM_FEATURE_CMSE == 3))

__STATIC_FORCEINLINE uint32_t __TZ_get_FAULTMASK_NS(void)
{
  uint32_t result;

  __ASM volatile ("MRS %0, faultmask_ns" : "=r" (result) );
  return(result);
}
#endif



__STATIC_FORCEINLINE void __set_FAULTMASK(uint32_t faultMask)
{
  __ASM volatile ("MSR faultmask, %0" : : "r" (faultMask) : "memory");
}


#if (defined (__ARM_FEATURE_CMSE ) && (__ARM_FEATURE_CMSE == 3))

__STATIC_FORCEINLINE void __TZ_set_FAULTMASK_NS(uint32_t faultMask)
{
  __ASM volatile ("MSR faultmask_ns, %0" : : "r" (faultMask) : "memory");
}
#endif

#endif 


#if ((defined (__ARM_ARCH_8M_MAIN__  ) && (__ARM_ARCH_8M_MAIN__   == 1)) || \
     (defined (__ARM_ARCH_8M_BASE__  ) && (__ARM_ARCH_8M_BASE__   == 1)) || \
     (defined (__ARM_ARCH_8_1M_MAIN__) && (__ARM_ARCH_8_1M_MAIN__ == 1))     )


__STATIC_FORCEINLINE uint32_t __get_PSPLIM(void)
{
#if (!((defined (__ARM_ARCH_8M_MAIN__   ) && (__ARM_ARCH_8M_MAIN__   == 1)) || \
       (defined (__ARM_ARCH_8_1M_MAIN__ ) && (__ARM_ARCH_8_1M_MAIN__ == 1))   ) && \
    (!defined (__ARM_FEATURE_CMSE) || (__ARM_FEATURE_CMSE < 3)))
    
  return 0U;
#else
  uint32_t result;
  __ASM volatile ("MRS %0, psplim"  : "=r" (result) );
  return result;
#endif
}

#if (defined (__ARM_FEATURE_CMSE) && (__ARM_FEATURE_CMSE == 3))

__STATIC_FORCEINLINE uint32_t __TZ_get_PSPLIM_NS(void)
{
#if (!((defined (__ARM_ARCH_8M_MAIN__   ) && (__ARM_ARCH_8M_MAIN__   == 1)) || \
       (defined (__ARM_ARCH_8_1M_MAIN__ ) && (__ARM_ARCH_8_1M_MAIN__ == 1))   ) )
  
  return 0U;
#else
  uint32_t result;
  __ASM volatile ("MRS %0, psplim_ns"  : "=r" (result) );
  return result;
#endif
}
#endif



__STATIC_FORCEINLINE void __set_PSPLIM(uint32_t ProcStackPtrLimit)
{
#if (!((defined (__ARM_ARCH_8M_MAIN__   ) && (__ARM_ARCH_8M_MAIN__   == 1)) || \
       (defined (__ARM_ARCH_8_1M_MAIN__ ) && (__ARM_ARCH_8_1M_MAIN__ == 1))   ) && \
    (!defined (__ARM_FEATURE_CMSE) || (__ARM_FEATURE_CMSE < 3)))
  
  (void)ProcStackPtrLimit;
#else
  __ASM volatile ("MSR psplim, %0" : : "r" (ProcStackPtrLimit));
#endif
}


#if (defined (__ARM_FEATURE_CMSE  ) && (__ARM_FEATURE_CMSE   == 3))

__STATIC_FORCEINLINE void __TZ_set_PSPLIM_NS(uint32_t ProcStackPtrLimit)
{
#if (!((defined (__ARM_ARCH_8M_MAIN__   ) && (__ARM_ARCH_8M_MAIN__   == 1)) || \
       (defined (__ARM_ARCH_8_1M_MAIN__ ) && (__ARM_ARCH_8_1M_MAIN__ == 1))   ) )
  
  (void)ProcStackPtrLimit;
#else
  __ASM volatile ("MSR psplim_ns, %0\n" : : "r" (ProcStackPtrLimit));
#endif
}
#endif



__STATIC_FORCEINLINE uint32_t __get_MSPLIM(void)
{
#if (!((defined (__ARM_ARCH_8M_MAIN__   ) && (__ARM_ARCH_8M_MAIN__   == 1)) || \
       (defined (__ARM_ARCH_8_1M_MAIN__ ) && (__ARM_ARCH_8_1M_MAIN__ == 1))   ) && \
    (!defined (__ARM_FEATURE_CMSE) || (__ARM_FEATURE_CMSE < 3)))
  
  return 0U;
#else
  uint32_t result;
  __ASM volatile ("MRS %0, msplim" : "=r" (result) );
  return result;
#endif
}


#if (defined (__ARM_FEATURE_CMSE  ) && (__ARM_FEATURE_CMSE   == 3))

__STATIC_FORCEINLINE uint32_t __TZ_get_MSPLIM_NS(void)
{
#if (!((defined (__ARM_ARCH_8M_MAIN__   ) && (__ARM_ARCH_8M_MAIN__   == 1)) || \
       (defined (__ARM_ARCH_8_1M_MAIN__ ) && (__ARM_ARCH_8_1M_MAIN__ == 1))   ) )
  
  return 0U;
#else
  uint32_t result;
  __ASM volatile ("MRS %0, msplim_ns" : "=r" (result) );
  return result;
#endif
}
#endif



__STATIC_FORCEINLINE void __set_MSPLIM(uint32_t MainStackPtrLimit)
{
#if (!((defined (__ARM_ARCH_8M_MAIN__   ) && (__ARM_ARCH_8M_MAIN__   == 1)) || \
       (defined (__ARM_ARCH_8_1M_MAIN__ ) && (__ARM_ARCH_8_1M_MAIN__ == 1))   ) && \
    (!defined (__ARM_FEATURE_CMSE) || (__ARM_FEATURE_CMSE < 3)))
  
  (void)MainStackPtrLimit;
#else
  __ASM volatile ("MSR msplim, %0" : : "r" (MainStackPtrLimit));
#endif
}


#if (defined (__ARM_FEATURE_CMSE  ) && (__ARM_FEATURE_CMSE   == 3))

__STATIC_FORCEINLINE void __TZ_set_MSPLIM_NS(uint32_t MainStackPtrLimit)
{
#if (!((defined (__ARM_ARCH_8M_MAIN__   ) && (__ARM_ARCH_8M_MAIN__   == 1)) || \
       (defined (__ARM_ARCH_8_1M_MAIN__ ) && (__ARM_ARCH_8_1M_MAIN__ == 1))   ) )
  
  (void)MainStackPtrLimit;
#else
  __ASM volatile ("MSR msplim_ns, %0" : : "r" (MainStackPtrLimit));
#endif
}
#endif

#endif 


#if ((defined (__FPU_PRESENT) && (__FPU_PRESENT == 1U)) && \
     (defined (__FPU_USED   ) && (__FPU_USED    == 1U))     )
#define __get_FPSCR      (uint32_t)__builtin_arm_get_fpscr
#else
#define __get_FPSCR()      ((uint32_t)0U)
#endif


#if ((defined (__FPU_PRESENT) && (__FPU_PRESENT == 1U)) && \
     (defined (__FPU_USED   ) && (__FPU_USED    == 1U))     )
#define __set_FPSCR      __builtin_arm_set_fpscr
#else
#define __set_FPSCR(x)      ((void)(x))
#endif








#if (defined (__ARM_FEATURE_DSP) && (__ARM_FEATURE_DSP == 1))

#define     __SADD8                 __builtin_arm_sadd8
#define     __QADD8                 __builtin_arm_qadd8
#define     __SHADD8                __builtin_arm_shadd8
#define     __UADD8                 __builtin_arm_uadd8
#define     __UQADD8                __builtin_arm_uqadd8
#define     __UHADD8                __builtin_arm_uhadd8
#define     __SSUB8                 __builtin_arm_ssub8
#define     __QSUB8                 __builtin_arm_qsub8
#define     __SHSUB8                __builtin_arm_shsub8
#define     __USUB8                 __builtin_arm_usub8
#define     __UQSUB8                __builtin_arm_uqsub8
#define     __UHSUB8                __builtin_arm_uhsub8
#define     __SADD16                __builtin_arm_sadd16
#define     __QADD16                __builtin_arm_qadd16
#define     __SHADD16               __builtin_arm_shadd16
#define     __UADD16                __builtin_arm_uadd16
#define     __UQADD16               __builtin_arm_uqadd16
#define     __UHADD16               __builtin_arm_uhadd16
#define     __SSUB16                __builtin_arm_ssub16
#define     __QSUB16                __builtin_arm_qsub16
#define     __SHSUB16               __builtin_arm_shsub16
#define     __USUB16                __builtin_arm_usub16
#define     __UQSUB16               __builtin_arm_uqsub16
#define     __UHSUB16               __builtin_arm_uhsub16
#define     __SASX                  __builtin_arm_sasx
#define     __QASX                  __builtin_arm_qasx
#define     __SHASX                 __builtin_arm_shasx
#define     __UASX                  __builtin_arm_uasx
#define     __UQASX                 __builtin_arm_uqasx
#define     __UHASX                 __builtin_arm_uhasx
#define     __SSAX                  __builtin_arm_ssax
#define     __QSAX                  __builtin_arm_qsax
#define     __SHSAX                 __builtin_arm_shsax
#define     __USAX                  __builtin_arm_usax
#define     __UQSAX                 __builtin_arm_uqsax
#define     __UHSAX                 __builtin_arm_uhsax
#define     __USAD8                 __builtin_arm_usad8
#define     __USADA8                __builtin_arm_usada8
#define     __SSAT16                __builtin_arm_ssat16
#define     __USAT16                __builtin_arm_usat16
#define     __UXTB16                __builtin_arm_uxtb16
#define     __UXTAB16               __builtin_arm_uxtab16
#define     __SXTB16                __builtin_arm_sxtb16
#define     __SXTAB16               __builtin_arm_sxtab16
#define     __SMUAD                 __builtin_arm_smuad
#define     __SMUADX                __builtin_arm_smuadx
#define     __SMLAD                 __builtin_arm_smlad
#define     __SMLADX                __builtin_arm_smladx
#define     __SMLALD                __builtin_arm_smlald
#define     __SMLALDX               __builtin_arm_smlaldx
#define     __SMUSD                 __builtin_arm_smusd
#define     __SMUSDX                __builtin_arm_smusdx
#define     __SMLSD                 __builtin_arm_smlsd
#define     __SMLSDX                __builtin_arm_smlsdx
#define     __SMLSLD                __builtin_arm_smlsld
#define     __SMLSLDX               __builtin_arm_smlsldx
#define     __SEL                   __builtin_arm_sel
#define     __QADD                  __builtin_arm_qadd
#define     __QSUB                  __builtin_arm_qsub

#define __PKHBT(ARG1,ARG2,ARG3)          ( ((((uint32_t)(ARG1))          ) & 0x0000FFFFUL) |  \
                                           ((((uint32_t)(ARG2)) << (ARG3)) & 0xFFFF0000UL)  )

#define __PKHTB(ARG1,ARG2,ARG3)          ( ((((uint32_t)(ARG1))          ) & 0xFFFF0000UL) |  \
                                           ((((uint32_t)(ARG2)) >> (ARG3)) & 0x0000FFFFUL)  )

#define __SXTB16_RORn(ARG1, ARG2)        __SXTB16(__ROR(ARG1, ARG2))

#define __SXTAB16_RORn(ARG1, ARG2, ARG3) __SXTAB16(ARG1, __ROR(ARG2, ARG3))

__STATIC_FORCEINLINE int32_t __SMMLA (int32_t op1, int32_t op2, int32_t op3)
{
  int32_t result;

  __ASM volatile ("smmla %0, %1, %2, %3" : "=r" (result): "r"  (op1), "r" (op2), "r" (op3) );
  return(result);
}

#endif 



#endif 
