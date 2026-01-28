#ifndef __SOUND_SOC_SOF_AUDIO_H
#define __SOUND_SOC_SOF_AUDIO_H
#include <linux/workqueue.h>
#include <sound/soc.h>
#include <sound/control.h>
#include <sound/sof/stream.h>  
#include <sound/sof/control.h>
#include <sound/sof/dai.h>
#include <sound/sof/topology.h>
#include "sof-priv.h"
#define SOF_AUDIO_PCM_DRV_NAME	"sof-audio-component"
#define SOF_WIDGET_MAX_NUM_PINS	8
#define SOF_PIN_TYPE_INPUT	0
#define SOF_PIN_TYPE_OUTPUT	1
#define SOF_BE_PCM_BASE		16
#define DMA_CHAN_INVALID	0xFFFFFFFF
#define WIDGET_IS_DAI(id) ((id) == snd_soc_dapm_dai_in || (id) == snd_soc_dapm_dai_out)
#define WIDGET_IS_AIF(id) ((id) == snd_soc_dapm_aif_in || (id) == snd_soc_dapm_aif_out)
#define WIDGET_IS_AIF_OR_DAI(id) (WIDGET_IS_DAI(id) || WIDGET_IS_AIF(id))
#define WIDGET_IS_COPIER(id) (WIDGET_IS_AIF_OR_DAI(id) || (id) == snd_soc_dapm_buffer)
#define SOF_DAI_CLK_INTEL_SSP_MCLK	0
#define SOF_DAI_CLK_INTEL_SSP_BCLK	1
enum sof_widget_op {
	SOF_WIDGET_PREPARE,
	SOF_WIDGET_SETUP,
	SOF_WIDGET_FREE,
	SOF_WIDGET_UNPREPARE,
};
#define VOLUME_FWL	16
#define SOF_TLV_ITEMS 3
static inline u32 mixer_to_ipc(unsigned int value, u32 *volume_map, int size)
{
	if (value >= size)
		return volume_map[size - 1];
	return volume_map[value];
}
static inline u32 ipc_to_mixer(u32 value, u32 *volume_map, int size)
{
	int i;
	for (i = 0; i < size; i++) {
		if (volume_map[i] >= value)
			return i;
	}
	return i - 1;
}
struct snd_sof_widget;
struct snd_sof_route;
struct snd_sof_control;
struct snd_sof_dai;
struct snd_sof_pcm;
struct snd_sof_dai_config_data {
	int dai_index;
	int dai_data;  
};
struct sof_ipc_pcm_ops {
	int (*hw_params)(struct snd_soc_component *component, struct snd_pcm_substream *substream,
			 struct snd_pcm_hw_params *params,
			 struct snd_sof_platform_stream_params *platform_params);
	int (*hw_free)(struct snd_soc_component *component, struct snd_pcm_substream *substream);
	int (*trigger)(struct snd_soc_component *component,  struct snd_pcm_substream *substream,
		       int cmd);
	int (*dai_link_fixup)(struct snd_soc_pcm_runtime *rtd, struct snd_pcm_hw_params *params);
	int (*pcm_setup)(struct snd_sof_dev *sdev, struct snd_sof_pcm *spcm);
	void (*pcm_free)(struct snd_sof_dev *sdev, struct snd_sof_pcm *spcm);
	snd_pcm_sframes_t (*delay)(struct snd_soc_component *component,
				   struct snd_pcm_substream *substream);
	bool reset_hw_params_during_stop;
	bool ipc_first_on_start;
	bool platform_stop_during_hw_free;
};
struct sof_ipc_tplg_control_ops {
	bool (*volume_put)(struct snd_sof_control *scontrol, struct snd_ctl_elem_value *ucontrol);
	int (*volume_get)(struct snd_sof_control *scontrol, struct snd_ctl_elem_value *ucontrol);
	bool (*switch_put)(struct snd_sof_control *scontrol, struct snd_ctl_elem_value *ucontrol);
	int (*switch_get)(struct snd_sof_control *scontrol, struct snd_ctl_elem_value *ucontrol);
	bool (*enum_put)(struct snd_sof_control *scontrol, struct snd_ctl_elem_value *ucontrol);
	int (*enum_get)(struct snd_sof_control *scontrol, struct snd_ctl_elem_value *ucontrol);
	int (*bytes_put)(struct snd_sof_control *scontrol, struct snd_ctl_elem_value *ucontrol);
	int (*bytes_get)(struct snd_sof_control *scontrol, struct snd_ctl_elem_value *ucontrol);
	int (*bytes_ext_get)(struct snd_sof_control *scontrol,
			     const unsigned int __user *binary_data, unsigned int size);
	int (*bytes_ext_volatile_get)(struct snd_sof_control *scontrol,
				      const unsigned int __user *binary_data, unsigned int size);
	int (*bytes_ext_put)(struct snd_sof_control *scontrol,
			     const unsigned int __user *binary_data, unsigned int size);
	void (*update)(struct snd_sof_dev *sdev, void *ipc_control_message);
	int (*widget_kcontrol_setup)(struct snd_sof_dev *sdev, struct snd_sof_widget *swidget);
	int (*set_up_volume_table)(struct snd_sof_control *scontrol, int tlv[SOF_TLV_ITEMS],
				   int size);
};
struct sof_ipc_tplg_widget_ops {
	int (*ipc_setup)(struct snd_sof_widget *swidget);
	void (*ipc_free)(struct snd_sof_widget *swidget);
	enum sof_tokens *token_list;
	int token_list_size;
	int (*bind_event)(struct snd_soc_component *scomp, struct snd_sof_widget *swidget,
			  u16 event_type);
	int (*ipc_prepare)(struct snd_sof_widget *swidget,
			   struct snd_pcm_hw_params *fe_params,
			   struct snd_sof_platform_stream_params *platform_params,
			   struct snd_pcm_hw_params *source_params, int dir);
	void (*ipc_unprepare)(struct snd_sof_widget *swidget);
};
struct sof_ipc_tplg_ops {
	const struct sof_ipc_tplg_widget_ops *widget;
	const struct sof_ipc_tplg_control_ops *control;
	int (*route_setup)(struct snd_sof_dev *sdev, struct snd_sof_route *sroute);
	int (*route_free)(struct snd_sof_dev *sdev, struct snd_sof_route *sroute);
	const struct sof_token_info *token_list;
	int (*control_setup)(struct snd_sof_dev *sdev, struct snd_sof_control *scontrol);
	int (*control_free)(struct snd_sof_dev *sdev, struct snd_sof_control *scontrol);
	int (*pipeline_complete)(struct snd_sof_dev *sdev, struct snd_sof_widget *swidget);
	int (*widget_setup)(struct snd_sof_dev *sdev, struct snd_sof_widget *swidget);
	int (*widget_free)(struct snd_sof_dev *sdev, struct snd_sof_widget *swidget);
	int (*dai_config)(struct snd_sof_dev *sdev, struct snd_sof_widget *swidget,
			  unsigned int flags, struct snd_sof_dai_config_data *data);
	int (*dai_get_clk)(struct snd_sof_dev *sdev, struct snd_sof_dai *dai, int clk_type);
	int (*set_up_all_pipelines)(struct snd_sof_dev *sdev, bool verify);
	int (*tear_down_all_pipelines)(struct snd_sof_dev *sdev, bool verify);
	int (*parse_manifest)(struct snd_soc_component *scomp, int index,
			      struct snd_soc_tplg_manifest *man);
	int (*link_setup)(struct snd_sof_dev *sdev, struct snd_soc_dai_link *link);
};
struct snd_sof_tuple {
	u32 token;
	union {
		u32 v;
		const char *s;
	} value;
};
enum sof_tokens {
	SOF_PCM_TOKENS,
	SOF_PIPELINE_TOKENS,
	SOF_SCHED_TOKENS,
	SOF_ASRC_TOKENS,
	SOF_SRC_TOKENS,
	SOF_COMP_TOKENS,
	SOF_BUFFER_TOKENS,
	SOF_VOLUME_TOKENS,
	SOF_PROCESS_TOKENS,
	SOF_DAI_TOKENS,
	SOF_DAI_LINK_TOKENS,
	SOF_HDA_TOKENS,
	SOF_SSP_TOKENS,
	SOF_ALH_TOKENS,
	SOF_DMIC_TOKENS,
	SOF_DMIC_PDM_TOKENS,
	SOF_ESAI_TOKENS,
	SOF_SAI_TOKENS,
	SOF_AFE_TOKENS,
	SOF_CORE_TOKENS,
	SOF_COMP_EXT_TOKENS,
	SOF_IN_AUDIO_FORMAT_TOKENS,
	SOF_OUT_AUDIO_FORMAT_TOKENS,
	SOF_COPIER_DEEP_BUFFER_TOKENS,
	SOF_COPIER_TOKENS,
	SOF_AUDIO_FMT_NUM_TOKENS,
	SOF_COPIER_FORMAT_TOKENS,
	SOF_GAIN_TOKENS,
	SOF_ACPDMIC_TOKENS,
	SOF_ACPI2S_TOKENS,
	SOF_TOKEN_COUNT,
};
struct sof_topology_token {
	u32 token;
	u32 type;
	int (*get_token)(void *elem, void *object, u32 offset);
	u32 offset;
};
struct sof_token_info {
	const char *name;
	const struct sof_topology_token *tokens;
	int count;
};
struct snd_sof_pcm_stream_pipeline_list {
	u32 count;
	struct snd_sof_pipeline **pipelines;
};
struct snd_sof_pcm_stream {
	u32 comp_id;
	struct snd_dma_buffer page_table;
	struct sof_ipc_stream_posn posn;
	struct snd_pcm_substream *substream;
	struct snd_compr_stream *cstream;
	struct work_struct period_elapsed_work;
	struct snd_soc_dapm_widget_list *list;  
	bool d0i3_compatible;  
	bool suspend_ignored;
	struct snd_sof_pcm_stream_pipeline_list pipeline_list;
	void *private;
};
struct snd_sof_pcm {
	struct snd_soc_component *scomp;
	struct snd_soc_tplg_pcm pcm;
	struct snd_sof_pcm_stream stream[2];
	struct list_head list;	 
	struct snd_pcm_hw_params params[2];
	bool prepared[2];  
};
struct snd_sof_led_control {
	unsigned int use_led;
	unsigned int direction;
	int led_value;
};
struct snd_sof_control {
	struct snd_soc_component *scomp;
	const char *name;
	int comp_id;
	int min_volume_step;  
	int max_volume_step;  
	int num_channels;
	unsigned int access;
	int info_type;
	int index;  
	void *priv;  
	size_t priv_size;  
	size_t max_size;
	void *ipc_control_data;
	void *old_ipc_control_data;
	int max;  
	u32 size;	 
	u32 *volume_table;  
	struct list_head list;	 
	struct snd_sof_led_control led_ctl;
	bool comp_data_dirty;
};
struct snd_sof_dai_link {
	struct snd_sof_tuple *tuples;
	int num_tuples;
	struct snd_soc_dai_link *link;
	struct snd_soc_tplg_hw_config *hw_configs;
	int num_hw_configs;
	int default_hw_cfg_id;
	int type;
	struct list_head list;
};
struct snd_sof_widget {
	struct snd_soc_component *scomp;
	int comp_id;
	int pipeline_id;
	bool prepared;
	struct mutex setup_mutex;  
	int use_count;
	int core;
	int id;  
	int instance_id;
	bool dynamic_pipeline_widget;
	struct snd_soc_dapm_widget *widget;
	struct list_head list;	 
	struct snd_sof_pipeline *spipe;
	void *module_info;
	const guid_t uuid;
	int num_tuples;
	struct snd_sof_tuple *tuples;
	u32 num_input_pins;
	u32 num_output_pins;
	char **input_pin_binding;
	char **output_pin_binding;
	struct ida output_queue_ida;
	struct ida input_queue_ida;
	void *private;		 
};
struct snd_sof_pipeline {
	struct snd_sof_widget *pipe_widget;
	int started_count;
	int paused_count;
	int complete;
	unsigned long core_mask;
	struct list_head list;
};
struct snd_sof_route {
	struct snd_soc_component *scomp;
	struct snd_soc_dapm_route *route;
	struct list_head list;	 
	struct snd_sof_widget *src_widget;
	struct snd_sof_widget *sink_widget;
	bool setup;
	int src_queue_id;
	int dst_queue_id;
	void *private;
};
struct snd_sof_dai {
	struct snd_soc_component *scomp;
	const char *name;
	int number_configs;
	int current_config;
	struct list_head list;	 
	const void *platform_private;
	void *private;
};
int snd_sof_volume_get(struct snd_kcontrol *kcontrol,
		       struct snd_ctl_elem_value *ucontrol);
