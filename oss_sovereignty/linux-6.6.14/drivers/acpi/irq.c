
 
#include <linux/acpi.h>
#include <linux/irq.h>
#include <linux/irqdomain.h>
#include <linux/of.h>

enum acpi_irq_model_id acpi_irq_model;

static struct fwnode_handle *(*acpi_get_gsi_domain_id)(u32 gsi);
static u32 (*acpi_gsi_to_irq_fallback)(u32 gsi);

 
int acpi_gsi_to_irq(u32 gsi, unsigned int *irq)
{
	struct irq_domain *d;

	d = irq_find_matching_fwnode(acpi_get_gsi_domain_id(gsi),
					DOMAIN_BUS_ANY);
	*irq = irq_find_mapping(d, gsi);
	 
	if (!*irq && acpi_gsi_to_irq_fallback)
		*irq = acpi_gsi_to_irq_fallback(gsi);

	return (*irq > 0) ? 0 : -EINVAL;
}
EXPORT_SYMBOL_GPL(acpi_gsi_to_irq);

 
int acpi_register_gsi(struct device *dev, u32 gsi, int trigger,
		      int polarity)
{
	struct irq_fwspec fwspec;
	unsigned int irq;

	fwspec.fwnode = acpi_get_gsi_domain_id(gsi);
	if (WARN_ON(!fwspec.fwnode)) {
		pr_warn("GSI: No registered irqchip, giving up\n");
		return -EINVAL;
	}

	fwspec.param[0] = gsi;
	fwspec.param[1] = acpi_dev_get_irq_type(trigger, polarity);
	fwspec.param_count = 2;

	irq = irq_create_fwspec_mapping(&fwspec);
	if (!irq)
		return -EINVAL;

	return irq;
}
EXPORT_SYMBOL_GPL(acpi_register_gsi);

 
void acpi_unregister_gsi(u32 gsi)
{
	struct irq_domain *d;
	int irq;

	if (WARN_ON(acpi_irq_model == ACPI_IRQ_MODEL_GIC && gsi < 16))
		return;

	d = irq_find_matching_fwnode(acpi_get_gsi_domain_id(gsi),
				     DOMAIN_BUS_ANY);
	irq = irq_find_mapping(d, gsi);
	irq_dispose_mapping(irq);
}
EXPORT_SYMBOL_GPL(acpi_unregister_gsi);

 
static struct fwnode_handle *
acpi_get_irq_source_fwhandle(const struct acpi_resource_source *source,
			     u32 gsi)
{
	struct fwnode_handle *result;
	struct acpi_device *device;
	acpi_handle handle;
	acpi_status status;

	if (!source->string_length)
		return acpi_get_gsi_domain_id(gsi);

	status = acpi_get_handle(NULL, source->string_ptr, &handle);
	if (WARN_ON(ACPI_FAILURE(status)))
		return NULL;

	device = acpi_get_acpi_dev(handle);
	if (WARN_ON(!device))
		return NULL;

	result = &device->fwnode;
	acpi_put_acpi_dev(device);
	return result;
}

 
struct acpi_irq_parse_one_ctx {
	int rc;
	unsigned int index;
	unsigned long *res_flags;
	struct irq_fwspec *fwspec;
};

 
static inline void acpi_irq_parse_one_match(struct fwnode_handle *fwnode,
					    u32 hwirq, u8 triggering,
					    u8 polarity, u8 shareable,
					    u8 wake_capable,
					    struct acpi_irq_parse_one_ctx *ctx)
{
	if (!fwnode)
		return;
	ctx->rc = 0;
	*ctx->res_flags = acpi_dev_irq_flags(triggering, polarity, shareable, wake_capable);
	ctx->fwspec->fwnode = fwnode;
	ctx->fwspec->param[0] = hwirq;
	ctx->fwspec->param[1] = acpi_dev_get_irq_type(triggering, polarity);
	ctx->fwspec->param_count = 2;
}

 
static acpi_status acpi_irq_parse_one_cb(struct acpi_resource *ares,
					 void *context)
{
	struct acpi_irq_parse_one_ctx *ctx = context;
	struct acpi_resource_irq *irq;
	struct acpi_resource_extended_irq *eirq;
	struct fwnode_handle *fwnode;

	switch (ares->type) {
	case ACPI_RESOURCE_TYPE_IRQ:
		irq = &ares->data.irq;
		if (ctx->index >= irq->interrupt_count) {
			ctx->index -= irq->interrupt_count;
			return AE_OK;
		}
		fwnode = acpi_get_gsi_domain_id(irq->interrupts[ctx->index]);
		acpi_irq_parse_one_match(fwnode, irq->interrupts[ctx->index],
					 irq->triggering, irq->polarity,
					 irq->shareable, irq->wake_capable, ctx);
		return AE_CTRL_TERMINATE;
	case ACPI_RESOURCE_TYPE_EXTENDED_IRQ:
		eirq = &ares->data.extended_irq;
		if (eirq->producer_consumer == ACPI_PRODUCER)
			return AE_OK;
		if (ctx->index >= eirq->interrupt_count) {
			ctx->index -= eirq->interrupt_count;
			return AE_OK;
		}
		fwnode = acpi_get_irq_source_fwhandle(&eirq->resource_source,
						      eirq->interrupts[ctx->index]);
		acpi_irq_parse_one_match(fwnode, eirq->interrupts[ctx->index],
					 eirq->triggering, eirq->polarity,
					 eirq->shareable, eirq->wake_capable, ctx);
		return AE_CTRL_TERMINATE;
	}

	return AE_OK;
}

 
static int acpi_irq_parse_one(acpi_handle handle, unsigned int index,
			      struct irq_fwspec *fwspec, unsigned long *flags)
{
	struct acpi_irq_parse_one_ctx ctx = { -EINVAL, index, flags, fwspec };

