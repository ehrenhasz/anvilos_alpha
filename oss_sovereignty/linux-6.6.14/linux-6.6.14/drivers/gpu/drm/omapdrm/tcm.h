#ifndef TCM_H
#define TCM_H
struct tcm;
struct tcm_pt {
	u16 x;
	u16 y;
};
struct tcm_area {
	bool is2d;		 
	struct tcm    *tcm;	 
	struct tcm_pt  p0;
	struct tcm_pt  p1;
};
struct tcm {
	u16 width, height;	 
	int lut_id;		 
	unsigned int y_offset;	 
	spinlock_t lock;
	unsigned long *bitmap;
	size_t map_size;
	s32 (*reserve_2d)(struct tcm *tcm, u16 height, u16 width, u16 align,
			  s16 offset, u16 slot_bytes,
			  struct tcm_area *area);
	s32 (*reserve_1d)(struct tcm *tcm, u32 slots, struct tcm_area *area);
	s32 (*free)(struct tcm *tcm, struct tcm_area *area);
	void (*deinit)(struct tcm *tcm);
};
struct tcm *sita_init(u16 width, u16 height);
static inline void tcm_deinit(struct tcm *tcm)
{
	if (tcm)
		tcm->deinit(tcm);
}
static inline s32 tcm_reserve_2d(struct tcm *tcm, u16 width, u16 height,
				u16 align, s16 offset, u16 slot_bytes,
				struct tcm_area *area)
{
	s32 res = tcm  == NULL ? -ENODEV :
		(area == NULL || width == 0 || height == 0 ||
		 (align & (align - 1))) ? -EINVAL :
		(height > tcm->height || width > tcm->width) ? -ENOMEM : 0;
	if (!res) {
		area->is2d = true;
		res = tcm->reserve_2d(tcm, height, width, align, offset,
					slot_bytes, area);
		area->tcm = res ? NULL : tcm;
	}
	return res;
}
static inline s32 tcm_reserve_1d(struct tcm *tcm, u32 slots,
				 struct tcm_area *area)
{
	s32 res = tcm  == NULL ? -ENODEV :
		(area == NULL || slots == 0) ? -EINVAL :
		slots > (tcm->width * (u32) tcm->height) ? -ENOMEM : 0;
	if (!res) {
		area->is2d = false;
		res = tcm->reserve_1d(tcm, slots, area);
		area->tcm = res ? NULL : tcm;
	}
	return res;
}
static inline s32 tcm_free(struct tcm_area *area)
{
	s32 res = 0;  
	if (area && area->tcm) {
		res = area->tcm->free(area->tcm, area);
		if (res == 0)
			area->tcm = NULL;
	}
	return res;
}
static inline void tcm_slice(struct tcm_area *parent, struct tcm_area *slice)
{
	*slice = *parent;
	if (slice->tcm && !slice->is2d &&
		slice->p0.y != slice->p1.y &&
		(slice->p0.x || (slice->p1.x != slice->tcm->width - 1))) {
		slice->p1.x = slice->tcm->width - 1;
		slice->p1.y = (slice->p0.x) ? slice->p0.y : slice->p1.y - 1;
		parent->p0.x = 0;
		parent->p0.y = slice->p1.y + 1;
	} else {
		parent->tcm = NULL;
	}
}
static inline bool tcm_area_is_valid(struct tcm_area *area)
{
	return area && area->tcm &&
		area->p1.x < area->tcm->width &&
		area->p1.y < area->tcm->height &&
		area->p0.y <= area->p1.y &&
		((!area->is2d &&
		  area->p0.x < area->tcm->width &&
		  area->p0.x + area->p0.y * area->tcm->width <=
		  area->p1.x + area->p1.y * area->tcm->width) ||
		 (area->is2d &&
		  area->p0.x <= area->p1.x));
}
static inline bool __tcm_is_in(struct tcm_pt *p, struct tcm_area *a)
{
	u16 i;
	if (a->is2d) {
		return p->x >= a->p0.x && p->x <= a->p1.x &&
		       p->y >= a->p0.y && p->y <= a->p1.y;
	} else {
		i = p->x + p->y * a->tcm->width;
		return i >= a->p0.x + a->p0.y * a->tcm->width &&
		       i <= a->p1.x + a->p1.y * a->tcm->width;
	}
}
static inline u16 __tcm_area_width(struct tcm_area *area)
{
	return area->p1.x - area->p0.x + 1;
}
static inline u16 __tcm_area_height(struct tcm_area *area)
{
	return area->p1.y - area->p0.y + 1;
}
static inline u16 __tcm_sizeof(struct tcm_area *area)
{
	return area->is2d ?
		__tcm_area_width(area) * __tcm_area_height(area) :
		(area->p1.x - area->p0.x + 1) + (area->p1.y - area->p0.y) *
							area->tcm->width;
}
#define tcm_sizeof(area) __tcm_sizeof(&(area))
#define tcm_awidth(area) __tcm_area_width(&(area))
#define tcm_aheight(area) __tcm_area_height(&(area))
#define tcm_is_in(pt, area) __tcm_is_in(&(pt), &(area))
static inline s32 tcm_1d_limit(struct tcm_area *a, u32 num_pg)
{
	if (__tcm_sizeof(a) < num_pg)
		return -ENOMEM;
	if (!num_pg)
		return -EINVAL;
	a->p1.x = (a->p0.x + num_pg - 1) % a->tcm->width;
	a->p1.y = a->p0.y + ((a->p0.x + num_pg - 1) / a->tcm->width);
	return 0;
}
#define tcm_for_each_slice(var, area, safe) \
	for (safe = area, \
	     tcm_slice(&safe, &var); \
	     var.tcm; tcm_slice(&safe, &var))
#endif
