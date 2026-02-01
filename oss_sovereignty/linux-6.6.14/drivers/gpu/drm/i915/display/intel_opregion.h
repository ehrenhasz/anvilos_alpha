 

#ifndef _INTEL_OPREGION_H_
#define _INTEL_OPREGION_H_

#include <linux/workqueue.h>
#include <linux/pci.h>

struct drm_i915_private;
struct intel_connector;
struct intel_encoder;

struct opregion_header;
struct opregion_acpi;
struct opregion_swsci;
struct opregion_asle;
struct opregion_asle_ext;

struct intel_opregion {
	struct opregion_header *header;
	struct opregion_acpi *acpi;
	struct opregion_swsci *swsci;
	u32 swsci_gbda_sub_functions;
	u32 swsci_sbcb_sub_functions;
	struct opregion_asle *asle;
	struct opregion_asle_ext *asle_ext;
	void *rvda;
	void *vbt_firmware;
	const void *vbt;
	u32 vbt_size;
	u32 *lid_state;
	struct work_struct asle_work;
	struct notifier_block acpi_notifier;
};

#define OPREGION_SIZE            (8 * 1024)

#ifdef CONFIG_ACPI

int intel_opregion_setup(struct drm_i915_private *dev_priv);
void intel_opregion_cleanup(struct drm_i915_private *i915);

void intel_opregion_register(struct drm_i915_private *dev_priv);
void intel_opregion_unregister(struct drm_i915_private *dev_priv);

void intel_opregion_resume(struct drm_i915_private *dev_priv);
void intel_opregion_suspend(struct drm_i915_private *dev_priv,
			    pci_power_t state);

void intel_opregion_asle_intr(struct drm_i915_private *dev_priv);
int intel_opregion_notify_encoder(struct intel_encoder *intel_encoder,
				  bool enable);
int intel_opregion_notify_adapter(struct drm_i915_private *dev_priv,
				  pci_power_t state);
int intel_opregion_get_panel_type(struct drm_i915_private *dev_priv);
const struct drm_edid *intel_opregion_get_edid(struct intel_connector *connector);

bool intel_opregion_headless_sku(struct drm_i915_private *i915);

#else  

static inline int intel_opregion_setup(struct drm_i915_private *dev_priv)
{
	return 0;
}

static inline void intel_opregion_cleanup(struct drm_i915_private *i915)
{
}

static inline void intel_opregion_register(struct drm_i915_private *dev_priv)
{
}

static inline void intel_opregion_unregister(struct drm_i915_private *dev_priv)
{
}

static inline void intel_opregion_resume(struct drm_i915_private *dev_priv)
{
}

static inline void intel_opregion_suspend(struct drm_i915_private *dev_priv,
					  pci_power_t state)
{
}

static inline void intel_opregion_asle_intr(struct drm_i915_private *dev_priv)
{
}

static inline int
intel_opregion_notify_encoder(struct intel_encoder *intel_encoder, bool enable)
{
	return 0;
}

static inline int
intel_opregion_notify_adapter(struct drm_i915_private *dev, pci_power_t state)
{
	return 0;
}

static inline int intel_opregion_get_panel_type(struct drm_i915_private *dev)
{
	return -ENODEV;
}

static inline const struct drm_edid *
intel_opregion_get_edid(struct intel_connector *connector)
{
	return NULL;
}

static inline bool intel_opregion_headless_sku(struct drm_i915_private *i915)
{
	return false;
}

#endif  

#endif
