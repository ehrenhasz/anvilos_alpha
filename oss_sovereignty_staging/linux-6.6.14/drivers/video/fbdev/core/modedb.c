 

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/fb.h>
#include <linux/kernel.h>

#undef DEBUG

#define name_matches(v, s, l) \
    ((v).name && !strncmp((s), (v).name, (l)) && strlen((v).name) == (l))
#define res_matches(v, x, y) \
    ((v).xres == (x) && (v).yres == (y))

#ifdef DEBUG
#define DPRINTK(fmt, args...)	printk("modedb %s: " fmt, __func__ , ## args)
#else
#define DPRINTK(fmt, args...)
#endif

 

static const struct fb_videomode modedb[] = {

	 
	{ NULL, 70, 640, 400, 39721, 40, 24, 39, 9, 96, 2, 0,
		FB_VMODE_NONINTERLACED },

	 
	{ NULL, 60, 640, 480, 39721, 40, 24, 32, 11, 96, 2,	0,
		FB_VMODE_NONINTERLACED },

	 
	{ NULL, 56, 800, 600, 27777, 128, 24, 22, 1, 72, 2,	0,
		FB_VMODE_NONINTERLACED },

	 
	{ NULL, 87, 1024, 768, 22271, 56, 24, 33, 8, 160, 8, 0,
		FB_VMODE_INTERLACED },

	 
	{ NULL, 85, 640, 400, 31746, 96, 32, 41, 1, 64, 3,
		FB_SYNC_VERT_HIGH_ACT, FB_VMODE_NONINTERLACED },

	 
	{ NULL, 72, 640, 480, 31746, 144, 40, 30, 8, 40, 3, 0,
		FB_VMODE_NONINTERLACED },

	 
	{ NULL, 75, 640, 480, 31746, 120, 16, 16, 1, 64, 3,	0,
		FB_VMODE_NONINTERLACED },

	 
	{ NULL, 60, 800, 600, 25000, 88, 40, 23, 1, 128, 4,
		FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
		FB_VMODE_NONINTERLACED },

	 
	{ NULL, 85, 640, 480, 27777, 80, 56, 25, 1, 56, 3, 0,
		FB_VMODE_NONINTERLACED },

	 
	{ NULL, 89, 1152, 864, 15384, 96, 16, 110, 1, 216, 10, 0,
		FB_VMODE_INTERLACED },
	 
	{ NULL, 72, 800, 600, 20000, 64, 56, 23, 37, 120, 6,
		FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
		FB_VMODE_NONINTERLACED },

	 
	{ NULL, 60, 1024, 768, 15384, 168, 8, 29, 3, 144, 6, 0,
		FB_VMODE_NONINTERLACED },

	 
	{ NULL, 100, 640, 480, 21834, 96, 32, 36, 8, 96, 6,	0,
		FB_VMODE_NONINTERLACED },

	 
	{ NULL, 60, 1152, 864, 11123, 208, 64, 16, 4, 256, 8, 0,
		FB_VMODE_NONINTERLACED },

	 
	{ NULL, 85, 800, 600, 16460, 160, 64, 36, 16, 64, 5, 0,
		FB_VMODE_NONINTERLACED },

	 
	{ NULL, 70, 1024, 768, 13333, 144, 24, 29, 3, 136, 6, 0,
		FB_VMODE_NONINTERLACED },

	 
	{ NULL, 87, 1280, 1024, 12500, 56, 16, 128, 1, 216, 12,	0,
		FB_VMODE_INTERLACED },

	 
	{ NULL, 100, 800, 600, 14357, 160, 64, 30, 4, 64, 6, 0,
		FB_VMODE_NONINTERLACED },

	 
	{ NULL, 76, 1024, 768, 11764, 208, 8, 36, 16, 120, 3, 0,
		FB_VMODE_NONINTERLACED },

	 
	{ NULL, 70, 1152, 864, 10869, 106, 56, 20, 1, 160, 10, 0,
		FB_VMODE_NONINTERLACED },

	 
	{ NULL, 61, 1280, 1024, 9090, 200, 48, 26, 1, 184, 3, 0,
		FB_VMODE_NONINTERLACED },

	 
	{ NULL, 60, 1400, 1050, 9259, 136, 40, 13, 1, 112, 3, 0,
		FB_VMODE_NONINTERLACED },

	 
	{ NULL, 75, 1400, 1050, 7190, 120, 56, 23, 10, 112, 13,
		FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
		FB_VMODE_NONINTERLACED },

	 
	{ NULL, 60, 1400, 1050, 9259, 128, 40, 12, 0, 112, 3,
		FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
		FB_VMODE_NONINTERLACED },

	 
	{ NULL, 85, 1024, 768, 10111, 192, 32, 34, 14, 160, 6, 0,
		FB_VMODE_NONINTERLACED },

	 
	{ NULL, 78, 1152, 864, 9090, 228, 88, 32, 0, 84, 12, 0,
		FB_VMODE_NONINTERLACED },

	 
	{ NULL, 70, 1280, 1024, 7905, 224, 32, 28, 8, 160, 8, 0,
		FB_VMODE_NONINTERLACED },

	 
	{ NULL, 60, 1600, 1200, 6172, 304, 64, 46, 1, 192, 3,
		FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
		FB_VMODE_NONINTERLACED },

	 
	{ NULL, 84, 1152, 864, 7407, 184, 312, 32, 0, 128, 12, 0,
		FB_VMODE_NONINTERLACED },

	 
	{ NULL, 74, 1280, 1024, 7407, 256, 32, 34, 3, 144, 3, 0,
		FB_VMODE_NONINTERLACED },

	 
	{ NULL, 100, 1024, 768, 8658, 192, 32, 21, 3, 192, 10, 0,
		FB_VMODE_NONINTERLACED },

	 
	{ NULL, 76, 1280, 1024, 7407, 248, 32, 34, 3, 104, 3, 0,
		FB_VMODE_NONINTERLACED },

	 
	{ NULL, 70, 1600, 1200, 5291, 304, 64, 46, 1, 192, 3, 0,
		FB_VMODE_NONINTERLACED },

	 
	{ NULL, 100, 1152, 864, 7264, 224, 32, 17, 2, 128, 19, 0,
		FB_VMODE_NONINTERLACED },

	 
	{ NULL, 85, 1280, 1024, 6349, 224, 64, 44, 1, 160, 3,
		FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
		FB_VMODE_NONINTERLACED },

	 
	{ NULL, 75, 1600, 1200, 4938, 304, 64, 46, 1, 192, 3,
		FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
		FB_VMODE_NONINTERLACED },

	 
	{ NULL, 60, 1680, 1050, 6848, 280, 104, 30, 3, 176, 6,
		FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
		FB_VMODE_NONINTERLACED },

	 
	{ NULL, 85, 1600, 1200, 4545, 272, 16, 37, 4, 192, 3,
		FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
		FB_VMODE_NONINTERLACED },

	 
	{ NULL, 100, 1280, 1024, 5502, 256, 32, 26, 7, 128, 15, 0,
		FB_VMODE_NONINTERLACED },

	 
	{ NULL, 64, 1800, 1440, 4347, 304, 96, 46, 1, 192, 3,
		FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
		FB_VMODE_NONINTERLACED },

	 
	{ NULL, 70, 1800, 1440, 4000, 304, 96, 46, 1, 192, 3,
		FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
		FB_VMODE_NONINTERLACED },

	 
	{ NULL, 78, 512, 384, 49603, 48, 16, 16, 1, 64, 3, 0,
		FB_VMODE_NONINTERLACED },

	 
	{ NULL, 85, 512, 384, 45454, 48, 16, 16, 1, 64, 3, 0,
		FB_VMODE_NONINTERLACED },

	 
	{ NULL, 70, 320, 200, 79440, 16, 16, 20, 4, 48, 1, 0,
		FB_VMODE_DOUBLE },

	 
	{ NULL, 60, 320, 240, 79440, 16, 16, 16, 5, 48, 1, 0,
		FB_VMODE_DOUBLE },

	 
	{ NULL, 72, 320, 240, 63492, 16, 16, 16, 4, 48, 2, 0,
		FB_VMODE_DOUBLE },

	 
	{ NULL, 56, 400, 300, 55555, 64, 16, 10, 1, 32, 1, 0,
		FB_VMODE_DOUBLE },

	 
	{ NULL, 60, 400, 300, 50000, 48, 16, 11, 1, 64, 2, 0,
		FB_VMODE_DOUBLE },

	 
	{ NULL, 72, 400, 300, 40000, 32, 24, 11, 19, 64, 3,	0,
		FB_VMODE_DOUBLE },

	 
	{ NULL, 56, 480, 300, 46176, 80, 16, 10, 1, 40, 1, 0,
		FB_VMODE_DOUBLE },

	 
	{ NULL, 60, 480, 300, 41858, 56, 16, 11, 1, 80, 2, 0,
		FB_VMODE_DOUBLE },

	 
	{ NULL, 63, 480, 300, 40000, 56, 16, 11, 1, 80, 2, 0,
		FB_VMODE_DOUBLE },

	 
	{ NULL, 72, 480, 300, 33386, 40, 24, 11, 19, 80, 3, 0,
		FB_VMODE_DOUBLE },

	 
	{ NULL, 60, 1920, 1080, 6734, 148, 88, 36, 4, 44, 5, 0,
		FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
		FB_VMODE_NONINTERLACED },

	 
	{ NULL, 60, 1920, 1200, 5177, 128, 336, 1, 38, 208, 3,
		FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
		FB_VMODE_NONINTERLACED },

	 
	{ NULL, 60, 1152, 768, 14047, 158, 26, 29, 3, 136, 6,
		FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
		FB_VMODE_NONINTERLACED },

	 
	{ NULL, 60, 1366, 768, 13806, 120, 10, 14, 3, 32, 5, 0,
		FB_VMODE_NONINTERLACED },

	 
	{ NULL, 60, 1280, 800, 12048, 200, 64, 24, 1, 136, 3, 0,
		FB_VMODE_NONINTERLACED },

	 
	{ NULL, 50, 720, 576, 74074, 64, 16, 39, 5, 64, 5, 0,
		FB_VMODE_INTERLACED },

	 
	{ NULL, 50, 800, 520, 58823, 144, 64, 72, 28, 80, 5, 0,
		FB_VMODE_INTERLACED },

	 
	{ NULL, 60, 864, 480, 27777, 1, 1, 1, 1, 0, 0,
		0, FB_VMODE_NONINTERLACED },
};

