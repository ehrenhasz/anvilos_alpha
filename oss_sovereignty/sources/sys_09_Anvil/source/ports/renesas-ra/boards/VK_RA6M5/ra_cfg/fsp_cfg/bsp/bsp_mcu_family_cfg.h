
#ifndef BSP_MCU_FAMILY_CFG_H_
#define BSP_MCU_FAMILY_CFG_H_
#ifdef __cplusplus
extern "C" {
#endif

#include "bsp_mcu_device_pn_cfg.h"
#include "bsp_mcu_device_cfg.h"
#include "../../../ra/fsp/src/bsp/mcu/ra6m5/bsp_mcu_info.h"
#include "bsp_clock_cfg.h"
#define BSP_MCU_GROUP_RA6M5 (1)
#define BSP_LOCO_HZ                 (32768)
#define BSP_MOCO_HZ                 (8000000)
#define BSP_SUB_CLOCK_HZ            (32768)
#if   BSP_CFG_HOCO_FREQUENCY == 0
#define BSP_HOCO_HZ                 (16000000)
#elif BSP_CFG_HOCO_FREQUENCY == 1
#define BSP_HOCO_HZ                 (18000000)
#elif BSP_CFG_HOCO_FREQUENCY == 2
#define BSP_HOCO_HZ                 (20000000)
#else
#error "Invalid HOCO frequency chosen (BSP_CFG_HOCO_FREQUENCY) in bsp_clock_cfg.h"
#endif

#define BSP_CFG_FLL_ENABLE                 (0)

#define BSP_CORTEX_VECTOR_TABLE_ENTRIES    (16U)
#define BSP_VECTOR_TABLE_MAX_ENTRIES       (112U)

#if defined(_RA_TZ_SECURE)
#define BSP_TZ_SECURE_BUILD           (1)
#define BSP_TZ_NONSECURE_BUILD        (0)
#elif defined(_RA_TZ_NONSECURE)
#define BSP_TZ_SECURE_BUILD           (0)
#define BSP_TZ_NONSECURE_BUILD        (1)
#else
#define BSP_TZ_SECURE_BUILD           (0)
#define BSP_TZ_NONSECURE_BUILD        (0)
#endif


#define BSP_TZ_CFG_INIT_SECURE_ONLY       (BSP_CFG_CLOCKS_SECURE || (!BSP_CFG_CLOCKS_OVERRIDE))
#define BSP_TZ_CFG_SKIP_INIT              (BSP_TZ_NONSECURE_BUILD && BSP_TZ_CFG_INIT_SECURE_ONLY)
#define BSP_TZ_CFG_EXCEPTION_RESPONSE     (0)


#define SCB_CSR_AIRCR_INIT                (1)
#define SCB_AIRCR_BFHFNMINS_VAL           (0)
#define SCB_AIRCR_SYSRESETREQS_VAL        (1)
#define SCB_AIRCR_PRIS_VAL                (0)
#define TZ_FPU_NS_USAGE                   (1)
#ifndef SCB_NSACR_CP10_11_VAL
#define SCB_NSACR_CP10_11_VAL             (3U)
#endif

#ifndef FPU_FPCCR_TS_VAL
#define FPU_FPCCR_TS_VAL                  (1U)
#endif
#define FPU_FPCCR_CLRONRETS_VAL           (1)

#ifndef FPU_FPCCR_CLRONRET_VAL
#define FPU_FPCCR_CLRONRET_VAL            (1)
#endif


#ifndef BSP_CFG_C_CACHE_LINE_SIZE
#define BSP_CFG_C_CACHE_LINE_SIZE   (1U)
#endif




#ifndef BSP_TZ_CFG_PSARB
#define BSP_TZ_CFG_PSARB ( \
    (((RA_NOT_DEFINED > 0) ? 0U : 1U) << 1)  | \
    (((RA_NOT_DEFINED > 0) ? 0U : 1U) << 2)  | \
    (((RA_NOT_DEFINED > 0) ? 0U : 1U) << 8)  | \
    (((RA_NOT_DEFINED > 0) ? 0U : 1U) << 9)  | \
    (((RA_NOT_DEFINED > 0) ? 0U : 1U) << 11)  | \
    (((RA_NOT_DEFINED > 0) ? 0U : 1U) << 18)  | \
    (((1 > 0) ? 0U : 1U) << 19)  | \
    (((1 > 0) ? 0U : 1U) << 22)  | \
    (((RA_NOT_DEFINED > 0) ? 0U : 1U) << 23)  | \
    (((1 > 0) ? 0U : 1U) << 24)  | \
    (((1 > 0) ? 0U : 1U) << 25)  | \
    (((RA_NOT_DEFINED > 0) ? 0U : 1U) << 26)  | \
    (((RA_NOT_DEFINED > 0) ? 0U : 1U) << 27)  | \
    (((RA_NOT_DEFINED > 0) ? 0U : 1U) << 28)  | \
    (((RA_NOT_DEFINED > 0) ? 0U : 1U) << 29)  | \
    (((RA_NOT_DEFINED > 0) ? 0U : 1U) << 30)  | \
    (((RA_NOT_DEFINED > 0) ? 0U : 1U) << 31)  | \
    0x33f4f9)         
