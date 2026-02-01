
 

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/backlight.h>
#include <linux/acpi.h>
#include <linux/pnp.h>
#include <linux/apple-gmux.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/pci.h>
#include <linux/vga_switcheroo.h>
#include <linux/debugfs.h>
#include <acpi/video.h>
#include <asm/io.h>

 

struct apple_gmux_config;

struct apple_gmux_data {
	u8 __iomem *iomem_base;
	unsigned long iostart;
	unsigned long iolen;
	const struct apple_gmux_config *config;
	struct mutex index_lock;

	struct backlight_device *bdev;

	 
	acpi_handle dhandle;
	int gpe;
	bool external_switchable;
	enum vga_switcheroo_client_id switch_state_display;
	enum vga_switcheroo_client_id switch_state_ddc;
	enum vga_switcheroo_client_id switch_state_external;
	enum vga_switcheroo_state power_state;
	struct completion powerchange_done;

	 
	u8 selected_port;
	struct dentry *debug_dentry;
};

static struct apple_gmux_data *apple_gmux_data;

struct apple_gmux_config {
	u8 (*read8)(struct apple_gmux_data *gmux_data, int port);
	void (*write8)(struct apple_gmux_data *gmux_data, int port, u8 val);
	u32 (*read32)(struct apple_gmux_data *gmux_data, int port);
	void (*write32)(struct apple_gmux_data *gmux_data, int port, u32 val);
	const struct vga_switcheroo_handler *gmux_handler;
	enum vga_switcheroo_handler_flags_t handler_flags;
	unsigned long resource_type;
	bool read_version_as_u32;
	char *name;
};

#define GMUX_INTERRUPT_ENABLE		0xff
#define GMUX_INTERRUPT_DISABLE		0x00

#define GMUX_INTERRUPT_STATUS_ACTIVE	0
#define GMUX_INTERRUPT_STATUS_DISPLAY	(1 << 0)
#define GMUX_INTERRUPT_STATUS_POWER	(1 << 2)
#define GMUX_INTERRUPT_STATUS_HOTPLUG	(1 << 3)

#define GMUX_BRIGHTNESS_MASK		0x00ffffff
#define GMUX_MAX_BRIGHTNESS		GMUX_BRIGHTNESS_MASK

# define MMIO_GMUX_MAX_BRIGHTNESS	0xffff

static u8 gmux_pio_read8(struct apple_gmux_data *gmux_data, int port)
{
	return inb(gmux_data->iostart + port);
}

static void gmux_pio_write8(struct apple_gmux_data *gmux_data, int port,
			       u8 val)
{
	outb(val, gmux_data->iostart + port);
}

static u32 gmux_pio_read32(struct apple_gmux_data *gmux_data, int port)
{
	return inl(gmux_data->iostart + port);
}

static void gmux_pio_write32(struct apple_gmux_data *gmux_data, int port,
			     u32 val)
{
	int i;
	u8 tmpval;

	for (i = 0; i < 4; i++) {
		tmpval = (val >> (i * 8)) & 0xff;
		outb(tmpval, gmux_data->iostart + port + i);
	}
}

static int gmux_index_wait_ready(struct apple_gmux_data *gmux_data)
{
	int i = 200;
	u8 gwr = inb(gmux_data->iostart + GMUX_PORT_WRITE);

	while (i && (gwr & 0x01)) {
		inb(gmux_data->iostart + GMUX_PORT_READ);
		gwr = inb(gmux_data->iostart + GMUX_PORT_WRITE);
		udelay(100);
		i--;
	}

	return !!i;
}

static int gmux_index_wait_complete(struct apple_gmux_data *gmux_data)
{
	int i = 200;
	u8 gwr = inb(gmux_data->iostart + GMUX_PORT_WRITE);

	while (i && !(gwr & 0x01)) {
		gwr = inb(gmux_data->iostart + GMUX_PORT_WRITE);
		udelay(100);
		i--;
	}

	if (gwr & 0x01)
		inb(gmux_data->iostart + GMUX_PORT_READ);

	return !!i;
}

static u8 gmux_index_read8(struct apple_gmux_data *gmux_data, int port)
{
	u8 val;

	mutex_lock(&gmux_data->index_lock);
	gmux_index_wait_ready(gmux_data);
	outb((port & 0xff), gmux_data->iostart + GMUX_PORT_READ);
	gmux_index_wait_complete(gmux_data);
	val = inb(gmux_data->iostart + GMUX_PORT_VALUE);
	mutex_unlock(&gmux_data->index_lock);

	return val;
}

