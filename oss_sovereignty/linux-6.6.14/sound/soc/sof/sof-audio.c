









#include <linux/bitfield.h>
#include <trace/events/sof.h>
#include "sof-audio.h"
#include "sof-of-dev.h"
#include "ops.h"

static bool is_virtual_widget(struct snd_sof_dev *sdev, struct snd_soc_dapm_widget *widget,
			      const char *func)
{
	switch (widget->id) {
	case snd_soc_dapm_out_drv:
	case snd_soc_dapm_output:
	case snd_soc_dapm_input:
		dev_dbg(sdev->dev, "%s: %s is a virtual widget\n", func, widget->name);
		return true;
	default:
		return false;
	}
}

static void sof_reset_route_setup_status(struct snd_sof_dev *sdev, struct snd_sof_widget *widget)
{
	const struct sof_ipc_tplg_ops *tplg_ops = sof_ipc_get_ops(sdev, tplg);
	struct snd_sof_route *sroute;

	list_for_each_entry(sroute, &sdev->route_list, list)
		if (sroute->src_widget == widget || sroute->sink_widget == widget) {
			if (sroute->setup && tplg_ops && tplg_ops->route_free)
				tplg_ops->route_free(sdev, sroute);

			sroute->setup = false;
		}
}

static int sof_widget_free_unlocked(struct snd_sof_dev *sdev,
				    struct snd_sof_widget *swidget)
{
	const struct sof_ipc_tplg_ops *tplg_ops = sof_ipc_get_ops(sdev, tplg);
	struct snd_sof_pipeline *spipe = swidget->spipe;
	struct snd_sof_widget *pipe_widget;
	int err = 0;
	int ret;

	if (!swidget->private)
		return 0;

	trace_sof_widget_free(swidget);

	 
	if (--swidget->use_count)
		return 0;

	pipe_widget = swidget->spipe->pipe_widget;

	 
	sof_reset_route_setup_status(sdev, swidget);

	 
	if (WIDGET_IS_DAI(swidget->id)) {
		struct snd_sof_dai_config_data data;
		unsigned int flags = SOF_DAI_CONFIG_FLAGS_HW_FREE;

		data.dai_data = DMA_CHAN_INVALID;

		if (tplg_ops && tplg_ops->dai_config) {
			err = tplg_ops->dai_config(sdev, swidget, flags, &data);
			if (err < 0)
				dev_err(sdev->dev, "failed to free config for widget %s\n",
					swidget->widget->name);
		}
	}

	 
	if (tplg_ops && tplg_ops->widget_free) {
		ret = tplg_ops->widget_free(sdev, swidget);
		if (ret < 0 && !err)
			err = ret;
	}

	 
	if (swidget->id == snd_soc_dapm_scheduler) {
		int i;

		for_each_set_bit(i, &spipe->core_mask, sdev->num_cores) {
			ret = snd_sof_dsp_core_put(sdev, i);
			if (ret < 0) {
				dev_err(sdev->dev, "failed to disable target core: %d for pipeline %s\n",
					i, swidget->widget->name);
				if (!err)
					err = ret;
			}
		}
		swidget->spipe->complete = 0;
	}

	 
	if (swidget->dynamic_pipeline_widget && swidget->id != snd_soc_dapm_scheduler) {
		ret = sof_widget_free_unlocked(sdev, pipe_widget);
		if (ret < 0 && !err)
			err = ret;
	}

	if (!err)
		dev_dbg(sdev->dev, "widget %s freed\n", swidget->widget->name);

	return err;
}

int sof_widget_free(struct snd_sof_dev *sdev, struct snd_sof_widget *swidget)
{
	int ret;

	mutex_lock(&swidget->setup_mutex);
	ret = sof_widget_free_unlocked(sdev, swidget);
	mutex_unlock(&swidget->setup_mutex);

	return ret;
}
EXPORT_SYMBOL(sof_widget_free);

static int sof_widget_setup_unlocked(struct snd_sof_dev *sdev,
				     struct snd_sof_widget *swidget)
{
	const struct sof_ipc_tplg_ops *tplg_ops = sof_ipc_get_ops(sdev, tplg);
	struct snd_sof_pipeline *spipe = swidget->spipe;
	bool use_count_decremented = false;
	int ret;
	int i;

	 
	if (!swidget->private)
		return 0;

