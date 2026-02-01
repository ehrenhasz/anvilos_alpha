
 

#include <linux/delay.h>

#include "iosm_ipc_chnl_cfg.h"
#include "iosm_ipc_devlink.h"
#include "iosm_ipc_flash.h"
#include "iosm_ipc_imem.h"
#include "iosm_ipc_port.h"
#include "iosm_ipc_trace.h"
#include "iosm_ipc_debugfs.h"

 
static int ipc_imem_check_wwan_ips(struct ipc_mem_channel *chnl)
{
	if (chnl)
		return chnl->ctype == IPC_CTYPE_WWAN &&
		       chnl->if_id == IPC_MEM_MUX_IP_CH_IF_ID;
	return false;
}

static int ipc_imem_msg_send_device_sleep(struct iosm_imem *ipc_imem, u32 state)
{
	union ipc_msg_prep_args prep_args = {
		.sleep.target = 1,
		.sleep.state = state,
	};

	ipc_imem->device_sleep = state;

	return ipc_protocol_tq_msg_send(ipc_imem->ipc_protocol,
					IPC_MSG_PREP_SLEEP, &prep_args, NULL);
}

static bool ipc_imem_dl_skb_alloc(struct iosm_imem *ipc_imem,
				  struct ipc_pipe *pipe)
{
	 
	if (pipe->nr_of_queued_entries >= pipe->max_nr_of_queued_entries)
		return false;

	return ipc_protocol_dl_td_prepare(ipc_imem->ipc_protocol, pipe);
}

 
static int ipc_imem_tq_td_alloc_timer(struct iosm_imem *ipc_imem, int arg,
				      void *msg, size_t size)
{
	bool new_buffers_available = false;
	bool retry_allocation = false;
	int i;

	for (i = 0; i < IPC_MEM_MAX_CHANNELS; i++) {
		struct ipc_pipe *pipe = &ipc_imem->channels[i].dl_pipe;

		if (!pipe->is_open || pipe->nr_of_queued_entries > 0)
			continue;

		while (ipc_imem_dl_skb_alloc(ipc_imem, pipe))
			new_buffers_available = true;

		if (pipe->nr_of_queued_entries == 0)
			retry_allocation = true;
	}

	if (new_buffers_available)
		ipc_protocol_doorbell_trigger(ipc_imem->ipc_protocol,
					      IPC_HP_DL_PROCESS);

	if (retry_allocation) {
		ipc_imem->hrtimer_period =
		ktime_set(0, IPC_TD_ALLOC_TIMER_PERIOD_MS * 1000 * 1000ULL);
		if (!hrtimer_active(&ipc_imem->td_alloc_timer))
			hrtimer_start(&ipc_imem->td_alloc_timer,
				      ipc_imem->hrtimer_period,
				      HRTIMER_MODE_REL);
	}
	return 0;
}

static enum hrtimer_restart ipc_imem_td_alloc_timer_cb(struct hrtimer *hr_timer)
{
	struct iosm_imem *ipc_imem =
		container_of(hr_timer, struct iosm_imem, td_alloc_timer);
	 
	ipc_task_queue_send_task(ipc_imem, ipc_imem_tq_td_alloc_timer, 0, NULL,
				 0, false);
	return HRTIMER_NORESTART;
}

 
static int ipc_imem_tq_fast_update_timer_cb(struct iosm_imem *ipc_imem, int arg,
					    void *msg, size_t size)
{
	ipc_protocol_doorbell_trigger(ipc_imem->ipc_protocol,
				      IPC_HP_FAST_TD_UPD_TMR);

	return 0;
}

static enum hrtimer_restart
ipc_imem_fast_update_timer_cb(struct hrtimer *hr_timer)
{
	struct iosm_imem *ipc_imem =
		container_of(hr_timer, struct iosm_imem, fast_update_timer);
	 
	ipc_task_queue_send_task(ipc_imem, ipc_imem_tq_fast_update_timer_cb, 0,
				 NULL, 0, false);
	return HRTIMER_NORESTART;
}

static int ipc_imem_tq_adb_timer_cb(struct iosm_imem *ipc_imem, int arg,
				    void *msg, size_t size)
{
	ipc_mux_ul_adb_finish(ipc_imem->mux);
	return 0;
}

static enum hrtimer_restart
ipc_imem_adb_timer_cb(struct hrtimer *hr_timer)
{
	struct iosm_imem *ipc_imem =
		container_of(hr_timer, struct iosm_imem, adb_timer);

	ipc_task_queue_send_task(ipc_imem, ipc_imem_tq_adb_timer_cb, 0,
				 NULL, 0, false);
	return HRTIMER_NORESTART;
}

static int ipc_imem_setup_cp_mux_cap_init(struct iosm_imem *ipc_imem,
					  struct ipc_mux_config *cfg)
{
	ipc_mmio_update_cp_capability(ipc_imem->mmio);

	if (ipc_imem->mmio->mux_protocol == MUX_UNKNOWN) {
		dev_err(ipc_imem->dev, "Failed to get Mux capability.");
		return -EINVAL;
	}

	cfg->protocol = ipc_imem->mmio->mux_protocol;

	cfg->ul_flow = (ipc_imem->mmio->has_ul_flow_credit == 1) ?
			       MUX_UL_ON_CREDITS :
			       MUX_UL;

	 
	cfg->instance_id = IPC_MEM_MUX_IP_CH_IF_ID;

	return 0;
}

void ipc_imem_msg_send_feature_set(struct iosm_imem *ipc_imem,
				   unsigned int reset_enable, bool atomic_ctx)
{
	union ipc_msg_prep_args prep_args = { .feature_set.reset_enable =
						      reset_enable };

	if (atomic_ctx)
		ipc_protocol_tq_msg_send(ipc_imem->ipc_protocol,
					 IPC_MSG_PREP_FEATURE_SET, &prep_args,
					 NULL);
	else
		ipc_protocol_msg_send(ipc_imem->ipc_protocol,
				      IPC_MSG_PREP_FEATURE_SET, &prep_args);
}

 
void ipc_imem_td_update_timer_start(struct iosm_imem *ipc_imem)
{
	 
	if (!ipc_imem->enter_runtime || ipc_imem->td_update_timer_suspended) {
		 
		ipc_protocol_doorbell_trigger(ipc_imem->ipc_protocol,
					      IPC_HP_TD_UPD_TMR_START);
		return;
	}

	if (!hrtimer_active(&ipc_imem->tdupdate_timer)) {
		ipc_imem->hrtimer_period =
		ktime_set(0, TD_UPDATE_DEFAULT_TIMEOUT_USEC * 1000ULL);
		if (!hrtimer_active(&ipc_imem->tdupdate_timer))
			hrtimer_start(&ipc_imem->tdupdate_timer,
				      ipc_imem->hrtimer_period,
				      HRTIMER_MODE_REL);
	}
}

