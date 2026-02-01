 

 

#ifndef ASIC_REG_DMA_MACRO_MASKS_H_
#define ASIC_REG_DMA_MACRO_MASKS_H_

 

 
#define DMA_MACRO_LBW_RANGE_HIT_BLOCK_R_SHIFT                        0
#define DMA_MACRO_LBW_RANGE_HIT_BLOCK_R_MASK                         0xFFFF

 
#define DMA_MACRO_LBW_RANGE_MASK_R_SHIFT                             0
#define DMA_MACRO_LBW_RANGE_MASK_R_MASK                              0x3FFFFFF

 
#define DMA_MACRO_LBW_RANGE_BASE_R_SHIFT                             0
#define DMA_MACRO_LBW_RANGE_BASE_R_MASK                              0x3FFFFFF

 
#define DMA_MACRO_HBW_RANGE_HIT_BLOCK_R_SHIFT                        0
#define DMA_MACRO_HBW_RANGE_HIT_BLOCK_R_MASK                         0xFF

 
#define DMA_MACRO_HBW_RANGE_MASK_49_32_R_SHIFT                       0
#define DMA_MACRO_HBW_RANGE_MASK_49_32_R_MASK                        0x3FFFF

 
#define DMA_MACRO_HBW_RANGE_MASK_31_0_R_SHIFT                        0
#define DMA_MACRO_HBW_RANGE_MASK_31_0_R_MASK                         0xFFFFFFFF

 
#define DMA_MACRO_HBW_RANGE_BASE_49_32_R_SHIFT                       0
#define DMA_MACRO_HBW_RANGE_BASE_49_32_R_MASK                        0x3FFFF

 
#define DMA_MACRO_HBW_RANGE_BASE_31_0_R_SHIFT                        0
#define DMA_MACRO_HBW_RANGE_BASE_31_0_R_MASK                         0xFFFFFFFF

 
#define DMA_MACRO_WRITE_EN_R_SHIFT                                   0
#define DMA_MACRO_WRITE_EN_R_MASK                                    0x1

 
#define DMA_MACRO_WRITE_CREDIT_R_SHIFT                               0
#define DMA_MACRO_WRITE_CREDIT_R_MASK                                0x3FF

 
#define DMA_MACRO_READ_EN_R_SHIFT                                    0
#define DMA_MACRO_READ_EN_R_MASK                                     0x1

 
#define DMA_MACRO_READ_CREDIT_R_SHIFT                                0
#define DMA_MACRO_READ_CREDIT_R_MASK                                 0x3FF

 

 
#define DMA_MACRO_RAZWI_LBW_WT_VLD_R_SHIFT                           0
#define DMA_MACRO_RAZWI_LBW_WT_VLD_R_MASK                            0x1

 
#define DMA_MACRO_RAZWI_LBW_WT_ID_R_SHIFT                            0
#define DMA_MACRO_RAZWI_LBW_WT_ID_R_MASK                             0x7FFF

 
#define DMA_MACRO_RAZWI_LBW_RD_VLD_R_SHIFT                           0
#define DMA_MACRO_RAZWI_LBW_RD_VLD_R_MASK                            0x1

 
#define DMA_MACRO_RAZWI_LBW_RD_ID_R_SHIFT                            0
#define DMA_MACRO_RAZWI_LBW_RD_ID_R_MASK                             0x7FFF

 
#define DMA_MACRO_RAZWI_HBW_WT_VLD_R_SHIFT                           0
#define DMA_MACRO_RAZWI_HBW_WT_VLD_R_MASK                            0x1

 
#define DMA_MACRO_RAZWI_HBW_WT_ID_R_SHIFT                            0
#define DMA_MACRO_RAZWI_HBW_WT_ID_R_MASK                             0x1FFFFFFF

 
#define DMA_MACRO_RAZWI_HBW_RD_VLD_R_SHIFT                           0
#define DMA_MACRO_RAZWI_HBW_RD_VLD_R_MASK                            0x1

 
#define DMA_MACRO_RAZWI_HBW_RD_ID_R_SHIFT                            0
#define DMA_MACRO_RAZWI_HBW_RD_ID_R_MASK                             0x1FFFFFFF

#endif  
