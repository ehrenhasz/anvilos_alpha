 
 
#ifndef _MEI_HW_MEI_REGS_H_
#define _MEI_HW_MEI_REGS_H_

 
#define MEI_DEV_ID_82946GZ    0x2974   
#define MEI_DEV_ID_82G35      0x2984   
#define MEI_DEV_ID_82Q965     0x2994   
#define MEI_DEV_ID_82G965     0x29A4   

#define MEI_DEV_ID_82GM965    0x2A04   
#define MEI_DEV_ID_82GME965   0x2A14   

#define MEI_DEV_ID_ICH9_82Q35 0x29B4   
#define MEI_DEV_ID_ICH9_82G33 0x29C4   
#define MEI_DEV_ID_ICH9_82Q33 0x29D4   
#define MEI_DEV_ID_ICH9_82X38 0x29E4   
#define MEI_DEV_ID_ICH9_3200  0x29F4   

#define MEI_DEV_ID_ICH9_6     0x28B4   
#define MEI_DEV_ID_ICH9_7     0x28C4   
#define MEI_DEV_ID_ICH9_8     0x28D4   
#define MEI_DEV_ID_ICH9_9     0x28E4   
#define MEI_DEV_ID_ICH9_10    0x28F4   

#define MEI_DEV_ID_ICH9M_1    0x2A44   
#define MEI_DEV_ID_ICH9M_2    0x2A54   
#define MEI_DEV_ID_ICH9M_3    0x2A64   
#define MEI_DEV_ID_ICH9M_4    0x2A74   

#define MEI_DEV_ID_ICH10_1    0x2E04   
#define MEI_DEV_ID_ICH10_2    0x2E14   
#define MEI_DEV_ID_ICH10_3    0x2E24   
#define MEI_DEV_ID_ICH10_4    0x2E34   

#define MEI_DEV_ID_IBXPK_1    0x3B64   
#define MEI_DEV_ID_IBXPK_2    0x3B65   

#define MEI_DEV_ID_CPT_1      0x1C3A   
#define MEI_DEV_ID_PBG_1      0x1D3A   

#define MEI_DEV_ID_PPT_1      0x1E3A   
#define MEI_DEV_ID_PPT_2      0x1CBA   
#define MEI_DEV_ID_PPT_3      0x1DBA   

#define MEI_DEV_ID_LPT_H      0x8C3A   
#define MEI_DEV_ID_LPT_W      0x8D3A   
#define MEI_DEV_ID_LPT_LP     0x9C3A   
#define MEI_DEV_ID_LPT_HR     0x8CBA   

#define MEI_DEV_ID_WPT_LP     0x9CBA   
#define MEI_DEV_ID_WPT_LP_2   0x9CBB   

#define MEI_DEV_ID_SPT        0x9D3A   
#define MEI_DEV_ID_SPT_2      0x9D3B   
#define MEI_DEV_ID_SPT_3      0x9D3E   
#define MEI_DEV_ID_SPT_H      0xA13A   
#define MEI_DEV_ID_SPT_H_2    0xA13B   

#define MEI_DEV_ID_LBG        0xA1BA   

#define MEI_DEV_ID_BXT_M      0x1A9A   
#define MEI_DEV_ID_APL_I      0x5A9A   

#define MEI_DEV_ID_DNV_IE     0x19E5   

#define MEI_DEV_ID_GLK        0x319A   

#define MEI_DEV_ID_KBP        0xA2BA   
#define MEI_DEV_ID_KBP_2      0xA2BB   
#define MEI_DEV_ID_KBP_3      0xA2BE   

#define MEI_DEV_ID_CNP_LP     0x9DE0   
#define MEI_DEV_ID_CNP_LP_3   0x9DE4   
#define MEI_DEV_ID_CNP_H      0xA360   
#define MEI_DEV_ID_CNP_H_3    0xA364   

#define MEI_DEV_ID_CMP_LP     0x02e0   
#define MEI_DEV_ID_CMP_LP_3   0x02e4   

#define MEI_DEV_ID_CMP_V      0xA3BA   

