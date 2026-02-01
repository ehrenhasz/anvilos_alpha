 

#if defined(CONFIG_ACPI)
extern void psb_intel_opregion_asle_intr(struct drm_device *dev);
extern void psb_intel_opregion_init(struct drm_device *dev);
extern void psb_intel_opregion_fini(struct drm_device *dev);
extern int psb_intel_opregion_setup(struct drm_device *dev);
extern void psb_intel_opregion_enable_asle(struct drm_device *dev);

#else

extern inline void psb_intel_opregion_asle_intr(struct drm_device *dev)
{
}

extern inline void psb_intel_opregion_init(struct drm_device *dev)
{
}

extern inline void psb_intel_opregion_fini(struct drm_device *dev)
{
}

extern inline int psb_intel_opregion_setup(struct drm_device *dev)
{
	return 0;
}

extern inline void psb_intel_opregion_enable_asle(struct drm_device *dev)
{
}
#endif
