#ifndef __SOUND_SOC_SOF_IPC3_PRIV_H
#define __SOUND_SOC_SOF_IPC3_PRIV_H
#include "sof-priv.h"
extern const struct sof_ipc_pcm_ops ipc3_pcm_ops;
extern const struct sof_ipc_tplg_ops ipc3_tplg_ops;
extern const struct sof_ipc_tplg_control_ops tplg_ipc3_control_ops;
extern const struct sof_ipc_fw_loader_ops ipc3_loader_ops;
extern const struct sof_ipc_fw_tracing_ops ipc3_dtrace_ops;
int sof_ipc3_get_ext_windows(struct snd_sof_dev *sdev,
			     const struct sof_ipc_ext_data_hdr *ext_hdr);
int sof_ipc3_get_cc_info(struct snd_sof_dev *sdev,
			 const struct sof_ipc_ext_data_hdr *ext_hdr);
int sof_ipc3_validate_fw_version(struct snd_sof_dev *sdev);
int ipc3_dtrace_posn_update(struct snd_sof_dev *sdev,
			    struct sof_ipc_dma_trace_posn *posn);
void sof_ipc3_do_rx_work(struct snd_sof_dev *sdev, struct sof_ipc_cmd_hdr *hdr, void *msg_buf);
static inline int sof_dtrace_host_init(struct snd_sof_dev *sdev,
				       struct snd_dma_buffer *dmatb,
				       struct sof_ipc_dma_trace_params_ext *dtrace_params)
{
	struct snd_sof_dsp_ops *dsp_ops = sdev->pdata->desc->ops;
	if (dsp_ops->trace_init)
		return dsp_ops->trace_init(sdev, dmatb, dtrace_params);
	return 0;
}
static inline int sof_dtrace_host_release(struct snd_sof_dev *sdev)
{
	struct snd_sof_dsp_ops *dsp_ops = sdev->pdata->desc->ops;
	if (dsp_ops->trace_release)
		return dsp_ops->trace_release(sdev);
	return 0;
}
static inline int sof_dtrace_host_trigger(struct snd_sof_dev *sdev, int cmd)
{
	struct snd_sof_dsp_ops *dsp_ops = sdev->pdata->desc->ops;
	if (dsp_ops->trace_trigger)
		return dsp_ops->trace_trigger(sdev, cmd);
	return 0;
}
#endif
