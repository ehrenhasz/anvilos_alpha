#ifndef __MEDIA_INFO_H__
#define __MEDIA_INFO_H__
#ifndef MSM_MEDIA_ALIGN
#define MSM_MEDIA_ALIGN(__sz, __align) (((__align) & ((__align) - 1)) ?\
	((((__sz) + (__align) - 1) / (__align)) * (__align)) :\
	(((__sz) + (__align) - 1) & (~((__align) - 1))))
#endif
#ifndef MSM_MEDIA_ROUNDUP
#define MSM_MEDIA_ROUNDUP(__sz, __r) (((__sz) + ((__r) - 1)) / (__r))
#endif
#ifndef MSM_MEDIA_MAX
#define MSM_MEDIA_MAX(__a, __b) ((__a) > (__b)?(__a):(__b))
#endif
enum color_fmts {
	COLOR_FMT_NV12,
	COLOR_FMT_NV21,
	COLOR_FMT_NV12_MVTB,
	COLOR_FMT_NV12_UBWC,
	COLOR_FMT_NV12_BPP10_UBWC,
	COLOR_FMT_RGBA8888,
	COLOR_FMT_RGBA8888_UBWC,
	COLOR_FMT_RGBA1010102_UBWC,
	COLOR_FMT_RGB565_UBWC,
	COLOR_FMT_P010_UBWC,
	COLOR_FMT_P010,
};
#define COLOR_FMT_RGBA1010102_UBWC	COLOR_FMT_RGBA1010102_UBWC
#define COLOR_FMT_RGB565_UBWC		COLOR_FMT_RGB565_UBWC
#define COLOR_FMT_P010_UBWC		COLOR_FMT_P010_UBWC
#define COLOR_FMT_P010		COLOR_FMT_P010
static unsigned int VENUS_Y_STRIDE(int color_fmt, int width)
{
	unsigned int stride = 0;
	if (!width)
		return 0;
	switch (color_fmt) {
	case COLOR_FMT_NV21:
	case COLOR_FMT_NV12:
	case COLOR_FMT_NV12_MVTB:
	case COLOR_FMT_NV12_UBWC:
		stride = MSM_MEDIA_ALIGN(width, 128);
		break;
	case COLOR_FMT_NV12_BPP10_UBWC:
		stride = MSM_MEDIA_ALIGN(width, 192);
		stride = MSM_MEDIA_ALIGN(stride * 4 / 3, 256);
		break;
	case COLOR_FMT_P010_UBWC:
		stride = MSM_MEDIA_ALIGN(width * 2, 256);
		break;
	case COLOR_FMT_P010:
		stride = MSM_MEDIA_ALIGN(width * 2, 128);
		break;
	}
	return stride;
}
static unsigned int VENUS_UV_STRIDE(int color_fmt, int width)
{
	unsigned int stride = 0;
	if (!width)
		return 0;
	switch (color_fmt) {
	case COLOR_FMT_NV21:
	case COLOR_FMT_NV12:
	case COLOR_FMT_NV12_MVTB:
	case COLOR_FMT_NV12_UBWC:
		stride = MSM_MEDIA_ALIGN(width, 128);
		break;
	case COLOR_FMT_NV12_BPP10_UBWC:
		stride = MSM_MEDIA_ALIGN(width, 192);
		stride = MSM_MEDIA_ALIGN(stride * 4 / 3, 256);
		break;
	case COLOR_FMT_P010_UBWC:
		stride = MSM_MEDIA_ALIGN(width * 2, 256);
		break;
	case COLOR_FMT_P010:
		stride = MSM_MEDIA_ALIGN(width * 2, 128);
		break;
	}
	return stride;
}
static unsigned int VENUS_Y_SCANLINES(int color_fmt, int height)
{
	unsigned int sclines = 0;
	if (!height)
		return 0;
	switch (color_fmt) {
	case COLOR_FMT_NV21:
	case COLOR_FMT_NV12:
	case COLOR_FMT_NV12_MVTB:
	case COLOR_FMT_NV12_UBWC:
	case COLOR_FMT_P010:
		sclines = MSM_MEDIA_ALIGN(height, 32);
		break;
	case COLOR_FMT_NV12_BPP10_UBWC:
	case COLOR_FMT_P010_UBWC:
		sclines = MSM_MEDIA_ALIGN(height, 16);
		break;
	}
	return sclines;
}
static unsigned int VENUS_UV_SCANLINES(int color_fmt, int height)
{
	unsigned int sclines = 0;
	if (!height)
		return 0;
	switch (color_fmt) {
	case COLOR_FMT_NV21:
	case COLOR_FMT_NV12:
	case COLOR_FMT_NV12_MVTB:
	case COLOR_FMT_NV12_BPP10_UBWC:
	case COLOR_FMT_P010_UBWC:
	case COLOR_FMT_P010:
		sclines = MSM_MEDIA_ALIGN((height + 1) >> 1, 16);
		break;
	case COLOR_FMT_NV12_UBWC:
		sclines = MSM_MEDIA_ALIGN((height + 1) >> 1, 32);
		break;
	}
	return sclines;
}
static unsigned int VENUS_Y_META_STRIDE(int color_fmt, int width)
{
	int y_tile_width = 0, y_meta_stride;
	if (!width)
		return 0;
	switch (color_fmt) {
	case COLOR_FMT_NV12_UBWC:
	case COLOR_FMT_P010_UBWC:
		y_tile_width = 32;
		break;
	case COLOR_FMT_NV12_BPP10_UBWC:
		y_tile_width = 48;
		break;
	default:
		return 0;
	}
	y_meta_stride = MSM_MEDIA_ROUNDUP(width, y_tile_width);
	return MSM_MEDIA_ALIGN(y_meta_stride, 64);
}
static unsigned int VENUS_Y_META_SCANLINES(int color_fmt, int height)
{
	int y_tile_height = 0, y_meta_scanlines;
	if (!height)
		return 0;
	switch (color_fmt) {
	case COLOR_FMT_NV12_UBWC:
		y_tile_height = 8;
		break;
	case COLOR_FMT_NV12_BPP10_UBWC:
	case COLOR_FMT_P010_UBWC:
		y_tile_height = 4;
		break;
	default:
		return 0;
	}
	y_meta_scanlines = MSM_MEDIA_ROUNDUP(height, y_tile_height);
	return MSM_MEDIA_ALIGN(y_meta_scanlines, 16);
}
static unsigned int VENUS_UV_META_STRIDE(int color_fmt, int width)
{
	int uv_tile_width = 0, uv_meta_stride;
	if (!width)
		return 0;
	switch (color_fmt) {
	case COLOR_FMT_NV12_UBWC:
	case COLOR_FMT_P010_UBWC:
		uv_tile_width = 16;
		break;
	case COLOR_FMT_NV12_BPP10_UBWC:
		uv_tile_width = 24;
		break;
	default:
		return 0;
	}
	uv_meta_stride = MSM_MEDIA_ROUNDUP((width+1)>>1, uv_tile_width);
	return MSM_MEDIA_ALIGN(uv_meta_stride, 64);
}
static unsigned int VENUS_UV_META_SCANLINES(int color_fmt, int height)
{
	int uv_tile_height = 0, uv_meta_scanlines;
	if (!height)
		return 0;
	switch (color_fmt) {
	case COLOR_FMT_NV12_UBWC:
		uv_tile_height = 8;
		break;
	case COLOR_FMT_NV12_BPP10_UBWC:
	case COLOR_FMT_P010_UBWC:
		uv_tile_height = 4;
		break;
	default:
		return 0;
	}
	uv_meta_scanlines = MSM_MEDIA_ROUNDUP((height+1)>>1, uv_tile_height);
	return MSM_MEDIA_ALIGN(uv_meta_scanlines, 16);
}
static unsigned int VENUS_RGB_STRIDE(int color_fmt, int width)
{
	unsigned int alignment = 0, bpp = 4;
	if (!width)
		return 0;
	switch (color_fmt) {
	case COLOR_FMT_RGBA8888:
		alignment = 128;
		break;
	case COLOR_FMT_RGB565_UBWC:
		alignment = 256;
		bpp = 2;
		break;
	case COLOR_FMT_RGBA8888_UBWC:
	case COLOR_FMT_RGBA1010102_UBWC:
		alignment = 256;
		break;
	default:
		return 0;
	}
	return MSM_MEDIA_ALIGN(width * bpp, alignment);
}
static unsigned int VENUS_RGB_SCANLINES(int color_fmt, int height)
{
	unsigned int alignment = 0;
	if (!height)
		return 0;
	switch (color_fmt) {
	case COLOR_FMT_RGBA8888:
		alignment = 32;
		break;
	case COLOR_FMT_RGBA8888_UBWC:
	case COLOR_FMT_RGBA1010102_UBWC:
	case COLOR_FMT_RGB565_UBWC:
		alignment = 16;
		break;
	default:
		return 0;
	}
	return MSM_MEDIA_ALIGN(height, alignment);
}
static unsigned int VENUS_RGB_META_STRIDE(int color_fmt, int width)
{
	int rgb_meta_stride;
	if (!width)
		return 0;
	switch (color_fmt) {
	case COLOR_FMT_RGBA8888_UBWC:
	case COLOR_FMT_RGBA1010102_UBWC:
	case COLOR_FMT_RGB565_UBWC:
		rgb_meta_stride = MSM_MEDIA_ROUNDUP(width, 16);
		return MSM_MEDIA_ALIGN(rgb_meta_stride, 64);
	}
	return 0;
}
static unsigned int VENUS_RGB_META_SCANLINES(int color_fmt, int height)
{
	int rgb_meta_scanlines;
	if (!height)
		return 0;
	switch (color_fmt) {
	case COLOR_FMT_RGBA8888_UBWC:
	case COLOR_FMT_RGBA1010102_UBWC:
	case COLOR_FMT_RGB565_UBWC:
		rgb_meta_scanlines = MSM_MEDIA_ROUNDUP(height, 4);
		return MSM_MEDIA_ALIGN(rgb_meta_scanlines, 16);
	}
	return 0;
}
#endif