	trace_sof_widget_setup(swidget);

	 
	if (++swidget->use_count > 1)
		return 0;

	 
	if (swidget->dynamic_pipeline_widget && swidget->id != snd_soc_dapm_scheduler) {
		if (!swidget->spipe || !swidget->spipe->pipe_widget) {
			dev_err(sdev->dev, "No pipeline set for %s\n", swidget->widget->name);
			ret = -EINVAL;
			goto use_count_dec;
		}

		ret = sof_widget_setup_unlocked(sdev, swidget->spipe->pipe_widget);
		if (ret < 0)
			goto use_count_dec;
	}

	 
	if (swidget->id == snd_soc_dapm_scheduler) {
		for_each_set_bit(i, &spipe->core_mask, sdev->num_cores) {
			ret = snd_sof_dsp_core_get(sdev, i);
			if (ret < 0) {
				dev_err(sdev->dev, "failed to enable target core %d for pipeline %s\n",
					i, swidget->widget->name);
				goto pipe_widget_free;
			}
		}
	}

	 
	if (tplg_ops && tplg_ops->widget_setup) {
		ret = tplg_ops->widget_setup(sdev, swidget);
		if (ret < 0)
			goto pipe_widget_free;
	}

	 
	if (WIDGET_IS_DAI(swidget->id)) {
		unsigned int flags = SOF_DAI_CONFIG_FLAGS_HW_PARAMS;

		 
		if (tplg_ops && tplg_ops->dai_config) {
			ret = tplg_ops->dai_config(sdev, swidget, flags, NULL);
			if (ret < 0)
				goto widget_free;
		}
	}

	 
	if (tplg_ops && tplg_ops->control && tplg_ops->control->widget_kcontrol_setup) {
		ret = tplg_ops->control->widget_kcontrol_setup(sdev, swidget);
		if (ret < 0)
			goto widget_free;
	}

	dev_dbg(sdev->dev, "widget %s setup complete\n", swidget->widget->name);

	return 0;

widget_free:
	 
	sof_widget_free_unlocked(sdev, swidget);
	use_count_decremented = true;
pipe_widget_free:
	if (swidget->id != snd_soc_dapm_scheduler) {
		sof_widget_free_unlocked(sdev, swidget->spipe->pipe_widget);
	} else {
		int j;

		 
		for_each_set_bit(j, &spipe->core_mask, sdev->num_cores) {
			if (j >= i)
				break;
			snd_sof_dsp_core_put(sdev, j);
		}
	}
use_count_dec:
	if (!use_count_decremented)
		swidget->use_count--;

	return ret;
}

int sof_widget_setup(struct snd_sof_dev *sdev, struct snd_sof_widget *swidget)
{
	int ret;

	mutex_lock(&swidget->setup_mutex);
	ret = sof_widget_setup_unlocked(sdev, swidget);
	mutex_unlock(&swidget->setup_mutex);

	return ret;
}
EXPORT_SYMBOL(sof_widget_setup);

int sof_route_setup(struct snd_sof_dev *sdev, struct snd_soc_dapm_widget *wsource,
		    struct snd_soc_dapm_widget *wsink)
{
	const struct sof_ipc_tplg_ops *tplg_ops = sof_ipc_get_ops(sdev, tplg);
	struct snd_sof_widget *src_widget = wsource->dobj.private;
	struct snd_sof_widget *sink_widget = wsink->dobj.private;
	struct snd_sof_route *sroute;
	bool route_found = false;

	 
	if (is_virtual_widget(sdev, src_widget->widget, __func__) ||
	    is_virtual_widget(sdev, sink_widget->widget, __func__))
		return 0;

	 
	list_for_each_entry(sroute, &sdev->route_list, list)
		if (sroute->src_widget == src_widget && sroute->sink_widget == sink_widget) {
			route_found = true;
			break;
		}

	if (!route_found) {
		dev_err(sdev->dev, "error: cannot find SOF route for source %s -> %s sink\n",
			wsource->name, wsink->name);
		return -EINVAL;
	}

	 
	if (sroute->setup)
		return 0;

