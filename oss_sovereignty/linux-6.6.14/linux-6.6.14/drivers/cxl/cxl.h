#ifndef __CXL_H__
#define __CXL_H__
#include <linux/libnvdimm.h>
#include <linux/bitfield.h>
#include <linux/bitops.h>
#include <linux/log2.h>
#include <linux/io.h>
#define CXL_COMPONENT_REG_BLOCK_SIZE SZ_64K
#define CXL_CM_OFFSET 0x1000
#define CXL_CM_CAP_HDR_OFFSET 0x0
#define   CXL_CM_CAP_HDR_ID_MASK GENMASK(15, 0)
#define     CM_CAP_HDR_CAP_ID 1
#define   CXL_CM_CAP_HDR_VERSION_MASK GENMASK(19, 16)
#define     CM_CAP_HDR_CAP_VERSION 1
#define   CXL_CM_CAP_HDR_CACHE_MEM_VERSION_MASK GENMASK(23, 20)
#define     CM_CAP_HDR_CACHE_MEM_VERSION 1
#define   CXL_CM_CAP_HDR_ARRAY_SIZE_MASK GENMASK(31, 24)
#define CXL_CM_CAP_PTR_MASK GENMASK(31, 20)
#define   CXL_CM_CAP_CAP_ID_RAS 0x2
#define   CXL_CM_CAP_CAP_ID_HDM 0x5
#define   CXL_CM_CAP_CAP_HDM_VERSION 1
#define CXL_HDM_DECODER_CAP_OFFSET 0x0
#define   CXL_HDM_DECODER_COUNT_MASK GENMASK(3, 0)
#define   CXL_HDM_DECODER_TARGET_COUNT_MASK GENMASK(7, 4)
#define   CXL_HDM_DECODER_INTERLEAVE_11_8 BIT(8)
#define   CXL_HDM_DECODER_INTERLEAVE_14_12 BIT(9)
#define CXL_HDM_DECODER_CTRL_OFFSET 0x4
#define   CXL_HDM_DECODER_ENABLE BIT(1)
#define CXL_HDM_DECODER0_BASE_LOW_OFFSET(i) (0x20 * (i) + 0x10)
#define CXL_HDM_DECODER0_BASE_HIGH_OFFSET(i) (0x20 * (i) + 0x14)
#define CXL_HDM_DECODER0_SIZE_LOW_OFFSET(i) (0x20 * (i) + 0x18)
#define CXL_HDM_DECODER0_SIZE_HIGH_OFFSET(i) (0x20 * (i) + 0x1c)
#define CXL_HDM_DECODER0_CTRL_OFFSET(i) (0x20 * (i) + 0x20)
#define   CXL_HDM_DECODER0_CTRL_IG_MASK GENMASK(3, 0)
#define   CXL_HDM_DECODER0_CTRL_IW_MASK GENMASK(7, 4)
#define   CXL_HDM_DECODER0_CTRL_LOCK BIT(8)
#define   CXL_HDM_DECODER0_CTRL_COMMIT BIT(9)
#define   CXL_HDM_DECODER0_CTRL_COMMITTED BIT(10)
#define   CXL_HDM_DECODER0_CTRL_COMMIT_ERROR BIT(11)
#define   CXL_HDM_DECODER0_CTRL_HOSTONLY BIT(12)
#define CXL_HDM_DECODER0_TL_LOW(i) (0x20 * (i) + 0x24)
#define CXL_HDM_DECODER0_TL_HIGH(i) (0x20 * (i) + 0x28)
#define CXL_HDM_DECODER0_SKIP_LOW(i) CXL_HDM_DECODER0_TL_LOW(i)
#define CXL_HDM_DECODER0_SKIP_HIGH(i) CXL_HDM_DECODER0_TL_HIGH(i)
#define CXL_DECODER_MIN_GRANULARITY 256
#define CXL_DECODER_MAX_ENCODED_IG 6
static inline int cxl_hdm_decoder_count(u32 cap_hdr)
{
	int val = FIELD_GET(CXL_HDM_DECODER_COUNT_MASK, cap_hdr);
	return val ? val * 2 : 1;
}
static inline int eig_to_granularity(u16 eig, unsigned int *granularity)
{
	if (eig > CXL_DECODER_MAX_ENCODED_IG)
		return -EINVAL;
	*granularity = CXL_DECODER_MIN_GRANULARITY << eig;
	return 0;
}
static inline int eiw_to_ways(u8 eiw, unsigned int *ways)
{
	switch (eiw) {
	case 0 ... 4:
		*ways = 1 << eiw;
		break;
	case 8 ... 10:
		*ways = 3 << (eiw - 8);
		break;
	default:
		return -EINVAL;
	}
	return 0;
}
static inline int granularity_to_eig(int granularity, u16 *eig)
{
	if (granularity > SZ_16K || granularity < CXL_DECODER_MIN_GRANULARITY ||
	    !is_power_of_2(granularity))
		return -EINVAL;
	*eig = ilog2(granularity) - 8;
	return 0;
}
static inline int ways_to_eiw(unsigned int ways, u8 *eiw)
{
	if (ways > 16)
		return -EINVAL;
	if (is_power_of_2(ways)) {
		*eiw = ilog2(ways);
		return 0;
	}
	if (ways % 3)
		return -EINVAL;
	ways /= 3;
	if (!is_power_of_2(ways))
		return -EINVAL;
	*eiw = ilog2(ways) + 8;
	return 0;
}
#define CXL_RAS_UNCORRECTABLE_STATUS_OFFSET 0x0
#define   CXL_RAS_UNCORRECTABLE_STATUS_MASK (GENMASK(16, 14) | GENMASK(11, 0))
#define CXL_RAS_UNCORRECTABLE_MASK_OFFSET 0x4
#define   CXL_RAS_UNCORRECTABLE_MASK_MASK (GENMASK(16, 14) | GENMASK(11, 0))
#define   CXL_RAS_UNCORRECTABLE_MASK_F256B_MASK BIT(8)
#define CXL_RAS_UNCORRECTABLE_SEVERITY_OFFSET 0x8
#define   CXL_RAS_UNCORRECTABLE_SEVERITY_MASK (GENMASK(16, 14) | GENMASK(11, 0))
#define CXL_RAS_CORRECTABLE_STATUS_OFFSET 0xC
#define   CXL_RAS_CORRECTABLE_STATUS_MASK GENMASK(6, 0)
#define CXL_RAS_CORRECTABLE_MASK_OFFSET 0x10
#define   CXL_RAS_CORRECTABLE_MASK_MASK GENMASK(6, 0)
#define CXL_RAS_CAP_CONTROL_OFFSET 0x14
#define CXL_RAS_CAP_CONTROL_FE_MASK GENMASK(5, 0)
#define CXL_RAS_HEADER_LOG_OFFSET 0x18
#define CXL_RAS_CAPABILITY_LENGTH 0x58
#define CXL_HEADERLOG_SIZE SZ_512
#define CXL_HEADERLOG_SIZE_U32 SZ_512 / sizeof(u32)
#define CXLDEV_CAP_ARRAY_OFFSET 0x0
#define   CXLDEV_CAP_ARRAY_CAP_ID 0
#define   CXLDEV_CAP_ARRAY_ID_MASK GENMASK_ULL(15, 0)
#define   CXLDEV_CAP_ARRAY_COUNT_MASK GENMASK_ULL(47, 32)
#define CXLDEV_CAP_HDR_CAP_ID_MASK GENMASK(15, 0)
#define CXLDEV_CAP_CAP_ID_DEVICE_STATUS 0x1
#define CXLDEV_CAP_CAP_ID_PRIMARY_MAILBOX 0x2
#define CXLDEV_CAP_CAP_ID_SECONDARY_MAILBOX 0x3
#define CXLDEV_CAP_CAP_ID_MEMDEV 0x4000
#define CXLDEV_DEV_EVENT_STATUS_OFFSET		0x00
#define CXLDEV_EVENT_STATUS_INFO		BIT(0)
#define CXLDEV_EVENT_STATUS_WARN		BIT(1)
#define CXLDEV_EVENT_STATUS_FAIL		BIT(2)
#define CXLDEV_EVENT_STATUS_FATAL		BIT(3)
#define CXLDEV_EVENT_STATUS_ALL (CXLDEV_EVENT_STATUS_INFO |	\
				 CXLDEV_EVENT_STATUS_WARN |	\
				 CXLDEV_EVENT_STATUS_FAIL |	\
				 CXLDEV_EVENT_STATUS_FATAL)
