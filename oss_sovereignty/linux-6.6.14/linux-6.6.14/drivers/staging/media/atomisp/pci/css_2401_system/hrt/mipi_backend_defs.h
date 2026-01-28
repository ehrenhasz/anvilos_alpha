#ifndef _mipi_backend_defs_h
#define _mipi_backend_defs_h
#include "mipi_backend_common_defs.h"
#define MIPI_BACKEND_REG_ALIGN                    4  
#define _HRT_MIPI_BACKEND_NOF_IRQS                         3  
#define _HRT_MIPI_BACKEND_ENABLE_REG_IDX                   0
#define _HRT_MIPI_BACKEND_STATUS_REG_IDX                   1
#define _HRT_MIPI_BACKEND_COMP_FORMAT_REG0_IDX             2
#define _HRT_MIPI_BACKEND_COMP_FORMAT_REG1_IDX             3
#define _HRT_MIPI_BACKEND_COMP_FORMAT_REG2_IDX             4
#define _HRT_MIPI_BACKEND_COMP_FORMAT_REG3_IDX             5
#define _HRT_MIPI_BACKEND_RAW16_CONFIG_REG_IDX             6
#define _HRT_MIPI_BACKEND_RAW18_CONFIG_REG_IDX             7
#define _HRT_MIPI_BACKEND_FORCE_RAW8_REG_IDX               8
#define _HRT_MIPI_BACKEND_IRQ_STATUS_REG_IDX               9
#define _HRT_MIPI_BACKEND_IRQ_CLEAR_REG_IDX               10
#define _HRT_MIPI_BACKEND_CUST_EN_REG_IDX                 11
#define _HRT_MIPI_BACKEND_CUST_DATA_STATE_REG_IDX         12
#define _HRT_MIPI_BACKEND_CUST_PIX_EXT_S0P0_REG_IDX       13
#define _HRT_MIPI_BACKEND_CUST_PIX_EXT_S0P1_REG_IDX       14
#define _HRT_MIPI_BACKEND_CUST_PIX_EXT_S0P2_REG_IDX       15
#define _HRT_MIPI_BACKEND_CUST_PIX_EXT_S0P3_REG_IDX       16
#define _HRT_MIPI_BACKEND_CUST_PIX_EXT_S1P0_REG_IDX       17
#define _HRT_MIPI_BACKEND_CUST_PIX_EXT_S1P1_REG_IDX       18
#define _HRT_MIPI_BACKEND_CUST_PIX_EXT_S1P2_REG_IDX       19
#define _HRT_MIPI_BACKEND_CUST_PIX_EXT_S1P3_REG_IDX       20
#define _HRT_MIPI_BACKEND_CUST_PIX_EXT_S2P0_REG_IDX       21
#define _HRT_MIPI_BACKEND_CUST_PIX_EXT_S2P1_REG_IDX       22
#define _HRT_MIPI_BACKEND_CUST_PIX_EXT_S2P2_REG_IDX       23
#define _HRT_MIPI_BACKEND_CUST_PIX_EXT_S2P3_REG_IDX       24
#define _HRT_MIPI_BACKEND_CUST_PIX_VALID_EOP_REG_IDX      25
#define _HRT_MIPI_BACKEND_GLOBAL_LUT_DISREGARD_REG_IDX    26
#define _HRT_MIPI_BACKEND_PKT_STALL_STATUS_REG_IDX        27
#define _HRT_MIPI_BACKEND_SP_LUT_ENTRY_0_REG_IDX          28
#define _HRT_MIPI_BACKEND_SP_LUT_ENTRY_1_REG_IDX          29
#define _HRT_MIPI_BACKEND_SP_LUT_ENTRY_2_REG_IDX          30
#define _HRT_MIPI_BACKEND_SP_LUT_ENTRY_3_REG_IDX          31
#define _HRT_MIPI_BACKEND_NOF_REGISTERS                   32  
#define _HRT_MIPI_BACKEND_LP_LUT_ENTRY_0_REG_IDX          32
#define _HRT_MIPI_BACKEND_ENABLE_REG_WIDTH                 1
#define _HRT_MIPI_BACKEND_STATUS_REG_WIDTH                 1
#define _HRT_MIPI_BACKEND_COMP_FORMAT_REG_WIDTH           32
#define _HRT_MIPI_BACKEND_RAW16_CONFIG_REG_WIDTH           7
#define _HRT_MIPI_BACKEND_RAW18_CONFIG_REG_WIDTH           9
#define _HRT_MIPI_BACKEND_FORCE_RAW8_REG_WIDTH             8
#define _HRT_MIPI_BACKEND_IRQ_STATUS_REG_WIDTH            _HRT_MIPI_BACKEND_NOF_IRQS
#define _HRT_MIPI_BACKEND_IRQ_CLEAR_REG_WIDTH              0
#define _HRT_MIPI_BACKEND_GLOBAL_LUT_DISREGARD_REG_WIDTH   1
#define _HRT_MIPI_BACKEND_PKT_STALL_STATUS_REG_WIDTH       1 + 2 + 6
#define _HRT_MIPI_BACKEND_NOF_SP_LUT_ENTRIES               4
#define _HRT_MIPI_BACKEND_CHANNEL_ID_WIDTH                 2
#define _HRT_MIPI_BACKEND_FORMAT_TYPE_WIDTH                6
#define _HRT_MIPI_BACKEND_PACKET_ID_WIDTH                  _HRT_MIPI_BACKEND_CHANNEL_ID_WIDTH + _HRT_MIPI_BACKEND_FORMAT_TYPE_WIDTH
#define _HRT_MIPI_BACKEND_STREAMING_PIX_A_LSB                 0
#define _HRT_MIPI_BACKEND_STREAMING_PIX_A_MSB(pix_width)     (_HRT_MIPI_BACKEND_STREAMING_PIX_A_LSB + (pix_width) - 1)
#define _HRT_MIPI_BACKEND_STREAMING_PIX_A_VAL_BIT(pix_width) (_HRT_MIPI_BACKEND_STREAMING_PIX_A_MSB(pix_width) + 1)
#define _HRT_MIPI_BACKEND_STREAMING_PIX_B_LSB(pix_width)     (_HRT_MIPI_BACKEND_STREAMING_PIX_A_VAL_BIT(pix_width) + 1)
#define _HRT_MIPI_BACKEND_STREAMING_PIX_B_MSB(pix_width)     (_HRT_MIPI_BACKEND_STREAMING_PIX_B_LSB(pix_width) + (pix_width) - 1)
#define _HRT_MIPI_BACKEND_STREAMING_PIX_B_VAL_BIT(pix_width) (_HRT_MIPI_BACKEND_STREAMING_PIX_B_MSB(pix_width) + 1)
#define _HRT_MIPI_BACKEND_STREAMING_SOP_BIT(pix_width)       (_HRT_MIPI_BACKEND_STREAMING_PIX_B_VAL_BIT(pix_width) + 1)
#define _HRT_MIPI_BACKEND_STREAMING_EOP_BIT(pix_width)       (_HRT_MIPI_BACKEND_STREAMING_SOP_BIT(pix_width) + 1)
#define _HRT_MIPI_BACKEND_STREAMING_WIDTH(pix_width)         (_HRT_MIPI_BACKEND_STREAMING_EOP_BIT(pix_width) + 1)
#define _HRT_MIPI_BACKEND_CUST_EN_IDX                     0      
#define _HRT_MIPI_BACKEND_CUST_EN_DATAID_IDX              2      
#define _HRT_MIPI_BACKEND_CUST_EN_HIGH_PREC_IDX           8      
#define _HRT_MIPI_BACKEND_CUST_EN_WIDTH                   9
#define _HRT_MIPI_BACKEND_CUST_MODE_ALL                   1      
#define _HRT_MIPI_BACKEND_CUST_MODE_ONE                   3      
#define _HRT_MIPI_BACKEND_CUST_EN_OPTION_IDX              1
#define _HRT_MIPI_BACKEND_CUST_DATA_STATE_S0_IDX          0      
#define _HRT_MIPI_BACKEND_CUST_DATA_STATE_S1_IDX          8      
#define _HRT_MIPI_BACKEND_CUST_DATA_STATE_S2_IDX          16     
#define _HRT_MIPI_BACKEND_CUST_DATA_STATE_WIDTH           24     
#define _HRT_MIPI_BACKEND_CUST_DATA_STATE_VALID_IDX       0      
#define _HRT_MIPI_BACKEND_CUST_DATA_STATE_GETBITS_IDX     1      
#define _HRT_MIPI_BACKEND_CUST_PIX_EXT_DATA_ALIGN_IDX     0      
#define _HRT_MIPI_BACKEND_CUST_PIX_EXT_PIX_ALIGN_IDX      6      
#define _HRT_MIPI_BACKEND_CUST_PIX_EXT_PIX_MASK_IDX       11     
#define _HRT_MIPI_BACKEND_CUST_PIX_EXT_PIX_EN_IDX         29     
#define _HRT_MIPI_BACKEND_CUST_PIX_EXT_WIDTH              30     
#define _HRT_MIPI_BACKEND_CUST_PIX_VALID_EOP_P0_IDX        0     
#define _HRT_MIPI_BACKEND_CUST_PIX_VALID_EOP_P1_IDX        4     
#define _HRT_MIPI_BACKEND_CUST_PIX_VALID_EOP_P2_IDX        8     
#define _HRT_MIPI_BACKEND_CUST_PIX_VALID_EOP_P3_IDX        12    
#define _HRT_MIPI_BACKEND_CUST_PIX_VALID_EOP_WIDTH         16
#define _HRT_MIPI_BACKEND_CUST_PIX_VALID_EOP_NOR_VALID_IDX 0     
#define _HRT_MIPI_BACKEND_CUST_PIX_VALID_EOP_NOR_EOP_IDX   1     
#define _HRT_MIPI_BACKEND_CUST_PIX_VALID_EOP_ESP_VALID_IDX 2     
#define _HRT_MIPI_BACKEND_CUST_PIX_VALID_EOP_ESP_EOP_IDX   3     
#define HRT_MIPI_BACKEND_STREAM_EOP_BIT                      0
#define HRT_MIPI_BACKEND_STREAM_SOP_BIT                      1
#define HRT_MIPI_BACKEND_STREAM_EOF_BIT                      2
#define HRT_MIPI_BACKEND_STREAM_SOF_BIT                      3
#define HRT_MIPI_BACKEND_STREAM_CHID_LS_BIT                  4
#define HRT_MIPI_BACKEND_STREAM_CHID_MS_BIT(sid_width)      (HRT_MIPI_BACKEND_STREAM_CHID_LS_BIT + (sid_width) - 1)
#define HRT_MIPI_BACKEND_STREAM_PIX_VAL_BIT(sid_width, p)    (HRT_MIPI_BACKEND_STREAM_CHID_MS_BIT(sid_width) + 1 + p)
#define HRT_MIPI_BACKEND_STREAM_PIX_LS_BIT(sid_width, ppc, pix_width, p) (HRT_MIPI_BACKEND_STREAM_PIX_VAL_BIT(sid_width, ppc) + ((pix_width) * p))
#define HRT_MIPI_BACKEND_STREAM_PIX_MS_BIT(sid_width, ppc, pix_width, p) (HRT_MIPI_BACKEND_STREAM_PIX_LS_BIT(sid_width, ppc, pix_width, p) + (pix_width) - 1)
#if 0
#endif
#define HRT_MIPI_BACKEND_STREAM_BITS(sid_width, ppc, pix_width)         (HRT_MIPI_BACKEND_STREAM_PIX_MS_BIT(sid_width, ppc, pix_width, (ppc - 1)) + 1)
#define HRT_MIPI_BACKEND_LUT_PKT_DISREGARD_BIT              0                                                                                            
#define HRT_MIPI_BACKEND_LUT_SID_LS_BIT                     HRT_MIPI_BACKEND_LUT_PKT_DISREGARD_BIT + 1                                                   
#define HRT_MIPI_BACKEND_LUT_SID_MS_BIT(sid_width)          (HRT_MIPI_BACKEND_LUT_SID_LS_BIT + (sid_width) - 1)                                              
#define HRT_MIPI_BACKEND_LUT_MIPI_CH_ID_LS_BIT(sid_width)   HRT_MIPI_BACKEND_LUT_SID_MS_BIT(sid_width) + 1                                               
#define HRT_MIPI_BACKEND_LUT_MIPI_CH_ID_MS_BIT(sid_width)   HRT_MIPI_BACKEND_LUT_MIPI_CH_ID_LS_BIT(sid_width) + _HRT_MIPI_BACKEND_CHANNEL_ID_WIDTH - 1   
#define HRT_MIPI_BACKEND_LUT_MIPI_FMT_LS_BIT(sid_width)     HRT_MIPI_BACKEND_LUT_MIPI_CH_ID_MS_BIT(sid_width) + 1                                        
#define HRT_MIPI_BACKEND_LUT_MIPI_FMT_MS_BIT(sid_width)     HRT_MIPI_BACKEND_LUT_MIPI_FMT_LS_BIT(sid_width) + _HRT_MIPI_BACKEND_FORMAT_TYPE_WIDTH - 1    
#define HRT_MIPI_BACKEND_SP_LUT_BITS(sid_width)             HRT_MIPI_BACKEND_LUT_SID_MS_BIT(sid_width) + 1
#define HRT_MIPI_BACKEND_LP_LUT_BITS(sid_width)             HRT_MIPI_BACKEND_LUT_MIPI_FMT_MS_BIT(sid_width) + 1                                          
#define HRT_MIPI_BACKEND_STREAM_VC_LS_BIT(sid_width, ppc, pix_width)  HRT_MIPI_BACKEND_STREAM_PIX_LS_BIT(sid_width, ppc, pix_width, 1) + 10   
#define HRT_MIPI_BACKEND_STREAM_VC_MS_BIT(sid_width, ppc, pix_width)  HRT_MIPI_BACKEND_STREAM_VC_LS_BIT(sid_width, ppc, pix_width) + 1     
#endif  
