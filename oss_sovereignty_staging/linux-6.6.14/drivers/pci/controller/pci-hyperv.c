
 

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/pci-ecam.h>
#include <linux/delay.h>
#include <linux/semaphore.h>
#include <linux/irq.h>
#include <linux/msi.h>
#include <linux/hyperv.h>
#include <linux/refcount.h>
#include <linux/irqdomain.h>
#include <linux/acpi.h>
#include <asm/mshyperv.h>

 

#define PCI_MAKE_VERSION(major, minor) ((u32)(((major) << 16) | (minor)))
#define PCI_MAJOR_VERSION(version) ((u32)(version) >> 16)
#define PCI_MINOR_VERSION(version) ((u32)(version) & 0xff)

enum pci_protocol_version_t {
	PCI_PROTOCOL_VERSION_1_1 = PCI_MAKE_VERSION(1, 1),	 
	PCI_PROTOCOL_VERSION_1_2 = PCI_MAKE_VERSION(1, 2),	 
	PCI_PROTOCOL_VERSION_1_3 = PCI_MAKE_VERSION(1, 3),	 
	PCI_PROTOCOL_VERSION_1_4 = PCI_MAKE_VERSION(1, 4),	 
};

#define CPU_AFFINITY_ALL	-1ULL

 
static enum pci_protocol_version_t pci_protocol_versions[] = {
	PCI_PROTOCOL_VERSION_1_4,
	PCI_PROTOCOL_VERSION_1_3,
	PCI_PROTOCOL_VERSION_1_2,
	PCI_PROTOCOL_VERSION_1_1,
};

#define PCI_CONFIG_MMIO_LENGTH	0x2000
#define CFG_PAGE_OFFSET 0x1000
#define CFG_PAGE_SIZE (PCI_CONFIG_MMIO_LENGTH - CFG_PAGE_OFFSET)

#define MAX_SUPPORTED_MSI_MESSAGES 0x400

#define STATUS_REVISION_MISMATCH 0xC0000059

 
#define SLOT_NAME_SIZE 11

 
#define HV_PCI_RQSTOR_SIZE 64

 

enum pci_message_type {
	 
	PCI_MESSAGE_BASE                = 0x42490000,
	PCI_BUS_RELATIONS               = PCI_MESSAGE_BASE + 0,
	PCI_QUERY_BUS_RELATIONS         = PCI_MESSAGE_BASE + 1,
	PCI_POWER_STATE_CHANGE          = PCI_MESSAGE_BASE + 4,
	PCI_QUERY_RESOURCE_REQUIREMENTS = PCI_MESSAGE_BASE + 5,
	PCI_QUERY_RESOURCE_RESOURCES    = PCI_MESSAGE_BASE + 6,
	PCI_BUS_D0ENTRY                 = PCI_MESSAGE_BASE + 7,
	PCI_BUS_D0EXIT                  = PCI_MESSAGE_BASE + 8,
	PCI_READ_BLOCK                  = PCI_MESSAGE_BASE + 9,
	PCI_WRITE_BLOCK                 = PCI_MESSAGE_BASE + 0xA,
	PCI_EJECT                       = PCI_MESSAGE_BASE + 0xB,
	PCI_QUERY_STOP                  = PCI_MESSAGE_BASE + 0xC,
	PCI_REENABLE                    = PCI_MESSAGE_BASE + 0xD,
	PCI_QUERY_STOP_FAILED           = PCI_MESSAGE_BASE + 0xE,
	PCI_EJECTION_COMPLETE           = PCI_MESSAGE_BASE + 0xF,
	PCI_RESOURCES_ASSIGNED          = PCI_MESSAGE_BASE + 0x10,
	PCI_RESOURCES_RELEASED          = PCI_MESSAGE_BASE + 0x11,
	PCI_INVALIDATE_BLOCK            = PCI_MESSAGE_BASE + 0x12,
	PCI_QUERY_PROTOCOL_VERSION      = PCI_MESSAGE_BASE + 0x13,
	PCI_CREATE_INTERRUPT_MESSAGE    = PCI_MESSAGE_BASE + 0x14,
	PCI_DELETE_INTERRUPT_MESSAGE    = PCI_MESSAGE_BASE + 0x15,
	PCI_RESOURCES_ASSIGNED2		= PCI_MESSAGE_BASE + 0x16,
	PCI_CREATE_INTERRUPT_MESSAGE2	= PCI_MESSAGE_BASE + 0x17,
	PCI_DELETE_INTERRUPT_MESSAGE2	= PCI_MESSAGE_BASE + 0x18,  
	PCI_BUS_RELATIONS2		= PCI_MESSAGE_BASE + 0x19,
	PCI_RESOURCES_ASSIGNED3         = PCI_MESSAGE_BASE + 0x1A,
	PCI_CREATE_INTERRUPT_MESSAGE3   = PCI_MESSAGE_BASE + 0x1B,
	PCI_MESSAGE_MAXIMUM
};

 

union pci_version {
	struct {
		u16 minor_version;
		u16 major_version;
	} parts;
	u32 version;
} __packed;

 
union win_slot_encoding {
	struct {
		u32	dev:5;
		u32	func:3;
		u32	reserved:24;
	} bits;
	u32 slot;
} __packed;

 
struct pci_function_description {
	u16	v_id;	 
	u16	d_id;	 
	u8	rev;
	u8	prog_intf;
	u8	subclass;
	u8	base_class;
	u32	subsystem_id;
	union win_slot_encoding win_slot;
	u32	ser;	 
} __packed;

enum pci_device_description_flags {
	HV_PCI_DEVICE_FLAG_NONE			= 0x0,
	HV_PCI_DEVICE_FLAG_NUMA_AFFINITY	= 0x1,
};

struct pci_function_description2 {
	u16	v_id;	 
	u16	d_id;	 
	u8	rev;
	u8	prog_intf;
	u8	subclass;
	u8	base_class;
	u32	subsystem_id;
	union	win_slot_encoding win_slot;
	u32	ser;	 
	u32	flags;
	u16	virtual_numa_node;
	u16	reserved;
} __packed;

 
struct hv_msi_desc {
	u8	vector;
	u8	delivery_mode;
	u16	vector_count;
	u32	reserved;
	u64	cpu_mask;
} __packed;

 
struct hv_msi_desc2 {
	u8	vector;
	u8	delivery_mode;
	u16	vector_count;
	u16	processor_count;
	u16	processor_array[32];
} __packed;

 
struct hv_msi_desc3 {
	u32	vector;
	u8	delivery_mode;
	u8	reserved;
	u16	vector_count;
	u16	processor_count;
	u16	processor_array[32];
} __packed;

 
struct tran_int_desc {
	u16	reserved;
	u16	vector_count;
	u32	data;
	u64	address;
} __packed;

 

struct pci_message {
	u32 type;
} __packed;

struct pci_child_message {
	struct pci_message message_type;
	union win_slot_encoding wslot;
} __packed;

struct pci_incoming_message {
	struct vmpacket_descriptor hdr;
	struct pci_message message_type;
} __packed;

struct pci_response {
	struct vmpacket_descriptor hdr;
	s32 status;			 
} __packed;

struct pci_packet {
	void (*completion_func)(void *context, struct pci_response *resp,
				int resp_packet_size);
	void *compl_ctxt;

	struct pci_message message[];
};

 

 

struct pci_version_request {
	struct pci_message message_type;
	u32 protocol_version;
} __packed;

 

struct pci_bus_d0_entry {
	struct pci_message message_type;
	u32 reserved;
	u64 mmio_base;
} __packed;

struct pci_bus_relations {
	struct pci_incoming_message incoming;
	u32 device_count;
	struct pci_function_description func[];
} __packed;

struct pci_bus_relations2 {
	struct pci_incoming_message incoming;
	u32 device_count;
	struct pci_function_description2 func[];
} __packed;

struct pci_q_res_req_response {
	struct vmpacket_descriptor hdr;
	s32 status;			 
	u32 probed_bar[PCI_STD_NUM_BARS];
} __packed;

struct pci_set_power {
	struct pci_message message_type;
	union win_slot_encoding wslot;
	u32 power_state;		 
	u32 reserved;
} __packed;

struct pci_set_power_response {
	struct vmpacket_descriptor hdr;
	s32 status;			 
	union win_slot_encoding wslot;
	u32 resultant_state;		 
	u32 reserved;
} __packed;

struct pci_resources_assigned {
	struct pci_message message_type;
	union win_slot_encoding wslot;
	u8 memory_range[0x14][6];	 
	u32 msi_descriptors;
	u32 reserved[4];
} __packed;

struct pci_resources_assigned2 {
	struct pci_message message_type;
	union win_slot_encoding wslot;
	u8 memory_range[0x14][6];	 
	u32 msi_descriptor_count;
	u8 reserved[70];
} __packed;

struct pci_create_interrupt {
	struct pci_message message_type;
	union win_slot_encoding wslot;
	struct hv_msi_desc int_desc;
} __packed;

struct pci_create_int_response {
	struct pci_response response;
	u32 reserved;
	struct tran_int_desc int_desc;
} __packed;

struct pci_create_interrupt2 {
	struct pci_message message_type;
	union win_slot_encoding wslot;
	struct hv_msi_desc2 int_desc;
} __packed;

struct pci_create_interrupt3 {
	struct pci_message message_type;
	union win_slot_encoding wslot;
	struct hv_msi_desc3 int_desc;
} __packed;

struct pci_delete_interrupt {
	struct pci_message message_type;
	union win_slot_encoding wslot;
	struct tran_int_desc int_desc;
} __packed;

 
struct pci_read_block {
	struct pci_message message_type;
	u32 block_id;
	union win_slot_encoding wslot;
	u32 bytes_requested;
} __packed;

struct pci_read_block_response {
	struct vmpacket_descriptor hdr;
	u32 status;
	u8 bytes[HV_CONFIG_BLOCK_SIZE_MAX];
} __packed;

 
struct pci_write_block {
	struct pci_message message_type;
	u32 block_id;
	union win_slot_encoding wslot;
	u32 byte_count;
	u8 bytes[HV_CONFIG_BLOCK_SIZE_MAX];
} __packed;

struct pci_dev_inval_block {
	struct pci_incoming_message incoming;
	union win_slot_encoding wslot;
	u64 block_mask;
} __packed;

struct pci_dev_incoming {
	struct pci_incoming_message incoming;
	union win_slot_encoding wslot;
} __packed;

struct pci_eject_response {
	struct pci_message message_type;
	union win_slot_encoding wslot;
	u32 status;
} __packed;

static int pci_ring_size = (4 * PAGE_SIZE);

 

enum hv_pcibus_state {
	hv_pcibus_init = 0,
	hv_pcibus_probed,
	hv_pcibus_installed,
	hv_pcibus_removing,
	hv_pcibus_maximum
};

struct hv_pcibus_device {
#ifdef CONFIG_X86
	struct pci_sysdata sysdata;
#elif defined(CONFIG_ARM64)
	struct pci_config_window sysdata;
#endif
	struct pci_host_bridge *bridge;
	struct fwnode_handle *fwnode;
	 
	enum pci_protocol_version_t protocol_version;

	struct mutex state_lock;
	enum hv_pcibus_state state;

	struct hv_device *hdev;
	resource_size_t low_mmio_space;
	resource_size_t high_mmio_space;
	struct resource *mem_config;
	struct resource *low_mmio_res;
	struct resource *high_mmio_res;
	struct completion *survey_event;
	struct pci_bus *pci_bus;
	spinlock_t config_lock;	 
	spinlock_t device_list_lock;	 
	void __iomem *cfg_addr;

	struct list_head children;
	struct list_head dr_list;

	struct msi_domain_info msi_info;
	struct irq_domain *irq_domain;

	struct workqueue_struct *wq;

	 
	int wslot_res_allocated;
	bool use_calls;  
};

 
struct hv_dr_work {
	struct work_struct wrk;
	struct hv_pcibus_device *bus;
};

struct hv_pcidev_description {
	u16	v_id;	 
	u16	d_id;	 
	u8	rev;
	u8	prog_intf;
	u8	subclass;
	u8	base_class;
	u32	subsystem_id;
	union	win_slot_encoding win_slot;
	u32	ser;	 
	u32	flags;
	u16	virtual_numa_node;
};

struct hv_dr_state {
	struct list_head list_entry;
	u32 device_count;
	struct hv_pcidev_description func[];
};

struct hv_pci_dev {
	 
	struct list_head list_entry;
	refcount_t refs;
	struct pci_slot *pci_slot;
	struct hv_pcidev_description desc;
	bool reported_missing;
	struct hv_pcibus_device *hbus;
	struct work_struct wrk;

	void (*block_invalidate)(void *context, u64 block_mask);
	void *invalidate_context;

	 
	u32 probed_bar[PCI_STD_NUM_BARS];
};

