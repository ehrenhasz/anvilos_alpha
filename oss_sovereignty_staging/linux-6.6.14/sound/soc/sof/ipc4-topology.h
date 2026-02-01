 
 

#ifndef __INCLUDE_SOUND_SOF_IPC4_TOPOLOGY_H__
#define __INCLUDE_SOUND_SOF_IPC4_TOPOLOGY_H__

#include <sound/sof/ipc4/header.h>

#define SOF_IPC4_FW_PAGE_SIZE BIT(12)
#define SOF_IPC4_FW_PAGE(x) ((((x) + BIT(12) - 1) & ~(BIT(12) - 1)) >> 12)
#define SOF_IPC4_FW_ROUNDUP(x) (((x) + BIT(6) - 1) & (~(BIT(6) - 1)))

#define SOF_IPC4_MODULE_LOAD_TYPE		GENMASK(3, 0)
#define SOF_IPC4_MODULE_AUTO_START		BIT(4)
 
#define SOF_IPC4_MODULE_LL		BIT(5)
#define SOF_IPC4_MODULE_DP		BIT(6)
#define SOF_IPC4_MODULE_LIB_CODE		BIT(7)
#define SOF_IPC4_MODULE_INIT_CONFIG_MASK	GENMASK(11, 8)

#define SOF_IPC4_MODULE_INIT_CONFIG_TYPE_BASE_CFG		0
#define SOF_IPC4_MODULE_INIT_CONFIG_TYPE_BASE_CFG_WITH_EXT	1

#define SOF_IPC4_MODULE_INSTANCE_LIST_ITEM_SIZE 12
#define SOF_IPC4_PIPELINE_OBJECT_SIZE 448
#define SOF_IPC4_DATA_QUEUE_OBJECT_SIZE 128
#define SOF_IPC4_LL_TASK_OBJECT_SIZE 72
#define SOF_IPC4_DP_TASK_OBJECT_SIZE 104
#define SOF_IPC4_DP_TASK_LIST_SIZE (12 + 8)
#define SOF_IPC4_LL_TASK_LIST_ITEM_SIZE 12
#define SOF_IPC4_FW_MAX_PAGE_COUNT 20
#define SOF_IPC4_FW_MAX_QUEUE_COUNT 8

 
#define SOF_IPC4_NODE_INDEX_MASK	0xFF
#define SOF_IPC4_NODE_INDEX(x)	((x) & SOF_IPC4_NODE_INDEX_MASK)
#define SOF_IPC4_NODE_TYPE(x)  ((x) << 8)

 
#define SOF_IPC4_NODE_INDEX_INTEL_SSP(x) (((x) & 0xf) << 4)

 
#define SOF_IPC4_NODE_INDEX_INTEL_DMIC(x) ((x) & 0x7)

#define SOF_IPC4_GAIN_ALL_CHANNELS_MASK 0xffffffff
#define SOF_IPC4_VOL_ZERO_DB	0x7fffffff

#define SOF_IPC4_DMA_DEVICE_MAX_COUNT 16

#define SOF_IPC4_INVALID_NODE_ID	0xffffffff

 
#define SOF_IPC4_MIN_DMA_BUFFER_SIZE	2

 
#define ALH_MULTI_GTW_BASE	0x50
 
#define ALH_MULTI_GTW_COUNT	8

enum sof_ipc4_copier_module_config_params {
 
	SOF_IPC4_COPIER_MODULE_CFG_PARAM_TIMESTAMP_INIT = 1,
 
	SOF_IPC4_COPIER_MODULE_CFG_PARAM_SET_SINK_FORMAT,
 
	SOF_IPC4_COPIER_MODULE_CFG_PARAM_DATA_SEGMENT_ENABLED,
 
	SOF_IPC4_COPIER_MODULE_CFG_PARAM_LLP_READING,
 
	SOF_IPC4_COPIER_MODULE_CFG_PARAM_LLP_READING_EXTENDED,
 
	SOF_IPC4_COPIER_MODULE_CFG_ATTENUATION,
};

struct sof_ipc4_copier_config_set_sink_format {
 
	u32 sink_id;
 
	struct sof_ipc4_audio_format source_fmt;
 