#endif
#ifndef BSP_TZ_CFG_PSARC
#define BSP_TZ_CFG_PSARC ( \
    (((RA_NOT_DEFINED > 0) ? 0U : 1U) << 0)  | \
    (((RA_NOT_DEFINED > 0) ? 0U : 1U) << 1)  | \
    (((RA_NOT_DEFINED > 0) ? 0U : 1U) << 3)  | \
    (((RA_NOT_DEFINED > 0) ? 0U : 1U) << 8)  | \
    (((1 > 0) ? 0U : 1U) << 12)  | \
    (((RA_NOT_DEFINED > 0) ? 0U : 1U) << 13)  | \
    (((RA_NOT_DEFINED > 0) ? 0U : 1U) << 31)  | \
    0x7fffcef4)         
#endif
#ifndef BSP_TZ_CFG_PSARD
#define BSP_TZ_CFG_PSARD ( \
    (((RA_NOT_DEFINED > 0) ? 0U : 1U) << 0)  | \
    (((RA_NOT_DEFINED > 0) ? 0U : 1U) << 1)  | \
    (((1 > 0) ? 0U : 1U) << 2)  | \
    (((1 > 0) ? 0U : 1U) << 3)  | \
    (((RA_NOT_DEFINED > 0) ? 0U : 1U) << 11)  | \
    (((RA_NOT_DEFINED > 0) ? 0U : 1U) << 12)  | \
    (((RA_NOT_DEFINED > 0) ? 0U : 1U) << 13)  | \
    (((RA_NOT_DEFINED > 0) ? 0U : 1U) << 14)  | \
    (((1 > 0) ? 0U : 1U) << 15)  | \
    (((1 > 0) ? 0U : 1U) << 16)  | \
    (((RA_NOT_DEFINED > 0) ? 0U : 1U) << 20)  | \
    (((RA_NOT_DEFINED > 0) ? 0U : 1U) << 22)  | \
    0xffae07f0)         
