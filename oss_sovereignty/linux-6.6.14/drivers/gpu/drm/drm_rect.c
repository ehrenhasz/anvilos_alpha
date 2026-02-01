 

#include <linux/errno.h>
#include <linux/export.h>
#include <linux/kernel.h>

#include <drm/drm_mode.h>
#include <drm/drm_print.h>
#include <drm/drm_rect.h>

 
bool drm_rect_intersect(struct drm_rect *r1, const struct drm_rect *r2)
{
	r1->x1 = max(r1->x1, r2->x1);
	r1->y1 = max(r1->y1, r2->y1);
	r1->x2 = min(r1->x2, r2->x2);
	r1->y2 = min(r1->y2, r2->y2);

	return drm_rect_visible(r1);
}
EXPORT_SYMBOL(drm_rect_intersect);

static u32 clip_scaled(int src, int dst, int *clip)
{
	u64 tmp;

	if (dst == 0)
		return 0;

	 
	*clip = min(*clip, dst);

	tmp = mul_u32_u32(src, dst - *clip);

	 
	if (src < (dst << 16))
		return DIV_ROUND_UP_ULL(tmp, dst);
	else
		return DIV_ROUND_DOWN_ULL(tmp, dst);
}

 
bool drm_rect_clip_scaled(struct drm_rect *src, struct drm_rect *dst,
			  const struct drm_rect *clip)
{
	int diff;

	diff = clip->x1 - dst->x1;
	if (diff > 0) {
		u32 new_src_w = clip_scaled(drm_rect_width(src),
					    drm_rect_width(dst), &diff);

		src->x1 = src->x2 - new_src_w;
		dst->x1 += diff;
	}
	diff = clip->y1 - dst->y1;
	if (diff > 0) {
		u32 new_src_h = clip_scaled(drm_rect_height(src),
					    drm_rect_height(dst), &diff);

		src->y1 = src->y2 - new_src_h;
		dst->y1 += diff;
	}
	diff = dst->x2 - clip->x2;
	if (diff > 0) {
		u32 new_src_w = clip_scaled(drm_rect_width(src),
					    drm_rect_width(dst), &diff);

		src->x2 = src->x1 + new_src_w;
		dst->x2 -= diff;
	}
	diff = dst->y2 - clip->y2;
	if (diff > 0) {
		u32 new_src_h = clip_scaled(drm_rect_height(src),
					    drm_rect_height(dst), &diff);

		src->y2 = src->y1 + new_src_h;
		dst->y2 -= diff;
	}

	return drm_rect_visible(dst);
}
EXPORT_SYMBOL(drm_rect_clip_scaled);

static int drm_calc_scale(int src, int dst)
{
	int scale = 0;

	if (WARN_ON(src < 0 || dst < 0))
		return -EINVAL;

	if (dst == 0)
		return 0;

	if (src > (dst << 16))
		return DIV_ROUND_UP(src, dst);
	else
		scale = src / dst;

	return scale;
}

 
int drm_rect_calc_hscale(const struct drm_rect *src,
			 const struct drm_rect *dst,
			 int min_hscale, int max_hscale)
{
	int src_w = drm_rect_width(src);
	int dst_w = drm_rect_width(dst);
	int hscale = drm_calc_scale(src_w, dst_w);

	if (hscale < 0 || dst_w == 0)
		return hscale;

	if (hscale < min_hscale || hscale > max_hscale)
		return -ERANGE;

	return hscale;
}
EXPORT_SYMBOL(drm_rect_calc_hscale);

 
int drm_rect_calc_vscale(const struct drm_rect *src,
			 const struct drm_rect *dst,
			 int min_vscale, int max_vscale)
{
	int src_h = drm_rect_height(src);
	int dst_h = drm_rect_height(dst);
	int vscale = drm_calc_scale(src_h, dst_h);

	if (vscale < 0 || dst_h == 0)
		return vscale;

	if (vscale < min_vscale || vscale > max_vscale)
		return -ERANGE;

	return vscale;
}
EXPORT_SYMBOL(drm_rect_calc_vscale);

 
void drm_rect_debug_print(const char *prefix, const struct drm_rect *r, bool fixed_point)
{
	if (fixed_point)
		DRM_DEBUG_KMS("%s" DRM_RECT_FP_FMT "\n", prefix, DRM_RECT_FP_ARG(r));
	else
		DRM_DEBUG_KMS("%s" DRM_RECT_FMT "\n", prefix, DRM_RECT_ARG(r));
}
EXPORT_SYMBOL(drm_rect_debug_print);

 
void drm_rect_rotate(struct drm_rect *r,
		     int width, int height,
		     unsigned int rotation)
{
	struct drm_rect tmp;

	if (rotation & (DRM_MODE_REFLECT_X | DRM_MODE_REFLECT_Y)) {
		tmp = *r;

		if (rotation & DRM_MODE_REFLECT_X) {
			r->x1 = width - tmp.x2;
			r->x2 = width - tmp.x1;
		}

		if (rotation & DRM_MODE_REFLECT_Y) {
			r->y1 = height - tmp.y2;
			r->y2 = height - tmp.y1;
		}
	}

	switch (rotation & DRM_MODE_ROTATE_MASK) {
	case DRM_MODE_ROTATE_0:
		break;
	case DRM_MODE_ROTATE_90:
		tmp = *r;
		r->x1 = tmp.y1;
		r->x2 = tmp.y2;
		r->y1 = width - tmp.x2;
		r->y2 = width - tmp.x1;
		break;
	case DRM_MODE_ROTATE_180:
		tmp = *r;
		r->x1 = width - tmp.x2;
		r->x2 = width - tmp.x1;
		r->y1 = height - tmp.y2;
		r->y2 = height - tmp.y1;
		break;
	case DRM_MODE_ROTATE_270:
		tmp = *r;
		r->x1 = height - tmp.y2;
		r->x2 = height - tmp.y1;
		r->y1 = tmp.x1;
		r->y2 = tmp.x2;
		break;
	default:
		break;
	}
}
EXPORT_SYMBOL(drm_rect_rotate);

 
void drm_rect_rotate_inv(struct drm_rect *r,
			 int width, int height,
			 unsigned int rotation)
{
	struct drm_rect tmp;

	switch (rotation & DRM_MODE_ROTATE_MASK) {
	case DRM_MODE_ROTATE_0:
		break;
	case DRM_MODE_ROTATE_90:
		tmp = *r;
		r->x1 = width - tmp.y2;
		r->x2 = width - tmp.y1;
		r->y1 = tmp.x1;
		r->y2 = tmp.x2;
		break;
	case DRM_MODE_ROTATE_180:
		tmp = *r;
		r->x1 = width - tmp.x2;
		r->x2 = width - tmp.x1;
		r->y1 = height - tmp.y2;
		r->y2 = height - tmp.y1;
		break;
	case DRM_MODE_ROTATE_270:
		tmp = *r;
		r->x1 = tmp.y1;
		r->x2 = tmp.y2;
		r->y1 = height - tmp.x2;
		r->y2 = height - tmp.x1;
		break;
	default:
		break;
	}

	if (rotation & (DRM_MODE_REFLECT_X | DRM_MODE_REFLECT_Y)) {
		tmp = *r;

		if (rotation & DRM_MODE_REFLECT_X) {
			r->x1 = width - tmp.x2;
			r->x2 = width - tmp.x1;
		}

		if (rotation & DRM_MODE_REFLECT_Y) {
			r->y1 = height - tmp.y2;
			r->y2 = height - tmp.y1;
		}
	}
}
EXPORT_SYMBOL(drm_rect_rotate_inv);