struct hv_pci_compl {
	struct completion host_event;
	s32 completion_status;
};

static void hv_pci_onchannelcallback(void *context);

#ifdef CONFIG_X86
#define DELIVERY_MODE	APIC_DELIVERY_MODE_FIXED
#define FLOW_HANDLER	handle_edge_irq
#define FLOW_NAME	"edge"

static int hv_pci_irqchip_init(void)
{
	return 0;
}

static struct irq_domain *hv_pci_get_root_domain(void)
{
	return x86_vector_domain;
}

static unsigned int hv_msi_get_int_vector(struct irq_data *data)
{
	struct irq_cfg *cfg = irqd_cfg(data);

	return cfg->vector;
}

#define hv_msi_prepare		pci_msi_prepare

 
static void hv_arch_irq_unmask(struct irq_data *data)
{
	struct msi_desc *msi_desc = irq_data_get_msi_desc(data);
	struct hv_retarget_device_interrupt *params;
	struct tran_int_desc *int_desc;
	struct hv_pcibus_device *hbus;
	const struct cpumask *dest;
	cpumask_var_t tmp;
	struct pci_bus *pbus;
	struct pci_dev *pdev;
	unsigned long flags;
	u32 var_size = 0;
	int cpu, nr_bank;
	u64 res;

	dest = irq_data_get_effective_affinity_mask(data);
	pdev = msi_desc_to_pci_dev(msi_desc);
	pbus = pdev->bus;
	hbus = container_of(pbus->sysdata, struct hv_pcibus_device, sysdata);
	int_desc = data->chip_data;
	if (!int_desc) {
		dev_warn(&hbus->hdev->device, "%s() can not unmask irq %u\n",
			 __func__, data->irq);
		return;
	}

	local_irq_save(flags);

	params = *this_cpu_ptr(hyperv_pcpu_input_arg);
	memset(params, 0, sizeof(*params));
	params->partition_id = HV_PARTITION_ID_SELF;
	params->int_entry.source = HV_INTERRUPT_SOURCE_MSI;
	params->int_entry.msi_entry.address.as_uint32 = int_desc->address & 0xffffffff;
	params->int_entry.msi_entry.data.as_uint32 = int_desc->data;
	params->device_id = (hbus->hdev->dev_instance.b[5] << 24) |
			   (hbus->hdev->dev_instance.b[4] << 16) |
			   (hbus->hdev->dev_instance.b[7] << 8) |
			   (hbus->hdev->dev_instance.b[6] & 0xf8) |
			   PCI_FUNC(pdev->devfn);
	params->int_target.vector = hv_msi_get_int_vector(data);

	 

	if (hbus->protocol_version >= PCI_PROTOCOL_VERSION_1_2) {
		 
		params->int_target.flags |=
			HV_DEVICE_INTERRUPT_TARGET_PROCESSOR_SET;

		if (!alloc_cpumask_var(&tmp, GFP_ATOMIC)) {
			res = 1;
			goto out;
		}

		cpumask_and(tmp, dest, cpu_online_mask);
		nr_bank = cpumask_to_vpset(&params->int_target.vp_set, tmp);
		free_cpumask_var(tmp);

		if (nr_bank <= 0) {
			res = 1;
			goto out;
		}

		 
		var_size = 1 + nr_bank;
	} else {
		for_each_cpu_and(cpu, dest, cpu_online_mask) {
			params->int_target.vp_mask |=
				(1ULL << hv_cpu_number_to_vp_number(cpu));
		}
	}

	res = hv_do_hypercall(HVCALL_RETARGET_INTERRUPT | (var_size << 17),
			      params, NULL);

out:
	local_irq_restore(flags);

	 
	if (!hv_result_success(res) && hbus->state != hv_pcibus_removing)
		dev_err(&hbus->hdev->device,
			"%s() failed: %#llx", __func__, res);
}
#elif defined(CONFIG_ARM64)
 
#define HV_PCI_MSI_SPI_START	64
#define HV_PCI_MSI_SPI_NR	(1020 - HV_PCI_MSI_SPI_START)
#define DELIVERY_MODE		0
#define FLOW_HANDLER		NULL
#define FLOW_NAME		NULL
#define hv_msi_prepare		NULL

struct hv_pci_chip_data {
	DECLARE_BITMAP(spi_map, HV_PCI_MSI_SPI_NR);
	struct mutex	map_lock;
};

 
static struct irq_domain *hv_msi_gic_irq_domain;

 
static struct irq_chip hv_arm64_msi_irq_chip = {
	.name = "MSI",
	.irq_set_affinity = irq_chip_set_affinity_parent,
	.irq_eoi = irq_chip_eoi_parent,
	.irq_mask = irq_chip_mask_parent,
	.irq_unmask = irq_chip_unmask_parent
};

static unsigned int hv_msi_get_int_vector(struct irq_data *irqd)
{
	return irqd->parent_data->hwirq;
}

 
static void hv_pci_vec_irq_free(struct irq_domain *domain,
				unsigned int virq,
				unsigned int nr_bm_irqs,
				unsigned int nr_dom_irqs)
{
	struct hv_pci_chip_data *chip_data = domain->host_data;
	struct irq_data *d = irq_domain_get_irq_data(domain, virq);
	int first = d->hwirq - HV_PCI_MSI_SPI_START;
	int i;

	mutex_lock(&chip_data->map_lock);
	bitmap_release_region(chip_data->spi_map,
			      first,
			      get_count_order(nr_bm_irqs));
	mutex_unlock(&chip_data->map_lock);
	for (i = 0; i < nr_dom_irqs; i++) {
		if (i)
			d = irq_domain_get_irq_data(domain, virq + i);
		irq_domain_reset_irq_data(d);
	}

	irq_domain_free_irqs_parent(domain, virq, nr_dom_irqs);
}

static void hv_pci_vec_irq_domain_free(struct irq_domain *domain,
				       unsigned int virq,
				       unsigned int nr_irqs)
{
	hv_pci_vec_irq_free(domain, virq, nr_irqs, nr_irqs);
}

static int hv_pci_vec_alloc_device_irq(struct irq_domain *domain,
				       unsigned int nr_irqs,
				       irq_hw_number_t *hwirq)
{
	struct hv_pci_chip_data *chip_data = domain->host_data;
	int index;

	 
	mutex_lock(&chip_data->map_lock);
	index = bitmap_find_free_region(chip_data->spi_map,
					HV_PCI_MSI_SPI_NR,
					get_count_order(nr_irqs));
	mutex_unlock(&chip_data->map_lock);
	if (index < 0)
		return -ENOSPC;

	*hwirq = index + HV_PCI_MSI_SPI_START;

	return 0;
}

static int hv_pci_vec_irq_gic_domain_alloc(struct irq_domain *domain,
					   unsigned int virq,
					   irq_hw_number_t hwirq)
{
	struct irq_fwspec fwspec;
	struct irq_data *d;
	int ret;

	fwspec.fwnode = domain->parent->fwnode;
	fwspec.param_count = 2;
	fwspec.param[0] = hwirq;
	fwspec.param[1] = IRQ_TYPE_EDGE_RISING;

	ret = irq_domain_alloc_irqs_parent(domain, virq, 1, &fwspec);
	if (ret)
		return ret;

	 
	d = irq_domain_get_irq_data(domain->parent, virq);

	return d->chip->irq_set_type(d, IRQ_TYPE_EDGE_RISING);
}

static int hv_pci_vec_irq_domain_alloc(struct irq_domain *domain,
				       unsigned int virq, unsigned int nr_irqs,
				       void *args)
{
	irq_hw_number_t hwirq;
	unsigned int i;
	int ret;

	ret = hv_pci_vec_alloc_device_irq(domain, nr_irqs, &hwirq);
	if (ret)
		return ret;

	for (i = 0; i < nr_irqs; i++) {
		ret = hv_pci_vec_irq_gic_domain_alloc(domain, virq + i,
						      hwirq + i);
		if (ret) {
			hv_pci_vec_irq_free(domain, virq, nr_irqs, i);
			return ret;
		}

		irq_domain_set_hwirq_and_chip(domain, virq + i,
					      hwirq + i,
					      &hv_arm64_msi_irq_chip,
					      domain->host_data);
		pr_debug("pID:%d vID:%u\n", (int)(hwirq + i), virq + i);
	}

	return 0;
}

 
static int hv_pci_vec_irq_domain_activate(struct irq_domain *domain,
					  struct irq_data *irqd, bool reserve)
{
	int cpu = cpumask_first(cpu_present_mask);

	irq_data_update_effective_affinity(irqd, cpumask_of(cpu));

	return 0;
}

static const struct irq_domain_ops hv_pci_domain_ops = {
	.alloc	= hv_pci_vec_irq_domain_alloc,
	.free	= hv_pci_vec_irq_domain_free,
	.activate = hv_pci_vec_irq_domain_activate,
};

static int hv_pci_irqchip_init(void)
{
	static struct hv_pci_chip_data *chip_data;
	struct fwnode_handle *fn = NULL;
	int ret = -ENOMEM;

	chip_data = kzalloc(sizeof(*chip_data), GFP_KERNEL);
	if (!chip_data)
		return ret;

	mutex_init(&chip_data->map_lock);
	fn = irq_domain_alloc_named_fwnode("hv_vpci_arm64");
	if (!fn)
		goto free_chip;

	 
	hv_msi_gic_irq_domain = acpi_irq_create_hierarchy(0, HV_PCI_MSI_SPI_NR,
							  fn, &hv_pci_domain_ops,
							  chip_data);

	if (!hv_msi_gic_irq_domain) {
		pr_err("Failed to create Hyper-V arm64 vPCI MSI IRQ domain\n");
		goto free_chip;
	}

	return 0;

free_chip:
	kfree(chip_data);
	if (fn)
		irq_domain_free_fwnode(fn);

	return ret;
}

static struct irq_domain *hv_pci_get_root_domain(void)
{
	return hv_msi_gic_irq_domain;
}

 
static void hv_arch_irq_unmask(struct irq_data *data) { }
#endif  

 
static void hv_pci_generic_compl(void *context, struct pci_response *resp,
				 int resp_packet_size)
{
	struct hv_pci_compl *comp_pkt = context;

	comp_pkt->completion_status = resp->status;
	complete(&comp_pkt->host_event);
}

static struct hv_pci_dev *get_pcichild_wslot(struct hv_pcibus_device *hbus,
						u32 wslot);

static void get_pcichild(struct hv_pci_dev *hpdev)
{
	refcount_inc(&hpdev->refs);
}

static void put_pcichild(struct hv_pci_dev *hpdev)
{
	if (refcount_dec_and_test(&hpdev->refs))
		kfree(hpdev);
}

 
static int wait_for_response(struct hv_device *hdev,
			     struct completion *comp)
{
	while (true) {
		if (hdev->channel->rescind) {
			dev_warn_once(&hdev->device, "The device is gone.\n");
			return -ENODEV;
		}

		if (wait_for_completion_timeout(comp, HZ / 10))
			break;
	}

	return 0;
}

 
static u32 devfn_to_wslot(int devfn)
{
	union win_slot_encoding wslot;

	wslot.slot = 0;
	wslot.bits.dev = PCI_SLOT(devfn);
	wslot.bits.func = PCI_FUNC(devfn);

	return wslot.slot;
}

 
static int wslot_to_devfn(u32 wslot)
{
	union win_slot_encoding slot_no;

	slot_no.slot = wslot;
	return PCI_DEVFN(slot_no.bits.dev, slot_no.bits.func);
}

static void hv_pci_read_mmio(struct device *dev, phys_addr_t gpa, int size, u32 *val)
{
	struct hv_mmio_read_input *in;
	struct hv_mmio_read_output *out;
	u64 ret;

	 
	in = *this_cpu_ptr(hyperv_pcpu_input_arg);
	out = *this_cpu_ptr(hyperv_pcpu_input_arg) + sizeof(*in);
	in->gpa = gpa;
	in->size = size;

	ret = hv_do_hypercall(HVCALL_MMIO_READ, in, out);
	if (hv_result_success(ret)) {
		switch (size) {
		case 1:
			*val = *(u8 *)(out->data);
			break;
		case 2:
			*val = *(u16 *)(out->data);
			break;
		default:
			*val = *(u32 *)(out->data);
			break;
		}
	} else
		dev_err(dev, "MMIO read hypercall error %llx addr %llx size %d\n",
				ret, gpa, size);
}