	if (tplg_ops && tplg_ops->route_setup) {
		int ret = tplg_ops->route_setup(sdev, sroute);

		if (ret < 0)
			return ret;
	}

	sroute->setup = true;
	return 0;
}

static int sof_setup_pipeline_connections(struct snd_sof_dev *sdev,
					  struct snd_soc_dapm_widget_list *list, int dir)
{
	const struct sof_ipc_tplg_ops *tplg_ops = sof_ipc_get_ops(sdev, tplg);
	struct snd_soc_dapm_widget *widget;
	struct snd_sof_route *sroute;
	struct snd_soc_dapm_path *p;
	int ret = 0;
	int i;

	 
	if (dir == SNDRV_PCM_STREAM_PLAYBACK) {
		for_each_dapm_widgets(list, i, widget) {
			if (!widget->dobj.private)
				continue;

			snd_soc_dapm_widget_for_each_sink_path(widget, p) {
				if (!widget_in_list(list, p->sink))
					continue;

				if (p->sink->dobj.private) {
					ret = sof_route_setup(sdev, widget, p->sink);
					if (ret < 0)
						return ret;
				}
			}
		}
	} else {
		for_each_dapm_widgets(list, i, widget) {
			if (!widget->dobj.private)
				continue;

			snd_soc_dapm_widget_for_each_source_path(widget, p) {
				if (!widget_in_list(list, p->source))
					continue;

				if (p->source->dobj.private) {
					ret = sof_route_setup(sdev, p->source, widget);
					if (ret < 0)
						return ret;
				}
			}
		}
	}

	 
	list_for_each_entry(sroute, &sdev->route_list, list) {
		bool src_widget_in_dapm_list, sink_widget_in_dapm_list;
		struct snd_sof_widget *swidget;

		if (sroute->setup)
			continue;

		src_widget_in_dapm_list = widget_in_list(list, sroute->src_widget->widget);
		sink_widget_in_dapm_list = widget_in_list(list, sroute->sink_widget->widget);

		 
		if (src_widget_in_dapm_list == sink_widget_in_dapm_list)
			continue;

		 
		if (src_widget_in_dapm_list)
			swidget = sroute->sink_widget;
		else
			swidget = sroute->src_widget;

		mutex_lock(&swidget->setup_mutex);
		if (!swidget->use_count) {
			mutex_unlock(&swidget->setup_mutex);
			continue;
		}

		if (tplg_ops && tplg_ops->route_setup) {
			 
			ret = tplg_ops->route_setup(sdev, sroute);
			if (!ret)
				sroute->setup = true;
		}

		mutex_unlock(&swidget->setup_mutex);

		if (ret < 0)
			return ret;
	}

	return 0;
}

static void
sof_unprepare_widgets_in_path(struct snd_sof_dev *sdev, struct snd_soc_dapm_widget *widget,
			      struct snd_soc_dapm_widget_list *list)
{
	const struct sof_ipc_tplg_ops *tplg_ops = sof_ipc_get_ops(sdev, tplg);
	struct snd_sof_widget *swidget = widget->dobj.private;
	const struct sof_ipc_tplg_widget_ops *widget_ops;
	struct snd_soc_dapm_path *p;

	if (is_virtual_widget(sdev, widget, __func__))
		return;

	 
	if (!swidget || !swidget->prepared || swidget->use_count > 0)
		goto sink_unprepare;

	widget_ops = tplg_ops ? tplg_ops->widget : NULL;
	if (widget_ops && widget_ops[widget->id].ipc_unprepare)
		 
		widget_ops[widget->id].ipc_unprepare(swidget);

	swidget->prepared = false;

sink_unprepare:
	 
	snd_soc_dapm_widget_for_each_sink_path(widget, p) {
		if (!widget_in_list(list, p->sink))
			continue;
		if (!p->walking && p->sink->dobj.private) {
			p->walking = true;
			sof_unprepare_widgets_in_path(sdev, p->sink, list);
			p->walking = false;
		}
	}
}