#endif
#ifndef BSP_TZ_CFG_PSARE
#define BSP_TZ_CFG_PSARE ( \
    (((RA_NOT_DEFINED > 0) ? 0U : 1U) << 0)  | \
    (((RA_NOT_DEFINED > 0) ? 0U : 1U) << 1)  | \
    (((1 > 0) ? 0U : 1U) << 2)  | \
    (((RA_NOT_DEFINED > 0) ? 0U : 1U) << 14)  | \
    (((RA_NOT_DEFINED > 0) ? 0U : 1U) << 15)  | \
    (((RA_NOT_DEFINED > 0) ? 0U : 1U) << 22)  | \
    (((RA_NOT_DEFINED > 0) ? 0U : 1U) << 23)  | \
    (((RA_NOT_DEFINED > 0) ? 0U : 1U) << 24)  | \
    (((RA_NOT_DEFINED > 0) ? 0U : 1U) << 25)  | \
    (((RA_NOT_DEFINED > 0) ? 0U : 1U) << 26)  | \
    (((RA_NOT_DEFINED > 0) ? 0U : 1U) << 27)  | \
    (((RA_NOT_DEFINED > 0) ? 0U : 1U) << 28)  | \
    (((RA_NOT_DEFINED > 0) ? 0U : 1U) << 29)  | \
    (((RA_NOT_DEFINED > 0) ? 0U : 1U) << 30)  | \
    (((RA_NOT_DEFINED > 0) ? 0U : 1U) << 31)  | \
    0x3f3ff8)         
#endif
#ifndef BSP_TZ_CFG_MSSAR
#define BSP_TZ_CFG_MSSAR ( \
    (((RA_NOT_DEFINED > 0) ? 0U : 1U) << 0)  | \
    (((3 > 0) ? 0U : 1U) << 1)  | \
    0xfffffffc)         
#endif




#ifndef BSP_TZ_CFG_CSAR
#define BSP_TZ_CFG_CSAR (0xFFFFFFFFU)
#endif


#ifndef BSP_TZ_CFG_RSTSAR
#define BSP_TZ_CFG_RSTSAR (0xFFFFFFFFU)
#endif


#ifndef BSP_TZ_CFG_LVDSAR
#define BSP_TZ_CFG_LVDSAR ( \
    (((RA_NOT_DEFINED > 0) ? 0U : 1U) << 0) |          \
    (((RA_NOT_DEFINED > 0) ? 0U : 1U) << 1) |          \
    0xFFFFFFFCU)
#endif


#ifndef BSP_TZ_CFG_LPMSAR
#define BSP_TZ_CFG_LPMSAR ((1 > 0) ? 0xFFFFFCEAU : 0xFFFFFFFFU)
#endif

#ifndef BSP_TZ_CFG_DPFSAR
#define BSP_TZ_CFG_DPFSAR ((1 > 0) ? 0xF2E00000U : 0xFFFFFFFFU)
#endif


#ifndef BSP_TZ_CFG_CGFSAR
#if BSP_CFG_CLOCKS_SECURE

#define BSP_TZ_CFG_CGFSAR (0xFFFCE402U)
#else

#define BSP_TZ_CFG_CGFSAR (0xFFFFFFFFU)
#endif
#endif


#ifndef BSP_TZ_CFG_BBFSAR
#define BSP_TZ_CFG_BBFSAR (0x00FFFFFF)
#endif


#ifndef BSP_TZ_CFG_ICUSARA
#define BSP_TZ_CFG_ICUSARA ( \
    (((RA_NOT_DEFINED > 0) ? 0U : 1U) << 0U)  | \
    (((RA_NOT_DEFINED > 0) ? 0U : 1U) << 1U)  | \
    (((RA_NOT_DEFINED > 0) ? 0U : 1U) << 2U)  | \
    (((RA_NOT_DEFINED > 0) ? 0U : 1U) << 3U)  | \
    (((RA_NOT_DEFINED > 0) ? 0U : 1U) << 4U)  | \
    (((RA_NOT_DEFINED > 0) ? 0U : 1U) << 5U)  | \
    (((RA_NOT_DEFINED > 0) ? 0U : 1U) << 6U)  | \
    (((1 > 0) ? 0U : 1U) << 7U)  | \
    (((RA_NOT_DEFINED > 0) ? 0U : 1U) << 8U)  | \
    (((RA_NOT_DEFINED > 0) ? 0U : 1U) << 9U)  | \
    (((RA_NOT_DEFINED > 0) ? 0U : 1U) << 10U)  | \
    (((RA_NOT_DEFINED > 0) ? 0U : 1U) << 11U)  | \
    (((RA_NOT_DEFINED > 0) ? 0U : 1U) << 12U)  | \
    (((1 > 0) ? 0U : 1U) << 13U)  | \
    (((1 > 0) ? 0U : 1U) << 14U)  | \
    (((RA_NOT_DEFINED > 0) ? 0U : 1U) << 15U)  | \
    0xFFFF0000U)
