


































#ifndef __HW_CAMERA_H__
#define __HW_CAMERA_H__






#define CAMERA_O_CC_REVISION    0x00000000  
                                            
#define CAMERA_O_CC_SYSCONFIG   0x00000010  
                                            
                                            
#define CAMERA_O_CC_SYSSTATUS   0x00000014  
                                            
                                            
                                            
                                            
#define CAMERA_O_CC_IRQSTATUS   0x00000018  
                                            
                                            
                                            
                                            
#define CAMERA_O_CC_IRQENABLE   0x0000001C  
                                            
                                            
                                            
                                            
#define CAMERA_O_CC_CTRL        0x00000040  
                                            
                                            
#define CAMERA_O_CC_CTRL_DMA    0x00000044  
                                            
                                            
#define CAMERA_O_CC_CTRL_XCLK   0x00000048  
                                            
                                            
                                            
#define CAMERA_O_CC_FIFO_DATA   0x0000004C  
                                            
                                            
#define CAMERA_O_CC_TEST        0x00000050  
                                            
                                            
                                            
#define CAMERA_O_CC_GEN_PAR     0x00000054  
                                            
                                            









#define CAMERA_CC_REVISION_REV_M \
                                0x000000FF  
                                            
                                            

#define CAMERA_CC_REVISION_REV_S 0






#define CAMERA_CC_SYSCONFIG_S_IDLE_MODE_M \
                                0x00000018  
                                            
                                            
                                            
                                            
                                            
                                            
                                            

#define CAMERA_CC_SYSCONFIG_S_IDLE_MODE_S 3
#define CAMERA_CC_SYSCONFIG_SOFT_RESET \
                                0x00000002  
                                            
                                            
                                            
                                            
                                            

#define CAMERA_CC_SYSCONFIG_AUTO_IDLE \
                                0x00000001  
                                            
                                            
                                            
                                            
                                            







#define CAMERA_CC_SYSSTATUS_RESET_DONE2 \
                                0x00000001  
                                            
                                            







#define CAMERA_CC_IRQSTATUS_FS_IRQ \
                                0x00080000  
                                            
                                            
                                            
                                            

#define CAMERA_CC_IRQSTATUS_LE_IRQ \
                                0x00040000  
                                            
                                            
                                            
                                            

#define CAMERA_CC_IRQSTATUS_LS_IRQ \
                                0x00020000  
                                            
                                            
                                            
                                            

#define CAMERA_CC_IRQSTATUS_FE_IRQ \
                                0x00010000  
                                            
                                            
                                            
                                            

#define CAMERA_CC_IRQSTATUS_FSP_ERR_IRQ \
                                0x00000800  
                                            
                                            
                                            

#define CAMERA_CC_IRQSTATUS_FW_ERR_IRQ \
                                0x00000400  
                                            
                                            
                                            

#define CAMERA_CC_IRQSTATUS_FSC_ERR_IRQ \
                                0x00000200  
                                            
                                            
                                            
                                            

#define CAMERA_CC_IRQSTATUS_SSC_ERR_IRQ \
                                0x00000100  
                                            
                                            
                                            
                                            

#define CAMERA_CC_IRQSTATUS_FIFO_NONEMPTY_IRQ \
                                0x00000010  
                                            
                                            
                                            

#define CAMERA_CC_IRQSTATUS_FIFO_FULL_IRQ \
                                0x00000008  
                                            
                                            
                                            

#define CAMERA_CC_IRQSTATUS_FIFO_THR_IRQ \
                                0x00000004  
                                            
                                            
                                            
                                            

#define CAMERA_CC_IRQSTATUS_FIFO_OF_IRQ \
                                0x00000002  
                                            
                                            
                                            
                                            

#define CAMERA_CC_IRQSTATUS_FIFO_UF_IRQ \
                                0x00000001  
                                            
                                            
                                            
                                            







#define CAMERA_CC_IRQENABLE_FS_IRQ_EN \
                                0x00080000  
                                            
                                            

#define CAMERA_CC_IRQENABLE_LE_IRQ_EN \
                                0x00040000  
                                            
                                            

#define CAMERA_CC_IRQENABLE_LS_IRQ_EN \
                                0x00020000  
                                            
                                            

#define CAMERA_CC_IRQENABLE_FE_IRQ_EN \
                                0x00010000  
                                            
                                            

#define CAMERA_CC_IRQENABLE_FSP_IRQ_EN \
                                0x00000800  
                                            
                                            