#define MEI_DEV_ID_CMP_H      0x06e0   
#define MEI_DEV_ID_CMP_H_3    0x06e4   

#define MEI_DEV_ID_CDF        0x18D3   

#define MEI_DEV_ID_ICP_LP     0x34E0   
#define MEI_DEV_ID_ICP_N      0x38E0   

#define MEI_DEV_ID_JSP_N      0x4DE0   

#define MEI_DEV_ID_TGP_LP     0xA0E0   
#define MEI_DEV_ID_TGP_H      0x43E0   

#define MEI_DEV_ID_MCC        0x4B70   
#define MEI_DEV_ID_MCC_4      0x4B75   

#define MEI_DEV_ID_EBG        0x1BE0   

#define MEI_DEV_ID_ADP_S      0x7AE8   
#define MEI_DEV_ID_ADP_LP     0x7A60   
#define MEI_DEV_ID_ADP_P      0x51E0   
#define MEI_DEV_ID_ADP_N      0x54E0   

#define MEI_DEV_ID_RPL_S      0x7A68   

#define MEI_DEV_ID_MTL_M      0x7E70   

 

 
#define PCI_CFG_HFS_1         0x40
#  define PCI_CFG_HFS_1_D0I3_MSK     0x80000000
#  define PCI_CFG_HFS_1_OPMODE_MSK 0xf0000  
#  define PCI_CFG_HFS_1_OPMODE_SPS 0xf0000  
#define PCI_CFG_HFS_2         0x48
#define PCI_CFG_HFS_3         0x60
#  define PCI_CFG_HFS_3_FW_SKU_MSK   0x00000070
#  define PCI_CFG_HFS_3_FW_SKU_IGN   0x00000000
#  define PCI_CFG_HFS_3_FW_SKU_SPS   0x00000060
#define PCI_CFG_HFS_4         0x64
#define PCI_CFG_HFS_5         0x68
#  define GSC_CFG_HFS_5_BOOT_TYPE_MSK      0x00000003
#  define GSC_CFG_HFS_5_BOOT_TYPE_PXP               3
#define PCI_CFG_HFS_6         0x6C

 
 
#define H_CB_WW    0
 
#define H_CSR      4
 
#define ME_CB_RW   8
 
#define ME_CSR_HA  0xC
 
#define H_HPG_CSR  0x10
 
#define H_D0I3C    0x800

#define H_GSC_EXT_OP_MEM_BASE_ADDR_LO_REG 0x100
#define H_GSC_EXT_OP_MEM_BASE_ADDR_HI_REG 0x104
#define H_GSC_EXT_OP_MEM_LIMIT_REG        0x108
#define GSC_EXT_OP_MEM_VALID              BIT(31)

 
 
#define H_CBD             0xFF000000
 
#define H_CBWP            0x00FF0000
 
#define H_CBRP            0x0000FF00
 
#define H_RST             0x00000010
 
#define H_RDY             0x00000008
 
#define H_IG              0x00000004
 
#define H_IS              0x00000002
 
#define H_IE              0x00000001
 
#define H_D0I3C_IE        0x00000020
 
#define H_D0I3C_IS        0x00000040

 
#define H_CSR_IE_MASK     (H_IE | H_D0I3C_IE)
#define H_CSR_IS_MASK     (H_IS | H_D0I3C_IS)

 
 
#define ME_CBD_HRA        0xFF000000
 
#define ME_CBWP_HRA       0x00FF0000
 
#define ME_CBRP_HRA       0x0000FF00
 
#define ME_PGIC_HRA       0x00000040
 
#define ME_RST_HRA        0x00000010
 
#define ME_RDY_HRA        0x00000008
 
#define ME_IG_HRA         0x00000004
 
#define ME_IS_HRA         0x00000002
 
#define ME_IE_HRA         0x00000001
 
#define ME_TRC            0x00000030

 
#define H_HPG_CSR_PGIHEXR 0x00000001
#define H_HPG_CSR_PGI     0x00000002

 
#define H_D0I3C_CIP      0x00000001
#define H_D0I3C_IR       0x00000002
#define H_D0I3C_I3       0x00000004
#define H_D0I3C_RR       0x00000008

#endif  