static void hv_pci_write_mmio(struct device *dev, phys_addr_t gpa, int size, u32 val)
{
	struct hv_mmio_write_input *in;
	u64 ret;

	 
	in = *this_cpu_ptr(hyperv_pcpu_input_arg);
	in->gpa = gpa;
	in->size = size;
	switch (size) {
	case 1:
		*(u8 *)(in->data) = val;
		break;
	case 2:
		*(u16 *)(in->data) = val;
		break;
	default:
		*(u32 *)(in->data) = val;
		break;
	}

	ret = hv_do_hypercall(HVCALL_MMIO_WRITE, in, NULL);
	if (!hv_result_success(ret))
		dev_err(dev, "MMIO write hypercall error %llx addr %llx size %d\n",
				ret, gpa, size);
}

 

 
static void _hv_pcifront_read_config(struct hv_pci_dev *hpdev, int where,
				     int size, u32 *val)
{
	struct hv_pcibus_device *hbus = hpdev->hbus;
	struct device *dev = &hbus->hdev->device;
	int offset = where + CFG_PAGE_OFFSET;
	unsigned long flags;

	 
	if (where + size <= PCI_COMMAND) {
		memcpy(val, ((u8 *)&hpdev->desc.v_id) + where, size);
	} else if (where >= PCI_CLASS_REVISION && where + size <=
		   PCI_CACHE_LINE_SIZE) {
		memcpy(val, ((u8 *)&hpdev->desc.rev) + where -
		       PCI_CLASS_REVISION, size);
	} else if (where >= PCI_SUBSYSTEM_VENDOR_ID && where + size <=
		   PCI_ROM_ADDRESS) {
		memcpy(val, (u8 *)&hpdev->desc.subsystem_id + where -
		       PCI_SUBSYSTEM_VENDOR_ID, size);
	} else if (where >= PCI_ROM_ADDRESS && where + size <=
		   PCI_CAPABILITY_LIST) {
		 
		*val = 0;
	} else if (where >= PCI_INTERRUPT_LINE && where + size <=
		   PCI_INTERRUPT_PIN) {
		 
		*val = 0;
	} else if (where + size <= CFG_PAGE_SIZE) {

		spin_lock_irqsave(&hbus->config_lock, flags);
		if (hbus->use_calls) {
			phys_addr_t addr = hbus->mem_config->start + offset;

			hv_pci_write_mmio(dev, hbus->mem_config->start, 4,
						hpdev->desc.win_slot.slot);
			hv_pci_read_mmio(dev, addr, size, val);
		} else {
			void __iomem *addr = hbus->cfg_addr + offset;

			 
			writel(hpdev->desc.win_slot.slot, hbus->cfg_addr);
			 
			mb();
			 
			switch (size) {
			case 1:
				*val = readb(addr);
				break;
			case 2:
				*val = readw(addr);
				break;
			default:
				*val = readl(addr);
				break;
			}
			 
			mb();
		}
		spin_unlock_irqrestore(&hbus->config_lock, flags);
	} else {
		dev_err(dev, "Attempt to read beyond a function's config space.\n");
	}
}

static u16 hv_pcifront_get_vendor_id(struct hv_pci_dev *hpdev)
{
	struct hv_pcibus_device *hbus = hpdev->hbus;
	struct device *dev = &hbus->hdev->device;
	u32 val;
	u16 ret;
	unsigned long flags;

	spin_lock_irqsave(&hbus->config_lock, flags);

	if (hbus->use_calls) {
		phys_addr_t addr = hbus->mem_config->start +
					 CFG_PAGE_OFFSET + PCI_VENDOR_ID;

		hv_pci_write_mmio(dev, hbus->mem_config->start, 4,
					hpdev->desc.win_slot.slot);
		hv_pci_read_mmio(dev, addr, 2, &val);
		ret = val;   
	} else {
		void __iomem *addr = hbus->cfg_addr + CFG_PAGE_OFFSET +
					     PCI_VENDOR_ID;
		 
		writel(hpdev->desc.win_slot.slot, hbus->cfg_addr);
		 
		mb();
		 
		ret = readw(addr);
		 
	}

	spin_unlock_irqrestore(&hbus->config_lock, flags);

	return ret;
}

 
static void _hv_pcifront_write_config(struct hv_pci_dev *hpdev, int where,
				      int size, u32 val)
{
	struct hv_pcibus_device *hbus = hpdev->hbus;
	struct device *dev = &hbus->hdev->device;
	int offset = where + CFG_PAGE_OFFSET;
	unsigned long flags;

	if (where >= PCI_SUBSYSTEM_VENDOR_ID &&
	    where + size <= PCI_CAPABILITY_LIST) {
		 
	} else if (where >= PCI_COMMAND && where + size <= CFG_PAGE_SIZE) {
		spin_lock_irqsave(&hbus->config_lock, flags);

		if (hbus->use_calls) {
			phys_addr_t addr = hbus->mem_config->start + offset;

			hv_pci_write_mmio(dev, hbus->mem_config->start, 4,
						hpdev->desc.win_slot.slot);
			hv_pci_write_mmio(dev, addr, size, val);
		} else {
			void __iomem *addr = hbus->cfg_addr + offset;

			 
			writel(hpdev->desc.win_slot.slot, hbus->cfg_addr);
			 
			wmb();
			 
			switch (size) {
			case 1:
				writeb(val, addr);
				break;
			case 2:
				writew(val, addr);
				break;
			default:
				writel(val, addr);
				break;
			}
			 
			mb();
		}
		spin_unlock_irqrestore(&hbus->config_lock, flags);
	} else {
		dev_err(dev, "Attempt to write beyond a function's config space.\n");
	}
}

 
static int hv_pcifront_read_config(struct pci_bus *bus, unsigned int devfn,
				   int where, int size, u32 *val)
{
	struct hv_pcibus_device *hbus =
		container_of(bus->sysdata, struct hv_pcibus_device, sysdata);
	struct hv_pci_dev *hpdev;

	hpdev = get_pcichild_wslot(hbus, devfn_to_wslot(devfn));
	if (!hpdev)
		return PCIBIOS_DEVICE_NOT_FOUND;

	_hv_pcifront_read_config(hpdev, where, size, val);

	put_pcichild(hpdev);
	return PCIBIOS_SUCCESSFUL;
}

 
static int hv_pcifront_write_config(struct pci_bus *bus, unsigned int devfn,
				    int where, int size, u32 val)
{
	struct hv_pcibus_device *hbus =
	    container_of(bus->sysdata, struct hv_pcibus_device, sysdata);
	struct hv_pci_dev *hpdev;

	hpdev = get_pcichild_wslot(hbus, devfn_to_wslot(devfn));
	if (!hpdev)
		return PCIBIOS_DEVICE_NOT_FOUND;

	_hv_pcifront_write_config(hpdev, where, size, val);

	put_pcichild(hpdev);
	return PCIBIOS_SUCCESSFUL;
}

 
static struct pci_ops hv_pcifront_ops = {
	.read  = hv_pcifront_read_config,
	.write = hv_pcifront_write_config,
};

 

struct hv_read_config_compl {
	struct hv_pci_compl comp_pkt;
	void *buf;
	unsigned int len;
	unsigned int bytes_returned;
};

 
static void hv_pci_read_config_compl(void *context, struct pci_response *resp,
				     int resp_packet_size)
{
	struct hv_read_config_compl *comp = context;
	struct pci_read_block_response *read_resp =
		(struct pci_read_block_response *)resp;
	unsigned int data_len, hdr_len;

	hdr_len = offsetof(struct pci_read_block_response, bytes);
	if (resp_packet_size < hdr_len) {
		comp->comp_pkt.completion_status = -1;
		goto out;
	}

	data_len = resp_packet_size - hdr_len;
	if (data_len > 0 && read_resp->status == 0) {
		comp->bytes_returned = min(comp->len, data_len);
		memcpy(comp->buf, read_resp->bytes, comp->bytes_returned);
	} else {
		comp->bytes_returned = 0;
	}

	comp->comp_pkt.completion_status = read_resp->status;
out:
	complete(&comp->comp_pkt.host_event);
}

 
static int hv_read_config_block(struct pci_dev *pdev, void *buf,
				unsigned int len, unsigned int block_id,
				unsigned int *bytes_returned)
{
	struct hv_pcibus_device *hbus =
		container_of(pdev->bus->sysdata, struct hv_pcibus_device,
			     sysdata);
	struct {
		struct pci_packet pkt;
		char buf[sizeof(struct pci_read_block)];
	} pkt;
	struct hv_read_config_compl comp_pkt;
	struct pci_read_block *read_blk;
	int ret;

	if (len == 0 || len > HV_CONFIG_BLOCK_SIZE_MAX)
		return -EINVAL;

	init_completion(&comp_pkt.comp_pkt.host_event);
	comp_pkt.buf = buf;
	comp_pkt.len = len;

	memset(&pkt, 0, sizeof(pkt));
	pkt.pkt.completion_func = hv_pci_read_config_compl;
	pkt.pkt.compl_ctxt = &comp_pkt;
	read_blk = (struct pci_read_block *)&pkt.pkt.message;
	read_blk->message_type.type = PCI_READ_BLOCK;
	read_blk->wslot.slot = devfn_to_wslot(pdev->devfn);
	read_blk->block_id = block_id;
	read_blk->bytes_requested = len;

	ret = vmbus_sendpacket(hbus->hdev->channel, read_blk,
			       sizeof(*read_blk), (unsigned long)&pkt.pkt,
			       VM_PKT_DATA_INBAND,
			       VMBUS_DATA_PACKET_FLAG_COMPLETION_REQUESTED);
	if (ret)
		return ret;

	ret = wait_for_response(hbus->hdev, &comp_pkt.comp_pkt.host_event);
	if (ret)
		return ret;

	if (comp_pkt.comp_pkt.completion_status != 0 ||
	    comp_pkt.bytes_returned == 0) {
		dev_err(&hbus->hdev->device,
			"Read Config Block failed: 0x%x, bytes_returned=%d\n",
			comp_pkt.comp_pkt.completion_status,
			comp_pkt.bytes_returned);
		return -EIO;
	}

	*bytes_returned = comp_pkt.bytes_returned;
	return 0;
}

 
static void hv_pci_write_config_compl(void *context, struct pci_response *resp,
				      int resp_packet_size)
{
	struct hv_pci_compl *comp_pkt = context;

	comp_pkt->completion_status = resp->status;
	complete(&comp_pkt->host_event);
}

 
static int hv_write_config_block(struct pci_dev *pdev, void *buf,
				unsigned int len, unsigned int block_id)
{
	struct hv_pcibus_device *hbus =
		container_of(pdev->bus->sysdata, struct hv_pcibus_device,
			     sysdata);
	struct {
		struct pci_packet pkt;
		char buf[sizeof(struct pci_write_block)];
		u32 reserved;
	} pkt;
	struct hv_pci_compl comp_pkt;
	struct pci_write_block *write_blk;
	u32 pkt_size;
	int ret;

	if (len == 0 || len > HV_CONFIG_BLOCK_SIZE_MAX)
		return -EINVAL;

	init_completion(&comp_pkt.host_event);

	memset(&pkt, 0, sizeof(pkt));
	pkt.pkt.completion_func = hv_pci_write_config_compl;
	pkt.pkt.compl_ctxt = &comp_pkt;
	write_blk = (struct pci_write_block *)&pkt.pkt.message;
	write_blk->message_type.type = PCI_WRITE_BLOCK;
	write_blk->wslot.slot = devfn_to_wslot(pdev->devfn);
	write_blk->block_id = block_id;
	write_blk->byte_count = len;
	memcpy(write_blk->bytes, buf, len);
	pkt_size = offsetof(struct pci_write_block, bytes) + len;
	 
	pkt_size += sizeof(pkt.reserved);

	ret = vmbus_sendpacket(hbus->hdev->channel, write_blk, pkt_size,
			       (unsigned long)&pkt.pkt, VM_PKT_DATA_INBAND,
			       VMBUS_DATA_PACKET_FLAG_COMPLETION_REQUESTED);
	if (ret)
		return ret;

	ret = wait_for_response(hbus->hdev, &comp_pkt.host_event);
	if (ret)
		return ret;

	if (comp_pkt.completion_status != 0) {
		dev_err(&hbus->hdev->device,
			"Write Config Block failed: 0x%x\n",
			comp_pkt.completion_status);
		return -EIO;
	}

	return 0;
}

 
static int hv_register_block_invalidate(struct pci_dev *pdev, void *context,
					void (*block_invalidate)(void *context,
								 u64 block_mask))
{
	struct hv_pcibus_device *hbus =
		container_of(pdev->bus->sysdata, struct hv_pcibus_device,
			     sysdata);
	struct hv_pci_dev *hpdev;

	hpdev = get_pcichild_wslot(hbus, devfn_to_wslot(pdev->devfn));
	if (!hpdev)
		return -ENODEV;

	hpdev->block_invalidate = block_invalidate;
	hpdev->invalidate_context = context;

	put_pcichild(hpdev);
	return 0;

}

 
static void hv_int_desc_free(struct hv_pci_dev *hpdev,
			     struct tran_int_desc *int_desc)
{
	struct pci_delete_interrupt *int_pkt;
	struct {
		struct pci_packet pkt;
		u8 buffer[sizeof(struct pci_delete_interrupt)];
	} ctxt;

