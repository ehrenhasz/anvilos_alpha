
 

#include <linux/io.h>
#include <linux/iosys-map.h>
#include <linux/module.h>
#include <linux/slab.h>

#include <drm/drm_device.h>
#include <drm/drm_format_helper.h>
#include <drm/drm_framebuffer.h>
#include <drm/drm_fourcc.h>
#include <drm/drm_print.h>
#include <drm/drm_rect.h>

static unsigned int clip_offset(const struct drm_rect *clip, unsigned int pitch, unsigned int cpp)
{
	return clip->y1 * pitch + clip->x1 * cpp;
}

 
unsigned int drm_fb_clip_offset(unsigned int pitch, const struct drm_format_info *format,
				const struct drm_rect *clip)
{
	return clip_offset(clip, pitch, format->cpp[0]);
}
EXPORT_SYMBOL(drm_fb_clip_offset);

 
static int __drm_fb_xfrm(void *dst, unsigned long dst_pitch, unsigned long dst_pixsize,
			 const void *vaddr, const struct drm_framebuffer *fb,
			 const struct drm_rect *clip, bool vaddr_cached_hint,
			 void (*xfrm_line)(void *dbuf, const void *sbuf, unsigned int npixels))
{
	unsigned long linepixels = drm_rect_width(clip);
	unsigned long lines = drm_rect_height(clip);
	size_t sbuf_len = linepixels * fb->format->cpp[0];
	void *stmp = NULL;
	unsigned long i;
	const void *sbuf;

	 
	if (!vaddr_cached_hint) {
		stmp = kmalloc(sbuf_len, GFP_KERNEL);
		if (!stmp)
			return -ENOMEM;
	}

	if (!dst_pitch)
		dst_pitch = drm_rect_width(clip) * dst_pixsize;
	vaddr += clip_offset(clip, fb->pitches[0], fb->format->cpp[0]);

	for (i = 0; i < lines; ++i) {
		if (stmp)
			sbuf = memcpy(stmp, vaddr, sbuf_len);
		else
			sbuf = vaddr;
		xfrm_line(dst, sbuf, linepixels);
		vaddr += fb->pitches[0];
		dst += dst_pitch;
	}

	kfree(stmp);

	return 0;
}

 
static int __drm_fb_xfrm_toio(void __iomem *dst, unsigned long dst_pitch, unsigned long dst_pixsize,
			      const void *vaddr, const struct drm_framebuffer *fb,
			      const struct drm_rect *clip, bool vaddr_cached_hint,
			      void (*xfrm_line)(void *dbuf, const void *sbuf, unsigned int npixels))
{
	unsigned long linepixels = drm_rect_width(clip);
	unsigned long lines = drm_rect_height(clip);
	size_t dbuf_len = linepixels * dst_pixsize;
	size_t stmp_off = round_up(dbuf_len, ARCH_KMALLOC_MINALIGN);  
	size_t sbuf_len = linepixels * fb->format->cpp[0];
	void *stmp = NULL;
	unsigned long i;
	const void *sbuf;
	void *dbuf;

	if (vaddr_cached_hint) {
		dbuf = kmalloc(dbuf_len, GFP_KERNEL);
	} else {
		dbuf = kmalloc(stmp_off + sbuf_len, GFP_KERNEL);
		stmp = dbuf + stmp_off;
	}
	if (!dbuf)
		return -ENOMEM;

	if (!dst_pitch)
		dst_pitch = linepixels * dst_pixsize;
	vaddr += clip_offset(clip, fb->pitches[0], fb->format->cpp[0]);

	for (i = 0; i < lines; ++i) {
		if (stmp)
			sbuf = memcpy(stmp, vaddr, sbuf_len);
		else
			sbuf = vaddr;
		xfrm_line(dbuf, sbuf, linepixels);
		memcpy_toio(dst, dbuf, dbuf_len);
		vaddr += fb->pitches[0];
		dst += dst_pitch;
	}

	kfree(dbuf);

	return 0;
}

 
static int drm_fb_xfrm(struct iosys_map *dst,
		       const unsigned int *dst_pitch, const u8 *dst_pixsize,
		       const struct iosys_map *src, const struct drm_framebuffer *fb,
		       const struct drm_rect *clip, bool vaddr_cached_hint,
		       void (*xfrm_line)(void *dbuf, const void *sbuf, unsigned int npixels))
{
	static const unsigned int default_dst_pitch[DRM_FORMAT_MAX_PLANES] = {
		0, 0, 0, 0
	};

	if (!dst_pitch)
		dst_pitch = default_dst_pitch;

	 
	if (dst[0].is_iomem)
		return __drm_fb_xfrm_toio(dst[0].vaddr_iomem, dst_pitch[0], dst_pixsize[0],
					  src[0].vaddr, fb, clip, vaddr_cached_hint, xfrm_line);
	else
		return __drm_fb_xfrm(dst[0].vaddr, dst_pitch[0], dst_pixsize[0],
				     src[0].vaddr, fb, clip, vaddr_cached_hint, xfrm_line);
}

 
void drm_fb_memcpy(struct iosys_map *dst, const unsigned int *dst_pitch,
		   const struct iosys_map *src, const struct drm_framebuffer *fb,
		   const struct drm_rect *clip)
{
	static const unsigned int default_dst_pitch[DRM_FORMAT_MAX_PLANES] = {
		0, 0, 0, 0
	};

	const struct drm_format_info *format = fb->format;
	unsigned int i, y, lines = drm_rect_height(clip);

	if (!dst_pitch)
		dst_pitch = default_dst_pitch;

	for (i = 0; i < format->num_planes; ++i) {
		unsigned int bpp_i = drm_format_info_bpp(format, i);
		unsigned int cpp_i = DIV_ROUND_UP(bpp_i, 8);
		size_t len_i = DIV_ROUND_UP(drm_rect_width(clip) * bpp_i, 8);
		unsigned int dst_pitch_i = dst_pitch[i];
		struct iosys_map dst_i = dst[i];
		struct iosys_map src_i = src[i];

		if (!dst_pitch_i)
			dst_pitch_i = len_i;

		iosys_map_incr(&src_i, clip_offset(clip, fb->pitches[i], cpp_i));
		for (y = 0; y < lines; y++) {
			 
			iosys_map_memcpy_to(&dst_i, 0, src_i.vaddr, len_i);
			iosys_map_incr(&src_i, fb->pitches[i]);
			iosys_map_incr(&dst_i, dst_pitch_i);
		}
	}
}
EXPORT_SYMBOL(drm_fb_memcpy);

