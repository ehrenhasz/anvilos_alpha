 
#define nv50_instmem(p) container_of((p), struct nv50_instmem, base)
#include "priv.h"

#include <core/memory.h>
#include <subdev/bar.h>
#include <subdev/fb.h>
#include <subdev/mmu.h>

struct nv50_instmem {
	struct nvkm_instmem base;
	u64 addr;

	 
	struct list_head lru;
};

 
#define nv50_instobj(p) container_of((p), struct nv50_instobj, base.memory)

struct nv50_instobj {
	struct nvkm_instobj base;
	struct nv50_instmem *imem;
	struct nvkm_memory *ram;
	struct nvkm_vma *bar;
	refcount_t maps;
	void *map;
	struct list_head lru;
};

static void
nv50_instobj_wr32_slow(struct nvkm_memory *memory, u64 offset, u32 data)
{
	struct nv50_instobj *iobj = nv50_instobj(memory);
	struct nv50_instmem *imem = iobj->imem;
	struct nvkm_device *device = imem->base.subdev.device;
	u64 base = (nvkm_memory_addr(iobj->ram) + offset) & 0xffffff00000ULL;
	u64 addr = (nvkm_memory_addr(iobj->ram) + offset) & 0x000000fffffULL;
	unsigned long flags;

	spin_lock_irqsave(&imem->base.lock, flags);
	if (unlikely(imem->addr != base)) {
		nvkm_wr32(device, 0x001700, base >> 16);
		imem->addr = base;
	}
	nvkm_wr32(device, 0x700000 + addr, data);
	spin_unlock_irqrestore(&imem->base.lock, flags);
}

static u32
nv50_instobj_rd32_slow(struct nvkm_memory *memory, u64 offset)
{
	struct nv50_instobj *iobj = nv50_instobj(memory);
	struct nv50_instmem *imem = iobj->imem;
	struct nvkm_device *device = imem->base.subdev.device;
	u64 base = (nvkm_memory_addr(iobj->ram) + offset) & 0xffffff00000ULL;
	u64 addr = (nvkm_memory_addr(iobj->ram) + offset) & 0x000000fffffULL;
	u32 data;
	unsigned long flags;

	spin_lock_irqsave(&imem->base.lock, flags);
	if (unlikely(imem->addr != base)) {
		nvkm_wr32(device, 0x001700, base >> 16);
		imem->addr = base;
	}
	data = nvkm_rd32(device, 0x700000 + addr);
	spin_unlock_irqrestore(&imem->base.lock, flags);
	return data;
}

static const struct nvkm_memory_ptrs
nv50_instobj_slow = {
	.rd32 = nv50_instobj_rd32_slow,
	.wr32 = nv50_instobj_wr32_slow,
};

static void
nv50_instobj_wr32(struct nvkm_memory *memory, u64 offset, u32 data)
{
	iowrite32_native(data, nv50_instobj(memory)->map + offset);
}

static u32
nv50_instobj_rd32(struct nvkm_memory *memory, u64 offset)
{
	return ioread32_native(nv50_instobj(memory)->map + offset);
}

static const struct nvkm_memory_ptrs
nv50_instobj_fast = {
	.rd32 = nv50_instobj_rd32,
	.wr32 = nv50_instobj_wr32,
};

