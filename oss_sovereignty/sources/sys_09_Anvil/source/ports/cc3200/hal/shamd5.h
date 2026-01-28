






































#ifndef __DRIVERLIB_SHAMD5_H__
#define __DRIVERLIB_SHAMD5_H__







#ifdef __cplusplus
extern "C"
{
#endif







#define SHAMD5_ALGO_MD5         0x00000018  
#define SHAMD5_ALGO_SHA1        0x0000001a  
#define SHAMD5_ALGO_SHA224      0x0000001c  
#define SHAMD5_ALGO_SHA256      0x0000001e  
#define SHAMD5_ALGO_HMAC_MD5    0x00000000  
#define SHAMD5_ALGO_HMAC_SHA1   0x00000002  
#define SHAMD5_ALGO_HMAC_SHA224 0x00000004  
#define SHAMD5_ALGO_HMAC_SHA256 0x00000006  








#define SHAMD5_INT_CONTEXT_READY   0x00000008
#define SHAMD5_INT_PARTHASH_READY  0x00000004
#define SHAMD5_INT_INPUT_READY     0x00000002
#define SHAMD5_INT_OUTPUT_READY    0x00000001
#define SHAMD5_INT_DMA_CONTEXT_IN  0x00010000
#define SHAMD5_INT_DMA_DATA_IN     0x00020000
#define SHAMD5_INT_DMA_CONTEXT_OUT 0x00040000






extern void SHAMD5ConfigSet(uint32_t ui32Base, uint32_t ui32Mode);
extern bool SHAMD5DataProcess(uint32_t ui32Base, uint8_t *pui8DataSrc,
                  uint32_t ui32DataLength, uint8_t *pui8HashResult);
extern void SHAMD5DataWrite(uint32_t ui32Base, uint8_t *pui8Src);
extern void SHAMD5DataWriteMultiple(uint8_t *pui8DataSrc, uint32_t ui32DataLength);
extern bool SHAMD5DataWriteNonBlocking(uint32_t ui32Base, uint8_t *pui8Src);
extern void SHAMD5DMADisable(uint32_t ui32Base);
extern void SHAMD5DMAEnable(uint32_t ui32Base);
extern void SHAMD5DataLengthSet(uint32_t ui32Base, uint32_t ui32Length);
extern void SHAMD5HMACKeySet(uint32_t ui32Base, uint8_t *pui8Src);
extern void SHAMD5HMACPPKeyGenerate(uint32_t ui32Base, uint8_t *pui8Key,
                        uint8_t *pui8PPKey);
extern void SHAMD5HMACPPKeySet(uint32_t ui32Base, uint8_t *pui8Src);
extern bool SHAMD5HMACProcess(uint32_t ui32Base, uint8_t *pui8DataSrc,
                  uint32_t ui32DataLength, uint8_t *pui8HashResult);
extern void SHAMD5IntClear(uint32_t ui32Base, uint32_t ui32IntFlags);
extern void SHAMD5IntDisable(uint32_t ui32Base, uint32_t ui32IntFlags);
extern void SHAMD5IntEnable(uint32_t ui32Base, uint32_t ui32IntFlags);
extern void SHAMD5IntRegister(uint32_t ui32Base, void(*pfnHandler)(void));
extern uint32_t SHAMD5IntStatus(uint32_t ui32Base, bool bMasked);
extern void SHAMD5IntUnregister(uint32_t ui32Base);
extern void SHAMD5ResultRead(uint32_t ui32Base, uint8_t *pui8Dest);






#ifdef __cplusplus
}
#endif

#endif 
