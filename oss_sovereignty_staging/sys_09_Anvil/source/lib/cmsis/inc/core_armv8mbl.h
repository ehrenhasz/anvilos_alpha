 
 

#if   defined ( __ICCARM__ )
  #pragma system_include                         
#elif defined (__clang__)
  #pragma clang system_header                    
#elif defined ( __GNUC__ )
  #pragma GCC diagnostic ignored "-Wpedantic"    
#endif

#ifndef __CORE_ARMV8MBL_H_GENERIC
#define __CORE_ARMV8MBL_H_GENERIC

#include <stdint.h>

#ifdef __cplusplus
 extern "C" {
#endif

 


 
 

#include "cmsis_version.h"

 
#define __ARMv8MBL_CMSIS_VERSION_MAIN  (__CM_CMSIS_VERSION_MAIN)                    
#define __ARMv8MBL_CMSIS_VERSION_SUB   (__CM_CMSIS_VERSION_SUB)                     
#define __ARMv8MBL_CMSIS_VERSION       ((__ARMv8MBL_CMSIS_VERSION_MAIN << 16U) | \
                                         __ARMv8MBL_CMSIS_VERSION_SUB           )   

#define __CORTEX_M                     (2U)                                         

 
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

#ifndef __CORE_ARMV8MBL_H_DEPENDANT
#define __CORE_ARMV8MBL_H_DEPENDANT

#ifdef __cplusplus
 extern "C" {
#endif

 
#if defined __CHECK_DEVICE_DEFINES
  #ifndef __ARMv8MBL_REV
    #define __ARMv8MBL_REV               0x0000U
    #warning "__ARMv8MBL_REV not defined in device header file; using default!"
  #endif

  #ifndef __FPU_PRESENT
    #define __FPU_PRESENT             0U
    #warning "__FPU_PRESENT not defined in device header file; using default!"
  #endif

  #ifndef __MPU_PRESENT
    #define __MPU_PRESENT             0U
    #warning "__MPU_PRESENT not defined in device header file; using default!"
  #endif

  #ifndef __SAUREGION_PRESENT
    #define __SAUREGION_PRESENT       0U
    #warning "__SAUREGION_PRESENT not defined in device header file; using default!"
  #endif

  #ifndef __VTOR_PRESENT
    #define __VTOR_PRESENT            0U
    #warning "__VTOR_PRESENT not defined in device header file; using default!"
  #endif

  #ifndef __NVIC_PRIO_BITS
    #define __NVIC_PRIO_BITS          2U
    #warning "__NVIC_PRIO_BITS not defined in device header file; using default!"
  #endif

  #ifndef __Vendor_SysTickConfig
    #define __Vendor_SysTickConfig    0U
    #warning "__Vendor_SysTickConfig not defined in device header file; using default!"
  #endif

  #ifndef __ETM_PRESENT
    #define __ETM_PRESENT             0U
    #warning "__ETM_PRESENT not defined in device header file; using default!"
  #endif

  #ifndef __MTB_PRESENT
    #define __MTB_PRESENT             0U
    #warning "__MTB_PRESENT not defined in device header file; using default!"
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
    uint32_t nPRIV:1;                     
    uint32_t SPSEL:1;                     
    uint32_t _reserved1:30;               
  } b;                                    
  uint32_t w;                             
} CONTROL_Type;

 
#define CONTROL_SPSEL_Pos                   1U                                             
#define CONTROL_SPSEL_Msk                  (1UL << CONTROL_SPSEL_Pos)                      

