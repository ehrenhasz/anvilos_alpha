#ifndef __DAL_IRQ_SERVICE_DCE60_H__
#define __DAL_IRQ_SERVICE_DCE60_H__
#include "../irq_service.h"
enum dc_irq_source to_dal_irq_source_dce60(
		struct irq_service *irq_service,
		uint32_t src_id,
		uint32_t ext_id);
struct irq_service *dal_irq_service_dce60_create(
	struct irq_service_init_data *init_data);
#endif
