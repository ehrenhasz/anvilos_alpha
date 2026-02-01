
 

#include <linux/pci.h>
#include "pci-bridge-emul.h"

#define PCI_BRIDGE_CONF_END	PCI_STD_HEADER_SIZEOF
#define PCI_CAP_SSID_SIZEOF	(PCI_SSVID_DEVICE_ID + 2)
#define PCI_CAP_PCIE_SIZEOF	(PCI_EXP_SLTSTA2 + 2)

 
struct pci_bridge_reg_behavior {
	 
	u32 ro;

	 
	u32 rw;

	 
	u32 w1c;
};

static const
struct pci_bridge_reg_behavior pci_regs_behavior[PCI_STD_HEADER_SIZEOF / 4] = {
	[PCI_VENDOR_ID / 4] = { .ro = ~0 },
	[PCI_COMMAND / 4] = {
		.rw = (PCI_COMMAND_IO | PCI_COMMAND_MEMORY |
		       PCI_COMMAND_MASTER | PCI_COMMAND_PARITY |
		       PCI_COMMAND_SERR),
		.ro = ((PCI_COMMAND_SPECIAL | PCI_COMMAND_INVALIDATE |
			PCI_COMMAND_VGA_PALETTE | PCI_COMMAND_WAIT |
			PCI_COMMAND_FAST_BACK) |
		       (PCI_STATUS_CAP_LIST | PCI_STATUS_66MHZ |
			PCI_STATUS_FAST_BACK | PCI_STATUS_DEVSEL_MASK) << 16),
		.w1c = PCI_STATUS_ERROR_BITS << 16,
	},
	[PCI_CLASS_REVISION / 4] = { .ro = ~0 },

	 
	[PCI_CACHE_LINE_SIZE / 4] = { .ro = ~0 },

	 
	[PCI_BASE_ADDRESS_0 / 4] = { .ro = ~0 },
	[PCI_BASE_ADDRESS_1 / 4] = { .ro = ~0 },

	[PCI_PRIMARY_BUS / 4] = {
		 
		.rw = GENMASK(24, 0),
		 
		.ro = GENMASK(31, 24),
	},

	[PCI_IO_BASE / 4] = {
		 
		.rw = (GENMASK(15, 12) | GENMASK(7, 4)),

		 
		.ro = (((PCI_STATUS_66MHZ | PCI_STATUS_FAST_BACK |
			 PCI_STATUS_DEVSEL_MASK) << 16) |
		       GENMASK(11, 8) | GENMASK(3, 0)),

		.w1c = PCI_STATUS_ERROR_BITS << 16,
	},

	[PCI_MEMORY_BASE / 4] = {
		 
		.rw = GENMASK(31, 20) | GENMASK(15, 4),

		 
		.ro = GENMASK(19, 16) | GENMASK(3, 0),
	},

	[PCI_PREF_MEMORY_BASE / 4] = {
		 
		.rw = GENMASK(31, 20) | GENMASK(15, 4),

		 
		.ro = GENMASK(19, 16) | GENMASK(3, 0),
	},

	[PCI_PREF_BASE_UPPER32 / 4] = {
		.rw = ~0,
	},

	[PCI_PREF_LIMIT_UPPER32 / 4] = {
		.rw = ~0,
	},

	[PCI_IO_BASE_UPPER16 / 4] = {
		.rw = ~0,
	},

	[PCI_CAPABILITY_LIST / 4] = {
		.ro = GENMASK(7, 0),
	},

	 
	[PCI_ROM_ADDRESS1 / 4] = {
		.ro = ~0,
	},

	 
	[PCI_INTERRUPT_LINE / 4] = {
		 
		.rw = (GENMASK(7, 0) |
		       ((PCI_BRIDGE_CTL_PARITY |
			 PCI_BRIDGE_CTL_SERR |
			 PCI_BRIDGE_CTL_ISA |
			 PCI_BRIDGE_CTL_VGA |
			 PCI_BRIDGE_CTL_MASTER_ABORT |
			 PCI_BRIDGE_CTL_BUS_RESET |
			 BIT(8) | BIT(9) | BIT(11)) << 16)),

		 
		.ro = (GENMASK(15, 8) | ((PCI_BRIDGE_CTL_FAST_BACK) << 16)),

		.w1c = BIT(10) << 16,
	},
};