static void gmux_index_write8(struct apple_gmux_data *gmux_data, int port,
			      u8 val)
{
	mutex_lock(&gmux_data->index_lock);
	outb(val, gmux_data->iostart + GMUX_PORT_VALUE);
	gmux_index_wait_ready(gmux_data);
	outb(port & 0xff, gmux_data->iostart + GMUX_PORT_WRITE);
	gmux_index_wait_complete(gmux_data);
	mutex_unlock(&gmux_data->index_lock);
}

static u32 gmux_index_read32(struct apple_gmux_data *gmux_data, int port)
{
	u32 val;

	mutex_lock(&gmux_data->index_lock);
	gmux_index_wait_ready(gmux_data);
	outb((port & 0xff), gmux_data->iostart + GMUX_PORT_READ);
	gmux_index_wait_complete(gmux_data);
	val = inl(gmux_data->iostart + GMUX_PORT_VALUE);
	mutex_unlock(&gmux_data->index_lock);

	return val;
}

static void gmux_index_write32(struct apple_gmux_data *gmux_data, int port,
			       u32 val)
{
	int i;
	u8 tmpval;

	mutex_lock(&gmux_data->index_lock);

	for (i = 0; i < 4; i++) {
		tmpval = (val >> (i * 8)) & 0xff;
		outb(tmpval, gmux_data->iostart + GMUX_PORT_VALUE + i);
	}

	gmux_index_wait_ready(gmux_data);
	outb(port & 0xff, gmux_data->iostart + GMUX_PORT_WRITE);
	gmux_index_wait_complete(gmux_data);
	mutex_unlock(&gmux_data->index_lock);
}

static int gmux_mmio_wait(struct apple_gmux_data *gmux_data)
{
	int i = 200;
	u8 gwr = ioread8(gmux_data->iomem_base + GMUX_MMIO_COMMAND_SEND);

	while (i && gwr) {
		gwr = ioread8(gmux_data->iomem_base + GMUX_MMIO_COMMAND_SEND);
		udelay(100);
		i--;
	}

	return !!i;
}

static u8 gmux_mmio_read8(struct apple_gmux_data *gmux_data, int port)
{
	u8 val;

	mutex_lock(&gmux_data->index_lock);
	gmux_mmio_wait(gmux_data);
	iowrite8((port & 0xff), gmux_data->iomem_base + GMUX_MMIO_PORT_SELECT);
	iowrite8(GMUX_MMIO_READ | sizeof(val),
		gmux_data->iomem_base + GMUX_MMIO_COMMAND_SEND);
	gmux_mmio_wait(gmux_data);
	val = ioread8(gmux_data->iomem_base);
	mutex_unlock(&gmux_data->index_lock);

	return val;
}

static void gmux_mmio_write8(struct apple_gmux_data *gmux_data, int port,
			      u8 val)
{
	mutex_lock(&gmux_data->index_lock);
	gmux_mmio_wait(gmux_data);
	iowrite8(val, gmux_data->iomem_base);

	iowrite8(port & 0xff, gmux_data->iomem_base + GMUX_MMIO_PORT_SELECT);
	iowrite8(GMUX_MMIO_WRITE | sizeof(val),
		gmux_data->iomem_base + GMUX_MMIO_COMMAND_SEND);

	gmux_mmio_wait(gmux_data);
	mutex_unlock(&gmux_data->index_lock);
}

static u32 gmux_mmio_read32(struct apple_gmux_data *gmux_data, int port)
{
	u32 val;

	mutex_lock(&gmux_data->index_lock);
	gmux_mmio_wait(gmux_data);
	iowrite8((port & 0xff), gmux_data->iomem_base + GMUX_MMIO_PORT_SELECT);
	iowrite8(GMUX_MMIO_READ | sizeof(val),
		gmux_data->iomem_base + GMUX_MMIO_COMMAND_SEND);
	gmux_mmio_wait(gmux_data);
	val = ioread32be(gmux_data->iomem_base);
	mutex_unlock(&gmux_data->index_lock);

	return val;
}

