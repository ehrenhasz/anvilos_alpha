#ifdef __KERNEL__
#ifndef __POWERPC_FSL_PCI_H
#define __POWERPC_FSL_PCI_H
struct platform_device;
#define PCI_FSL_BRR1      0xbf8
#define PCI_FSL_BRR1_VER 0xffff
#define PCIE_LTSSM	0x0404		 
#define PCIE_LTSSM_L0	0x16		 
#define PCIE_FSL_CSR_CLASSCODE	0x474	 
#define PCIE_IP_REV_2_2		0x02080202  
#define PCIE_IP_REV_3_0		0x02080300  
#define PIWAR_EN		0x80000000	 
#define PIWAR_PF		0x20000000	 
#define PIWAR_TGI_LOCAL		0x00f00000	 
#define PIWAR_READ_SNOOP	0x00050000
#define PIWAR_WRITE_SNOOP	0x00005000
#define PIWAR_SZ_MASK          0x0000003f
#define PEX_PMCR_PTOMR		0x1
#define PEX_PMCR_EXL2S		0x2
#define PME_DISR_EN_PTOD	0x00008000
#define PME_DISR_EN_ENL23D	0x00002000
#define PME_DISR_EN_EXL23D	0x00001000
struct pci_outbound_window_regs {
	__be32	potar;	 
	__be32	potear;	 
	__be32	powbar;	 
	u8	res1[4];
	__be32	powar;	 
	u8	res2[12];
};
struct pci_inbound_window_regs {
	__be32	pitar;	 
	u8	res1[4];
	__be32	piwbar;	 
	__be32	piwbear;	 
	__be32	piwar;	 
	u8	res2[12];
};
struct ccsr_pci {
	__be32	config_addr;		 
	__be32	config_data;		 
	__be32	int_ack;		 
	__be32	pex_otb_cpl_tor;	 
	__be32	pex_conf_tor;		 
	__be32	pex_config;		 
	__be32	pex_int_status;		 
	u8	res2[4];
	__be32	pex_pme_mes_dr;		 
	__be32	pex_pme_mes_disr;	 
	__be32	pex_pme_mes_ier;	 
	__be32	pex_pmcr;		 
	u8	res3[3016];
	__be32	block_rev1;	 
	__be32	block_rev2;	 
	struct pci_outbound_window_regs pow[5];
	u8	res14[96];
	struct pci_inbound_window_regs	pmit;	 
	u8	res6[96];
	struct pci_inbound_window_regs piw[4];
	__be32	pex_err_dr;		 
	u8	res21[4];
	__be32	pex_err_en;		 
	u8	res22[4];
	__be32	pex_err_disr;		 
	u8	res23[12];
	__be32	pex_err_cap_stat;	 
	u8	res24[4];
	__be32	pex_err_cap_r0;		 
	__be32	pex_err_cap_r1;		 
	__be32	pex_err_cap_r2;		 
	__be32	pex_err_cap_r3;		 
	u8	res_e38[200];
	__be32	pdb_stat;		 
	u8	res_f04[16];
	__be32	pex_csr0;		 
#define PEX_CSR0_LTSSM_MASK	0xFC
#define PEX_CSR0_LTSSM_SHIFT	2
#define PEX_CSR0_LTSSM_L0	0x11
	__be32	pex_csr1;		 
	u8	res_f1c[228];
};
extern void fsl_pcibios_fixup_bus(struct pci_bus *bus);
extern void fsl_pcibios_fixup_phb(struct pci_controller *phb);
extern int mpc83xx_add_bridge(struct device_node *dev);
u64 fsl_pci_immrbar_base(struct pci_controller *hose);
extern struct device_node *fsl_pci_primary;
#ifdef CONFIG_PCI
void __init fsl_pci_assign_primary(void);
#else
static inline void fsl_pci_assign_primary(void) {}
#endif
#ifdef CONFIG_FSL_PCI
extern int fsl_pci_mcheck_exception(struct pt_regs *);
#else
static inline int fsl_pci_mcheck_exception(struct pt_regs *regs) {return 0; }
#endif
#endif  
#endif  
