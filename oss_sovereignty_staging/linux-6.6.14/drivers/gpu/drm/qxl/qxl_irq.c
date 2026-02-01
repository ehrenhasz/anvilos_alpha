 

#include <linux/pci.h>

#include <drm/drm_drv.h>

#include "qxl_drv.h"

static irqreturn_t qxl_irq_handler(int irq, void *arg)
{
	struct drm_device *dev = (struct drm_device *) arg;
	struct qxl_device *qdev = to_qxl(dev);
	uint32_t pending;

	pending = xchg(&qdev->ram_header->int_pending, 0);

	if (!pending)
		return IRQ_NONE;

	atomic_inc(&qdev->irq_received);

	if (pending & QXL_INTERRUPT_DISPLAY) {
		atomic_inc(&qdev->irq_received_display);
		wake_up_all(&qdev->display_event);
		qxl_queue_garbage_collect(qdev, false);
	}
	if (pending & QXL_INTERRUPT_CURSOR) {
		atomic_inc(&qdev->irq_received_cursor);
		wake_up_all(&qdev->cursor_event);
	}
	if (pending & QXL_INTERRUPT_IO_CMD) {
		atomic_inc(&qdev->irq_received_io_cmd);
		wake_up_all(&qdev->io_cmd_event);
	}
	if (pending & QXL_INTERRUPT_ERROR) {
		 
		qdev->irq_received_error++;
		DRM_WARN("driver is in bug mode\n");
	}
	if (pending & QXL_INTERRUPT_CLIENT_MONITORS_CONFIG) {
		schedule_work(&qdev->client_monitors_config_work);
	}
	qdev->ram_header->int_mask = QXL_INTERRUPT_MASK;
	outb(0, qdev->io_base + QXL_IO_UPDATE_IRQ);
	return IRQ_HANDLED;
}

static void qxl_client_monitors_config_work_func(struct work_struct *work)
{
	struct qxl_device *qdev = container_of(work, struct qxl_device,
					       client_monitors_config_work);

	qxl_display_read_client_monitors_config(qdev);
}

int qxl_irq_init(struct qxl_device *qdev)
{
	struct drm_device *ddev = &qdev->ddev;
	struct pci_dev *pdev = to_pci_dev(ddev->dev);
	int ret;

	init_waitqueue_head(&qdev->display_event);
	init_waitqueue_head(&qdev->cursor_event);
	init_waitqueue_head(&qdev->io_cmd_event);
	init_waitqueue_head(&qdev->release_event);
	INIT_WORK(&qdev->client_monitors_config_work,
		  qxl_client_monitors_config_work_func);
	atomic_set(&qdev->irq_received, 0);
	atomic_set(&qdev->irq_received_display, 0);
	atomic_set(&qdev->irq_received_cursor, 0);
	atomic_set(&qdev->irq_received_io_cmd, 0);
	qdev->irq_received_error = 0;
	ret = request_irq(pdev->irq, qxl_irq_handler, IRQF_SHARED, ddev->driver->name, ddev);
	qdev->ram_header->int_mask = QXL_INTERRUPT_MASK;
	if (unlikely(ret != 0)) {
		DRM_ERROR("Failed installing irq: %d\n", ret);
		return 1;
	}
	return 0;
}