static void drm_fb_swab16_line(void *dbuf, const void *sbuf, unsigned int pixels)
{
	u16 *dbuf16 = dbuf;
	const u16 *sbuf16 = sbuf;
	const u16 *send16 = sbuf16 + pixels;

	while (sbuf16 < send16)
		*dbuf16++ = swab16(*sbuf16++);
}

static void drm_fb_swab32_line(void *dbuf, const void *sbuf, unsigned int pixels)
{
	u32 *dbuf32 = dbuf;
	const u32 *sbuf32 = sbuf;
	const u32 *send32 = sbuf32 + pixels;

	while (sbuf32 < send32)
		*dbuf32++ = swab32(*sbuf32++);
}

 
void drm_fb_swab(struct iosys_map *dst, const unsigned int *dst_pitch,
		 const struct iosys_map *src, const struct drm_framebuffer *fb,
		 const struct drm_rect *clip, bool cached)
{
	const struct drm_format_info *format = fb->format;
	u8 cpp = DIV_ROUND_UP(drm_format_info_bpp(format, 0), 8);
	void (*swab_line)(void *dbuf, const void *sbuf, unsigned int npixels);

	switch (cpp) {
	case 4:
		swab_line = drm_fb_swab32_line;
		break;
	case 2:
		swab_line = drm_fb_swab16_line;
		break;
	default:
		drm_warn_once(fb->dev, "Format %p4cc has unsupported pixel size.\n",
			      &format->format);
		return;
	}

	drm_fb_xfrm(dst, dst_pitch, &cpp, src, fb, clip, cached, swab_line);
}
EXPORT_SYMBOL(drm_fb_swab);