static const
struct pci_bridge_reg_behavior pcie_cap_regs_behavior[PCI_CAP_PCIE_SIZEOF / 4] = {
	[PCI_CAP_LIST_ID / 4] = {
		 
		.ro = GENMASK(30, 0),
	},

	[PCI_EXP_DEVCAP / 4] = {
		 
		.ro = BIT(15) | GENMASK(5, 0),
	},

	[PCI_EXP_DEVCTL / 4] = {
		 
		.rw = GENMASK(14, 0),

		 
		.w1c = GENMASK(3, 0) << 16,
		.ro = GENMASK(5, 4) << 16,
	},

	[PCI_EXP_LNKCAP / 4] = {
		 
		.ro = lower_32_bits(~(BIT(23) | PCI_EXP_LNKCAP_CLKPM)),
	},

	[PCI_EXP_LNKCTL / 4] = {
		 
		.rw = GENMASK(15, 14) | GENMASK(11, 9) | GENMASK(7, 3) | GENMASK(1, 0),
		.ro = GENMASK(13, 0) << 16,
		.w1c = GENMASK(15, 14) << 16,
	},

	[PCI_EXP_SLTCAP / 4] = {
		.ro = ~0,
	},

	[PCI_EXP_SLTCTL / 4] = {
		 
		.rw = GENMASK(14, 0),
		.w1c = (PCI_EXP_SLTSTA_ABP | PCI_EXP_SLTSTA_PFD |
			PCI_EXP_SLTSTA_MRLSC | PCI_EXP_SLTSTA_PDC |
			PCI_EXP_SLTSTA_CC | PCI_EXP_SLTSTA_DLLSC) << 16,
		.ro = (PCI_EXP_SLTSTA_MRLSS | PCI_EXP_SLTSTA_PDS |
		       PCI_EXP_SLTSTA_EIS) << 16,
	},

	[PCI_EXP_RTCTL / 4] = {
		 
		.rw = (PCI_EXP_RTCTL_SECEE | PCI_EXP_RTCTL_SENFEE |
		       PCI_EXP_RTCTL_SEFEE | PCI_EXP_RTCTL_PMEIE |
		       PCI_EXP_RTCTL_CRSSVE),
		.ro = PCI_EXP_RTCAP_CRSVIS << 16,
	},

	[PCI_EXP_RTSTA / 4] = {
		 
		.ro = GENMASK(15, 0) | PCI_EXP_RTSTA_PENDING,
		.w1c = PCI_EXP_RTSTA_PME,
	},

	[PCI_EXP_DEVCAP2 / 4] = {
		 
		.ro = BIT(31) | GENMASK(23, 0),
	},

	[PCI_EXP_DEVCTL2 / 4] = {
		 
		.rw = GENMASK(15, 12) | GENMASK(10, 0),
	},

	[PCI_EXP_LNKCAP2 / 4] = {
		 
		.ro = BIT(31) | GENMASK(24, 1),
	},

	[PCI_EXP_LNKCTL2 / 4] = {
		 
		.rw = GENMASK(15, 0),
		.w1c = (BIT(15) | BIT(5)) << 16,
		.ro = (GENMASK(14, 12) | GENMASK(9, 6) | GENMASK(4, 0)) << 16,
	},

	[PCI_EXP_SLTCAP2 / 4] = {
		 
	},

	[PCI_EXP_SLTCTL2 / 4] = {
		 
	},
};