void ipc_imem_hrtimer_stop(struct hrtimer *hr_timer)
{
	if (hrtimer_active(hr_timer))
		hrtimer_cancel(hr_timer);
}

 
void ipc_imem_adb_timer_start(struct iosm_imem *ipc_imem)
{
	if (!hrtimer_active(&ipc_imem->adb_timer)) {
		ipc_imem->hrtimer_period =
			ktime_set(0, IOSM_AGGR_MUX_ADB_FINISH_TIMEOUT_NSEC);
		hrtimer_start(&ipc_imem->adb_timer,
			      ipc_imem->hrtimer_period,
			      HRTIMER_MODE_REL);
	}
}

bool ipc_imem_ul_write_td(struct iosm_imem *ipc_imem)
{
	struct ipc_mem_channel *channel;
	bool hpda_ctrl_pending = false;
	struct sk_buff_head *ul_list;
	bool hpda_pending = false;
	struct ipc_pipe *pipe;
	int i;

	 
	for (i = 0; i < ipc_imem->nr_of_channels; i++) {
		channel = &ipc_imem->channels[i];

		if (channel->state != IMEM_CHANNEL_ACTIVE)
			continue;

		pipe = &channel->ul_pipe;

		 
		ul_list = &channel->ul_list;

		 
		if (!ipc_imem_check_wwan_ips(channel)) {
			hpda_ctrl_pending |=
				ipc_protocol_ul_td_send(ipc_imem->ipc_protocol,
							pipe, ul_list);
		} else {
			hpda_pending |=
				ipc_protocol_ul_td_send(ipc_imem->ipc_protocol,
							pipe, ul_list);
		}
	}

	 
	if (hpda_ctrl_pending) {
		hpda_pending = false;
		ipc_protocol_doorbell_trigger(ipc_imem->ipc_protocol,
					      IPC_HP_UL_WRITE_TD);
	}

	return hpda_pending;
}

void ipc_imem_ipc_init_check(struct iosm_imem *ipc_imem)
{
	int timeout = IPC_MODEM_BOOT_TIMEOUT;

	ipc_imem->ipc_requested_state = IPC_MEM_DEVICE_IPC_INIT;

	 
	ipc_doorbell_fire(ipc_imem->pcie, IPC_DOORBELL_IRQ_IPC,
			  IPC_MEM_DEVICE_IPC_INIT);
	 
	do {
		if (ipc_mmio_get_ipc_state(ipc_imem->mmio) ==
		    ipc_imem->ipc_requested_state) {
			 
			ipc_mmio_config(ipc_imem->mmio);

			 
			ipc_imem->ipc_requested_state =
				IPC_MEM_DEVICE_IPC_RUNNING;
			ipc_doorbell_fire(ipc_imem->pcie, IPC_DOORBELL_IRQ_IPC,
					  IPC_MEM_DEVICE_IPC_RUNNING);

			return;
		}
		msleep(20);
	} while (--timeout);

	 
	dev_err(ipc_imem->dev, "%s: ipc_status(%d) ne. IPC_MEM_DEVICE_IPC_INIT",
		ipc_imem_phase_get_string(ipc_imem->phase),
		ipc_mmio_get_ipc_state(ipc_imem->mmio));

	ipc_uevent_send(ipc_imem->dev, UEVENT_MDM_TIMEOUT);
}

 
static void ipc_imem_dl_skb_process(struct iosm_imem *ipc_imem,
				    struct ipc_pipe *pipe, struct sk_buff *skb)
{
	u16 port_id;

	if (!skb)
		return;

	 
	switch (pipe->channel->ctype) {
	case IPC_CTYPE_CTRL:
		port_id = pipe->channel->channel_id;
		ipc_pcie_addr_unmap(ipc_imem->pcie, IPC_CB(skb)->len,
				    IPC_CB(skb)->mapping,
				    IPC_CB(skb)->direction);
		if (port_id == IPC_MEM_CTRL_CHL_ID_7)
			ipc_imem_sys_devlink_notify_rx(ipc_imem->ipc_devlink,
						       skb);
		else if (ipc_is_trace_channel(ipc_imem, port_id))
			ipc_trace_port_rx(ipc_imem, skb);
		else
			wwan_port_rx(ipc_imem->ipc_port[port_id]->iosm_port,
				     skb);
		break;

	case IPC_CTYPE_WWAN:
		if (pipe->channel->if_id == IPC_MEM_MUX_IP_CH_IF_ID)
			ipc_mux_dl_decode(ipc_imem->mux, skb);
		break;
	default:
		dev_err(ipc_imem->dev, "Invalid channel type");
		break;
	}
}

 
static void ipc_imem_dl_pipe_process(struct iosm_imem *ipc_imem,
				     struct ipc_pipe *pipe)
{
	s32 cnt = 0, processed_td_cnt = 0;
	struct ipc_mem_channel *channel;
	u32 head = 0, tail = 0;
	bool processed = false;
	struct sk_buff *skb;

	channel = pipe->channel;

	ipc_protocol_get_head_tail_index(ipc_imem->ipc_protocol, pipe, &head,
					 &tail);
	if (pipe->old_tail != tail) {
		if (pipe->old_tail < tail)
			cnt = tail - pipe->old_tail;
		else
			cnt = pipe->nr_of_entries - pipe->old_tail + tail;
	}

	processed_td_cnt = cnt;

	 
	while (cnt--) {
		skb = ipc_protocol_dl_td_process(ipc_imem->ipc_protocol, pipe);

		 
		ipc_imem_dl_skb_process(ipc_imem, pipe, skb);
	}

	 
	while (ipc_imem_dl_skb_alloc(ipc_imem, pipe))
		processed = true;

	if (processed && !ipc_imem_check_wwan_ips(channel)) {
		 
		ipc_protocol_doorbell_trigger(ipc_imem->ipc_protocol,
					      IPC_HP_DL_PROCESS);
		processed = false;

		 
		ipc_imem_hrtimer_stop(&ipc_imem->fast_update_timer);
	}

	 
	if (processed && (processed_td_cnt == pipe->nr_of_entries - 1)) {
		ipc_imem->hrtimer_period =
		ktime_set(0, FORCE_UPDATE_DEFAULT_TIMEOUT_USEC * 1000ULL);
		hrtimer_start(&ipc_imem->fast_update_timer,
			      ipc_imem->hrtimer_period, HRTIMER_MODE_REL);
	}

