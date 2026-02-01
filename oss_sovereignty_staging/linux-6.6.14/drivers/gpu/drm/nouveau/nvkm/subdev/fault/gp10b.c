 

#include "priv.h"

#include <core/memory.h>

#include <nvif/class.h>

u64
gp10b_fault_buffer_pin(struct nvkm_fault_buffer *buffer)
{
	return nvkm_memory_addr(buffer->mem);
}

static const struct nvkm_fault_func
gp10b_fault = {
	.intr = gp100_fault_intr,
	.buffer.nr = 1,
	.buffer.entry_size = 32,
	.buffer.info = gp100_fault_buffer_info,
	.buffer.pin = gp10b_fault_buffer_pin,
	.buffer.init = gp100_fault_buffer_init,
	.buffer.fini = gp100_fault_buffer_fini,
	.buffer.intr = gp100_fault_buffer_intr,
	.user = { { 0, 0, MAXWELL_FAULT_BUFFER_A }, 0 },
};

int
gp10b_fault_new(struct nvkm_device *device, enum nvkm_subdev_type type, int inst,
		struct nvkm_fault **pfault)
{
	return nvkm_fault_new_(&gp10b_fault, device, type, inst, pfault);
}