#define CXLDEV_EVENT_INT_MODE_MASK	GENMASK(1, 0)
#define CXLDEV_EVENT_INT_MSGNUM_MASK	GENMASK(7, 4)
#define CXLDEV_MBOX_CAPS_OFFSET 0x00
#define   CXLDEV_MBOX_CAP_PAYLOAD_SIZE_MASK GENMASK(4, 0)
#define   CXLDEV_MBOX_CAP_BG_CMD_IRQ BIT(6)
#define   CXLDEV_MBOX_CAP_IRQ_MSGNUM_MASK GENMASK(10, 7)
#define CXLDEV_MBOX_CTRL_OFFSET 0x04
#define   CXLDEV_MBOX_CTRL_DOORBELL BIT(0)
#define   CXLDEV_MBOX_CTRL_BG_CMD_IRQ BIT(2)
#define CXLDEV_MBOX_CMD_OFFSET 0x08
#define   CXLDEV_MBOX_CMD_COMMAND_OPCODE_MASK GENMASK_ULL(15, 0)
#define   CXLDEV_MBOX_CMD_PAYLOAD_LENGTH_MASK GENMASK_ULL(36, 16)
#define CXLDEV_MBOX_STATUS_OFFSET 0x10
#define   CXLDEV_MBOX_STATUS_BG_CMD BIT(0)
#define   CXLDEV_MBOX_STATUS_RET_CODE_MASK GENMASK_ULL(47, 32)
#define CXLDEV_MBOX_BG_CMD_STATUS_OFFSET 0x18
#define   CXLDEV_MBOX_BG_CMD_COMMAND_OPCODE_MASK GENMASK_ULL(15, 0)
#define   CXLDEV_MBOX_BG_CMD_COMMAND_PCT_MASK GENMASK_ULL(22, 16)
#define   CXLDEV_MBOX_BG_CMD_COMMAND_RC_MASK GENMASK_ULL(47, 32)
#define   CXLDEV_MBOX_BG_CMD_COMMAND_VENDOR_MASK GENMASK_ULL(63, 48)
#define CXLDEV_MBOX_PAYLOAD_OFFSET 0x20
struct cxl_regs {
	struct_group_tagged(cxl_component_regs, component,
		void __iomem *hdm_decoder;
		void __iomem *ras;
	);
	struct_group_tagged(cxl_device_regs, device_regs,
		void __iomem *status, *mbox, *memdev;
	);
	struct_group_tagged(cxl_pmu_regs, pmu_regs,
		void __iomem *pmu;
	);
};
struct cxl_reg_map {
	bool valid;
	int id;
	unsigned long offset;
	unsigned long size;
};
struct cxl_component_reg_map {
	struct cxl_reg_map hdm_decoder;
	struct cxl_reg_map ras;
};
struct cxl_device_reg_map {
	struct cxl_reg_map status;
	struct cxl_reg_map mbox;
	struct cxl_reg_map memdev;
};
struct cxl_pmu_reg_map {
	struct cxl_reg_map pmu;
};
struct cxl_register_map {
	struct device *host;
	void __iomem *base;
	resource_size_t resource;
	resource_size_t max_size;
	u8 reg_type;
	union {
		struct cxl_component_reg_map component_map;
		struct cxl_device_reg_map device_map;
		struct cxl_pmu_reg_map pmu_map;
	};
};
void cxl_probe_component_regs(struct device *dev, void __iomem *base,
			      struct cxl_component_reg_map *map);