static void gmux_mmio_write32(struct apple_gmux_data *gmux_data, int port,
			       u32 val)
{
	mutex_lock(&gmux_data->index_lock);
	iowrite32be(val, gmux_data->iomem_base);
	iowrite8(port & 0xff, gmux_data->iomem_base + GMUX_MMIO_PORT_SELECT);
	iowrite8(GMUX_MMIO_WRITE | sizeof(val),
		gmux_data->iomem_base + GMUX_MMIO_COMMAND_SEND);
	gmux_mmio_wait(gmux_data);
	mutex_unlock(&gmux_data->index_lock);
}

static u8 gmux_read8(struct apple_gmux_data *gmux_data, int port)
{
	return gmux_data->config->read8(gmux_data, port);
}

static void gmux_write8(struct apple_gmux_data *gmux_data, int port, u8 val)
{
	return gmux_data->config->write8(gmux_data, port, val);
}

static u32 gmux_read32(struct apple_gmux_data *gmux_data, int port)
{
	return gmux_data->config->read32(gmux_data, port);
}

static void gmux_write32(struct apple_gmux_data *gmux_data, int port,
			     u32 val)
{
	return gmux_data->config->write32(gmux_data, port, val);
}

 

static int gmux_get_brightness(struct backlight_device *bd)
{
	struct apple_gmux_data *gmux_data = bl_get_data(bd);
	return gmux_read32(gmux_data, GMUX_PORT_BRIGHTNESS) &
	       GMUX_BRIGHTNESS_MASK;
}

static int gmux_update_status(struct backlight_device *bd)
{
	struct apple_gmux_data *gmux_data = bl_get_data(bd);
	u32 brightness = backlight_get_brightness(bd);

	gmux_write32(gmux_data, GMUX_PORT_BRIGHTNESS, brightness);

	return 0;
}

static const struct backlight_ops gmux_bl_ops = {
	.options = BL_CORE_SUSPENDRESUME,
	.get_brightness = gmux_get_brightness,
	.update_status = gmux_update_status,
};

 

static void gmux_read_switch_state(struct apple_gmux_data *gmux_data)
{
	if (gmux_read8(gmux_data, GMUX_PORT_SWITCH_DDC) == 1)
		gmux_data->switch_state_ddc = VGA_SWITCHEROO_IGD;
	else
		gmux_data->switch_state_ddc = VGA_SWITCHEROO_DIS;

	if (gmux_read8(gmux_data, GMUX_PORT_SWITCH_DISPLAY) & 1)
		gmux_data->switch_state_display = VGA_SWITCHEROO_DIS;
	else
		gmux_data->switch_state_display = VGA_SWITCHEROO_IGD;

	if (gmux_read8(gmux_data, GMUX_PORT_SWITCH_EXTERNAL) == 2)
		gmux_data->switch_state_external = VGA_SWITCHEROO_IGD;
	else
		gmux_data->switch_state_external = VGA_SWITCHEROO_DIS;
}

static void gmux_write_switch_state(struct apple_gmux_data *gmux_data)
{
	if (gmux_data->switch_state_ddc == VGA_SWITCHEROO_IGD)
		gmux_write8(gmux_data, GMUX_PORT_SWITCH_DDC, 1);
	else
		gmux_write8(gmux_data, GMUX_PORT_SWITCH_DDC, 2);

	if (gmux_data->switch_state_display == VGA_SWITCHEROO_IGD)
		gmux_write8(gmux_data, GMUX_PORT_SWITCH_DISPLAY, 2);
	else
		gmux_write8(gmux_data, GMUX_PORT_SWITCH_DISPLAY, 3);

	if (gmux_data->switch_state_external == VGA_SWITCHEROO_IGD)
		gmux_write8(gmux_data, GMUX_PORT_SWITCH_EXTERNAL, 2);
	else
		gmux_write8(gmux_data, GMUX_PORT_SWITCH_EXTERNAL, 3);
}

static int gmux_switchto(enum vga_switcheroo_client_id id)
{
	apple_gmux_data->switch_state_ddc = id;
	apple_gmux_data->switch_state_display = id;
	if (apple_gmux_data->external_switchable)
		apple_gmux_data->switch_state_external = id;

	gmux_write_switch_state(apple_gmux_data);

	return 0;
}

static int gmux_switch_ddc(enum vga_switcheroo_client_id id)
{
	enum vga_switcheroo_client_id old_ddc_owner =
		apple_gmux_data->switch_state_ddc;

	if (id == old_ddc_owner)
		return id;

	pr_debug("Switching DDC from %d to %d\n", old_ddc_owner, id);
	apple_gmux_data->switch_state_ddc = id;

	if (id == VGA_SWITCHEROO_IGD)
		gmux_write8(apple_gmux_data, GMUX_PORT_SWITCH_DDC, 1);
	else
		gmux_write8(apple_gmux_data, GMUX_PORT_SWITCH_DDC, 2);

	return old_ddc_owner;
}

 

