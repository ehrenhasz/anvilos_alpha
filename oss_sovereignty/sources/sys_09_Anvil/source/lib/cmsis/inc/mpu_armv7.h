

 
#if   defined ( __ICCARM__ )
  #pragma system_include         
#elif defined (__clang__)
  #pragma clang system_header    
#endif
 
#ifndef ARM_MPU_ARMV7_H
#define ARM_MPU_ARMV7_H

#define ARM_MPU_REGION_SIZE_32B      ((uint8_t)0x04U) 
#define ARM_MPU_REGION_SIZE_64B      ((uint8_t)0x05U) 
#define ARM_MPU_REGION_SIZE_128B     ((uint8_t)0x06U) 
#define ARM_MPU_REGION_SIZE_256B     ((uint8_t)0x07U) 
#define ARM_MPU_REGION_SIZE_512B     ((uint8_t)0x08U) 
#define ARM_MPU_REGION_SIZE_1KB      ((uint8_t)0x09U) 
#define ARM_MPU_REGION_SIZE_2KB      ((uint8_t)0x0AU) 
#define ARM_MPU_REGION_SIZE_4KB      ((uint8_t)0x0BU) 
#define ARM_MPU_REGION_SIZE_8KB      ((uint8_t)0x0CU) 
#define ARM_MPU_REGION_SIZE_16KB     ((uint8_t)0x0DU) 
#define ARM_MPU_REGION_SIZE_32KB     ((uint8_t)0x0EU) 
#define ARM_MPU_REGION_SIZE_64KB     ((uint8_t)0x0FU) 
#define ARM_MPU_REGION_SIZE_128KB    ((uint8_t)0x10U) 
#define ARM_MPU_REGION_SIZE_256KB    ((uint8_t)0x11U) 
#define ARM_MPU_REGION_SIZE_512KB    ((uint8_t)0x12U) 
#define ARM_MPU_REGION_SIZE_1MB      ((uint8_t)0x13U) 
#define ARM_MPU_REGION_SIZE_2MB      ((uint8_t)0x14U) 
#define ARM_MPU_REGION_SIZE_4MB      ((uint8_t)0x15U) 
#define ARM_MPU_REGION_SIZE_8MB      ((uint8_t)0x16U) 
#define ARM_MPU_REGION_SIZE_16MB     ((uint8_t)0x17U) 
#define ARM_MPU_REGION_SIZE_32MB     ((uint8_t)0x18U) 
#define ARM_MPU_REGION_SIZE_64MB     ((uint8_t)0x19U) 
#define ARM_MPU_REGION_SIZE_128MB    ((uint8_t)0x1AU) 
#define ARM_MPU_REGION_SIZE_256MB    ((uint8_t)0x1BU) 
#define ARM_MPU_REGION_SIZE_512MB    ((uint8_t)0x1CU) 
#define ARM_MPU_REGION_SIZE_1GB      ((uint8_t)0x1DU) 
#define ARM_MPU_REGION_SIZE_2GB      ((uint8_t)0x1EU) 
#define ARM_MPU_REGION_SIZE_4GB      ((uint8_t)0x1FU) 

#define ARM_MPU_AP_NONE 0U 
#define ARM_MPU_AP_PRIV 1U 
#define ARM_MPU_AP_URO  2U 
#define ARM_MPU_AP_FULL 3U 
#define ARM_MPU_AP_PRO  5U 
#define ARM_MPU_AP_RO   6U 


#define ARM_MPU_RBAR(Region, BaseAddress) \
  (((BaseAddress) & MPU_RBAR_ADDR_Msk) |  \
   ((Region) & MPU_RBAR_REGION_Msk)    |  \
   (MPU_RBAR_VALID_Msk))

  
#define ARM_MPU_ACCESS_(TypeExtField, IsShareable, IsCacheable, IsBufferable)   \
  ((((TypeExtField) << MPU_RASR_TEX_Pos) & MPU_RASR_TEX_Msk)                  | \
   (((IsShareable)  << MPU_RASR_S_Pos)   & MPU_RASR_S_Msk)                    | \
   (((IsCacheable)  << MPU_RASR_C_Pos)   & MPU_RASR_C_Msk)                    | \
   (((IsBufferable) << MPU_RASR_B_Pos)   & MPU_RASR_B_Msk))


