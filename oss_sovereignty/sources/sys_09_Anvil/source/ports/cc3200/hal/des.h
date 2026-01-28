






































#ifndef __DRIVERLIB_DES_H__
#define __DRIVERLIB_DES_H__







#ifdef __cplusplus
extern "C"
{
#endif







#define DES_CFG_DIR_DECRYPT     0x00000000
#define DES_CFG_DIR_ENCRYPT     0x00000004







#define DES_CFG_MODE_ECB        0x00000000
#define DES_CFG_MODE_CBC        0x00000010
#define DES_CFG_MODE_CFB        0x00000020








#define DES_CFG_SINGLE          0x00000000
#define DES_CFG_TRIPLE          0x00000008







#define DES_INT_CONTEXT_IN      0x00000001
#define DES_INT_DATA_IN         0x00000002
#define DES_INT_DATA_OUT        0x00000004
#define DES_INT_DMA_CONTEXT_IN  0x00010000
#define DES_INT_DMA_DATA_IN     0x00020000
#define DES_INT_DMA_DATA_OUT    0x00040000







#define DES_DMA_CONTEXT_IN      0x00000080
#define DES_DMA_DATA_OUT        0x00000040
#define DES_DMA_DATA_IN         0x00000020






extern void DESConfigSet(uint32_t ui32Base, uint32_t ui32Config);
extern void DESDataRead(uint32_t ui32Base, uint8_t *pui8Dest,
                        uint8_t ui8Length);
extern bool DESDataReadNonBlocking(uint32_t ui32Base, uint8_t *pui8Dest,
                                   uint8_t ui8Length);
extern bool DESDataProcess(uint32_t ui32Base, uint8_t *pui8Src,
                           uint8_t *pui8Dest, uint32_t ui32Length);
extern void DESDataWrite(uint32_t ui32Base, uint8_t *pui8Src,
                         uint8_t ui8Length);
extern bool DESDataWriteNonBlocking(uint32_t ui32Base, uint8_t *pui8Src,
                                    uint8_t ui8Length);
extern void DESDMADisable(uint32_t ui32Base, uint32_t ui32Flags);
extern void DESDMAEnable(uint32_t ui32Base, uint32_t ui32Flags);
extern void DESIntClear(uint32_t ui32Base, uint32_t ui32IntFlags);
extern void DESIntDisable(uint32_t ui32Base, uint32_t ui32IntFlags);
extern void DESIntEnable(uint32_t ui32Base, uint32_t ui32IntFlags);
extern void DESIntRegister(uint32_t ui32Base, void(*pfnHandler)(void));
extern uint32_t DESIntStatus(uint32_t ui32Base, bool bMasked);
extern void DESIntUnregister(uint32_t ui32Base);
extern bool DESIVSet(uint32_t ui32Base, uint8_t *pui8IVdata);
extern void DESKeySet(uint32_t ui32Base, uint8_t *pui8Key);
extern void DESDataLengthSet(uint32_t ui32Base, uint32_t ui32Length);






#ifdef __cplusplus
}
#endif

#endif 