int snd_sof_volume_put(struct snd_kcontrol *kcontrol,
		       struct snd_ctl_elem_value *ucontrol);
int snd_sof_volume_info(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_info *uinfo);
int snd_sof_switch_get(struct snd_kcontrol *kcontrol,
		       struct snd_ctl_elem_value *ucontrol);
int snd_sof_switch_put(struct snd_kcontrol *kcontrol,
		       struct snd_ctl_elem_value *ucontrol);
int snd_sof_enum_get(struct snd_kcontrol *kcontrol,
		     struct snd_ctl_elem_value *ucontrol);
int snd_sof_enum_put(struct snd_kcontrol *kcontrol,
		     struct snd_ctl_elem_value *ucontrol);
int snd_sof_bytes_get(struct snd_kcontrol *kcontrol,
		      struct snd_ctl_elem_value *ucontrol);
int snd_sof_bytes_put(struct snd_kcontrol *kcontrol,
		      struct snd_ctl_elem_value *ucontrol);
int snd_sof_bytes_ext_put(struct snd_kcontrol *kcontrol,
			  const unsigned int __user *binary_data,
			  unsigned int size);
int snd_sof_bytes_ext_get(struct snd_kcontrol *kcontrol,
			  unsigned int __user *binary_data,
			  unsigned int size);
