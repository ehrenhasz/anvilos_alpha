#ifndef __Q6PRM_H__
#define __Q6PRM_H__
#define Q6PRM_LPASS_CLK_ID_PRI_MI2S_IBIT                          0x100
#define Q6PRM_LPASS_CLK_ID_PRI_MI2S_EBIT                          0x101
#define Q6PRM_LPASS_CLK_ID_SEC_MI2S_IBIT                          0x102
#define Q6PRM_LPASS_CLK_ID_SEC_MI2S_EBIT                          0x103
#define Q6PRM_LPASS_CLK_ID_TER_MI2S_IBIT                          0x104
#define Q6PRM_LPASS_CLK_ID_TER_MI2S_EBIT                          0x105
#define Q6PRM_LPASS_CLK_ID_QUAD_MI2S_IBIT                         0x106
#define Q6PRM_LPASS_CLK_ID_QUAD_MI2S_EBIT                         0x107
#define Q6PRM_LPASS_CLK_ID_SPEAKER_I2S_IBIT                       0x108
#define Q6PRM_LPASS_CLK_ID_SPEAKER_I2S_EBIT                       0x109
#define Q6PRM_LPASS_CLK_ID_SPEAKER_I2S_OSR                        0x10A
#define Q6PRM_LPASS_CLK_ID_QUI_MI2S_IBIT			0x10B
#define Q6PRM_LPASS_CLK_ID_QUI_MI2S_EBIT			0x10C
#define Q6PRM_LPASS_CLK_ID_SEN_MI2S_IBIT			0x10D
#define Q6PRM_LPASS_CLK_ID_SEN_MI2S_EBIT			0x10E
#define Q6PRM_LPASS_CLK_ID_INT0_MI2S_IBIT                       0x10F
#define Q6PRM_LPASS_CLK_ID_INT1_MI2S_IBIT                       0x110
#define Q6PRM_LPASS_CLK_ID_INT2_MI2S_IBIT                       0x111
#define Q6PRM_LPASS_CLK_ID_INT3_MI2S_IBIT                       0x112
#define Q6PRM_LPASS_CLK_ID_INT4_MI2S_IBIT                       0x113
#define Q6PRM_LPASS_CLK_ID_INT5_MI2S_IBIT                       0x114
#define Q6PRM_LPASS_CLK_ID_INT6_MI2S_IBIT                       0x115
#define Q6PRM_LPASS_CLK_ID_QUI_MI2S_OSR                         0x116
#define Q6PRM_LPASS_CLK_ID_WSA_CORE_MCLK			0x305
#define Q6PRM_LPASS_CLK_ID_WSA_CORE_NPL_MCLK			0x306
#define Q6PRM_LPASS_CLK_ID_VA_CORE_MCLK				0x307
#define Q6PRM_LPASS_CLK_ID_VA_CORE_2X_MCLK			0x308
#define Q6PRM_LPASS_CLK_ID_TX_CORE_MCLK				0x30c
#define Q6PRM_LPASS_CLK_ID_TX_CORE_NPL_MCLK			0x30d
#define Q6PRM_LPASS_CLK_ID_RX_CORE_MCLK				0x30e
#define Q6PRM_LPASS_CLK_ID_RX_CORE_NPL_MCLK			0x30f
#define Q6PRM_LPASS_CLK_ID_WSA2_CORE_MCLK 0x310
#define Q6PRM_LPASS_CLK_ID_WSA2_CORE_2X_MCLK 0x311
#define Q6PRM_LPASS_CLK_ID_RX_CORE_TX_MCLK 0x312
#define Q6PRM_LPASS_CLK_ID_RX_CORE_TX_2X_MCLK 0x313
#define Q6PRM_LPASS_CLK_ID_WSA_CORE_TX_MCLK 0x314
#define Q6PRM_LPASS_CLK_ID_WSA_CORE_TX_2X_MCLK 0x315
#define Q6PRM_LPASS_CLK_ID_WSA2_CORE_TX_MCLK 0x316
#define Q6PRM_LPASS_CLK_ID_WSA2_CORE_TX_2X_MCLK 0x317
#define Q6PRM_LPASS_CLK_ID_RX_CORE_MCLK2_2X_MCLK 0x318
#define Q6PRM_LPASS_CLK_SRC_INTERNAL	1
#define Q6PRM_LPASS_CLK_ROOT_DEFAULT	0
#define Q6PRM_HW_CORE_ID_LPASS		1
#define Q6PRM_HW_CORE_ID_DCODEC		2
int q6prm_set_lpass_clock(struct device *dev, int clk_id, int clk_attr,
			  int clk_root, unsigned int freq);
int q6prm_vote_lpass_core_hw(struct device *dev, uint32_t hw_block_id,
			     const char *client_name, uint32_t *client_handle);
int q6prm_unvote_lpass_core_hw(struct device *dev, uint32_t hw_block_id,
			       uint32_t client_handle);
#endif  