static int
sof_prepare_widgets_in_path(struct snd_sof_dev *sdev, struct snd_soc_dapm_widget *widget,
			    struct snd_pcm_hw_params *fe_params,
			    struct snd_sof_platform_stream_params *platform_params,
			    struct snd_pcm_hw_params *pipeline_params, int dir,
			    struct snd_soc_dapm_widget_list *list)
{
	const struct sof_ipc_tplg_ops *tplg_ops = sof_ipc_get_ops(sdev, tplg);
	struct snd_sof_widget *swidget = widget->dobj.private;
	const struct sof_ipc_tplg_widget_ops *widget_ops;
	struct snd_soc_dapm_path *p;
	int ret;

	if (is_virtual_widget(sdev, widget, __func__))
		return 0;

	widget_ops = tplg_ops ? tplg_ops->widget : NULL;
	if (!widget_ops)
		return 0;

	if (!swidget || !widget_ops[widget->id].ipc_prepare || swidget->prepared)
		goto sink_prepare;

	 
	ret = widget_ops[widget->id].ipc_prepare(swidget, fe_params, platform_params,
					     pipeline_params, dir);
	if (ret < 0) {
		dev_err(sdev->dev, "failed to prepare widget %s\n", widget->name);
		return ret;
	}

	swidget->prepared = true;

sink_prepare:
	 
	snd_soc_dapm_widget_for_each_sink_path(widget, p) {
		if (!widget_in_list(list, p->sink))
			continue;
		if (!p->walking && p->sink->dobj.private) {
			p->walking = true;
			ret = sof_prepare_widgets_in_path(sdev, p->sink,  fe_params,
							  platform_params, pipeline_params, dir,
							  list);
			p->walking = false;
			if (ret < 0) {
				 
				if (widget_ops[widget->id].ipc_unprepare &&
				    swidget && swidget->prepared) {
					widget_ops[widget->id].ipc_unprepare(swidget);
					swidget->prepared = false;
				}
				return ret;
			}
		}
	}

	return 0;
}

 
static int sof_free_widgets_in_path(struct snd_sof_dev *sdev, struct snd_soc_dapm_widget *widget,
				    int dir, struct snd_sof_pcm *spcm)
{
	struct snd_soc_dapm_widget_list *list = spcm->stream[dir].list;
	struct snd_soc_dapm_path *p;
	int err;
	int ret = 0;

	if (is_virtual_widget(sdev, widget, __func__))
		return 0;

	if (widget->dobj.private) {
		err = sof_widget_free(sdev, widget->dobj.private);
		if (err < 0)
			ret = err;
	}

	 
	snd_soc_dapm_widget_for_each_sink_path(widget, p) {
		if (!p->walking) {
			if (!widget_in_list(list, p->sink))
				continue;

			p->walking = true;

			err = sof_free_widgets_in_path(sdev, p->sink, dir, spcm);
			if (err < 0)
				ret = err;
			p->walking = false;
		}
	}

	return ret;
}

 
static int sof_set_up_widgets_in_path(struct snd_sof_dev *sdev, struct snd_soc_dapm_widget *widget,
				      int dir, struct snd_sof_pcm *spcm)
{
	struct snd_sof_pcm_stream_pipeline_list *pipeline_list = &spcm->stream[dir].pipeline_list;
	struct snd_soc_dapm_widget_list *list = spcm->stream[dir].list;
	struct snd_sof_widget *swidget = widget->dobj.private;
	struct snd_sof_pipeline *spipe;
	struct snd_soc_dapm_path *p;
	int ret;

	if (is_virtual_widget(sdev, widget, __func__))
		return 0;

	if (swidget) {
		int i;

		ret = sof_widget_setup(sdev, widget->dobj.private);
		if (ret < 0)
			return ret;

		 
		if (!pipeline_list->pipelines)
			goto sink_setup;

		 
		for (i = 0; i < pipeline_list->count; i++) {
			spipe = pipeline_list->pipelines[i];
			if (spipe == swidget->spipe)
				break;
		}

		if (i == pipeline_list->count) {
			pipeline_list->count++;
			pipeline_list->pipelines[i] = swidget->spipe;
		}
	}

sink_setup:
	snd_soc_dapm_widget_for_each_sink_path(widget, p) {
		if (!p->walking) {
			if (!widget_in_list(list, p->sink))
				continue;

			p->walking = true;

			ret = sof_set_up_widgets_in_path(sdev, p->sink, dir, spcm);
			p->walking = false;
			if (ret < 0) {
				if (swidget)
					sof_widget_free(sdev, swidget);
				return ret;
			}
		}
	}

	return 0;
}