static int gmux_set_discrete_state(struct apple_gmux_data *gmux_data,
				   enum vga_switcheroo_state state)
{
	reinit_completion(&gmux_data->powerchange_done);

	if (state == VGA_SWITCHEROO_ON) {
		gmux_write8(gmux_data, GMUX_PORT_DISCRETE_POWER, 1);
		gmux_write8(gmux_data, GMUX_PORT_DISCRETE_POWER, 3);
		pr_debug("Discrete card powered up\n");
	} else {
		gmux_write8(gmux_data, GMUX_PORT_DISCRETE_POWER, 1);
		gmux_write8(gmux_data, GMUX_PORT_DISCRETE_POWER, 0);
		pr_debug("Discrete card powered down\n");
	}

	gmux_data->power_state = state;

	if (gmux_data->gpe >= 0 &&
	    !wait_for_completion_interruptible_timeout(&gmux_data->powerchange_done,
						       msecs_to_jiffies(200)))
		pr_warn("Timeout waiting for gmux switch to complete\n");

	return 0;
}

static int gmux_set_power_state(enum vga_switcheroo_client_id id,
				enum vga_switcheroo_state state)
{
	if (id == VGA_SWITCHEROO_IGD)
		return 0;

	return gmux_set_discrete_state(apple_gmux_data, state);
}

static enum vga_switcheroo_client_id gmux_get_client_id(struct pci_dev *pdev)
{
	 
	if (pdev->vendor == PCI_VENDOR_ID_INTEL)
		return VGA_SWITCHEROO_IGD;
	else if (pdev->vendor == PCI_VENDOR_ID_NVIDIA &&
		 pdev->device == 0x0863)
		return VGA_SWITCHEROO_IGD;
	else
		return VGA_SWITCHEROO_DIS;
}

static const struct vga_switcheroo_handler gmux_handler_no_ddc = {
	.switchto = gmux_switchto,
	.power_state = gmux_set_power_state,
	.get_client_id = gmux_get_client_id,
};

static const struct vga_switcheroo_handler gmux_handler_ddc = {
	.switchto = gmux_switchto,
	.switch_ddc = gmux_switch_ddc,
	.power_state = gmux_set_power_state,
	.get_client_id = gmux_get_client_id,
};

static const struct apple_gmux_config apple_gmux_pio = {
	.read8 = &gmux_pio_read8,
	.write8 = &gmux_pio_write8,
	.read32 = &gmux_pio_read32,
	.write32 = &gmux_pio_write32,
	.gmux_handler = &gmux_handler_ddc,
	.handler_flags = VGA_SWITCHEROO_CAN_SWITCH_DDC,
	.resource_type = IORESOURCE_IO,
	.read_version_as_u32 = false,
	.name = "classic"
};

static const struct apple_gmux_config apple_gmux_index = {
	.read8 = &gmux_index_read8,
	.write8 = &gmux_index_write8,
	.read32 = &gmux_index_read32,
	.write32 = &gmux_index_write32,
	.gmux_handler = &gmux_handler_no_ddc,
	.handler_flags = VGA_SWITCHEROO_NEEDS_EDP_CONFIG,
	.resource_type = IORESOURCE_IO,
	.read_version_as_u32 = true,
	.name = "indexed"
};

static const struct apple_gmux_config apple_gmux_mmio = {
	.read8 = &gmux_mmio_read8,
	.write8 = &gmux_mmio_write8,
	.read32 = &gmux_mmio_read32,
	.write32 = &gmux_mmio_write32,
	.gmux_handler = &gmux_handler_no_ddc,
	.handler_flags = VGA_SWITCHEROO_NEEDS_EDP_CONFIG,
	.resource_type = IORESOURCE_MEM,
	.read_version_as_u32 = true,
	.name = "T2"
};


 

static inline void gmux_disable_interrupts(struct apple_gmux_data *gmux_data)
{
	gmux_write8(gmux_data, GMUX_PORT_INTERRUPT_ENABLE,
		    GMUX_INTERRUPT_DISABLE);
}