static void
nv50_instobj_kmap(struct nv50_instobj *iobj, struct nvkm_vmm *vmm)
{
	struct nv50_instmem *imem = iobj->imem;
	struct nv50_instobj *eobj;
	struct nvkm_memory *memory = &iobj->base.memory;
	struct nvkm_subdev *subdev = &imem->base.subdev;
	struct nvkm_device *device = subdev->device;
	struct nvkm_vma *bar = NULL, *ebar;
	u64 size = nvkm_memory_size(memory);
	void *emap;
	int ret;

	 
	mutex_unlock(&imem->base.mutex);
	while ((ret = nvkm_vmm_get(vmm, 12, size, &bar))) {
		 
		mutex_lock(&imem->base.mutex);
		eobj = list_first_entry_or_null(&imem->lru, typeof(*eobj), lru);
		if (eobj) {
			nvkm_debug(subdev, "evict %016llx %016llx @ %016llx\n",
				   nvkm_memory_addr(&eobj->base.memory),
				   nvkm_memory_size(&eobj->base.memory),
				   eobj->bar->addr);
			list_del_init(&eobj->lru);
			ebar = eobj->bar;
			eobj->bar = NULL;
			emap = eobj->map;
			eobj->map = NULL;
		}
		mutex_unlock(&imem->base.mutex);
		if (!eobj)
			break;
		iounmap(emap);
		nvkm_vmm_put(vmm, &ebar);
	}

	if (ret == 0)
		ret = nvkm_memory_map(memory, 0, vmm, bar, NULL, 0);
	mutex_lock(&imem->base.mutex);
	if (ret || iobj->bar) {
		 
		mutex_unlock(&imem->base.mutex);
		nvkm_vmm_put(vmm, &bar);
		mutex_lock(&imem->base.mutex);
		return;
	}

	 
	iobj->bar = bar;
	iobj->map = ioremap_wc(device->func->resource_addr(device, 3) +
			       (u32)iobj->bar->addr, size);
	if (!iobj->map) {
		nvkm_warn(subdev, "PRAMIN ioremap failed\n");
		nvkm_vmm_put(vmm, &iobj->bar);
	}
}

static int
nv50_instobj_map(struct nvkm_memory *memory, u64 offset, struct nvkm_vmm *vmm,
		 struct nvkm_vma *vma, void *argv, u32 argc)
{
	memory = nv50_instobj(memory)->ram;
	return nvkm_memory_map(memory, offset, vmm, vma, argv, argc);
}

static void
nv50_instobj_release(struct nvkm_memory *memory)
{
	struct nv50_instobj *iobj = nv50_instobj(memory);
	struct nv50_instmem *imem = iobj->imem;
	struct nvkm_subdev *subdev = &imem->base.subdev;

	wmb();
	nvkm_bar_flush(subdev->device->bar);

	if (refcount_dec_and_mutex_lock(&iobj->maps, &imem->base.mutex)) {
		 
		if (likely(iobj->lru.next) && iobj->map) {
			BUG_ON(!list_empty(&iobj->lru));
			list_add_tail(&iobj->lru, &imem->lru);
		}

		 
		iobj->base.memory.ptrs = NULL;
		mutex_unlock(&imem->base.mutex);
	}
}

static void __iomem *
nv50_instobj_acquire(struct nvkm_memory *memory)
{
	struct nv50_instobj *iobj = nv50_instobj(memory);
	struct nvkm_instmem *imem = &iobj->imem->base;
	struct nvkm_vmm *vmm;
	void __iomem *map = NULL;

	 
	if (refcount_inc_not_zero(&iobj->maps))
		return iobj->map;

	 
	mutex_lock(&imem->mutex);
	if (refcount_inc_not_zero(&iobj->maps)) {
		mutex_unlock(&imem->mutex);
		return iobj->map;
	}

	 
	if ((vmm = nvkm_bar_bar2_vmm(imem->subdev.device))) {
		if (!iobj->map)
			nv50_instobj_kmap(iobj, vmm);
		map = iobj->map;
	}

	if (!refcount_inc_not_zero(&iobj->maps)) {
		 
		if (likely(iobj->lru.next))
			list_del_init(&iobj->lru);

		if (map)
			iobj->base.memory.ptrs = &nv50_instobj_fast;
		else
			iobj->base.memory.ptrs = &nv50_instobj_slow;
		refcount_set(&iobj->maps, 1);
	}

	mutex_unlock(&imem->mutex);
	return map;
}

static void
nv50_instobj_boot(struct nvkm_memory *memory, struct nvkm_vmm *vmm)
{
	struct nv50_instobj *iobj = nv50_instobj(memory);
	struct nvkm_instmem *imem = &iobj->imem->base;

	 
	mutex_lock(&imem->mutex);
	if (likely(iobj->lru.next)) {
		list_del_init(&iobj->lru);
		iobj->lru.next = NULL;
	}

	nv50_instobj_kmap(iobj, vmm);
	nvkm_instmem_boot(imem);
	mutex_unlock(&imem->mutex);
}

static u64
nv50_instobj_size(struct nvkm_memory *memory)
{
	return nvkm_memory_size(nv50_instobj(memory)->ram);
}

