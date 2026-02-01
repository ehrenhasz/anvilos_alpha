
 

#include <linux/acpi.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/platform_device.h>

static const struct acpi_gpio_params shps_base_presence_int   = { 0, 0, false };
static const struct acpi_gpio_params shps_base_presence       = { 1, 0, false };
static const struct acpi_gpio_params shps_device_power_int    = { 2, 0, false };
static const struct acpi_gpio_params shps_device_power        = { 3, 0, false };
static const struct acpi_gpio_params shps_device_presence_int = { 4, 0, false };
static const struct acpi_gpio_params shps_device_presence     = { 5, 0, false };

static const struct acpi_gpio_mapping shps_acpi_gpios[] = {
	{ "base_presence-int-gpio",   &shps_base_presence_int,   1 },
	{ "base_presence-gpio",       &shps_base_presence,       1 },
	{ "device_power-int-gpio",    &shps_device_power_int,    1 },
	{ "device_power-gpio",        &shps_device_power,        1 },
	{ "device_presence-int-gpio", &shps_device_presence_int, 1 },
	{ "device_presence-gpio",     &shps_device_presence,     1 },
	{ },
};

 
static const guid_t shps_dsm_guid =
	GUID_INIT(0x5515a847, 0xed55, 0x4b27, 0x83, 0x52, 0xcd, 0x32, 0x0e, 0x10, 0x36, 0x0a);

#define SHPS_DSM_REVISION		1

enum shps_dsm_fn {
	SHPS_DSM_FN_PCI_NUM_ENTRIES	= 0x01,
	SHPS_DSM_FN_PCI_GET_ENTRIES	= 0x02,
	SHPS_DSM_FN_IRQ_BASE_PRESENCE	= 0x03,
	SHPS_DSM_FN_IRQ_DEVICE_POWER	= 0x04,
	SHPS_DSM_FN_IRQ_DEVICE_PRESENCE	= 0x05,
};

enum shps_irq_type {
	 
	SHPS_IRQ_TYPE_BASE_PRESENCE	= 0,
	SHPS_IRQ_TYPE_DEVICE_POWER	= 1,
	SHPS_IRQ_TYPE_DEVICE_PRESENCE	= 2,
	SHPS_NUM_IRQS,
};

static const char *const shps_gpio_names[] = {
	[SHPS_IRQ_TYPE_BASE_PRESENCE]	= "base_presence",
	[SHPS_IRQ_TYPE_DEVICE_POWER]	= "device_power",
	[SHPS_IRQ_TYPE_DEVICE_PRESENCE]	= "device_presence",
};

struct shps_device {
	struct mutex lock[SHPS_NUM_IRQS];   
	struct gpio_desc *gpio[SHPS_NUM_IRQS];
	unsigned int irq[SHPS_NUM_IRQS];
};

#define SHPS_IRQ_NOT_PRESENT		((unsigned int)-1)

static enum shps_dsm_fn shps_dsm_fn_for_irq(enum shps_irq_type type)
{
	return SHPS_DSM_FN_IRQ_BASE_PRESENCE + type;
}

static void shps_dsm_notify_irq(struct platform_device *pdev, enum shps_irq_type type)
{
	struct shps_device *sdev = platform_get_drvdata(pdev);
	acpi_handle handle = ACPI_HANDLE(&pdev->dev);
	union acpi_object *result;
	union acpi_object param;
	int value;

	mutex_lock(&sdev->lock[type]);

	value = gpiod_get_value_cansleep(sdev->gpio[type]);
	if (value < 0) {
		mutex_unlock(&sdev->lock[type]);
		dev_err(&pdev->dev, "failed to get gpio: %d (irq=%d)\n", type, value);
		return;
	}

	dev_dbg(&pdev->dev, "IRQ notification via DSM (irq=%d, value=%d)\n", type, value);

	param.type = ACPI_TYPE_INTEGER;
	param.integer.value = value;

	result = acpi_evaluate_dsm_typed(handle, &shps_dsm_guid, SHPS_DSM_REVISION,
					 shps_dsm_fn_for_irq(type), &param, ACPI_TYPE_BUFFER);
	if (!result) {
		dev_err(&pdev->dev, "IRQ notification via DSM failed (irq=%d, gpio=%d)\n",
			type, value);

	} else if (result->buffer.length != 1 || result->buffer.pointer[0] != 0) {
		dev_err(&pdev->dev,
			"IRQ notification via DSM failed: unexpected result value (irq=%d, gpio=%d)\n",
			type, value);
	}

	mutex_unlock(&sdev->lock[type]);

	ACPI_FREE(result);
}

