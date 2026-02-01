 

#ifndef __DAL_IRQ_SERVICE_INTERFACE_H__
#define __DAL_IRQ_SERVICE_INTERFACE_H__

struct irq_service_init_data {
	struct dc_context *ctx;
};

struct irq_service;

void dal_irq_service_destroy(struct irq_service **irq_service);

bool dal_irq_service_set(
	struct irq_service *irq_service,
	enum dc_irq_source source,
	bool enable);

bool dal_irq_service_ack(
	struct irq_service *irq_service,
	enum dc_irq_source source);

enum dc_irq_source dal_irq_service_to_irq_source(
		struct irq_service *irq_service,
		uint32_t src_id,
		uint32_t ext_id);

#endif