#ifdef CONFIG_FB_MODE_HELPERS
const struct fb_videomode vesa_modes[] = {
	 
	{ NULL, 85, 640, 350, 31746,  96, 32, 60, 32, 64, 3,
	  FB_SYNC_HOR_HIGH_ACT, FB_VMODE_NONINTERLACED, FB_MODE_IS_VESA},
	 
	{ NULL, 85, 640, 400, 31746,  96, 32, 41, 01, 64, 3,
	  FB_SYNC_VERT_HIGH_ACT, FB_VMODE_NONINTERLACED, FB_MODE_IS_VESA },
	 
	{ NULL, 85, 721, 400, 28169, 108, 36, 42, 01, 72, 3,
	  FB_SYNC_VERT_HIGH_ACT, FB_VMODE_NONINTERLACED, FB_MODE_IS_VESA },
	 
	{ NULL, 60, 640, 480, 39682,  48, 16, 33, 10, 96, 2,
	  0, FB_VMODE_NONINTERLACED, FB_MODE_IS_VESA },
	 
	{ NULL, 72, 640, 480, 31746, 128, 24, 29, 9, 40, 2,
	  0, FB_VMODE_NONINTERLACED, FB_MODE_IS_VESA },
	 
	{ NULL, 75, 640, 480, 31746, 120, 16, 16, 01, 64, 3,
	  0, FB_VMODE_NONINTERLACED, FB_MODE_IS_VESA },
	 
	{ NULL, 85, 640, 480, 27777, 80, 56, 25, 01, 56, 3,
	  0, FB_VMODE_NONINTERLACED, FB_MODE_IS_VESA },
	 
	{ NULL, 56, 800, 600, 27777, 128, 24, 22, 01, 72, 2,
	  FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
	  FB_VMODE_NONINTERLACED, FB_MODE_IS_VESA },
	 
	{ NULL, 60, 800, 600, 25000, 88, 40, 23, 01, 128, 4,
	  FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
	  FB_VMODE_NONINTERLACED, FB_MODE_IS_VESA },
	 
	{ NULL, 72, 800, 600, 20000, 64, 56, 23, 37, 120, 6,
	  FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
	  FB_VMODE_NONINTERLACED, FB_MODE_IS_VESA },
	 
	{ NULL, 75, 800, 600, 20202, 160, 16, 21, 01, 80, 3,
	  FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
	  FB_VMODE_NONINTERLACED, FB_MODE_IS_VESA },
	 
	{ NULL, 85, 800, 600, 17761, 152, 32, 27, 01, 64, 3,
	  FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
	  FB_VMODE_NONINTERLACED, FB_MODE_IS_VESA },
         
	{ NULL, 43, 1024, 768, 22271, 56, 8, 41, 0, 176, 8,
	  FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
	  FB_VMODE_INTERLACED, FB_MODE_IS_VESA },
	 
	{ NULL, 60, 1024, 768, 15384, 160, 24, 29, 3, 136, 6,
	  0, FB_VMODE_NONINTERLACED, FB_MODE_IS_VESA },
	 
	{ NULL, 70, 1024, 768, 13333, 144, 24, 29, 3, 136, 6,
	  0, FB_VMODE_NONINTERLACED, FB_MODE_IS_VESA },
	 
	{ NULL, 75, 1024, 768, 12690, 176, 16, 28, 1, 96, 3,
	  FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
	  FB_VMODE_NONINTERLACED, FB_MODE_IS_VESA },
	 
	{ NULL, 85, 1024, 768, 10582, 208, 48, 36, 1, 96, 3,
	  FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
	  FB_VMODE_NONINTERLACED, FB_MODE_IS_VESA },
	 
	{ NULL, 75, 1152, 864, 9259, 256, 64, 32, 1, 128, 3,
	  FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
	  FB_VMODE_NONINTERLACED, FB_MODE_IS_VESA },
	 
	{ NULL, 60, 1280, 960, 9259, 312, 96, 36, 1, 112, 3,
	  FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
	  FB_VMODE_NONINTERLACED, FB_MODE_IS_VESA },
	 
	{ NULL, 85, 1280, 960, 6734, 224, 64, 47, 1, 160, 3,
	  FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
	  FB_VMODE_NONINTERLACED, FB_MODE_IS_VESA },
	 
	{ NULL, 60, 1280, 1024, 9259, 248, 48, 38, 1, 112, 3,
	  FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
	  FB_VMODE_NONINTERLACED, FB_MODE_IS_VESA },
	 
	{ NULL, 75, 1280, 1024, 7407, 248, 16, 38, 1, 144, 3,
	  FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
	  FB_VMODE_NONINTERLACED, FB_MODE_IS_VESA },
	 
	{ NULL, 85, 1280, 1024, 6349, 224, 64, 44, 1, 160, 3,
	  FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
	  FB_VMODE_NONINTERLACED, FB_MODE_IS_VESA },
	 
	{ NULL, 60, 1600, 1200, 6172, 304, 64, 46, 1, 192, 3,
	  FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
	  FB_VMODE_NONINTERLACED, FB_MODE_IS_VESA },
	 
	{ NULL, 65, 1600, 1200, 5698, 304,  64, 46, 1, 192, 3,
	  FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
	  FB_VMODE_NONINTERLACED, FB_MODE_IS_VESA },
	 
	{ NULL, 70, 1600, 1200, 5291, 304, 64, 46, 1, 192, 3,
	  FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
	  FB_VMODE_NONINTERLACED, FB_MODE_IS_VESA },
	 
	{ NULL, 75, 1600, 1200, 4938, 304, 64, 46, 1, 192, 3,
	  FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
	  FB_VMODE_NONINTERLACED, FB_MODE_IS_VESA },
	 
	{ NULL, 85, 1600, 1200, 4357, 304, 64, 46, 1, 192, 3,
	  FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
	  FB_VMODE_NONINTERLACED, FB_MODE_IS_VESA },
	 
	{ NULL, 60, 1792, 1344, 4882, 328, 128, 46, 1, 200, 3,
	  FB_SYNC_VERT_HIGH_ACT, FB_VMODE_NONINTERLACED, FB_MODE_IS_VESA },
	 
	{ NULL, 75, 1792, 1344, 3831, 352, 96, 69, 1, 216, 3,
	  FB_SYNC_VERT_HIGH_ACT, FB_VMODE_NONINTERLACED, FB_MODE_IS_VESA },
	 
	{ NULL, 60, 1856, 1392, 4580, 352, 96, 43, 1, 224, 3,
	  FB_SYNC_VERT_HIGH_ACT, FB_VMODE_NONINTERLACED, FB_MODE_IS_VESA },
	 
	{ NULL, 75, 1856, 1392, 3472, 352, 128, 104, 1, 224, 3,
	  FB_SYNC_VERT_HIGH_ACT, FB_VMODE_NONINTERLACED, FB_MODE_IS_VESA },
	 
	{ NULL, 60, 1920, 1440, 4273, 344, 128, 56, 1, 200, 3,
	  FB_SYNC_VERT_HIGH_ACT, FB_VMODE_NONINTERLACED, FB_MODE_IS_VESA },
	 
	{ NULL, 75, 1920, 1440, 3367, 352, 144, 56, 1, 224, 3,
	  FB_SYNC_VERT_HIGH_ACT, FB_VMODE_NONINTERLACED, FB_MODE_IS_VESA },
	 
	{ NULL, 60, 1920, 1200, 6493, 80, 48, 26, 3, 32, 6,
	  FB_SYNC_VERT_HIGH_ACT, FB_VMODE_NONINTERLACED, FB_MODE_IS_VESA },
	 
	{ NULL, 60, 1920, 1200, 5174, 336, 136, 36, 3, 200, 6,
	  FB_SYNC_VERT_HIGH_ACT, FB_VMODE_NONINTERLACED, FB_MODE_IS_VESA },
	 
	{ NULL, 75, 1920, 1200, 4077, 344, 136, 46, 3, 208, 6,
	  FB_SYNC_VERT_HIGH_ACT, FB_VMODE_NONINTERLACED, FB_MODE_IS_VESA },
	 
	{ NULL, 85, 1920, 1200, 3555, 352, 144, 53, 3, 208, 6,
	  FB_SYNC_VERT_HIGH_ACT, FB_VMODE_NONINTERLACED, FB_MODE_IS_VESA },
	 
	{ NULL, 60, 2560, 1600, 3724, 80, 48, 37, 3, 32, 6,
	  FB_SYNC_HOR_HIGH_ACT, FB_VMODE_NONINTERLACED, FB_MODE_IS_VESA },
	 
	{ NULL, 60, 2560, 1600, 2869, 472, 192, 49, 3, 280, 6,
	  FB_SYNC_VERT_HIGH_ACT, FB_VMODE_NONINTERLACED, FB_MODE_IS_VESA },
	 
	{ NULL, 75, 2560, 1600, 2256, 488, 208, 63, 3, 280, 6,
	  FB_SYNC_VERT_HIGH_ACT, FB_VMODE_NONINTERLACED, FB_MODE_IS_VESA },
	 
	{ NULL, 85, 2560, 1600, 1979, 488, 208, 73, 3, 280, 6,
	  FB_SYNC_VERT_HIGH_ACT, FB_VMODE_NONINTERLACED, FB_MODE_IS_VESA },
	 
	{ NULL, 120, 2560, 1600, 1809, 80, 48, 85, 3, 32, 6,
	  FB_SYNC_HOR_HIGH_ACT, FB_VMODE_NONINTERLACED, FB_MODE_IS_VESA },
};
EXPORT_SYMBOL(vesa_modes);