#endif


#ifndef BSP_TZ_CFG_ICUSARB
#define BSP_TZ_CFG_ICUSARB (0 | 0xFFFFFFFEU) 
#endif


#ifndef BSP_TZ_CFG_ICUSARC
#define BSP_TZ_CFG_ICUSARC ( \
    (((RA_NOT_DEFINED > 0) ? 0U : 1U) << 0U)  | \
    (((RA_NOT_DEFINED > 0) ? 0U : 1U) << 1U)  | \
    (((RA_NOT_DEFINED > 0) ? 0U : 1U) << 2U)  | \
    (((RA_NOT_DEFINED > 0) ? 0U : 1U) << 3U)  | \
    (((RA_NOT_DEFINED > 0) ? 0U : 1U) << 4U)  | \
    (((RA_NOT_DEFINED > 0) ? 0U : 1U) << 5U)  | \
    (((RA_NOT_DEFINED > 0) ? 0U : 1U) << 6U)  | \
    (((RA_NOT_DEFINED > 0) ? 0U : 1U) << 7U)  | \
    0xFFFFFF00U)
#endif


#ifndef BSP_TZ_CFG_ICUSARD
#define BSP_TZ_CFG_ICUSARD ((1 > 0) ? 0xFFFFFFFEU : 0xFFFFFFFFU)
#endif


#ifndef BSP_TZ_CFG_ICUSARE
#define BSP_TZ_CFG_ICUSARE ((1 > 0) ? 0x04F2FFFFU : 0xFFFFFFFFU)
#endif


#ifndef BSP_TZ_CFG_ICUSARF
#define BSP_TZ_CFG_ICUSARF ((1 > 0) ? 0xFFFFFFF8U : 0xFFFFFFFFU)
#endif


#if 3 == RA_NOT_DEFINED
 #define BSP_TZ_CFG_DTC_USED (0U)
#else
#define BSP_TZ_CFG_DTC_USED (1U)
#endif


#ifndef BSP_TZ_CFG_FSAR

#if BSP_CFG_CLOCKS_SECURE

#define BSP_TZ_CFG_FSAR (0xFEFEU)
#else

#define BSP_TZ_CFG_FSAR (0xFFFFU)
#endif
#endif


#ifndef BSP_TZ_CFG_SRAMSAR

#define BSP_TZ_CFG_SRAMSAR ( \
    1 | \
    ((BSP_CFG_CLOCKS_SECURE == 0) ? (1U << 1U) : 0U) | \
    4 | \
    0xFFFFFFF8U)
#endif


#ifndef BSP_TZ_CFG_STBRAMSAR
#define BSP_TZ_CFG_STBRAMSAR (0 | 0xFFFFFFF0U)
#endif


#ifndef BSP_TZ_CFG_MMPUSARA

#define BSP_TZ_CFG_MMPUSARA (BSP_TZ_CFG_ICUSARC)
#endif


#ifndef BSP_TZ_CFG_BUSSARA
#define BSP_TZ_CFG_BUSSARA (0xFFFFFFFFU)
#endif

#ifndef BSP_TZ_CFG_BUSSARB
#define BSP_TZ_CFG_BUSSARB (0xFFFFFFFFU)
#endif


#ifndef BSP_TZ_CFG_NON_SECURE_APPLICATION_FALLBACK
#define BSP_TZ_CFG_NON_SECURE_APPLICATION_FALLBACK (1U)
#endif

