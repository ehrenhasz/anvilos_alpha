 
#include <nvif/head.h>
#include <nvif/disp.h>
#include <nvif/printf.h>

#include <nvif/class.h>
#include <nvif/if0013.h>

int
nvif_head_vblank_event_ctor(struct nvif_head *head, const char *name, nvif_event_func func,
			    bool wait, struct nvif_event *event)
{
	int ret = nvif_event_ctor(&head->object, name ?: "nvifHeadVBlank", nvif_head_id(head),
				  func, wait, NULL, 0, event);
	NVIF_ERRON(ret, &head->object, "[NEW EVENT:VBLANK]");
	return ret;
}

void
nvif_head_dtor(struct nvif_head *head)
{
	nvif_object_dtor(&head->object);
}

int
nvif_head_ctor(struct nvif_disp *disp, const char *name, int id, struct nvif_head *head)
{
	struct nvif_head_v0 args;
	int ret;

	args.version = 0;
	args.id = id;

	ret = nvif_object_ctor(&disp->object, name ? name : "nvifHead", id, NVIF_CLASS_HEAD,
			       &args, sizeof(args), &head->object);
	NVIF_ERRON(ret, &disp->object, "[NEW head id:%d]", args.id);
	return ret;
}
