



































#ifndef __HW_APPS_CONFIG_H__
#define __HW_APPS_CONFIG_H__






#define APPS_CONFIG_O_PATCH_TRAP_ADDR_REG \
                                0x00000000  
                                            

#define APPS_CONFIG_O_PATCH_TRAP_EN_REG \
                                0x00000078

#define APPS_CONFIG_O_FAULT_STATUS_REG \
                                0x0000007C

#define APPS_CONFIG_O_MEMSS_WR_ERR_CLR_REG \
                                0x00000080

#define APPS_CONFIG_O_MEMSS_WR_ERR_ADDR_REG \
                                0x00000084

#define APPS_CONFIG_O_DMA_DONE_INT_MASK \
                                0x0000008C

#define APPS_CONFIG_O_DMA_DONE_INT_MASK_SET \
                                0x00000090

#define APPS_CONFIG_O_DMA_DONE_INT_MASK_CLR \
                                0x00000094

#define APPS_CONFIG_O_DMA_DONE_INT_STS_CLR \
                                0x00000098

#define APPS_CONFIG_O_DMA_DONE_INT_ACK \
                                0x0000009C

#define APPS_CONFIG_O_DMA_DONE_INT_STS_MASKED \
                                0x000000A0

#define APPS_CONFIG_O_DMA_DONE_INT_STS_RAW \
                                0x000000A4

#define APPS_CONFIG_O_FAULT_STATUS_CLR_REG \
                                0x000000A8

#define APPS_CONFIG_O_RESERVD_REG_0 \
                                0x000000AC

#define APPS_CONFIG_O_GPT_TRIG_SEL \
                                0x000000B0

#define APPS_CONFIG_O_TOP_DIE_SPARE_DIN_REG \
                                0x000000B4

#define APPS_CONFIG_O_TOP_DIE_SPARE_DOUT_REG \
                                0x000000B8










#define APPS_CONFIG_PATCH_TRAP_ADDR_REG_PATCH_TRAP_ADDR_M \
                                0xFFFFFFFF  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            

#define APPS_CONFIG_PATCH_TRAP_ADDR_REG_PATCH_TRAP_ADDR_S 0






#define APPS_CONFIG_PATCH_TRAP_EN_REG_PATCH_TRAP_EN_M \
                                0x3FFFFFFF  
                                            
                                            
                                            
                                            
                                            
                                            
                                            

#define APPS_CONFIG_PATCH_TRAP_EN_REG_PATCH_TRAP_EN_S 0






#define APPS_CONFIG_FAULT_STATUS_REG_PATCH_ERR_INDEX_M \
                                0x0000003E  
                                            
                                            
                                            
                                            
                                            
                                            

#define APPS_CONFIG_FAULT_STATUS_REG_PATCH_ERR_INDEX_S 1
#define APPS_CONFIG_FAULT_STATUS_REG_PATCH_ERR \
                                0x00000001  
                                            
                                            
                                            
                                            







#define APPS_CONFIG_MEMSS_WR_ERR_CLR_REG_MEMSS_WR_ERR_CLR \
                                0x00000001  
                                            
                                            
                                            
                                            
                                            
                                            













#define APPS_CONFIG_DMA_DONE_INT_MASK_ADC_WR_DMA_DONE_INT_MASK_M \
                                0x0000F000  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            

#define APPS_CONFIG_DMA_DONE_INT_MASK_ADC_WR_DMA_DONE_INT_MASK_S 12
#define APPS_CONFIG_DMA_DONE_INT_MASK_MCASP_WR_DMA_DONE_INT_MASK \
                                0x00000800  
                                            

#define APPS_CONFIG_DMA_DONE_INT_MASK_MCASP_RD_DMA_DONE_INT_MASK \
                                0x00000400  
                                            

#define APPS_CONFIG_DMA_DONE_INT_MASK_CAM_FIFO_EMPTY_DMA_DONE_INT_MASK \
                                0x00000200  
                                            

#define APPS_CONFIG_DMA_DONE_INT_MASK_CAM_THRESHHOLD_DMA_DONE_INT_MASK \
                                0x00000100  
                                            