static void drm_fb_xrgb8888_to_rgb332_line(void *dbuf, const void *sbuf, unsigned int pixels)
{
	u8 *dbuf8 = dbuf;
	const __le32 *sbuf32 = sbuf;
	unsigned int x;
	u32 pix;

	for (x = 0; x < pixels; x++) {
		pix = le32_to_cpu(sbuf32[x]);
		dbuf8[x] = ((pix & 0x00e00000) >> 16) |
			   ((pix & 0x0000e000) >> 11) |
			   ((pix & 0x000000c0) >> 6);
	}
}

 
void drm_fb_xrgb8888_to_rgb332(struct iosys_map *dst, const unsigned int *dst_pitch,
			       const struct iosys_map *src, const struct drm_framebuffer *fb,
			       const struct drm_rect *clip)
{
	static const u8 dst_pixsize[DRM_FORMAT_MAX_PLANES] = {
		1,
	};

	drm_fb_xfrm(dst, dst_pitch, dst_pixsize, src, fb, clip, false,
		    drm_fb_xrgb8888_to_rgb332_line);
}
EXPORT_SYMBOL(drm_fb_xrgb8888_to_rgb332);

static void drm_fb_xrgb8888_to_rgb565_line(void *dbuf, const void *sbuf, unsigned int pixels)
{
	__le16 *dbuf16 = dbuf;
	const __le32 *sbuf32 = sbuf;
	unsigned int x;
	u16 val16;
	u32 pix;

	for (x = 0; x < pixels; x++) {
		pix = le32_to_cpu(sbuf32[x]);
		val16 = ((pix & 0x00F80000) >> 8) |
			((pix & 0x0000FC00) >> 5) |
			((pix & 0x000000F8) >> 3);
		dbuf16[x] = cpu_to_le16(val16);
	}
}

 
static void drm_fb_xrgb8888_to_rgb565_swab_line(void *dbuf, const void *sbuf,
						unsigned int pixels)
{
	__le16 *dbuf16 = dbuf;
	const __le32 *sbuf32 = sbuf;
	unsigned int x;
	u16 val16;
	u32 pix;

	for (x = 0; x < pixels; x++) {
		pix = le32_to_cpu(sbuf32[x]);
		val16 = ((pix & 0x00F80000) >> 8) |
			((pix & 0x0000FC00) >> 5) |
			((pix & 0x000000F8) >> 3);
		dbuf16[x] = cpu_to_le16(swab16(val16));
	}
}

 
void drm_fb_xrgb8888_to_rgb565(struct iosys_map *dst, const unsigned int *dst_pitch,
			       const struct iosys_map *src, const struct drm_framebuffer *fb,
			       const struct drm_rect *clip, bool swab)
{
	static const u8 dst_pixsize[DRM_FORMAT_MAX_PLANES] = {
		2,
	};

	void (*xfrm_line)(void *dbuf, const void *sbuf, unsigned int npixels);

	if (swab)
		xfrm_line = drm_fb_xrgb8888_to_rgb565_swab_line;
	else
		xfrm_line = drm_fb_xrgb8888_to_rgb565_line;

	drm_fb_xfrm(dst, dst_pitch, dst_pixsize, src, fb, clip, false, xfrm_line);
}
EXPORT_SYMBOL(drm_fb_xrgb8888_to_rgb565);

static void drm_fb_xrgb8888_to_xrgb1555_line(void *dbuf, const void *sbuf, unsigned int pixels)
{
	__le16 *dbuf16 = dbuf;
	const __le32 *sbuf32 = sbuf;
	unsigned int x;
	u16 val16;
	u32 pix;

	for (x = 0; x < pixels; x++) {
		pix = le32_to_cpu(sbuf32[x]);
		val16 = ((pix & 0x00f80000) >> 9) |
			((pix & 0x0000f800) >> 6) |
			((pix & 0x000000f8) >> 3);
		dbuf16[x] = cpu_to_le16(val16);
	}
}

 
void drm_fb_xrgb8888_to_xrgb1555(struct iosys_map *dst, const unsigned int *dst_pitch,
				 const struct iosys_map *src, const struct drm_framebuffer *fb,
				 const struct drm_rect *clip)
{
	static const u8 dst_pixsize[DRM_FORMAT_MAX_PLANES] = {
		2,
	};

	drm_fb_xfrm(dst, dst_pitch, dst_pixsize, src, fb, clip, false,
		    drm_fb_xrgb8888_to_xrgb1555_line);
}
EXPORT_SYMBOL(drm_fb_xrgb8888_to_xrgb1555);

