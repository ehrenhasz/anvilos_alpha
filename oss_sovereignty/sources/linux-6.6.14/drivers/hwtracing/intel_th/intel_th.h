


#ifndef __INTEL_TH_H__
#define __INTEL_TH_H__

#include <linux/irqreturn.h>


enum {
	
	INTEL_TH_SOURCE = 0,
	
	INTEL_TH_OUTPUT,
	
	INTEL_TH_SWITCH,
};

struct intel_th_device;


struct intel_th_output {
	int		port;
	unsigned int	type;
	unsigned int	scratchpad;
	bool		multiblock;
	bool		active;
};


struct intel_th_drvdata {
	unsigned int	tscu_enable        : 1,
			multi_is_broken    : 1,
			has_mintctl        : 1,
			host_mode_only     : 1;
};

#define INTEL_TH_CAP(_th, _cap) ((_th)->drvdata ? (_th)->drvdata->_cap : 0)


struct intel_th_device {
	struct device		dev;
	const struct intel_th_drvdata *drvdata;
	struct resource		*resource;
	unsigned int		num_resources;
	unsigned int		type;
	int			id;

	
	bool			host_mode;

	
	struct intel_th_output	output;

	char		name[];
};

#define to_intel_th_device(_d)				\
	container_of((_d), struct intel_th_device, dev)


static inline struct resource *
intel_th_device_get_resource(struct intel_th_device *thdev, unsigned int type,
			     unsigned int num)
{
	int i;

	for (i = 0; i < thdev->num_resources; i++)
		if (resource_type(&thdev->resource[i]) == type && !num--)
			return &thdev->resource[i];

	return NULL;
}


enum {
	GTH_NONE = 0,
	GTH_MSU,	
	GTH_CTP,	
	GTH_LPP,	
	GTH_PTI,	
};


static inline bool
intel_th_output_assigned(struct intel_th_device *thdev)
{
	return thdev->type == INTEL_TH_OUTPUT &&
		(thdev->output.port >= 0 ||
		 thdev->output.type == GTH_NONE);
}


struct intel_th_driver {
	struct device_driver	driver;
	int			(*probe)(struct intel_th_device *thdev);
	void			(*remove)(struct intel_th_device *thdev);
	
	int			(*assign)(struct intel_th_device *thdev,
					  struct intel_th_device *othdev);
	void			(*unassign)(struct intel_th_device *thdev,
					    struct intel_th_device *othdev);
	void			(*prepare)(struct intel_th_device *thdev,
					   struct intel_th_output *output);
	void			(*enable)(struct intel_th_device *thdev,
					  struct intel_th_output *output);
	void			(*trig_switch)(struct intel_th_device *thdev,
					       struct intel_th_output *output);
	void			(*disable)(struct intel_th_device *thdev,
					   struct intel_th_output *output);
	
	irqreturn_t		(*irq)(struct intel_th_device *thdev);
	void			(*wait_empty)(struct intel_th_device *thdev);
	int			(*activate)(struct intel_th_device *thdev);
	void			(*deactivate)(struct intel_th_device *thdev);
	
	const struct file_operations *fops;
	
	const struct attribute_group *attr_group;

	
	int			(*set_output)(struct intel_th_device *thdev,
					      unsigned int master);
};

#define to_intel_th_driver(_d)					\
	container_of((_d), struct intel_th_driver, driver)

#define to_intel_th_driver_or_null(_d)		\
	((_d) ? to_intel_th_driver(_d) : NULL)


static inline struct intel_th_device *
to_intel_th_parent(const struct intel_th_device *thdev)
{
	struct device *parent = thdev->dev.parent;

	if (!parent)
		return NULL;

	return to_intel_th_device(parent);
}

static inline struct intel_th *to_intel_th(const struct intel_th_device *thdev)
{
	if (thdev->type == INTEL_TH_OUTPUT)
		thdev = to_intel_th_parent(thdev);

	if (WARN_ON_ONCE(!thdev || thdev->type == INTEL_TH_OUTPUT))
		return NULL;

	return dev_get_drvdata(thdev->dev.parent);
}

struct intel_th *
intel_th_alloc(struct device *dev, const struct intel_th_drvdata *drvdata,
	       struct resource *devres, unsigned int ndevres);
void intel_th_free(struct intel_th *th);

int intel_th_driver_register(struct intel_th_driver *thdrv);
void intel_th_driver_unregister(struct intel_th_driver *thdrv);