int snd_sof_bytes_ext_volatile_get(struct snd_kcontrol *kcontrol, unsigned int __user *binary_data,
				   unsigned int size);
void snd_sof_control_notify(struct snd_sof_dev *sdev,
			    struct sof_ipc_ctrl_data *cdata);
int snd_sof_load_topology(struct snd_soc_component *scomp, const char *file);
int snd_sof_ipc_stream_posn(struct snd_soc_component *scomp,
			    struct snd_sof_pcm *spcm, int direction,
			    struct sof_ipc_stream_posn *posn);
struct snd_sof_widget *snd_sof_find_swidget(struct snd_soc_component *scomp,
					    const char *name);
struct snd_sof_widget *
snd_sof_find_swidget_sname(struct snd_soc_component *scomp,
			   const char *pcm_name, int dir);
struct snd_sof_dai *snd_sof_find_dai(struct snd_soc_component *scomp,
				     const char *name);
static inline
struct snd_sof_pcm *snd_sof_find_spcm_dai(struct snd_soc_component *scomp,
					  struct snd_soc_pcm_runtime *rtd)
{
	struct snd_sof_dev *sdev = snd_soc_component_get_drvdata(scomp);
	struct snd_sof_pcm *spcm;
	list_for_each_entry(spcm, &sdev->pcm_list, list) {
		if (le32_to_cpu(spcm->pcm.dai_id) == rtd->dai_link->id)
			return spcm;
	}
	return NULL;
}
struct snd_sof_pcm *snd_sof_find_spcm_name(struct snd_soc_component *scomp,
					   const char *name);