static void drm_fb_xrgb8888_to_argb1555_line(void *dbuf, const void *sbuf, unsigned int pixels)
{
	__le16 *dbuf16 = dbuf;
	const __le32 *sbuf32 = sbuf;
	unsigned int x;
	u16 val16;
	u32 pix;

	for (x = 0; x < pixels; x++) {
		pix = le32_to_cpu(sbuf32[x]);
		val16 = BIT(15) |  
			((pix & 0x00f80000) >> 9) |
			((pix & 0x0000f800) >> 6) |
			((pix & 0x000000f8) >> 3);
		dbuf16[x] = cpu_to_le16(val16);
	}
}

 
void drm_fb_xrgb8888_to_argb1555(struct iosys_map *dst, const unsigned int *dst_pitch,
				 const struct iosys_map *src, const struct drm_framebuffer *fb,
				 const struct drm_rect *clip)
{
	static const u8 dst_pixsize[DRM_FORMAT_MAX_PLANES] = {
		2,
	};

	drm_fb_xfrm(dst, dst_pitch, dst_pixsize, src, fb, clip, false,
		    drm_fb_xrgb8888_to_argb1555_line);
}
EXPORT_SYMBOL(drm_fb_xrgb8888_to_argb1555);

static void drm_fb_xrgb8888_to_rgba5551_line(void *dbuf, const void *sbuf, unsigned int pixels)
{
	__le16 *dbuf16 = dbuf;
	const __le32 *sbuf32 = sbuf;
	unsigned int x;
	u16 val16;
	u32 pix;

	for (x = 0; x < pixels; x++) {
		pix = le32_to_cpu(sbuf32[x]);
		val16 = ((pix & 0x00f80000) >> 8) |
			((pix & 0x0000f800) >> 5) |
			((pix & 0x000000f8) >> 2) |
			BIT(0);  
		dbuf16[x] = cpu_to_le16(val16);
	}
}

 
void drm_fb_xrgb8888_to_rgba5551(struct iosys_map *dst, const unsigned int *dst_pitch,
				 const struct iosys_map *src, const struct drm_framebuffer *fb,
				 const struct drm_rect *clip)
{
	static const u8 dst_pixsize[DRM_FORMAT_MAX_PLANES] = {
		2,
	};

	drm_fb_xfrm(dst, dst_pitch, dst_pixsize, src, fb, clip, false,
		    drm_fb_xrgb8888_to_rgba5551_line);
}
EXPORT_SYMBOL(drm_fb_xrgb8888_to_rgba5551);

static void drm_fb_xrgb8888_to_rgb888_line(void *dbuf, const void *sbuf, unsigned int pixels)
{
	u8 *dbuf8 = dbuf;
	const __le32 *sbuf32 = sbuf;
	unsigned int x;
	u32 pix;

	for (x = 0; x < pixels; x++) {
		pix = le32_to_cpu(sbuf32[x]);
		 
		*dbuf8++ = (pix & 0x000000FF) >>  0;
		*dbuf8++ = (pix & 0x0000FF00) >>  8;
		*dbuf8++ = (pix & 0x00FF0000) >> 16;
	}
}

 
void drm_fb_xrgb8888_to_rgb888(struct iosys_map *dst, const unsigned int *dst_pitch,
			       const struct iosys_map *src, const struct drm_framebuffer *fb,
			       const struct drm_rect *clip)
{
	static const u8 dst_pixsize[DRM_FORMAT_MAX_PLANES] = {
		3,
	};

	drm_fb_xfrm(dst, dst_pitch, dst_pixsize, src, fb, clip, false,
		    drm_fb_xrgb8888_to_rgb888_line);
}
EXPORT_SYMBOL(drm_fb_xrgb8888_to_rgb888);