const struct dmt_videomode dmt_modes[DMT_SIZE] = {
	{ 0x01, 0x0000, 0x000000, &vesa_modes[0] },
	{ 0x02, 0x3119, 0x000000, &vesa_modes[1] },
	{ 0x03, 0x0000, 0x000000, &vesa_modes[2] },
	{ 0x04, 0x3140, 0x000000, &vesa_modes[3] },
	{ 0x05, 0x314c, 0x000000, &vesa_modes[4] },
	{ 0x06, 0x314f, 0x000000, &vesa_modes[5] },
	{ 0x07, 0x3159, 0x000000, &vesa_modes[6] },
	{ 0x08, 0x0000, 0x000000, &vesa_modes[7] },
	{ 0x09, 0x4540, 0x000000, &vesa_modes[8] },
	{ 0x0a, 0x454c, 0x000000, &vesa_modes[9] },
	{ 0x0b, 0x454f, 0x000000, &vesa_modes[10] },
	{ 0x0c, 0x4559, 0x000000, &vesa_modes[11] },
	{ 0x0d, 0x0000, 0x000000, NULL },
	{ 0x0e, 0x0000, 0x000000, NULL },
	{ 0x0f, 0x0000, 0x000000, &vesa_modes[12] },
	{ 0x10, 0x6140, 0x000000, &vesa_modes[13] },
	{ 0x11, 0x614a, 0x000000, &vesa_modes[14] },
	{ 0x12, 0x614f, 0x000000, &vesa_modes[15] },
	{ 0x13, 0x6159, 0x000000, &vesa_modes[16] },
	{ 0x14, 0x0000, 0x000000, NULL },
	{ 0x15, 0x714f, 0x000000, &vesa_modes[17] },
	{ 0x16, 0x0000, 0x7f1c21, NULL },
	{ 0x17, 0x0000, 0x7f1c28, NULL },
	{ 0x18, 0x0000, 0x7f1c44, NULL },
	{ 0x19, 0x0000, 0x7f1c62, NULL },
	{ 0x1a, 0x0000, 0x000000, NULL },
	{ 0x1b, 0x0000, 0x8f1821, NULL },
	{ 0x1c, 0x8100, 0x8f1828, NULL },
	{ 0x1d, 0x810f, 0x8f1844, NULL },
	{ 0x1e, 0x8119, 0x8f1862, NULL },
	{ 0x1f, 0x0000, 0x000000, NULL },
	{ 0x20, 0x8140, 0x000000, &vesa_modes[18] },
	{ 0x21, 0x8159, 0x000000, &vesa_modes[19] },
	{ 0x22, 0x0000, 0x000000, NULL },
	{ 0x23, 0x8180, 0x000000, &vesa_modes[20] },
	{ 0x24, 0x818f, 0x000000, &vesa_modes[21] },
	{ 0x25, 0x8199, 0x000000, &vesa_modes[22] },
	{ 0x26, 0x0000, 0x000000, NULL },
	{ 0x27, 0x0000, 0x000000, NULL },
	{ 0x28, 0x0000, 0x000000, NULL },
	{ 0x29, 0x0000, 0x0c2021, NULL },
	{ 0x2a, 0x9040, 0x0c2028, NULL },
	{ 0x2b, 0x904f, 0x0c2044, NULL },
	{ 0x2c, 0x9059, 0x0c2062, NULL },
	{ 0x2d, 0x0000, 0x000000, NULL },
	{ 0x2e, 0x9500, 0xc11821, NULL },
	{ 0x2f, 0x9500, 0xc11828, NULL },
	{ 0x30, 0x950f, 0xc11844, NULL },
	{ 0x31, 0x9519, 0xc11868, NULL },
	{ 0x32, 0x0000, 0x000000, NULL },
	{ 0x33, 0xa940, 0x000000, &vesa_modes[23] },
	{ 0x34, 0xa945, 0x000000, &vesa_modes[24] },
	{ 0x35, 0xa94a, 0x000000, &vesa_modes[25] },
	{ 0x36, 0xa94f, 0x000000, &vesa_modes[26] },
	{ 0x37, 0xa959, 0x000000, &vesa_modes[27] },
	{ 0x38, 0x0000, 0x000000, NULL },
	{ 0x39, 0x0000, 0x0c2821, NULL },
	{ 0x3a, 0xb300, 0x0c2828, NULL },
	{ 0x3b, 0xb30f, 0x0c2844, NULL },
	{ 0x3c, 0xb319, 0x0c2868, NULL },
	{ 0x3d, 0x0000, 0x000000, NULL },
	{ 0x3e, 0xc140, 0x000000, &vesa_modes[28] },
	{ 0x3f, 0xc14f, 0x000000, &vesa_modes[29] },
	{ 0x40, 0x0000, 0x000000, NULL},
	{ 0x41, 0xc940, 0x000000, &vesa_modes[30] },
	{ 0x42, 0xc94f, 0x000000, &vesa_modes[31] },
	{ 0x43, 0x0000, 0x000000, NULL },
	{ 0x44, 0x0000, 0x572821, &vesa_modes[34] },
	{ 0x45, 0xd100, 0x572828, &vesa_modes[35] },
	{ 0x46, 0xd10f, 0x572844, &vesa_modes[36] },
	{ 0x47, 0xd119, 0x572862, &vesa_modes[37] },
	{ 0x48, 0x0000, 0x000000, NULL },
	{ 0x49, 0xd140, 0x000000, &vesa_modes[32] },
	{ 0x4a, 0xd14f, 0x000000, &vesa_modes[33] },
	{ 0x4b, 0x0000, 0x000000, NULL },
	{ 0x4c, 0x0000, 0x1f3821, &vesa_modes[38] },
	{ 0x4d, 0x0000, 0x1f3828, &vesa_modes[39] },
	{ 0x4e, 0x0000, 0x1f3844, &vesa_modes[40] },
	{ 0x4f, 0x0000, 0x1f3862, &vesa_modes[41] },
	{ 0x50, 0x0000, 0x000000, &vesa_modes[42] },
};
EXPORT_SYMBOL(dmt_modes);
#endif  

 

