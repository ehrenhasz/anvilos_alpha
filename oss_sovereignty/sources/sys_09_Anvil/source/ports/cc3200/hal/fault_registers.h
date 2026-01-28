

#ifndef FAULT_REGISTERS_H_
#define FAULT_REGISTERS_H_


typedef struct
{
    uint32_t    IERR        :1;
    uint32_t    DERR        :1;
    uint32_t                :1;
    uint32_t    MUSTKE      :1;
    uint32_t    MSTKE       :1;
    uint32_t    MLSPERR     :1;
    uint32_t                :1;
    uint32_t    MMARV       :1;
    uint32_t    IBUS        :1;
    uint32_t    PRECISE     :1;
    uint32_t    IMPRE       :1;
    uint32_t    BUSTKE      :1;
    uint32_t    BSTKE       :1;
    uint32_t    BLSPERR     :1;
    uint32_t                :1;
    uint32_t    BFARV       :1;
    uint32_t    UNDEF       :1;
    uint32_t    INVSTAT     :1;
    uint32_t    INVCP       :1;
    uint32_t    NOCP        :1;
    uint32_t                :4;
    uint32_t    UNALIGN     :1;
    uint32_t    DIVO0       :1;
    uint32_t                :6;

}_CFSR_t;


typedef struct
{

    uint32_t    DBG         :1;
    uint32_t    FORCED      :1;
    uint32_t                :28;
    uint32_t    VECT        :1;
    uint32_t                :1;

}_HFSR_t;


#endif 