	if (!int_desc->vector_count) {
		kfree(int_desc);
		return;
	}
	memset(&ctxt, 0, sizeof(ctxt));
	int_pkt = (struct pci_delete_interrupt *)&ctxt.pkt.message;
	int_pkt->message_type.type =
		PCI_DELETE_INTERRUPT_MESSAGE;
	int_pkt->wslot.slot = hpdev->desc.win_slot.slot;
	int_pkt->int_desc = *int_desc;
	vmbus_sendpacket(hpdev->hbus->hdev->channel, int_pkt, sizeof(*int_pkt),
			 0, VM_PKT_DATA_INBAND, 0);
	kfree(int_desc);
}

 
static void hv_msi_free(struct irq_domain *domain, struct msi_domain_info *info,
			unsigned int irq)
{
	struct hv_pcibus_device *hbus;
	struct hv_pci_dev *hpdev;
	struct pci_dev *pdev;
	struct tran_int_desc *int_desc;
	struct irq_data *irq_data = irq_domain_get_irq_data(domain, irq);
	struct msi_desc *msi = irq_data_get_msi_desc(irq_data);

	pdev = msi_desc_to_pci_dev(msi);
	hbus = info->data;
	int_desc = irq_data_get_irq_chip_data(irq_data);
	if (!int_desc)
		return;

	irq_data->chip_data = NULL;
	hpdev = get_pcichild_wslot(hbus, devfn_to_wslot(pdev->devfn));
	if (!hpdev) {
		kfree(int_desc);
		return;
	}

	hv_int_desc_free(hpdev, int_desc);
	put_pcichild(hpdev);
}

static void hv_irq_mask(struct irq_data *data)
{
	pci_msi_mask_irq(data);
	if (data->parent_data->chip->irq_mask)
		irq_chip_mask_parent(data);
}

static void hv_irq_unmask(struct irq_data *data)
{
	hv_arch_irq_unmask(data);

	if (data->parent_data->chip->irq_unmask)
		irq_chip_unmask_parent(data);
	pci_msi_unmask_irq(data);
}

struct compose_comp_ctxt {
	struct hv_pci_compl comp_pkt;
	struct tran_int_desc int_desc;
};

static void hv_pci_compose_compl(void *context, struct pci_response *resp,
				 int resp_packet_size)
{
	struct compose_comp_ctxt *comp_pkt = context;
	struct pci_create_int_response *int_resp =
		(struct pci_create_int_response *)resp;

	if (resp_packet_size < sizeof(*int_resp)) {
		comp_pkt->comp_pkt.completion_status = -1;
		goto out;
	}
	comp_pkt->comp_pkt.completion_status = resp->status;
	comp_pkt->int_desc = int_resp->int_desc;
out:
	complete(&comp_pkt->comp_pkt.host_event);
}

static u32 hv_compose_msi_req_v1(
	struct pci_create_interrupt *int_pkt,
	u32 slot, u8 vector, u16 vector_count)
{
	int_pkt->message_type.type = PCI_CREATE_INTERRUPT_MESSAGE;
	int_pkt->wslot.slot = slot;
	int_pkt->int_desc.vector = vector;
	int_pkt->int_desc.vector_count = vector_count;
	int_pkt->int_desc.delivery_mode = DELIVERY_MODE;

	 
	int_pkt->int_desc.cpu_mask = CPU_AFFINITY_ALL;

	return sizeof(*int_pkt);
}

 

 
static int hv_compose_msi_req_get_cpu(const struct cpumask *affinity)
{
	return cpumask_first_and(affinity, cpu_online_mask);
}

 
static int hv_compose_multi_msi_req_get_cpu(void)
{
	static DEFINE_SPINLOCK(multi_msi_cpu_lock);

	 
	static int cpu_next = -1;

	unsigned long flags;
	int cpu;

	spin_lock_irqsave(&multi_msi_cpu_lock, flags);

	cpu_next = cpumask_next_wrap(cpu_next, cpu_online_mask, nr_cpu_ids,
				     false);
	cpu = cpu_next;

	spin_unlock_irqrestore(&multi_msi_cpu_lock, flags);

	return cpu;
}

static u32 hv_compose_msi_req_v2(
	struct pci_create_interrupt2 *int_pkt, int cpu,
	u32 slot, u8 vector, u16 vector_count)
{
	int_pkt->message_type.type = PCI_CREATE_INTERRUPT_MESSAGE2;
	int_pkt->wslot.slot = slot;
	int_pkt->int_desc.vector = vector;
	int_pkt->int_desc.vector_count = vector_count;
	int_pkt->int_desc.delivery_mode = DELIVERY_MODE;
	int_pkt->int_desc.processor_array[0] =
		hv_cpu_number_to_vp_number(cpu);
	int_pkt->int_desc.processor_count = 1;

	return sizeof(*int_pkt);
}

static u32 hv_compose_msi_req_v3(
	struct pci_create_interrupt3 *int_pkt, int cpu,
	u32 slot, u32 vector, u16 vector_count)
{
	int_pkt->message_type.type = PCI_CREATE_INTERRUPT_MESSAGE3;
	int_pkt->wslot.slot = slot;
	int_pkt->int_desc.vector = vector;
	int_pkt->int_desc.reserved = 0;
	int_pkt->int_desc.vector_count = vector_count;
	int_pkt->int_desc.delivery_mode = DELIVERY_MODE;
	int_pkt->int_desc.processor_array[0] =
		hv_cpu_number_to_vp_number(cpu);
	int_pkt->int_desc.processor_count = 1;

	return sizeof(*int_pkt);
}

 
static void hv_compose_msi_msg(struct irq_data *data, struct msi_msg *msg)
{
	struct hv_pcibus_device *hbus;
	struct vmbus_channel *channel;
	struct hv_pci_dev *hpdev;
	struct pci_bus *pbus;
	struct pci_dev *pdev;
	const struct cpumask *dest;
	struct compose_comp_ctxt comp;
	struct tran_int_desc *int_desc;
	struct msi_desc *msi_desc;
	 
	u16 vector_count;
	u32 vector;
	struct {
		struct pci_packet pci_pkt;
		union {
			struct pci_create_interrupt v1;
			struct pci_create_interrupt2 v2;
			struct pci_create_interrupt3 v3;
		} int_pkts;
	} __packed ctxt;
	bool multi_msi;
	u64 trans_id;
	u32 size;
	int ret;
	int cpu;

	msi_desc  = irq_data_get_msi_desc(data);
	multi_msi = !msi_desc->pci.msi_attrib.is_msix &&
		    msi_desc->nvec_used > 1;

	 
	if (data->chip_data && multi_msi) {
		int_desc = data->chip_data;
		msg->address_hi = int_desc->address >> 32;
		msg->address_lo = int_desc->address & 0xffffffff;
		msg->data = int_desc->data;
		return;
	}

	pdev = msi_desc_to_pci_dev(msi_desc);
	dest = irq_data_get_effective_affinity_mask(data);
	pbus = pdev->bus;
	hbus = container_of(pbus->sysdata, struct hv_pcibus_device, sysdata);
	channel = hbus->hdev->channel;
	hpdev = get_pcichild_wslot(hbus, devfn_to_wslot(pdev->devfn));
	if (!hpdev)
		goto return_null_message;

	 
	if (data->chip_data && !multi_msi) {
		int_desc = data->chip_data;
		data->chip_data = NULL;
		hv_int_desc_free(hpdev, int_desc);
	}

	int_desc = kzalloc(sizeof(*int_desc), GFP_ATOMIC);
	if (!int_desc)
		goto drop_reference;

	if (multi_msi) {
		 
		if (msi_desc->irq != data->irq) {
			data->chip_data = int_desc;
			int_desc->address = msi_desc->msg.address_lo |
					    (u64)msi_desc->msg.address_hi << 32;
			int_desc->data = msi_desc->msg.data +
					 (data->irq - msi_desc->irq);
			msg->address_hi = msi_desc->msg.address_hi;
			msg->address_lo = msi_desc->msg.address_lo;
			msg->data = int_desc->data;
			put_pcichild(hpdev);
			return;
		}
		 
		vector = 32;
		vector_count = msi_desc->nvec_used;
		cpu = hv_compose_multi_msi_req_get_cpu();
	} else {
		vector = hv_msi_get_int_vector(data);
		vector_count = 1;
		cpu = hv_compose_msi_req_get_cpu(dest);
	}

	 
	memset(&ctxt, 0, sizeof(ctxt));
	init_completion(&comp.comp_pkt.host_event);
	ctxt.pci_pkt.completion_func = hv_pci_compose_compl;
	ctxt.pci_pkt.compl_ctxt = &comp;

	switch (hbus->protocol_version) {
	case PCI_PROTOCOL_VERSION_1_1:
		size = hv_compose_msi_req_v1(&ctxt.int_pkts.v1,
					hpdev->desc.win_slot.slot,
					(u8)vector,
					vector_count);
		break;

	case PCI_PROTOCOL_VERSION_1_2:
	case PCI_PROTOCOL_VERSION_1_3:
		size = hv_compose_msi_req_v2(&ctxt.int_pkts.v2,
					cpu,
					hpdev->desc.win_slot.slot,
					(u8)vector,
					vector_count);
		break;

	case PCI_PROTOCOL_VERSION_1_4:
		size = hv_compose_msi_req_v3(&ctxt.int_pkts.v3,
					cpu,
					hpdev->desc.win_slot.slot,
					vector,
					vector_count);
		break;

	default:
		 
		dev_err(&hbus->hdev->device,
			"Unexpected vPCI protocol, update driver.");
		goto free_int_desc;
	}

	ret = vmbus_sendpacket_getid(hpdev->hbus->hdev->channel, &ctxt.int_pkts,
				     size, (unsigned long)&ctxt.pci_pkt,
				     &trans_id, VM_PKT_DATA_INBAND,
				     VMBUS_DATA_PACKET_FLAG_COMPLETION_REQUESTED);
	if (ret) {
		dev_err(&hbus->hdev->device,
			"Sending request for interrupt failed: 0x%x",
			comp.comp_pkt.completion_status);
		goto free_int_desc;
	}

	 
	tasklet_disable_in_atomic(&channel->callback_event);

	 
	while (!try_wait_for_completion(&comp.comp_pkt.host_event)) {
		unsigned long flags;

		 
		if (hv_pcifront_get_vendor_id(hpdev) == 0xFFFF) {
			dev_err_once(&hbus->hdev->device,
				     "the device has gone\n");
			goto enable_tasklet;
		}

		 
		spin_lock_irqsave(&channel->sched_lock, flags);
		if (unlikely(channel->onchannel_callback == NULL)) {
			spin_unlock_irqrestore(&channel->sched_lock, flags);
			goto enable_tasklet;
		}
		hv_pci_onchannelcallback(hbus);
		spin_unlock_irqrestore(&channel->sched_lock, flags);

		udelay(100);
	}

	tasklet_enable(&channel->callback_event);

	if (comp.comp_pkt.completion_status < 0) {
		dev_err(&hbus->hdev->device,
			"Request for interrupt failed: 0x%x",
			comp.comp_pkt.completion_status);
		goto free_int_desc;
	}

	 
	*int_desc = comp.int_desc;
	data->chip_data = int_desc;

	 
	msg->address_hi = comp.int_desc.address >> 32;
	msg->address_lo = comp.int_desc.address & 0xffffffff;
	msg->data = comp.int_desc.data;

	put_pcichild(hpdev);
	return;

enable_tasklet:
	tasklet_enable(&channel->callback_event);
	 
	vmbus_request_addr_match(channel, trans_id, (unsigned long)&ctxt.pci_pkt);
free_int_desc:
	kfree(int_desc);
drop_reference:
	put_pcichild(hpdev);
return_null_message:
	msg->address_hi = 0;
	msg->address_lo = 0;
	msg->data = 0;
}

 
static struct irq_chip hv_msi_irq_chip = {
	.name			= "Hyper-V PCIe MSI",
	.irq_compose_msi_msg	= hv_compose_msi_msg,
	.irq_set_affinity	= irq_chip_set_affinity_parent,
#ifdef CONFIG_X86
	.irq_ack		= irq_chip_ack_parent,
#elif defined(CONFIG_ARM64)
	.irq_eoi		= irq_chip_eoi_parent,
#endif
	.irq_mask		= hv_irq_mask,
	.irq_unmask		= hv_irq_unmask,
};

