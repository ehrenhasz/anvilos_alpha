 
 

#if   defined ( __ICCARM__ )
  #pragma system_include          
#elif defined (__clang__)
  #pragma clang system_header    
#endif

#ifndef __CORE_CM1_H_GENERIC
#define __CORE_CM1_H_GENERIC

#include <stdint.h>

#ifdef __cplusplus
 extern "C" {
#endif

 


 
 

#include "cmsis_version.h"
 
 
#define __CM1_CMSIS_VERSION_MAIN  (__CM_CMSIS_VERSION_MAIN)               
#define __CM1_CMSIS_VERSION_SUB   (__CM_CMSIS_VERSION_SUB)                
#define __CM1_CMSIS_VERSION       ((__CM1_CMSIS_VERSION_MAIN << 16U) | \
                                    __CM1_CMSIS_VERSION_SUB           )   

#define __CORTEX_M                (1U)                                    

 
#define __FPU_USED       0U

#if defined ( __CC_ARM )
  #if defined __TARGET_FPU_VFP
    #error "Compiler generates FPU instructions for a device without an FPU (check __FPU_PRESENT)"
  #endif

#elif defined (__ARMCC_VERSION) && (__ARMCC_VERSION >= 6010050)
  #if defined __ARM_FP
    #error "Compiler generates FPU instructions for a device without an FPU (check __FPU_PRESENT)"
  #endif

#elif defined ( __GNUC__ )
  #if defined (__VFP_FP__) && !defined(__SOFTFP__)
    #error "Compiler generates FPU instructions for a device without an FPU (check __FPU_PRESENT)"
  #endif

#elif defined ( __ICCARM__ )
  #if defined __ARMVFP__
    #error "Compiler generates FPU instructions for a device without an FPU (check __FPU_PRESENT)"
  #endif

#elif defined ( __TI_ARM__ )
  #if defined __TI_VFP_SUPPORT__
    #error "Compiler generates FPU instructions for a device without an FPU (check __FPU_PRESENT)"
  #endif

#elif defined ( __TASKING__ )
  #if defined __FPU_VFP__
    #error "Compiler generates FPU instructions for a device without an FPU (check __FPU_PRESENT)"
  #endif

#elif defined ( __CSMC__ )
  #if ( __CSMC__ & 0x400U)
    #error "Compiler generates FPU instructions for a device without an FPU (check __FPU_PRESENT)"
  #endif

#endif

#include "cmsis_compiler.h"                


#ifdef __cplusplus
}
#endif

#endif  

#ifndef __CMSIS_GENERIC

#ifndef __CORE_CM1_H_DEPENDANT
#define __CORE_CM1_H_DEPENDANT