static pci_bridge_emul_read_status_t
pci_bridge_emul_read_ssid(struct pci_bridge_emul *bridge, int reg, u32 *value)
{
	switch (reg) {
	case PCI_CAP_LIST_ID:
		*value = PCI_CAP_ID_SSVID |
			((bridge->pcie_start > bridge->ssid_start) ? (bridge->pcie_start << 8) : 0);
		return PCI_BRIDGE_EMUL_HANDLED;

	case PCI_SSVID_VENDOR_ID:
		*value = bridge->subsystem_vendor_id |
			(bridge->subsystem_id << 16);
		return PCI_BRIDGE_EMUL_HANDLED;

	default:
		return PCI_BRIDGE_EMUL_NOT_HANDLED;
	}
}

 
int pci_bridge_emul_init(struct pci_bridge_emul *bridge,
			 unsigned int flags)
{
	BUILD_BUG_ON(sizeof(bridge->conf) != PCI_BRIDGE_CONF_END);

	 
	bridge->conf.class_revision |=
		cpu_to_le32(PCI_CLASS_BRIDGE_PCI_NORMAL << 8);
	bridge->conf.header_type = PCI_HEADER_TYPE_BRIDGE;
	bridge->conf.cache_line_size = 0x10;
	bridge->conf.status = cpu_to_le16(PCI_STATUS_CAP_LIST);
	bridge->pci_regs_behavior = kmemdup(pci_regs_behavior,
					    sizeof(pci_regs_behavior),
					    GFP_KERNEL);
	if (!bridge->pci_regs_behavior)
		return -ENOMEM;

	 
	if (!bridge->ssid_start && !bridge->pcie_start) {
		if (bridge->subsystem_vendor_id)
			bridge->ssid_start = PCI_BRIDGE_CONF_END;
		if (bridge->has_pcie)
			bridge->pcie_start = bridge->ssid_start + PCI_CAP_SSID_SIZEOF;
	} else if (!bridge->ssid_start && bridge->subsystem_vendor_id) {
		if (bridge->pcie_start - PCI_BRIDGE_CONF_END >= PCI_CAP_SSID_SIZEOF)
			bridge->ssid_start = PCI_BRIDGE_CONF_END;
		else
			bridge->ssid_start = bridge->pcie_start + PCI_CAP_PCIE_SIZEOF;
	} else if (!bridge->pcie_start && bridge->has_pcie) {
		if (bridge->ssid_start - PCI_BRIDGE_CONF_END >= PCI_CAP_PCIE_SIZEOF)
			bridge->pcie_start = PCI_BRIDGE_CONF_END;
		else
			bridge->pcie_start = bridge->ssid_start + PCI_CAP_SSID_SIZEOF;
	}

	bridge->conf.capabilities_pointer = min(bridge->ssid_start, bridge->pcie_start);

	if (bridge->conf.capabilities_pointer)
		bridge->conf.status |= cpu_to_le16(PCI_STATUS_CAP_LIST);

	if (bridge->has_pcie) {
		bridge->pcie_conf.cap_id = PCI_CAP_ID_EXP;
		bridge->pcie_conf.next = (bridge->ssid_start > bridge->pcie_start) ?
					 bridge->ssid_start : 0;
		bridge->pcie_conf.cap |= cpu_to_le16(PCI_EXP_TYPE_ROOT_PORT << 4);
		bridge->pcie_cap_regs_behavior =
			kmemdup(pcie_cap_regs_behavior,
				sizeof(pcie_cap_regs_behavior),
				GFP_KERNEL);
		if (!bridge->pcie_cap_regs_behavior) {
			kfree(bridge->pci_regs_behavior);
			return -ENOMEM;
		}
		 
		bridge->pci_regs_behavior[PCI_CACHE_LINE_SIZE / 4].ro &=
			~GENMASK(15, 8);
		bridge->pci_regs_behavior[PCI_COMMAND / 4].ro &=
			~((PCI_COMMAND_SPECIAL | PCI_COMMAND_INVALIDATE |
			   PCI_COMMAND_VGA_PALETTE | PCI_COMMAND_WAIT |
			   PCI_COMMAND_FAST_BACK) |
			  (PCI_STATUS_66MHZ | PCI_STATUS_FAST_BACK |
			   PCI_STATUS_DEVSEL_MASK) << 16);
		bridge->pci_regs_behavior[PCI_PRIMARY_BUS / 4].ro &=
			~GENMASK(31, 24);
		bridge->pci_regs_behavior[PCI_IO_BASE / 4].ro &=
			~((PCI_STATUS_66MHZ | PCI_STATUS_FAST_BACK |
			   PCI_STATUS_DEVSEL_MASK) << 16);
		bridge->pci_regs_behavior[PCI_INTERRUPT_LINE / 4].rw &=
			~((PCI_BRIDGE_CTL_MASTER_ABORT |
			   BIT(8) | BIT(9) | BIT(11)) << 16);
		bridge->pci_regs_behavior[PCI_INTERRUPT_LINE / 4].ro &=
			~((PCI_BRIDGE_CTL_FAST_BACK) << 16);
		bridge->pci_regs_behavior[PCI_INTERRUPT_LINE / 4].w1c &=
			~(BIT(10) << 16);
	}

	if (flags & PCI_BRIDGE_EMUL_NO_PREFMEM_FORWARD) {
		bridge->pci_regs_behavior[PCI_PREF_MEMORY_BASE / 4].ro = ~0;
		bridge->pci_regs_behavior[PCI_PREF_MEMORY_BASE / 4].rw = 0;
	}

	if (flags & PCI_BRIDGE_EMUL_NO_IO_FORWARD) {
		bridge->pci_regs_behavior[PCI_COMMAND / 4].ro |= PCI_COMMAND_IO;
		bridge->pci_regs_behavior[PCI_COMMAND / 4].rw &= ~PCI_COMMAND_IO;
		bridge->pci_regs_behavior[PCI_IO_BASE / 4].ro |= GENMASK(15, 0);
		bridge->pci_regs_behavior[PCI_IO_BASE / 4].rw &= ~GENMASK(15, 0);
		bridge->pci_regs_behavior[PCI_IO_BASE_UPPER16 / 4].ro = ~0;
		bridge->pci_regs_behavior[PCI_IO_BASE_UPPER16 / 4].rw = 0;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(pci_bridge_emul_init);

 
void pci_bridge_emul_cleanup(struct pci_bridge_emul *bridge)
{
	if (bridge->has_pcie)
		kfree(bridge->pcie_cap_regs_behavior);
	kfree(bridge->pci_regs_behavior);
}
EXPORT_SYMBOL_GPL(pci_bridge_emul_cleanup);

 
int pci_bridge_emul_conf_read(struct pci_bridge_emul *bridge, int where,
			      int size, u32 *value)
{
	int ret;
	int reg = where & ~3;
	pci_bridge_emul_read_status_t (*read_op)(struct pci_bridge_emul *bridge,
						 int reg, u32 *value);
	__le32 *cfgspace;
	const struct pci_bridge_reg_behavior *behavior;

	if (reg < PCI_BRIDGE_CONF_END) {
		 
		read_op = bridge->ops->read_base;
		cfgspace = (__le32 *) &bridge->conf;
		behavior = bridge->pci_regs_behavior;
	} else if (reg >= bridge->ssid_start && reg < bridge->ssid_start + PCI_CAP_SSID_SIZEOF &&
		   bridge->subsystem_vendor_id) {
		 
		reg -= bridge->ssid_start;
		read_op = pci_bridge_emul_read_ssid;
		cfgspace = NULL;
		behavior = NULL;
	} else if (reg >= bridge->pcie_start && reg < bridge->pcie_start + PCI_CAP_PCIE_SIZEOF &&
		   bridge->has_pcie) {
		 
		reg -= bridge->pcie_start;
		read_op = bridge->ops->read_pcie;
		cfgspace = (__le32 *) &bridge->pcie_conf;
		behavior = bridge->pcie_cap_regs_behavior;
	} else if (reg >= PCI_CFG_SPACE_SIZE && bridge->has_pcie) {
		 
		reg -= PCI_CFG_SPACE_SIZE;
		read_op = bridge->ops->read_ext;
		cfgspace = NULL;
		behavior = NULL;
	} else {
		 
		*value = 0;
		return PCIBIOS_SUCCESSFUL;
	}

	if (read_op)
		ret = read_op(bridge, reg, value);
	else
		ret = PCI_BRIDGE_EMUL_NOT_HANDLED;

	if (ret == PCI_BRIDGE_EMUL_NOT_HANDLED) {
		if (cfgspace)
			*value = le32_to_cpu(cfgspace[reg / 4]);
		else
			*value = 0;
	}

	 
	if (behavior)
		*value &= behavior[reg / 4].ro | behavior[reg / 4].rw |
			  behavior[reg / 4].w1c;

	if (size == 1)
		*value = (*value >> (8 * (where & 3))) & 0xff;
	else if (size == 2)
		*value = (*value >> (8 * (where & 3))) & 0xffff;
	else if (size != 4)
		return PCIBIOS_BAD_REGISTER_NUMBER;

	return PCIBIOS_SUCCESSFUL;
}
EXPORT_SYMBOL_GPL(pci_bridge_emul_conf_read);

 
int pci_bridge_emul_conf_write(struct pci_bridge_emul *bridge, int where,
			       int size, u32 value)
{
	int reg = where & ~3;
	int mask, ret, old, new, shift;
	void (*write_op)(struct pci_bridge_emul *bridge, int reg,
			 u32 old, u32 new, u32 mask);
	__le32 *cfgspace;
	const struct pci_bridge_reg_behavior *behavior;

	ret = pci_bridge_emul_conf_read(bridge, reg, 4, &old);
	if (ret != PCIBIOS_SUCCESSFUL)
		return ret;

	if (reg < PCI_BRIDGE_CONF_END) {
		 
		write_op = bridge->ops->write_base;
		cfgspace = (__le32 *) &bridge->conf;
		behavior = bridge->pci_regs_behavior;
	} else if (reg >= bridge->pcie_start && reg < bridge->pcie_start + PCI_CAP_PCIE_SIZEOF &&
		   bridge->has_pcie) {
		 
		reg -= bridge->pcie_start;
		write_op = bridge->ops->write_pcie;
		cfgspace = (__le32 *) &bridge->pcie_conf;
		behavior = bridge->pcie_cap_regs_behavior;
	} else if (reg >= PCI_CFG_SPACE_SIZE && bridge->has_pcie) {
		 
		reg -= PCI_CFG_SPACE_SIZE;
		write_op = bridge->ops->write_ext;
		cfgspace = NULL;
		behavior = NULL;
	} else {
		 
		return PCIBIOS_SUCCESSFUL;
	}

	shift = (where & 0x3) * 8;

	if (size == 4)
		mask = 0xffffffff;
	else if (size == 2)
		mask = 0xffff << shift;
	else if (size == 1)
		mask = 0xff << shift;
	else
		return PCIBIOS_BAD_REGISTER_NUMBER;

	if (behavior) {
		 
		new = old & (~mask | ~behavior[reg / 4].rw);

		 
		new |= (value << shift) & (behavior[reg / 4].rw & mask);

		 
		new &= ~((value << shift) & (behavior[reg / 4].w1c & mask));
	} else {
		new = old & ~mask;
		new |= (value << shift) & mask;
	}

	if (cfgspace) {
		 
		cfgspace[reg / 4] = cpu_to_le32(new);
	}

	if (behavior) {
		 
		new &= ~(behavior[reg / 4].w1c & ~mask);

		 
		new |= (value << shift) & (behavior[reg / 4].w1c & mask);
	}

	if (write_op)
		write_op(bridge, reg, old, new, mask);

	return PCIBIOS_SUCCESSFUL;
}
EXPORT_SYMBOL_GPL(pci_bridge_emul_conf_write);
