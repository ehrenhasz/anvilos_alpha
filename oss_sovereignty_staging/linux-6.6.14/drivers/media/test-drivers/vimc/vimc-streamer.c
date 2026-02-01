
 

#include <linux/init.h>
#include <linux/freezer.h>
#include <linux/kthread.h>

#include "vimc-streamer.h"

 
static struct media_entity *vimc_get_source_entity(struct media_entity *ent)
{
	struct media_pad *pad;
	int i;

	for (i = 0; i < ent->num_pads; i++) {
		if (ent->pads[i].flags & MEDIA_PAD_FL_SOURCE)
			continue;
		pad = media_pad_remote_pad_first(&ent->pads[i]);
		return pad ? pad->entity : NULL;
	}
	return NULL;
}

 
static void vimc_streamer_pipeline_terminate(struct vimc_stream *stream)
{
	struct vimc_ent_device *ved;
	struct v4l2_subdev *sd;

	while (stream->pipe_size) {
		stream->pipe_size--;
		ved = stream->ved_pipeline[stream->pipe_size];
		stream->ved_pipeline[stream->pipe_size] = NULL;

		if (!is_media_entity_v4l2_subdev(ved->ent))
			continue;

		sd = media_entity_to_v4l2_subdev(ved->ent);
		v4l2_subdev_call(sd, video, s_stream, 0);
	}
}

 
static int vimc_streamer_pipeline_init(struct vimc_stream *stream,
				       struct vimc_ent_device *ved)
{
	struct media_entity *entity;
	struct video_device *vdev;
	struct v4l2_subdev *sd;
	int ret = 0;

	stream->pipe_size = 0;
	while (stream->pipe_size < VIMC_STREAMER_PIPELINE_MAX_SIZE) {
		if (!ved) {
			vimc_streamer_pipeline_terminate(stream);
			return -EINVAL;
		}
		stream->ved_pipeline[stream->pipe_size++] = ved;

		if (is_media_entity_v4l2_subdev(ved->ent)) {
			sd = media_entity_to_v4l2_subdev(ved->ent);
			ret = v4l2_subdev_call(sd, video, s_stream, 1);
			if (ret && ret != -ENOIOCTLCMD) {
				dev_err(ved->dev, "subdev_call error %s\n",
					ved->ent->name);
				vimc_streamer_pipeline_terminate(stream);
				return ret;
			}
		}

		entity = vimc_get_source_entity(ved->ent);
		 
		if (!entity) {
			 
			if (!vimc_is_source(ved->ent)) {
				dev_err(ved->dev,
					"first entity in the pipe '%s' is not a source\n",
					ved->ent->name);
				vimc_streamer_pipeline_terminate(stream);
				return -EPIPE;
			}
			return 0;
		}

		 
		if (is_media_entity_v4l2_subdev(entity)) {
			sd = media_entity_to_v4l2_subdev(entity);
			ved = v4l2_get_subdevdata(sd);
		} else {
			vdev = container_of(entity,
					    struct video_device,
					    entity);
			ved = video_get_drvdata(vdev);
		}
	}

	vimc_streamer_pipeline_terminate(stream);
	return -EINVAL;
}

 
static int vimc_streamer_thread(void *data)
{
	struct vimc_stream *stream = data;
	u8 *frame = NULL;
	int i;

	set_freezable();

	for (;;) {
		try_to_freeze();
		if (kthread_should_stop())
			break;

		for (i = stream->pipe_size - 1; i >= 0; i--) {
			frame = stream->ved_pipeline[i]->process_frame(
					stream->ved_pipeline[i], frame);
			if (!frame || IS_ERR(frame))
				break;
		}
		 
		set_current_state(TASK_UNINTERRUPTIBLE);
		schedule_timeout(HZ / 60);
	}

	return 0;
}

 
int vimc_streamer_s_stream(struct vimc_stream *stream,
			   struct vimc_ent_device *ved,
			   int enable)
{
	int ret;

	if (!stream || !ved)
		return -EINVAL;

	if (enable) {
		if (stream->kthread)
			return 0;

		ret = vimc_streamer_pipeline_init(stream, ved);
		if (ret)
			return ret;

		stream->kthread = kthread_run(vimc_streamer_thread, stream,
					      "vimc-streamer thread");

		if (IS_ERR(stream->kthread)) {
			ret = PTR_ERR(stream->kthread);
			dev_err(ved->dev, "kthread_run failed with %d\n", ret);
			vimc_streamer_pipeline_terminate(stream);
			stream->kthread = NULL;
			return ret;
		}

	} else {
		if (!stream->kthread)
			return 0;

		ret = kthread_stop(stream->kthread);
		 
		if (ret)
			dev_dbg(ved->dev, "kthread_stop returned '%d'\n", ret);

		stream->kthread = NULL;

		vimc_streamer_pipeline_terminate(stream);
	}

	return 0;
}
