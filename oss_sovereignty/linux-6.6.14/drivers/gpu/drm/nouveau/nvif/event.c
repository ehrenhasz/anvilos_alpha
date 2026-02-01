 
#include <nvif/event.h>
#include <nvif/printf.h>

#include <nvif/class.h>
#include <nvif/if000e.h>

int
nvif_event_block(struct nvif_event *event)
{
	if (nvif_event_constructed(event)) {
		int ret = nvif_mthd(&event->object, NVIF_EVENT_V0_BLOCK, NULL, 0);
		NVIF_ERRON(ret, &event->object, "[BLOCK]");
		return ret;
	}
	return 0;
}

int
nvif_event_allow(struct nvif_event *event)
{
	if (nvif_event_constructed(event)) {
		int ret = nvif_mthd(&event->object, NVIF_EVENT_V0_ALLOW, NULL, 0);
		NVIF_ERRON(ret, &event->object, "[ALLOW]");
		return ret;
	}
	return 0;
}

void
nvif_event_dtor(struct nvif_event *event)
{
	nvif_object_dtor(&event->object);
}

int
nvif_event_ctor_(struct nvif_object *parent, const char *name, u32 handle, nvif_event_func func,
		 bool wait, struct nvif_event_v0 *args, u32 argc, bool warn,
		 struct nvif_event *event)
{
	struct nvif_event_v0 _args;
	int ret;

	if (!args) {
		args = &_args;
		argc = sizeof(_args);
	}

	args->version = 0;
	args->wait = wait;

	ret = nvif_object_ctor(parent, name ?: "nvifEvent", handle,
			       NVIF_CLASS_EVENT, args, argc, &event->object);
	NVIF_ERRON(ret && warn, parent, "[NEW EVENT wait:%d size:%zd]",
		   args->wait, argc - sizeof(*args));
	if (ret)
		return ret;

	event->func = func;
	return 0;
}