	if (ipc_imem->app_notify_dl_pend)
		complete(&ipc_imem->dl_pend_sem);
}

 
static void ipc_imem_ul_pipe_process(struct iosm_imem *ipc_imem,
				     struct ipc_pipe *pipe)
{
	struct ipc_mem_channel *channel;
	u32 tail = 0, head = 0;
	struct sk_buff *skb;
	s32 cnt = 0;

	channel = pipe->channel;

	 
	ipc_protocol_get_head_tail_index(ipc_imem->ipc_protocol, pipe, &head,
					 &tail);

	if (pipe->old_tail != tail) {
		if (pipe->old_tail < tail)
			cnt = tail - pipe->old_tail;
		else
			cnt = pipe->nr_of_entries - pipe->old_tail + tail;
	}

	 
	while (cnt--) {
		skb = ipc_protocol_ul_td_process(ipc_imem->ipc_protocol, pipe);

		if (!skb)
			continue;

		 
		if (IPC_CB(skb)->op_type == UL_USR_OP_BLOCKED)
			complete(&channel->ul_sem);

		 
		if (IPC_CB(skb)->op_type == UL_MUX_OP_ADB) {
			if (channel->if_id == IPC_MEM_MUX_IP_CH_IF_ID)
				ipc_mux_ul_encoded_process(ipc_imem->mux, skb);
			else
				dev_err(ipc_imem->dev,
					"OP Type is UL_MUX, unknown if_id %d",
					channel->if_id);
		} else {
			ipc_pcie_kfree_skb(ipc_imem->pcie, skb);
		}
	}

	 
	if (ipc_imem_check_wwan_ips(pipe->channel))
		ipc_mux_check_n_restart_tx(ipc_imem->mux);

	if (ipc_imem->app_notify_ul_pend)
		complete(&ipc_imem->ul_pend_sem);
}

 
static void ipc_imem_rom_irq_exec(struct iosm_imem *ipc_imem)
{
	struct ipc_mem_channel *channel;

	channel = ipc_imem->ipc_devlink->devlink_sio.channel;
	ipc_imem->rom_exit_code = ipc_mmio_get_rom_exit_code(ipc_imem->mmio);
	complete(&channel->ul_sem);
}

 
static int ipc_imem_tq_td_update_timer_cb(struct iosm_imem *ipc_imem, int arg,
					  void *msg, size_t size)
{
	ipc_protocol_doorbell_trigger(ipc_imem->ipc_protocol,
				      IPC_HP_TD_UPD_TMR);
	return 0;
}

 
static void ipc_imem_slp_control_exec(struct iosm_imem *ipc_imem)
{
	     
	if (ipc_protocol_pm_dev_sleep_handle(ipc_imem->ipc_protocol) &&
	    hrtimer_active(&ipc_imem->tdupdate_timer)) {
		 
		ipc_imem_tq_td_update_timer_cb(ipc_imem, 0, NULL, 0);
		 
		ipc_imem_hrtimer_stop(&ipc_imem->tdupdate_timer);
		 
		ipc_imem_hrtimer_stop(&ipc_imem->fast_update_timer);
	}
}

 
static int ipc_imem_tq_startup_timer_cb(struct iosm_imem *ipc_imem, int arg,
					void *msg, size_t size)
{
	 
	if (ipc_imem_phase_update(ipc_imem) != IPC_P_RUN)
		return -EIO;

	if (ipc_mmio_get_ipc_state(ipc_imem->mmio) ==
	    IPC_MEM_DEVICE_IPC_UNINIT) {
		ipc_imem->ipc_requested_state = IPC_MEM_DEVICE_IPC_INIT;

		ipc_doorbell_fire(ipc_imem->pcie, IPC_DOORBELL_IRQ_IPC,
				  IPC_MEM_DEVICE_IPC_INIT);

		ipc_imem->hrtimer_period = ktime_set(0, 100 * 1000UL * 1000ULL);
		 
		if (!hrtimer_active(&ipc_imem->startup_timer))
			hrtimer_start(&ipc_imem->startup_timer,
				      ipc_imem->hrtimer_period,
				      HRTIMER_MODE_REL);
	} else if (ipc_mmio_get_ipc_state(ipc_imem->mmio) ==
		   IPC_MEM_DEVICE_IPC_INIT) {
		 
		ipc_imem_hrtimer_stop(&ipc_imem->startup_timer);

		 
		ipc_mmio_config(ipc_imem->mmio);
		ipc_imem->ipc_requested_state = IPC_MEM_DEVICE_IPC_RUNNING;
		ipc_doorbell_fire(ipc_imem->pcie, IPC_DOORBELL_IRQ_IPC,
				  IPC_MEM_DEVICE_IPC_RUNNING);
	}

	return 0;
}

static enum hrtimer_restart ipc_imem_startup_timer_cb(struct hrtimer *hr_timer)
{
	enum hrtimer_restart result = HRTIMER_NORESTART;
	struct iosm_imem *ipc_imem =
		container_of(hr_timer, struct iosm_imem, startup_timer);

	if (ktime_to_ns(ipc_imem->hrtimer_period)) {
		hrtimer_forward_now(&ipc_imem->startup_timer,
				    ipc_imem->hrtimer_period);
		result = HRTIMER_RESTART;
	}