static void drm_fb_xrgb8888_to_argb8888_line(void *dbuf, const void *sbuf, unsigned int pixels)
{
	__le32 *dbuf32 = dbuf;
	const __le32 *sbuf32 = sbuf;
	unsigned int x;
	u32 pix;

	for (x = 0; x < pixels; x++) {
		pix = le32_to_cpu(sbuf32[x]);
		pix |= GENMASK(31, 24);  
		dbuf32[x] = cpu_to_le32(pix);
	}
}

 
void drm_fb_xrgb8888_to_argb8888(struct iosys_map *dst, const unsigned int *dst_pitch,
				 const struct iosys_map *src, const struct drm_framebuffer *fb,
				 const struct drm_rect *clip)
{
	static const u8 dst_pixsize[DRM_FORMAT_MAX_PLANES] = {
		4,
	};

	drm_fb_xfrm(dst, dst_pitch, dst_pixsize, src, fb, clip, false,
		    drm_fb_xrgb8888_to_argb8888_line);
}
EXPORT_SYMBOL(drm_fb_xrgb8888_to_argb8888);

static void drm_fb_xrgb8888_to_abgr8888_line(void *dbuf, const void *sbuf, unsigned int pixels)
{
	__le32 *dbuf32 = dbuf;
	const __le32 *sbuf32 = sbuf;
	unsigned int x;
	u32 pix;

	for (x = 0; x < pixels; x++) {
		pix = le32_to_cpu(sbuf32[x]);
		pix = ((pix & 0x00ff0000) >> 16) <<  0 |
		      ((pix & 0x0000ff00) >>  8) <<  8 |
		      ((pix & 0x000000ff) >>  0) << 16 |
		      GENMASK(31, 24);  
		*dbuf32++ = cpu_to_le32(pix);
	}
}

static void drm_fb_xrgb8888_to_abgr8888(struct iosys_map *dst, const unsigned int *dst_pitch,
					const struct iosys_map *src,
					const struct drm_framebuffer *fb,
					const struct drm_rect *clip)
{
	static const u8 dst_pixsize[DRM_FORMAT_MAX_PLANES] = {
		4,
	};

	drm_fb_xfrm(dst, dst_pitch, dst_pixsize, src, fb, clip, false,
		    drm_fb_xrgb8888_to_abgr8888_line);
}

static void drm_fb_xrgb8888_to_xbgr8888_line(void *dbuf, const void *sbuf, unsigned int pixels)
{
	__le32 *dbuf32 = dbuf;
	const __le32 *sbuf32 = sbuf;
	unsigned int x;
	u32 pix;

	for (x = 0; x < pixels; x++) {
		pix = le32_to_cpu(sbuf32[x]);
		pix = ((pix & 0x00ff0000) >> 16) <<  0 |
		      ((pix & 0x0000ff00) >>  8) <<  8 |
		      ((pix & 0x000000ff) >>  0) << 16 |
		      ((pix & 0xff000000) >> 24) << 24;
		*dbuf32++ = cpu_to_le32(pix);
	}
}

static void drm_fb_xrgb8888_to_xbgr8888(struct iosys_map *dst, const unsigned int *dst_pitch,
					const struct iosys_map *src,
					const struct drm_framebuffer *fb,
					const struct drm_rect *clip)
{
	static const u8 dst_pixsize[DRM_FORMAT_MAX_PLANES] = {
		4,
	};

	drm_fb_xfrm(dst, dst_pitch, dst_pixsize, src, fb, clip, false,
		    drm_fb_xrgb8888_to_xbgr8888_line);
}

