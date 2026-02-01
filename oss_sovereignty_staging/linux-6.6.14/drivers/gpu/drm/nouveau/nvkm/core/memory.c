 
#include <core/memory.h>
#include <core/mm.h>
#include <subdev/fb.h>
#include <subdev/instmem.h>

void
nvkm_memory_tags_put(struct nvkm_memory *memory, struct nvkm_device *device,
		     struct nvkm_tags **ptags)
{
	struct nvkm_fb *fb = device->fb;
	struct nvkm_tags *tags = *ptags;
	if (tags) {
		mutex_lock(&fb->tags.mutex);
		if (refcount_dec_and_test(&tags->refcount)) {
			nvkm_mm_free(&fb->tags.mm, &tags->mn);
			kfree(memory->tags);
			memory->tags = NULL;
		}
		mutex_unlock(&fb->tags.mutex);
		*ptags = NULL;
	}
}

int
nvkm_memory_tags_get(struct nvkm_memory *memory, struct nvkm_device *device,
		     u32 nr, void (*clr)(struct nvkm_device *, u32, u32),
		     struct nvkm_tags **ptags)
{
	struct nvkm_fb *fb = device->fb;
	struct nvkm_tags *tags;

	mutex_lock(&fb->tags.mutex);
	if ((tags = memory->tags)) {
		 
		if (tags->mn && tags->mn->length != nr) {
			mutex_unlock(&fb->tags.mutex);
			return -EINVAL;
		}

		refcount_inc(&tags->refcount);
		mutex_unlock(&fb->tags.mutex);
		*ptags = tags;
		return 0;
	}

	if (!(tags = kmalloc(sizeof(*tags), GFP_KERNEL))) {
		mutex_unlock(&fb->tags.mutex);
		return -ENOMEM;
	}

	if (!nvkm_mm_head(&fb->tags.mm, 0, 1, nr, nr, 1, &tags->mn)) {
		if (clr)
			clr(device, tags->mn->offset, tags->mn->length);
	} else {
		 
		tags->mn = NULL;
	}

	refcount_set(&tags->refcount, 1);
	*ptags = memory->tags = tags;
	mutex_unlock(&fb->tags.mutex);
	return 0;
}

void
nvkm_memory_ctor(const struct nvkm_memory_func *func,
		 struct nvkm_memory *memory)
{
	memory->func = func;
	kref_init(&memory->kref);
}

static void
nvkm_memory_del(struct kref *kref)
{
	struct nvkm_memory *memory = container_of(kref, typeof(*memory), kref);
	if (!WARN_ON(!memory->func)) {
		if (memory->func->dtor)
			memory = memory->func->dtor(memory);
		kfree(memory);
	}
}

void
nvkm_memory_unref(struct nvkm_memory **pmemory)
{
	struct nvkm_memory *memory = *pmemory;
	if (memory) {
		kref_put(&memory->kref, nvkm_memory_del);
		*pmemory = NULL;
	}
}

struct nvkm_memory *
nvkm_memory_ref(struct nvkm_memory *memory)
{
	if (memory)
		kref_get(&memory->kref);
	return memory;
}

int
nvkm_memory_new(struct nvkm_device *device, enum nvkm_memory_target target,
		u64 size, u32 align, bool zero,
		struct nvkm_memory **pmemory)
{
	struct nvkm_instmem *imem = device->imem;
	struct nvkm_memory *memory;
	int ret;

	if (unlikely(target != NVKM_MEM_TARGET_INST || !imem))
		return -ENOSYS;

	ret = nvkm_instobj_new(imem, size, align, zero, &memory);
	if (ret)
		return ret;

	*pmemory = memory;
	return 0;
}