static int
sof_walk_widgets_in_order(struct snd_sof_dev *sdev, struct snd_sof_pcm *spcm,
			  struct snd_pcm_hw_params *fe_params,
			  struct snd_sof_platform_stream_params *platform_params, int dir,
			  enum sof_widget_op op)
{
	struct snd_soc_dapm_widget_list *list = spcm->stream[dir].list;
	struct snd_soc_dapm_widget *widget;
	char *str;
	int ret = 0;
	int i;

	if (!list)
		return 0;

	for_each_dapm_widgets(list, i, widget) {
		if (is_virtual_widget(sdev, widget, __func__))
			continue;

		 
		if (dir == SNDRV_PCM_STREAM_PLAYBACK && widget->id != snd_soc_dapm_aif_in)
			continue;

		 
		if (dir == SNDRV_PCM_STREAM_CAPTURE && widget->id != snd_soc_dapm_dai_out)
			continue;

		switch (op) {
		case SOF_WIDGET_SETUP:
			ret = sof_set_up_widgets_in_path(sdev, widget, dir, spcm);
			str = "set up";
			break;
		case SOF_WIDGET_FREE:
			ret = sof_free_widgets_in_path(sdev, widget, dir, spcm);
			str = "free";
			break;
		case SOF_WIDGET_PREPARE:
		{
			struct snd_pcm_hw_params pipeline_params;

			str = "prepare";
			 
			memcpy(&pipeline_params, fe_params, sizeof(*fe_params));

			ret = sof_prepare_widgets_in_path(sdev, widget, fe_params, platform_params,
							  &pipeline_params, dir, list);
			break;
		}
		case SOF_WIDGET_UNPREPARE:
			sof_unprepare_widgets_in_path(sdev, widget, list);
			break;
		default:
			dev_err(sdev->dev, "Invalid widget op %d\n", op);
			return -EINVAL;
		}
		if (ret < 0) {
			dev_err(sdev->dev, "Failed to %s connected widgets\n", str);
			return ret;
		}
	}

	return 0;
}

int sof_widget_list_setup(struct snd_sof_dev *sdev, struct snd_sof_pcm *spcm,
			  struct snd_pcm_hw_params *fe_params,
			  struct snd_sof_platform_stream_params *platform_params,
			  int dir)
{
	const struct sof_ipc_tplg_ops *tplg_ops = sof_ipc_get_ops(sdev, tplg);
	struct snd_soc_dapm_widget_list *list = spcm->stream[dir].list;
	struct snd_soc_dapm_widget *widget;
	int i, ret;

	 
	if (!list)
		return 0;

	 
	ret = sof_walk_widgets_in_order(sdev, spcm, fe_params, platform_params,
					dir, SOF_WIDGET_PREPARE);
	if (ret < 0)
		return ret;

	 
	ret = sof_walk_widgets_in_order(sdev, spcm, fe_params, platform_params,
					dir, SOF_WIDGET_SETUP);
	if (ret < 0) {
		sof_walk_widgets_in_order(sdev, spcm, fe_params, platform_params,
					  dir, SOF_WIDGET_UNPREPARE);
		return ret;
	}

	 
	ret = sof_setup_pipeline_connections(sdev, list, dir);
	if (ret < 0)
		goto widget_free;

	 
	for_each_dapm_widgets(list, i, widget) {
		struct snd_sof_widget *swidget = widget->dobj.private;
		struct snd_sof_widget *pipe_widget;
		struct snd_sof_pipeline *spipe;

		if (!swidget || sdev->dspless_mode_selected)
			continue;

		spipe = swidget->spipe;
		if (!spipe) {
			dev_err(sdev->dev, "no pipeline found for %s\n",
				swidget->widget->name);
			ret = -EINVAL;
			goto widget_free;
		}

		pipe_widget = spipe->pipe_widget;
		if (!pipe_widget) {
			dev_err(sdev->dev, "error: no pipeline widget found for %s\n",
				swidget->widget->name);
			ret = -EINVAL;
			goto widget_free;
		}

		if (spipe->complete)
			continue;

		if (tplg_ops && tplg_ops->pipeline_complete) {
			spipe->complete = tplg_ops->pipeline_complete(sdev, pipe_widget);
			if (spipe->complete < 0) {
				ret = spipe->complete;
				goto widget_free;
			}
		}
	}

	return 0;

widget_free:
	sof_walk_widgets_in_order(sdev, spcm, fe_params, platform_params, dir,
				  SOF_WIDGET_FREE);
	sof_walk_widgets_in_order(sdev, spcm, NULL, NULL, dir, SOF_WIDGET_UNPREPARE);

	return ret;
}