static void drm_fb_xrgb8888_to_xrgb2101010_line(void *dbuf, const void *sbuf, unsigned int pixels)
{
	__le32 *dbuf32 = dbuf;
	const __le32 *sbuf32 = sbuf;
	unsigned int x;
	u32 val32;
	u32 pix;

	for (x = 0; x < pixels; x++) {
		pix = le32_to_cpu(sbuf32[x]);
		val32 = ((pix & 0x000000FF) << 2) |
			((pix & 0x0000FF00) << 4) |
			((pix & 0x00FF0000) << 6);
		pix = val32 | ((val32 >> 8) & 0x00300C03);
		*dbuf32++ = cpu_to_le32(pix);
	}
}

 
void drm_fb_xrgb8888_to_xrgb2101010(struct iosys_map *dst, const unsigned int *dst_pitch,
				    const struct iosys_map *src, const struct drm_framebuffer *fb,
				    const struct drm_rect *clip)
{
	static const u8 dst_pixsize[DRM_FORMAT_MAX_PLANES] = {
		4,
	};

	drm_fb_xfrm(dst, dst_pitch, dst_pixsize, src, fb, clip, false,
		    drm_fb_xrgb8888_to_xrgb2101010_line);
}
EXPORT_SYMBOL(drm_fb_xrgb8888_to_xrgb2101010);

static void drm_fb_xrgb8888_to_argb2101010_line(void *dbuf, const void *sbuf, unsigned int pixels)
{
	__le32 *dbuf32 = dbuf;
	const __le32 *sbuf32 = sbuf;
	unsigned int x;
	u32 val32;
	u32 pix;

	for (x = 0; x < pixels; x++) {
		pix = le32_to_cpu(sbuf32[x]);
		val32 = ((pix & 0x000000ff) << 2) |
			((pix & 0x0000ff00) << 4) |
			((pix & 0x00ff0000) << 6);
		pix = GENMASK(31, 30) |  
		      val32 | ((val32 >> 8) & 0x00300c03);
		*dbuf32++ = cpu_to_le32(pix);
	}
}

 
void drm_fb_xrgb8888_to_argb2101010(struct iosys_map *dst, const unsigned int *dst_pitch,
				    const struct iosys_map *src, const struct drm_framebuffer *fb,
				    const struct drm_rect *clip)
{
	static const u8 dst_pixsize[DRM_FORMAT_MAX_PLANES] = {
		4,
	};

	drm_fb_xfrm(dst, dst_pitch, dst_pixsize, src, fb, clip, false,
		    drm_fb_xrgb8888_to_argb2101010_line);
}
EXPORT_SYMBOL(drm_fb_xrgb8888_to_argb2101010);

