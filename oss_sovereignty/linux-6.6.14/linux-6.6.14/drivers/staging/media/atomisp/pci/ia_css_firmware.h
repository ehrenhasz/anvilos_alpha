#ifndef __IA_CSS_FIRMWARE_H
#define __IA_CSS_FIRMWARE_H
#include <linux/device.h>
#include "ia_css_err.h"
#include "ia_css_env.h"
struct ia_css_fw {
	void	    *data;   
	unsigned int bytes;  
};
struct device;
int
ia_css_load_firmware(struct device *dev, const struct ia_css_env *env,
		     const struct ia_css_fw  *fw);
void
ia_css_unload_firmware(void);
#endif  