static struct msi_domain_ops hv_msi_ops = {
	.msi_prepare	= hv_msi_prepare,
	.msi_free	= hv_msi_free,
};

 
static int hv_pcie_init_irq_domain(struct hv_pcibus_device *hbus)
{
	hbus->msi_info.chip = &hv_msi_irq_chip;
	hbus->msi_info.ops = &hv_msi_ops;
	hbus->msi_info.flags = (MSI_FLAG_USE_DEF_DOM_OPS |
		MSI_FLAG_USE_DEF_CHIP_OPS | MSI_FLAG_MULTI_PCI_MSI |
		MSI_FLAG_PCI_MSIX);
	hbus->msi_info.handler = FLOW_HANDLER;
	hbus->msi_info.handler_name = FLOW_NAME;
	hbus->msi_info.data = hbus;
	hbus->irq_domain = pci_msi_create_irq_domain(hbus->fwnode,
						     &hbus->msi_info,
						     hv_pci_get_root_domain());
	if (!hbus->irq_domain) {
		dev_err(&hbus->hdev->device,
			"Failed to build an MSI IRQ domain\n");
		return -ENODEV;
	}

	dev_set_msi_domain(&hbus->bridge->dev, hbus->irq_domain);

	return 0;
}

 
static u64 get_bar_size(u64 bar_val)
{
	return round_up((1 + ~(bar_val & PCI_BASE_ADDRESS_MEM_MASK)),
			PAGE_SIZE);
}

 
static void survey_child_resources(struct hv_pcibus_device *hbus)
{
	struct hv_pci_dev *hpdev;
	resource_size_t bar_size = 0;
	unsigned long flags;
	struct completion *event;
	u64 bar_val;
	int i;

	 
	event = xchg(&hbus->survey_event, NULL);
	if (!event)
		return;

	 
	if (hbus->low_mmio_space || hbus->high_mmio_space) {
		complete(event);
		return;
	}

	spin_lock_irqsave(&hbus->device_list_lock, flags);

	 
	list_for_each_entry(hpdev, &hbus->children, list_entry) {
		for (i = 0; i < PCI_STD_NUM_BARS; i++) {
			if (hpdev->probed_bar[i] & PCI_BASE_ADDRESS_SPACE_IO)
				dev_err(&hbus->hdev->device,
					"There's an I/O BAR in this list!\n");

			if (hpdev->probed_bar[i] != 0) {
				 

				bar_val = hpdev->probed_bar[i];
				if (bar_val & PCI_BASE_ADDRESS_MEM_TYPE_64)
					bar_val |=
					((u64)hpdev->probed_bar[++i] << 32);
				else
					bar_val |= 0xffffffff00000000ULL;

				bar_size = get_bar_size(bar_val);

				if (bar_val & PCI_BASE_ADDRESS_MEM_TYPE_64)
					hbus->high_mmio_space += bar_size;
				else
					hbus->low_mmio_space += bar_size;
			}
		}
	}

	spin_unlock_irqrestore(&hbus->device_list_lock, flags);
	complete(event);
}

 
static void prepopulate_bars(struct hv_pcibus_device *hbus)
{
	resource_size_t high_size = 0;
	resource_size_t low_size = 0;
	resource_size_t high_base = 0;
	resource_size_t low_base = 0;
	resource_size_t bar_size;
	struct hv_pci_dev *hpdev;
	unsigned long flags;
	u64 bar_val;
	u32 command;
	bool high;
	int i;

	if (hbus->low_mmio_space) {
		low_size = 1ULL << (63 - __builtin_clzll(hbus->low_mmio_space));
		low_base = hbus->low_mmio_res->start;
	}

	if (hbus->high_mmio_space) {
		high_size = 1ULL <<
			(63 - __builtin_clzll(hbus->high_mmio_space));
		high_base = hbus->high_mmio_res->start;
	}

	spin_lock_irqsave(&hbus->device_list_lock, flags);

	 
	list_for_each_entry(hpdev, &hbus->children, list_entry) {
		_hv_pcifront_read_config(hpdev, PCI_COMMAND, 2, &command);
		command &= ~PCI_COMMAND_MEMORY;
		_hv_pcifront_write_config(hpdev, PCI_COMMAND, 2, command);
	}

	 
	do {
		list_for_each_entry(hpdev, &hbus->children, list_entry) {
			for (i = 0; i < PCI_STD_NUM_BARS; i++) {
				bar_val = hpdev->probed_bar[i];
				if (bar_val == 0)
					continue;
				high = bar_val & PCI_BASE_ADDRESS_MEM_TYPE_64;
				if (high) {
					bar_val |=
						((u64)hpdev->probed_bar[i + 1]
						 << 32);
				} else {
					bar_val |= 0xffffffffULL << 32;
				}
				bar_size = get_bar_size(bar_val);
				if (high) {
					if (high_size != bar_size) {
						i++;
						continue;
					}
					_hv_pcifront_write_config(hpdev,
						PCI_BASE_ADDRESS_0 + (4 * i),
						4,
						(u32)(high_base & 0xffffff00));
					i++;
					_hv_pcifront_write_config(hpdev,
						PCI_BASE_ADDRESS_0 + (4 * i),
						4, (u32)(high_base >> 32));
					high_base += bar_size;
				} else {
					if (low_size != bar_size)
						continue;
					_hv_pcifront_write_config(hpdev,
						PCI_BASE_ADDRESS_0 + (4 * i),
						4,
						(u32)(low_base & 0xffffff00));
					low_base += bar_size;
				}
			}
			if (high_size <= 1 && low_size <= 1) {
				 
				break;
			}
		}

		high_size >>= 1;
		low_size >>= 1;
	}  while (high_size || low_size);

	spin_unlock_irqrestore(&hbus->device_list_lock, flags);
}

 
static void hv_pci_assign_slots(struct hv_pcibus_device *hbus)
{
	struct hv_pci_dev *hpdev;
	char name[SLOT_NAME_SIZE];
	int slot_nr;

	list_for_each_entry(hpdev, &hbus->children, list_entry) {
		if (hpdev->pci_slot)
			continue;

		slot_nr = PCI_SLOT(wslot_to_devfn(hpdev->desc.win_slot.slot));
		snprintf(name, SLOT_NAME_SIZE, "%u", hpdev->desc.ser);
		hpdev->pci_slot = pci_create_slot(hbus->bridge->bus, slot_nr,
					  name, NULL);
		if (IS_ERR(hpdev->pci_slot)) {
			pr_warn("pci_create slot %s failed\n", name);
			hpdev->pci_slot = NULL;
		}
	}
}

 
static void hv_pci_remove_slots(struct hv_pcibus_device *hbus)
{
	struct hv_pci_dev *hpdev;

	list_for_each_entry(hpdev, &hbus->children, list_entry) {
		if (!hpdev->pci_slot)
			continue;
		pci_destroy_slot(hpdev->pci_slot);
		hpdev->pci_slot = NULL;
	}
}

 
static void hv_pci_assign_numa_node(struct hv_pcibus_device *hbus)
{
	struct pci_dev *dev;
	struct pci_bus *bus = hbus->bridge->bus;
	struct hv_pci_dev *hv_dev;

	list_for_each_entry(dev, &bus->devices, bus_list) {
		hv_dev = get_pcichild_wslot(hbus, devfn_to_wslot(dev->devfn));
		if (!hv_dev)
			continue;

		if (hv_dev->desc.flags & HV_PCI_DEVICE_FLAG_NUMA_AFFINITY &&
		    hv_dev->desc.virtual_numa_node < num_possible_nodes())
			 
			set_dev_node(&dev->dev,
				     numa_map_to_online_node(
					     hv_dev->desc.virtual_numa_node));

		put_pcichild(hv_dev);
	}
}

 
static int create_root_hv_pci_bus(struct hv_pcibus_device *hbus)
{
	int error;
	struct pci_host_bridge *bridge = hbus->bridge;

	bridge->dev.parent = &hbus->hdev->device;
	bridge->sysdata = &hbus->sysdata;
	bridge->ops = &hv_pcifront_ops;

	error = pci_scan_root_bus_bridge(bridge);
	if (error)
		return error;

	pci_lock_rescan_remove();
	hv_pci_assign_numa_node(hbus);
	pci_bus_assign_resources(bridge->bus);
	hv_pci_assign_slots(hbus);
	pci_bus_add_devices(bridge->bus);
	pci_unlock_rescan_remove();
	hbus->state = hv_pcibus_installed;
	return 0;
}

struct q_res_req_compl {
	struct completion host_event;
	struct hv_pci_dev *hpdev;
};

 
static void q_resource_requirements(void *context, struct pci_response *resp,
				    int resp_packet_size)
{
	struct q_res_req_compl *completion = context;
	struct pci_q_res_req_response *q_res_req =
		(struct pci_q_res_req_response *)resp;
	s32 status;
	int i;

	status = (resp_packet_size < sizeof(*q_res_req)) ? -1 : resp->status;
	if (status < 0) {
		dev_err(&completion->hpdev->hbus->hdev->device,
			"query resource requirements failed: %x\n",
			status);
	} else {
		for (i = 0; i < PCI_STD_NUM_BARS; i++) {
			completion->hpdev->probed_bar[i] =
				q_res_req->probed_bar[i];
		}
	}

	complete(&completion->host_event);
}

 
static struct hv_pci_dev *new_pcichild_device(struct hv_pcibus_device *hbus,
		struct hv_pcidev_description *desc)
{
	struct hv_pci_dev *hpdev;
	struct pci_child_message *res_req;
	struct q_res_req_compl comp_pkt;
	struct {
		struct pci_packet init_packet;
		u8 buffer[sizeof(struct pci_child_message)];
	} pkt;
	unsigned long flags;
	int ret;

	hpdev = kzalloc(sizeof(*hpdev), GFP_KERNEL);
	if (!hpdev)
		return NULL;

	hpdev->hbus = hbus;

	memset(&pkt, 0, sizeof(pkt));
	init_completion(&comp_pkt.host_event);
	comp_pkt.hpdev = hpdev;
	pkt.init_packet.compl_ctxt = &comp_pkt;
	pkt.init_packet.completion_func = q_resource_requirements;
	res_req = (struct pci_child_message *)&pkt.init_packet.message;
	res_req->message_type.type = PCI_QUERY_RESOURCE_REQUIREMENTS;
	res_req->wslot.slot = desc->win_slot.slot;

	ret = vmbus_sendpacket(hbus->hdev->channel, res_req,
			       sizeof(struct pci_child_message),
			       (unsigned long)&pkt.init_packet,
			       VM_PKT_DATA_INBAND,
			       VMBUS_DATA_PACKET_FLAG_COMPLETION_REQUESTED);
	if (ret)
		goto error;

	if (wait_for_response(hbus->hdev, &comp_pkt.host_event))
		goto error;

	hpdev->desc = *desc;
	refcount_set(&hpdev->refs, 1);
	get_pcichild(hpdev);
	spin_lock_irqsave(&hbus->device_list_lock, flags);