static int fb_try_mode(struct fb_var_screeninfo *var, struct fb_info *info,
		       const struct fb_videomode *mode, unsigned int bpp)
{
	int err = 0;

	DPRINTK("Trying mode %s %dx%d-%d@%d\n",
		mode->name ? mode->name : "noname",
		mode->xres, mode->yres, bpp, mode->refresh);
	var->xres = mode->xres;
	var->yres = mode->yres;
	var->xres_virtual = mode->xres;
	var->yres_virtual = mode->yres;
	var->xoffset = 0;
	var->yoffset = 0;
	var->bits_per_pixel = bpp;
	var->activate |= FB_ACTIVATE_TEST;
	var->pixclock = mode->pixclock;
	var->left_margin = mode->left_margin;
	var->right_margin = mode->right_margin;
	var->upper_margin = mode->upper_margin;
	var->lower_margin = mode->lower_margin;
	var->hsync_len = mode->hsync_len;
	var->vsync_len = mode->vsync_len;
	var->sync = mode->sync;
	var->vmode = mode->vmode;
	if (info->fbops->fb_check_var)
		err = info->fbops->fb_check_var(var, info);
	var->activate &= ~FB_ACTIVATE_TEST;
	return err;
}

 

int fb_find_mode(struct fb_var_screeninfo *var,
		 struct fb_info *info, const char *mode_option,
		 const struct fb_videomode *db, unsigned int dbsize,
		 const struct fb_videomode *default_mode,
		 unsigned int default_bpp)
{
	char *mode_option_buf = NULL;
	int i;

	 
	if (!db) {
		db = modedb;
		dbsize = ARRAY_SIZE(modedb);
	}

	if (!default_mode)
		default_mode = &db[0];

	if (!default_bpp)
		default_bpp = 8;

	 
	if (!mode_option) {
		fb_get_options(NULL, &mode_option_buf);
		mode_option = mode_option_buf;
	}
	if (mode_option) {
		const char *name = mode_option;
		unsigned int namelen = strlen(name);
		int res_specified = 0, bpp_specified = 0, refresh_specified = 0;
		unsigned int xres = 0, yres = 0, bpp = default_bpp, refresh = 0;
		int yres_specified = 0, cvt = 0, rb = 0;
		int interlace_specified = 0, interlace = 0;
		int margins = 0;
		u32 best, diff, tdiff;

		for (i = namelen-1; i >= 0; i--) {
			switch (name[i]) {
			case '@':
				namelen = i;
				if (!refresh_specified && !bpp_specified &&
				    !yres_specified) {
					refresh = simple_strtol(&name[i+1], NULL,
								10);
					refresh_specified = 1;
					if (cvt || rb)
						cvt = 0;
				} else
					goto done;
				break;
			case '-':
				namelen = i;
				if (!bpp_specified && !yres_specified) {
					bpp = simple_strtol(&name[i+1], NULL,
							    10);
					bpp_specified = 1;
					if (cvt || rb)
						cvt = 0;
				} else
					goto done;
				break;
			case 'x':
				if (!yres_specified) {
					yres = simple_strtol(&name[i+1], NULL,
							     10);
					yres_specified = 1;
				} else
					goto done;
				break;
			case '0' ... '9':
				break;
			case 'M':
				if (!yres_specified)
					cvt = 1;
				break;
			case 'R':
				if (!cvt)
					rb = 1;
				break;
			case 'm':
				if (!cvt)
					margins = 1;
				break;
			case 'p':
				if (!cvt) {
					interlace = 0;
					interlace_specified = 1;
				}
				break;
			case 'i':
				if (!cvt) {
					interlace = 1;
					interlace_specified = 1;
				}
				break;
			default:
				goto done;
			}
		}
		if (i < 0 && yres_specified) {
			xres = simple_strtol(name, NULL, 10);
			res_specified = 1;
		}
done:
		kfree(mode_option_buf);
		if (cvt) {
			struct fb_videomode cvt_mode;
			int ret;

			DPRINTK("CVT mode %dx%d@%dHz%s%s%s\n", xres, yres,
				(refresh) ? refresh : 60,
				(rb) ? " reduced blanking" : "",
				(margins) ? " with margins" : "",
				(interlace) ? " interlaced" : "");

			memset(&cvt_mode, 0, sizeof(cvt_mode));
			cvt_mode.xres = xres;
			cvt_mode.yres = yres;
			cvt_mode.refresh = (refresh) ? refresh : 60;

			if (interlace)
				cvt_mode.vmode |= FB_VMODE_INTERLACED;
			else
				cvt_mode.vmode &= ~FB_VMODE_INTERLACED;

			ret = fb_find_mode_cvt(&cvt_mode, margins, rb);

			if (!ret && !fb_try_mode(var, info, &cvt_mode, bpp)) {
				DPRINTK("modedb CVT: CVT mode ok\n");
				return 1;
			}

			DPRINTK("CVT mode invalid, getting mode from database\n");
		}

		DPRINTK("Trying specified video mode%s %ix%i\n",
			refresh_specified ? "" : " (ignoring refresh rate)",
			xres, yres);

		if (!refresh_specified) {
			 
			if (db != modedb &&
			    info->monspecs.vfmin && info->monspecs.vfmax &&
			    info->monspecs.hfmin && info->monspecs.hfmax &&
			    info->monspecs.dclkmax) {
				refresh = 1000;
			} else {
				refresh = 60;
			}
		}

		diff = -1;
		best = -1;
		for (i = 0; i < dbsize; i++) {
			if ((name_matches(db[i], name, namelen) ||
			     (res_specified && res_matches(db[i], xres, yres))) &&
			    !fb_try_mode(var, info, &db[i], bpp)) {
				const int db_interlace = (db[i].vmode &
					FB_VMODE_INTERLACED ? 1 : 0);
				int score = abs(db[i].refresh - refresh);

				if (interlace_specified)
					score += abs(db_interlace - interlace);

				if (!interlace_specified ||
				    db_interlace == interlace)
					if (refresh_specified &&
					    db[i].refresh == refresh)
						return 1;

				if (score < diff) {
					diff = score;
					best = i;
				}
			}
		}
		if (best != -1) {
			fb_try_mode(var, info, &db[best], bpp);
			return (refresh_specified) ? 2 : 1;
		}

		diff = 2 * (xres + yres);
		best = -1;
		DPRINTK("Trying best-fit modes\n");
		for (i = 0; i < dbsize; i++) {
			DPRINTK("Trying %ix%i\n", db[i].xres, db[i].yres);
			if (!fb_try_mode(var, info, &db[i], bpp)) {
				tdiff = abs(db[i].xres - xres) +
					abs(db[i].yres - yres);

				 
				if (xres > db[i].xres || yres > db[i].yres)
					tdiff += xres + yres;

				if (diff > tdiff) {
					diff = tdiff;
					best = i;
				}
			}
		}
		if (best != -1) {
			fb_try_mode(var, info, &db[best], bpp);
			return 5;
		}
	}

	DPRINTK("Trying default video mode\n");
	if (!fb_try_mode(var, info, default_mode, default_bpp))
		return 3;

	DPRINTK("Trying all modes\n");
	for (i = 0; i < dbsize; i++)
		if (!fb_try_mode(var, info, &db[i], default_bpp))
			return 4;

	DPRINTK("No valid mode found\n");
	return 0;
}

 
void fb_var_to_videomode(struct fb_videomode *mode,
			 const struct fb_var_screeninfo *var)
{
	u32 pixclock, hfreq, htotal, vtotal;

	mode->name = NULL;
	mode->xres = var->xres;
	mode->yres = var->yres;
	mode->pixclock = var->pixclock;
	mode->hsync_len = var->hsync_len;
	mode->vsync_len = var->vsync_len;
	mode->left_margin = var->left_margin;
	mode->right_margin = var->right_margin;
	mode->upper_margin = var->upper_margin;
	mode->lower_margin = var->lower_margin;
	mode->sync = var->sync;
	mode->vmode = var->vmode & FB_VMODE_MASK;
	mode->flag = FB_MODE_IS_FROM_VAR;
	mode->refresh = 0;

	if (!var->pixclock)
		return;

	pixclock = PICOS2KHZ(var->pixclock) * 1000;

	htotal = var->xres + var->right_margin + var->hsync_len +
		var->left_margin;
	vtotal = var->yres + var->lower_margin + var->vsync_len +
		var->upper_margin;

	if (var->vmode & FB_VMODE_INTERLACED)
		vtotal /= 2;
	if (var->vmode & FB_VMODE_DOUBLE)
		vtotal *= 2;

	if (!htotal || !vtotal)
		return;

	hfreq = pixclock/htotal;
	mode->refresh = hfreq/vtotal;
}

 
void fb_videomode_to_var(struct fb_var_screeninfo *var,
			 const struct fb_videomode *mode)
{
	var->xres = mode->xres;
	var->yres = mode->yres;
	var->xres_virtual = mode->xres;
	var->yres_virtual = mode->yres;
	var->xoffset = 0;
	var->yoffset = 0;
	var->pixclock = mode->pixclock;
	var->left_margin = mode->left_margin;
	var->right_margin = mode->right_margin;
	var->upper_margin = mode->upper_margin;
	var->lower_margin = mode->lower_margin;
	var->hsync_len = mode->hsync_len;
	var->vsync_len = mode->vsync_len;
	var->sync = mode->sync;
	var->vmode = mode->vmode & FB_VMODE_MASK;
}

 
int fb_mode_is_equal(const struct fb_videomode *mode1,
		     const struct fb_videomode *mode2)
{
	return (mode1->xres         == mode2->xres &&
		mode1->yres         == mode2->yres &&
		mode1->pixclock     == mode2->pixclock &&
		mode1->hsync_len    == mode2->hsync_len &&
		mode1->vsync_len    == mode2->vsync_len &&
		mode1->left_margin  == mode2->left_margin &&
		mode1->right_margin == mode2->right_margin &&
		mode1->upper_margin == mode2->upper_margin &&
		mode1->lower_margin == mode2->lower_margin &&
		mode1->sync         == mode2->sync &&
		mode1->vmode        == mode2->vmode);
}

 
const struct fb_videomode *fb_find_best_mode(const struct fb_var_screeninfo *var,
					     struct list_head *head)
{
	struct fb_modelist *modelist;
	struct fb_videomode *mode, *best = NULL;
	u32 diff = -1;