	ipc_task_queue_send_task(ipc_imem, ipc_imem_tq_startup_timer_cb, 0,
				 NULL, 0, false);
	return result;
}

 
static enum ipc_mem_exec_stage
ipc_imem_get_exec_stage_buffered(struct iosm_imem *ipc_imem)
{
	return (ipc_imem->phase == IPC_P_RUN &&
		ipc_imem->ipc_status == IPC_MEM_DEVICE_IPC_RUNNING) ?
		       ipc_protocol_get_ap_exec_stage(ipc_imem->ipc_protocol) :
		       ipc_mmio_get_exec_stage(ipc_imem->mmio);
}

 
static int ipc_imem_send_mdm_rdy_cb(struct iosm_imem *ipc_imem, int arg,
				    void *msg, size_t size)
{
	enum ipc_mem_exec_stage exec_stage =
		ipc_imem_get_exec_stage_buffered(ipc_imem);

	if (exec_stage == IPC_MEM_EXEC_STAGE_RUN)
		ipc_uevent_send(ipc_imem->dev, UEVENT_MDM_READY);

	return 0;
}

 
static void ipc_imem_run_state_worker(struct work_struct *instance)
{
	struct ipc_chnl_cfg chnl_cfg_port = { 0 };
	struct ipc_mux_config mux_cfg;
	struct iosm_imem *ipc_imem;
	u8 ctrl_chl_idx = 0;
	int ret;

	ipc_imem = container_of(instance, struct iosm_imem, run_state_worker);

	if (ipc_imem->phase != IPC_P_RUN) {
		dev_err(ipc_imem->dev,
			"Modem link down. Exit run state worker.");
		goto err_out;
	}

	if (test_and_clear_bit(IOSM_DEVLINK_INIT, &ipc_imem->flag))
		ipc_devlink_deinit(ipc_imem->ipc_devlink);

	ret = ipc_imem_setup_cp_mux_cap_init(ipc_imem, &mux_cfg);
	if (ret < 0)
		goto err_out;

	ipc_imem->mux = ipc_mux_init(&mux_cfg, ipc_imem);
	if (!ipc_imem->mux)
		goto err_out;

	ret = ipc_imem_wwan_channel_init(ipc_imem, mux_cfg.protocol);
	if (ret < 0)
		goto err_ipc_mux_deinit;

	ipc_imem->mux->wwan = ipc_imem->wwan;

	while (ctrl_chl_idx < IPC_MEM_MAX_CHANNELS) {
		if (!ipc_chnl_cfg_get(&chnl_cfg_port, ctrl_chl_idx)) {
			ipc_imem->ipc_port[ctrl_chl_idx] = NULL;

			if (ipc_imem->pcie->pci->device == INTEL_CP_DEVICE_7560_ID &&
			    chnl_cfg_port.wwan_port_type == WWAN_PORT_XMMRPC) {
				ctrl_chl_idx++;
				continue;
			}

			if (ipc_imem->pcie->pci->device == INTEL_CP_DEVICE_7360_ID &&
			    chnl_cfg_port.wwan_port_type == WWAN_PORT_MBIM) {
				ctrl_chl_idx++;
				continue;
			}
			if (chnl_cfg_port.wwan_port_type != WWAN_PORT_UNKNOWN) {
				ipc_imem_channel_init(ipc_imem, IPC_CTYPE_CTRL,
						      chnl_cfg_port,
						      IRQ_MOD_OFF);
				ipc_imem->ipc_port[ctrl_chl_idx] =
					ipc_port_init(ipc_imem, chnl_cfg_port);
			}
		}
		ctrl_chl_idx++;
	}

	ipc_debugfs_init(ipc_imem);

	ipc_task_queue_send_task(ipc_imem, ipc_imem_send_mdm_rdy_cb, 0, NULL, 0,
				 false);

	 
	smp_mb__before_atomic();

	set_bit(FULLY_FUNCTIONAL, &ipc_imem->flag);

	 
	smp_mb__after_atomic();

	return;

err_ipc_mux_deinit:
	ipc_mux_deinit(ipc_imem->mux);
err_out:
	ipc_uevent_send(ipc_imem->dev, UEVENT_CD_READY_LINK_DOWN);
}

static void ipc_imem_handle_irq(struct iosm_imem *ipc_imem, int irq)
{
	enum ipc_mem_device_ipc_state curr_ipc_status;
	enum ipc_phase old_phase, phase;
	bool retry_allocation = false;
	bool ul_pending = false;
	int i;

	if (irq != IMEM_IRQ_DONT_CARE)
		ipc_imem->ev_irq_pending[irq] = false;

	 
	old_phase = ipc_imem->phase;

	if (old_phase == IPC_P_OFF_REQ) {
		dev_dbg(ipc_imem->dev,
			"[%s]: Ignoring MSI. Deinit sequence in progress!",
			ipc_imem_phase_get_string(old_phase));
		return;
	}

	 
	phase = ipc_imem_phase_update(ipc_imem);

	switch (phase) {
	case IPC_P_RUN:
		if (!ipc_imem->enter_runtime) {
			 
			ipc_imem->enter_runtime = 1;

			 
			ipc_imem_msg_send_device_sleep(ipc_imem,
						       ipc_imem->device_sleep);

			ipc_imem_msg_send_feature_set(ipc_imem,
						      IPC_MEM_INBAND_CRASH_SIG,
						  true);
		}

		curr_ipc_status =
			ipc_protocol_get_ipc_status(ipc_imem->ipc_protocol);

		 
		if (ipc_imem->ipc_status != curr_ipc_status) {
			ipc_imem->ipc_status = curr_ipc_status;

			if (ipc_imem->ipc_status ==
			    IPC_MEM_DEVICE_IPC_RUNNING) {
				schedule_work(&ipc_imem->run_state_worker);
			}
		}

		 
		ipc_imem_slp_control_exec(ipc_imem);
		break;  

		 
	case IPC_P_OFF:
	case IPC_P_OFF_REQ:
		dev_err(ipc_imem->dev, "confused phase %s",
			ipc_imem_phase_get_string(phase));
		return;

	case IPC_P_PSI:
		if (old_phase != IPC_P_ROM)
			break;

		fallthrough;
		 

	case IPC_P_ROM:
		 
		ipc_imem_rom_irq_exec(ipc_imem);
		return;

	default:
		break;
	}

	 
	ipc_protocol_msg_process(ipc_imem, irq);

	 
	for (i = 0; i < IPC_MEM_MAX_CHANNELS; i++) {
		struct ipc_pipe *ul_pipe = &ipc_imem->channels[i].ul_pipe;
		struct ipc_pipe *dl_pipe = &ipc_imem->channels[i].dl_pipe;

		if (dl_pipe->is_open &&
		    (irq == IMEM_IRQ_DONT_CARE || irq == dl_pipe->irq)) {
			ipc_imem_dl_pipe_process(ipc_imem, dl_pipe);

			if (dl_pipe->nr_of_queued_entries == 0)
				retry_allocation = true;
		}

		if (ul_pipe->is_open)
			ipc_imem_ul_pipe_process(ipc_imem, ul_pipe);
	}

	 
	if (ipc_mux_ul_data_encode(ipc_imem->mux)) {
		ipc_imem_td_update_timer_start(ipc_imem);
		if (ipc_imem->mux->protocol == MUX_AGGREGATION)
			ipc_imem_adb_timer_start(ipc_imem);
	}

	 
	ul_pending |= ipc_imem_ul_write_td(ipc_imem);

	 
	if (ul_pending) {
		ipc_imem->hrtimer_period =
		ktime_set(0, TD_UPDATE_DEFAULT_TIMEOUT_USEC * 1000ULL);
		if (!hrtimer_active(&ipc_imem->tdupdate_timer))
			hrtimer_start(&ipc_imem->tdupdate_timer,
				      ipc_imem->hrtimer_period,
				      HRTIMER_MODE_REL);
	}

	 
	if ((phase == IPC_P_PSI || phase == IPC_P_EBL) &&
	    ipc_imem->ipc_requested_state == IPC_MEM_DEVICE_IPC_RUNNING &&
	    ipc_mmio_get_ipc_state(ipc_imem->mmio) ==
						IPC_MEM_DEVICE_IPC_RUNNING) {
		complete(&ipc_imem->ipc_devlink->devlink_sio.channel->ul_sem);
	}

	 
	ipc_imem->ipc_requested_state = IPC_MEM_DEVICE_IPC_DONT_CARE;

	if (retry_allocation) {
		ipc_imem->hrtimer_period =
		ktime_set(0, IPC_TD_ALLOC_TIMER_PERIOD_MS * 1000 * 1000ULL);
		if (!hrtimer_active(&ipc_imem->td_alloc_timer))
			hrtimer_start(&ipc_imem->td_alloc_timer,
				      ipc_imem->hrtimer_period,
				      HRTIMER_MODE_REL);
	}
}

 
static int ipc_imem_tq_irq_cb(struct iosm_imem *ipc_imem, int arg, void *msg,
			      size_t size)
{
	ipc_imem_handle_irq(ipc_imem, arg);

	return 0;
}

