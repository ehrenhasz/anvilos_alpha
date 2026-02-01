
 
#include <linux/thread_info.h>
#include <asm/apic.h>

#include "local.h"

 
void __init x86_64_probe_apic(void)
{
	struct apic **drv;

	enable_IR_x2apic();

	for (drv = __apicdrivers; drv < __apicdrivers_end; drv++) {
		if ((*drv)->probe && (*drv)->probe()) {
			apic_install_driver(*drv);
			break;
		}
	}
}

int __init default_acpi_madt_oem_check(char *oem_id, char *oem_table_id)
{
	struct apic **drv;

	for (drv = __apicdrivers; drv < __apicdrivers_end; drv++) {
		if ((*drv)->acpi_madt_oem_check(oem_id, oem_table_id)) {
			apic_install_driver(*drv);
			return 1;
		}
	}
	return 0;
}