static u64
nv50_instobj_addr(struct nvkm_memory *memory)
{
	return nvkm_memory_addr(nv50_instobj(memory)->ram);
}

static u64
nv50_instobj_bar2(struct nvkm_memory *memory)
{
	struct nv50_instobj *iobj = nv50_instobj(memory);
	u64 addr = ~0ULL;
	if (nv50_instobj_acquire(&iobj->base.memory)) {
		iobj->lru.next = NULL;  
		addr = iobj->bar->addr;
	}
	nv50_instobj_release(&iobj->base.memory);
	return addr;
}

static enum nvkm_memory_target
nv50_instobj_target(struct nvkm_memory *memory)
{
	return nvkm_memory_target(nv50_instobj(memory)->ram);
}

static void *
nv50_instobj_dtor(struct nvkm_memory *memory)
{
	struct nv50_instobj *iobj = nv50_instobj(memory);
	struct nvkm_instmem *imem = &iobj->imem->base;
	struct nvkm_vma *bar;
	void *map;

	mutex_lock(&imem->mutex);
	if (likely(iobj->lru.next))
		list_del(&iobj->lru);
	map = iobj->map;
	bar = iobj->bar;
	mutex_unlock(&imem->mutex);

	if (map) {
		struct nvkm_vmm *vmm = nvkm_bar_bar2_vmm(imem->subdev.device);
		iounmap(map);
		if (likely(vmm))  
			nvkm_vmm_put(vmm, &bar);
	}

	nvkm_memory_unref(&iobj->ram);
	nvkm_instobj_dtor(imem, &iobj->base);
	return iobj;
}

static const struct nvkm_memory_func
nv50_instobj_func = {
	.dtor = nv50_instobj_dtor,
	.target = nv50_instobj_target,
	.bar2 = nv50_instobj_bar2,
	.addr = nv50_instobj_addr,
	.size = nv50_instobj_size,
	.boot = nv50_instobj_boot,
	.acquire = nv50_instobj_acquire,
	.release = nv50_instobj_release,
	.map = nv50_instobj_map,
};

static int
nv50_instobj_wrap(struct nvkm_instmem *base,
		  struct nvkm_memory *memory, struct nvkm_memory **pmemory)
{
	struct nv50_instmem *imem = nv50_instmem(base);
	struct nv50_instobj *iobj;

	if (!(iobj = kzalloc(sizeof(*iobj), GFP_KERNEL)))
		return -ENOMEM;
	*pmemory = &iobj->base.memory;

	nvkm_instobj_ctor(&nv50_instobj_func, &imem->base, &iobj->base);
	iobj->imem = imem;
	refcount_set(&iobj->maps, 0);
	INIT_LIST_HEAD(&iobj->lru);

	iobj->ram = nvkm_memory_ref(memory);
	return 0;
}

static int
nv50_instobj_new(struct nvkm_instmem *imem, u32 size, u32 align, bool zero,
		 struct nvkm_memory **pmemory)
{
	u8 page = max(order_base_2(align), 12);
	struct nvkm_memory *ram;
	int ret;

	ret = nvkm_ram_get(imem->subdev.device, 0, 1, page, size, true, true, &ram);
	if (ret)
		return ret;

	ret = nv50_instobj_wrap(imem, ram, pmemory);
	nvkm_memory_unref(&ram);
	return ret;
}

 

static void
nv50_instmem_fini(struct nvkm_instmem *base)
{
	nv50_instmem(base)->addr = ~0ULL;
}

static const struct nvkm_instmem_func
nv50_instmem = {
	.fini = nv50_instmem_fini,
	.memory_new = nv50_instobj_new,
	.memory_wrap = nv50_instobj_wrap,
	.zero = false,
};

int
nv50_instmem_new(struct nvkm_device *device, enum nvkm_subdev_type type, int inst,
		 struct nvkm_instmem **pimem)
{
	struct nv50_instmem *imem;

	if (!(imem = kzalloc(sizeof(*imem), GFP_KERNEL)))
		return -ENOMEM;
	nvkm_instmem_ctor(&nv50_instmem, device, type, inst, &imem->base);
	INIT_LIST_HEAD(&imem->lru);
	*pimem = &imem->base;
	return 0;
}