	list_for_each_entry(modelist, head, list) {
		u32 d;
		mode = &modelist->mode;

		if (mode->xres >= var->xres && mode->yres >= var->yres) {
			d = (mode->xres - var->xres) +
				(mode->yres - var->yres);
			if (diff > d) {
				diff = d;
				best = mode;
			} else if (diff == d && best &&
				   mode->refresh > best->refresh)
				best = mode;
		}
	}
	return best;
}

 
const struct fb_videomode *fb_find_nearest_mode(const struct fb_videomode *mode,
					        struct list_head *head)
{
	struct fb_modelist *modelist;
	struct fb_videomode *cmode, *best = NULL;
	u32 diff = -1, diff_refresh = -1;

	list_for_each_entry(modelist, head, list) {
		u32 d;
		cmode = &modelist->mode;

		d = abs(cmode->xres - mode->xres) +
			abs(cmode->yres - mode->yres);
		if (diff > d) {
			diff = d;
			diff_refresh = abs(cmode->refresh - mode->refresh);
			best = cmode;
		} else if (diff == d) {
			d = abs(cmode->refresh - mode->refresh);
			if (diff_refresh > d) {
				diff_refresh = d;
				best = cmode;
			}
		}
	}

	return best;
}

 
const struct fb_videomode *fb_match_mode(const struct fb_var_screeninfo *var,
					 struct list_head *head)
{
	struct fb_modelist *modelist;
	struct fb_videomode *m, mode;

