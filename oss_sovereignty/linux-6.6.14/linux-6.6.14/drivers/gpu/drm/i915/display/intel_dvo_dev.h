#ifndef __INTEL_DVO_DEV_H__
#define __INTEL_DVO_DEV_H__
#include "i915_reg_defs.h"
#include "intel_display_limits.h"
enum drm_connector_status;
struct drm_display_mode;
struct i2c_adapter;
struct intel_dvo_device {
	const char *name;
	int type;
	enum port port;
	u32 gpio;
	int slave_addr;
	const struct intel_dvo_dev_ops *dev_ops;
	void *dev_priv;
	struct i2c_adapter *i2c_bus;
};
struct intel_dvo_dev_ops {
	bool (*init)(struct intel_dvo_device *dvo,
		     struct i2c_adapter *i2cbus);
	void (*create_resources)(struct intel_dvo_device *dvo);
	void (*dpms)(struct intel_dvo_device *dvo, bool enable);
	enum drm_mode_status (*mode_valid)(struct intel_dvo_device *dvo,
					   struct drm_display_mode *mode);
	void (*prepare)(struct intel_dvo_device *dvo);
	void (*commit)(struct intel_dvo_device *dvo);
	void (*mode_set)(struct intel_dvo_device *dvo,
			 const struct drm_display_mode *mode,
			 const struct drm_display_mode *adjusted_mode);
	enum drm_connector_status (*detect)(struct intel_dvo_device *dvo);
	bool (*get_hw_state)(struct intel_dvo_device *dev);
	struct drm_display_mode *(*get_modes)(struct intel_dvo_device *dvo);
	void (*destroy) (struct intel_dvo_device *dvo);
	void (*dump_regs)(struct intel_dvo_device *dvo);
};
extern const struct intel_dvo_dev_ops sil164_ops;
extern const struct intel_dvo_dev_ops ch7xxx_ops;
extern const struct intel_dvo_dev_ops ivch_ops;
extern const struct intel_dvo_dev_ops tfp410_ops;
extern const struct intel_dvo_dev_ops ch7017_ops;
extern const struct intel_dvo_dev_ops ns2501_ops;
#endif  