void ipc_imem_ul_send(struct iosm_imem *ipc_imem)
{
	 
	if (ipc_imem_ul_write_td(ipc_imem))
		ipc_imem_td_update_timer_start(ipc_imem);
}

 
static enum ipc_phase ipc_imem_phase_update_check(struct iosm_imem *ipc_imem,
						  enum ipc_mem_exec_stage stage)
{
	switch (stage) {
	case IPC_MEM_EXEC_STAGE_BOOT:
		if (ipc_imem->phase != IPC_P_ROM) {
			 
			ipc_uevent_send(ipc_imem->dev, UEVENT_ROM_READY);
		}

		ipc_imem->phase = IPC_P_ROM;
		break;

	case IPC_MEM_EXEC_STAGE_PSI:
		ipc_imem->phase = IPC_P_PSI;
		break;

	case IPC_MEM_EXEC_STAGE_EBL:
		ipc_imem->phase = IPC_P_EBL;
		break;

	case IPC_MEM_EXEC_STAGE_RUN:
		if (ipc_imem->phase != IPC_P_RUN &&
		    ipc_imem->ipc_status == IPC_MEM_DEVICE_IPC_RUNNING) {
			ipc_uevent_send(ipc_imem->dev, UEVENT_MDM_READY);
		}
		ipc_imem->phase = IPC_P_RUN;
		break;

	case IPC_MEM_EXEC_STAGE_CRASH:
		if (ipc_imem->phase != IPC_P_CRASH)
			ipc_uevent_send(ipc_imem->dev, UEVENT_CRASH);

		ipc_imem->phase = IPC_P_CRASH;
		break;

	case IPC_MEM_EXEC_STAGE_CD_READY:
		if (ipc_imem->phase != IPC_P_CD_READY)
			ipc_uevent_send(ipc_imem->dev, UEVENT_CD_READY);
		ipc_imem->phase = IPC_P_CD_READY;
		break;

	default:
		 
		ipc_uevent_send(ipc_imem->dev, UEVENT_CD_READY_LINK_DOWN);
		break;
	}

	return ipc_imem->phase;
}

 
static bool ipc_imem_pipe_open(struct iosm_imem *ipc_imem,
			       struct ipc_pipe *pipe)
{
	union ipc_msg_prep_args prep_args = {
		.pipe_open.pipe = pipe,
	};

	if (ipc_protocol_msg_send(ipc_imem->ipc_protocol,
				  IPC_MSG_PREP_PIPE_OPEN, &prep_args) == 0)
		pipe->is_open = true;

	return pipe->is_open;
}

 
static int ipc_imem_tq_pipe_td_alloc(struct iosm_imem *ipc_imem, int arg,
				     void *msg, size_t size)
{
	struct ipc_pipe *dl_pipe = msg;
	bool processed = false;
	int i;

	for (i = 0; i < dl_pipe->nr_of_entries - 1; i++)
		processed |= ipc_imem_dl_skb_alloc(ipc_imem, dl_pipe);

	 
	if (processed)
		ipc_protocol_doorbell_trigger(ipc_imem->ipc_protocol, arg);

	return 0;
}

static enum hrtimer_restart
ipc_imem_td_update_timer_cb(struct hrtimer *hr_timer)
{
	struct iosm_imem *ipc_imem =
		container_of(hr_timer, struct iosm_imem, tdupdate_timer);

	ipc_task_queue_send_task(ipc_imem, ipc_imem_tq_td_update_timer_cb, 0,
				 NULL, 0, false);
	return HRTIMER_NORESTART;
}

 
enum ipc_phase ipc_imem_phase_update(struct iosm_imem *ipc_imem)
{
	enum ipc_mem_exec_stage exec_stage =
				ipc_imem_get_exec_stage_buffered(ipc_imem);
	 
	return ipc_imem->phase == IPC_P_OFF_REQ ?
		       ipc_imem->phase :
		       ipc_imem_phase_update_check(ipc_imem, exec_stage);
}

const char *ipc_imem_phase_get_string(enum ipc_phase phase)
{
	switch (phase) {
	case IPC_P_RUN:
		return "A-RUN";

	case IPC_P_OFF:
		return "A-OFF";

	case IPC_P_ROM:
		return "A-ROM";

	case IPC_P_PSI:
		return "A-PSI";

	case IPC_P_EBL:
		return "A-EBL";

	case IPC_P_CRASH:
		return "A-CRASH";

	case IPC_P_CD_READY:
		return "A-CD_READY";

	case IPC_P_OFF_REQ:
		return "A-OFF_REQ";

	default:
		return "A-???";
	}
}

void ipc_imem_pipe_close(struct iosm_imem *ipc_imem, struct ipc_pipe *pipe)
{
	union ipc_msg_prep_args prep_args = { .pipe_close.pipe = pipe };

	pipe->is_open = false;
	ipc_protocol_msg_send(ipc_imem->ipc_protocol, IPC_MSG_PREP_PIPE_CLOSE,
			      &prep_args);

	ipc_imem_pipe_cleanup(ipc_imem, pipe);
}

