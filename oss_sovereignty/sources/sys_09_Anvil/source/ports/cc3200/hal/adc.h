






































#ifndef __ADC_H__
#define __ADC_H__







#ifdef __cplusplus
extern "C"
{
#endif




#define ADC_CH_0   0x00000000
#define ADC_CH_1   0x00000008
#define ADC_CH_2   0x00000010
#define ADC_CH_3   0x00000018








#define ADC_DMA_DONE        0x00000010
#define ADC_FIFO_OVERFLOW   0x00000008
#define ADC_FIFO_UNDERFLOW  0x00000004
#define ADC_FIFO_EMPTY      0x00000002
#define ADC_FIFO_FULL       0x00000001







extern void ADCEnable(unsigned long ulBase);
extern void ADCDisable(unsigned long ulBase);
extern void ADCChannelEnable(unsigned long ulBase,unsigned long ulChannel);
extern void ADCChannelDisable(unsigned long ulBase,unsigned long ulChannel);
extern void ADCIntRegister(unsigned long ulBase, unsigned long ulChannel,
                    void (*pfnHandler)(void));
extern void ADCIntUnregister(unsigned long ulBase, unsigned long ulChannel);
extern void ADCIntEnable(unsigned long ulBase, unsigned long ulChannel,
                  unsigned long ulIntFlags);
extern void ADCIntDisable(unsigned long ulBase, unsigned long ulChannel,
                  unsigned long ulIntFlags);
extern unsigned long ADCIntStatus(unsigned long ulBase,unsigned long ulChannel);
extern void ADCIntClear(unsigned long ulBase, unsigned long ulChannel,
                  unsigned long ulIntFlags);
extern void ADCDMAEnable(unsigned long ulBase, unsigned long ulChannel);
extern void ADCDMADisable(unsigned long ulBase, unsigned long ulChannel);
extern void ADCTimerConfig(unsigned long ulBase, unsigned long ulValue);
extern void ADCTimerEnable(unsigned long ulBase);
extern void ADCTimerDisable(unsigned long ulBase);
extern void ADCTimerReset(unsigned long ulBase);
extern unsigned long ADCTimerValueGet(unsigned long ulBase);
extern unsigned char ADCFIFOLvlGet(unsigned long ulBase,
                                   unsigned long ulChannel);
extern unsigned long ADCFIFORead(unsigned long ulBase,
                                   unsigned long ulChannel);






#ifdef __cplusplus
}
#endif

#endif 

