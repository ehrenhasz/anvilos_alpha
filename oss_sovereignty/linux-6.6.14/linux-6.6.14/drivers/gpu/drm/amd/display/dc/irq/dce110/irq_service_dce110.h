#ifndef __DAL_IRQ_SERVICE_DCE110_H__
#define __DAL_IRQ_SERVICE_DCE110_H__
#include "../irq_service.h"
struct irq_service *dal_irq_service_dce110_create(
	struct irq_service_init_data *init_data);
enum dc_irq_source to_dal_irq_source_dce110(
		struct irq_service *irq_service,
		uint32_t src_id,
		uint32_t ext_id);
bool dal_irq_service_dummy_set(
	struct irq_service *irq_service,
	const struct irq_source_info *info,
	bool enable);
bool dal_irq_service_dummy_ack(
	struct irq_service *irq_service,
	const struct irq_source_info *info);
bool dce110_vblank_set(
	struct irq_service *irq_service,
	const struct irq_source_info *info,
	bool enable);
#endif