static inline void gmux_enable_interrupts(struct apple_gmux_data *gmux_data)
{
	gmux_write8(gmux_data, GMUX_PORT_INTERRUPT_ENABLE,
		    GMUX_INTERRUPT_ENABLE);
}

static inline u8 gmux_interrupt_get_status(struct apple_gmux_data *gmux_data)
{
	return gmux_read8(gmux_data, GMUX_PORT_INTERRUPT_STATUS);
}

static void gmux_clear_interrupts(struct apple_gmux_data *gmux_data)
{
	u8 status;

	 
	status = gmux_interrupt_get_status(gmux_data);
	gmux_write8(gmux_data, GMUX_PORT_INTERRUPT_STATUS, status);
	 
	if (gmux_data->config == &apple_gmux_mmio)
		acpi_execute_simple_method(gmux_data->dhandle, "GMSP", 0);
}

static void gmux_notify_handler(acpi_handle device, u32 value, void *context)
{
	u8 status;
	struct pnp_dev *pnp = (struct pnp_dev *)context;
	struct apple_gmux_data *gmux_data = pnp_get_drvdata(pnp);

	status = gmux_interrupt_get_status(gmux_data);
	gmux_disable_interrupts(gmux_data);
	pr_debug("Notify handler called: status %d\n", status);

	gmux_clear_interrupts(gmux_data);
	gmux_enable_interrupts(gmux_data);

	if (status & GMUX_INTERRUPT_STATUS_POWER)
		complete(&gmux_data->powerchange_done);
}

 

static ssize_t gmux_selected_port_data_write(struct file *file,
		const char __user *userbuf, size_t count, loff_t *ppos)
{
	struct apple_gmux_data *gmux_data = file->private_data;

	if (*ppos)
		return -EINVAL;

	if (count == 1) {
		u8 data;

		if (copy_from_user(&data, userbuf, 1))
			return -EFAULT;

		gmux_write8(gmux_data, gmux_data->selected_port, data);
	} else if (count == 4) {
		u32 data;

		if (copy_from_user(&data, userbuf, 4))
			return -EFAULT;

		gmux_write32(gmux_data, gmux_data->selected_port, data);
	} else
		return -EINVAL;

	return count;
}

static ssize_t gmux_selected_port_data_read(struct file *file,
		char __user *userbuf, size_t count, loff_t *ppos)
{
	struct apple_gmux_data *gmux_data = file->private_data;
	u32 data;

	data = gmux_read32(gmux_data, gmux_data->selected_port);

	return simple_read_from_buffer(userbuf, count, ppos, &data, sizeof(data));
}

static const struct file_operations gmux_port_data_ops = {
	.open = simple_open,
	.write = gmux_selected_port_data_write,
	.read = gmux_selected_port_data_read
};

static void gmux_init_debugfs(struct apple_gmux_data *gmux_data)
{
	gmux_data->debug_dentry = debugfs_create_dir(KBUILD_MODNAME, NULL);

	debugfs_create_u8("selected_port", 0644, gmux_data->debug_dentry,
			&gmux_data->selected_port);
	debugfs_create_file("selected_port_data", 0644, gmux_data->debug_dentry,
			gmux_data, &gmux_port_data_ops);
}

static void gmux_fini_debugfs(struct apple_gmux_data *gmux_data)
{
	debugfs_remove_recursive(gmux_data->debug_dentry);
}

static int gmux_suspend(struct device *dev)
{
	struct pnp_dev *pnp = to_pnp_dev(dev);
	struct apple_gmux_data *gmux_data = pnp_get_drvdata(pnp);

	gmux_disable_interrupts(gmux_data);
	return 0;
}

static int gmux_resume(struct device *dev)
{
	struct pnp_dev *pnp = to_pnp_dev(dev);
	struct apple_gmux_data *gmux_data = pnp_get_drvdata(pnp);

	gmux_enable_interrupts(gmux_data);
	gmux_write_switch_state(gmux_data);
	if (gmux_data->power_state == VGA_SWITCHEROO_OFF)
		gmux_set_discrete_state(gmux_data, gmux_data->power_state);
	return 0;
}

static int is_thunderbolt(struct device *dev, void *data)
{
	return to_pci_dev(dev)->is_thunderbolt;
}

