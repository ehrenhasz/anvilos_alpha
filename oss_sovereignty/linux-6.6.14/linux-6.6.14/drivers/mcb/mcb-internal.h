#ifndef __MCB_INTERNAL
#define __MCB_INTERNAL
#include <linux/types.h>
#define PCI_VENDOR_ID_MEN		0x1a88
#define PCI_DEVICE_ID_MEN_CHAMELEON	0x4d45
#define CHAMELEONV2_MAGIC		0xabce
#define CHAM_HEADER_SIZE		0x200
enum chameleon_descriptor_type {
	CHAMELEON_DTYPE_GENERAL = 0x0,
	CHAMELEON_DTYPE_BRIDGE = 0x1,
	CHAMELEON_DTYPE_CPU = 0x2,
	CHAMELEON_DTYPE_BAR = 0x3,
	CHAMELEON_DTYPE_END = 0xf,
};
enum chameleon_bus_type {
	CHAMELEON_BUS_WISHBONE,
	CHAMELEON_BUS_AVALON,
	CHAMELEON_BUS_LPC,
	CHAMELEON_BUS_ISA,
};
struct chameleon_fpga_header {
	u8 revision;
	char model;
	u8 minor;
	u8 bus_type;
	u16 magic;
	u16 reserved;
	char filename[CHAMELEON_FILENAME_LEN];
} __packed;
#define HEADER_MAGIC_OFFSET 0x4
struct chameleon_gdd {
	__le32 reg1;
	__le32 reg2;
	__le32 offset;
	__le32 size;
} __packed;
#define GDD_IRQ(x) ((x) & 0x1f)
#define GDD_REV(x) (((x) >> 5) & 0x3f)
#define GDD_VAR(x) (((x) >> 11) & 0x3f)
#define GDD_DEV(x) (((x) >> 18) & 0x3ff)
#define GDD_DTY(x) (((x) >> 28) & 0xf)
#define GDD_BAR(x) ((x) & 0x7)
#define GDD_INS(x) (((x) >> 3) & 0x3f)
#define GDD_GRP(x) (((x) >> 9) & 0x3f)
struct chameleon_bdd {
	unsigned int irq:6;
	unsigned int rev:6;
	unsigned int var:6;
	unsigned int dev:10;
	unsigned int dtype:4;
	unsigned int bar:3;
	unsigned int inst:6;
	unsigned int dbar:3;
	unsigned int group:6;
	unsigned int reserved:14;
	u32 chamoff;
	u32 offset;
	u32 size;
} __packed;
struct chameleon_bar {
	u32 addr;
	u32 size;
};
#define BAR_CNT(x) ((x) & 0x07)
#define CHAMELEON_BAR_MAX	6
#define BAR_DESC_SIZE(x)	((x) * sizeof(struct chameleon_bar) + sizeof(__le32))
int chameleon_parse_cells(struct mcb_bus *bus, phys_addr_t mapbase,
			  void __iomem *base);
#endif