void cxl_probe_device_regs(struct device *dev, void __iomem *base,
			   struct cxl_device_reg_map *map);
int cxl_map_component_regs(const struct cxl_register_map *map,
			   struct cxl_component_regs *regs,
			   unsigned long map_mask);
int cxl_map_device_regs(const struct cxl_register_map *map,
			struct cxl_device_regs *regs);
int cxl_map_pmu_regs(struct pci_dev *pdev, struct cxl_pmu_regs *regs,
		     struct cxl_register_map *map);
enum cxl_regloc_type;
int cxl_count_regblock(struct pci_dev *pdev, enum cxl_regloc_type type);
int cxl_find_regblock_instance(struct pci_dev *pdev, enum cxl_regloc_type type,
			       struct cxl_register_map *map, int index);
int cxl_find_regblock(struct pci_dev *pdev, enum cxl_regloc_type type,
		      struct cxl_register_map *map);
int cxl_setup_regs(struct cxl_register_map *map);
struct cxl_dport;
resource_size_t cxl_rcd_component_reg_phys(struct device *dev,
					   struct cxl_dport *dport);
#define CXL_RESOURCE_NONE ((resource_size_t) -1)
#define CXL_TARGET_STRLEN 20
#define CXL_DECODER_F_RAM   BIT(0)
#define CXL_DECODER_F_PMEM  BIT(1)
#define CXL_DECODER_F_TYPE2 BIT(2)
#define CXL_DECODER_F_TYPE3 BIT(3)
#define CXL_DECODER_F_LOCK  BIT(4)
#define CXL_DECODER_F_ENABLE    BIT(5)
#define CXL_DECODER_F_MASK  GENMASK(5, 0)
enum cxl_decoder_type {
	CXL_DECODER_DEVMEM = 2,
	CXL_DECODER_HOSTONLYMEM = 3,
};
#define CXL_DECODER_MAX_INTERLEAVE 16
struct cxl_decoder {
	struct device dev;
	int id;
	struct range hpa_range;
	int interleave_ways;
	int interleave_granularity;
	enum cxl_decoder_type target_type;
	struct cxl_region *region;
	unsigned long flags;
	int (*commit)(struct cxl_decoder *cxld);
	int (*reset)(struct cxl_decoder *cxld);
};
enum cxl_decoder_mode {
	CXL_DECODER_NONE,
	CXL_DECODER_RAM,
	CXL_DECODER_PMEM,
	CXL_DECODER_MIXED,
	CXL_DECODER_DEAD,
};
static inline const char *cxl_decoder_mode_name(enum cxl_decoder_mode mode)
{
	static const char * const names[] = {
		[CXL_DECODER_NONE] = "none",
		[CXL_DECODER_RAM] = "ram",
		[CXL_DECODER_PMEM] = "pmem",
		[CXL_DECODER_MIXED] = "mixed",
	};
	if (mode >= CXL_DECODER_NONE && mode <= CXL_DECODER_MIXED)
		return names[mode];
	return "mixed";
}
enum cxl_decoder_state {
	CXL_DECODER_STATE_MANUAL,
	CXL_DECODER_STATE_AUTO,
};
struct cxl_endpoint_decoder {
	struct cxl_decoder cxld;
	struct resource *dpa_res;
	resource_size_t skip;
	enum cxl_decoder_mode mode;
	enum cxl_decoder_state state;
	int pos;
};
struct cxl_switch_decoder {
	struct cxl_decoder cxld;
	int nr_targets;
	struct cxl_dport *target[];
};
struct cxl_root_decoder;
typedef struct cxl_dport *(*cxl_calc_hb_fn)(struct cxl_root_decoder *cxlrd,
					    int pos);
