 

#ifndef __NOUVEAU_LED_H__
#define __NOUVEAU_LED_H__

#include "nouveau_drv.h"

#include <linux/leds.h>

struct nouveau_led {
	struct drm_device *dev;

	struct led_classdev led;
};

static inline struct nouveau_led *
nouveau_led(struct drm_device *dev)
{
	return nouveau_drm(dev)->led;
}

 
#if IS_REACHABLE(CONFIG_LEDS_CLASS)
int  nouveau_led_init(struct drm_device *dev);
void nouveau_led_suspend(struct drm_device *dev);
void nouveau_led_resume(struct drm_device *dev);
void nouveau_led_fini(struct drm_device *dev);
#else
static inline int  nouveau_led_init(struct drm_device *dev) { return 0; };
static inline void nouveau_led_suspend(struct drm_device *dev) { };
static inline void nouveau_led_resume(struct drm_device *dev) { };
static inline void nouveau_led_fini(struct drm_device *dev) { };
#endif

#endif
