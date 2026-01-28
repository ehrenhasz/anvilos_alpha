






































#ifndef __INTERRUPT_H__
#define __INTERRUPT_H__







#ifdef __cplusplus
extern "C"
{
#endif








typedef union
{
    void (*pfnHandler)(void);
    unsigned long ulPtr;
}
uVectorEntry;








#define INT_PRIORITY_MASK       ((0xFF << (8 - NUM_PRIORITY_BITS)) & 0xFF)




#define INT_PRIORITY_LVL_0      0x00
#define INT_PRIORITY_LVL_1      0x20
#define INT_PRIORITY_LVL_2      0x40
#define INT_PRIORITY_LVL_3      0x60
#define INT_PRIORITY_LVL_4      0x80
#define INT_PRIORITY_LVL_5      0xA0
#define INT_PRIORITY_LVL_6      0xC0
#define INT_PRIORITY_LVL_7      0xE0






extern tBoolean IntMasterEnable(void);
extern tBoolean IntMasterDisable(void);
extern void IntVTableBaseSet(unsigned long ulVtableBase);
extern void IntRegister(unsigned long ulInterrupt, void (*pfnHandler)(void));
extern void IntUnregister(unsigned long ulInterrupt);
extern void IntPriorityGroupingSet(unsigned long ulBits);
extern unsigned long IntPriorityGroupingGet(void);
extern void IntPrioritySet(unsigned long ulInterrupt,
                           unsigned char ucPriority);
extern long IntPriorityGet(unsigned long ulInterrupt);
extern void IntEnable(unsigned long ulInterrupt);
extern void IntDisable(unsigned long ulInterrupt);
extern void IntPendSet(unsigned long ulInterrupt);
extern void IntPendClear(unsigned long ulInterrupt);
extern void IntPriorityMaskSet(unsigned long ulPriorityMask);
extern unsigned long IntPriorityMaskGet(void);






#ifdef __cplusplus
}
#endif

#endif 
