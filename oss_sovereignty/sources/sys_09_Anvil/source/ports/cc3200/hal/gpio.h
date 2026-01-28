






































#ifndef __GPIO_H__
#define __GPIO_H__







#ifdef __cplusplus
extern "C"
{
#endif







#define GPIO_PIN_0              0x00000001  
#define GPIO_PIN_1              0x00000002  
#define GPIO_PIN_2              0x00000004  
#define GPIO_PIN_3              0x00000008  
#define GPIO_PIN_4              0x00000010  
#define GPIO_PIN_5              0x00000020  
#define GPIO_PIN_6              0x00000040  
#define GPIO_PIN_7              0x00000080  







#define GPIO_DIR_MODE_IN        0x00000000  
#define GPIO_DIR_MODE_OUT       0x00000001  







#define GPIO_FALLING_EDGE       0x00000000  
#define GPIO_RISING_EDGE        0x00000004  
#define GPIO_BOTH_EDGES         0x00000001  
#define GPIO_LOW_LEVEL          0x00000002  
#define GPIO_HIGH_LEVEL         0x00000006  







#define GPIO_INT_DMA            0x00000100
#define GPIO_INT_PIN_0          0x00000001
#define GPIO_INT_PIN_1          0x00000002
#define GPIO_INT_PIN_2          0x00000004
#define GPIO_INT_PIN_3          0x00000008
#define GPIO_INT_PIN_4          0x00000010
#define GPIO_INT_PIN_5          0x00000020
#define GPIO_INT_PIN_6          0x00000040
#define GPIO_INT_PIN_7          0x00000080






extern void GPIODirModeSet(unsigned long ulPort, unsigned char ucPins,
                           unsigned long ulPinIO);
extern unsigned long GPIODirModeGet(unsigned long ulPort, unsigned char ucPin);
extern void GPIOIntTypeSet(unsigned long ulPort, unsigned char ucPins,
                           unsigned long ulIntType);
extern void GPIODMATriggerEnable(unsigned long ulPort);
extern void GPIODMATriggerDisable(unsigned long ulPort);
extern unsigned long GPIOIntTypeGet(unsigned long ulPort, unsigned char ucPin);
extern void GPIOIntEnable(unsigned long ulPort, unsigned long ulIntFlags);
extern void GPIOIntDisable(unsigned long ulPort, unsigned long ulIntFlags);
extern long GPIOIntStatus(unsigned long ulPort, tBoolean bMasked);
extern void GPIOIntClear(unsigned long ulPort, unsigned long ulIntFlags);
extern void GPIOIntRegister(unsigned long ulPort,
                                void (*pfnIntHandler)(void));
extern void GPIOIntUnregister(unsigned long ulPort);
extern long GPIOPinRead(unsigned long ulPort, unsigned char ucPins);
extern void GPIOPinWrite(unsigned long ulPort, unsigned char ucPins,
                         unsigned char ucVal);






#ifdef __cplusplus
}
#endif

#endif 