int intel_th_trace_enable(struct intel_th_device *thdev);
int intel_th_trace_switch(struct intel_th_device *thdev);
int intel_th_trace_disable(struct intel_th_device *thdev);
int intel_th_set_output(struct intel_th_device *thdev,
			unsigned int master);
int intel_th_output_enable(struct intel_th *th, unsigned int otype);

enum th_mmio_idx {
	TH_MMIO_CONFIG = 0,
	TH_MMIO_SW = 1,
	TH_MMIO_RTIT = 2,
	TH_MMIO_END,
};

#define TH_POSSIBLE_OUTPUTS	8

#define TH_SUBDEVICE_MAX	(TH_POSSIBLE_OUTPUTS + 2)
#define TH_CONFIGURABLE_MASTERS 256
#define TH_MSC_MAX		2


#define TH_NVEC_MAX		8


struct intel_th {
	struct device		*dev;

	struct intel_th_device	*thdev[TH_SUBDEVICE_MAX];
	struct intel_th_device	*hub;
	const struct intel_th_drvdata	*drvdata;

	struct resource		resource[TH_MMIO_END];
	int			(*activate)(struct intel_th *);
	void			(*deactivate)(struct intel_th *);
	unsigned int		num_thdevs;
	unsigned int		num_resources;
	int			irq;
	int			num_irqs;

	int			id;
	int			major;
#ifdef CONFIG_MODULES
	struct work_struct	request_module_work;
#endif 
#ifdef CONFIG_INTEL_TH_DEBUG
	struct dentry		*dbg;
#endif
};

static inline struct intel_th_device *
to_intel_th_hub(struct intel_th_device *thdev)
{
	if (thdev->type == INTEL_TH_SWITCH)
		return thdev;
	else if (thdev->type == INTEL_TH_OUTPUT)
		return to_intel_th_parent(thdev);

	return to_intel_th(thdev)->hub;
}


enum {
	
	REG_GTH_OFFSET		= 0x0000,
	REG_GTH_LENGTH		= 0x2000,

	
	REG_TSCU_OFFSET		= 0x2000,
	REG_TSCU_LENGTH		= 0x1000,

	REG_CTS_OFFSET		= 0x3000,
	REG_CTS_LENGTH		= 0x1000,

	
	REG_STH_OFFSET		= 0x4000,
	REG_STH_LENGTH		= 0x2000,

	
	REG_MSU_OFFSET		= 0xa0000,
	REG_MSU_LENGTH		= 0x02000,

	
	BUF_MSU_OFFSET		= 0x80000,
	BUF_MSU_LENGTH		= 0x20000,

	
	REG_PTI_OFFSET		= REG_GTH_OFFSET,
	REG_PTI_LENGTH		= REG_GTH_LENGTH,

	
	REG_DCIH_OFFSET		= REG_MSU_OFFSET,
	REG_DCIH_LENGTH		= REG_MSU_LENGTH,
};


enum {
	
	SCRPD_MEM_IS_PRIM_DEST		= BIT(0),
	
	SCRPD_DBC_IS_PRIM_DEST		= BIT(1),
	
	SCRPD_PTI_IS_PRIM_DEST		= BIT(2),
	
	SCRPD_BSSB_IS_PRIM_DEST		= BIT(3),
	
	SCRPD_PTI_IS_ALT_DEST		= BIT(4),
	
	SCRPD_BSSB_IS_ALT_DEST		= BIT(5),
	
	SCRPD_DEEPSX_EXIT		= BIT(6),
	
	SCRPD_S4_EXIT			= BIT(7),
	
	SCRPD_S5_EXIT			= BIT(8),
	
	SCRPD_MSC0_IS_ENABLED		= BIT(9),
	SCRPD_MSC1_IS_ENABLED		= BIT(10),
	
	SCRPD_SX_EXIT			= BIT(11),
	
	SCRPD_TRIGGER_IS_ENABLED	= BIT(12),
	SCRPD_ODLA_IS_ENABLED		= BIT(13),
	SCRPD_SOCHAP_IS_ENABLED		= BIT(14),
	SCRPD_STH_IS_ENABLED		= BIT(15),
	SCRPD_DCIH_IS_ENABLED		= BIT(16),
	SCRPD_VER_IS_ENABLED		= BIT(17),
	
	SCRPD_DEBUGGER_IN_USE		= BIT(24),
};

#endif