static void drm_fb_xrgb8888_to_gray8_line(void *dbuf, const void *sbuf, unsigned int pixels)
{
	u8 *dbuf8 = dbuf;
	const __le32 *sbuf32 = sbuf;
	unsigned int x;

	for (x = 0; x < pixels; x++) {
		u32 pix = le32_to_cpu(sbuf32[x]);
		u8 r = (pix & 0x00ff0000) >> 16;
		u8 g = (pix & 0x0000ff00) >> 8;
		u8 b =  pix & 0x000000ff;

		 
		*dbuf8++ = (3 * r + 6 * g + b) / 10;
	}
}

 
void drm_fb_xrgb8888_to_gray8(struct iosys_map *dst, const unsigned int *dst_pitch,
			      const struct iosys_map *src, const struct drm_framebuffer *fb,
			      const struct drm_rect *clip)
{
	static const u8 dst_pixsize[DRM_FORMAT_MAX_PLANES] = {
		1,
	};

	drm_fb_xfrm(dst, dst_pitch, dst_pixsize, src, fb, clip, false,
		    drm_fb_xrgb8888_to_gray8_line);
}
EXPORT_SYMBOL(drm_fb_xrgb8888_to_gray8);

 
int drm_fb_blit(struct iosys_map *dst, const unsigned int *dst_pitch, uint32_t dst_format,
		const struct iosys_map *src, const struct drm_framebuffer *fb,
		const struct drm_rect *clip)
{
	uint32_t fb_format = fb->format->format;

	if (fb_format == dst_format) {
		drm_fb_memcpy(dst, dst_pitch, src, fb, clip);
		return 0;
	} else if (fb_format == (dst_format | DRM_FORMAT_BIG_ENDIAN)) {
		drm_fb_swab(dst, dst_pitch, src, fb, clip, false);
		return 0;
	} else if (fb_format == (dst_format & ~DRM_FORMAT_BIG_ENDIAN)) {
		drm_fb_swab(dst, dst_pitch, src, fb, clip, false);
		return 0;
	} else if (fb_format == DRM_FORMAT_XRGB8888) {
		if (dst_format == DRM_FORMAT_RGB565) {
			drm_fb_xrgb8888_to_rgb565(dst, dst_pitch, src, fb, clip, false);
			return 0;
		} else if (dst_format == DRM_FORMAT_XRGB1555) {
			drm_fb_xrgb8888_to_xrgb1555(dst, dst_pitch, src, fb, clip);
			return 0;
		} else if (dst_format == DRM_FORMAT_ARGB1555) {
			drm_fb_xrgb8888_to_argb1555(dst, dst_pitch, src, fb, clip);
			return 0;
		} else if (dst_format == DRM_FORMAT_RGBA5551) {
			drm_fb_xrgb8888_to_rgba5551(dst, dst_pitch, src, fb, clip);
			return 0;
		} else if (dst_format == DRM_FORMAT_RGB888) {
			drm_fb_xrgb8888_to_rgb888(dst, dst_pitch, src, fb, clip);
			return 0;
		} else if (dst_format == DRM_FORMAT_ARGB8888) {
			drm_fb_xrgb8888_to_argb8888(dst, dst_pitch, src, fb, clip);
			return 0;
		} else if (dst_format == DRM_FORMAT_XBGR8888) {
			drm_fb_xrgb8888_to_xbgr8888(dst, dst_pitch, src, fb, clip);
			return 0;
		} else if (dst_format == DRM_FORMAT_ABGR8888) {
			drm_fb_xrgb8888_to_abgr8888(dst, dst_pitch, src, fb, clip);
			return 0;
		} else if (dst_format == DRM_FORMAT_XRGB2101010) {
			drm_fb_xrgb8888_to_xrgb2101010(dst, dst_pitch, src, fb, clip);
			return 0;
		} else if (dst_format == DRM_FORMAT_ARGB2101010) {
			drm_fb_xrgb8888_to_argb2101010(dst, dst_pitch, src, fb, clip);
			return 0;
		} else if (dst_format == DRM_FORMAT_BGRX8888) {
			drm_fb_swab(dst, dst_pitch, src, fb, clip, false);
			return 0;
		}
	}

	drm_warn_once(fb->dev, "No conversion helper from %p4cc to %p4cc found.\n",
		      &fb_format, &dst_format);

	return -EINVAL;
}
EXPORT_SYMBOL(drm_fb_blit);

static void drm_fb_gray8_to_mono_line(void *dbuf, const void *sbuf, unsigned int pixels)
{
	u8 *dbuf8 = dbuf;
	const u8 *sbuf8 = sbuf;

	while (pixels) {
		unsigned int i, bits = min(pixels, 8U);
		u8 byte = 0;

		for (i = 0; i < bits; i++, pixels--) {
			if (*sbuf8++ >= 128)
				byte |= BIT(i);
		}
		*dbuf8++ = byte;
	}
}

 
void drm_fb_xrgb8888_to_mono(struct iosys_map *dst, const unsigned int *dst_pitch,
			     const struct iosys_map *src, const struct drm_framebuffer *fb,
			     const struct drm_rect *clip)
{
	static const unsigned int default_dst_pitch[DRM_FORMAT_MAX_PLANES] = {
		0, 0, 0, 0
	};
	unsigned int linepixels = drm_rect_width(clip);
	unsigned int lines = drm_rect_height(clip);
	unsigned int cpp = fb->format->cpp[0];
	unsigned int len_src32 = linepixels * cpp;
	struct drm_device *dev = fb->dev;
	void *vaddr = src[0].vaddr;
	unsigned int dst_pitch_0;
	unsigned int y;
	u8 *mono = dst[0].vaddr, *gray8;
	u32 *src32;

	if (drm_WARN_ON(dev, fb->format->format != DRM_FORMAT_XRGB8888))
		return;

	if (!dst_pitch)
		dst_pitch = default_dst_pitch;
	dst_pitch_0 = dst_pitch[0];

	 
	if (!dst_pitch_0)
		dst_pitch_0 = DIV_ROUND_UP(linepixels, 8);

	 
	src32 = kmalloc(len_src32 + linepixels, GFP_KERNEL);
	if (!src32)
		return;

	gray8 = (u8 *)src32 + len_src32;

	vaddr += clip_offset(clip, fb->pitches[0], cpp);
	for (y = 0; y < lines; y++) {
		src32 = memcpy(src32, vaddr, len_src32);
		drm_fb_xrgb8888_to_gray8_line(gray8, src32, linepixels);
		drm_fb_gray8_to_mono_line(mono, gray8, linepixels);
		vaddr += fb->pitches[0];
		mono += dst_pitch_0;
	}

	kfree(src32);
}
EXPORT_SYMBOL(drm_fb_xrgb8888_to_mono);