	acpi_walk_resources(handle, METHOD_NAME__CRS, acpi_irq_parse_one_cb, &ctx);
	return ctx.rc;
}

 
int acpi_irq_get(acpi_handle handle, unsigned int index, struct resource *res)
{
	struct irq_fwspec fwspec;
	struct irq_domain *domain;
	unsigned long flags;
	int rc;

	rc = acpi_irq_parse_one(handle, index, &fwspec, &flags);
	if (rc)
		return rc;

	domain = irq_find_matching_fwnode(fwspec.fwnode, DOMAIN_BUS_ANY);
	if (!domain)
		return -EPROBE_DEFER;

	rc = irq_create_fwspec_mapping(&fwspec);
	if (rc <= 0)
		return -EINVAL;

	res->start = rc;
	res->end = rc;
	res->flags = flags;

	return 0;
}
EXPORT_SYMBOL_GPL(acpi_irq_get);

 
void __init acpi_set_irq_model(enum acpi_irq_model_id model,
			       struct fwnode_handle *(*fn)(u32))
{
	acpi_irq_model = model;
	acpi_get_gsi_domain_id = fn;
}

 
void __init acpi_set_gsi_to_irq_fallback(u32 (*fn)(u32))
{
	acpi_gsi_to_irq_fallback = fn;
}

 
struct irq_domain *acpi_irq_create_hierarchy(unsigned int flags,
					     unsigned int size,
					     struct fwnode_handle *fwnode,
					     const struct irq_domain_ops *ops,
					     void *host_data)
{
	struct irq_domain *d;

	 
	if (acpi_irq_model != ACPI_IRQ_MODEL_GIC)
		return NULL;

	d = irq_find_matching_fwnode(acpi_get_gsi_domain_id(0),
				     DOMAIN_BUS_ANY);

	if (!d)
		return NULL;

	return irq_domain_create_hierarchy(d, flags, size, fwnode, ops,
					   host_data);
}
EXPORT_SYMBOL_GPL(acpi_irq_create_hierarchy);