#define CONTROL_nPRIV_Pos                   0U                                             
#define CONTROL_nPRIV_Msk                  (1UL  )                  

 


 

 
typedef struct
{
  __IOM uint32_t ISER[16U];               
        uint32_t RESERVED0[16U];
  __IOM uint32_t ICER[16U];               
        uint32_t RSERVED1[16U];
  __IOM uint32_t ISPR[16U];               
        uint32_t RESERVED2[16U];
  __IOM uint32_t ICPR[16U];               
        uint32_t RESERVED3[16U];
  __IOM uint32_t IABR[16U];               
        uint32_t RESERVED4[16U];
  __IOM uint32_t ITNS[16U];               
        uint32_t RESERVED5[16U];
  __IOM uint32_t IPR[124U];               
}  NVIC_Type;

 


 

 
typedef struct
{
  __IM  uint32_t CPUID;                   
  __IOM uint32_t ICSR;                    
#if defined (__VTOR_PRESENT) && (__VTOR_PRESENT == 1U)
  __IOM uint32_t VTOR;                    
#else
        uint32_t RESERVED0;
#endif
  __IOM uint32_t AIRCR;                   
  __IOM uint32_t SCR;                     
  __IOM uint32_t CCR;                     
        uint32_t RESERVED1;
  __IOM uint32_t SHPR[2U];                
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

 
#define SCB_ICSR_PENDNMISET_Pos            31U                                             
#define SCB_ICSR_PENDNMISET_Msk            (1UL << SCB_ICSR_PENDNMISET_Pos)                

#define SCB_ICSR_NMIPENDSET_Pos            SCB_ICSR_PENDNMISET_Pos                         
#define SCB_ICSR_NMIPENDSET_Msk            SCB_ICSR_PENDNMISET_Msk                         

#define SCB_ICSR_PENDNMICLR_Pos            30U                                             
#define SCB_ICSR_PENDNMICLR_Msk            (1UL << SCB_ICSR_PENDNMICLR_Pos)                

#define SCB_ICSR_PENDSVSET_Pos             28U                                             
#define SCB_ICSR_PENDSVSET_Msk             (1UL << SCB_ICSR_PENDSVSET_Pos)                 

#define SCB_ICSR_PENDSVCLR_Pos             27U                                             
#define SCB_ICSR_PENDSVCLR_Msk             (1UL << SCB_ICSR_PENDSVCLR_Pos)                 

#define SCB_ICSR_PENDSTSET_Pos             26U                                             
#define SCB_ICSR_PENDSTSET_Msk             (1UL << SCB_ICSR_PENDSTSET_Pos)                 

#define SCB_ICSR_PENDSTCLR_Pos             25U                                             
#define SCB_ICSR_PENDSTCLR_Msk             (1UL << SCB_ICSR_PENDSTCLR_Pos)                 

#define SCB_ICSR_STTNS_Pos                 24U                                             
#define SCB_ICSR_STTNS_Msk                 (1UL << SCB_ICSR_STTNS_Pos)                     

#define SCB_ICSR_ISRPREEMPT_Pos            23U                                             
#define SCB_ICSR_ISRPREEMPT_Msk            (1UL << SCB_ICSR_ISRPREEMPT_Pos)                

#define SCB_ICSR_ISRPENDING_Pos            22U                                             
#define SCB_ICSR_ISRPENDING_Msk            (1UL << SCB_ICSR_ISRPENDING_Pos)                

#define SCB_ICSR_VECTPENDING_Pos           12U                                             
#define SCB_ICSR_VECTPENDING_Msk           (0x1FFUL << SCB_ICSR_VECTPENDING_Pos)           

#define SCB_ICSR_RETTOBASE_Pos             11U                                             
#define SCB_ICSR_RETTOBASE_Msk             (1UL << SCB_ICSR_RETTOBASE_Pos)                 

#define SCB_ICSR_VECTACTIVE_Pos             0U                                             
#define SCB_ICSR_VECTACTIVE_Msk            (0x1FFUL  )        

#if defined (__VTOR_PRESENT) && (__VTOR_PRESENT == 1U)
 
#define SCB_VTOR_TBLOFF_Pos                 7U                                             
#define SCB_VTOR_TBLOFF_Msk                (0x1FFFFFFUL << SCB_VTOR_TBLOFF_Pos)            
#endif

 
#define SCB_AIRCR_VECTKEY_Pos              16U                                             
#define SCB_AIRCR_VECTKEY_Msk              (0xFFFFUL << SCB_AIRCR_VECTKEY_Pos)             

#define SCB_AIRCR_VECTKEYSTAT_Pos          16U                                             
#define SCB_AIRCR_VECTKEYSTAT_Msk          (0xFFFFUL << SCB_AIRCR_VECTKEYSTAT_Pos)         

#define SCB_AIRCR_ENDIANESS_Pos            15U                                             
#define SCB_AIRCR_ENDIANESS_Msk            (1UL << SCB_AIRCR_ENDIANESS_Pos)                

#define SCB_AIRCR_PRIS_Pos                 14U                                             
#define SCB_AIRCR_PRIS_Msk                 (1UL << SCB_AIRCR_PRIS_Pos)                     

#define SCB_AIRCR_BFHFNMINS_Pos            13U                                             
#define SCB_AIRCR_BFHFNMINS_Msk            (1UL << SCB_AIRCR_BFHFNMINS_Pos)                

#define SCB_AIRCR_SYSRESETREQS_Pos          3U                                             
#define SCB_AIRCR_SYSRESETREQS_Msk         (1UL << SCB_AIRCR_SYSRESETREQS_Pos)             

#define SCB_AIRCR_SYSRESETREQ_Pos           2U                                             
#define SCB_AIRCR_SYSRESETREQ_Msk          (1UL << SCB_AIRCR_SYSRESETREQ_Pos)              

#define SCB_AIRCR_VECTCLRACTIVE_Pos         1U                                             
#define SCB_AIRCR_VECTCLRACTIVE_Msk        (1UL << SCB_AIRCR_VECTCLRACTIVE_Pos)            

 
#define SCB_SCR_SEVONPEND_Pos               4U                                             
#define SCB_SCR_SEVONPEND_Msk              (1UL << SCB_SCR_SEVONPEND_Pos)                  

#define SCB_SCR_SLEEPDEEPS_Pos              3U                                             
#define SCB_SCR_SLEEPDEEPS_Msk             (1UL << SCB_SCR_SLEEPDEEPS_Pos)                 

#define SCB_SCR_SLEEPDEEP_Pos               2U                                             
#define SCB_SCR_SLEEPDEEP_Msk              (1UL << SCB_SCR_SLEEPDEEP_Pos)                  

#define SCB_SCR_SLEEPONEXIT_Pos             1U                                             
#define SCB_SCR_SLEEPONEXIT_Msk            (1UL << SCB_SCR_SLEEPONEXIT_Pos)                

 
#define SCB_CCR_BP_Pos                     18U                                             
#define SCB_CCR_BP_Msk                     (1UL << SCB_CCR_BP_Pos)                         

#define SCB_CCR_IC_Pos                     17U                                             
#define SCB_CCR_IC_Msk                     (1UL << SCB_CCR_IC_Pos)                         

#define SCB_CCR_DC_Pos                     16U                                             
#define SCB_CCR_DC_Msk                     (1UL << SCB_CCR_DC_Pos)                         

#define SCB_CCR_STKOFHFNMIGN_Pos           10U                                             
#define SCB_CCR_STKOFHFNMIGN_Msk           (1UL << SCB_CCR_STKOFHFNMIGN_Pos)               

#define SCB_CCR_BFHFNMIGN_Pos               8U                                             
#define SCB_CCR_BFHFNMIGN_Msk              (1UL << SCB_CCR_BFHFNMIGN_Pos)                  

#define SCB_CCR_DIV_0_TRP_Pos               4U                                             
#define SCB_CCR_DIV_0_TRP_Msk              (1UL << SCB_CCR_DIV_0_TRP_Pos)                  

#define SCB_CCR_UNALIGN_TRP_Pos             3U                                             
#define SCB_CCR_UNALIGN_TRP_Msk            (1UL << SCB_CCR_UNALIGN_TRP_Pos)                

#define SCB_CCR_USERSETMPEND_Pos            1U                                             
#define SCB_CCR_USERSETMPEND_Msk           (1UL << SCB_CCR_USERSETMPEND_Pos)               

 
#define SCB_SHCSR_HARDFAULTPENDED_Pos      21U                                             
#define SCB_SHCSR_HARDFAULTPENDED_Msk      (1UL << SCB_SHCSR_HARDFAULTPENDED_Pos)          

#define SCB_SHCSR_SVCALLPENDED_Pos         15U                                             
#define SCB_SHCSR_SVCALLPENDED_Msk         (1UL << SCB_SHCSR_SVCALLPENDED_Pos)             

#define SCB_SHCSR_SYSTICKACT_Pos           11U                                             
#define SCB_SHCSR_SYSTICKACT_Msk           (1UL << SCB_SHCSR_SYSTICKACT_Pos)               

#define SCB_SHCSR_PENDSVACT_Pos            10U                                             
#define SCB_SHCSR_PENDSVACT_Msk            (1UL << SCB_SHCSR_PENDSVACT_Pos)                

#define SCB_SHCSR_SVCALLACT_Pos             7U                                             
#define SCB_SHCSR_SVCALLACT_Msk            (1UL << SCB_SHCSR_SVCALLACT_Pos)                

#define SCB_SHCSR_NMIACT_Pos                5U                                             
#define SCB_SHCSR_NMIACT_Msk               (1UL << SCB_SHCSR_NMIACT_Pos)                   

#define SCB_SHCSR_HARDFAULTACT_Pos          2U                                             
#define SCB_SHCSR_HARDFAULTACT_Msk         (1UL << SCB_SHCSR_HARDFAULTACT_Pos)             

 


 

 
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

 


 

 
typedef struct
{
  __IOM uint32_t CTRL;                    
        uint32_t RESERVED0[6U];
  __IM  uint32_t PCSR;                    
  __IOM uint32_t COMP0;                   
        uint32_t RESERVED1[1U];
  __IOM uint32_t FUNCTION0;               
        uint32_t RESERVED2[1U];
  __IOM uint32_t COMP1;                   
        uint32_t RESERVED3[1U];
  __IOM uint32_t FUNCTION1;               
        uint32_t RESERVED4[1U];
  __IOM uint32_t COMP2;                   
        uint32_t RESERVED5[1U];
  __IOM uint32_t FUNCTION2;               
        uint32_t RESERVED6[1U];
  __IOM uint32_t COMP3;                   
        uint32_t RESERVED7[1U];
  __IOM uint32_t FUNCTION3;               
        uint32_t RESERVED8[1U];
  __IOM uint32_t COMP4;                   
        uint32_t RESERVED9[1U];
  __IOM uint32_t FUNCTION4;               
        uint32_t RESERVED10[1U];
  __IOM uint32_t COMP5;                   
        uint32_t RESERVED11[1U];
  __IOM uint32_t FUNCTION5;               
        uint32_t RESERVED12[1U];
  __IOM uint32_t COMP6;                   
        uint32_t RESERVED13[1U];
  __IOM uint32_t FUNCTION6;               
        uint32_t RESERVED14[1U];
  __IOM uint32_t COMP7;                   
        uint32_t RESERVED15[1U];
  __IOM uint32_t FUNCTION7;               
        uint32_t RESERVED16[1U];
  __IOM uint32_t COMP8;                   
        uint32_t RESERVED17[1U];
  __IOM uint32_t FUNCTION8;               
        uint32_t RESERVED18[1U];
  __IOM uint32_t COMP9;                   
        uint32_t RESERVED19[1U];
  __IOM uint32_t FUNCTION9;               
        uint32_t RESERVED20[1U];
  __IOM uint32_t COMP10;                  
        uint32_t RESERVED21[1U];
  __IOM uint32_t FUNCTION10;              
        uint32_t RESERVED22[1U];
  __IOM uint32_t COMP11;                  
        uint32_t RESERVED23[1U];
  __IOM uint32_t FUNCTION11;              
        uint32_t RESERVED24[1U];
  __IOM uint32_t COMP12;                  
        uint32_t RESERVED25[1U];
  __IOM uint32_t FUNCTION12;              
        uint32_t RESERVED26[1U];
  __IOM uint32_t COMP13;                  
        uint32_t RESERVED27[1U];
  __IOM uint32_t FUNCTION13;              
        uint32_t RESERVED28[1U];
  __IOM uint32_t COMP14;                  
        uint32_t RESERVED29[1U];
  __IOM uint32_t FUNCTION14;              
        uint32_t RESERVED30[1U];
  __IOM uint32_t COMP15;                  
        uint32_t RESERVED31[1U];
  __IOM uint32_t FUNCTION15;              
} DWT_Type;

 
#define DWT_CTRL_NUMCOMP_Pos               28U                                          
#define DWT_CTRL_NUMCOMP_Msk               (0xFUL << DWT_CTRL_NUMCOMP_Pos)              

#define DWT_CTRL_NOTRCPKT_Pos              27U                                          
#define DWT_CTRL_NOTRCPKT_Msk              (0x1UL << DWT_CTRL_NOTRCPKT_Pos)             

#define DWT_CTRL_NOEXTTRIG_Pos             26U                                          
#define DWT_CTRL_NOEXTTRIG_Msk             (0x1UL << DWT_CTRL_NOEXTTRIG_Pos)            

#define DWT_CTRL_NOCYCCNT_Pos              25U                                          
#define DWT_CTRL_NOCYCCNT_Msk              (0x1UL << DWT_CTRL_NOCYCCNT_Pos)             

#define DWT_CTRL_NOPRFCNT_Pos              24U                                          
#define DWT_CTRL_NOPRFCNT_Msk              (0x1UL << DWT_CTRL_NOPRFCNT_Pos)             

 
#define DWT_FUNCTION_ID_Pos                27U                                          
#define DWT_FUNCTION_ID_Msk                (0x1FUL << DWT_FUNCTION_ID_Pos)              

#define DWT_FUNCTION_MATCHED_Pos           24U                                          
#define DWT_FUNCTION_MATCHED_Msk           (0x1UL << DWT_FUNCTION_MATCHED_Pos)          

#define DWT_FUNCTION_DATAVSIZE_Pos         10U                                          
#define DWT_FUNCTION_DATAVSIZE_Msk         (0x3UL << DWT_FUNCTION_DATAVSIZE_Pos)        

#define DWT_FUNCTION_ACTION_Pos             4U                                          
#define DWT_FUNCTION_ACTION_Msk            (0x3UL << DWT_FUNCTION_ACTION_Pos)           

#define DWT_FUNCTION_MATCH_Pos              0U                                          
#define DWT_FUNCTION_MATCH_Msk             (0xFUL  )        

   


 

 
typedef struct
{
  __IM  uint32_t SSPSR;                   
  __IOM uint32_t CSPSR;                   
        uint32_t RESERVED0[2U];
  __IOM uint32_t ACPR;                    
        uint32_t RESERVED1[55U];
  __IOM uint32_t SPPR;                    
        uint32_t RESERVED2[131U];
  __IM  uint32_t FFSR;                    
  __IOM uint32_t FFCR;                    
  __IOM uint32_t PSCR;                    
        uint32_t RESERVED3[809U];
  __OM  uint32_t LAR;                     
  __IM  uint32_t LSR;                     
        uint32_t RESERVED4[4U];
  __IM  uint32_t TYPE;                    
  __IM  uint32_t DEVTYPE;                 
} TPI_Type;

 
#define TPI_ACPR_SWOSCALER_Pos              0U                                          
#define TPI_ACPR_SWOSCALER_Msk             (0xFFFFUL  )     

 
#define TPI_SPPR_TXMODE_Pos                 0U                                          
#define TPI_SPPR_TXMODE_Msk                (0x3UL  )           

 
#define TPI_FFSR_FtNonStop_Pos              3U                                          
#define TPI_FFSR_FtNonStop_Msk             (0x1UL << TPI_FFSR_FtNonStop_Pos)            

#define TPI_FFSR_TCPresent_Pos              2U                                          
#define TPI_FFSR_TCPresent_Msk             (0x1UL << TPI_FFSR_TCPresent_Pos)            

#define TPI_FFSR_FtStopped_Pos              1U                                          
#define TPI_FFSR_FtStopped_Msk             (0x1UL << TPI_FFSR_FtStopped_Pos)            

#define TPI_FFSR_FlInProg_Pos               0U                                          
#define TPI_FFSR_FlInProg_Msk              (0x1UL  )         

 
#define TPI_FFCR_TrigIn_Pos                 8U                                          
#define TPI_FFCR_TrigIn_Msk                (0x1UL << TPI_FFCR_TrigIn_Pos)               

#define TPI_FFCR_FOnMan_Pos                 6U                                          
#define TPI_FFCR_FOnMan_Msk                (0x1UL << TPI_FFCR_FOnMan_Pos)               

#define TPI_FFCR_EnFCont_Pos                1U                                          
#define TPI_FFCR_EnFCont_Msk               (0x1UL << TPI_FFCR_EnFCont_Pos)              

 
#define TPI_PSCR_PSCount_Pos                0U                                          
#define TPI_PSCR_PSCount_Msk               (0x1FUL  )         

 
#define TPI_LSR_nTT_Pos                     1U                                          
#define TPI_LSR_nTT_Msk                    (0x1UL << TPI_LSR_nTT_Pos)                   

#define TPI_LSR_SLK_Pos                     1U                                          
#define TPI_LSR_SLK_Msk                    (0x1UL << TPI_LSR_SLK_Pos)                   

#define TPI_LSR_SLI_Pos                     0U                                          
#define TPI_LSR_SLI_Msk                    (0x1UL  )               

 
#define TPI_DEVID_NRZVALID_Pos             11U                                          
#define TPI_DEVID_NRZVALID_Msk             (0x1UL << TPI_DEVID_NRZVALID_Pos)            

#define TPI_DEVID_MANCVALID_Pos            10U                                          
#define TPI_DEVID_MANCVALID_Msk            (0x1UL << TPI_DEVID_MANCVALID_Pos)           

#define TPI_DEVID_PTINVALID_Pos             9U                                          
#define TPI_DEVID_PTINVALID_Msk            (0x1UL << TPI_DEVID_PTINVALID_Pos)           

#define TPI_DEVID_FIFOSZ_Pos                6U                                          
#define TPI_DEVID_FIFOSZ_Msk               (0x7UL << TPI_DEVID_FIFOSZ_Pos)              

 
#define TPI_DEVTYPE_SubType_Pos             4U                                          
#define TPI_DEVTYPE_SubType_Msk            (0xFUL  )       

#define TPI_DEVTYPE_MajorType_Pos           0U                                          
#define TPI_DEVTYPE_MajorType_Msk          (0xFUL << TPI_DEVTYPE_MajorType_Pos)         

   


#if defined (__MPU_PRESENT) && (__MPU_PRESENT == 1U)
 

 
typedef struct
{
  __IM  uint32_t TYPE;                    
  __IOM uint32_t CTRL;                    
  __IOM uint32_t RNR;                     
  __IOM uint32_t RBAR;                    
  __IOM uint32_t RLAR;                    
        uint32_t RESERVED0[7U];
  union {
  __IOM uint32_t MAIR[2];
  struct {
  __IOM uint32_t MAIR0;                   
  __IOM uint32_t MAIR1;                   
  };
  };
} MPU_Type;

#define MPU_TYPE_RALIASES                  1U

 
#define MPU_TYPE_IREGION_Pos               16U                                             
#define MPU_TYPE_IREGION_Msk               (0xFFUL << MPU_TYPE_IREGION_Pos)                

#define MPU_TYPE_DREGION_Pos                8U                                             
#define MPU_TYPE_DREGION_Msk               (0xFFUL << MPU_TYPE_DREGION_Pos)                

#define MPU_TYPE_SEPARATE_Pos               0U                                             
#define MPU_TYPE_SEPARATE_Msk              (1UL  )              

 
#define MPU_CTRL_PRIVDEFENA_Pos             2U                                             
#define MPU_CTRL_PRIVDEFENA_Msk            (1UL << MPU_CTRL_PRIVDEFENA_Pos)                

#define MPU_CTRL_HFNMIENA_Pos               1U                                             
#define MPU_CTRL_HFNMIENA_Msk              (1UL << MPU_CTRL_HFNMIENA_Pos)                  

#define MPU_CTRL_ENABLE_Pos                 0U                                             
#define MPU_CTRL_ENABLE_Msk                (1UL  )                

 
#define MPU_RNR_REGION_Pos                  0U                                             
#define MPU_RNR_REGION_Msk                 (0xFFUL  )              

 
#define MPU_RBAR_BASE_Pos                   5U                                             
#define MPU_RBAR_BASE_Msk                  (0x7FFFFFFUL << MPU_RBAR_BASE_Pos)              

#define MPU_RBAR_SH_Pos                     3U                                             
#define MPU_RBAR_SH_Msk                    (0x3UL << MPU_RBAR_SH_Pos)                      

#define MPU_RBAR_AP_Pos                     1U                                             
#define MPU_RBAR_AP_Msk                    (0x3UL << MPU_RBAR_AP_Pos)                      

#define MPU_RBAR_XN_Pos                     0U                                             
#define MPU_RBAR_XN_Msk                    (01UL  )                   

 
#define MPU_RLAR_LIMIT_Pos                  5U                                             
#define MPU_RLAR_LIMIT_Msk                 (0x7FFFFFFUL << MPU_RLAR_LIMIT_Pos)             

#define MPU_RLAR_AttrIndx_Pos               1U                                             
#define MPU_RLAR_AttrIndx_Msk              (0x7UL << MPU_RLAR_AttrIndx_Pos)                

#define MPU_RLAR_EN_Pos                     0U                                             
#define MPU_RLAR_EN_Msk                    (1UL  )                    

 
#define MPU_MAIR0_Attr3_Pos                24U                                             
#define MPU_MAIR0_Attr3_Msk                (0xFFUL << MPU_MAIR0_Attr3_Pos)                 

#define MPU_MAIR0_Attr2_Pos                16U                                             
#define MPU_MAIR0_Attr2_Msk                (0xFFUL << MPU_MAIR0_Attr2_Pos)                 

#define MPU_MAIR0_Attr1_Pos                 8U                                             
#define MPU_MAIR0_Attr1_Msk                (0xFFUL << MPU_MAIR0_Attr1_Pos)                 

#define MPU_MAIR0_Attr0_Pos                 0U                                             
#define MPU_MAIR0_Attr0_Msk                (0xFFUL  )             

 
#define MPU_MAIR1_Attr7_Pos                24U                                             
#define MPU_MAIR1_Attr7_Msk                (0xFFUL << MPU_MAIR1_Attr7_Pos)                 

#define MPU_MAIR1_Attr6_Pos                16U                                             
#define MPU_MAIR1_Attr6_Msk                (0xFFUL << MPU_MAIR1_Attr6_Pos)                 

#define MPU_MAIR1_Attr5_Pos                 8U                                             
#define MPU_MAIR1_Attr5_Msk                (0xFFUL << MPU_MAIR1_Attr5_Pos)                 

#define MPU_MAIR1_Attr4_Pos                 0U                                             
#define MPU_MAIR1_Attr4_Msk                (0xFFUL  )             

 
#endif


#if defined (__ARM_FEATURE_CMSE) && (__ARM_FEATURE_CMSE == 3U)
 

 
typedef struct
{
  __IOM uint32_t CTRL;                    
  __IM  uint32_t TYPE;                    
#if defined (__SAUREGION_PRESENT) && (__SAUREGION_PRESENT == 1U)
  __IOM uint32_t RNR;                     
  __IOM uint32_t RBAR;                    
  __IOM uint32_t RLAR;                    
#endif
} SAU_Type;

 
#define SAU_CTRL_ALLNS_Pos                  1U                                             
#define SAU_CTRL_ALLNS_Msk                 (1UL << SAU_CTRL_ALLNS_Pos)                     

#define SAU_CTRL_ENABLE_Pos                 0U                                             
#define SAU_CTRL_ENABLE_Msk                (1UL  )                

 
#define SAU_TYPE_SREGION_Pos                0U                                             
#define SAU_TYPE_SREGION_Msk               (0xFFUL  )            

#if defined (__SAUREGION_PRESENT) && (__SAUREGION_PRESENT == 1U)
 
#define SAU_RNR_REGION_Pos                  0U                                             
#define SAU_RNR_REGION_Msk                 (0xFFUL  )              

 
#define SAU_RBAR_BADDR_Pos                  5U                                             
#define SAU_RBAR_BADDR_Msk                 (0x7FFFFFFUL << SAU_RBAR_BADDR_Pos)             

 
#define SAU_RLAR_LADDR_Pos                  5U                                             
#define SAU_RLAR_LADDR_Msk                 (0x7FFFFFFUL << SAU_RLAR_LADDR_Pos)             

#define SAU_RLAR_NSC_Pos                    1U                                             
#define SAU_RLAR_NSC_Msk                   (1UL << SAU_RLAR_NSC_Pos)                       

#define SAU_RLAR_ENABLE_Pos                 0U                                             
#define SAU_RLAR_ENABLE_Msk                (1UL  )                

#endif  

 
#endif  


 
 

 
typedef struct
{
  __IOM uint32_t DHCSR;                   
  __OM  uint32_t DCRSR;                   
  __IOM uint32_t DCRDR;                   
  __IOM uint32_t DEMCR;                   
        uint32_t RESERVED0[1U];
  __IOM uint32_t DAUTHCTRL;               
  __IOM uint32_t DSCSR;                   
} CoreDebug_Type;

 
#define CoreDebug_DHCSR_DBGKEY_Pos         16U                                             
#define CoreDebug_DHCSR_DBGKEY_Msk         (0xFFFFUL << CoreDebug_DHCSR_DBGKEY_Pos)        

#define CoreDebug_DHCSR_S_RESTART_ST_Pos   26U                                             
#define CoreDebug_DHCSR_S_RESTART_ST_Msk   (1UL << CoreDebug_DHCSR_S_RESTART_ST_Pos)       

#define CoreDebug_DHCSR_S_RESET_ST_Pos     25U                                             
#define CoreDebug_DHCSR_S_RESET_ST_Msk     (1UL << CoreDebug_DHCSR_S_RESET_ST_Pos)         

#define CoreDebug_DHCSR_S_RETIRE_ST_Pos    24U                                             
#define CoreDebug_DHCSR_S_RETIRE_ST_Msk    (1UL << CoreDebug_DHCSR_S_RETIRE_ST_Pos)        

#define CoreDebug_DHCSR_S_LOCKUP_Pos       19U                                             
#define CoreDebug_DHCSR_S_LOCKUP_Msk       (1UL << CoreDebug_DHCSR_S_LOCKUP_Pos)           

#define CoreDebug_DHCSR_S_SLEEP_Pos        18U                                             
#define CoreDebug_DHCSR_S_SLEEP_Msk        (1UL << CoreDebug_DHCSR_S_SLEEP_Pos)            

#define CoreDebug_DHCSR_S_HALT_Pos         17U                                             
#define CoreDebug_DHCSR_S_HALT_Msk         (1UL << CoreDebug_DHCSR_S_HALT_Pos)             

#define CoreDebug_DHCSR_S_REGRDY_Pos       16U                                             
#define CoreDebug_DHCSR_S_REGRDY_Msk       (1UL << CoreDebug_DHCSR_S_REGRDY_Pos)           

#define CoreDebug_DHCSR_C_MASKINTS_Pos      3U                                             
#define CoreDebug_DHCSR_C_MASKINTS_Msk     (1UL << CoreDebug_DHCSR_C_MASKINTS_Pos)         

#define CoreDebug_DHCSR_C_STEP_Pos          2U                                             
#define CoreDebug_DHCSR_C_STEP_Msk         (1UL << CoreDebug_DHCSR_C_STEP_Pos)             

#define CoreDebug_DHCSR_C_HALT_Pos          1U                                             
#define CoreDebug_DHCSR_C_HALT_Msk         (1UL << CoreDebug_DHCSR_C_HALT_Pos)             

#define CoreDebug_DHCSR_C_DEBUGEN_Pos       0U                                             
#define CoreDebug_DHCSR_C_DEBUGEN_Msk      (1UL  )      

 
#define CoreDebug_DCRSR_REGWnR_Pos         16U                                             
#define CoreDebug_DCRSR_REGWnR_Msk         (1UL << CoreDebug_DCRSR_REGWnR_Pos)             

#define CoreDebug_DCRSR_REGSEL_Pos          0U                                             
#define CoreDebug_DCRSR_REGSEL_Msk         (0x1FUL  )      

 
#define CoreDebug_DEMCR_DWTENA_Pos         24U                                             
#define CoreDebug_DEMCR_DWTENA_Msk         (1UL << CoreDebug_DEMCR_DWTENA_Pos)             

#define CoreDebug_DEMCR_VC_HARDERR_Pos     10U                                             
#define CoreDebug_DEMCR_VC_HARDERR_Msk     (1UL << CoreDebug_DEMCR_VC_HARDERR_Pos)         

#define CoreDebug_DEMCR_VC_CORERESET_Pos    0U                                             
#define CoreDebug_DEMCR_VC_CORERESET_Msk   (1UL  )   

 
#define CoreDebug_DAUTHCTRL_INTSPNIDEN_Pos  3U                                             
#define CoreDebug_DAUTHCTRL_INTSPNIDEN_Msk (1UL << CoreDebug_DAUTHCTRL_INTSPNIDEN_Pos)     

#define CoreDebug_DAUTHCTRL_SPNIDENSEL_Pos  2U                                             
#define CoreDebug_DAUTHCTRL_SPNIDENSEL_Msk (1UL << CoreDebug_DAUTHCTRL_SPNIDENSEL_Pos)     

#define CoreDebug_DAUTHCTRL_INTSPIDEN_Pos   1U                                             
#define CoreDebug_DAUTHCTRL_INTSPIDEN_Msk  (1UL << CoreDebug_DAUTHCTRL_INTSPIDEN_Pos)      

#define CoreDebug_DAUTHCTRL_SPIDENSEL_Pos   0U                                             
#define CoreDebug_DAUTHCTRL_SPIDENSEL_Msk  (1UL  )  

 
#define CoreDebug_DSCSR_CDS_Pos            16U                                             
#define CoreDebug_DSCSR_CDS_Msk            (1UL << CoreDebug_DSCSR_CDS_Pos)                

#define CoreDebug_DSCSR_SBRSEL_Pos          1U                                             
#define CoreDebug_DSCSR_SBRSEL_Msk         (1UL << CoreDebug_DSCSR_SBRSEL_Pos)             

#define CoreDebug_DSCSR_SBRSELEN_Pos        0U                                             
#define CoreDebug_DSCSR_SBRSELEN_Msk       (1UL  )       

 


 

 
typedef struct
{
  __IOM uint32_t DHCSR;                   
  __OM  uint32_t DCRSR;                   
  __IOM uint32_t DCRDR;                   
  __IOM uint32_t DEMCR;                   
        uint32_t RESERVED0[1U];
  __IOM uint32_t DAUTHCTRL;               
  __IOM uint32_t DSCSR;                   
} DCB_Type;

 
#define DCB_DHCSR_DBGKEY_Pos               16U                                             
#define DCB_DHCSR_DBGKEY_Msk               (0xFFFFUL << DCB_DHCSR_DBGKEY_Pos)              

#define DCB_DHCSR_S_RESTART_ST_Pos         26U                                             
#define DCB_DHCSR_S_RESTART_ST_Msk         (0x1UL << DCB_DHCSR_S_RESTART_ST_Pos)           

#define DCB_DHCSR_S_RESET_ST_Pos           25U                                             
#define DCB_DHCSR_S_RESET_ST_Msk           (0x1UL << DCB_DHCSR_S_RESET_ST_Pos)             

#define DCB_DHCSR_S_RETIRE_ST_Pos          24U                                             
#define DCB_DHCSR_S_RETIRE_ST_Msk          (0x1UL << DCB_DHCSR_S_RETIRE_ST_Pos)            

#define DCB_DHCSR_S_SDE_Pos                20U                                             
#define DCB_DHCSR_S_SDE_Msk                (0x1UL << DCB_DHCSR_S_SDE_Pos)                  

#define DCB_DHCSR_S_LOCKUP_Pos             19U                                             
#define DCB_DHCSR_S_LOCKUP_Msk             (0x1UL << DCB_DHCSR_S_LOCKUP_Pos)               

#define DCB_DHCSR_S_SLEEP_Pos              18U                                             
#define DCB_DHCSR_S_SLEEP_Msk              (0x1UL << DCB_DHCSR_S_SLEEP_Pos)                

#define DCB_DHCSR_S_HALT_Pos               17U                                             
#define DCB_DHCSR_S_HALT_Msk               (0x1UL << DCB_DHCSR_S_HALT_Pos)                 

#define DCB_DHCSR_S_REGRDY_Pos             16U                                             
#define DCB_DHCSR_S_REGRDY_Msk             (0x1UL << DCB_DHCSR_S_REGRDY_Pos)               

#define DCB_DHCSR_C_MASKINTS_Pos            3U                                             
#define DCB_DHCSR_C_MASKINTS_Msk           (0x1UL << DCB_DHCSR_C_MASKINTS_Pos)             

#define DCB_DHCSR_C_STEP_Pos                2U                                             
#define DCB_DHCSR_C_STEP_Msk               (0x1UL << DCB_DHCSR_C_STEP_Pos)                 

#define DCB_DHCSR_C_HALT_Pos                1U                                             
#define DCB_DHCSR_C_HALT_Msk               (0x1UL << DCB_DHCSR_C_HALT_Pos)                 

#define DCB_DHCSR_C_DEBUGEN_Pos             0U                                             
#define DCB_DHCSR_C_DEBUGEN_Msk            (0x1UL  )          

 
#define DCB_DCRSR_REGWnR_Pos               16U                                             
#define DCB_DCRSR_REGWnR_Msk               (0x1UL << DCB_DCRSR_REGWnR_Pos)                 

#define DCB_DCRSR_REGSEL_Pos                0U                                             
#define DCB_DCRSR_REGSEL_Msk               (0x7FUL  )            

 
#define DCB_DCRDR_DBGTMP_Pos                0U                                             
#define DCB_DCRDR_DBGTMP_Msk               (0xFFFFFFFFUL  )      

 
#define DCB_DEMCR_TRCENA_Pos               24U                                             
#define DCB_DEMCR_TRCENA_Msk               (0x1UL << DCB_DEMCR_TRCENA_Pos)                 

#define DCB_DEMCR_VC_HARDERR_Pos           10U                                             
#define DCB_DEMCR_VC_HARDERR_Msk           (0x1UL << DCB_DEMCR_VC_HARDERR_Pos)             

#define DCB_DEMCR_VC_CORERESET_Pos          0U                                             
#define DCB_DEMCR_VC_CORERESET_Msk         (0x1UL  )       

 
#define DCB_DAUTHCTRL_INTSPNIDEN_Pos        3U                                             
#define DCB_DAUTHCTRL_INTSPNIDEN_Msk       (0x1UL << DCB_DAUTHCTRL_INTSPNIDEN_Pos)         

#define DCB_DAUTHCTRL_SPNIDENSEL_Pos        2U                                             
#define DCB_DAUTHCTRL_SPNIDENSEL_Msk       (0x1UL << DCB_DAUTHCTRL_SPNIDENSEL_Pos)         

#define DCB_DAUTHCTRL_INTSPIDEN_Pos         1U                                             
#define DCB_DAUTHCTRL_INTSPIDEN_Msk        (0x1UL << DCB_DAUTHCTRL_INTSPIDEN_Pos)          

#define DCB_DAUTHCTRL_SPIDENSEL_Pos         0U                                             
#define DCB_DAUTHCTRL_SPIDENSEL_Msk        (0x1UL  )      

 
#define DCB_DSCSR_CDSKEY_Pos               17U                                             
#define DCB_DSCSR_CDSKEY_Msk               (0x1UL << DCB_DSCSR_CDSKEY_Pos)                 

#define DCB_DSCSR_CDS_Pos                  16U                                             
#define DCB_DSCSR_CDS_Msk                  (0x1UL << DCB_DSCSR_CDS_Pos)                    

#define DCB_DSCSR_SBRSEL_Pos                1U                                             
#define DCB_DSCSR_SBRSEL_Msk               (0x1UL << DCB_DSCSR_SBRSEL_Pos)                 

#define DCB_DSCSR_SBRSELEN_Pos              0U                                             
#define DCB_DSCSR_SBRSELEN_Msk             (0x1UL  )           

 



 

 
typedef struct
{
  __OM  uint32_t DLAR;                    
  __IM  uint32_t DLSR;                    
  __IM  uint32_t DAUTHSTATUS;             
  __IM  uint32_t DDEVARCH;                
  __IM  uint32_t DDEVTYPE;                
} DIB_Type;

 
#define DIB_DLAR_KEY_Pos                    0U                                             
#define DIB_DLAR_KEY_Msk                   (0xFFFFFFFFUL  )         

 
#define DIB_DLSR_nTT_Pos                    2U                                             
#define DIB_DLSR_nTT_Msk                   (0x1UL << DIB_DLSR_nTT_Pos )                    

#define DIB_DLSR_SLK_Pos                    1U                                             
#define DIB_DLSR_SLK_Msk                   (0x1UL << DIB_DLSR_SLK_Pos )                    

#define DIB_DLSR_SLI_Pos                    0U                                             
#define DIB_DLSR_SLI_Msk                   (0x1UL  )                 

 
#define DIB_DAUTHSTATUS_SNID_Pos            6U                                             
#define DIB_DAUTHSTATUS_SNID_Msk           (0x3UL << DIB_DAUTHSTATUS_SNID_Pos )            

#define DIB_DAUTHSTATUS_SID_Pos             4U                                             
#define DIB_DAUTHSTATUS_SID_Msk            (0x3UL << DIB_DAUTHSTATUS_SID_Pos )             

#define DIB_DAUTHSTATUS_NSNID_Pos           2U                                             
#define DIB_DAUTHSTATUS_NSNID_Msk          (0x3UL << DIB_DAUTHSTATUS_NSNID_Pos )           

#define DIB_DAUTHSTATUS_NSID_Pos            0U                                             
#define DIB_DAUTHSTATUS_NSID_Msk           (0x3UL  )         

 
#define DIB_DDEVARCH_ARCHITECT_Pos         21U                                             
#define DIB_DDEVARCH_ARCHITECT_Msk         (0x7FFUL << DIB_DDEVARCH_ARCHITECT_Pos )        

#define DIB_DDEVARCH_PRESENT_Pos           20U                                             
#define DIB_DDEVARCH_PRESENT_Msk           (0x1FUL << DIB_DDEVARCH_PRESENT_Pos )           

#define DIB_DDEVARCH_REVISION_Pos          16U                                             
#define DIB_DDEVARCH_REVISION_Msk          (0xFUL << DIB_DDEVARCH_REVISION_Pos )           

#define DIB_DDEVARCH_ARCHVER_Pos           12U                                             
#define DIB_DDEVARCH_ARCHVER_Msk           (0xFUL << DIB_DDEVARCH_ARCHVER_Pos )            

#define DIB_DDEVARCH_ARCHPART_Pos           0U                                             
#define DIB_DDEVARCH_ARCHPART_Msk          (0xFFFUL  )      

 
#define DIB_DDEVTYPE_SUB_Pos                4U                                             
#define DIB_DDEVTYPE_SUB_Msk               (0xFUL << DIB_DDEVTYPE_SUB_Pos )                

#define DIB_DDEVTYPE_MAJOR_Pos              0U                                             
#define DIB_DDEVTYPE_MAJOR_Msk             (0xFUL  )           


 


 

 
#define _VAL2FLD(field, value)    (((uint32_t)(value) << field ## _Pos) & field ## _Msk)

 
#define _FLD2VAL(field, value)    (((uint32_t)(value) & field ## _Msk) >> field ## _Pos)

 


 

 
  #define SCS_BASE            (0xE000E000UL)                              
  #define DWT_BASE            (0xE0001000UL)                              
  #define TPI_BASE            (0xE0040000UL)                              
  #define CoreDebug_BASE      (0xE000EDF0UL)                              
  #define DCB_BASE            (0xE000EDF0UL)                              
  #define DIB_BASE            (0xE000EFB0UL)                              
  #define SysTick_BASE        (SCS_BASE +  0x0010UL)                      
  #define NVIC_BASE           (SCS_BASE +  0x0100UL)                      
  #define SCB_BASE            (SCS_BASE +  0x0D00UL)                      


  #define SCB                 ((SCB_Type       *)     SCB_BASE         )  
  #define SysTick             ((SysTick_Type   *)     SysTick_BASE     )  
  #define NVIC                ((NVIC_Type      *)     NVIC_BASE        )  
  #define DWT                 ((DWT_Type       *)     DWT_BASE         )  
  #define TPI                 ((TPI_Type       *)     TPI_BASE         )  
  #define CoreDebug           ((CoreDebug_Type *)     CoreDebug_BASE   )  
  #define DCB                 ((DCB_Type       *)     DCB_BASE         )  
  #define DIB                 ((DIB_Type       *)     DIB_BASE         )  

  #if defined (__MPU_PRESENT) && (__MPU_PRESENT == 1U)
    #define MPU_BASE          (SCS_BASE +  0x0D90UL)                      
    #define MPU               ((MPU_Type       *)     MPU_BASE         )  
  #endif

  #if defined (__ARM_FEATURE_CMSE) && (__ARM_FEATURE_CMSE == 3U)
    #define SAU_BASE          (SCS_BASE +  0x0DD0UL)                      
    #define SAU               ((SAU_Type       *)     SAU_BASE         )  
  #endif

#if defined (__ARM_FEATURE_CMSE) && (__ARM_FEATURE_CMSE == 3U)
  #define SCS_BASE_NS         (0xE002E000UL)                              
  #define CoreDebug_BASE_NS   (0xE002EDF0UL)                              
  #define DCB_BASE_NS         (0xE002EDF0UL)                              
  #define DIB_BASE_NS         (0xE002EFB0UL)                              
  #define SysTick_BASE_NS     (SCS_BASE_NS +  0x0010UL)                   
  #define NVIC_BASE_NS        (SCS_BASE_NS +  0x0100UL)                   
  #define SCB_BASE_NS         (SCS_BASE_NS +  0x0D00UL)                   

  #define SCB_NS              ((SCB_Type       *)     SCB_BASE_NS      )  
  #define SysTick_NS          ((SysTick_Type   *)     SysTick_BASE_NS  )  
  #define NVIC_NS             ((NVIC_Type      *)     NVIC_BASE_NS     )  
  #define CoreDebug_NS        ((CoreDebug_Type *)     CoreDebug_BASE_NS)  
  #define DCB_NS              ((DCB_Type       *)     DCB_BASE_NS      )  
  #define DIB_NS              ((DIB_Type       *)     DIB_BASE_NS      )  

  #if defined (__MPU_PRESENT) && (__MPU_PRESENT == 1U)
    #define MPU_BASE_NS       (SCS_BASE_NS +  0x0D90UL)                   
    #define MPU_NS            ((MPU_Type       *)     MPU_BASE_NS      )  
  #endif

#endif  
 



 
 



 
 

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
  #define NVIC_GetActive              __NVIC_GetActive
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


 

 
#define FNC_RETURN                 (0xFEFFFFFFUL)      

 
#define EXC_RETURN_PREFIX          (0xFF000000UL)      
#define EXC_RETURN_S               (0x00000040UL)      
#define EXC_RETURN_DCRS            (0x00000020UL)      
#define EXC_RETURN_FTYPE           (0x00000010UL)      
#define EXC_RETURN_MODE            (0x00000008UL)      
#define EXC_RETURN_SPSEL           (0x00000004UL)      
#define EXC_RETURN_ES              (0x00000001UL)      

 
#if defined (__FPU_PRESENT) && (__FPU_PRESENT == 1U)   
#define EXC_INTEGRITY_SIGNATURE     (0xFEFA125AUL)      
#else
#define EXC_INTEGRITY_SIGNATURE     (0xFEFA125BUL)      
#endif


 
 
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
    NVIC->ISER[(((uint32_t)IRQn) >> 5UL)] = (uint32_t)(1UL << (((uint32_t)IRQn) & 0x1FUL));
    __COMPILER_BARRIER();
  }
}


 
__STATIC_INLINE uint32_t __NVIC_GetEnableIRQ(IRQn_Type IRQn)
{
  if ((int32_t)(IRQn) >= 0)
  {
    return((uint32_t)(((NVIC->ISER[(((uint32_t)IRQn) >> 5UL)] & (1UL << (((uint32_t)IRQn) & 0x1FUL))) != 0UL) ? 1UL : 0UL));
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
    NVIC->ICER[(((uint32_t)IRQn) >> 5UL)] = (uint32_t)(1UL << (((uint32_t)IRQn) & 0x1FUL));
    __DSB();
    __ISB();
  }
}


 
__STATIC_INLINE uint32_t __NVIC_GetPendingIRQ(IRQn_Type IRQn)
{
  if ((int32_t)(IRQn) >= 0)
  {
    return((uint32_t)(((NVIC->ISPR[(((uint32_t)IRQn) >> 5UL)] & (1UL << (((uint32_t)IRQn) & 0x1FUL))) != 0UL) ? 1UL : 0UL));
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
    NVIC->ISPR[(((uint32_t)IRQn) >> 5UL)] = (uint32_t)(1UL << (((uint32_t)IRQn) & 0x1FUL));
  }
}


 
__STATIC_INLINE void __NVIC_ClearPendingIRQ(IRQn_Type IRQn)
{
  if ((int32_t)(IRQn) >= 0)
  {
    NVIC->ICPR[(((uint32_t)IRQn) >> 5UL)] = (uint32_t)(1UL << (((uint32_t)IRQn) & 0x1FUL));
  }
}


 
__STATIC_INLINE uint32_t __NVIC_GetActive(IRQn_Type IRQn)
{
  if ((int32_t)(IRQn) >= 0)
  {
    return((uint32_t)(((NVIC->IABR[(((uint32_t)IRQn) >> 5UL)] & (1UL << (((uint32_t)IRQn) & 0x1FUL))) != 0UL) ? 1UL : 0UL));
  }
  else
  {
    return(0U);
  }
}