struct cxl_root_decoder {
	struct resource *res;
	atomic_t region_id;
	cxl_calc_hb_fn calc_hb;
	void *platform_data;
	struct mutex range_lock;
	struct cxl_switch_decoder cxlsd;
};
enum cxl_config_state {
	CXL_CONFIG_IDLE,
	CXL_CONFIG_INTERLEAVE_ACTIVE,
	CXL_CONFIG_ACTIVE,
	CXL_CONFIG_RESET_PENDING,
	CXL_CONFIG_COMMIT,
};
struct cxl_region_params {
	enum cxl_config_state state;
	uuid_t uuid;
	int interleave_ways;
	int interleave_granularity;
	struct resource *res;
	struct cxl_endpoint_decoder *targets[CXL_DECODER_MAX_INTERLEAVE];
	int nr_targets;
};
#define CXL_REGION_F_AUTO 0
#define CXL_REGION_F_NEEDS_RESET 1
struct cxl_region {
	struct device dev;
	int id;
	enum cxl_decoder_mode mode;
	enum cxl_decoder_type type;
	struct cxl_nvdimm_bridge *cxl_nvb;
	struct cxl_pmem_region *cxlr_pmem;
	unsigned long flags;
	struct cxl_region_params params;
};
struct cxl_nvdimm_bridge {
	int id;
	struct device dev;
	struct cxl_port *port;
	struct nvdimm_bus *nvdimm_bus;
	struct nvdimm_bus_descriptor nd_desc;
};
#define CXL_DEV_ID_LEN 19
struct cxl_nvdimm {
	struct device dev;
	struct cxl_memdev *cxlmd;
	u8 dev_id[CXL_DEV_ID_LEN];  
};
struct cxl_pmem_region_mapping {
	struct cxl_memdev *cxlmd;
	struct cxl_nvdimm *cxl_nvd;
	u64 start;
	u64 size;
	int position;
};
struct cxl_pmem_region {
	struct device dev;
	struct cxl_region *cxlr;
	struct nd_region *nd_region;
	struct range hpa_range;
	int nr_mappings;
	struct cxl_pmem_region_mapping mapping[];
};
struct cxl_dax_region {
	struct device dev;
	struct cxl_region *cxlr;
	struct range hpa_range;
};
struct cxl_port {
	struct device dev;
	struct device *uport_dev;
	struct device *host_bridge;
	int id;
	struct xarray dports;
	struct xarray endpoints;
	struct xarray regions;
	struct cxl_dport *parent_dport;
	struct ida decoder_ida;
	struct cxl_register_map comp_map;
	int nr_dports;
	int hdm_end;
	int commit_end;
	resource_size_t component_reg_phys;
	bool dead;
	unsigned int depth;
	struct cxl_cdat {
		void *table;
		size_t length;
	} cdat;
	bool cdat_available;
};
static inline struct cxl_dport *
cxl_find_dport_by_dev(struct cxl_port *port, const struct device *dport_dev)
{
	return xa_load(&port->dports, (unsigned long)dport_dev);
}
struct cxl_rcrb_info {
	resource_size_t base;
	u16 aer_cap;
};
struct cxl_dport {
	struct device *dport_dev;
	struct cxl_register_map comp_map;
	int port_id;
	struct cxl_rcrb_info rcrb;
	bool rch;
	struct cxl_port *port;
};
struct cxl_ep {
	struct device *ep;
	struct cxl_dport *dport;
	struct cxl_port *next;
};
struct cxl_region_ref {
	struct cxl_port *port;
	struct cxl_decoder *decoder;
	struct cxl_region *region;
	struct xarray endpoints;
	int nr_targets_set;
	int nr_eps;
	int nr_targets;
};
static inline bool is_cxl_root(struct cxl_port *port)
{
	return port->uport_dev == port->dev.parent;
}
int cxl_num_decoders_committed(struct cxl_port *port);
bool is_cxl_port(const struct device *dev);
struct cxl_port *to_cxl_port(const struct device *dev);
struct pci_bus;
int devm_cxl_register_pci_bus(struct device *host, struct device *uport_dev,
			      struct pci_bus *bus);