	list_add_tail(&hpdev->list_entry, &hbus->children);
	spin_unlock_irqrestore(&hbus->device_list_lock, flags);
	return hpdev;

error:
	kfree(hpdev);
	return NULL;
}

 
static struct hv_pci_dev *get_pcichild_wslot(struct hv_pcibus_device *hbus,
					     u32 wslot)
{
	unsigned long flags;
	struct hv_pci_dev *iter, *hpdev = NULL;

	spin_lock_irqsave(&hbus->device_list_lock, flags);
	list_for_each_entry(iter, &hbus->children, list_entry) {
		if (iter->desc.win_slot.slot == wslot) {
			hpdev = iter;
			get_pcichild(hpdev);
			break;
		}
	}
	spin_unlock_irqrestore(&hbus->device_list_lock, flags);

	return hpdev;
}

 
static void pci_devices_present_work(struct work_struct *work)
{
	u32 child_no;
	bool found;
	struct hv_pcidev_description *new_desc;
	struct hv_pci_dev *hpdev;
	struct hv_pcibus_device *hbus;
	struct list_head removed;
	struct hv_dr_work *dr_wrk;
	struct hv_dr_state *dr = NULL;
	unsigned long flags;

	dr_wrk = container_of(work, struct hv_dr_work, wrk);
	hbus = dr_wrk->bus;
	kfree(dr_wrk);

	INIT_LIST_HEAD(&removed);

	 
	spin_lock_irqsave(&hbus->device_list_lock, flags);
	while (!list_empty(&hbus->dr_list)) {
		dr = list_first_entry(&hbus->dr_list, struct hv_dr_state,
				      list_entry);
		list_del(&dr->list_entry);

		 
		if (!list_empty(&hbus->dr_list)) {
			kfree(dr);
			continue;
		}
	}
	spin_unlock_irqrestore(&hbus->device_list_lock, flags);

	if (!dr)
		return;

	mutex_lock(&hbus->state_lock);

	 
	spin_lock_irqsave(&hbus->device_list_lock, flags);
	list_for_each_entry(hpdev, &hbus->children, list_entry) {
		hpdev->reported_missing = true;
	}
	spin_unlock_irqrestore(&hbus->device_list_lock, flags);

	 
	for (child_no = 0; child_no < dr->device_count; child_no++) {
		found = false;
		new_desc = &dr->func[child_no];

		spin_lock_irqsave(&hbus->device_list_lock, flags);
		list_for_each_entry(hpdev, &hbus->children, list_entry) {
			if ((hpdev->desc.win_slot.slot == new_desc->win_slot.slot) &&
			    (hpdev->desc.v_id == new_desc->v_id) &&
			    (hpdev->desc.d_id == new_desc->d_id) &&
			    (hpdev->desc.ser == new_desc->ser)) {
				hpdev->reported_missing = false;
				found = true;
			}
		}
		spin_unlock_irqrestore(&hbus->device_list_lock, flags);

		if (!found) {
			hpdev = new_pcichild_device(hbus, new_desc);
			if (!hpdev)
				dev_err(&hbus->hdev->device,
					"couldn't record a child device.\n");
		}
	}

	 
	spin_lock_irqsave(&hbus->device_list_lock, flags);
	do {
		found = false;
		list_for_each_entry(hpdev, &hbus->children, list_entry) {
			if (hpdev->reported_missing) {
				found = true;
				put_pcichild(hpdev);
				list_move_tail(&hpdev->list_entry, &removed);
				break;
			}
		}
	} while (found);
	spin_unlock_irqrestore(&hbus->device_list_lock, flags);

	 
	while (!list_empty(&removed)) {
		hpdev = list_first_entry(&removed, struct hv_pci_dev,
					 list_entry);
		list_del(&hpdev->list_entry);

		if (hpdev->pci_slot)
			pci_destroy_slot(hpdev->pci_slot);

		put_pcichild(hpdev);
	}

	switch (hbus->state) {
	case hv_pcibus_installed:
		 
		pci_lock_rescan_remove();
		pci_scan_child_bus(hbus->bridge->bus);
		hv_pci_assign_numa_node(hbus);
		hv_pci_assign_slots(hbus);
		pci_unlock_rescan_remove();
		break;

	case hv_pcibus_init:
	case hv_pcibus_probed:
		survey_child_resources(hbus);
		break;

	default:
		break;
	}

	mutex_unlock(&hbus->state_lock);

	kfree(dr);
}

 
static int hv_pci_start_relations_work(struct hv_pcibus_device *hbus,
				       struct hv_dr_state *dr)
{
	struct hv_dr_work *dr_wrk;
	unsigned long flags;
	bool pending_dr;

	if (hbus->state == hv_pcibus_removing) {
		dev_info(&hbus->hdev->device,
			 "PCI VMBus BUS_RELATIONS: ignored\n");
		return -ENOENT;
	}

	dr_wrk = kzalloc(sizeof(*dr_wrk), GFP_NOWAIT);
	if (!dr_wrk)
		return -ENOMEM;

	INIT_WORK(&dr_wrk->wrk, pci_devices_present_work);
	dr_wrk->bus = hbus;

	spin_lock_irqsave(&hbus->device_list_lock, flags);
	 
	pending_dr = !list_empty(&hbus->dr_list);
	list_add_tail(&dr->list_entry, &hbus->dr_list);
	spin_unlock_irqrestore(&hbus->device_list_lock, flags);

	if (pending_dr)
		kfree(dr_wrk);
	else
		queue_work(hbus->wq, &dr_wrk->wrk);

	return 0;
}

 
static void hv_pci_devices_present(struct hv_pcibus_device *hbus,
				   struct pci_bus_relations *relations)
{
	struct hv_dr_state *dr;
	int i;

	dr = kzalloc(struct_size(dr, func, relations->device_count),
		     GFP_NOWAIT);
	if (!dr)
		return;

	dr->device_count = relations->device_count;
	for (i = 0; i < dr->device_count; i++) {
		dr->func[i].v_id = relations->func[i].v_id;
		dr->func[i].d_id = relations->func[i].d_id;
		dr->func[i].rev = relations->func[i].rev;
		dr->func[i].prog_intf = relations->func[i].prog_intf;
		dr->func[i].subclass = relations->func[i].subclass;
		dr->func[i].base_class = relations->func[i].base_class;
		dr->func[i].subsystem_id = relations->func[i].subsystem_id;
		dr->func[i].win_slot = relations->func[i].win_slot;
		dr->func[i].ser = relations->func[i].ser;
	}

	if (hv_pci_start_relations_work(hbus, dr))
		kfree(dr);
}

 
static void hv_pci_devices_present2(struct hv_pcibus_device *hbus,
				    struct pci_bus_relations2 *relations)
{
	struct hv_dr_state *dr;
	int i;

	dr = kzalloc(struct_size(dr, func, relations->device_count),
		     GFP_NOWAIT);
	if (!dr)
		return;

	dr->device_count = relations->device_count;
	for (i = 0; i < dr->device_count; i++) {
		dr->func[i].v_id = relations->func[i].v_id;
		dr->func[i].d_id = relations->func[i].d_id;
		dr->func[i].rev = relations->func[i].rev;
		dr->func[i].prog_intf = relations->func[i].prog_intf;
		dr->func[i].subclass = relations->func[i].subclass;
		dr->func[i].base_class = relations->func[i].base_class;
		dr->func[i].subsystem_id = relations->func[i].subsystem_id;
		dr->func[i].win_slot = relations->func[i].win_slot;
		dr->func[i].ser = relations->func[i].ser;
		dr->func[i].flags = relations->func[i].flags;
		dr->func[i].virtual_numa_node =
			relations->func[i].virtual_numa_node;
	}

	if (hv_pci_start_relations_work(hbus, dr))
		kfree(dr);
}

 
static void hv_eject_device_work(struct work_struct *work)
{
	struct pci_eject_response *ejct_pkt;
	struct hv_pcibus_device *hbus;
	struct hv_pci_dev *hpdev;
	struct pci_dev *pdev;
	unsigned long flags;
	int wslot;
	struct {
		struct pci_packet pkt;
		u8 buffer[sizeof(struct pci_eject_response)];
	} ctxt;

	hpdev = container_of(work, struct hv_pci_dev, wrk);
	hbus = hpdev->hbus;

	mutex_lock(&hbus->state_lock);

	 
	wslot = wslot_to_devfn(hpdev->desc.win_slot.slot);
	pdev = pci_get_domain_bus_and_slot(hbus->bridge->domain_nr, 0, wslot);
	if (pdev) {
		pci_lock_rescan_remove();
		pci_stop_and_remove_bus_device(pdev);
		pci_dev_put(pdev);
		pci_unlock_rescan_remove();
	}

	spin_lock_irqsave(&hbus->device_list_lock, flags);
	list_del(&hpdev->list_entry);
	spin_unlock_irqrestore(&hbus->device_list_lock, flags);

	if (hpdev->pci_slot)
		pci_destroy_slot(hpdev->pci_slot);

	memset(&ctxt, 0, sizeof(ctxt));
	ejct_pkt = (struct pci_eject_response *)&ctxt.pkt.message;
	ejct_pkt->message_type.type = PCI_EJECTION_COMPLETE;
	ejct_pkt->wslot.slot = hpdev->desc.win_slot.slot;
	vmbus_sendpacket(hbus->hdev->channel, ejct_pkt,
			 sizeof(*ejct_pkt), 0,
			 VM_PKT_DATA_INBAND, 0);

	 
	put_pcichild(hpdev);
	 
	put_pcichild(hpdev);
	put_pcichild(hpdev);
	 

	mutex_unlock(&hbus->state_lock);
}

 
static void hv_pci_eject_device(struct hv_pci_dev *hpdev)
{
	struct hv_pcibus_device *hbus = hpdev->hbus;
	struct hv_device *hdev = hbus->hdev;

	if (hbus->state == hv_pcibus_removing) {
		dev_info(&hdev->device, "PCI VMBus EJECT: ignored\n");
		return;
	}

	get_pcichild(hpdev);
	INIT_WORK(&hpdev->wrk, hv_eject_device_work);
	queue_work(hbus->wq, &hpdev->wrk);
}

 
static void hv_pci_onchannelcallback(void *context)
{
	const int packet_size = 0x100;
	int ret;
	struct hv_pcibus_device *hbus = context;
	struct vmbus_channel *chan = hbus->hdev->channel;
	u32 bytes_recvd;
	u64 req_id, req_addr;
	struct vmpacket_descriptor *desc;
	unsigned char *buffer;
	int bufferlen = packet_size;
	struct pci_packet *comp_packet;
	struct pci_response *response;
	struct pci_incoming_message *new_message;
	struct pci_bus_relations *bus_rel;
	struct pci_bus_relations2 *bus_rel2;
	struct pci_dev_inval_block *inval;
	struct pci_dev_incoming *dev_message;
	struct hv_pci_dev *hpdev;
	unsigned long flags;

	buffer = kmalloc(bufferlen, GFP_ATOMIC);
	if (!buffer)
		return;

	while (1) {
		ret = vmbus_recvpacket_raw(chan, buffer, bufferlen,
					   &bytes_recvd, &req_id);

		if (ret == -ENOBUFS) {
			kfree(buffer);
			 
			bufferlen = bytes_recvd;
			buffer = kmalloc(bytes_recvd, GFP_ATOMIC);
			if (!buffer)
				return;
			continue;
		}

		 
		if (ret || !bytes_recvd)
			break;

		 
		if (bytes_recvd <= sizeof(struct pci_response))
			continue;
		desc = (struct vmpacket_descriptor *)buffer;

		switch (desc->type) {
		case VM_PKT_COMP:

			lock_requestor(chan, flags);
			req_addr = __vmbus_request_addr_match(chan, req_id,
							      VMBUS_RQST_ADDR_ANY);
			if (req_addr == VMBUS_RQST_ERROR) {
				unlock_requestor(chan, flags);
				dev_err(&hbus->hdev->device,
					"Invalid transaction ID %llx\n",
					req_id);
				break;
			}
			comp_packet = (struct pci_packet *)req_addr;
			response = (struct pci_response *)buffer;
			 
			comp_packet->completion_func(comp_packet->compl_ctxt,
						     response,
						     bytes_recvd);
			unlock_requestor(chan, flags);
			break;

		case VM_PKT_DATA_INBAND:

			new_message = (struct pci_incoming_message *)buffer;
			switch (new_message->message_type.type) {
			case PCI_BUS_RELATIONS:

				bus_rel = (struct pci_bus_relations *)buffer;
				if (bytes_recvd < sizeof(*bus_rel) ||
				    bytes_recvd <
					struct_size(bus_rel, func,
						    bus_rel->device_count)) {
					dev_err(&hbus->hdev->device,
						"bus relations too small\n");
					break;
				}

				hv_pci_devices_present(hbus, bus_rel);
				break;

			case PCI_BUS_RELATIONS2:

				bus_rel2 = (struct pci_bus_relations2 *)buffer;
				if (bytes_recvd < sizeof(*bus_rel2) ||
				    bytes_recvd <
					struct_size(bus_rel2, func,
						    bus_rel2->device_count)) {
					dev_err(&hbus->hdev->device,
						"bus relations v2 too small\n");
					break;
				}

				hv_pci_devices_present2(hbus, bus_rel2);
				break;

			case PCI_EJECT:

				dev_message = (struct pci_dev_incoming *)buffer;
				if (bytes_recvd < sizeof(*dev_message)) {
					dev_err(&hbus->hdev->device,
						"eject message too small\n");
					break;
				}
				hpdev = get_pcichild_wslot(hbus,
						      dev_message->wslot.slot);
				if (hpdev) {
					hv_pci_eject_device(hpdev);
					put_pcichild(hpdev);
				}
				break;

			case PCI_INVALIDATE_BLOCK:

				inval = (struct pci_dev_inval_block *)buffer;
				if (bytes_recvd < sizeof(*inval)) {
					dev_err(&hbus->hdev->device,
						"invalidate message too small\n");
					break;
				}
				hpdev = get_pcichild_wslot(hbus,
							   inval->wslot.slot);
				if (hpdev) {
					if (hpdev->block_invalidate) {
						hpdev->block_invalidate(
						    hpdev->invalidate_context,
						    inval->block_mask);
					}
					put_pcichild(hpdev);
				}
				break;

			default:
				dev_warn(&hbus->hdev->device,
					"Unimplemented protocol message %x\n",
					new_message->message_type.type);
				break;
			}
			break;

		default:
			dev_err(&hbus->hdev->device,
				"unhandled packet type %d, tid %llx len %d\n",
				desc->type, req_id, bytes_recvd);
			break;
		}
	}

	kfree(buffer);
}

 
static int hv_pci_protocol_negotiation(struct hv_device *hdev,
				       enum pci_protocol_version_t version[],
				       int num_version)
{
	struct hv_pcibus_device *hbus = hv_get_drvdata(hdev);
	struct pci_version_request *version_req;
	struct hv_pci_compl comp_pkt;
	struct pci_packet *pkt;
	int ret;
	int i;

	 
	pkt = kzalloc(sizeof(*pkt) + sizeof(*version_req), GFP_KERNEL);
	if (!pkt)
		return -ENOMEM;

	init_completion(&comp_pkt.host_event);
	pkt->completion_func = hv_pci_generic_compl;
	pkt->compl_ctxt = &comp_pkt;
	version_req = (struct pci_version_request *)&pkt->message;
	version_req->message_type.type = PCI_QUERY_PROTOCOL_VERSION;

	for (i = 0; i < num_version; i++) {
		version_req->protocol_version = version[i];
		ret = vmbus_sendpacket(hdev->channel, version_req,
				sizeof(struct pci_version_request),
				(unsigned long)pkt, VM_PKT_DATA_INBAND,
				VMBUS_DATA_PACKET_FLAG_COMPLETION_REQUESTED);
		if (!ret)
			ret = wait_for_response(hdev, &comp_pkt.host_event);

		if (ret) {
			dev_err(&hdev->device,
				"PCI Pass-through VSP failed to request version: %d",
				ret);
			goto exit;
		}

		if (comp_pkt.completion_status >= 0) {
			hbus->protocol_version = version[i];
			dev_info(&hdev->device,
				"PCI VMBus probing: Using version %#x\n",
				hbus->protocol_version);
			goto exit;
		}

		if (comp_pkt.completion_status != STATUS_REVISION_MISMATCH) {
			dev_err(&hdev->device,
				"PCI Pass-through VSP failed version request: %#x",
				comp_pkt.completion_status);
			ret = -EPROTO;
			goto exit;
		}

		reinit_completion(&comp_pkt.host_event);
	}

	dev_err(&hdev->device,
		"PCI pass-through VSP failed to find supported version");
	ret = -EPROTO;

exit:
	kfree(pkt);
	return ret;
}

 
static void hv_pci_free_bridge_windows(struct hv_pcibus_device *hbus)
{
	 

	if (hbus->low_mmio_space && hbus->low_mmio_res) {
		hbus->low_mmio_res->flags |= IORESOURCE_BUSY;
		vmbus_free_mmio(hbus->low_mmio_res->start,
				resource_size(hbus->low_mmio_res));
	}

	if (hbus->high_mmio_space && hbus->high_mmio_res) {
		hbus->high_mmio_res->flags |= IORESOURCE_BUSY;
		vmbus_free_mmio(hbus->high_mmio_res->start,
				resource_size(hbus->high_mmio_res));
	}
}

 
static int hv_pci_allocate_bridge_windows(struct hv_pcibus_device *hbus)
{
	resource_size_t align;
	int ret;

	if (hbus->low_mmio_space) {
		align = 1ULL << (63 - __builtin_clzll(hbus->low_mmio_space));
		ret = vmbus_allocate_mmio(&hbus->low_mmio_res, hbus->hdev, 0,
					  (u64)(u32)0xffffffff,
					  hbus->low_mmio_space,
					  align, false);
		if (ret) {
			dev_err(&hbus->hdev->device,
				"Need %#llx of low MMIO space. Consider reconfiguring the VM.\n",
				hbus->low_mmio_space);
			return ret;
		}

		 
		hbus->low_mmio_res->flags |= IORESOURCE_WINDOW;
		hbus->low_mmio_res->flags &= ~IORESOURCE_BUSY;
		pci_add_resource(&hbus->bridge->windows, hbus->low_mmio_res);
	}

	if (hbus->high_mmio_space) {
		align = 1ULL << (63 - __builtin_clzll(hbus->high_mmio_space));
		ret = vmbus_allocate_mmio(&hbus->high_mmio_res, hbus->hdev,
					  0x100000000, -1,
					  hbus->high_mmio_space, align,
					  false);
		if (ret) {
			dev_err(&hbus->hdev->device,
				"Need %#llx of high MMIO space. Consider reconfiguring the VM.\n",
				hbus->high_mmio_space);
			goto release_low_mmio;
		}

		 
		hbus->high_mmio_res->flags |= IORESOURCE_WINDOW;
		hbus->high_mmio_res->flags &= ~IORESOURCE_BUSY;
		pci_add_resource(&hbus->bridge->windows, hbus->high_mmio_res);
	}

	return 0;

release_low_mmio:
	if (hbus->low_mmio_res) {
		vmbus_free_mmio(hbus->low_mmio_res->start,
				resource_size(hbus->low_mmio_res));
	}

	return ret;
}

 
static int hv_allocate_config_window(struct hv_pcibus_device *hbus)
{
	int ret;

	 
	ret = vmbus_allocate_mmio(&hbus->mem_config, hbus->hdev, 0, -1,
				  PCI_CONFIG_MMIO_LENGTH, 0x1000, false);
	if (ret)
		return ret;

	 

	hbus->mem_config->flags |= IORESOURCE_BUSY;

	return 0;
}