#define ARM_MPU_RASR_EX(DisableExec, AccessPermission, AccessAttributes, SubRegionDisable, Size)    \
  ((((DisableExec)      << MPU_RASR_XN_Pos)   & MPU_RASR_XN_Msk)                                  | \
   (((AccessPermission) << MPU_RASR_AP_Pos)   & MPU_RASR_AP_Msk)                                  | \
   (((AccessAttributes) & (MPU_RASR_TEX_Msk | MPU_RASR_S_Msk | MPU_RASR_C_Msk | MPU_RASR_B_Msk))) | \
   (((SubRegionDisable) << MPU_RASR_SRD_Pos)  & MPU_RASR_SRD_Msk)                                 | \
   (((Size)             << MPU_RASR_SIZE_Pos) & MPU_RASR_SIZE_Msk)                                | \
   (((MPU_RASR_ENABLE_Msk))))

                         
#define ARM_MPU_RASR(DisableExec, AccessPermission, TypeExtField, IsShareable, IsCacheable, IsBufferable, SubRegionDisable, Size) \
  ARM_MPU_RASR_EX(DisableExec, AccessPermission, ARM_MPU_ACCESS_(TypeExtField, IsShareable, IsCacheable, IsBufferable), SubRegionDisable, Size)

 
#define ARM_MPU_ACCESS_ORDERED ARM_MPU_ACCESS_(0U, 1U, 0U, 0U)

 
#define ARM_MPU_ACCESS_DEVICE(IsShareable) ((IsShareable) ? ARM_MPU_ACCESS_(0U, 1U, 0U, 1U) : ARM_MPU_ACCESS_(2U, 0U, 0U, 0U))

 
#define ARM_MPU_ACCESS_NORMAL(OuterCp, InnerCp, IsShareable) ARM_MPU_ACCESS_((4U | (OuterCp)), IsShareable, ((InnerCp) >> 1U), ((InnerCp) & 1U))


#define ARM_MPU_CACHEP_NOCACHE 0U


#define ARM_MPU_CACHEP_WB_WRA 1U


#define ARM_MPU_CACHEP_WT_NWA 2U


#define ARM_MPU_CACHEP_WB_NWA 3U



typedef struct {
  uint32_t RBAR; 
  uint32_t RASR; 
} ARM_MPU_Region_t;
    

__STATIC_INLINE void ARM_MPU_Enable(uint32_t MPU_Control)
{
  __DMB();
  MPU->CTRL = MPU_Control | MPU_CTRL_ENABLE_Msk;
#ifdef SCB_SHCSR_MEMFAULTENA_Msk
  SCB->SHCSR |= SCB_SHCSR_MEMFAULTENA_Msk;
#endif
  __DSB();
  __ISB();
}


__STATIC_INLINE void ARM_MPU_Disable(void)
{
  __DMB();
#ifdef SCB_SHCSR_MEMFAULTENA_Msk
  SCB->SHCSR &= ~SCB_SHCSR_MEMFAULTENA_Msk;
#endif
  MPU->CTRL  &= ~MPU_CTRL_ENABLE_Msk;
  __DSB();
  __ISB();
}


__STATIC_INLINE void ARM_MPU_ClrRegion(uint32_t rnr)
{
  MPU->RNR = rnr;
  MPU->RASR = 0U;
}

   
__STATIC_INLINE void ARM_MPU_SetRegion(uint32_t rbar, uint32_t rasr)
{
  MPU->RBAR = rbar;
  MPU->RASR = rasr;
}

   
__STATIC_INLINE void ARM_MPU_SetRegionEx(uint32_t rnr, uint32_t rbar, uint32_t rasr)
{
  MPU->RNR = rnr;
  MPU->RBAR = rbar;
  MPU->RASR = rasr;
}


__STATIC_INLINE void ARM_MPU_OrderedMemcpy(volatile uint32_t* dst, const uint32_t* __RESTRICT src, uint32_t len)
{
  uint32_t i;
  for (i = 0U; i < len; ++i) 
  {
    dst[i] = src[i];
  }
}


__STATIC_INLINE void ARM_MPU_Load(ARM_MPU_Region_t const* table, uint32_t cnt) 
{
  const uint32_t rowWordSize = sizeof(ARM_MPU_Region_t)/4U;
  while (cnt > MPU_TYPE_RALIASES) {
    ARM_MPU_OrderedMemcpy(&(MPU->RBAR), &(table->RBAR), MPU_TYPE_RALIASES*rowWordSize);
    table += MPU_TYPE_RALIASES;
    cnt -= MPU_TYPE_RALIASES;
  }
  ARM_MPU_OrderedMemcpy(&(MPU->RBAR), &(table->RBAR), cnt*rowWordSize);
}

#endif
