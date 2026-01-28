






































#ifndef __DRIVERLIB_I2C_H__
#define __DRIVERLIB_I2C_H__







#ifdef __cplusplus
extern "C"
{
#endif












#define I2C_INT_MASTER          0x00000001
#define I2C_INT_SLAVE           0x00000002






#define I2C_MASTER_CMD_SINGLE_SEND                                            \
                                0x00000007
#define I2C_MASTER_CMD_SINGLE_RECEIVE                                         \
                                0x00000007
#define I2C_MASTER_CMD_BURST_SEND_START                                       \
                                0x00000003
#define I2C_MASTER_CMD_BURST_SEND_CONT                                        \
                                0x00000001
#define I2C_MASTER_CMD_BURST_SEND_FINISH                                      \
                                0x00000005
#define I2C_MASTER_CMD_BURST_SEND_STOP                                        \
                                0x00000004
#define I2C_MASTER_CMD_BURST_SEND_ERROR_STOP                                  \
                                0x00000004
#define I2C_MASTER_CMD_BURST_RECEIVE_START                                    \
                                0x0000000b
#define I2C_MASTER_CMD_BURST_RECEIVE_CONT                                     \
                                0x00000009
#define I2C_MASTER_CMD_BURST_RECEIVE_FINISH                                   \
                                0x00000005
#define I2C_MASTER_CMD_BURST_RECEIVE_ERROR_STOP                               \
                                0x00000004
#define I2C_MASTER_CMD_QUICK_COMMAND                                          \
                                0x00000027
#define I2C_MASTER_CMD_HS_MASTER_CODE_SEND                                    \
                                0x00000013
#define I2C_MASTER_CMD_FIFO_SINGLE_SEND                                       \
                                0x00000046
#define I2C_MASTER_CMD_FIFO_SINGLE_RECEIVE                                    \
                                0x00000046
#define I2C_MASTER_CMD_FIFO_BURST_SEND_START                                  \
                                0x00000042
#define I2C_MASTER_CMD_FIFO_BURST_SEND_CONT                                   \
                                0x00000040
#define I2C_MASTER_CMD_FIFO_BURST_SEND_FINISH                                 \
                                0x00000044
#define I2C_MASTER_CMD_FIFO_BURST_SEND_ERROR_STOP                             \
                                0x00000004
#define I2C_MASTER_CMD_FIFO_BURST_RECEIVE_START                               \
                                0x0000004a
#define I2C_MASTER_CMD_FIFO_BURST_RECEIVE_CONT                                \
                                0x00000048
#define I2C_MASTER_CMD_FIFO_BURST_RECEIVE_FINISH                              \
                                0x00000044
#define I2C_MASTER_CMD_FIFO_BURST_RECEIVE_ERROR_STOP                          \
                                0x00000004






#define I2C_MASTER_GLITCH_FILTER_DISABLED                                     \
                                0
#define I2C_MASTER_GLITCH_FILTER_1                                            \
                                0x00010000
#define I2C_MASTER_GLITCH_FILTER_2                                            \
                                0x00020000
#define I2C_MASTER_GLITCH_FILTER_3                                            \
                                0x00030000
#define I2C_MASTER_GLITCH_FILTER_4                                            \
                                0x00040000
#define I2C_MASTER_GLITCH_FILTER_8                                            \
                                0x00050000
#define I2C_MASTER_GLITCH_FILTER_16                                           \
                                0x00060000
#define I2C_MASTER_GLITCH_FILTER_32                                           \
                                0x00070000






#define I2C_MASTER_ERR_NONE     0
#define I2C_MASTER_ERR_ADDR_ACK 0x00000004
#define I2C_MASTER_ERR_DATA_ACK 0x00000008
#define I2C_MASTER_ERR_ARB_LOST 0x00000010
#define I2C_MASTER_ERR_CLK_TOUT 0x00000080






#define I2C_SLAVE_ACT_NONE      0
#define I2C_SLAVE_ACT_RREQ      0x00000001  
#define I2C_SLAVE_ACT_TREQ      0x00000002  
#define I2C_SLAVE_ACT_RREQ_FBR  0x00000005  
#define I2C_SLAVE_ACT_OWN2SEL   0x00000008  
#define I2C_SLAVE_ACT_QCMD      0x00000010  
#define I2C_SLAVE_ACT_QCMD_DATA 0x00000020  






#define I2C_MASTER_MAX_RETRIES  1000        






#define I2C_MASTER_INT_RX_FIFO_FULL                                           \
                                0x00000800  
#define I2C_MASTER_INT_TX_FIFO_EMPTY                                          \
                                0x00000400  
#define I2C_MASTER_INT_RX_FIFO_REQ                                            \
                                0x00000200  
#define I2C_MASTER_INT_TX_FIFO_REQ                                            \
                                0x00000100  
#define I2C_MASTER_INT_ARB_LOST                                               \
                                0x00000080  
#define I2C_MASTER_INT_STOP     0x00000040  
#define I2C_MASTER_INT_START    0x00000020  
#define I2C_MASTER_INT_NACK     0x00000010  
#define I2C_MASTER_INT_TX_DMA_DONE                                            \
                                0x00000008  
#define I2C_MASTER_INT_RX_DMA_DONE                                            \
                                0x00000004  
#define I2C_MASTER_INT_TIMEOUT  0x00000002  
#define I2C_MASTER_INT_DATA     0x00000001  






#define I2C_SLAVE_INT_RX_FIFO_FULL                                            \
                                0x00000100  
#define I2C_SLAVE_INT_TX_FIFO_EMPTY                                           \
                                0x00000080  
#define I2C_SLAVE_INT_RX_FIFO_REQ                                             \
                                0x00000040  
#define I2C_SLAVE_INT_TX_FIFO_REQ                                             \
                                0x00000020  
#define I2C_SLAVE_INT_TX_DMA_DONE                                             \
                                0x00000010  
#define I2C_SLAVE_INT_RX_DMA_DONE                                             \
                                0x00000008  
#define I2C_SLAVE_INT_STOP      0x00000004  
#define I2C_SLAVE_INT_START     0x00000002  
#define I2C_SLAVE_INT_DATA      0x00000001  






#define I2C_SLAVE_TX_FIFO_ENABLE                                              \
                                0x00000002
#define I2C_SLAVE_RX_FIFO_ENABLE                                              \
                                0x00000004






#define I2C_FIFO_CFG_TX_MASTER  0x00000000
#define I2C_FIFO_CFG_TX_SLAVE   0x00008000
#define I2C_FIFO_CFG_RX_MASTER  0x00000000
#define I2C_FIFO_CFG_RX_SLAVE   0x80000000
#define I2C_FIFO_CFG_TX_MASTER_DMA                                            \
                                0x00002000
#define I2C_FIFO_CFG_TX_SLAVE_DMA                                             \
                                0x0000a000
#define I2C_FIFO_CFG_RX_MASTER_DMA                                            \
                                0x20000000
#define I2C_FIFO_CFG_RX_SLAVE_DMA                                             \
                                0xa0000000
#define I2C_FIFO_CFG_TX_NO_TRIG 0x00000000
#define I2C_FIFO_CFG_TX_TRIG_1  0x00000001
#define I2C_FIFO_CFG_TX_TRIG_2  0x00000002
#define I2C_FIFO_CFG_TX_TRIG_3  0x00000003
#define I2C_FIFO_CFG_TX_TRIG_4  0x00000004
#define I2C_FIFO_CFG_TX_TRIG_5  0x00000005
#define I2C_FIFO_CFG_TX_TRIG_6  0x00000006
#define I2C_FIFO_CFG_TX_TRIG_7  0x00000007
#define I2C_FIFO_CFG_TX_TRIG_8  0x00000008
#define I2C_FIFO_CFG_RX_NO_TRIG 0x00000000
#define I2C_FIFO_CFG_RX_TRIG_1  0x00010000
#define I2C_FIFO_CFG_RX_TRIG_2  0x00020000
#define I2C_FIFO_CFG_RX_TRIG_3  0x00030000
#define I2C_FIFO_CFG_RX_TRIG_4  0x00040000
#define I2C_FIFO_CFG_RX_TRIG_5  0x00050000
#define I2C_FIFO_CFG_RX_TRIG_6  0x00060000
#define I2C_FIFO_CFG_RX_TRIG_7  0x00070000
#define I2C_FIFO_CFG_RX_TRIG_8  0x00080000






#define I2C_FIFO_RX_BELOW_TRIG_LEVEL                                          \
                                0x00040000
#define I2C_FIFO_RX_FULL        0x00020000
#define I2C_FIFO_RX_EMPTY       0x00010000
#define I2C_FIFO_TX_BELOW_TRIG_LEVEL                                          \
                                0x00000004
#define I2C_FIFO_TX_FULL        0x00000002
#define I2C_FIFO_TX_EMPTY       0x00000001






extern void I2CIntRegister(uint32_t ui32Base, void(pfnHandler)(void));
extern void I2CIntUnregister(uint32_t ui32Base);
extern void I2CTxFIFOConfigSet(uint32_t ui32Base, uint32_t ui32Config);
extern void I2CTxFIFOFlush(uint32_t ui32Base);
extern void I2CRxFIFOConfigSet(uint32_t ui32Base, uint32_t ui32Config);
extern void I2CRxFIFOFlush(uint32_t ui32Base);
extern uint32_t I2CFIFOStatus(uint32_t ui32Base);
extern void I2CFIFODataPut(uint32_t ui32Base, uint8_t ui8Data);
extern uint32_t I2CFIFODataPutNonBlocking(uint32_t ui32Base,
                                          uint8_t ui8Data);
extern uint32_t I2CFIFODataGet(uint32_t ui32Base);
extern uint32_t I2CFIFODataGetNonBlocking(uint32_t ui32Base,
                                          uint8_t *pui8Data);
extern void I2CMasterBurstLengthSet(uint32_t ui32Base,
                                    uint8_t ui8Length);
extern uint32_t I2CMasterBurstCountGet(uint32_t ui32Base);
extern void I2CMasterGlitchFilterConfigSet(uint32_t ui32Base,
                                           uint32_t ui32Config);
extern void I2CSlaveFIFOEnable(uint32_t ui32Base, uint32_t ui32Config);
extern void I2CSlaveFIFODisable(uint32_t ui32Base);
extern bool I2CMasterBusBusy(uint32_t ui32Base);
extern bool I2CMasterBusy(uint32_t ui32Base);
extern void I2CMasterControl(uint32_t ui32Base, uint32_t ui32Cmd);
extern uint32_t I2CMasterDataGet(uint32_t ui32Base);
extern void I2CMasterDataPut(uint32_t ui32Base, uint8_t ui8Data);
extern void I2CMasterDisable(uint32_t ui32Base);
extern void I2CMasterEnable(uint32_t ui32Base);
extern uint32_t I2CMasterErr(uint32_t ui32Base);
extern void I2CMasterInitExpClk(uint32_t ui32Base, uint32_t ui32SCLFreq);
extern void I2CMasterIntClear(uint32_t ui32Base);
extern void I2CMasterIntDisable(uint32_t ui32Base);
extern void I2CMasterIntEnable(uint32_t ui32Base);
extern bool I2CMasterIntStatus(uint32_t ui32Base, bool bMasked);
extern void I2CMasterIntEnableEx(uint32_t ui32Base,
                                 uint32_t ui32IntFlags);
extern void I2CMasterIntDisableEx(uint32_t ui32Base,
                                  uint32_t ui32IntFlags);
extern uint32_t I2CMasterIntStatusEx(uint32_t ui32Base,
                                       bool bMasked);
extern void I2CMasterIntClearEx(uint32_t ui32Base,
                                uint32_t ui32IntFlags);
extern void I2CMasterTimeoutSet(uint32_t ui32Base, uint32_t ui32Value);
extern void I2CSlaveACKOverride(uint32_t ui32Base, bool bEnable);
extern void I2CSlaveACKValueSet(uint32_t ui32Base, bool bACK);
extern uint32_t I2CMasterLineStateGet(uint32_t ui32Base);
extern void I2CMasterSlaveAddrSet(uint32_t ui32Base,
                                  uint8_t ui8SlaveAddr,
                                  bool bReceive);
extern uint32_t I2CSlaveDataGet(uint32_t ui32Base);
extern void I2CSlaveDataPut(uint32_t ui32Base, uint8_t ui8Data);
extern void I2CSlaveDisable(uint32_t ui32Base);
extern void I2CSlaveEnable(uint32_t ui32Base);
extern void I2CSlaveInit(uint32_t ui32Base, uint8_t ui8SlaveAddr);
extern void I2CSlaveAddressSet(uint32_t ui32Base, uint8_t ui8AddrNum,
                                 uint8_t ui8SlaveAddr);
extern void I2CSlaveIntClear(uint32_t ui32Base);
extern void I2CSlaveIntDisable(uint32_t ui32Base);
extern void I2CSlaveIntEnable(uint32_t ui32Base);
extern void I2CSlaveIntClearEx(uint32_t ui32Base, uint32_t ui32IntFlags);
extern void I2CSlaveIntDisableEx(uint32_t ui32Base,
                                 uint32_t ui32IntFlags);
extern void I2CSlaveIntEnableEx(uint32_t ui32Base, uint32_t ui32IntFlags);
extern bool I2CSlaveIntStatus(uint32_t ui32Base, bool bMasked);
extern uint32_t I2CSlaveIntStatusEx(uint32_t ui32Base,
                                      bool bMasked);
extern uint32_t I2CSlaveStatus(uint32_t ui32Base);






#ifdef __cplusplus
}
#endif

#endif 