static int gmux_probe(struct pnp_dev *pnp, const struct pnp_device_id *id)
{
	struct apple_gmux_data *gmux_data;
	struct resource *res;
	struct backlight_properties props;
	struct backlight_device *bdev = NULL;
	u8 ver_major, ver_minor, ver_release;
	bool register_bdev = true;
	int ret = -ENXIO;
	acpi_status status;
	unsigned long long gpe;
	enum apple_gmux_type type;
	u32 version;

	if (apple_gmux_data)
		return -EBUSY;

	if (!apple_gmux_detect(pnp, &type)) {
		pr_info("gmux device not present\n");
		return -ENODEV;
	}

	gmux_data = kzalloc(sizeof(*gmux_data), GFP_KERNEL);
	if (!gmux_data)
		return -ENOMEM;
	pnp_set_drvdata(pnp, gmux_data);

	switch (type) {
	case APPLE_GMUX_TYPE_MMIO:
		gmux_data->config = &apple_gmux_mmio;
		mutex_init(&gmux_data->index_lock);

		res = pnp_get_resource(pnp, IORESOURCE_MEM, 0);
		gmux_data->iostart = res->start;
		 
		gmux_data->iolen = 16;
		if (!request_mem_region(gmux_data->iostart, gmux_data->iolen,
					"Apple gmux")) {
			pr_err("gmux I/O already in use\n");
			goto err_free;
		}
		gmux_data->iomem_base = ioremap(gmux_data->iostart, gmux_data->iolen);
		if (!gmux_data->iomem_base) {
			pr_err("couldn't remap gmux mmio region");
			goto err_release;
		}
		goto get_version;
	case APPLE_GMUX_TYPE_INDEXED:
		gmux_data->config = &apple_gmux_index;
		mutex_init(&gmux_data->index_lock);
		break;
	case APPLE_GMUX_TYPE_PIO:
		gmux_data->config = &apple_gmux_pio;
		break;
	}

	res = pnp_get_resource(pnp, IORESOURCE_IO, 0);
	gmux_data->iostart = res->start;
	gmux_data->iolen = resource_size(res);

	if (!request_region(gmux_data->iostart, gmux_data->iolen,
			    "Apple gmux")) {
		pr_err("gmux I/O already in use\n");
		goto err_free;
	}

get_version:
	if (gmux_data->config->read_version_as_u32) {
		version = gmux_read32(gmux_data, GMUX_PORT_VERSION_MAJOR);
		ver_major = (version >> 24) & 0xff;
		ver_minor = (version >> 16) & 0xff;
		ver_release = (version >> 8) & 0xff;
	} else {
		ver_major = gmux_read8(gmux_data, GMUX_PORT_VERSION_MAJOR);
		ver_minor = gmux_read8(gmux_data, GMUX_PORT_VERSION_MINOR);
		ver_release = gmux_read8(gmux_data, GMUX_PORT_VERSION_RELEASE);
	}
	pr_info("Found gmux version %d.%d.%d [%s]\n", ver_major, ver_minor,
		ver_release, gmux_data->config->name);

	memset(&props, 0, sizeof(props));
	props.type = BACKLIGHT_PLATFORM;

	 
	if (type == APPLE_GMUX_TYPE_MMIO)
		props.max_brightness = MMIO_GMUX_MAX_BRIGHTNESS;
	else
		props.max_brightness = gmux_read32(gmux_data, GMUX_PORT_MAX_BRIGHTNESS);

#if IS_REACHABLE(CONFIG_ACPI_VIDEO)
	register_bdev = acpi_video_get_backlight_type() == acpi_backlight_apple_gmux;
#endif
	if (register_bdev) {
		 
		if (WARN_ON(props.max_brightness > GMUX_MAX_BRIGHTNESS))
			props.max_brightness = GMUX_MAX_BRIGHTNESS;

		bdev = backlight_device_register("gmux_backlight", &pnp->dev,
						 gmux_data, &gmux_bl_ops, &props);
		if (IS_ERR(bdev)) {
			ret = PTR_ERR(bdev);
			goto err_unmap;
		}

		gmux_data->bdev = bdev;
		bdev->props.brightness = gmux_get_brightness(bdev);
		backlight_update_status(bdev);
	}

	gmux_data->power_state = VGA_SWITCHEROO_ON;

	gmux_data->dhandle = ACPI_HANDLE(&pnp->dev);
	if (!gmux_data->dhandle) {
		pr_err("Cannot find acpi handle for pnp device %s\n",
		       dev_name(&pnp->dev));
		ret = -ENODEV;
		goto err_notify;
	}

	status = acpi_evaluate_integer(gmux_data->dhandle, "GMGP", NULL, &gpe);
	if (ACPI_SUCCESS(status)) {
		gmux_data->gpe = (int)gpe;

		status = acpi_install_notify_handler(gmux_data->dhandle,
						     ACPI_DEVICE_NOTIFY,
						     &gmux_notify_handler, pnp);
		if (ACPI_FAILURE(status)) {
			pr_err("Install notify handler failed: %s\n",
			       acpi_format_exception(status));
			ret = -ENODEV;
			goto err_notify;
		}

		status = acpi_enable_gpe(NULL, gmux_data->gpe);
		if (ACPI_FAILURE(status)) {
			pr_err("Cannot enable gpe: %s\n",
			       acpi_format_exception(status));
			goto err_enable_gpe;
		}
	} else {
		pr_warn("No GPE found for gmux\n");
		gmux_data->gpe = -1;
	}

	 
	gmux_data->external_switchable =
		!bus_for_each_dev(&pci_bus_type, NULL, NULL, is_thunderbolt);
	if (!gmux_data->external_switchable)
		gmux_write8(gmux_data, GMUX_PORT_SWITCH_EXTERNAL, 3);

	apple_gmux_data = gmux_data;
	init_completion(&gmux_data->powerchange_done);
	gmux_enable_interrupts(gmux_data);
	gmux_read_switch_state(gmux_data);

	 
	ret = vga_switcheroo_register_handler(gmux_data->config->gmux_handler,
			gmux_data->config->handler_flags);
	if (ret) {
		pr_err("Failed to register vga_switcheroo handler\n");
		goto err_register_handler;
	}

	gmux_init_debugfs(gmux_data);
	return 0;

err_register_handler:
	gmux_disable_interrupts(gmux_data);
	apple_gmux_data = NULL;
	if (gmux_data->gpe >= 0)
		acpi_disable_gpe(NULL, gmux_data->gpe);
err_enable_gpe:
	if (gmux_data->gpe >= 0)
		acpi_remove_notify_handler(gmux_data->dhandle,
					   ACPI_DEVICE_NOTIFY,
					   &gmux_notify_handler);
err_notify:
	backlight_device_unregister(bdev);
err_unmap:
	if (gmux_data->iomem_base)
		iounmap(gmux_data->iomem_base);
err_release:
	if (gmux_data->config->resource_type == IORESOURCE_MEM)
		release_mem_region(gmux_data->iostart, gmux_data->iolen);
	else
		release_region(gmux_data->iostart, gmux_data->iolen);
err_free:
	kfree(gmux_data);
	return ret;
}