struct pci_bus *cxl_port_to_pci_bus(struct cxl_port *port);
struct cxl_port *devm_cxl_add_port(struct device *host,
				   struct device *uport_dev,
				   resource_size_t component_reg_phys,
				   struct cxl_dport *parent_dport);
struct cxl_port *find_cxl_root(struct cxl_port *port);
int devm_cxl_enumerate_ports(struct cxl_memdev *cxlmd);
void cxl_bus_rescan(void);
void cxl_bus_drain(void);
struct cxl_port *cxl_pci_find_port(struct pci_dev *pdev,
				   struct cxl_dport **dport);
struct cxl_port *cxl_mem_find_port(struct cxl_memdev *cxlmd,
				   struct cxl_dport **dport);
bool schedule_cxl_memdev_detach(struct cxl_memdev *cxlmd);
struct cxl_dport *devm_cxl_add_dport(struct cxl_port *port,
				     struct device *dport, int port_id,
				     resource_size_t component_reg_phys);
struct cxl_dport *devm_cxl_add_rch_dport(struct cxl_port *port,
					 struct device *dport_dev, int port_id,
					 resource_size_t rcrb);
struct cxl_decoder *to_cxl_decoder(struct device *dev);
struct cxl_root_decoder *to_cxl_root_decoder(struct device *dev);
struct cxl_switch_decoder *to_cxl_switch_decoder(struct device *dev);
struct cxl_endpoint_decoder *to_cxl_endpoint_decoder(struct device *dev);
bool is_root_decoder(struct device *dev);
bool is_switch_decoder(struct device *dev);
bool is_endpoint_decoder(struct device *dev);
struct cxl_root_decoder *cxl_root_decoder_alloc(struct cxl_port *port,
						unsigned int nr_targets,
						cxl_calc_hb_fn calc_hb);
