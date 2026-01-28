#ifndef __TILCDC_EXTERNAL_H__
#define __TILCDC_EXTERNAL_H__
int tilcdc_add_component_encoder(struct drm_device *dev);
int tilcdc_get_external_components(struct device *dev,
				   struct component_match **match);
int tilcdc_attach_external_device(struct drm_device *ddev);
#endif  