#define APPS_CONFIG_DMA_DONE_INT_MASK_SHSPI_WR_DMA_DONE_INT_MASK \
                                0x00000080  
                                            

#define APPS_CONFIG_DMA_DONE_INT_MASK_SHSPI_RD_DMA_DONE_INT_MASK \
                                0x00000040  
                                            

#define APPS_CONFIG_DMA_DONE_INT_MASK_HOSTSPI_WR_DMA_DONE_INT_MASK \
                                0x00000020  
                                            

#define APPS_CONFIG_DMA_DONE_INT_MASK_HOSTSPI_RD_DMA_DONE_INT_MASK \
                                0x00000010  
                                            

#define APPS_CONFIG_DMA_DONE_INT_MASK_APPS_SPI_WR_DMA_DONE_INT_MASK \
                                0x00000008  
                                            

#define APPS_CONFIG_DMA_DONE_INT_MASK_APPS_SPI_RD_DMA_DONE_INT_MASK \
                                0x00000004  
                                            

#define APPS_CONFIG_DMA_DONE_INT_MASK_SDIOM_WR_DMA_DONE_INT_MASK \
                                0x00000002  
                                            

#define APPS_CONFIG_DMA_DONE_INT_MASK_SDIOM_RD_DMA_DONE_INT_MASK \
                                0x00000001  
                                            







#define APPS_CONFIG_DMA_DONE_INT_MASK_SET_ADC_WR_DMA_DONE_INT_MASK_SET_M \
                                0x0000F000  
                                            
                                            
                                            
                                            
                                            
                                            

#define APPS_CONFIG_DMA_DONE_INT_MASK_SET_ADC_WR_DMA_DONE_INT_MASK_SET_S 12
#define APPS_CONFIG_DMA_DONE_INT_MASK_SET_MCASP_WR_DMA_DONE_INT_MASK_SET \
                                0x00000800  
                                            
                                            

#define APPS_CONFIG_DMA_DONE_INT_MASK_SET_MCASP_RD_DMA_DONE_INT_MASK_SET \
                                0x00000400  
                                            
                                            

#define APPS_CONFIG_DMA_DONE_INT_MASK_SET_CAM_FIFO_EMPTY_DMA_DONE_INT_MASK_SET \
                                0x00000200  
                                            
                                            

#define APPS_CONFIG_DMA_DONE_INT_MASK_SET_CAM_THRESHHOLD_DMA_DONE_INT_MASK_SET \
                                0x00000100  
                                            
                                            

#define APPS_CONFIG_DMA_DONE_INT_MASK_SET_SHSPI_WR_DMA_DONE_INT_MASK_SET \
                                0x00000080  
                                            
                                            

#define APPS_CONFIG_DMA_DONE_INT_MASK_SET_SHSPI_RD_DMA_DONE_INT_MASK_SET \
                                0x00000040  
                                            
                                            

#define APPS_CONFIG_DMA_DONE_INT_MASK_SET_HOSTSPI_WR_DMA_DONE_INT_MASK_SET \
                                0x00000020  
                                            
                                            

#define APPS_CONFIG_DMA_DONE_INT_MASK_SET_HOSTSPI_RD_DMA_DONE_INT_MASK_SET \
                                0x00000010  
                                            
                                            

#define APPS_CONFIG_DMA_DONE_INT_MASK_SET_APPS_SPI_WR_DMA_DONE_INT_MASK_SET \
                                0x00000008  
                                            
                                            

#define APPS_CONFIG_DMA_DONE_INT_MASK_SET_APPS_SPI_RD_DMA_DONE_INT_MASK_SET \
                                0x00000004  
                                            
                                            

#define APPS_CONFIG_DMA_DONE_INT_MASK_SET_SDIOM_WR_DMA_DONE_INT_MASK_SET \
                                0x00000002  
                                            
                                            

#define APPS_CONFIG_DMA_DONE_INT_MASK_SET_SDIOM_RD_DMA_DONE_INT_MASK_SET \
                                0x00000001  
                                            
                                            