static void hv_free_config_window(struct hv_pcibus_device *hbus)
{
	vmbus_free_mmio(hbus->mem_config->start, PCI_CONFIG_MMIO_LENGTH);
}

static int hv_pci_bus_exit(struct hv_device *hdev, bool keep_devs);

 
static int hv_pci_enter_d0(struct hv_device *hdev)
{
	struct hv_pcibus_device *hbus = hv_get_drvdata(hdev);
	struct pci_bus_d0_entry *d0_entry;
	struct hv_pci_compl comp_pkt;
	struct pci_packet *pkt;
	bool retry = true;
	int ret;

enter_d0_retry:
	 
	pkt = kzalloc(sizeof(*pkt) + sizeof(*d0_entry), GFP_KERNEL);
	if (!pkt)
		return -ENOMEM;

	init_completion(&comp_pkt.host_event);
	pkt->completion_func = hv_pci_generic_compl;
	pkt->compl_ctxt = &comp_pkt;
	d0_entry = (struct pci_bus_d0_entry *)&pkt->message;
	d0_entry->message_type.type = PCI_BUS_D0ENTRY;
	d0_entry->mmio_base = hbus->mem_config->start;

	ret = vmbus_sendpacket(hdev->channel, d0_entry, sizeof(*d0_entry),
			       (unsigned long)pkt, VM_PKT_DATA_INBAND,
			       VMBUS_DATA_PACKET_FLAG_COMPLETION_REQUESTED);
	if (!ret)
		ret = wait_for_response(hdev, &comp_pkt.host_event);

	if (ret)
		goto exit;

	 
	if (comp_pkt.completion_status < 0 && retry) {
		retry = false;

		dev_err(&hdev->device, "Retrying D0 Entry\n");

		 
		hbus->wslot_res_allocated = 255;

		ret = hv_pci_bus_exit(hdev, true);

		if (ret == 0) {
			kfree(pkt);
			goto enter_d0_retry;
		}
		dev_err(&hdev->device,
			"Retrying D0 failed with ret %d\n", ret);
	}

	if (comp_pkt.completion_status < 0) {
		dev_err(&hdev->device,
			"PCI Pass-through VSP failed D0 Entry with status %x\n",
			comp_pkt.completion_status);
		ret = -EPROTO;
		goto exit;
	}

	ret = 0;

exit:
	kfree(pkt);
	return ret;
}

 
static int hv_pci_query_relations(struct hv_device *hdev)
{
	struct hv_pcibus_device *hbus = hv_get_drvdata(hdev);
	struct pci_message message;
	struct completion comp;
	int ret;

	 
	init_completion(&comp);
	if (cmpxchg(&hbus->survey_event, NULL, &comp))
		return -ENOTEMPTY;

	memset(&message, 0, sizeof(message));
	message.type = PCI_QUERY_BUS_RELATIONS;

	ret = vmbus_sendpacket(hdev->channel, &message, sizeof(message),
			       0, VM_PKT_DATA_INBAND, 0);
	if (!ret)
		ret = wait_for_response(hdev, &comp);

	 
	flush_workqueue(hbus->wq);

	return ret;
}

 
static int hv_send_resources_allocated(struct hv_device *hdev)
{
	struct hv_pcibus_device *hbus = hv_get_drvdata(hdev);
	struct pci_resources_assigned *res_assigned;
	struct pci_resources_assigned2 *res_assigned2;
	struct hv_pci_compl comp_pkt;
	struct hv_pci_dev *hpdev;
	struct pci_packet *pkt;
	size_t size_res;
	int wslot;
	int ret;

	size_res = (hbus->protocol_version < PCI_PROTOCOL_VERSION_1_2)
			? sizeof(*res_assigned) : sizeof(*res_assigned2);

	pkt = kmalloc(sizeof(*pkt) + size_res, GFP_KERNEL);
	if (!pkt)
		return -ENOMEM;

	ret = 0;

	for (wslot = 0; wslot < 256; wslot++) {
		hpdev = get_pcichild_wslot(hbus, wslot);
		if (!hpdev)
			continue;

		memset(pkt, 0, sizeof(*pkt) + size_res);
		init_completion(&comp_pkt.host_event);
		pkt->completion_func = hv_pci_generic_compl;
		pkt->compl_ctxt = &comp_pkt;

		if (hbus->protocol_version < PCI_PROTOCOL_VERSION_1_2) {
			res_assigned =
				(struct pci_resources_assigned *)&pkt->message;
			res_assigned->message_type.type =
				PCI_RESOURCES_ASSIGNED;
			res_assigned->wslot.slot = hpdev->desc.win_slot.slot;
		} else {
			res_assigned2 =
				(struct pci_resources_assigned2 *)&pkt->message;
			res_assigned2->message_type.type =
				PCI_RESOURCES_ASSIGNED2;
			res_assigned2->wslot.slot = hpdev->desc.win_slot.slot;
		}
		put_pcichild(hpdev);

		ret = vmbus_sendpacket(hdev->channel, &pkt->message,
				size_res, (unsigned long)pkt,
				VM_PKT_DATA_INBAND,
				VMBUS_DATA_PACKET_FLAG_COMPLETION_REQUESTED);
		if (!ret)
			ret = wait_for_response(hdev, &comp_pkt.host_event);
		if (ret)
			break;

		if (comp_pkt.completion_status < 0) {
			ret = -EPROTO;
			dev_err(&hdev->device,
				"resource allocated returned 0x%x",
				comp_pkt.completion_status);
			break;
		}

		hbus->wslot_res_allocated = wslot;
	}

	kfree(pkt);
	return ret;
}

 
static int hv_send_resources_released(struct hv_device *hdev)
{
	struct hv_pcibus_device *hbus = hv_get_drvdata(hdev);
	struct pci_child_message pkt;
	struct hv_pci_dev *hpdev;
	int wslot;
	int ret;

	for (wslot = hbus->wslot_res_allocated; wslot >= 0; wslot--) {
		hpdev = get_pcichild_wslot(hbus, wslot);
		if (!hpdev)
			continue;

		memset(&pkt, 0, sizeof(pkt));
		pkt.message_type.type = PCI_RESOURCES_RELEASED;
		pkt.wslot.slot = hpdev->desc.win_slot.slot;

		put_pcichild(hpdev);

		ret = vmbus_sendpacket(hdev->channel, &pkt, sizeof(pkt), 0,
				       VM_PKT_DATA_INBAND, 0);
		if (ret)
			return ret;

		hbus->wslot_res_allocated = wslot - 1;
	}

	hbus->wslot_res_allocated = -1;

	return 0;
}