int sof_widget_list_free(struct snd_sof_dev *sdev, struct snd_sof_pcm *spcm, int dir)
{
	struct snd_sof_pcm_stream_pipeline_list *pipeline_list = &spcm->stream[dir].pipeline_list;
	struct snd_soc_dapm_widget_list *list = spcm->stream[dir].list;
	int ret;

	 
	if (!list)
		return 0;

	 
	ret = sof_walk_widgets_in_order(sdev, spcm, NULL, NULL, dir, SOF_WIDGET_FREE);

	 
	sof_walk_widgets_in_order(sdev, spcm, NULL, NULL, dir, SOF_WIDGET_UNPREPARE);

	snd_soc_dapm_dai_free_widgets(&list);
	spcm->stream[dir].list = NULL;

	pipeline_list->count = 0;

	return ret;
}

 
bool snd_sof_dsp_only_d0i3_compatible_stream_active(struct snd_sof_dev *sdev)
{
	struct snd_pcm_substream *substream;
	struct snd_sof_pcm *spcm;
	bool d0i3_compatible_active = false;
	int dir;

	list_for_each_entry(spcm, &sdev->pcm_list, list) {
		for_each_pcm_streams(dir) {
			substream = spcm->stream[dir].substream;
			if (!substream || !substream->runtime)
				continue;

			 
			if (!spcm->stream[dir].d0i3_compatible)
				return false;

			d0i3_compatible_active = true;
		}
	}

	return d0i3_compatible_active;
}
EXPORT_SYMBOL(snd_sof_dsp_only_d0i3_compatible_stream_active);

bool snd_sof_stream_suspend_ignored(struct snd_sof_dev *sdev)
{
	struct snd_sof_pcm *spcm;

	list_for_each_entry(spcm, &sdev->pcm_list, list) {
		if (spcm->stream[SNDRV_PCM_STREAM_PLAYBACK].suspend_ignored ||
		    spcm->stream[SNDRV_PCM_STREAM_CAPTURE].suspend_ignored)
			return true;
	}

	return false;
}

int sof_pcm_stream_free(struct snd_sof_dev *sdev, struct snd_pcm_substream *substream,
			struct snd_sof_pcm *spcm, int dir, bool free_widget_list)
{
	const struct sof_ipc_pcm_ops *pcm_ops = sof_ipc_get_ops(sdev, pcm);
	int ret;

	if (spcm->prepared[substream->stream]) {
		 
		if (pcm_ops && pcm_ops->platform_stop_during_hw_free)
			snd_sof_pcm_platform_trigger(sdev, substream, SNDRV_PCM_TRIGGER_STOP);

		 
		if (pcm_ops && pcm_ops->hw_free) {
			ret = pcm_ops->hw_free(sdev->component, substream);
			if (ret < 0)
				return ret;
		}

		spcm->prepared[substream->stream] = false;
	}

	 
	ret = snd_sof_pcm_platform_hw_free(sdev, substream);
	if (ret < 0)
		return ret;

	 
	if (free_widget_list) {
		ret = sof_widget_list_free(sdev, spcm, dir);
		if (ret < 0)
			dev_err(sdev->dev, "failed to free widgets during suspend\n");
	}

	return ret;
}

 

