






































#ifndef __DRIVERLIB_CRC_H__
#define __DRIVERLIB_CRC_H__







#ifdef __cplusplus
extern "C"
{
#endif







#define CRC_CFG_INIT_SEED       0x00000000  
#define CRC_CFG_INIT_0          0x00004000  
#define CRC_CFG_INIT_1          0x00006000  
#define CRC_CFG_SIZE_8BIT       0x00001000  
#define CRC_CFG_SIZE_32BIT      0x00000000  
#define CRC_CFG_RESINV          0x00000200  
#define CRC_CFG_OBR             0x00000100  
#define CRC_CFG_IBR             0x00000080  
#define CRC_CFG_ENDIAN_SBHW     0x00000000  
#define CRC_CFG_ENDIAN_SHW      0x00000010  
#define CRC_CFG_TYPE_P8005      0x00000000  
#define CRC_CFG_TYPE_P1021      0x00000001  
#define CRC_CFG_TYPE_P4C11DB7   0x00000002  
#define CRC_CFG_TYPE_P1EDC6F41  0x00000003  
#define CRC_CFG_TYPE_TCPCHKSUM  0x00000008  






extern void CRCConfigSet(uint32_t ui32Base, uint32_t ui32CRCConfig);
extern uint32_t CRCDataProcess(uint32_t ui32Base, void *puiDataIn,
               uint32_t ui32DataLength, uint32_t ui32Config);
extern void CRCDataWrite(uint32_t ui32Base, uint32_t ui32Data);
extern uint32_t CRCResultRead(uint32_t ui32Base);
extern void CRCSeedSet(uint32_t ui32Base, uint32_t ui32Seed);







#ifdef __cplusplus
}
#endif

#endif 
