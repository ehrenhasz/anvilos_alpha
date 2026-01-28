


#ifndef __PSP_DEV_H__
#define __PSP_DEV_H__

#include <linux/device.h>
#include <linux/list.h>
#include <linux/bits.h>
#include <linux/interrupt.h>

#include "sp-dev.h"

#define MAX_PSP_NAME_LEN		16

extern struct psp_device *psp_master;

typedef void (*psp_irq_handler_t)(int, void *, unsigned int);

struct psp_device {
	struct list_head entry;

	struct psp_vdata *vdata;
	char name[MAX_PSP_NAME_LEN];

	struct device *dev;
	struct sp_device *sp;

	void __iomem *io_regs;

	psp_irq_handler_t sev_irq_handler;
	void *sev_irq_data;

	void *sev_data;
	void *tee_data;
	void *platform_access_data;
	void *dbc_data;

	unsigned int capability;
};

void psp_set_sev_irq_handler(struct psp_device *psp, psp_irq_handler_t handler,
			     void *data);
void psp_clear_sev_irq_handler(struct psp_device *psp);

struct psp_device *psp_get_master_device(void);

#define PSP_CAPABILITY_SEV			BIT(0)
#define PSP_CAPABILITY_TEE			BIT(1)
#define PSP_CAPABILITY_PSP_SECURITY_REPORTING	BIT(7)

#define PSP_CAPABILITY_PSP_SECURITY_OFFSET	8

#define PSP_SECURITY_FUSED_PART			BIT(0)
#define PSP_SECURITY_DEBUG_LOCK_ON		BIT(2)
#define PSP_SECURITY_TSME_STATUS		BIT(5)
#define PSP_SECURITY_ANTI_ROLLBACK_STATUS	BIT(7)
#define PSP_SECURITY_RPMC_PRODUCTION_ENABLED	BIT(8)
#define PSP_SECURITY_RPMC_SPIROM_AVAILABLE	BIT(9)
#define PSP_SECURITY_HSP_TPM_AVAILABLE		BIT(10)
#define PSP_SECURITY_ROM_ARMOR_ENFORCED		BIT(11)

#endif 