struct snd_sof_pcm *snd_sof_find_spcm_name(struct snd_soc_component *scomp,
					   const char *name)
{
	struct snd_sof_dev *sdev = snd_soc_component_get_drvdata(scomp);
	struct snd_sof_pcm *spcm;

	list_for_each_entry(spcm, &sdev->pcm_list, list) {
		 
		if (strcmp(spcm->pcm.dai_name, name) == 0)
			return spcm;

		 
		if (*spcm->pcm.caps[0].name &&
		    !strcmp(spcm->pcm.caps[0].name, name))
			return spcm;

		 
		if (*spcm->pcm.caps[1].name &&
		    !strcmp(spcm->pcm.caps[1].name, name))
			return spcm;
	}

	return NULL;
}

struct snd_sof_pcm *snd_sof_find_spcm_comp(struct snd_soc_component *scomp,
					   unsigned int comp_id,
					   int *direction)
{
	struct snd_sof_dev *sdev = snd_soc_component_get_drvdata(scomp);
	struct snd_sof_pcm *spcm;
	int dir;

	list_for_each_entry(spcm, &sdev->pcm_list, list) {
		for_each_pcm_streams(dir) {
			if (spcm->stream[dir].comp_id == comp_id) {
				*direction = dir;
				return spcm;
			}
		}
	}

	return NULL;
}

struct snd_sof_widget *snd_sof_find_swidget(struct snd_soc_component *scomp,
					    const char *name)
{
	struct snd_sof_dev *sdev = snd_soc_component_get_drvdata(scomp);
	struct snd_sof_widget *swidget;

	list_for_each_entry(swidget, &sdev->widget_list, list) {
		if (strcmp(name, swidget->widget->name) == 0)
			return swidget;
	}

	return NULL;
}

 
struct snd_sof_widget *
snd_sof_find_swidget_sname(struct snd_soc_component *scomp,
			   const char *pcm_name, int dir)
{
	struct snd_sof_dev *sdev = snd_soc_component_get_drvdata(scomp);
	struct snd_sof_widget *swidget;
	enum snd_soc_dapm_type type;

	if (dir == SNDRV_PCM_STREAM_PLAYBACK)
		type = snd_soc_dapm_aif_in;
	else
		type = snd_soc_dapm_aif_out;

	list_for_each_entry(swidget, &sdev->widget_list, list) {
		if (!strcmp(pcm_name, swidget->widget->sname) &&
		    swidget->id == type)
			return swidget;
	}

	return NULL;
}

struct snd_sof_dai *snd_sof_find_dai(struct snd_soc_component *scomp,
				     const char *name)
{
	struct snd_sof_dev *sdev = snd_soc_component_get_drvdata(scomp);
	struct snd_sof_dai *dai;

	list_for_each_entry(dai, &sdev->dai_list, list) {
		if (dai->name && (strcmp(name, dai->name) == 0))
			return dai;
	}

	return NULL;
}

static int sof_dai_get_clk(struct snd_soc_pcm_runtime *rtd, int clk_type)
{
	struct snd_soc_component *component =
		snd_soc_rtdcom_lookup(rtd, SOF_AUDIO_PCM_DRV_NAME);
	struct snd_sof_dai *dai =
		snd_sof_find_dai(component, (char *)rtd->dai_link->name);
	struct snd_sof_dev *sdev = snd_soc_component_get_drvdata(component);
	const struct sof_ipc_tplg_ops *tplg_ops = sof_ipc_get_ops(sdev, tplg);

	 
	if (!dai)
		return 0;

	if (tplg_ops && tplg_ops->dai_get_clk)
		return tplg_ops->dai_get_clk(sdev, dai, clk_type);

	return 0;
}

 
int sof_dai_get_mclk(struct snd_soc_pcm_runtime *rtd)
{
	return sof_dai_get_clk(rtd, SOF_DAI_CLK_INTEL_SSP_MCLK);
}
EXPORT_SYMBOL(sof_dai_get_mclk);

 
int sof_dai_get_bclk(struct snd_soc_pcm_runtime *rtd)
{
	return sof_dai_get_clk(rtd, SOF_DAI_CLK_INTEL_SSP_BCLK);
}
EXPORT_SYMBOL(sof_dai_get_bclk);