void ipc_imem_channel_close(struct iosm_imem *ipc_imem, int channel_id)
{
	struct ipc_mem_channel *channel;

	if (channel_id < 0 || channel_id >= ipc_imem->nr_of_channels) {
		dev_err(ipc_imem->dev, "invalid channel id %d", channel_id);
		return;
	}

	channel = &ipc_imem->channels[channel_id];

	if (channel->state == IMEM_CHANNEL_FREE) {
		dev_err(ipc_imem->dev, "ch[%d]: invalid channel state %d",
			channel_id, channel->state);
		return;
	}

	 
	if (channel->state == IMEM_CHANNEL_RESERVED)
		 
		goto channel_free;

	if (ipc_imem->phase == IPC_P_RUN) {
		ipc_imem_pipe_close(ipc_imem, &channel->ul_pipe);
		ipc_imem_pipe_close(ipc_imem, &channel->dl_pipe);
	}

	ipc_imem_pipe_cleanup(ipc_imem, &channel->ul_pipe);
	ipc_imem_pipe_cleanup(ipc_imem, &channel->dl_pipe);

channel_free:
	ipc_imem_channel_free(channel);
}

struct ipc_mem_channel *ipc_imem_channel_open(struct iosm_imem *ipc_imem,
					      int channel_id, u32 db_id)
{
	struct ipc_mem_channel *channel;

	if (channel_id < 0 || channel_id >= IPC_MEM_MAX_CHANNELS) {
		dev_err(ipc_imem->dev, "invalid channel ID: %d", channel_id);
		return NULL;
	}

	channel = &ipc_imem->channels[channel_id];

	channel->state = IMEM_CHANNEL_ACTIVE;

	if (!ipc_imem_pipe_open(ipc_imem, &channel->ul_pipe))
		goto ul_pipe_err;

	if (!ipc_imem_pipe_open(ipc_imem, &channel->dl_pipe))
		goto dl_pipe_err;

	 
	if (ipc_task_queue_send_task(ipc_imem, ipc_imem_tq_pipe_td_alloc, db_id,
				     &channel->dl_pipe, 0, false)) {
		dev_err(ipc_imem->dev, "td allocation failed : %d", channel_id);
		goto task_failed;
	}

	 
	return channel;
task_failed:
	ipc_imem_pipe_close(ipc_imem, &channel->dl_pipe);
dl_pipe_err:
	ipc_imem_pipe_close(ipc_imem, &channel->ul_pipe);
ul_pipe_err:
	ipc_imem_channel_free(channel);
	return NULL;
}

void ipc_imem_pm_suspend(struct iosm_imem *ipc_imem)
{
	ipc_protocol_suspend(ipc_imem->ipc_protocol);
}

void ipc_imem_pm_s2idle_sleep(struct iosm_imem *ipc_imem, bool sleep)
{
	ipc_protocol_s2idle_sleep(ipc_imem->ipc_protocol, sleep);
}

void ipc_imem_pm_resume(struct iosm_imem *ipc_imem)
{
	enum ipc_mem_exec_stage stage;

	if (ipc_protocol_resume(ipc_imem->ipc_protocol)) {
		stage = ipc_mmio_get_exec_stage(ipc_imem->mmio);
		ipc_imem_phase_update_check(ipc_imem, stage);
	}
}

void ipc_imem_channel_free(struct ipc_mem_channel *channel)
{
	 
	channel->state = IMEM_CHANNEL_FREE;
}

int ipc_imem_channel_alloc(struct iosm_imem *ipc_imem, int index,
			   enum ipc_ctype ctype)
{
	struct ipc_mem_channel *channel;
	int i;

	 
	for (i = 0; i < ipc_imem->nr_of_channels; i++) {
		channel = &ipc_imem->channels[i];
		if (channel->ctype == ctype && channel->index == index)
			break;
	}

	if (i >= ipc_imem->nr_of_channels) {
		dev_dbg(ipc_imem->dev,
			"no channel definition for index=%d ctype=%d", index,
			ctype);
		return -ECHRNG;
	}

	if (ipc_imem->channels[i].state != IMEM_CHANNEL_FREE) {
		dev_dbg(ipc_imem->dev, "channel is in use");
		return -EBUSY;
	}

	if (channel->ctype == IPC_CTYPE_WWAN &&
	    index == IPC_MEM_MUX_IP_CH_IF_ID)
		channel->if_id = index;

	channel->channel_id = index;
	channel->state = IMEM_CHANNEL_RESERVED;

	return i;
}

void ipc_imem_channel_init(struct iosm_imem *ipc_imem, enum ipc_ctype ctype,
			   struct ipc_chnl_cfg chnl_cfg, u32 irq_moderation)
{
	struct ipc_mem_channel *channel;

	if (chnl_cfg.ul_pipe >= IPC_MEM_MAX_PIPES ||
	    chnl_cfg.dl_pipe >= IPC_MEM_MAX_PIPES) {
		dev_err(ipc_imem->dev, "invalid pipe: ul_pipe=%d, dl_pipe=%d",
			chnl_cfg.ul_pipe, chnl_cfg.dl_pipe);
		return;
	}

	if (ipc_imem->nr_of_channels >= IPC_MEM_MAX_CHANNELS) {
		dev_err(ipc_imem->dev, "too many channels");
		return;
	}

	channel = &ipc_imem->channels[ipc_imem->nr_of_channels];
	channel->channel_id = ipc_imem->nr_of_channels;
	channel->ctype = ctype;
	channel->index = chnl_cfg.id;
	channel->net_err_count = 0;
	channel->state = IMEM_CHANNEL_FREE;
	ipc_imem->nr_of_channels++;

	ipc_imem_channel_update(ipc_imem, channel->channel_id, chnl_cfg,
				IRQ_MOD_OFF);

	skb_queue_head_init(&channel->ul_list);

	init_completion(&channel->ul_sem);
}

void ipc_imem_channel_update(struct iosm_imem *ipc_imem, int id,
			     struct ipc_chnl_cfg chnl_cfg, u32 irq_moderation)
{
	struct ipc_mem_channel *channel;

	if (id < 0 || id >= ipc_imem->nr_of_channels) {
		dev_err(ipc_imem->dev, "invalid channel id %d", id);
		return;
	}

	channel = &ipc_imem->channels[id];

	if (channel->state != IMEM_CHANNEL_FREE &&
	    channel->state != IMEM_CHANNEL_RESERVED) {
		dev_err(ipc_imem->dev, "invalid channel state %d",
			channel->state);
		return;
	}

	channel->ul_pipe.nr_of_entries = chnl_cfg.ul_nr_of_entries;
	channel->ul_pipe.pipe_nr = chnl_cfg.ul_pipe;
	channel->ul_pipe.is_open = false;
	channel->ul_pipe.irq = IPC_UL_PIPE_IRQ_VECTOR;
	channel->ul_pipe.channel = channel;
	channel->ul_pipe.dir = IPC_MEM_DIR_UL;
	channel->ul_pipe.accumulation_backoff = chnl_cfg.accumulation_backoff;
	channel->ul_pipe.irq_moderation = irq_moderation;
	channel->ul_pipe.buf_size = 0;