#ifdef __cplusplus
 extern "C" {
#endif

 
#if defined __CHECK_DEVICE_DEFINES
  #ifndef __CM1_REV
    #define __CM1_REV               0x0100U
    #warning "__CM1_REV not defined in device header file; using default!"
  #endif

  #ifndef __NVIC_PRIO_BITS
    #define __NVIC_PRIO_BITS          2U
    #warning "__NVIC_PRIO_BITS not defined in device header file; using default!"
  #endif

  #ifndef __Vendor_SysTickConfig
    #define __Vendor_SysTickConfig    0U
    #warning "__Vendor_SysTickConfig not defined in device header file; using default!"
  #endif
#endif

 
 
#ifdef __cplusplus
  #define   __I     volatile              
#else
  #define   __I     volatile const        
#endif
#define     __O     volatile              
#define     __IO    volatile              

 
#define     __IM     volatile const       
#define     __OM     volatile             
#define     __IOM    volatile             

 



 
 

 

 
typedef union
{
  struct
  {
    uint32_t _reserved0:28;               
    uint32_t V:1;                         
    uint32_t C:1;                         
    uint32_t Z:1;                         
    uint32_t N:1;                         
  } b;                                    
  uint32_t w;                             
} APSR_Type;

 
#define APSR_N_Pos                         31U                                             
#define APSR_N_Msk                         (1UL << APSR_N_Pos)                             

#define APSR_Z_Pos                         30U                                             
#define APSR_Z_Msk                         (1UL << APSR_Z_Pos)                             

#define APSR_C_Pos                         29U                                             
#define APSR_C_Msk                         (1UL << APSR_C_Pos)                             

#define APSR_V_Pos                         28U                                             
#define APSR_V_Msk                         (1UL << APSR_V_Pos)                             


 
typedef union
{
  struct
  {
    uint32_t ISR:9;                       
    uint32_t _reserved0:23;               
  } b;                                    
  uint32_t w;                             
} IPSR_Type;

 
#define IPSR_ISR_Pos                        0U                                             
#define IPSR_ISR_Msk                       (0x1FFUL  )                   


 
typedef union
{
  struct
  {
    uint32_t ISR:9;                       
    uint32_t _reserved0:15;               
    uint32_t T:1;                         
    uint32_t _reserved1:3;                
    uint32_t V:1;                         
    uint32_t C:1;                         
    uint32_t Z:1;                         
    uint32_t N:1;                         
  } b;                                    
  uint32_t w;                             
} xPSR_Type;

 
#define xPSR_N_Pos                         31U                                             
#define xPSR_N_Msk                         (1UL << xPSR_N_Pos)                             

#define xPSR_Z_Pos                         30U                                             
#define xPSR_Z_Msk                         (1UL << xPSR_Z_Pos)                             

#define xPSR_C_Pos                         29U                                             
#define xPSR_C_Msk                         (1UL << xPSR_C_Pos)                             

#define xPSR_V_Pos                         28U                                             
#define xPSR_V_Msk                         (1UL << xPSR_V_Pos)                             

#define xPSR_T_Pos                         24U                                             
#define xPSR_T_Msk                         (1UL << xPSR_T_Pos)                             

#define xPSR_ISR_Pos                        0U                                             
#define xPSR_ISR_Msk                       (0x1FFUL  )                   


 
typedef union
{
  struct
  {
    uint32_t _reserved0:1;                
    uint32_t SPSEL:1;                     
    uint32_t _reserved1:30;               
  } b;                                    
  uint32_t w;                             
} CONTROL_Type;

 
#define CONTROL_SPSEL_Pos                   1U                                             
#define CONTROL_SPSEL_Msk                  (1UL << CONTROL_SPSEL_Pos)                      

 


 

 
typedef struct
{
  __IOM uint32_t ISER[1U];                
        uint32_t RESERVED0[31U];
  __IOM uint32_t ICER[1U];                
        uint32_t RSERVED1[31U];
  __IOM uint32_t ISPR[1U];                
        uint32_t RESERVED2[31U];
  __IOM uint32_t ICPR[1U];                
        uint32_t RESERVED3[31U];
        uint32_t RESERVED4[64U];
  __IOM uint32_t IP[8U];                  
}  NVIC_Type;

 


 

 
typedef struct
{
  __IM  uint32_t CPUID;                   
  __IOM uint32_t ICSR;                    
        uint32_t RESERVED0;
  __IOM uint32_t AIRCR;                   
  __IOM uint32_t SCR;                     
  __IOM uint32_t CCR;                     
        uint32_t RESERVED1;
  __IOM uint32_t SHP[2U];                 
  __IOM uint32_t SHCSR;                   
} SCB_Type;

 
#define SCB_CPUID_IMPLEMENTER_Pos          24U                                             
#define SCB_CPUID_IMPLEMENTER_Msk          (0xFFUL << SCB_CPUID_IMPLEMENTER_Pos)           

#define SCB_CPUID_VARIANT_Pos              20U                                             
#define SCB_CPUID_VARIANT_Msk              (0xFUL << SCB_CPUID_VARIANT_Pos)                

#define SCB_CPUID_ARCHITECTURE_Pos         16U                                             
#define SCB_CPUID_ARCHITECTURE_Msk         (0xFUL << SCB_CPUID_ARCHITECTURE_Pos)           

#define SCB_CPUID_PARTNO_Pos                4U                                             
#define SCB_CPUID_PARTNO_Msk               (0xFFFUL << SCB_CPUID_PARTNO_Pos)               

#define SCB_CPUID_REVISION_Pos              0U                                             
#define SCB_CPUID_REVISION_Msk             (0xFUL  )           

 
#define SCB_ICSR_NMIPENDSET_Pos            31U                                             
#define SCB_ICSR_NMIPENDSET_Msk            (1UL << SCB_ICSR_NMIPENDSET_Pos)                

#define SCB_ICSR_PENDSVSET_Pos             28U                                             
#define SCB_ICSR_PENDSVSET_Msk             (1UL << SCB_ICSR_PENDSVSET_Pos)                 

#define SCB_ICSR_PENDSVCLR_Pos             27U                                             
#define SCB_ICSR_PENDSVCLR_Msk             (1UL << SCB_ICSR_PENDSVCLR_Pos)                 

#define SCB_ICSR_PENDSTSET_Pos             26U                                             
#define SCB_ICSR_PENDSTSET_Msk             (1UL << SCB_ICSR_PENDSTSET_Pos)                 

#define SCB_ICSR_PENDSTCLR_Pos             25U                                             
#define SCB_ICSR_PENDSTCLR_Msk             (1UL << SCB_ICSR_PENDSTCLR_Pos)                 

#define SCB_ICSR_ISRPREEMPT_Pos            23U                                             
#define SCB_ICSR_ISRPREEMPT_Msk            (1UL << SCB_ICSR_ISRPREEMPT_Pos)                

#define SCB_ICSR_ISRPENDING_Pos            22U                                             
#define SCB_ICSR_ISRPENDING_Msk            (1UL << SCB_ICSR_ISRPENDING_Pos)                

#define SCB_ICSR_VECTPENDING_Pos           12U                                             
#define SCB_ICSR_VECTPENDING_Msk           (0x1FFUL << SCB_ICSR_VECTPENDING_Pos)           

#define SCB_ICSR_VECTACTIVE_Pos             0U                                             
#define SCB_ICSR_VECTACTIVE_Msk            (0x1FFUL  )        

 
#define SCB_AIRCR_VECTKEY_Pos              16U                                             
#define SCB_AIRCR_VECTKEY_Msk              (0xFFFFUL << SCB_AIRCR_VECTKEY_Pos)             

#define SCB_AIRCR_VECTKEYSTAT_Pos          16U                                             
#define SCB_AIRCR_VECTKEYSTAT_Msk          (0xFFFFUL << SCB_AIRCR_VECTKEYSTAT_Pos)         

#define SCB_AIRCR_ENDIANESS_Pos            15U                                             
#define SCB_AIRCR_ENDIANESS_Msk            (1UL << SCB_AIRCR_ENDIANESS_Pos)                

#define SCB_AIRCR_SYSRESETREQ_Pos           2U                                             
#define SCB_AIRCR_SYSRESETREQ_Msk          (1UL << SCB_AIRCR_SYSRESETREQ_Pos)              

#define SCB_AIRCR_VECTCLRACTIVE_Pos         1U                                             
#define SCB_AIRCR_VECTCLRACTIVE_Msk        (1UL << SCB_AIRCR_VECTCLRACTIVE_Pos)            

 
#define SCB_SCR_SEVONPEND_Pos               4U                                             
#define SCB_SCR_SEVONPEND_Msk              (1UL << SCB_SCR_SEVONPEND_Pos)                  

#define SCB_SCR_SLEEPDEEP_Pos               2U                                             
#define SCB_SCR_SLEEPDEEP_Msk              (1UL << SCB_SCR_SLEEPDEEP_Pos)                  

#define SCB_SCR_SLEEPONEXIT_Pos             1U                                             
#define SCB_SCR_SLEEPONEXIT_Msk            (1UL << SCB_SCR_SLEEPONEXIT_Pos)                

 
#define SCB_CCR_STKALIGN_Pos                9U                                             
#define SCB_CCR_STKALIGN_Msk               (1UL << SCB_CCR_STKALIGN_Pos)                   

#define SCB_CCR_UNALIGN_TRP_Pos             3U                                             
#define SCB_CCR_UNALIGN_TRP_Msk            (1UL << SCB_CCR_UNALIGN_TRP_Pos)                

 
#define SCB_SHCSR_SVCALLPENDED_Pos         15U                                             
#define SCB_SHCSR_SVCALLPENDED_Msk         (1UL << SCB_SHCSR_SVCALLPENDED_Pos)             

 


 

 
typedef struct
{
        uint32_t RESERVED0[2U];
  __IOM uint32_t ACTLR;                   
} SCnSCB_Type;

 
#define SCnSCB_ACTLR_ITCMUAEN_Pos            4U                                         
#define SCnSCB_ACTLR_ITCMUAEN_Msk           (1UL << SCnSCB_ACTLR_ITCMUAEN_Pos)          

#define SCnSCB_ACTLR_ITCMLAEN_Pos            3U                                         
#define SCnSCB_ACTLR_ITCMLAEN_Msk           (1UL << SCnSCB_ACTLR_ITCMLAEN_Pos)          

 


 

 
typedef struct
{
  __IOM uint32_t CTRL;                    
  __IOM uint32_t LOAD;                    
  __IOM uint32_t VAL;                     
  __IM  uint32_t CALIB;                   
} SysTick_Type;

 
#define SysTick_CTRL_COUNTFLAG_Pos         16U                                             
#define SysTick_CTRL_COUNTFLAG_Msk         (1UL << SysTick_CTRL_COUNTFLAG_Pos)             

#define SysTick_CTRL_CLKSOURCE_Pos          2U                                             
#define SysTick_CTRL_CLKSOURCE_Msk         (1UL << SysTick_CTRL_CLKSOURCE_Pos)             

#define SysTick_CTRL_TICKINT_Pos            1U                                             
#define SysTick_CTRL_TICKINT_Msk           (1UL << SysTick_CTRL_TICKINT_Pos)               

#define SysTick_CTRL_ENABLE_Pos             0U                                             
#define SysTick_CTRL_ENABLE_Msk            (1UL  )            

 
#define SysTick_LOAD_RELOAD_Pos             0U                                             
#define SysTick_LOAD_RELOAD_Msk            (0xFFFFFFUL  )     

 
#define SysTick_VAL_CURRENT_Pos             0U                                             
#define SysTick_VAL_CURRENT_Msk            (0xFFFFFFUL  )     

 
#define SysTick_CALIB_NOREF_Pos            31U                                             
#define SysTick_CALIB_NOREF_Msk            (1UL << SysTick_CALIB_NOREF_Pos)                

#define SysTick_CALIB_SKEW_Pos             30U                                             
#define SysTick_CALIB_SKEW_Msk             (1UL << SysTick_CALIB_SKEW_Pos)                 

#define SysTick_CALIB_TENMS_Pos             0U                                             
#define SysTick_CALIB_TENMS_Msk            (0xFFFFFFUL  )     

 


 
 


 

 
#define _VAL2FLD(field, value)    (((uint32_t)(value) << field ## _Pos) & field ## _Msk)

 
#define _FLD2VAL(field, value)    (((uint32_t)(value) & field ## _Msk) >> field ## _Pos)

 


 

 
#define SCS_BASE            (0xE000E000UL)                             
#define SysTick_BASE        (SCS_BASE +  0x0010UL)                     
#define NVIC_BASE           (SCS_BASE +  0x0100UL)                     
#define SCB_BASE            (SCS_BASE +  0x0D00UL)                     

#define SCnSCB              ((SCnSCB_Type    *)     SCS_BASE      )    
#define SCB                 ((SCB_Type       *)     SCB_BASE      )    
#define SysTick             ((SysTick_Type   *)     SysTick_BASE  )    
#define NVIC                ((NVIC_Type      *)     NVIC_BASE     )    


 



 
 



 
 

#ifdef CMSIS_NVIC_VIRTUAL
  #ifndef CMSIS_NVIC_VIRTUAL_HEADER_FILE
    #define CMSIS_NVIC_VIRTUAL_HEADER_FILE "cmsis_nvic_virtual.h"
  #endif
  #include CMSIS_NVIC_VIRTUAL_HEADER_FILE
#else
  #define NVIC_SetPriorityGrouping    __NVIC_SetPriorityGrouping
  #define NVIC_GetPriorityGrouping    __NVIC_GetPriorityGrouping
  #define NVIC_EnableIRQ              __NVIC_EnableIRQ
  #define NVIC_GetEnableIRQ           __NVIC_GetEnableIRQ
  #define NVIC_DisableIRQ             __NVIC_DisableIRQ
  #define NVIC_GetPendingIRQ          __NVIC_GetPendingIRQ
  #define NVIC_SetPendingIRQ          __NVIC_SetPendingIRQ
  #define NVIC_ClearPendingIRQ        __NVIC_ClearPendingIRQ
 
  #define NVIC_SetPriority            __NVIC_SetPriority
  #define NVIC_GetPriority            __NVIC_GetPriority
  #define NVIC_SystemReset            __NVIC_SystemReset
#endif  

#ifdef CMSIS_VECTAB_VIRTUAL
  #ifndef CMSIS_VECTAB_VIRTUAL_HEADER_FILE
    #define CMSIS_VECTAB_VIRTUAL_HEADER_FILE "cmsis_vectab_virtual.h"
  #endif
  #include CMSIS_VECTAB_VIRTUAL_HEADER_FILE
#else
  #define NVIC_SetVector              __NVIC_SetVector
  #define NVIC_GetVector              __NVIC_GetVector
#endif   

#define NVIC_USER_IRQ_OFFSET          16


 
#define EXC_RETURN_HANDLER         (0xFFFFFFF1UL)      
#define EXC_RETURN_THREAD_MSP      (0xFFFFFFF9UL)      
#define EXC_RETURN_THREAD_PSP      (0xFFFFFFFDUL)      


 
 
#define _BIT_SHIFT(IRQn)         (  ((((uint32_t)(int32_t)(IRQn))         )      &  0x03UL) * 8UL)
#define _SHP_IDX(IRQn)           ( (((((uint32_t)(int32_t)(IRQn)) & 0x0FUL)-8UL) >>    2UL)      )
#define _IP_IDX(IRQn)            (   (((uint32_t)(int32_t)(IRQn))                >>    2UL)      )

#define __NVIC_SetPriorityGrouping(X) (void)(X)
#define __NVIC_GetPriorityGrouping()  (0U)

 
__STATIC_INLINE void __NVIC_EnableIRQ(IRQn_Type IRQn)
{
  if ((int32_t)(IRQn) >= 0)
  {
    __COMPILER_BARRIER();
    NVIC->ISER[0U] = (uint32_t)(1UL << (((uint32_t)IRQn) & 0x1FUL));
    __COMPILER_BARRIER();
  }
}


 
__STATIC_INLINE uint32_t __NVIC_GetEnableIRQ(IRQn_Type IRQn)
{
  if ((int32_t)(IRQn) >= 0)
  {
    return((uint32_t)(((NVIC->ISER[0U] & (1UL << (((uint32_t)IRQn) & 0x1FUL))) != 0UL) ? 1UL : 0UL));
  }
  else
  {
    return(0U);
  }
}


 
__STATIC_INLINE void __NVIC_DisableIRQ(IRQn_Type IRQn)
{
  if ((int32_t)(IRQn) >= 0)
  {
    NVIC->ICER[0U] = (uint32_t)(1UL << (((uint32_t)IRQn) & 0x1FUL));
    __DSB();
    __ISB();
  }
}


 
__STATIC_INLINE uint32_t __NVIC_GetPendingIRQ(IRQn_Type IRQn)
{
  if ((int32_t)(IRQn) >= 0)
  {
    return((uint32_t)(((NVIC->ISPR[0U] & (1UL << (((uint32_t)IRQn) & 0x1FUL))) != 0UL) ? 1UL : 0UL));
  }
  else
  {
    return(0U);
  }
}


 
__STATIC_INLINE void __NVIC_SetPendingIRQ(IRQn_Type IRQn)
{
  if ((int32_t)(IRQn) >= 0)
  {
    NVIC->ISPR[0U] = (uint32_t)(1UL << (((uint32_t)IRQn) & 0x1FUL));
  }
}


 
__STATIC_INLINE void __NVIC_ClearPendingIRQ(IRQn_Type IRQn)
{
  if ((int32_t)(IRQn) >= 0)
  {
    NVIC->ICPR[0U] = (uint32_t)(1UL << (((uint32_t)IRQn) & 0x1FUL));
  }
}


 
__STATIC_INLINE void __NVIC_SetPriority(IRQn_Type IRQn, uint32_t priority)
{
  if ((int32_t)(IRQn) >= 0)
  {
    NVIC->IP[_IP_IDX(IRQn)]  = ((uint32_t)(NVIC->IP[_IP_IDX(IRQn)]  & ~(0xFFUL << _BIT_SHIFT(IRQn))) |
       (((priority << (8U - __NVIC_PRIO_BITS)) & (uint32_t)0xFFUL) << _BIT_SHIFT(IRQn)));
  }
  else
  {
    SCB->SHP[_SHP_IDX(IRQn)] = ((uint32_t)(SCB->SHP[_SHP_IDX(IRQn)] & ~(0xFFUL << _BIT_SHIFT(IRQn))) |
       (((priority << (8U - __NVIC_PRIO_BITS)) & (uint32_t)0xFFUL) << _BIT_SHIFT(IRQn)));
  }
}


 
__STATIC_INLINE uint32_t __NVIC_GetPriority(IRQn_Type IRQn)
{

  if ((int32_t)(IRQn) >= 0)
  {
    return((uint32_t)(((NVIC->IP[ _IP_IDX(IRQn)] >> _BIT_SHIFT(IRQn) ) & (uint32_t)0xFFUL) >> (8U - __NVIC_PRIO_BITS)));
  }
  else
  {
    return((uint32_t)(((SCB->SHP[_SHP_IDX(IRQn)] >> _BIT_SHIFT(IRQn) ) & (uint32_t)0xFFUL) >> (8U - __NVIC_PRIO_BITS)));
  }
}


 
__STATIC_INLINE uint32_t NVIC_EncodePriority (uint32_t PriorityGroup, uint32_t PreemptPriority, uint32_t SubPriority)
{
  uint32_t PriorityGroupTmp = (PriorityGroup & (uint32_t)0x07UL);    
  uint32_t PreemptPriorityBits;
  uint32_t SubPriorityBits;

  PreemptPriorityBits = ((7UL - PriorityGroupTmp) > (uint32_t)(__NVIC_PRIO_BITS)) ? (uint32_t)(__NVIC_PRIO_BITS) : (uint32_t)(7UL - PriorityGroupTmp);
  SubPriorityBits     = ((PriorityGroupTmp + (uint32_t)(__NVIC_PRIO_BITS)) < (uint32_t)7UL) ? (uint32_t)0UL : (uint32_t)((PriorityGroupTmp - 7UL) + (uint32_t)(__NVIC_PRIO_BITS));

  return (
           ((PreemptPriority & (uint32_t)((1UL << (PreemptPriorityBits)) - 1UL)) << SubPriorityBits) |
           ((SubPriority     & (uint32_t)((1UL << (SubPriorityBits    )) - 1UL)))
         );
}


 
__STATIC_INLINE void NVIC_DecodePriority (uint32_t Priority, uint32_t PriorityGroup, uint32_t* const pPreemptPriority, uint32_t* const pSubPriority)
{
  uint32_t PriorityGroupTmp = (PriorityGroup & (uint32_t)0x07UL);    
  uint32_t PreemptPriorityBits;
  uint32_t SubPriorityBits;

  PreemptPriorityBits = ((7UL - PriorityGroupTmp) > (uint32_t)(__NVIC_PRIO_BITS)) ? (uint32_t)(__NVIC_PRIO_BITS) : (uint32_t)(7UL - PriorityGroupTmp);
  SubPriorityBits     = ((PriorityGroupTmp + (uint32_t)(__NVIC_PRIO_BITS)) < (uint32_t)7UL) ? (uint32_t)0UL : (uint32_t)((PriorityGroupTmp - 7UL) + (uint32_t)(__NVIC_PRIO_BITS));

  *pPreemptPriority = (Priority >> SubPriorityBits) & (uint32_t)((1UL << (PreemptPriorityBits)) - 1UL);
  *pSubPriority     = (Priority                   ) & (uint32_t)((1UL << (SubPriorityBits    )) - 1UL);
}



 
__STATIC_INLINE void __NVIC_SetVector(IRQn_Type IRQn, uint32_t vector)
{
  uint32_t *vectors = (uint32_t *)0x0U;
  vectors[(int32_t)IRQn + NVIC_USER_IRQ_OFFSET] = vector;
   
}


 
__STATIC_INLINE uint32_t __NVIC_GetVector(IRQn_Type IRQn)
{
  uint32_t *vectors = (uint32_t *)0x0U;
  return vectors[(int32_t)IRQn + NVIC_USER_IRQ_OFFSET];
}


 
__NO_RETURN __STATIC_INLINE void __NVIC_SystemReset(void)
{
  __DSB();                                                           
  SCB->AIRCR  = ((0x5FAUL << SCB_AIRCR_VECTKEY_Pos) |
                 SCB_AIRCR_SYSRESETREQ_Msk);
  __DSB();                                                           

  for(;;)                                                            
  {
    __NOP();
  }
}

 


 
 

 
__STATIC_INLINE uint32_t SCB_GetFPUType(void)
{
    return 0U;            
}


 



 
 

#if defined (__Vendor_SysTickConfig) && (__Vendor_SysTickConfig == 0U)

 
__STATIC_INLINE uint32_t SysTick_Config(uint32_t ticks)
{
  if ((ticks - 1UL) > SysTick_LOAD_RELOAD_Msk)
  {
    return (1UL);                                                    
  }

  SysTick->LOAD  = (uint32_t)(ticks - 1UL);                          
  NVIC_SetPriority (SysTick_IRQn, (1UL << __NVIC_PRIO_BITS) - 1UL);  
  SysTick->VAL   = 0UL;                                              
  SysTick->CTRL  = SysTick_CTRL_CLKSOURCE_Msk |
                   SysTick_CTRL_TICKINT_Msk   |
                   SysTick_CTRL_ENABLE_Msk;                          
  return (0UL);                                                      
}

#endif

 




#ifdef __cplusplus
}
#endif

#endif  

#endif  
