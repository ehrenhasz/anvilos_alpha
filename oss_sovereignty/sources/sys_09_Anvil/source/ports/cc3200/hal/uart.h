






































#ifndef __UART_H__
#define __UART_H__







#ifdef __cplusplus
extern "C"
{
#endif







#define UART_INT_DMATX          0x20000     
#define UART_INT_DMARX          0x10000     
#define UART_INT_EOT            0x800       
#define UART_INT_OE             0x400       
#define UART_INT_BE             0x200       
#define UART_INT_PE             0x100       
#define UART_INT_FE             0x080       
#define UART_INT_RT             0x040       
#define UART_INT_TX             0x020       
#define UART_INT_RX             0x010       
#define UART_INT_CTS            0x002       











#define UART_CONFIG_WLEN_MASK   0x00000060  
#define UART_CONFIG_WLEN_8      0x00000060  
#define UART_CONFIG_WLEN_7      0x00000040  
#define UART_CONFIG_WLEN_6      0x00000020  
#define UART_CONFIG_WLEN_5      0x00000000  
#define UART_CONFIG_STOP_MASK   0x00000008  
#define UART_CONFIG_STOP_ONE    0x00000000  
#define UART_CONFIG_STOP_TWO    0x00000008  
#define UART_CONFIG_PAR_MASK    0x00000086  
#define UART_CONFIG_PAR_NONE    0x00000000  
#define UART_CONFIG_PAR_EVEN    0x00000006  
#define UART_CONFIG_PAR_ODD     0x00000002  
#define UART_CONFIG_PAR_ONE     0x00000082  
#define UART_CONFIG_PAR_ZERO    0x00000086  







#define UART_FIFO_TX1_8         0x00000000  
#define UART_FIFO_TX2_8         0x00000001  
#define UART_FIFO_TX4_8         0x00000002  
#define UART_FIFO_TX6_8         0x00000003  
#define UART_FIFO_TX7_8         0x00000004  







#define UART_FIFO_RX1_8         0x00000000  
#define UART_FIFO_RX2_8         0x00000008  
#define UART_FIFO_RX4_8         0x00000010  
#define UART_FIFO_RX6_8         0x00000018  
#define UART_FIFO_RX7_8         0x00000020  






#define UART_DMA_ERR_RXSTOP     0x00000004  
#define UART_DMA_TX             0x00000002  
#define UART_DMA_RX             0x00000001  






#define UART_RXERROR_OVERRUN    0x00000008
#define UART_RXERROR_BREAK      0x00000004
#define UART_RXERROR_PARITY     0x00000002
#define UART_RXERROR_FRAMING    0x00000001







#define UART_OUTPUT_RTS         0x00000800






#define UART_INPUT_CTS          0x00000001







#define UART_FLOWCONTROL_TX     0x00008000
#define UART_FLOWCONTROL_RX     0x00004000
#define UART_FLOWCONTROL_NONE   0x00000000







#define UART_TXINT_MODE_FIFO    0x00000000
#define UART_TXINT_MODE_EOT     0x00000010







extern void UARTParityModeSet(unsigned long ulBase, unsigned long ulParity);
extern unsigned long UARTParityModeGet(unsigned long ulBase);
extern void UARTFIFOLevelSet(unsigned long ulBase, unsigned long ulTxLevel,
                             unsigned long ulRxLevel);
extern void UARTFIFOLevelGet(unsigned long ulBase, unsigned long *pulTxLevel,
                             unsigned long *pulRxLevel);
extern void UARTConfigSetExpClk(unsigned long ulBase, unsigned long ulUARTClk,
                                unsigned long ulBaud, unsigned long ulConfig);
extern void UARTConfigGetExpClk(unsigned long ulBase, unsigned long ulUARTClk,
                                unsigned long *pulBaud,
                                unsigned long *pulConfig);
extern void UARTEnable(unsigned long ulBase);
extern void UARTDisable(unsigned long ulBase);
extern void UARTFIFOEnable(unsigned long ulBase);
extern void UARTFIFODisable(unsigned long ulBase);
extern tBoolean UARTCharsAvail(unsigned long ulBase);
extern tBoolean UARTSpaceAvail(unsigned long ulBase);
extern long UARTCharGetNonBlocking(unsigned long ulBase);
extern long UARTCharGet(unsigned long ulBase);
extern tBoolean UARTCharPutNonBlocking(unsigned long ulBase,
                                       unsigned char ucData);
extern void UARTCharPut(unsigned long ulBase, unsigned char ucData);
extern void UARTBreakCtl(unsigned long ulBase, tBoolean bBreakState);
extern tBoolean UARTBusy(unsigned long ulBase);
extern void UARTIntRegister(unsigned long ulBase, void(*pfnHandler)(void));
extern void UARTIntUnregister(unsigned long ulBase);
extern void UARTIntEnable(unsigned long ulBase, unsigned long ulIntFlags);
extern void UARTIntDisable(unsigned long ulBase, unsigned long ulIntFlags);
extern unsigned long UARTIntStatus(unsigned long ulBase, tBoolean bMasked);
extern void UARTIntClear(unsigned long ulBase, unsigned long ulIntFlags);
extern void UARTDMAEnable(unsigned long ulBase, unsigned long ulDMAFlags);
extern void UARTDMADisable(unsigned long ulBase, unsigned long ulDMAFlags);
extern unsigned long UARTRxErrorGet(unsigned long ulBase);
extern void UARTRxErrorClear(unsigned long ulBase);
extern void UARTModemControlSet(unsigned long ulBase,
                                unsigned long ulControl);
extern void UARTModemControlClear(unsigned long ulBase,
                                  unsigned long ulControl);
extern unsigned long UARTModemControlGet(unsigned long ulBase);
extern unsigned long UARTModemStatusGet(unsigned long ulBase);
extern void UARTFlowControlSet(unsigned long ulBase, unsigned long ulMode);
extern unsigned long UARTFlowControlGet(unsigned long ulBase);
extern void UARTTxIntModeSet(unsigned long ulBase, unsigned long ulMode);
extern unsigned long UARTTxIntModeGet(unsigned long ulBase);






#ifdef __cplusplus
}
#endif

#endif 