static void gmux_remove(struct pnp_dev *pnp)
{
	struct apple_gmux_data *gmux_data = pnp_get_drvdata(pnp);

	gmux_fini_debugfs(gmux_data);
	vga_switcheroo_unregister_handler();
	gmux_disable_interrupts(gmux_data);
	if (gmux_data->gpe >= 0) {
		acpi_disable_gpe(NULL, gmux_data->gpe);
		acpi_remove_notify_handler(gmux_data->dhandle,
					   ACPI_DEVICE_NOTIFY,
					   &gmux_notify_handler);
	}

	backlight_device_unregister(gmux_data->bdev);

	if (gmux_data->iomem_base) {
		iounmap(gmux_data->iomem_base);
		release_mem_region(gmux_data->iostart, gmux_data->iolen);
	} else
		release_region(gmux_data->iostart, gmux_data->iolen);
	apple_gmux_data = NULL;
	kfree(gmux_data);
}

static const struct pnp_device_id gmux_device_ids[] = {
	{GMUX_ACPI_HID, 0},
	{"", 0}
};

static const struct dev_pm_ops gmux_dev_pm_ops = {
	.suspend = gmux_suspend,
	.resume = gmux_resume,
};

static struct pnp_driver gmux_pnp_driver = {
	.name		= "apple-gmux",
	.probe		= gmux_probe,
	.remove		= gmux_remove,
	.id_table	= gmux_device_ids,
	.driver		= {
			.pm = &gmux_dev_pm_ops,
	},
};

module_pnp_driver(gmux_pnp_driver);
MODULE_AUTHOR("Seth Forshee <seth.forshee@canonical.com>");
MODULE_DESCRIPTION("Apple Gmux Driver");
MODULE_LICENSE("GPL");
MODULE_DEVICE_TABLE(pnp, gmux_device_ids);
