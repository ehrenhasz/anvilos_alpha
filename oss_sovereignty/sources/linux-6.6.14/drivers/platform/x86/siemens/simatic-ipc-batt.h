


#ifndef _SIMATIC_IPC_BATT_H
#define _SIMATIC_IPC_BATT_H

int simatic_ipc_batt_probe(struct platform_device *pdev,
			   struct gpiod_lookup_table *table);

int simatic_ipc_batt_remove(struct platform_device *pdev,
			    struct gpiod_lookup_table *table);

#endif 