static irqreturn_t shps_handle_irq(int irq, void *data)
{
	struct platform_device *pdev = data;
	struct shps_device *sdev = platform_get_drvdata(pdev);
	int type;

	 
	for (type = 0; type < SHPS_NUM_IRQS; type++)
		if (irq == sdev->irq[type])
			break;

	 
	if (WARN(type >= SHPS_NUM_IRQS, "invalid IRQ number: %d\n", irq))
		return IRQ_HANDLED;

	 
	shps_dsm_notify_irq(pdev, type);
	return IRQ_HANDLED;
}

static int shps_setup_irq(struct platform_device *pdev, enum shps_irq_type type)
{
	unsigned long flags = IRQF_ONESHOT | IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING;
	struct shps_device *sdev = platform_get_drvdata(pdev);
	struct gpio_desc *gpiod;
	acpi_handle handle = ACPI_HANDLE(&pdev->dev);
	const char *irq_name;
	const int dsm = shps_dsm_fn_for_irq(type);
	int status, irq;

	 
	if (!acpi_check_dsm(handle, &shps_dsm_guid, SHPS_DSM_REVISION, BIT(dsm))) {
		dev_dbg(&pdev->dev, "IRQ notification via DSM not present (irq=%d)\n", type);
		return 0;
	}

	gpiod = devm_gpiod_get(&pdev->dev, shps_gpio_names[type], GPIOD_ASIS);
	if (IS_ERR(gpiod))
		return PTR_ERR(gpiod);

	irq = gpiod_to_irq(gpiod);
	if (irq < 0)
		return irq;

	irq_name = devm_kasprintf(&pdev->dev, GFP_KERNEL, "shps-irq-%d", type);
	if (!irq_name)
		return -ENOMEM;

	status = devm_request_threaded_irq(&pdev->dev, irq, NULL, shps_handle_irq,
					   flags, irq_name, pdev);
	if (status)
		return status;

	dev_dbg(&pdev->dev, "set up irq %d as type %d\n", irq, type);

	sdev->gpio[type] = gpiod;
	sdev->irq[type] = irq;

	return 0;
}

static int surface_hotplug_remove(struct platform_device *pdev)
{
	struct shps_device *sdev = platform_get_drvdata(pdev);
	int i;

	 
	for (i = 0; i < SHPS_NUM_IRQS; i++) {
		if (sdev->irq[i] != SHPS_IRQ_NOT_PRESENT)
			disable_irq(sdev->irq[i]);

		mutex_destroy(&sdev->lock[i]);
	}

	return 0;
}

static int surface_hotplug_probe(struct platform_device *pdev)
{
	struct shps_device *sdev;
	int status, i;

	 
	if (gpiod_count(&pdev->dev, NULL) < 0)
		return -ENODEV;

	status = devm_acpi_dev_add_driver_gpios(&pdev->dev, shps_acpi_gpios);
	if (status)
		return status;

	sdev = devm_kzalloc(&pdev->dev, sizeof(*sdev), GFP_KERNEL);
	if (!sdev)
		return -ENOMEM;

	platform_set_drvdata(pdev, sdev);

	 
	for (i = 0; i < SHPS_NUM_IRQS; i++)
		sdev->irq[i] = SHPS_IRQ_NOT_PRESENT;

	 
	for (i = 0; i < SHPS_NUM_IRQS; i++) {
		mutex_init(&sdev->lock[i]);

		status = shps_setup_irq(pdev, i);
		if (status) {
			dev_err(&pdev->dev, "failed to set up IRQ %d: %d\n", i, status);
			goto err;
		}
	}

	 
	for (i = 0; i < SHPS_NUM_IRQS; i++)
		if (sdev->irq[i] != SHPS_IRQ_NOT_PRESENT)
			shps_dsm_notify_irq(pdev, i);

	return 0;

err:
	surface_hotplug_remove(pdev);
	return status;
}

static const struct acpi_device_id surface_hotplug_acpi_match[] = {
	{ "MSHW0153", 0 },
	{ },
};
MODULE_DEVICE_TABLE(acpi, surface_hotplug_acpi_match);

static struct platform_driver surface_hotplug_driver = {
	.probe = surface_hotplug_probe,
	.remove = surface_hotplug_remove,
	.driver = {
		.name = "surface_hotplug",
		.acpi_match_table = surface_hotplug_acpi_match,
		.probe_type = PROBE_PREFER_ASYNCHRONOUS,
	},
};
module_platform_driver(surface_hotplug_driver);

MODULE_AUTHOR("Maximilian Luz <luzmaximilian@gmail.com>");
MODULE_DESCRIPTION("Surface Hot-Plug Signaling Driver for Surface Book Devices");
MODULE_LICENSE("GPL");
