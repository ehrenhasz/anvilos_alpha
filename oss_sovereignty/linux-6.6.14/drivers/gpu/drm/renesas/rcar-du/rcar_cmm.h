 
 

#ifndef __RCAR_CMM_H__
#define __RCAR_CMM_H__

#define CM2_LUT_SIZE		256

struct drm_color_lut;
struct platform_device;

 
struct rcar_cmm_config {
	struct {
		struct drm_color_lut *table;
	} lut;
};

#if IS_ENABLED(CONFIG_DRM_RCAR_CMM)
int rcar_cmm_init(struct platform_device *pdev);

int rcar_cmm_enable(struct platform_device *pdev);
void rcar_cmm_disable(struct platform_device *pdev);

int rcar_cmm_setup(struct platform_device *pdev,
		   const struct rcar_cmm_config *config);
#else
static inline int rcar_cmm_init(struct platform_device *pdev)
{
	return -ENODEV;
}

static inline int rcar_cmm_enable(struct platform_device *pdev)
{
	return 0;
}

static inline void rcar_cmm_disable(struct platform_device *pdev)
{
}

static inline int rcar_cmm_setup(struct platform_device *pdev,
				 const struct rcar_cmm_config *config)
{
	return 0;
}
#endif  

#endif  
