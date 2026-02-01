
 

#define dev_fmt(fmt) "EDR: " fmt

#include <linux/pci.h>
#include <linux/pci-acpi.h>

#include "portdrv.h"
#include "../pci.h"

#define EDR_PORT_DPC_ENABLE_DSM		0x0C
#define EDR_PORT_LOCATE_DSM		0x0D
#define EDR_OST_SUCCESS			0x80
#define EDR_OST_FAILED			0x81

 
static int acpi_enable_dpc(struct pci_dev *pdev)
{
	struct acpi_device *adev = ACPI_COMPANION(&pdev->dev);
	union acpi_object *obj, argv4, req;
	int status = 0;

	 
	if (!acpi_check_dsm(adev->handle, &pci_acpi_dsm_guid, 5,
			    1ULL << EDR_PORT_DPC_ENABLE_DSM))
		return 0;

	req.type = ACPI_TYPE_INTEGER;
	req.integer.value = 1;

	argv4.type = ACPI_TYPE_PACKAGE;
	argv4.package.count = 1;
	argv4.package.elements = &req;

	 
	obj = acpi_evaluate_dsm(adev->handle, &pci_acpi_dsm_guid, 5,
				EDR_PORT_DPC_ENABLE_DSM, &argv4);
	if (!obj)
		return 0;

	if (obj->type != ACPI_TYPE_INTEGER) {
		pci_err(pdev, FW_BUG "Enable DPC _DSM returned non integer\n");
		status = -EIO;
	}

	if (obj->integer.value != 1) {
		pci_err(pdev, "Enable DPC _DSM failed to enable DPC\n");
		status = -EIO;
	}

	ACPI_FREE(obj);

	return status;
}

 
static struct pci_dev *acpi_dpc_port_get(struct pci_dev *pdev)
{
	struct acpi_device *adev = ACPI_COMPANION(&pdev->dev);
	union acpi_object *obj;
	u16 port;

	 
	if (!acpi_check_dsm(adev->handle, &pci_acpi_dsm_guid, 5,
			    1ULL << EDR_PORT_LOCATE_DSM))
		return pci_dev_get(pdev);

	obj = acpi_evaluate_dsm(adev->handle, &pci_acpi_dsm_guid, 5,
				EDR_PORT_LOCATE_DSM, NULL);
	if (!obj)
		return pci_dev_get(pdev);

	if (obj->type != ACPI_TYPE_INTEGER) {
		ACPI_FREE(obj);
		pci_err(pdev, FW_BUG "Locate Port _DSM returned non integer\n");
		return NULL;
	}

	 
	port = obj->integer.value;

	ACPI_FREE(obj);

	return pci_get_domain_bus_and_slot(pci_domain_nr(pdev->bus),
					   PCI_BUS_NUM(port), port & 0xff);
}

 
static int acpi_send_edr_status(struct pci_dev *pdev, struct pci_dev *edev,
				u16 status)
{
	struct acpi_device *adev = ACPI_COMPANION(&pdev->dev);
	u32 ost_status;

	pci_dbg(pdev, "Status for %s: %#x\n", pci_name(edev), status);

	ost_status = PCI_DEVID(edev->bus->number, edev->devfn) << 16;
	ost_status |= status;

	status = acpi_evaluate_ost(adev->handle, ACPI_NOTIFY_DISCONNECT_RECOVER,
				   ost_status, NULL);
	if (ACPI_FAILURE(status))
		return -EINVAL;

	return 0;
}

static void edr_handle_event(acpi_handle handle, u32 event, void *data)
{
	struct pci_dev *pdev = data, *edev;
	pci_ers_result_t estate = PCI_ERS_RESULT_DISCONNECT;
	u16 status;

	if (event != ACPI_NOTIFY_DISCONNECT_RECOVER)
		return;

	 
	pci_info(pdev, "EDR event received\n");

	 
	edev = acpi_dpc_port_get(pdev);
	if (!edev) {
		pci_err(pdev, "Firmware failed to locate DPC port\n");
		return;
	}

	pci_dbg(pdev, "Reported EDR dev: %s\n", pci_name(edev));

	 
	if (!edev->dpc_cap) {
		pci_err(edev, FW_BUG "This device doesn't support DPC\n");
		goto send_ost;
	}

	 
	pci_read_config_word(edev, edev->dpc_cap + PCI_EXP_DPC_STATUS, &status);
	if (!(status & PCI_EXP_DPC_STATUS_TRIGGER)) {
		pci_err(edev, "Invalid DPC trigger %#010x\n", status);
		goto send_ost;
	}

	dpc_process_error(edev);
	pci_aer_raw_clear_status(edev);

	 
	estate = pcie_do_recovery(edev, pci_channel_io_frozen, dpc_reset_link);

send_ost:

	 
	if (estate == PCI_ERS_RESULT_RECOVERED) {
		pci_dbg(edev, "DPC port successfully recovered\n");
		pcie_clear_device_status(edev);
		acpi_send_edr_status(pdev, edev, EDR_OST_SUCCESS);
	} else {
		pci_dbg(edev, "DPC port recovery failed\n");
		acpi_send_edr_status(pdev, edev, EDR_OST_FAILED);
	}

	pci_dev_put(edev);
}

void pci_acpi_add_edr_notifier(struct pci_dev *pdev)
{
	struct acpi_device *adev = ACPI_COMPANION(&pdev->dev);
	acpi_status status;

	if (!adev) {
		pci_dbg(pdev, "No valid ACPI node, skipping EDR init\n");
		return;
	}

	status = acpi_install_notify_handler(adev->handle, ACPI_SYSTEM_NOTIFY,
					     edr_handle_event, pdev);
	if (ACPI_FAILURE(status)) {
		pci_err(pdev, "Failed to install notify handler\n");
		return;
	}

	if (acpi_enable_dpc(pdev))
		acpi_remove_notify_handler(adev->handle, ACPI_SYSTEM_NOTIFY,
					   edr_handle_event);
	else
		pci_dbg(pdev, "Notify handler installed\n");
}

void pci_acpi_remove_edr_notifier(struct pci_dev *pdev)
{
	struct acpi_device *adev = ACPI_COMPANION(&pdev->dev);

	if (!adev)
		return;

	acpi_remove_notify_handler(adev->handle, ACPI_SYSTEM_NOTIFY,
				   edr_handle_event);
	pci_dbg(pdev, "Notify handler removed\n");
}
