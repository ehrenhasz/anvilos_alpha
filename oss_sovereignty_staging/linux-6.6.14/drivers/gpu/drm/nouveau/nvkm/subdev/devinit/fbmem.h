 
#include <subdev/fb/regsnv04.h>

#define NV04_PFB_DEBUG_0					0x00100080
#	define NV04_PFB_DEBUG_0_PAGE_MODE			0x00000001
#	define NV04_PFB_DEBUG_0_REFRESH_OFF			0x00000010
#	define NV04_PFB_DEBUG_0_REFRESH_COUNTX64		0x00003f00
#	define NV04_PFB_DEBUG_0_REFRESH_SLOW_CLK		0x00004000
#	define NV04_PFB_DEBUG_0_SAFE_MODE			0x00008000
#	define NV04_PFB_DEBUG_0_ALOM_ENABLE			0x00010000
#	define NV04_PFB_DEBUG_0_CASOE				0x00100000
#	define NV04_PFB_DEBUG_0_CKE_INVERT			0x10000000
#	define NV04_PFB_DEBUG_0_REFINC				0x20000000
#	define NV04_PFB_DEBUG_0_SAVE_POWER_OFF			0x40000000
#define NV04_PFB_CFG0						0x00100200
#	define NV04_PFB_CFG0_SCRAMBLE				0x20000000
#define NV04_PFB_CFG1						0x00100204
#define NV04_PFB_SCRAMBLE(i)                         (0x00100400 + 4 * (i))

#define NV10_PFB_REFCTRL					0x00100210
#	define NV10_PFB_REFCTRL_VALID_1				(1 << 31)

static inline struct io_mapping *
fbmem_init(struct nvkm_device *dev)
{
	return io_mapping_create_wc(dev->func->resource_addr(dev, 1),
				    dev->func->resource_size(dev, 1));
}

static inline void
fbmem_fini(struct io_mapping *fb)
{
	io_mapping_free(fb);
}

static inline u32
fbmem_peek(struct io_mapping *fb, u32 off)
{
	u8 __iomem *p = io_mapping_map_atomic_wc(fb, off & PAGE_MASK);
	u32 val = ioread32(p + (off & ~PAGE_MASK));
	io_mapping_unmap_atomic(p);
	return val;
}

static inline void
fbmem_poke(struct io_mapping *fb, u32 off, u32 val)
{
	u8 __iomem *p = io_mapping_map_atomic_wc(fb, off & PAGE_MASK);
	iowrite32(val, p + (off & ~PAGE_MASK));
	wmb();
	io_mapping_unmap_atomic(p);
}

static inline bool
fbmem_readback(struct io_mapping *fb, u32 off, u32 val)
{
	fbmem_poke(fb, off, val);
	return val == fbmem_peek(fb, off);
}