struct cxl_dport *cxl_hb_modulo(struct cxl_root_decoder *cxlrd, int pos);
struct cxl_switch_decoder *cxl_switch_decoder_alloc(struct cxl_port *port,
						    unsigned int nr_targets);
int cxl_decoder_add(struct cxl_decoder *cxld, int *target_map);
struct cxl_endpoint_decoder *cxl_endpoint_decoder_alloc(struct cxl_port *port);
int cxl_decoder_add_locked(struct cxl_decoder *cxld, int *target_map);
int cxl_decoder_autoremove(struct device *host, struct cxl_decoder *cxld);
int cxl_endpoint_autoremove(struct cxl_memdev *cxlmd, struct cxl_port *endpoint);
struct cxl_endpoint_dvsec_info {
	bool mem_enabled;
	int ranges;
	struct cxl_port *port;
	struct range dvsec_range[2];
};
struct cxl_hdm;
struct cxl_hdm *devm_cxl_setup_hdm(struct cxl_port *port,
				   struct cxl_endpoint_dvsec_info *info);
int devm_cxl_enumerate_decoders(struct cxl_hdm *cxlhdm,
				struct cxl_endpoint_dvsec_info *info);
int devm_cxl_add_passthrough_decoder(struct cxl_port *port);
int cxl_dvsec_rr_decode(struct device *dev, int dvsec,
			struct cxl_endpoint_dvsec_info *info);
bool is_cxl_region(struct device *dev);
extern struct bus_type cxl_bus_type;
struct cxl_driver {
	const char *name;
	int (*probe)(struct device *dev);
	void (*remove)(struct device *dev);
	struct device_driver drv;
	int id;
};
static inline struct cxl_driver *to_cxl_drv(struct device_driver *drv)
{
	return container_of(drv, struct cxl_driver, drv);
}
int __cxl_driver_register(struct cxl_driver *cxl_drv, struct module *owner,
			  const char *modname);
#define cxl_driver_register(x) __cxl_driver_register(x, THIS_MODULE, KBUILD_MODNAME)
void cxl_driver_unregister(struct cxl_driver *cxl_drv);
#define module_cxl_driver(__cxl_driver) \
	module_driver(__cxl_driver, cxl_driver_register, cxl_driver_unregister)
#define CXL_DEVICE_NVDIMM_BRIDGE	1
#define CXL_DEVICE_NVDIMM		2
#define CXL_DEVICE_PORT			3
#define CXL_DEVICE_ROOT			4
#define CXL_DEVICE_MEMORY_EXPANDER	5
#define CXL_DEVICE_REGION		6
#define CXL_DEVICE_PMEM_REGION		7
#define CXL_DEVICE_DAX_REGION		8
#define CXL_DEVICE_PMU			9
#define MODULE_ALIAS_CXL(type) MODULE_ALIAS("cxl:t" __stringify(type) "*")
#define CXL_MODALIAS_FMT "cxl:t%d"
struct cxl_nvdimm_bridge *to_cxl_nvdimm_bridge(struct device *dev);
struct cxl_nvdimm_bridge *devm_cxl_add_nvdimm_bridge(struct device *host,
						     struct cxl_port *port);
struct cxl_nvdimm *to_cxl_nvdimm(struct device *dev);
bool is_cxl_nvdimm(struct device *dev);
bool is_cxl_nvdimm_bridge(struct device *dev);
int devm_cxl_add_nvdimm(struct cxl_memdev *cxlmd);
struct cxl_nvdimm_bridge *cxl_find_nvdimm_bridge(struct cxl_memdev *cxlmd);
#ifdef CONFIG_CXL_REGION
bool is_cxl_pmem_region(struct device *dev);
struct cxl_pmem_region *to_cxl_pmem_region(struct device *dev);
int cxl_add_to_region(struct cxl_port *root,
		      struct cxl_endpoint_decoder *cxled);
struct cxl_dax_region *to_cxl_dax_region(struct device *dev);
#else
static inline bool is_cxl_pmem_region(struct device *dev)
{
	return false;
}
static inline struct cxl_pmem_region *to_cxl_pmem_region(struct device *dev)
{
	return NULL;
}
static inline int cxl_add_to_region(struct cxl_port *root,
				    struct cxl_endpoint_decoder *cxled)
{
	return 0;
}
static inline struct cxl_dax_region *to_cxl_dax_region(struct device *dev)
{
	return NULL;
}
#endif
#ifndef __mock
#define __mock static
#endif
#endif  