#define APPS_CONFIG_DMA_DONE_INT_MASK_CLR_ADC_WR_DMA_DONE_INT_MASK_CLR_M \
                                0x0000F000  
                                            
                                            
                                            
                                            
                                            
                                            
                                            

#define APPS_CONFIG_DMA_DONE_INT_MASK_CLR_ADC_WR_DMA_DONE_INT_MASK_CLR_S 12
#define APPS_CONFIG_DMA_DONE_INT_MASK_CLR_MACASP_WR_DMA_DONE_INT_MASK_CLR \
                                0x00000800  
                                            
                                            

#define APPS_CONFIG_DMA_DONE_INT_MASK_CLR_MCASP_RD_DMA_DONE_INT_MASK_CLR \
                                0x00000400  
                                            
                                            

#define APPS_CONFIG_DMA_DONE_INT_MASK_CLR_CAM_FIFO_EMPTY_DMA_DONE_INT_MASK_CLR \
                                0x00000200  
                                            
                                            

#define APPS_CONFIG_DMA_DONE_INT_MASK_CLR_CAM_THRESHHOLD_DMA_DONE_INT_MASK_CLR \
                                0x00000100  
                                            
                                            

#define APPS_CONFIG_DMA_DONE_INT_MASK_CLR_SHSPI_WR_DMA_DONE_INT_MASK_CLR \
                                0x00000080  
                                            
                                            

#define APPS_CONFIG_DMA_DONE_INT_MASK_CLR_SHSPI_RD_DMA_DONE_INT_MASK_CLR \
                                0x00000040  
                                            
                                            

#define APPS_CONFIG_DMA_DONE_INT_MASK_CLR_HOSTSPI_WR_DMA_DONE_INT_MASK_CLR \
                                0x00000020  
                                            
                                            

#define APPS_CONFIG_DMA_DONE_INT_MASK_CLR_HOSTSPI_RD_DMA_DONE_INT_MASK_CLR \
                                0x00000010  
                                            
                                            

#define APPS_CONFIG_DMA_DONE_INT_MASK_CLR_APPS_SPI_WR_DMA_DONE_INT_MASK_CLR \
                                0x00000008  
                                            
                                            

#define APPS_CONFIG_DMA_DONE_INT_MASK_CLR_APPS_SPI_RD_DMA_DONE_INT_MASK_CLR \
                                0x00000004  
                                            
                                            

#define APPS_CONFIG_DMA_DONE_INT_MASK_CLR_SDIOM_WR_DMA_DONE_INT_MASK_CLR \
                                0x00000002  
                                            
                                            

#define APPS_CONFIG_DMA_DONE_INT_MASK_CLR_SDIOM_RD_DMA_DONE_INT_MASK_CLR \
                                0x00000001  
                                            
                                            







#define APPS_CONFIG_DMA_DONE_INT_STS_CLR_DMA_INT_STS_CLR_M \
                                0xFFFFFFFF  
                                            

#define APPS_CONFIG_DMA_DONE_INT_STS_CLR_DMA_INT_STS_CLR_S 0






#define APPS_CONFIG_DMA_DONE_INT_ACK_ADC_WR_DMA_DONE_INT_ACK_M \
                                0x0000F000  
                                            
                                            
                                            
                                            
                                            
                                            

#define APPS_CONFIG_DMA_DONE_INT_ACK_ADC_WR_DMA_DONE_INT_ACK_S 12
#define APPS_CONFIG_DMA_DONE_INT_ACK_MCASP_WR_DMA_DONE_INT_ACK \
                                0x00000800  
                                            

#define APPS_CONFIG_DMA_DONE_INT_ACK_MCASP_RD_DMA_DONE_INT_ACK \
                                0x00000400  
                                            

#define APPS_CONFIG_DMA_DONE_INT_ACK_CAM_FIFO_EMPTY_DMA_DONE_INT_ACK \
                                0x00000200  
                                            

#define APPS_CONFIG_DMA_DONE_INT_ACK_CAM_THRESHHOLD_DMA_DONE_INT_ACK \
                                0x00000100  
                                            