struct snd_sof_pcm *snd_sof_find_spcm_comp(struct snd_soc_component *scomp,
					   unsigned int comp_id,
					   int *direction);
void snd_sof_pcm_period_elapsed(struct snd_pcm_substream *substream);
void snd_sof_pcm_init_elapsed_work(struct work_struct *work);
#if IS_ENABLED(CONFIG_SND_SOC_SOF_COMPRESS)
void snd_sof_compr_fragment_elapsed(struct snd_compr_stream *cstream);
void snd_sof_compr_init_elapsed_work(struct work_struct *work);
#else
static inline void snd_sof_compr_fragment_elapsed(struct snd_compr_stream *cstream) { }
static inline void snd_sof_compr_init_elapsed_work(struct work_struct *work) { }
#endif
int sof_pcm_dai_link_fixup(struct snd_soc_pcm_runtime *rtd, struct snd_pcm_hw_params *params);
bool snd_sof_stream_suspend_ignored(struct snd_sof_dev *sdev);
bool snd_sof_dsp_only_d0i3_compatible_stream_active(struct snd_sof_dev *sdev);
int sof_machine_register(struct snd_sof_dev *sdev, void *pdata);
void sof_machine_unregister(struct snd_sof_dev *sdev, void *pdata);
int sof_widget_setup(struct snd_sof_dev *sdev, struct snd_sof_widget *swidget);
int sof_widget_free(struct snd_sof_dev *sdev, struct snd_sof_widget *swidget);
int sof_route_setup(struct snd_sof_dev *sdev, struct snd_soc_dapm_widget *wsource,
		    struct snd_soc_dapm_widget *wsink);
int sof_widget_list_setup(struct snd_sof_dev *sdev, struct snd_sof_pcm *spcm,
			  struct snd_pcm_hw_params *fe_params,
			  struct snd_sof_platform_stream_params *platform_params,
			  int dir);
int sof_widget_list_free(struct snd_sof_dev *sdev, struct snd_sof_pcm *spcm, int dir);
int sof_pcm_dsp_pcm_free(struct snd_pcm_substream *substream, struct snd_sof_dev *sdev,
			 struct snd_sof_pcm *spcm);
int sof_pcm_stream_free(struct snd_sof_dev *sdev, struct snd_pcm_substream *substream,
			struct snd_sof_pcm *spcm, int dir, bool free_widget_list);
int get_token_u32(void *elem, void *object, u32 offset);
int get_token_u16(void *elem, void *object, u32 offset);
int get_token_comp_format(void *elem, void *object, u32 offset);
int get_token_dai_type(void *elem, void *object, u32 offset);
int get_token_uuid(void *elem, void *object, u32 offset);
int get_token_string(void *elem, void *object, u32 offset);
int sof_update_ipc_object(struct snd_soc_component *scomp, void *object, enum sof_tokens token_id,
			  struct snd_sof_tuple *tuples, int num_tuples,
			  size_t object_size, int token_instance_num);
u32 vol_compute_gain(u32 value, int *tlv);
#endif
