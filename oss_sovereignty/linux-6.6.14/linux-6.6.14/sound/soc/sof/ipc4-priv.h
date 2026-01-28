#ifndef __SOUND_SOC_SOF_IPC4_PRIV_H
#define __SOUND_SOC_SOF_IPC4_PRIV_H
#include <linux/idr.h>
#include <sound/sof/ext_manifest4.h>
#include "sof-priv.h"
#define SOF_IPC4_INBOX_WINDOW_IDX	0
#define SOF_IPC4_OUTBOX_WINDOW_IDX	1
#define SOF_IPC4_DEBUG_WINDOW_IDX	2
enum sof_ipc4_mtrace_type {
	SOF_IPC4_MTRACE_NOT_AVAILABLE = 0,
	SOF_IPC4_MTRACE_INTEL_CAVS_1_5,
	SOF_IPC4_MTRACE_INTEL_CAVS_1_8,
	SOF_IPC4_MTRACE_INTEL_CAVS_2,
};
struct sof_ipc4_fw_module {
	struct sof_man4_module man4_module_entry;
	const struct sof_man4_module_config *fw_mod_cfg;
	struct ida m_ida;
	void *private;
};
struct sof_ipc4_fw_library {
	struct sof_firmware sof_fw;
	const char *name;
	u32 id;
	int num_modules;
	struct sof_ipc4_fw_module *modules;
};
struct sof_ipc4_fw_data {
	u32 manifest_fw_hdr_offset;
	struct xarray fw_lib_xa;
	void *nhlt;
	enum sof_ipc4_mtrace_type mtrace_type;
	u32 mtrace_log_bytes;
	int max_num_pipelines;
	u32 max_libs_count;
	int (*load_library)(struct snd_sof_dev *sdev,
			    struct sof_ipc4_fw_library *fw_lib, bool reload);
	struct mutex pipeline_state_mutex;  
};
struct sof_ipc4_timestamp_info {
	struct sof_ipc4_copier *host_copier;
	struct sof_ipc4_copier *dai_copier;
	u64 stream_start_offset;
	u32 llp_offset;
};
extern const struct sof_ipc_fw_loader_ops ipc4_loader_ops;
extern const struct sof_ipc_tplg_ops ipc4_tplg_ops;
extern const struct sof_ipc_tplg_control_ops tplg_ipc4_control_ops;
extern const struct sof_ipc_pcm_ops ipc4_pcm_ops;
extern const struct sof_ipc_fw_tracing_ops ipc4_mtrace_ops;
int sof_ipc4_set_pipeline_state(struct snd_sof_dev *sdev, u32 id, u32 state);
int sof_ipc4_mtrace_update_pos(struct snd_sof_dev *sdev, int core);
int sof_ipc4_query_fw_configuration(struct snd_sof_dev *sdev);
int sof_ipc4_reload_fw_libraries(struct snd_sof_dev *sdev);
struct sof_ipc4_fw_module *sof_ipc4_find_module_by_uuid(struct snd_sof_dev *sdev,
							const guid_t *uuid);
struct sof_ipc4_base_module_cfg;
void sof_ipc4_update_cpc_from_manifest(struct snd_sof_dev *sdev,
				       struct sof_ipc4_fw_module *fw_module,
				       struct sof_ipc4_base_module_cfg *basecfg);
#endif