	channel->dl_pipe.nr_of_entries = chnl_cfg.dl_nr_of_entries;
	channel->dl_pipe.pipe_nr = chnl_cfg.dl_pipe;
	channel->dl_pipe.is_open = false;
	channel->dl_pipe.irq = IPC_DL_PIPE_IRQ_VECTOR;
	channel->dl_pipe.channel = channel;
	channel->dl_pipe.dir = IPC_MEM_DIR_DL;
	channel->dl_pipe.accumulation_backoff = chnl_cfg.accumulation_backoff;
	channel->dl_pipe.irq_moderation = irq_moderation;
	channel->dl_pipe.buf_size = chnl_cfg.dl_buf_size;
}

static void ipc_imem_channel_reset(struct iosm_imem *ipc_imem)
{
	int i;

	for (i = 0; i < ipc_imem->nr_of_channels; i++) {
		struct ipc_mem_channel *channel;

		channel = &ipc_imem->channels[i];

		ipc_imem_pipe_cleanup(ipc_imem, &channel->dl_pipe);
		ipc_imem_pipe_cleanup(ipc_imem, &channel->ul_pipe);

		ipc_imem_channel_free(channel);
	}
}

void ipc_imem_pipe_cleanup(struct iosm_imem *ipc_imem, struct ipc_pipe *pipe)
{
	struct sk_buff *skb;

	 
	pipe->is_open = false;

	 
	while ((skb = skb_dequeue(&pipe->channel->ul_list)))
		ipc_pcie_kfree_skb(ipc_imem->pcie, skb);

	ipc_protocol_pipe_cleanup(ipc_imem->ipc_protocol, pipe);
}

 
static void ipc_imem_device_ipc_uninit(struct iosm_imem *ipc_imem)
{
	int timeout = IPC_MODEM_UNINIT_TIMEOUT_MS;
	enum ipc_mem_device_ipc_state ipc_state;

	 
	if (ipc_pcie_check_data_link_active(ipc_imem->pcie)) {
		 
		ipc_doorbell_fire(ipc_imem->pcie, IPC_DOORBELL_IRQ_IPC,
				  IPC_MEM_DEVICE_IPC_UNINIT);
		ipc_state = ipc_mmio_get_ipc_state(ipc_imem->mmio);

		 
		while ((ipc_state <= IPC_MEM_DEVICE_IPC_DONT_CARE) &&
		       (ipc_state != IPC_MEM_DEVICE_IPC_UNINIT) &&
		       (timeout > 0)) {
			usleep_range(1000, 1250);
			timeout--;
			ipc_state = ipc_mmio_get_ipc_state(ipc_imem->mmio);
		}
	}
}