#define OFS_SEQ1 0xA001A001 | (1 << 1) | (3 << 2)
#define OFS_SEQ2 (15 << 4) | (3 << 8) | (3 << 10)
#define OFS_SEQ3 (1 << 12) | (1 << 14) | (1 << 17)
#define OFS_SEQ4 (3 << 18) | (15 << 20) | (3 << 24) | (3 << 26)
#define OFS_SEQ5 (1 << 28) | (1 << 30)
#define BSP_CFG_ROM_REG_OFS0 (OFS_SEQ1 | OFS_SEQ2 | OFS_SEQ3 | OFS_SEQ4 | OFS_SEQ5)


#ifndef BSP_CFG_ROM_REG_OFS1_SEL
#if defined(_RA_TZ_SECURE) || defined(_RA_TZ_NONSECURE)
            #define BSP_CFG_ROM_REG_OFS1_SEL (0xFFFFF8F8U | ((BSP_CFG_CLOCKS_SECURE == 0) ? 0x700U : 0U) | ((RA_NOT_DEFINED > 0) ? 0U : 0x7U))
#else
#define BSP_CFG_ROM_REG_OFS1_SEL (0xFFFFF8F8U)
#endif
#endif

#define BSP_CFG_ROM_REG_OFS1 (0xFFFFFEF8 | (1 << 2) | (3) | (1 << 8))


#define BSP_PRV_IELS_ENUM(vector)    (ELC_##vector)


#ifndef BSP_CFG_ROM_REG_DUALSEL
#define BSP_CFG_ROM_REG_DUALSEL (0xFFFFFFF8U | (0x7U))
#endif


#ifndef BSP_CFG_ROM_REG_BPS0
#define BSP_CFG_ROM_REG_BPS0 (~(0U))
#endif

#ifndef BSP_CFG_ROM_REG_BPS1
#define BSP_CFG_ROM_REG_BPS1 (~(0U))
#endif

#ifndef BSP_CFG_ROM_REG_BPS2
#define BSP_CFG_ROM_REG_BPS2 (~(0U))
#endif

#ifndef BSP_CFG_ROM_REG_BPS3
#define BSP_CFG_ROM_REG_BPS3 (0xFFFFFFFFU)
#endif

#ifndef BSP_CFG_ROM_REG_PBPS0
#define BSP_CFG_ROM_REG_PBPS0 (~(0U))
#endif

#ifndef BSP_CFG_ROM_REG_PBPS1
#define BSP_CFG_ROM_REG_PBPS1 (~(0U))
#endif

#ifndef BSP_CFG_ROM_REG_PBPS2
#define BSP_CFG_ROM_REG_PBPS2 (~(0U))
#endif

#ifndef BSP_CFG_ROM_REG_PBPS3
#define BSP_CFG_ROM_REG_PBPS3 (0xFFFFFFFFU)
#endif

#ifndef BSP_CFG_ROM_REG_BPS_SEL0
#define BSP_CFG_ROM_REG_BPS_SEL0 (BSP_CFG_ROM_REG_BPS0 & BSP_CFG_ROM_REG_PBPS0)
#endif

#ifndef BSP_CFG_ROM_REG_BPS_SEL1
#define BSP_CFG_ROM_REG_BPS_SEL1 (BSP_CFG_ROM_REG_BPS1 & BSP_CFG_ROM_REG_PBPS1)
#endif

#ifndef BSP_CFG_ROM_REG_BPS_SEL2
#define BSP_CFG_ROM_REG_BPS_SEL2 (BSP_CFG_ROM_REG_BPS2 & BSP_CFG_ROM_REG_PBPS2)
#endif

#ifndef BSP_CFG_ROM_REG_BPS_SEL3
#define BSP_CFG_ROM_REG_BPS_SEL3 (BSP_CFG_ROM_REG_BPS3 & BSP_CFG_ROM_REG_PBPS3)
#endif
#ifndef BSP_CLOCK_CFG_MAIN_OSC_WAIT
#define BSP_CLOCK_CFG_MAIN_OSC_WAIT (9)
#endif

#ifdef __cplusplus
}
#endif
#endif 
