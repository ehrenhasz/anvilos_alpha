 
#include "lut.h"
#include "disp.h"

#include <drm/drm_color_mgmt.h>
#include <drm/drm_mode.h>
#include <drm/drm_property.h>

#include <nvif/class.h>

u32
nv50_lut_load(struct nv50_lut *lut, int buffer, struct drm_property_blob *blob,
	      void (*load)(struct drm_color_lut *, int, void __iomem *))
{
	struct drm_color_lut *in = blob ? blob->data : NULL;
	void __iomem *mem = lut->mem[buffer].object.map.ptr;
	const u32 addr = lut->mem[buffer].addr;
	int i;

	if (!in) {
		in = kvmalloc_array(1024, sizeof(*in), GFP_KERNEL);
		if (!WARN_ON(!in)) {
			for (i = 0; i < 1024; i++) {
				in[i].red   =
				in[i].green =
				in[i].blue  = (i << 16) >> 10;
			}
			load(in, 1024, mem);
			kvfree(in);
		}
	} else {
		load(in, drm_color_lut_size(blob), mem);
	}

	return addr;
}

void
nv50_lut_fini(struct nv50_lut *lut)
{
	int i;
	for (i = 0; i < ARRAY_SIZE(lut->mem); i++)
		nvif_mem_dtor(&lut->mem[i]);
}

int
nv50_lut_init(struct nv50_disp *disp, struct nvif_mmu *mmu,
	      struct nv50_lut *lut)
{
	const u32 size = disp->disp->object.oclass < GF110_DISP ? 257 : 1025;
	int i;
	for (i = 0; i < ARRAY_SIZE(lut->mem); i++) {
		int ret = nvif_mem_ctor_map(mmu, "kmsLut", NVIF_MEM_VRAM,
					    size * 8, &lut->mem[i]);
		if (ret)
			return ret;
	}
	return 0;
}