#define CAMERA_CC_IRQENABLE_FW_ERR_IRQ_EN \
                                0x00000400  
                                            
                                            
                                            

#define CAMERA_CC_IRQENABLE_FSC_ERR_IRQ_EN \
                                0x00000200  
                                            
                                            
                                            

#define CAMERA_CC_IRQENABLE_SSC_ERR_IRQ_EN \
                                0x00000100  
                                            
                                            
                                            

#define CAMERA_CC_IRQENABLE_FIFO_NONEMPTY_IRQ_EN \
                                0x00000010  
                                            
                                            
                                            

#define CAMERA_CC_IRQENABLE_FIFO_FULL_IRQ_EN \
                                0x00000008  
                                            
                                            
                                            

#define CAMERA_CC_IRQENABLE_FIFO_THR_IRQ_EN \
                                0x00000004  
                                            
                                            
                                            

#define CAMERA_CC_IRQENABLE_FIFO_OF_IRQ_EN \
                                0x00000002  
                                            
                                            

#define CAMERA_CC_IRQENABLE_FIFO_UF_IRQ_EN \
                                0x00000001  
                                            
                                            
                                            






#define CAMERA_CC_CTRL_CC_IF_SYNCHRO \
                                0x00080000  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            

#define CAMERA_CC_CTRL_CC_RST   0x00040000  
                                            
                                            
                                            
                                            
#define CAMERA_CC_CTRL_CC_FRAME_TRIG \
                                0x00020000  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            

#define CAMERA_CC_CTRL_CC_EN    0x00010000  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
#define CAMERA_CC_CTRL_NOBT_SYNCHRO \
                                0x00002000  
                                            
                                            
                                            
                                            
                                            
                                            
                                            

#define CAMERA_CC_CTRL_BT_CORRECT \
                                0x00001000  
                                            
                                            
                                            

#define CAMERA_CC_CTRL_PAR_ORDERCAM \
                                0x00000800  
                                            
                                            

#define CAMERA_CC_CTRL_PAR_CLK_POL \
                                0x00000400  
                                            
                                            
                                            
                                            

#define CAMERA_CC_CTRL_NOBT_HS_POL \
                                0x00000200  
                                            
                                            
                                            
                                            

#define CAMERA_CC_CTRL_NOBT_VS_POL \
                                0x00000100  
                                            
                                            
                                            
                                            

#define CAMERA_CC_CTRL_PAR_MODE_M \
                                0x0000000E  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            

#define CAMERA_CC_CTRL_PAR_MODE_S 1
#define CAMERA_CC_CTRL_CCP_MODE 0x00000001  
                                            
                                            






#define CAMERA_CC_CTRL_DMA_DMA_EN \
                                0x00000100  
                                            
                                            
                                            
                                            
                                            

#define CAMERA_CC_CTRL_DMA_FIFO_THRESHOLD_M \
                                0x0000007F  
                                            
                                            
                                            
                                            
                                            
                                            
                                            

#define CAMERA_CC_CTRL_DMA_FIFO_THRESHOLD_S 0






#define CAMERA_CC_CTRL_XCLK_XCLK_DIV_M \
                                0x0000001F  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            

#define CAMERA_CC_CTRL_XCLK_XCLK_DIV_S 0






#define CAMERA_CC_FIFO_DATA_FIFO_DATA_M \
                                0xFFFFFFFF  
                                            
                                            

#define CAMERA_CC_FIFO_DATA_FIFO_DATA_S 0





#define CAMERA_CC_TEST_FIFO_RD_POINTER_M \
                                0xFF000000  
                                            
                                            
                                            

#define CAMERA_CC_TEST_FIFO_RD_POINTER_S 24
#define CAMERA_CC_TEST_FIFO_WR_POINTER_M \
                                0x00FF0000  
                                            
                                            
                                            

#define CAMERA_CC_TEST_FIFO_WR_POINTER_S 16
#define CAMERA_CC_TEST_FIFO_LEVEL_M \
                                0x0000FF00  
                                            
                                            
                                            
                                            

#define CAMERA_CC_TEST_FIFO_LEVEL_S 8
#define CAMERA_CC_TEST_FIFO_LEVEL_PEAK_M \
                                0x000000FF  
                                            
                                            
                                            

#define CAMERA_CC_TEST_FIFO_LEVEL_PEAK_S 0






#define CAMERA_CC_GEN_PAR_CC_FIFO_DEPTH_M \
                                0x00000007  
                                            

#define CAMERA_CC_GEN_PAR_CC_FIFO_DEPTH_S 0



#endif 