void ipc_imem_cleanup(struct iosm_imem *ipc_imem)
{
	ipc_imem->phase = IPC_P_OFF_REQ;

	 
	ipc_uevent_send(ipc_imem->dev, UEVENT_MDM_NOT_READY);

	hrtimer_cancel(&ipc_imem->td_alloc_timer);
	hrtimer_cancel(&ipc_imem->tdupdate_timer);
	hrtimer_cancel(&ipc_imem->fast_update_timer);
	hrtimer_cancel(&ipc_imem->startup_timer);

	 
	cancel_work_sync(&ipc_imem->run_state_worker);

	if (test_and_clear_bit(FULLY_FUNCTIONAL, &ipc_imem->flag)) {
		ipc_mux_deinit(ipc_imem->mux);
		ipc_debugfs_deinit(ipc_imem);
		ipc_wwan_deinit(ipc_imem->wwan);
		ipc_port_deinit(ipc_imem->ipc_port);
	}

	if (test_and_clear_bit(IOSM_DEVLINK_INIT, &ipc_imem->flag))
		ipc_devlink_deinit(ipc_imem->ipc_devlink);

	ipc_imem_device_ipc_uninit(ipc_imem);
	ipc_imem_channel_reset(ipc_imem);

	ipc_protocol_deinit(ipc_imem->ipc_protocol);
	ipc_task_deinit(ipc_imem->ipc_task);

	kfree(ipc_imem->ipc_task);
	kfree(ipc_imem->mmio);

	ipc_imem->phase = IPC_P_OFF;
}

 
static int ipc_imem_config(struct iosm_imem *ipc_imem)
{
	enum ipc_phase phase;

	 
	init_completion(&ipc_imem->ul_pend_sem);

	init_completion(&ipc_imem->dl_pend_sem);

	 
	ipc_imem->ipc_status = IPC_MEM_DEVICE_IPC_UNINIT;
	ipc_imem->enter_runtime = 0;

	phase = ipc_imem_phase_update(ipc_imem);

	 
	switch (phase) {
	case IPC_P_ROM:
		ipc_imem->hrtimer_period = ktime_set(0, 1000 * 1000 * 1000ULL);
		 
		if (!hrtimer_active(&ipc_imem->startup_timer))
			hrtimer_start(&ipc_imem->startup_timer,
				      ipc_imem->hrtimer_period,
				      HRTIMER_MODE_REL);
		return 0;

	case IPC_P_PSI:
	case IPC_P_EBL:
	case IPC_P_RUN:
		 
		ipc_imem->ipc_requested_state = IPC_MEM_DEVICE_IPC_UNINIT;

		 
		if (ipc_imem->ipc_requested_state ==
		    ipc_mmio_get_ipc_state(ipc_imem->mmio)) {
			ipc_imem_ipc_init_check(ipc_imem);

			return 0;
		}
		dev_err(ipc_imem->dev,
			"ipc_status(%d) != IPC_MEM_DEVICE_IPC_UNINIT",
			ipc_mmio_get_ipc_state(ipc_imem->mmio));
		break;
	case IPC_P_CRASH:
	case IPC_P_CD_READY:
		dev_dbg(ipc_imem->dev,
			"Modem is in phase %d, reset Modem to collect CD",
			phase);
		return 0;
	default:
		dev_err(ipc_imem->dev, "unexpected operation phase %d", phase);
		break;
	}

	complete(&ipc_imem->dl_pend_sem);
	complete(&ipc_imem->ul_pend_sem);
	ipc_imem->phase = IPC_P_OFF;
	return -EIO;
}

 
struct iosm_imem *ipc_imem_init(struct iosm_pcie *pcie, unsigned int device_id,
				void __iomem *mmio, struct device *dev)
{
	struct iosm_imem *ipc_imem = kzalloc(sizeof(*pcie->imem), GFP_KERNEL);
	enum ipc_mem_exec_stage stage;

	if (!ipc_imem)
		return NULL;

	 
	ipc_imem->pcie = pcie;
	ipc_imem->dev = dev;

	ipc_imem->pci_device_id = device_id;

	ipc_imem->cp_version = 0;
	ipc_imem->device_sleep = IPC_HOST_SLEEP_ENTER_SLEEP;

	 
	ipc_imem->nr_of_channels = 0;

	 
	ipc_imem->mmio = ipc_mmio_init(mmio, ipc_imem->dev);
	if (!ipc_imem->mmio) {
		dev_err(ipc_imem->dev, "failed to initialize mmio region");
		goto mmio_init_fail;
	}

	ipc_imem->ipc_task = kzalloc(sizeof(*ipc_imem->ipc_task),
				     GFP_KERNEL);

	 
	if (!ipc_imem->ipc_task)
		goto ipc_task_fail;

	if (ipc_task_init(ipc_imem->ipc_task))
		goto ipc_task_init_fail;

	ipc_imem->ipc_task->dev = ipc_imem->dev;

	INIT_WORK(&ipc_imem->run_state_worker, ipc_imem_run_state_worker);

	ipc_imem->ipc_protocol = ipc_protocol_init(ipc_imem);

	if (!ipc_imem->ipc_protocol)
		goto protocol_init_fail;

	 
	ipc_imem->phase = IPC_P_OFF;

	hrtimer_init(&ipc_imem->startup_timer, CLOCK_MONOTONIC,
		     HRTIMER_MODE_REL);
	ipc_imem->startup_timer.function = ipc_imem_startup_timer_cb;

	hrtimer_init(&ipc_imem->tdupdate_timer, CLOCK_MONOTONIC,
		     HRTIMER_MODE_REL);
	ipc_imem->tdupdate_timer.function = ipc_imem_td_update_timer_cb;

	hrtimer_init(&ipc_imem->fast_update_timer, CLOCK_MONOTONIC,
		     HRTIMER_MODE_REL);
	ipc_imem->fast_update_timer.function = ipc_imem_fast_update_timer_cb;

	hrtimer_init(&ipc_imem->td_alloc_timer, CLOCK_MONOTONIC,
		     HRTIMER_MODE_REL);
	ipc_imem->td_alloc_timer.function = ipc_imem_td_alloc_timer_cb;

	hrtimer_init(&ipc_imem->adb_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	ipc_imem->adb_timer.function = ipc_imem_adb_timer_cb;

	if (ipc_imem_config(ipc_imem)) {
		dev_err(ipc_imem->dev, "failed to initialize the imem");
		goto imem_config_fail;
	}

	stage = ipc_mmio_get_exec_stage(ipc_imem->mmio);
	if (stage == IPC_MEM_EXEC_STAGE_BOOT) {
		 
		ipc_imem->ipc_devlink = ipc_devlink_init(ipc_imem);
		if (!ipc_imem->ipc_devlink) {
			dev_err(ipc_imem->dev, "Devlink register failed");
			goto imem_config_fail;
		}

		if (ipc_flash_link_establish(ipc_imem))
			goto devlink_channel_fail;

		set_bit(IOSM_DEVLINK_INIT, &ipc_imem->flag);
	}
	return ipc_imem;
devlink_channel_fail:
	ipc_devlink_deinit(ipc_imem->ipc_devlink);
imem_config_fail:
	hrtimer_cancel(&ipc_imem->td_alloc_timer);
	hrtimer_cancel(&ipc_imem->fast_update_timer);
	hrtimer_cancel(&ipc_imem->tdupdate_timer);
	hrtimer_cancel(&ipc_imem->startup_timer);
protocol_init_fail:
	cancel_work_sync(&ipc_imem->run_state_worker);
	ipc_task_deinit(ipc_imem->ipc_task);
ipc_task_init_fail:
	kfree(ipc_imem->ipc_task);
ipc_task_fail:
	kfree(ipc_imem->mmio);
mmio_init_fail:
	kfree(ipc_imem);
	return NULL;
}

void ipc_imem_irq_process(struct iosm_imem *ipc_imem, int irq)
{
	 
	if (ipc_imem && !ipc_imem->ev_irq_pending[irq]) {
		ipc_imem->ev_irq_pending[irq] = true;
		ipc_task_queue_send_task(ipc_imem, ipc_imem_tq_irq_cb, irq,
					 NULL, 0, false);
	}
}

void ipc_imem_td_update_timer_suspend(struct iosm_imem *ipc_imem, bool suspend)
{
	ipc_imem->td_update_timer_suspended = suspend;
}

 
static int ipc_imem_devlink_trigger_chip_info_cb(struct iosm_imem *ipc_imem,
						 int arg, void *msg,
						 size_t msgsize)
{
	enum ipc_mem_exec_stage stage;
	struct sk_buff *skb;
	int rc = -EINVAL;
	size_t size;

	 
	stage = ipc_mmio_get_exec_stage(ipc_imem->mmio);
	if (stage != IPC_MEM_EXEC_STAGE_BOOT) {
		dev_err(ipc_imem->dev,
			"Execution_stage: expected BOOT, received = %X", stage);
		goto trigger_chip_info_fail;
	}
	 
	size = ipc_imem->mmio->chip_info_size;
	if (size > IOSM_CHIP_INFO_SIZE_MAX)
		goto trigger_chip_info_fail;

	skb = ipc_pcie_alloc_local_skb(ipc_imem->pcie, GFP_ATOMIC, size);
	if (!skb) {
		dev_err(ipc_imem->dev, "exhausted skbuf kernel DL memory");
		rc = -ENOMEM;
		goto trigger_chip_info_fail;
	}
	 
	ipc_mmio_copy_chip_info(ipc_imem->mmio, skb_put(skb, size), size);
	 
	dev_dbg(ipc_imem->dev, "execution_stage[%X] eq. BOOT", stage);
	ipc_imem->phase = ipc_imem_phase_update(ipc_imem);
	ipc_imem_sys_devlink_notify_rx(ipc_imem->ipc_devlink, skb);
	rc = 0;
trigger_chip_info_fail:
	return rc;
}

int ipc_imem_devlink_trigger_chip_info(struct iosm_imem *ipc_imem)
{
	return ipc_task_queue_send_task(ipc_imem,
					ipc_imem_devlink_trigger_chip_info_cb,
					0, NULL, 0, true);
}
