


#if   defined ( __ICCARM__ )
  #pragma system_include         
#elif defined (__clang__)
  #pragma clang system_header    
#endif

#ifndef ARM_CACHEL1_ARMV7_H
#define ARM_CACHEL1_ARMV7_H




#define CCSIDR_WAYS(x)         (((x) & SCB_CCSIDR_ASSOCIATIVITY_Msk) >> SCB_CCSIDR_ASSOCIATIVITY_Pos)
#define CCSIDR_SETS(x)         (((x) & SCB_CCSIDR_NUMSETS_Msk      ) >> SCB_CCSIDR_NUMSETS_Pos      )

#ifndef __SCB_DCACHE_LINE_SIZE
#define __SCB_DCACHE_LINE_SIZE  32U 
#endif

#ifndef __SCB_ICACHE_LINE_SIZE
#define __SCB_ICACHE_LINE_SIZE  32U 
#endif


__STATIC_FORCEINLINE void SCB_EnableICache (void)
{
  #if defined (__ICACHE_PRESENT) && (__ICACHE_PRESENT == 1U)
    if (SCB->CCR & SCB_CCR_IC_Msk) return;  

    __DSB();
    __ISB();
    SCB->ICIALLU = 0UL;                     
    __DSB();
    __ISB();
    SCB->CCR |=  (uint32_t)SCB_CCR_IC_Msk;  
    __DSB();
    __ISB();
  #endif
}



__STATIC_FORCEINLINE void SCB_DisableICache (void)
{
  #if defined (__ICACHE_PRESENT) && (__ICACHE_PRESENT == 1U)
    __DSB();
    __ISB();
    SCB->CCR &= ~(uint32_t)SCB_CCR_IC_Msk;  
    SCB->ICIALLU = 0UL;                     
    __DSB();
    __ISB();
  #endif
}



__STATIC_FORCEINLINE void SCB_InvalidateICache (void)
{
  #if defined (__ICACHE_PRESENT) && (__ICACHE_PRESENT == 1U)
    __DSB();
    __ISB();
    SCB->ICIALLU = 0UL;
    __DSB();
    __ISB();
  #endif
}



__STATIC_FORCEINLINE void SCB_InvalidateICache_by_Addr (volatile void *addr, int32_t isize)
{
  #if defined (__ICACHE_PRESENT) && (__ICACHE_PRESENT == 1U)
    if ( isize > 0 ) {
       int32_t op_size = isize + (((uint32_t)addr) & (__SCB_ICACHE_LINE_SIZE - 1U));
      uint32_t op_addr = (uint32_t)addr ;

      __DSB();

      do {
        SCB->ICIMVAU = op_addr;             
        op_addr += __SCB_ICACHE_LINE_SIZE;
        op_size -= __SCB_ICACHE_LINE_SIZE;
      } while ( op_size > 0 );

      __DSB();
      __ISB();
    }
  #endif
}



__STATIC_FORCEINLINE void SCB_EnableDCache (void)
{
  #if defined (__DCACHE_PRESENT) && (__DCACHE_PRESENT == 1U)
    uint32_t ccsidr;
    uint32_t sets;
    uint32_t ways;

    if (SCB->CCR & SCB_CCR_DC_Msk) return;  

    SCB->CSSELR = 0U;                       
    __DSB();

    ccsidr = SCB->CCSIDR;

                                            
    sets = (uint32_t)(CCSIDR_SETS(ccsidr));
    do {
      ways = (uint32_t)(CCSIDR_WAYS(ccsidr));
      do {
        SCB->DCISW = (((sets << SCB_DCISW_SET_Pos) & SCB_DCISW_SET_Msk) |
                      ((ways << SCB_DCISW_WAY_Pos) & SCB_DCISW_WAY_Msk)  );
        #if defined ( __CC_ARM )
          __schedule_barrier();
        #endif
      } while (ways-- != 0U);
    } while(sets-- != 0U);
    __DSB();

    SCB->CCR |=  (uint32_t)SCB_CCR_DC_Msk;  

    __DSB();
    __ISB();
  #endif
}



__STATIC_FORCEINLINE void SCB_DisableDCache (void)
{
  #if defined (__DCACHE_PRESENT) && (__DCACHE_PRESENT == 1U)
    uint32_t ccsidr;
    uint32_t sets;
    uint32_t ways;

    SCB->CSSELR = 0U;                       
    __DSB();

    SCB->CCR &= ~(uint32_t)SCB_CCR_DC_Msk;  
    __DSB();

    ccsidr = SCB->CCSIDR;

                                            
    sets = (uint32_t)(CCSIDR_SETS(ccsidr));
    do {
      ways = (uint32_t)(CCSIDR_WAYS(ccsidr));
      do {
        SCB->DCCISW = (((sets << SCB_DCCISW_SET_Pos) & SCB_DCCISW_SET_Msk) |
                       ((ways << SCB_DCCISW_WAY_Pos) & SCB_DCCISW_WAY_Msk)  );
        #if defined ( __CC_ARM )
          __schedule_barrier();
        #endif
      } while (ways-- != 0U);
    } while(sets-- != 0U);

    __DSB();
    __ISB();
  #endif
}



