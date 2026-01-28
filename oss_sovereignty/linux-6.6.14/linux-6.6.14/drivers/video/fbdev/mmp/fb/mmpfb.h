#ifndef _MMP_FB_H_
#define _MMP_FB_H_
#include <video/mmp_disp.h>
#include <linux/fb.h>
struct mmpfb_info {
	struct device	*dev;
	int	id;
	const char	*name;
	struct fb_info	*fb_info;
	struct fb_videomode	mode;
	int	pix_fmt;
	void	*fb_start;
	int	fb_size;
	dma_addr_t	fb_start_dma;
	struct mmp_overlay	*overlay;
	struct mmp_path	*path;
	struct mutex	access_ok;
	unsigned int		pseudo_palette[16];
	int output_fmt;
};
#define MMPFB_DEFAULT_SIZE (PAGE_ALIGN(1920 * 1080 * 4 * 2))
#endif  