#define HVPCI_DOM_MAP_SIZE (64 * 1024)
static DECLARE_BITMAP(hvpci_dom_map, HVPCI_DOM_MAP_SIZE);

 
#define HVPCI_DOM_INVALID 0

 
static u16 hv_get_dom_num(u16 dom)
{
	unsigned int i;

	if (test_and_set_bit(dom, hvpci_dom_map) == 0)
		return dom;

	for_each_clear_bit(i, hvpci_dom_map, HVPCI_DOM_MAP_SIZE) {
		if (test_and_set_bit(i, hvpci_dom_map) == 0)
			return i;
	}

	return HVPCI_DOM_INVALID;
}

 
static void hv_put_dom_num(u16 dom)
{
	clear_bit(dom, hvpci_dom_map);
}

 
static int hv_pci_probe(struct hv_device *hdev,
			const struct hv_vmbus_device_id *dev_id)
{
	struct pci_host_bridge *bridge;
	struct hv_pcibus_device *hbus;
	u16 dom_req, dom;
	char *name;
	int ret;

	bridge = devm_pci_alloc_host_bridge(&hdev->device, 0);
	if (!bridge)
		return -ENOMEM;

	hbus = kzalloc(sizeof(*hbus), GFP_KERNEL);
	if (!hbus)
		return -ENOMEM;

	hbus->bridge = bridge;
	mutex_init(&hbus->state_lock);
	hbus->state = hv_pcibus_init;
	hbus->wslot_res_allocated = -1;

	 
	dom_req = hdev->dev_instance.b[5] << 8 | hdev->dev_instance.b[4];
	dom = hv_get_dom_num(dom_req);

	if (dom == HVPCI_DOM_INVALID) {
		dev_err(&hdev->device,
			"Unable to use dom# 0x%x or other numbers", dom_req);
		ret = -EINVAL;
		goto free_bus;
	}

	if (dom != dom_req)
		dev_info(&hdev->device,
			 "PCI dom# 0x%x has collision, using 0x%x",
			 dom_req, dom);

	hbus->bridge->domain_nr = dom;
#ifdef CONFIG_X86
	hbus->sysdata.domain = dom;
	hbus->use_calls = !!(ms_hyperv.hints & HV_X64_USE_MMIO_HYPERCALLS);
#elif defined(CONFIG_ARM64)
	 
	hbus->sysdata.parent = hdev->device.parent;
	hbus->use_calls = false;
#endif

	hbus->hdev = hdev;
	INIT_LIST_HEAD(&hbus->children);
	INIT_LIST_HEAD(&hbus->dr_list);
	spin_lock_init(&hbus->config_lock);
	spin_lock_init(&hbus->device_list_lock);
	hbus->wq = alloc_ordered_workqueue("hv_pci_%x", 0,
					   hbus->bridge->domain_nr);
	if (!hbus->wq) {
		ret = -ENOMEM;
		goto free_dom;
	}

	hdev->channel->next_request_id_callback = vmbus_next_request_id;
	hdev->channel->request_addr_callback = vmbus_request_addr;
	hdev->channel->rqstor_size = HV_PCI_RQSTOR_SIZE;

	ret = vmbus_open(hdev->channel, pci_ring_size, pci_ring_size, NULL, 0,
			 hv_pci_onchannelcallback, hbus);
	if (ret)
		goto destroy_wq;

	hv_set_drvdata(hdev, hbus);

	ret = hv_pci_protocol_negotiation(hdev, pci_protocol_versions,
					  ARRAY_SIZE(pci_protocol_versions));
	if (ret)
		goto close;

	ret = hv_allocate_config_window(hbus);
	if (ret)
		goto close;

	hbus->cfg_addr = ioremap(hbus->mem_config->start,
				 PCI_CONFIG_MMIO_LENGTH);
	if (!hbus->cfg_addr) {
		dev_err(&hdev->device,
			"Unable to map a virtual address for config space\n");
		ret = -ENOMEM;
		goto free_config;
	}

	name = kasprintf(GFP_KERNEL, "%pUL", &hdev->dev_instance);
	if (!name) {
		ret = -ENOMEM;
		goto unmap;
	}

	hbus->fwnode = irq_domain_alloc_named_fwnode(name);
	kfree(name);
	if (!hbus->fwnode) {
		ret = -ENOMEM;
		goto unmap;
	}

	ret = hv_pcie_init_irq_domain(hbus);
	if (ret)
		goto free_fwnode;

	ret = hv_pci_query_relations(hdev);
	if (ret)
		goto free_irq_domain;

	mutex_lock(&hbus->state_lock);

	ret = hv_pci_enter_d0(hdev);
	if (ret)
		goto release_state_lock;

	ret = hv_pci_allocate_bridge_windows(hbus);
	if (ret)
		goto exit_d0;

	ret = hv_send_resources_allocated(hdev);
	if (ret)
		goto free_windows;

	prepopulate_bars(hbus);

	hbus->state = hv_pcibus_probed;

	ret = create_root_hv_pci_bus(hbus);
	if (ret)
		goto free_windows;

	mutex_unlock(&hbus->state_lock);
	return 0;

free_windows:
	hv_pci_free_bridge_windows(hbus);
exit_d0:
	(void) hv_pci_bus_exit(hdev, true);
release_state_lock:
	mutex_unlock(&hbus->state_lock);
free_irq_domain:
	irq_domain_remove(hbus->irq_domain);
free_fwnode:
	irq_domain_free_fwnode(hbus->fwnode);
unmap:
	iounmap(hbus->cfg_addr);
free_config:
	hv_free_config_window(hbus);
close:
	vmbus_close(hdev->channel);
destroy_wq:
	destroy_workqueue(hbus->wq);
free_dom:
	hv_put_dom_num(hbus->bridge->domain_nr);
free_bus:
	kfree(hbus);
	return ret;
}

static int hv_pci_bus_exit(struct hv_device *hdev, bool keep_devs)
{
	struct hv_pcibus_device *hbus = hv_get_drvdata(hdev);
	struct vmbus_channel *chan = hdev->channel;
	struct {
		struct pci_packet teardown_packet;
		u8 buffer[sizeof(struct pci_message)];
	} pkt;
	struct hv_pci_compl comp_pkt;
	struct hv_pci_dev *hpdev, *tmp;
	unsigned long flags;
	u64 trans_id;
	int ret;

	 
	if (chan->rescind)
		return 0;

	if (!keep_devs) {
		struct list_head removed;

		 
		INIT_LIST_HEAD(&removed);
		spin_lock_irqsave(&hbus->device_list_lock, flags);
		list_for_each_entry_safe(hpdev, tmp, &hbus->children, list_entry)
			list_move_tail(&hpdev->list_entry, &removed);
		spin_unlock_irqrestore(&hbus->device_list_lock, flags);

		 
		list_for_each_entry_safe(hpdev, tmp, &removed, list_entry) {
			list_del(&hpdev->list_entry);
			if (hpdev->pci_slot)
				pci_destroy_slot(hpdev->pci_slot);
			 
			put_pcichild(hpdev);
			put_pcichild(hpdev);
		}
	}

	ret = hv_send_resources_released(hdev);
	if (ret) {
		dev_err(&hdev->device,
			"Couldn't send resources released packet(s)\n");
		return ret;
	}

	memset(&pkt.teardown_packet, 0, sizeof(pkt.teardown_packet));
	init_completion(&comp_pkt.host_event);
	pkt.teardown_packet.completion_func = hv_pci_generic_compl;
	pkt.teardown_packet.compl_ctxt = &comp_pkt;
	pkt.teardown_packet.message[0].type = PCI_BUS_D0EXIT;

	ret = vmbus_sendpacket_getid(chan, &pkt.teardown_packet.message,
				     sizeof(struct pci_message),
				     (unsigned long)&pkt.teardown_packet,
				     &trans_id, VM_PKT_DATA_INBAND,
				     VMBUS_DATA_PACKET_FLAG_COMPLETION_REQUESTED);
	if (ret)
		return ret;

	if (wait_for_completion_timeout(&comp_pkt.host_event, 10 * HZ) == 0) {
		 
		vmbus_request_addr_match(chan, trans_id,
					 (unsigned long)&pkt.teardown_packet);
		return -ETIMEDOUT;
	}

	return 0;
}

 
static void hv_pci_remove(struct hv_device *hdev)
{
	struct hv_pcibus_device *hbus;

	hbus = hv_get_drvdata(hdev);
	if (hbus->state == hv_pcibus_installed) {
		tasklet_disable(&hdev->channel->callback_event);
		hbus->state = hv_pcibus_removing;
		tasklet_enable(&hdev->channel->callback_event);
		destroy_workqueue(hbus->wq);
		hbus->wq = NULL;
		 

		 
		pci_lock_rescan_remove();
		pci_stop_root_bus(hbus->bridge->bus);
		hv_pci_remove_slots(hbus);
		pci_remove_root_bus(hbus->bridge->bus);
		pci_unlock_rescan_remove();
	}

	hv_pci_bus_exit(hdev, false);

	vmbus_close(hdev->channel);

	iounmap(hbus->cfg_addr);
	hv_free_config_window(hbus);
	hv_pci_free_bridge_windows(hbus);
	irq_domain_remove(hbus->irq_domain);
	irq_domain_free_fwnode(hbus->fwnode);

	hv_put_dom_num(hbus->bridge->domain_nr);

	kfree(hbus);
}

static int hv_pci_suspend(struct hv_device *hdev)
{
	struct hv_pcibus_device *hbus = hv_get_drvdata(hdev);
	enum hv_pcibus_state old_state;
	int ret;

	 
	tasklet_disable(&hdev->channel->callback_event);

	 
	old_state = hbus->state;
	if (hbus->state == hv_pcibus_installed)
		hbus->state = hv_pcibus_removing;

	tasklet_enable(&hdev->channel->callback_event);

	if (old_state != hv_pcibus_installed)
		return -EINVAL;

	flush_workqueue(hbus->wq);

	ret = hv_pci_bus_exit(hdev, true);
	if (ret)
		return ret;

	vmbus_close(hdev->channel);

	return 0;
}

static int hv_pci_restore_msi_msg(struct pci_dev *pdev, void *arg)
{
	struct irq_data *irq_data;
	struct msi_desc *entry;
	int ret = 0;

	if (!pdev->msi_enabled && !pdev->msix_enabled)
		return 0;

	msi_lock_descs(&pdev->dev);
	msi_for_each_desc(entry, &pdev->dev, MSI_DESC_ASSOCIATED) {
		irq_data = irq_get_irq_data(entry->irq);
		if (WARN_ON_ONCE(!irq_data)) {
			ret = -EINVAL;
			break;
		}

		hv_compose_msi_msg(irq_data, &entry->msg);
	}
	msi_unlock_descs(&pdev->dev);

	return ret;
}

 
static void hv_pci_restore_msi_state(struct hv_pcibus_device *hbus)
{
	pci_walk_bus(hbus->bridge->bus, hv_pci_restore_msi_msg, NULL);
}

static int hv_pci_resume(struct hv_device *hdev)
{
	struct hv_pcibus_device *hbus = hv_get_drvdata(hdev);
	enum pci_protocol_version_t version[1];
	int ret;

	hbus->state = hv_pcibus_init;

	hdev->channel->next_request_id_callback = vmbus_next_request_id;
	hdev->channel->request_addr_callback = vmbus_request_addr;
	hdev->channel->rqstor_size = HV_PCI_RQSTOR_SIZE;

	ret = vmbus_open(hdev->channel, pci_ring_size, pci_ring_size, NULL, 0,
			 hv_pci_onchannelcallback, hbus);
	if (ret)
		return ret;

	 
	version[0] = hbus->protocol_version;
	ret = hv_pci_protocol_negotiation(hdev, version, 1);
	if (ret)
		goto out;

	ret = hv_pci_query_relations(hdev);
	if (ret)
		goto out;

	mutex_lock(&hbus->state_lock);

	ret = hv_pci_enter_d0(hdev);
	if (ret)
		goto release_state_lock;

	ret = hv_send_resources_allocated(hdev);
	if (ret)
		goto release_state_lock;

	prepopulate_bars(hbus);

	hv_pci_restore_msi_state(hbus);

	hbus->state = hv_pcibus_installed;
	mutex_unlock(&hbus->state_lock);
	return 0;

release_state_lock:
	mutex_unlock(&hbus->state_lock);
out:
	vmbus_close(hdev->channel);
	return ret;
}

static const struct hv_vmbus_device_id hv_pci_id_table[] = {
	 
	 
	{ HV_PCIE_GUID, },
	{ },
};

MODULE_DEVICE_TABLE(vmbus, hv_pci_id_table);

static struct hv_driver hv_pci_drv = {
	.name		= "hv_pci",
	.id_table	= hv_pci_id_table,
	.probe		= hv_pci_probe,
	.remove		= hv_pci_remove,
	.suspend	= hv_pci_suspend,
	.resume		= hv_pci_resume,
};

static void __exit exit_hv_pci_drv(void)
{
	vmbus_driver_unregister(&hv_pci_drv);

	hvpci_block_ops.read_block = NULL;
	hvpci_block_ops.write_block = NULL;
	hvpci_block_ops.reg_blk_invalidate = NULL;
}

static int __init init_hv_pci_drv(void)
{
	int ret;

	if (!hv_is_hyperv_initialized())
		return -ENODEV;

	ret = hv_pci_irqchip_init();
	if (ret)
		return ret;

	 
	set_bit(HVPCI_DOM_INVALID, hvpci_dom_map);

	 
	hvpci_block_ops.read_block = hv_read_config_block;
	hvpci_block_ops.write_block = hv_write_config_block;
	hvpci_block_ops.reg_blk_invalidate = hv_register_block_invalidate;

	return vmbus_driver_register(&hv_pci_drv);
}

module_init(init_hv_pci_drv);
module_exit(exit_hv_pci_drv);

MODULE_DESCRIPTION("Hyper-V PCI");
MODULE_LICENSE("GPL v2");