__STATIC_FORCEINLINE void SCB_InvalidateDCache (void)
{
  #if defined (__DCACHE_PRESENT) && (__DCACHE_PRESENT == 1U)
    uint32_t ccsidr;
    uint32_t sets;
    uint32_t ways;

    SCB->CSSELR = 0U;                       
    __DSB();

    ccsidr = SCB->CCSIDR;

                                            
    sets = (uint32_t)(CCSIDR_SETS(ccsidr));
    do {
      ways = (uint32_t)(CCSIDR_WAYS(ccsidr));
      do {
        SCB->DCISW = (((sets << SCB_DCISW_SET_Pos) & SCB_DCISW_SET_Msk) |
                      ((ways << SCB_DCISW_WAY_Pos) & SCB_DCISW_WAY_Msk)  );
        #if defined ( __CC_ARM )
          __schedule_barrier();
        #endif
      } while (ways-- != 0U);
    } while(sets-- != 0U);

    __DSB();
    __ISB();
  #endif
}



__STATIC_FORCEINLINE void SCB_CleanDCache (void)
{
  #if defined (__DCACHE_PRESENT) && (__DCACHE_PRESENT == 1U)
    uint32_t ccsidr;
    uint32_t sets;
    uint32_t ways;

    SCB->CSSELR = 0U;                       
    __DSB();

    ccsidr = SCB->CCSIDR;

                                            
    sets = (uint32_t)(CCSIDR_SETS(ccsidr));
    do {
      ways = (uint32_t)(CCSIDR_WAYS(ccsidr));
      do {
        SCB->DCCSW = (((sets << SCB_DCCSW_SET_Pos) & SCB_DCCSW_SET_Msk) |
                      ((ways << SCB_DCCSW_WAY_Pos) & SCB_DCCSW_WAY_Msk)  );
        #if defined ( __CC_ARM )
          __schedule_barrier();
        #endif
      } while (ways-- != 0U);
    } while(sets-- != 0U);

    __DSB();
    __ISB();
  #endif
}



__STATIC_FORCEINLINE void SCB_CleanInvalidateDCache (void)
{
  #if defined (__DCACHE_PRESENT) && (__DCACHE_PRESENT == 1U)
    uint32_t ccsidr;
    uint32_t sets;
    uint32_t ways;

    SCB->CSSELR = 0U;                       
    __DSB();

    ccsidr = SCB->CCSIDR;

                                            
    sets = (uint32_t)(CCSIDR_SETS(ccsidr));
    do {
      ways = (uint32_t)(CCSIDR_WAYS(ccsidr));
      do {
        SCB->DCCISW = (((sets << SCB_DCCISW_SET_Pos) & SCB_DCCISW_SET_Msk) |
                       ((ways << SCB_DCCISW_WAY_Pos) & SCB_DCCISW_WAY_Msk)  );
        #if defined ( __CC_ARM )
          __schedule_barrier();
        #endif
      } while (ways-- != 0U);
    } while(sets-- != 0U);

    __DSB();
    __ISB();
  #endif
}



__STATIC_FORCEINLINE void SCB_InvalidateDCache_by_Addr (volatile void *addr, int32_t dsize)
{
  #if defined (__DCACHE_PRESENT) && (__DCACHE_PRESENT == 1U)
    if ( dsize > 0 ) {
       int32_t op_size = dsize + (((uint32_t)addr) & (__SCB_DCACHE_LINE_SIZE - 1U));
      uint32_t op_addr = (uint32_t)addr ;

      __DSB();

      do {
        SCB->DCIMVAC = op_addr;             
        op_addr += __SCB_DCACHE_LINE_SIZE;
        op_size -= __SCB_DCACHE_LINE_SIZE;
      } while ( op_size > 0 );

      __DSB();
      __ISB();
    }
  #endif
}



__STATIC_FORCEINLINE void SCB_CleanDCache_by_Addr (volatile void *addr, int32_t dsize)
{
  #if defined (__DCACHE_PRESENT) && (__DCACHE_PRESENT == 1U)
    if ( dsize > 0 ) {
       int32_t op_size = dsize + (((uint32_t)addr) & (__SCB_DCACHE_LINE_SIZE - 1U));
      uint32_t op_addr = (uint32_t)addr ;

      __DSB();

      do {
        SCB->DCCMVAC = op_addr;             
        op_addr += __SCB_DCACHE_LINE_SIZE;
        op_size -= __SCB_DCACHE_LINE_SIZE;
      } while ( op_size > 0 );

      __DSB();
      __ISB();
    }
  #endif
}



__STATIC_FORCEINLINE void SCB_CleanInvalidateDCache_by_Addr (volatile void *addr, int32_t dsize)
{
  #if defined (__DCACHE_PRESENT) && (__DCACHE_PRESENT == 1U)
    if ( dsize > 0 ) {
       int32_t op_size = dsize + (((uint32_t)addr) & (__SCB_DCACHE_LINE_SIZE - 1U));
      uint32_t op_addr = (uint32_t)addr ;

      __DSB();

      do {
        SCB->DCCIMVAC = op_addr;            
        op_addr +=          __SCB_DCACHE_LINE_SIZE;
        op_size -=          __SCB_DCACHE_LINE_SIZE;
      } while ( op_size > 0 );

      __DSB();
      __ISB();
    }
  #endif
}



#endif 