#if defined (__ARM_FEATURE_CMSE) && (__ARM_FEATURE_CMSE == 3U)
 
__STATIC_INLINE uint32_t NVIC_GetTargetState(IRQn_Type IRQn)
{
  if ((int32_t)(IRQn) >= 0)
  {
    return((uint32_t)(((NVIC->ITNS[(((uint32_t)IRQn) >> 5UL)] & (1UL << (((uint32_t)IRQn) & 0x1FUL))) != 0UL) ? 1UL : 0UL));
  }
  else
  {
    return(0U);
  }
}


 
__STATIC_INLINE uint32_t NVIC_SetTargetState(IRQn_Type IRQn)
{
  if ((int32_t)(IRQn) >= 0)
  {
    NVIC->ITNS[(((uint32_t)IRQn) >> 5UL)] |=  ((uint32_t)(1UL << (((uint32_t)IRQn) & 0x1FUL)));
    return((uint32_t)(((NVIC->ITNS[(((uint32_t)IRQn) >> 5UL)] & (1UL << (((uint32_t)IRQn) & 0x1FUL))) != 0UL) ? 1UL : 0UL));
  }
  else
  {
    return(0U);
  }
}


 
__STATIC_INLINE uint32_t NVIC_ClearTargetState(IRQn_Type IRQn)
{
  if ((int32_t)(IRQn) >= 0)
  {
    NVIC->ITNS[(((uint32_t)IRQn) >> 5UL)] &= ~((uint32_t)(1UL << (((uint32_t)IRQn) & 0x1FUL)));
    return((uint32_t)(((NVIC->ITNS[(((uint32_t)IRQn) >> 5UL)] & (1UL << (((uint32_t)IRQn) & 0x1FUL))) != 0UL) ? 1UL : 0UL));
  }
  else
  {
    return(0U);
  }
}
#endif  


 
__STATIC_INLINE void __NVIC_SetPriority(IRQn_Type IRQn, uint32_t priority)
{
  if ((int32_t)(IRQn) >= 0)
  {
    NVIC->IPR[_IP_IDX(IRQn)]  = ((uint32_t)(NVIC->IPR[_IP_IDX(IRQn)]  & ~(0xFFUL << _BIT_SHIFT(IRQn))) |
       (((priority << (8U - __NVIC_PRIO_BITS)) & (uint32_t)0xFFUL) << _BIT_SHIFT(IRQn)));
  }
  else
  {
    SCB->SHPR[_SHP_IDX(IRQn)] = ((uint32_t)(SCB->SHPR[_SHP_IDX(IRQn)] & ~(0xFFUL << _BIT_SHIFT(IRQn))) |
       (((priority << (8U - __NVIC_PRIO_BITS)) & (uint32_t)0xFFUL) << _BIT_SHIFT(IRQn)));
  }
}


 
__STATIC_INLINE uint32_t __NVIC_GetPriority(IRQn_Type IRQn)
{

  if ((int32_t)(IRQn) >= 0)
  {
    return((uint32_t)(((NVIC->IPR[ _IP_IDX(IRQn)] >> _BIT_SHIFT(IRQn) ) & (uint32_t)0xFFUL) >> (8U - __NVIC_PRIO_BITS)));
  }
  else
  {
    return((uint32_t)(((SCB->SHPR[_SHP_IDX(IRQn)] >> _BIT_SHIFT(IRQn) ) & (uint32_t)0xFFUL) >> (8U - __NVIC_PRIO_BITS)));
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
#if defined (__VTOR_PRESENT) && (__VTOR_PRESENT == 1U)
  uint32_t *vectors = (uint32_t *)SCB->VTOR;
#else
  uint32_t *vectors = (uint32_t *)0x0U;
#endif
  vectors[(int32_t)IRQn + NVIC_USER_IRQ_OFFSET] = vector;
  __DSB();
}


 
__STATIC_INLINE uint32_t __NVIC_GetVector(IRQn_Type IRQn)
{
#if defined (__VTOR_PRESENT) && (__VTOR_PRESENT == 1U)
  uint32_t *vectors = (uint32_t *)SCB->VTOR;
#else
  uint32_t *vectors = (uint32_t *)0x0U;
#endif
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

#if defined (__ARM_FEATURE_CMSE) && (__ARM_FEATURE_CMSE == 3U)
 
__STATIC_INLINE void TZ_NVIC_EnableIRQ_NS(IRQn_Type IRQn)
{
  if ((int32_t)(IRQn) >= 0)
  {
    NVIC_NS->ISER[(((uint32_t)IRQn) >> 5UL)] = (uint32_t)(1UL << (((uint32_t)IRQn) & 0x1FUL));
  }
}


 
__STATIC_INLINE uint32_t TZ_NVIC_GetEnableIRQ_NS(IRQn_Type IRQn)
{
  if ((int32_t)(IRQn) >= 0)
  {
    return((uint32_t)(((NVIC_NS->ISER[(((uint32_t)IRQn) >> 5UL)] & (1UL << (((uint32_t)IRQn) & 0x1FUL))) != 0UL) ? 1UL : 0UL));
  }
  else
  {
    return(0U);
  }
}


 
__STATIC_INLINE void TZ_NVIC_DisableIRQ_NS(IRQn_Type IRQn)
{
  if ((int32_t)(IRQn) >= 0)
  {
    NVIC_NS->ICER[(((uint32_t)IRQn) >> 5UL)] = (uint32_t)(1UL << (((uint32_t)IRQn) & 0x1FUL));
  }
}


 
__STATIC_INLINE uint32_t TZ_NVIC_GetPendingIRQ_NS(IRQn_Type IRQn)
{
  if ((int32_t)(IRQn) >= 0)
  {
    return((uint32_t)(((NVIC_NS->ISPR[(((uint32_t)IRQn) >> 5UL)] & (1UL << (((uint32_t)IRQn) & 0x1FUL))) != 0UL) ? 1UL : 0UL));
  }
  else
  {
    return(0U);
  }
}


 
__STATIC_INLINE void TZ_NVIC_SetPendingIRQ_NS(IRQn_Type IRQn)
{
  if ((int32_t)(IRQn) >= 0)
  {
    NVIC_NS->ISPR[(((uint32_t)IRQn) >> 5UL)] = (uint32_t)(1UL << (((uint32_t)IRQn) & 0x1FUL));
  }
}


 
__STATIC_INLINE void TZ_NVIC_ClearPendingIRQ_NS(IRQn_Type IRQn)
{
  if ((int32_t)(IRQn) >= 0)
  {
    NVIC_NS->ICPR[(((uint32_t)IRQn) >> 5UL)] = (uint32_t)(1UL << (((uint32_t)IRQn) & 0x1FUL));
  }
}


 
__STATIC_INLINE uint32_t TZ_NVIC_GetActive_NS(IRQn_Type IRQn)
{
  if ((int32_t)(IRQn) >= 0)
  {
    return((uint32_t)(((NVIC_NS->IABR[(((uint32_t)IRQn) >> 5UL)] & (1UL << (((uint32_t)IRQn) & 0x1FUL))) != 0UL) ? 1UL : 0UL));
  }
  else
  {
    return(0U);
  }
}


 
__STATIC_INLINE void TZ_NVIC_SetPriority_NS(IRQn_Type IRQn, uint32_t priority)
{
  if ((int32_t)(IRQn) >= 0)
  {
    NVIC_NS->IPR[_IP_IDX(IRQn)]  = ((uint32_t)(NVIC_NS->IPR[_IP_IDX(IRQn)]  & ~(0xFFUL << _BIT_SHIFT(IRQn))) |
       (((priority << (8U - __NVIC_PRIO_BITS)) & (uint32_t)0xFFUL) << _BIT_SHIFT(IRQn)));
  }
  else
  {
    SCB_NS->SHPR[_SHP_IDX(IRQn)] = ((uint32_t)(SCB_NS->SHPR[_SHP_IDX(IRQn)] & ~(0xFFUL << _BIT_SHIFT(IRQn))) |
       (((priority << (8U - __NVIC_PRIO_BITS)) & (uint32_t)0xFFUL) << _BIT_SHIFT(IRQn)));
  }
}


 
__STATIC_INLINE uint32_t TZ_NVIC_GetPriority_NS(IRQn_Type IRQn)
{

  if ((int32_t)(IRQn) >= 0)
  {
    return((uint32_t)(((NVIC_NS->IPR[ _IP_IDX(IRQn)] >> _BIT_SHIFT(IRQn) ) & (uint32_t)0xFFUL) >> (8U - __NVIC_PRIO_BITS)));
  }
  else
  {
    return((uint32_t)(((SCB_NS->SHPR[_SHP_IDX(IRQn)] >> _BIT_SHIFT(IRQn) ) & (uint32_t)0xFFUL) >> (8U - __NVIC_PRIO_BITS)));
  }
}
#endif  

 

 

#if defined (__MPU_PRESENT) && (__MPU_PRESENT == 1U)

#include "mpu_armv8.h"

#endif

 
 

 
__STATIC_INLINE uint32_t SCB_GetFPUType(void)
{
    return 0U;            
}


 



 
 

#if defined (__ARM_FEATURE_CMSE) && (__ARM_FEATURE_CMSE == 3U)

 
__STATIC_INLINE void TZ_SAU_Enable(void)
{
    SAU->CTRL |=  (SAU_CTRL_ENABLE_Msk);
}



 
__STATIC_INLINE void TZ_SAU_Disable(void)
{
    SAU->CTRL &= ~(SAU_CTRL_ENABLE_Msk);
}

#endif  

 




 
 

 
 
__STATIC_INLINE void DCB_SetAuthCtrl(uint32_t value)
{
    __DSB();
    __ISB();
    DCB->DAUTHCTRL = value;
    __DSB();
    __ISB();
}


 
__STATIC_INLINE uint32_t DCB_GetAuthCtrl(void)
{
    return (DCB->DAUTHCTRL);
}


#if defined (__ARM_FEATURE_CMSE) && (__ARM_FEATURE_CMSE == 3U)
 
__STATIC_INLINE void TZ_DCB_SetAuthCtrl_NS(uint32_t value)
{
    __DSB();
    __ISB();
    DCB_NS->DAUTHCTRL = value;
    __DSB();
    __ISB();
}


 
__STATIC_INLINE uint32_t TZ_DCB_GetAuthCtrl_NS(void)
{
    return (DCB_NS->DAUTHCTRL);
}
#endif  

 




 
 

 
 
__STATIC_INLINE uint32_t DIB_GetAuthStatus(void)
{
    return (DIB->DAUTHSTATUS);
}


#if defined (__ARM_FEATURE_CMSE) && (__ARM_FEATURE_CMSE == 3U)
 
__STATIC_INLINE uint32_t TZ_DIB_GetAuthStatus_NS(void)
{
    return (DIB_NS->DAUTHSTATUS);
}
#endif  

 




 
 

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

#if defined (__ARM_FEATURE_CMSE) && (__ARM_FEATURE_CMSE == 3U)
 
__STATIC_INLINE uint32_t TZ_SysTick_Config_NS(uint32_t ticks)
{
  if ((ticks - 1UL) > SysTick_LOAD_RELOAD_Msk)
  {
    return (1UL);                                                          
  }

  SysTick_NS->LOAD  = (uint32_t)(ticks - 1UL);                             
  TZ_NVIC_SetPriority_NS (SysTick_IRQn, (1UL << __NVIC_PRIO_BITS) - 1UL);  
  SysTick_NS->VAL   = 0UL;                                                 
  SysTick_NS->CTRL  = SysTick_CTRL_CLKSOURCE_Msk |
                      SysTick_CTRL_TICKINT_Msk   |
                      SysTick_CTRL_ENABLE_Msk;                             
  return (0UL);                                                            
}
#endif  

#endif

 




#ifdef __cplusplus
}
#endif

#endif  

#endif  
