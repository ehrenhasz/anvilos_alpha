


#if   defined ( __ICCARM__ )
  #pragma system_include         
#elif defined (__clang__)
  #pragma clang system_header    
#endif

#ifndef ARM_MPU_ARMV8_H
#define ARM_MPU_ARMV8_H


#define ARM_MPU_ATTR_DEVICE                           ( 0U )


#define ARM_MPU_ATTR_NON_CACHEABLE                    ( 4U )


#define ARM_MPU_ATTR_MEMORY_(NT, WB, RA, WA) \
  ((((NT) & 1U) << 3U) | (((WB) & 1U) << 2U) | (((RA) & 1U) << 1U) | ((WA) & 1U))


#define ARM_MPU_ATTR_DEVICE_nGnRnE (0U)


#define ARM_MPU_ATTR_DEVICE_nGnRE  (1U)


#define ARM_MPU_ATTR_DEVICE_nGRE   (2U)


#define ARM_MPU_ATTR_DEVICE_GRE    (3U)


#define ARM_MPU_ATTR(O, I) ((((O) & 0xFU) << 4U) | ((((O) & 0xFU) != 0U) ? ((I) & 0xFU) : (((I) & 0x3U) << 2U)))


#define ARM_MPU_SH_NON   (0U)


#define ARM_MPU_SH_OUTER (2U)


#define ARM_MPU_SH_INNER (3U)


#define ARM_MPU_AP_(RO, NP) ((((RO) & 1U) << 1U) | ((NP) & 1U))


#define ARM_MPU_RBAR(BASE, SH, RO, NP, XN) \
  (((BASE) & MPU_RBAR_BASE_Msk) | \
  (((SH) << MPU_RBAR_SH_Pos) & MPU_RBAR_SH_Msk) | \
  ((ARM_MPU_AP_(RO, NP) << MPU_RBAR_AP_Pos) & MPU_RBAR_AP_Msk) | \
  (((XN) << MPU_RBAR_XN_Pos) & MPU_RBAR_XN_Msk))


#define ARM_MPU_RLAR(LIMIT, IDX) \
  (((LIMIT) & MPU_RLAR_LIMIT_Msk) | \
  (((IDX) << MPU_RLAR_AttrIndx_Pos) & MPU_RLAR_AttrIndx_Msk) | \
  (MPU_RLAR_EN_Msk))

#if defined(MPU_RLAR_PXN_Pos)
  

#define ARM_MPU_RLAR_PXN(LIMIT, PXN, IDX) \
  (((LIMIT) & MPU_RLAR_LIMIT_Msk) | \
  (((PXN) << MPU_RLAR_PXN_Pos) & MPU_RLAR_PXN_Msk) | \
  (((IDX) << MPU_RLAR_AttrIndx_Pos) & MPU_RLAR_AttrIndx_Msk) | \
  (MPU_RLAR_EN_Msk))
  
#endif