static uint32_t drm_fb_nonalpha_fourcc(uint32_t fourcc)
{
	 
	switch (fourcc) {
	case DRM_FORMAT_ARGB1555:
		return DRM_FORMAT_XRGB1555;
	case DRM_FORMAT_ABGR1555:
		return DRM_FORMAT_XBGR1555;
	case DRM_FORMAT_RGBA5551:
		return DRM_FORMAT_RGBX5551;
	case DRM_FORMAT_BGRA5551:
		return DRM_FORMAT_BGRX5551;
	case DRM_FORMAT_ARGB8888:
		return DRM_FORMAT_XRGB8888;
	case DRM_FORMAT_ABGR8888:
		return DRM_FORMAT_XBGR8888;
	case DRM_FORMAT_RGBA8888:
		return DRM_FORMAT_RGBX8888;
	case DRM_FORMAT_BGRA8888:
		return DRM_FORMAT_BGRX8888;
	case DRM_FORMAT_ARGB2101010:
		return DRM_FORMAT_XRGB2101010;
	case DRM_FORMAT_ABGR2101010:
		return DRM_FORMAT_XBGR2101010;
	case DRM_FORMAT_RGBA1010102:
		return DRM_FORMAT_RGBX1010102;
	case DRM_FORMAT_BGRA1010102:
		return DRM_FORMAT_BGRX1010102;
	}

	return fourcc;
}

static bool is_listed_fourcc(const uint32_t *fourccs, size_t nfourccs, uint32_t fourcc)
{
	const uint32_t *fourccs_end = fourccs + nfourccs;

	while (fourccs < fourccs_end) {
		if (*fourccs == fourcc)
			return true;
		++fourccs;
	}
	return false;
}

 
size_t drm_fb_build_fourcc_list(struct drm_device *dev,
				const u32 *native_fourccs, size_t native_nfourccs,
				u32 *fourccs_out, size_t nfourccs_out)
{
	 
	static const uint32_t extra_fourccs[] = {
		DRM_FORMAT_XRGB8888,
	};
	static const size_t extra_nfourccs = ARRAY_SIZE(extra_fourccs);

	u32 *fourccs = fourccs_out;
	const u32 *fourccs_end = fourccs_out + nfourccs_out;
	size_t i;

	 

	for (i = 0; i < native_nfourccs; ++i) {
		 
		u32 fourcc = drm_fb_nonalpha_fourcc(native_fourccs[i]);

		if (is_listed_fourcc(fourccs_out, fourccs - fourccs_out, fourcc)) {
			continue;  
		} else if (fourccs == fourccs_end) {
			drm_warn(dev, "Ignoring native format %p4cc\n", &fourcc);
			continue;  
		}

		drm_dbg_kms(dev, "adding native format %p4cc\n", &fourcc);

		*fourccs = fourcc;
		++fourccs;
	}

	 

	for (i = 0; (i < extra_nfourccs) && (fourccs < fourccs_end); ++i) {
		u32 fourcc = extra_fourccs[i];

		if (is_listed_fourcc(fourccs_out, fourccs - fourccs_out, fourcc)) {
			continue;  
		} else if (fourccs == fourccs_end) {
			drm_warn(dev, "Ignoring emulated format %p4cc\n", &fourcc);
			continue;  
		}

		drm_dbg_kms(dev, "adding emulated format %p4cc\n", &fourcc);

		*fourccs = fourcc;
		++fourccs;
	}

	return fourccs - fourccs_out;
}
EXPORT_SYMBOL(drm_fb_build_fourcc_list);