#define APPS_CONFIG_DMA_DONE_INT_ACK_SHSPI_WR_DMA_DONE_INT_ACK \
                                0x00000080  
                                            

#define APPS_CONFIG_DMA_DONE_INT_ACK_SHSPI_RD_DMA_DONE_INT_ACK \
                                0x00000040  
                                            

#define APPS_CONFIG_DMA_DONE_INT_ACK_HOSTSPI_WR_DMA_DONE_INT_ACK \
                                0x00000020  
                                            

#define APPS_CONFIG_DMA_DONE_INT_ACK_HOSTSPI_RD_DMA_DONE_INT_ACK \
                                0x00000010  
                                            

#define APPS_CONFIG_DMA_DONE_INT_ACK_APPS_SPI_WR_DMA_DONE_INT_ACK \
                                0x00000008  
                                            

#define APPS_CONFIG_DMA_DONE_INT_ACK_APPS_SPI_RD_DMA_DONE_INT_ACK \
                                0x00000004  
                                            

#define APPS_CONFIG_DMA_DONE_INT_ACK_SDIOM_WR_DMA_DONE_INT_ACK \
                                0x00000002  
                                            

#define APPS_CONFIG_DMA_DONE_INT_ACK_SDIOM_RD_DMA_DONE_INT_ACK \
                                0x00000001  
                                            







#define APPS_CONFIG_DMA_DONE_INT_STS_MASKED_ADC_WR_DMA_DONE_INT_STS_MASKED_M \
                                0x0000F000  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            

#define APPS_CONFIG_DMA_DONE_INT_STS_MASKED_ADC_WR_DMA_DONE_INT_STS_MASKED_S 12
#define APPS_CONFIG_DMA_DONE_INT_STS_MASKED_MCASP_WR_DMA_DONE_INT_STS_MASKED \
                                0x00000800  
                                            
                                            
                                            
                                            

#define APPS_CONFIG_DMA_DONE_INT_STS_MASKED_MCASP_RD_DMA_DONE_INT_STS_MASKED \
                                0x00000400  
                                            
                                            
                                            
                                            

#define APPS_CONFIG_DMA_DONE_INT_STS_MASKED_CAM_FIFO_EMPTY_DMA_DONE_INT_STS_MASKED \
                                0x00000200  
                                            
                                            
                                            
                                            

#define APPS_CONFIG_DMA_DONE_INT_STS_MASKED_CAM_THRESHHOLD_DMA_DONE_INT_STS_MASKED \
                                0x00000100  
                                            
                                            
                                            
                                            

#define APPS_CONFIG_DMA_DONE_INT_STS_MASKED_SHSPI_WR_DMA_DONE_INT_STS_MASKED \
                                0x00000080  
                                            
                                            
                                            
                                            

#define APPS_CONFIG_DMA_DONE_INT_STS_MASKED_SHSPI_RD_DMA_DONE_INT_STS_MASKED \
                                0x00000040  
                                            
                                            
                                            
                                            

#define APPS_CONFIG_DMA_DONE_INT_STS_MASKED_HOSTSPI_WR_DMA_DONE_INT_STS_MASKED \
                                0x00000020  
                                            
                                            
                                            
                                            

#define APPS_CONFIG_DMA_DONE_INT_STS_MASKED_HOSTSPI_RD_DMA_DONE_INT_STS_MASKED \
                                0x00000010  
                                            
                                            
                                            
                                            

#define APPS_CONFIG_DMA_DONE_INT_STS_MASKED_APPS_SPI_WR_DMA_DONE_INT_STS_MASKED \
                                0x00000008  
                                            
                                            
                                            
                                            

#define APPS_CONFIG_DMA_DONE_INT_STS_MASKED_APPS_SPI_RD_DMA_DONE_INT_STS_MASKED \
                                0x00000004  
                                            
                                            
                                            
                                            

#define APPS_CONFIG_DMA_DONE_INT_STS_MASKED_SDIOM_WR_DMA_DONE_INT_STS_MASKED \
                                0x00000002  
                                            
                                            
                                            
                                            

