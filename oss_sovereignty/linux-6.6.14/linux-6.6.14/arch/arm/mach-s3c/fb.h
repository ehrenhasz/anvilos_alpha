#ifndef __PLAT_S3C_FB_H
#define __PLAT_S3C_FB_H __FILE__
#include <linux/platform_data/video_s3c.h>
extern void s3c_fb_set_platdata(struct s3c_fb_platdata *pd);
extern void s3c64xx_fb_gpio_setup_24bpp(void);
#endif  