	fb_var_to_videomode(&mode, var);
	list_for_each_entry(modelist, head, list) {
		m = &modelist->mode;
		if (fb_mode_is_equal(m, &mode))
			return m;
	}
	return NULL;
}

 
int fb_add_videomode(const struct fb_videomode *mode, struct list_head *head)
{
	struct fb_modelist *modelist;
	struct fb_videomode *m;
	int found = 0;

	list_for_each_entry(modelist, head, list) {
		m = &modelist->mode;
		if (fb_mode_is_equal(m, mode)) {
			found = 1;
			break;
		}
	}
	if (!found) {
		modelist = kmalloc(sizeof(struct fb_modelist),
						  GFP_KERNEL);

		if (!modelist)
			return -ENOMEM;
		modelist->mode = *mode;
		list_add(&modelist->list, head);
	}
	return 0;
}

 
void fb_delete_videomode(const struct fb_videomode *mode,
			 struct list_head *head)
{
	struct list_head *pos, *n;
	struct fb_modelist *modelist;
	struct fb_videomode *m;

	list_for_each_safe(pos, n, head) {
		modelist = list_entry(pos, struct fb_modelist, list);
		m = &modelist->mode;
		if (fb_mode_is_equal(m, mode)) {
			list_del(pos);
			kfree(pos);
		}
	}
}

 
void fb_destroy_modelist(struct list_head *head)
{
	struct list_head *pos, *n;

	list_for_each_safe(pos, n, head) {
		list_del(pos);
		kfree(pos);
	}
}
EXPORT_SYMBOL_GPL(fb_destroy_modelist);

 
void fb_videomode_to_modelist(const struct fb_videomode *modedb, int num,
			      struct list_head *head)
{
	int i;

