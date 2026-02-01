 
 

#ifndef _UFSHCI_DWC_H
#define _UFSHCI_DWC_H

 
enum dwc_specific_registers {
	DWC_UFS_REG_HCLKDIV	= 0xFC,
};

 
enum clk_div_values {
	DWC_UFS_REG_HCLKDIV_DIV_62_5	= 0x3e,
	DWC_UFS_REG_HCLKDIV_DIV_125	= 0x7d,
	DWC_UFS_REG_HCLKDIV_DIV_200	= 0xc8,
};

 
enum selector_index {
	SELIND_LN0_TX		= 0x00,
	SELIND_LN1_TX		= 0x01,
	SELIND_LN0_RX		= 0x04,
	SELIND_LN1_RX		= 0x05,
};

#endif  
