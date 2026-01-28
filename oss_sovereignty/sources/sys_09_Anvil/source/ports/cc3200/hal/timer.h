






































#ifndef __TIMER_H__
#define __TIMER_H__







#ifdef __cplusplus
extern "C"
{
#endif







#define TIMER_CFG_ONE_SHOT       0x00000021  
#define TIMER_CFG_ONE_SHOT_UP    0x00000031  
                                             
#define TIMER_CFG_PERIODIC       0x00000022  
#define TIMER_CFG_PERIODIC_UP    0x00000032  
                                             
#define TIMER_CFG_SPLIT_PAIR     0x04000000  

#define TIMER_CFG_A_ONE_SHOT     0x00000021  
#define TIMER_CFG_A_ONE_SHOT_UP  0x00000031  
#define TIMER_CFG_A_PERIODIC     0x00000022  
#define TIMER_CFG_A_PERIODIC_UP  0x00000032  
#define TIMER_CFG_A_CAP_COUNT    0x00000003  
#define TIMER_CFG_A_CAP_COUNT_UP 0x00000013  
#define TIMER_CFG_A_CAP_TIME     0x00000007  
#define TIMER_CFG_A_CAP_TIME_UP  0x00000017  
#define TIMER_CFG_A_PWM          0x0000000A  
#define TIMER_CFG_B_ONE_SHOT     0x00002100  
#define TIMER_CFG_B_ONE_SHOT_UP  0x00003100  
#define TIMER_CFG_B_PERIODIC     0x00002200  
#define TIMER_CFG_B_PERIODIC_UP  0x00003200  
#define TIMER_CFG_B_CAP_COUNT    0x00000300  
#define TIMER_CFG_B_CAP_COUNT_UP 0x00001300  
#define TIMER_CFG_B_CAP_TIME     0x00000700  
#define TIMER_CFG_B_CAP_TIME_UP  0x00001700  
#define TIMER_CFG_B_PWM          0x00000A00  








#define TIMER_TIMB_DMA          0x00002000  
#define TIMER_TIMB_MATCH        0x00000800  
#define TIMER_CAPB_EVENT        0x00000400  
#define TIMER_CAPB_MATCH        0x00000200  
#define TIMER_TIMB_TIMEOUT      0x00000100  
#define TIMER_TIMA_DMA          0x00000020  
#define TIMER_TIMA_MATCH        0x00000010  
#define TIMER_CAPA_EVENT        0x00000004  
#define TIMER_CAPA_MATCH        0x00000002  
#define TIMER_TIMA_TIMEOUT      0x00000001  






#define TIMER_EVENT_POS_EDGE    0x00000000  
#define TIMER_EVENT_NEG_EDGE    0x00000404  
#define TIMER_EVENT_BOTH_EDGES  0x00000C0C  







#define TIMER_A                 0x000000ff  
#define TIMER_B                 0x0000ff00  
#define TIMER_BOTH              0x0000ffff  







#define TIMER_0A_SYNC           0x00000001  
#define TIMER_0B_SYNC           0x00000002  
#define TIMER_1A_SYNC           0x00000004  
#define TIMER_1B_SYNC           0x00000008  
#define TIMER_2A_SYNC           0x00000010  
#define TIMER_2B_SYNC           0x00000020  
#define TIMER_3A_SYNC           0x00000040  
#define TIMER_3B_SYNC           0x00000080  







#define TIMER_DMA_MODEMATCH_B   0x00000800
#define TIMER_DMA_CAPEVENT_B    0x00000400
#define TIMER_DMA_CAPMATCH_B    0x00000200
#define TIMER_DMA_TIMEOUT_B     0x00000100
#define TIMER_DMA_MODEMATCH_A   0x00000010
#define TIMER_DMA_CAPEVENT_A    0x00000004
#define TIMER_DMA_CAPMATCH_A    0x00000002
#define TIMER_DMA_TIMEOUT_A     0x00000001







extern void TimerEnable(unsigned long ulBase, unsigned long ulTimer);
extern void TimerDisable(unsigned long ulBase, unsigned long ulTimer);
extern void TimerConfigure(unsigned long ulBase, unsigned long ulConfig);
extern void TimerControlLevel(unsigned long ulBase, unsigned long ulTimer,
                              tBoolean bInvert);
extern void TimerControlEvent(unsigned long ulBase, unsigned long ulTimer,
                              unsigned long ulEvent);
extern void TimerControlStall(unsigned long ulBase, unsigned long ulTimer,
                              tBoolean bStall);
extern void TimerPrescaleSet(unsigned long ulBase, unsigned long ulTimer,
                             unsigned long ulValue);
extern unsigned long TimerPrescaleGet(unsigned long ulBase,
                                      unsigned long ulTimer);
extern void TimerPrescaleMatchSet(unsigned long ulBase, unsigned long ulTimer,
                                  unsigned long ulValue);
extern unsigned long TimerPrescaleMatchGet(unsigned long ulBase,
                                           unsigned long ulTimer);
extern void TimerLoadSet(unsigned long ulBase, unsigned long ulTimer,
                         unsigned long ulValue);
extern unsigned long TimerLoadGet(unsigned long ulBase, unsigned long ulTimer);

extern unsigned long TimerValueGet(unsigned long ulBase,
                                   unsigned long ulTimer);
extern void TimerValueSet(unsigned long ulBase, unsigned long ulTimer,
              unsigned long ulValue);

extern void TimerMatchSet(unsigned long ulBase, unsigned long ulTimer,
                          unsigned long ulValue);
extern unsigned long TimerMatchGet(unsigned long ulBase,
                                   unsigned long ulTimer);
extern void TimerIntRegister(unsigned long ulBase, unsigned long ulTimer,
                             void (*pfnHandler)(void));
extern void TimerIntUnregister(unsigned long ulBase, unsigned long ulTimer);
extern void TimerIntEnable(unsigned long ulBase, unsigned long ulIntFlags);
extern void TimerIntDisable(unsigned long ulBase, unsigned long ulIntFlags);
extern unsigned long TimerIntStatus(unsigned long ulBase, tBoolean bMasked);
extern void TimerIntClear(unsigned long ulBase, unsigned long ulIntFlags);
extern void TimerDMAEventSet(unsigned long ulBase, unsigned long ulDMAEvent);
extern unsigned long TimerDMAEventGet(unsigned long ulBase);







#ifdef __cplusplus
}
#endif

#endif 
