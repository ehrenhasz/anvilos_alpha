


#include <linux/module.h>
#include <linux/printk.h>
#include "watermark.h"
#include <nfit.h>

nfit_test_watermark(acpi_nfit);

 
void nfit_intel_shutdown_status(struct nfit_mem *nfit_mem)
{
	set_bit(NFIT_MEM_DIRTY_COUNT, &nfit_mem->flags);
	nfit_mem->dirty_shutdown = 42;
}