	struct sof_ipc4_audio_format sink_fmt;
} __packed __aligned(4);

 
struct sof_ipc4_pipeline {
	uint32_t priority;
	uint32_t lp_mode;
	uint32_t mem_usage;
	uint32_t core_id;
	int state;
	bool use_chain_dma;
	struct sof_ipc4_msg msg;
	bool skip_during_fe_trigger;
};

 
struct ipc4_pipeline_set_state_data {
	u32 count;
	DECLARE_FLEX_ARRAY(u32, pipeline_instance_ids);
} __packed;

 
struct sof_ipc4_pin_format {
	u32 pin_index;
	u32 buffer_size;
	struct sof_ipc4_audio_format audio_fmt;
};

 
struct sof_ipc4_available_audio_format {
	struct sof_ipc4_pin_format *output_pin_fmts;
	struct sof_ipc4_pin_format *input_pin_fmts;
	u32 num_input_formats;
	u32 num_output_formats;
};

 
struct sof_copier_gateway_cfg {
	uint32_t node_id;
	uint32_t dma_buffer_size;
	uint32_t config_length;
	uint32_t config_data[];
};

 
struct sof_ipc4_copier_data {
	struct sof_ipc4_base_module_cfg base_config;
	struct sof_ipc4_audio_format out_format;
	uint32_t copier_feature_mask;
	struct sof_copier_gateway_cfg gtw_cfg;
};

 
struct sof_ipc4_gtw_attributes {
	uint32_t lp_buffer_alloc : 1;
	uint32_t alloc_from_reg_file : 1;
	uint32_t rsvd : 30;
};

 
struct sof_ipc4_dma_device_stream_ch_map {
	uint32_t device;
	uint32_t channel_mask;
};

 
struct sof_ipc4_dma_stream_ch_map {
	uint32_t device_count;
	struct sof_ipc4_dma_device_stream_ch_map mapping[SOF_IPC4_DMA_DEVICE_MAX_COUNT];
} __packed;

#define SOF_IPC4_DMA_METHOD_HDA   1
#define SOF_IPC4_DMA_METHOD_GPDMA 2  

 
struct sof_ipc4_dma_config {
	uint8_t dma_method;
	uint8_t pre_allocated_by_host;
	uint16_t rsvd;
	uint32_t dma_channel_id;
	uint32_t stream_id;
	struct sof_ipc4_dma_stream_ch_map dma_stream_channel_map;
	uint32_t dma_priv_config_size;
	uint8_t dma_priv_config[];
} __packed;

#define SOF_IPC4_GTW_DMA_CONFIG_ID 0x1000

 
struct sof_ipc4_dma_config_tlv {
	uint32_t type;
	uint32_t length;
	struct sof_ipc4_dma_config dma_config;
} __packed;

 
struct sof_ipc4_alh_configuration_blob {
	struct sof_ipc4_gtw_attributes gw_attr;
	struct sof_ipc4_dma_stream_ch_map alh_cfg;
};

 
struct sof_ipc4_copier {
	struct sof_ipc4_copier_data data;
	u32 *copier_config;
	uint32_t ipc_config_size;
	void *ipc_config_data;
	struct sof_ipc4_available_audio_format available_fmt;
	u32 frame_fmt;
	struct sof_ipc4_msg msg;
	struct sof_ipc4_gtw_attributes *gtw_attr;
	u32 dai_type;
	int dai_index;
	struct sof_ipc4_dma_config_tlv dma_config_tlv;
};

 
struct sof_ipc4_ctrl_value_chan {
	u32 channel;
	u32 value;
};

 
struct sof_ipc4_control_data {
	struct sof_ipc4_msg msg;
	int index;

	union {
		DECLARE_FLEX_ARRAY(struct sof_ipc4_ctrl_value_chan, chanv);
		DECLARE_FLEX_ARRAY(struct sof_abi_hdr, data);
	};
};

 
struct sof_ipc4_gain_params {
	uint32_t channels;
	uint32_t init_val;
	uint32_t curve_type;
	uint32_t reserved;
	uint32_t curve_duration_l;
	uint32_t curve_duration_h;
} __packed __aligned(4);

 
struct sof_ipc4_gain_data {
	struct sof_ipc4_base_module_cfg base_config;
	struct sof_ipc4_gain_params params;
} __packed __aligned(4);

 
struct sof_ipc4_gain {
	struct sof_ipc4_gain_data data;
	struct sof_ipc4_available_audio_format available_fmt;
	struct sof_ipc4_msg msg;
};

 
struct sof_ipc4_mixer {
	struct sof_ipc4_base_module_cfg base_config;
	struct sof_ipc4_available_audio_format available_fmt;
	struct sof_ipc4_msg msg;
};

 
struct sof_ipc4_src_data {
	struct sof_ipc4_base_module_cfg base_config;
	uint32_t sink_rate;
} __packed __aligned(4);

 
struct sof_ipc4_src {
	struct sof_ipc4_src_data data;
	struct sof_ipc4_available_audio_format available_fmt;
	struct sof_ipc4_msg msg;
};

 
struct sof_ipc4_base_module_cfg_ext {
	u16 num_input_pin_fmts;
	u16 num_output_pin_fmts;
	u8 reserved[12];
	DECLARE_FLEX_ARRAY(struct sof_ipc4_pin_format, pin_formats);
} __packed;

 
struct sof_ipc4_process {
	struct sof_ipc4_base_module_cfg base_config;
	struct sof_ipc4_base_module_cfg_ext *base_config_ext;
	struct sof_ipc4_audio_format output_format;
	struct sof_ipc4_available_audio_format available_fmt;
	void *ipc_config_data;
	uint32_t ipc_config_size;
	struct sof_ipc4_msg msg;
	u32 base_config_ext_size;
	u32 init_config;
};

#endif
