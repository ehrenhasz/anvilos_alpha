
 

#define pr_fmt(fmt)	"ACPI: APMT: " fmt

#include <linux/acpi.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include "init.h"

#define DEV_NAME "arm-cs-arch-pmu"

 
#define DEV_MAX_RESOURCE_COUNT 3

 
static struct acpi_table_header *apmt_table;

static int __init apmt_init_resources(struct resource *res,
				      struct acpi_apmt_node *node)
{
	int irq, trigger;
	int num_res = 0;

	res[num_res].start = node->base_address0;
	res[num_res].end = node->base_address0 + SZ_4K - 1;
	res[num_res].flags = IORESOURCE_MEM;

	num_res++;

	if (node->flags & ACPI_APMT_FLAGS_DUAL_PAGE) {
		res[num_res].start = node->base_address1;
		res[num_res].end = node->base_address1 + SZ_4K - 1;
		res[num_res].flags = IORESOURCE_MEM;

		num_res++;
	}

	if (node->ovflw_irq != 0) {
		trigger = (node->ovflw_irq_flags & ACPI_APMT_OVFLW_IRQ_FLAGS_MODE);
		trigger = (trigger == ACPI_APMT_OVFLW_IRQ_FLAGS_MODE_LEVEL) ?
			ACPI_LEVEL_SENSITIVE : ACPI_EDGE_SENSITIVE;
		irq = acpi_register_gsi(NULL, node->ovflw_irq, trigger,
						ACPI_ACTIVE_HIGH);

		if (irq <= 0) {
			pr_warn("APMT could not register gsi hwirq %d\n", irq);
			return num_res;
		}

		res[num_res].start = irq;
		res[num_res].end = irq;
		res[num_res].flags = IORESOURCE_IRQ;

		num_res++;
	}

	return num_res;
}

 
static int __init apmt_add_platform_device(struct acpi_apmt_node *node,
					   struct fwnode_handle *fwnode)
{
	struct platform_device *pdev;
	int ret, count;
	struct resource res[DEV_MAX_RESOURCE_COUNT];

	pdev = platform_device_alloc(DEV_NAME, PLATFORM_DEVID_AUTO);
	if (!pdev)
		return -ENOMEM;

	memset(res, 0, sizeof(res));

	count = apmt_init_resources(res, node);

	ret = platform_device_add_resources(pdev, res, count);
	if (ret)
		goto dev_put;

	 
	ret = platform_device_add_data(pdev, &node, sizeof(node));
	if (ret)
		goto dev_put;

	pdev->dev.fwnode = fwnode;

	ret = platform_device_add(pdev);

	if (ret)
		goto dev_put;

	return 0;

dev_put:
	platform_device_put(pdev);

	return ret;
}

static int __init apmt_init_platform_devices(void)
{
	struct acpi_apmt_node *apmt_node;
	struct acpi_table_apmt *apmt;
	struct fwnode_handle *fwnode;
	u64 offset, end;
	int ret;

	 
	apmt = (struct acpi_table_apmt *)apmt_table;
	offset = sizeof(*apmt);
	end = apmt->header.length;

	while (offset < end) {
		apmt_node = ACPI_ADD_PTR(struct acpi_apmt_node, apmt,
				 offset);

		fwnode = acpi_alloc_fwnode_static();
		if (!fwnode)
			return -ENOMEM;

		ret = apmt_add_platform_device(apmt_node, fwnode);
		if (ret) {
			acpi_free_fwnode_static(fwnode);
			return ret;
		}

		offset += apmt_node->length;
	}

	return 0;
}

void __init acpi_apmt_init(void)
{
	acpi_status status;
	int ret;

	 
	status = acpi_get_table(ACPI_SIG_APMT, 0, &apmt_table);

	if (ACPI_FAILURE(status)) {
		if (status != AE_NOT_FOUND) {
			const char *msg = acpi_format_exception(status);

			pr_err("Failed to get APMT table, %s\n", msg);
		}

		return;
	}

	ret = apmt_init_platform_devices();
	if (ret) {
		pr_err("Failed to initialize APMT platform devices, ret: %d\n", ret);
		acpi_put_table(apmt_table);
	}
}