#define APPS_CONFIG_DMA_DONE_INT_STS_MASKED_SDIOM_RD_DMA_DONE_INT_STS_MASKED \
                                0x00000001  
                                            
                                            
                                            
                                            







#define APPS_CONFIG_DMA_DONE_INT_STS_RAW_ADC_WR_DMA_DONE_INT_STS_RAW_M \
                                0x0000F000  
                                            
                                            
                                            
                                            
                                            
                                            
                                            

#define APPS_CONFIG_DMA_DONE_INT_STS_RAW_ADC_WR_DMA_DONE_INT_STS_RAW_S 12
#define APPS_CONFIG_DMA_DONE_INT_STS_RAW_MCASP_WR_DMA_DONE_INT_STS_RAW \
                                0x00000800  
                                            
                                            
                                            

#define APPS_CONFIG_DMA_DONE_INT_STS_RAW_MCASP_RD_DMA_DONE_INT_STS_RAW \
                                0x00000400  
                                            
                                            
                                            

#define APPS_CONFIG_DMA_DONE_INT_STS_RAW_CAM_EPMTY_FIFO_DMA_DONE_INT_STS_RAW \
                                0x00000200  
                                            
                                            
                                            

#define APPS_CONFIG_DMA_DONE_INT_STS_RAW_CAM_THRESHHOLD_DMA_DONE_INT_STS_RAW \
                                0x00000100  
                                            
                                            
                                            

#define APPS_CONFIG_DMA_DONE_INT_STS_RAW_SHSPI_WR_DMA_DONE_INT_STS_RAW \
                                0x00000080  
                                            
                                            
                                            

#define APPS_CONFIG_DMA_DONE_INT_STS_RAW_SHSPI_RD_DMA_DONE_INT_STS_RAW \
                                0x00000040  
                                            
                                            
                                            

#define APPS_CONFIG_DMA_DONE_INT_STS_RAW_HOSTSPI_WR_DMA_DONE_INT_STS_RAW \
                                0x00000020  
                                            
                                            
                                            

#define APPS_CONFIG_DMA_DONE_INT_STS_RAW_HOSTSPI_RD_DMA_DONE_INT_STS_RAW \
                                0x00000010  
                                            
                                            
                                            

#define APPS_CONFIG_DMA_DONE_INT_STS_RAW_APPS_SPI_WR_DMA_DONE_INT_STS_RAW \
                                0x00000008  
                                            
                                            
                                            

#define APPS_CONFIG_DMA_DONE_INT_STS_RAW_APPS_SPI_RD_DMA_DONE_INT_STS_RAW \
                                0x00000004  
                                            
                                            
                                            

#define APPS_CONFIG_DMA_DONE_INT_STS_RAW_SDIOM_WR_DMA_DONE_INT_STS_RAW \
                                0x00000002  
                                            
                                            
                                            

#define APPS_CONFIG_DMA_DONE_INT_STS_RAW_SDIOM_RD_DMA_DONE_INT_STS_RAW \
                                0x00000001  
                                            
                                            
                                            







#define APPS_CONFIG_FAULT_STATUS_CLR_REG_PATCH_ERR_CLR \
                                0x00000001  
                                            













#define APPS_CONFIG_GPT_TRIG_SEL_GPT_TRIG_SEL_M \
                                0x000000FF  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            

#define APPS_CONFIG_GPT_TRIG_SEL_GPT_TRIG_SEL_S 0






#define APPS_CONFIG_TOP_DIE_SPARE_DIN_REG_D2D_SPARE_DIN_M \
                                0x00000007  

#define APPS_CONFIG_TOP_DIE_SPARE_DIN_REG_D2D_SPARE_DIN_S 0






#define APPS_CONFIG_TOP_DIE_SPARE_DOUT_REG_D2D_SPARE_DOUT_M \
                                0x00000007  
                                            
                                            

#define APPS_CONFIG_TOP_DIE_SPARE_DOUT_REG_D2D_SPARE_DOUT_S 0



#endif 