	INIT_LIST_HEAD(head);

	for (i = 0; i < num; i++) {
		if (fb_add_videomode(&modedb[i], head))
			return;
	}
}

const struct fb_videomode *fb_find_best_display(const struct fb_monspecs *specs,
					        struct list_head *head)
{
	struct fb_modelist *modelist;
	const struct fb_videomode *m, *m1 = NULL, *md = NULL, *best = NULL;
	int first = 0;

	if (!head->prev || !head->next || list_empty(head))
		goto finished;

	 
	list_for_each_entry(modelist, head, list) {
		m = &modelist->mode;

		if (!first) {
			m1 = m;
			first = 1;
		}

		if (m->flag & FB_MODE_IS_FIRST) {
 			md = m;
			break;
		}
	}

	 
	if (specs->misc & FB_MISC_1ST_DETAIL) {
		best = md;
		goto finished;
	}

	 
	if (specs->max_x && specs->max_y) {
		struct fb_var_screeninfo var;

		memset(&var, 0, sizeof(struct fb_var_screeninfo));
		var.xres = (specs->max_x * 7200)/254;
		var.yres = (specs->max_y * 7200)/254;
		m = fb_find_best_mode(&var, head);
		if (m) {
			best = m;
			goto finished;
		}
	}

	 
	if (md) {
		best = md;
		goto finished;
	}

	 
	best = m1;
finished:
	return best;
}
EXPORT_SYMBOL(fb_find_best_display);

EXPORT_SYMBOL(fb_videomode_to_var);
EXPORT_SYMBOL(fb_var_to_videomode);
EXPORT_SYMBOL(fb_mode_is_equal);
EXPORT_SYMBOL(fb_add_videomode);
EXPORT_SYMBOL(fb_match_mode);
EXPORT_SYMBOL(fb_find_best_mode);
EXPORT_SYMBOL(fb_find_nearest_mode);
EXPORT_SYMBOL(fb_videomode_to_modelist);
EXPORT_SYMBOL(fb_find_mode);
EXPORT_SYMBOL(fb_find_mode_cvt);