typedef struct {
  uint32_t RBAR;                   
  uint32_t RLAR;                   
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

#ifdef MPU_NS

__STATIC_INLINE void ARM_MPU_Enable_NS(uint32_t MPU_Control)
{
  __DMB();
  MPU_NS->CTRL = MPU_Control | MPU_CTRL_ENABLE_Msk;
#ifdef SCB_SHCSR_MEMFAULTENA_Msk
  SCB_NS->SHCSR |= SCB_SHCSR_MEMFAULTENA_Msk;
#endif
  __DSB();
  __ISB();
}


__STATIC_INLINE void ARM_MPU_Disable_NS(void)
{
  __DMB();
#ifdef SCB_SHCSR_MEMFAULTENA_Msk
  SCB_NS->SHCSR &= ~SCB_SHCSR_MEMFAULTENA_Msk;
#endif
  MPU_NS->CTRL  &= ~MPU_CTRL_ENABLE_Msk;
  __DSB();
  __ISB();
}
#endif


__STATIC_INLINE void ARM_MPU_SetMemAttrEx(MPU_Type* mpu, uint8_t idx, uint8_t attr)
{
  const uint8_t reg = idx / 4U;
  const uint32_t pos = ((idx % 4U) * 8U);
  const uint32_t mask = 0xFFU << pos;
  
  if (reg >= (sizeof(mpu->MAIR) / sizeof(mpu->MAIR[0]))) {
    return; 
  }
  
  mpu->MAIR[reg] = ((mpu->MAIR[reg] & ~mask) | ((attr << pos) & mask));
}


__STATIC_INLINE void ARM_MPU_SetMemAttr(uint8_t idx, uint8_t attr)
{
  ARM_MPU_SetMemAttrEx(MPU, idx, attr);
}

#ifdef MPU_NS

__STATIC_INLINE void ARM_MPU_SetMemAttr_NS(uint8_t idx, uint8_t attr)
{
  ARM_MPU_SetMemAttrEx(MPU_NS, idx, attr);
}
#endif


__STATIC_INLINE void ARM_MPU_ClrRegionEx(MPU_Type* mpu, uint32_t rnr)
{
  mpu->RNR = rnr;
  mpu->RLAR = 0U;
}


__STATIC_INLINE void ARM_MPU_ClrRegion(uint32_t rnr)
{
  ARM_MPU_ClrRegionEx(MPU, rnr);
}

#ifdef MPU_NS

__STATIC_INLINE void ARM_MPU_ClrRegion_NS(uint32_t rnr)
{  
  ARM_MPU_ClrRegionEx(MPU_NS, rnr);
}
#endif

   
__STATIC_INLINE void ARM_MPU_SetRegionEx(MPU_Type* mpu, uint32_t rnr, uint32_t rbar, uint32_t rlar)
{
  mpu->RNR = rnr;
  mpu->RBAR = rbar;
  mpu->RLAR = rlar;
}

   
__STATIC_INLINE void ARM_MPU_SetRegion(uint32_t rnr, uint32_t rbar, uint32_t rlar)
{
  ARM_MPU_SetRegionEx(MPU, rnr, rbar, rlar);
}

#ifdef MPU_NS
   
__STATIC_INLINE void ARM_MPU_SetRegion_NS(uint32_t rnr, uint32_t rbar, uint32_t rlar)
{
  ARM_MPU_SetRegionEx(MPU_NS, rnr, rbar, rlar);  
}
#endif


__STATIC_INLINE void ARM_MPU_OrderedMemcpy(volatile uint32_t* dst, const uint32_t* __RESTRICT src, uint32_t len)
{
  uint32_t i;
  for (i = 0U; i < len; ++i) 
  {
    dst[i] = src[i];
  }
}


__STATIC_INLINE void ARM_MPU_LoadEx(MPU_Type* mpu, uint32_t rnr, ARM_MPU_Region_t const* table, uint32_t cnt) 
{
  const uint32_t rowWordSize = sizeof(ARM_MPU_Region_t)/4U;
  if (cnt == 1U) {
    mpu->RNR = rnr;
    ARM_MPU_OrderedMemcpy(&(mpu->RBAR), &(table->RBAR), rowWordSize);
  } else {
    uint32_t rnrBase   = rnr & ~(MPU_TYPE_RALIASES-1U);
    uint32_t rnrOffset = rnr % MPU_TYPE_RALIASES;
    
    mpu->RNR = rnrBase;
    while ((rnrOffset + cnt) > MPU_TYPE_RALIASES) {
      uint32_t c = MPU_TYPE_RALIASES - rnrOffset;
      ARM_MPU_OrderedMemcpy(&(mpu->RBAR)+(rnrOffset*2U), &(table->RBAR), c*rowWordSize);
      table += c;
      cnt -= c;
      rnrOffset = 0U;
      rnrBase += MPU_TYPE_RALIASES;
      mpu->RNR = rnrBase;
    }
    
    ARM_MPU_OrderedMemcpy(&(mpu->RBAR)+(rnrOffset*2U), &(table->RBAR), cnt*rowWordSize);
  }
}


__STATIC_INLINE void ARM_MPU_Load(uint32_t rnr, ARM_MPU_Region_t const* table, uint32_t cnt) 
{
  ARM_MPU_LoadEx(MPU, rnr, table, cnt);
}

#ifdef MPU_NS

__STATIC_INLINE void ARM_MPU_Load_NS(uint32_t rnr, ARM_MPU_Region_t const* table, uint32_t cnt) 
{
  ARM_MPU_LoadEx(MPU_NS, rnr, table, cnt);
}
#endif

#endif