static struct snd_sof_of_mach *sof_of_machine_select(struct snd_sof_dev *sdev)
{
	struct snd_sof_pdata *sof_pdata = sdev->pdata;
	const struct sof_dev_desc *desc = sof_pdata->desc;
	struct snd_sof_of_mach *mach = desc->of_machines;

	if (!mach)
		return NULL;

	for (; mach->compatible; mach++) {
		if (of_machine_is_compatible(mach->compatible)) {
			sof_pdata->tplg_filename = mach->sof_tplg_filename;
			if (mach->fw_filename)
				sof_pdata->fw_filename = mach->fw_filename;

			return mach;
		}
	}

	return NULL;
}

 
int sof_machine_check(struct snd_sof_dev *sdev)
{
	struct snd_sof_pdata *sof_pdata = sdev->pdata;
	const struct sof_dev_desc *desc = sof_pdata->desc;
	struct snd_soc_acpi_mach *mach;

	if (!IS_ENABLED(CONFIG_SND_SOC_SOF_FORCE_NOCODEC_MODE)) {
		const struct snd_sof_of_mach *of_mach;

		if (IS_ENABLED(CONFIG_SND_SOC_SOF_NOCODEC_DEBUG_SUPPORT) &&
		    sof_debug_check_flag(SOF_DBG_FORCE_NOCODEC))
			goto nocodec;

		 
		mach = snd_sof_machine_select(sdev);
		if (mach) {
			sof_pdata->machine = mach;

			if (sof_pdata->subsystem_id_set) {
				mach->mach_params.subsystem_vendor = sof_pdata->subsystem_vendor;
				mach->mach_params.subsystem_device = sof_pdata->subsystem_device;
				mach->mach_params.subsystem_id_set = true;
			}

			snd_sof_set_mach_params(mach, sdev);
			return 0;
		}

		of_mach = sof_of_machine_select(sdev);
		if (of_mach) {
			sof_pdata->of_machine = of_mach;
			return 0;
		}

		if (!IS_ENABLED(CONFIG_SND_SOC_SOF_NOCODEC)) {
			dev_err(sdev->dev, "error: no matching ASoC machine driver found - aborting probe\n");
			return -ENODEV;
		}
	} else {
		dev_warn(sdev->dev, "Force to use nocodec mode\n");
	}

nocodec:
	 
	dev_warn(sdev->dev, "Using nocodec machine driver\n");
	mach = devm_kzalloc(sdev->dev, sizeof(*mach), GFP_KERNEL);
	if (!mach)
		return -ENOMEM;

	mach->drv_name = "sof-nocodec";
	if (!sof_pdata->tplg_filename)
		sof_pdata->tplg_filename = desc->nocodec_tplg_filename;

	sof_pdata->machine = mach;
	snd_sof_set_mach_params(mach, sdev);

	return 0;
}
EXPORT_SYMBOL(sof_machine_check);

int sof_machine_register(struct snd_sof_dev *sdev, void *pdata)
{
	struct snd_sof_pdata *plat_data = pdata;
	const char *drv_name;
	const void *mach;
	int size;

	drv_name = plat_data->machine->drv_name;
	mach = plat_data->machine;
	size = sizeof(*plat_data->machine);

	 
	plat_data->pdev_mach =
		platform_device_register_data(sdev->dev, drv_name,
					      PLATFORM_DEVID_NONE, mach, size);
	if (IS_ERR(plat_data->pdev_mach))
		return PTR_ERR(plat_data->pdev_mach);

	dev_dbg(sdev->dev, "created machine %s\n",
		dev_name(&plat_data->pdev_mach->dev));

	return 0;
}
EXPORT_SYMBOL(sof_machine_register);

void sof_machine_unregister(struct snd_sof_dev *sdev, void *pdata)
{
	struct snd_sof_pdata *plat_data = pdata;

	if (!IS_ERR_OR_NULL(plat_data->pdev_mach))
		platform_device_unregister(plat_data->pdev_mach);
}
EXPORT_SYMBOL(sof_machine_unregister);
