
 
#include <linux/of.h>
#include <linux/pci.h>
#include <linux/string.h>

#include <asm/pci-bridge.h>
#include <asm/rtas.h>
#include <asm/machdep.h>

#include "../pci.h"		 
#include "rpaphp.h"

 
#define RTAS_SLOT_UNISOLATED		-9000
#define RTAS_SLOT_NOT_UNISOLATED	-9001
#define RTAS_SLOT_NOT_USABLE		-9002

static int rtas_get_sensor_errno(int rtas_rc)
{
	switch (rtas_rc) {
	case 0:
		 
		return 0;
	case RTAS_SLOT_UNISOLATED:
	case RTAS_SLOT_NOT_UNISOLATED:
		return -EFAULT;
	case RTAS_SLOT_NOT_USABLE:
		return -ENODEV;
	case RTAS_BUSY:
	case RTAS_EXTENDED_DELAY_MIN...RTAS_EXTENDED_DELAY_MAX:
		return -EBUSY;
	default:
		return rtas_error_rc(rtas_rc);
	}
}

 

static int __rpaphp_get_sensor_state(struct slot *slot, int *state)
{
	int rc;
	int token = rtas_token("get-sensor-state");
	struct pci_dn *pdn;
	struct eeh_pe *pe;
	struct pci_controller *phb = PCI_DN(slot->dn)->phb;

	if (token == RTAS_UNKNOWN_SERVICE)
		return -ENOENT;

	 
	pdn = list_first_entry_or_null(&PCI_DN(phb->dn)->child_list,
					struct pci_dn, list);
	if (!pdn)
		goto fallback;

	pe = eeh_dev_to_pe(pdn->edev);
	if (pe && (pe->state & EEH_PE_RECOVERING)) {
		rc = rtas_call(token, 2, 2, state, DR_ENTITY_SENSE,
			       slot->index);
		return rtas_get_sensor_errno(rc);
	}
fallback:
	return rtas_get_sensor(DR_ENTITY_SENSE, slot->index, state);
}

int rpaphp_get_sensor_state(struct slot *slot, int *state)
{
	int rc;
	int setlevel;

	rc = __rpaphp_get_sensor_state(slot, state);

	if (rc < 0) {
		if (rc == -EFAULT || rc == -EEXIST) {
			dbg("%s: slot must be power up to get sensor-state\n",
			    __func__);

			 
			rc = rtas_set_power_level(slot->power_domain, POWER_ON,
						  &setlevel);
			if (rc < 0) {
				dbg("%s: power on slot[%s] failed rc=%d.\n",
				    __func__, slot->name, rc);
			} else {
				rc = __rpaphp_get_sensor_state(slot, state);
			}
		} else if (rc == -ENODEV)
			info("%s: slot is unusable\n", __func__);
		else
			err("%s failed to get sensor state\n", __func__);
	}
	return rc;
}

 
int rpaphp_enable_slot(struct slot *slot)
{
	int rc, level, state;
	struct pci_bus *bus;

	slot->state = EMPTY;

	 
	rc = rtas_get_power_level(slot->power_domain, &level);
	if (rc)
		return rc;

	 
	rc = rpaphp_get_sensor_state(slot, &state);
	if (rc)
		return rc;

	bus = pci_find_bus_by_node(slot->dn);
	if (!bus) {
		err("%s: no pci_bus for dn %pOF\n", __func__, slot->dn);
		return -EINVAL;
	}

	slot->bus = bus;
	slot->pci_devs = &bus->devices;

	 
	if (state == PRESENT) {
		slot->state = NOT_CONFIGURED;

		 
		if (!slot->dn->child) {
			err("%s: slot[%s]'s device_node doesn't have child for adapter\n",
			    __func__, slot->name);
			return -EINVAL;
		}

		if (list_empty(&bus->devices)) {
			pseries_eeh_init_edev_recursive(PCI_DN(slot->dn));
			pci_hp_add_devices(bus);
		}

		if (!list_empty(&bus->devices)) {
			slot->state = CONFIGURED;
		}

		if (rpaphp_debug) {
			struct pci_dev *dev;
			dbg("%s: pci_devs of slot[%pOF]\n", __func__, slot->dn);
			list_for_each_entry(dev, &bus->devices, bus_list)
				dbg("\t%s\n", pci_name(dev));
		}
	}

	return 0;
}
