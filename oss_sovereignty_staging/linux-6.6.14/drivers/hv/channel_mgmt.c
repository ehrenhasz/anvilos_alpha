
 
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/sched.h>
#include <linux/wait.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/completion.h>
#include <linux/delay.h>
#include <linux/cpu.h>
#include <linux/hyperv.h>
#include <asm/mshyperv.h>
#include <linux/sched/isolation.h>

#include "hyperv_vmbus.h"

static void init_vp_index(struct vmbus_channel *channel);

const struct vmbus_device vmbus_devs[] = {
	 
	{ .dev_type = HV_IDE,
	  HV_IDE_GUID,
	  .perf_device = true,
	  .allowed_in_isolated = false,
	},

	 
	{ .dev_type = HV_SCSI,
	  HV_SCSI_GUID,
	  .perf_device = true,
	  .allowed_in_isolated = true,
	},

	 
	{ .dev_type = HV_FC,
	  HV_SYNTHFC_GUID,
	  .perf_device = true,
	  .allowed_in_isolated = false,
	},

	 
	{ .dev_type = HV_NIC,
	  HV_NIC_GUID,
	  .perf_device = true,
	  .allowed_in_isolated = true,
	},

	 
	{ .dev_type = HV_ND,
	  HV_ND_GUID,
	  .perf_device = true,
	  .allowed_in_isolated = false,
	},

	 
	{ .dev_type = HV_PCIE,
	  HV_PCIE_GUID,
	  .perf_device = false,
	  .allowed_in_isolated = true,
	},

	 
	{ .dev_type = HV_FB,
	  HV_SYNTHVID_GUID,
	  .perf_device = false,
	  .allowed_in_isolated = false,
	},

	 
	{ .dev_type = HV_KBD,
	  HV_KBD_GUID,
	  .perf_device = false,
	  .allowed_in_isolated = false,
	},

	 
	{ .dev_type = HV_MOUSE,
	  HV_MOUSE_GUID,
	  .perf_device = false,
	  .allowed_in_isolated = false,
	},

	 
	{ .dev_type = HV_KVP,
	  HV_KVP_GUID,
	  .perf_device = false,
	  .allowed_in_isolated = false,
	},

	 
	{ .dev_type = HV_TS,
	  HV_TS_GUID,
	  .perf_device = false,
	  .allowed_in_isolated = true,
	},

	 
	{ .dev_type = HV_HB,
	  HV_HEART_BEAT_GUID,
	  .perf_device = false,
	  .allowed_in_isolated = true,
	},

	 
	{ .dev_type = HV_SHUTDOWN,
	  HV_SHUTDOWN_GUID,
	  .perf_device = false,
	  .allowed_in_isolated = true,
	},

	 
	{ .dev_type = HV_FCOPY,
	  HV_FCOPY_GUID,
	  .perf_device = false,
	  .allowed_in_isolated = false,
	},

	 
	{ .dev_type = HV_BACKUP,
	  HV_VSS_GUID,
	  .perf_device = false,
	  .allowed_in_isolated = false,
	},

	 
	{ .dev_type = HV_DM,
	  HV_DM_GUID,
	  .perf_device = false,
	  .allowed_in_isolated = false,
	},

	 
	{ .dev_type = HV_UNKNOWN,
	  .perf_device = false,
	  .allowed_in_isolated = false,
	},
};

static const struct {
	guid_t guid;
} vmbus_unsupported_devs[] = {
	{ HV_AVMA1_GUID },
	{ HV_AVMA2_GUID },
	{ HV_RDV_GUID	},
	{ HV_IMC_GUID	},
};

 
static void vmbus_rescind_cleanup(struct vmbus_channel *channel)
{
	struct vmbus_channel_msginfo *msginfo;
	unsigned long flags;


	spin_lock_irqsave(&vmbus_connection.channelmsg_lock, flags);
	channel->rescind = true;
	list_for_each_entry(msginfo, &vmbus_connection.chn_msg_list,
				msglistentry) {

		if (msginfo->waiting_channel == channel) {
			complete(&msginfo->waitevent);
			break;
		}
	}
	spin_unlock_irqrestore(&vmbus_connection.channelmsg_lock, flags);
}

static bool is_unsupported_vmbus_devs(const guid_t *guid)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(vmbus_unsupported_devs); i++)
		if (guid_equal(guid, &vmbus_unsupported_devs[i].guid))
			return true;
	return false;
}

static u16 hv_get_dev_type(const struct vmbus_channel *channel)
{
	const guid_t *guid = &channel->offermsg.offer.if_type;
	u16 i;

	if (is_hvsock_channel(channel) || is_unsupported_vmbus_devs(guid))
		return HV_UNKNOWN;

	for (i = HV_IDE; i < HV_UNKNOWN; i++) {
		if (guid_equal(guid, &vmbus_devs[i].guid))
			return i;
	}
	pr_info("Unknown GUID: %pUl\n", guid);
	return i;
}

 
bool vmbus_prep_negotiate_resp(struct icmsg_hdr *icmsghdrp, u8 *buf,
				u32 buflen, const int *fw_version, int fw_vercnt,
				const int *srv_version, int srv_vercnt,
				int *nego_fw_version, int *nego_srv_version)
{
	int icframe_major, icframe_minor;
	int icmsg_major, icmsg_minor;
	int fw_major, fw_minor;
	int srv_major, srv_minor;
	int i, j;
	bool found_match = false;
	struct icmsg_negotiate *negop;

	 
	if (buflen < ICMSG_HDR + offsetof(struct icmsg_negotiate, reserved)) {
		pr_err_ratelimited("Invalid icmsg negotiate\n");
		return false;
	}

	icmsghdrp->icmsgsize = 0x10;
	negop = (struct icmsg_negotiate *)&buf[ICMSG_HDR];

	icframe_major = negop->icframe_vercnt;
	icframe_minor = 0;

	icmsg_major = negop->icmsg_vercnt;
	icmsg_minor = 0;

	 
	if (icframe_major > IC_VERSION_NEGOTIATION_MAX_VER_COUNT ||
	    icmsg_major > IC_VERSION_NEGOTIATION_MAX_VER_COUNT ||
	    ICMSG_NEGOTIATE_PKT_SIZE(icframe_major, icmsg_major) > buflen) {
		pr_err_ratelimited("Invalid icmsg negotiate - icframe_major: %u, icmsg_major: %u\n",
				   icframe_major, icmsg_major);
		goto fw_error;
	}

	 

	for (i = 0; i < fw_vercnt; i++) {
		fw_major = (fw_version[i] >> 16);
		fw_minor = (fw_version[i] & 0xFFFF);

		for (j = 0; j < negop->icframe_vercnt; j++) {
			if ((negop->icversion_data[j].major == fw_major) &&
			    (negop->icversion_data[j].minor == fw_minor)) {
				icframe_major = negop->icversion_data[j].major;
				icframe_minor = negop->icversion_data[j].minor;
				found_match = true;
				break;
			}
		}

		if (found_match)
			break;
	}

	if (!found_match)
		goto fw_error;

	found_match = false;

	for (i = 0; i < srv_vercnt; i++) {
		srv_major = (srv_version[i] >> 16);
		srv_minor = (srv_version[i] & 0xFFFF);

		for (j = negop->icframe_vercnt;
			(j < negop->icframe_vercnt + negop->icmsg_vercnt);
			j++) {

			if ((negop->icversion_data[j].major == srv_major) &&
				(negop->icversion_data[j].minor == srv_minor)) {

				icmsg_major = negop->icversion_data[j].major;
				icmsg_minor = negop->icversion_data[j].minor;
				found_match = true;
				break;
			}
		}

		if (found_match)
			break;
	}

	 

fw_error:
	if (!found_match) {
		negop->icframe_vercnt = 0;
		negop->icmsg_vercnt = 0;
	} else {
		negop->icframe_vercnt = 1;
		negop->icmsg_vercnt = 1;
	}

	if (nego_fw_version)
		*nego_fw_version = (icframe_major << 16) | icframe_minor;

	if (nego_srv_version)
		*nego_srv_version = (icmsg_major << 16) | icmsg_minor;

	negop->icversion_data[0].major = icframe_major;
	negop->icversion_data[0].minor = icframe_minor;
	negop->icversion_data[1].major = icmsg_major;
	negop->icversion_data[1].minor = icmsg_minor;
	return found_match;
}
EXPORT_SYMBOL_GPL(vmbus_prep_negotiate_resp);

 
static struct vmbus_channel *alloc_channel(void)
{
	struct vmbus_channel *channel;

	channel = kzalloc(sizeof(*channel), GFP_ATOMIC);
	if (!channel)
		return NULL;

	spin_lock_init(&channel->sched_lock);
	init_completion(&channel->rescind_event);

	INIT_LIST_HEAD(&channel->sc_list);

	tasklet_init(&channel->callback_event,
		     vmbus_on_event, (unsigned long)channel);

	hv_ringbuffer_pre_init(channel);

	return channel;
}

 
static void free_channel(struct vmbus_channel *channel)
{
	tasklet_kill(&channel->callback_event);
	vmbus_remove_channel_attr_group(channel);

	kobject_put(&channel->kobj);
}

void vmbus_channel_map_relid(struct vmbus_channel *channel)
{
	if (WARN_ON(channel->offermsg.child_relid >= MAX_CHANNEL_RELIDS))
		return;
	 
	virt_store_mb(
		vmbus_connection.channels[channel->offermsg.child_relid],
		channel);
}

void vmbus_channel_unmap_relid(struct vmbus_channel *channel)
{
	if (WARN_ON(channel->offermsg.child_relid >= MAX_CHANNEL_RELIDS))
		return;
	WRITE_ONCE(
		vmbus_connection.channels[channel->offermsg.child_relid],
		NULL);
}

static void vmbus_release_relid(u32 relid)
{
	struct vmbus_channel_relid_released msg;
	int ret;

	memset(&msg, 0, sizeof(struct vmbus_channel_relid_released));
	msg.child_relid = relid;
	msg.header.msgtype = CHANNELMSG_RELID_RELEASED;
	ret = vmbus_post_msg(&msg, sizeof(struct vmbus_channel_relid_released),
			     true);

	trace_vmbus_release_relid(&msg, ret);
}

void hv_process_channel_removal(struct vmbus_channel *channel)
{
	lockdep_assert_held(&vmbus_connection.channel_mutex);
	BUG_ON(!channel->rescind);

	 
	WARN_ON(channel->offermsg.child_relid == INVALID_RELID &&
		!is_hvsock_channel(channel));

	 
	if (channel->offermsg.child_relid != INVALID_RELID)
		vmbus_channel_unmap_relid(channel);

	if (channel->primary_channel == NULL)
		list_del(&channel->listentry);
	else
		list_del(&channel->sc_list);

	 
	if (hv_is_perf_channel(channel))
		hv_clear_allocated_cpu(channel->target_cpu);

	 
	if (channel->offermsg.child_relid != INVALID_RELID)
		vmbus_release_relid(channel->offermsg.child_relid);

	free_channel(channel);
}

void vmbus_free_channels(void)
{
	struct vmbus_channel *channel, *tmp;

	list_for_each_entry_safe(channel, tmp, &vmbus_connection.chn_list,
		listentry) {
		 
		channel->rescind = true;

		vmbus_device_unregister(channel->device_obj);
	}
}

 
static void vmbus_add_channel_work(struct work_struct *work)
{
	struct vmbus_channel *newchannel =
		container_of(work, struct vmbus_channel, add_channel_work);
	struct vmbus_channel *primary_channel = newchannel->primary_channel;
	int ret;

	 
	newchannel->state = CHANNEL_OPEN_STATE;

	if (primary_channel != NULL) {
		 
		struct hv_device *dev = primary_channel->device_obj;

		if (vmbus_add_channel_kobj(dev, newchannel))
			goto err_deq_chan;

		if (primary_channel->sc_creation_callback != NULL)
			primary_channel->sc_creation_callback(newchannel);

		newchannel->probe_done = true;
		return;
	}

	 
	newchannel->device_obj = vmbus_device_create(
		&newchannel->offermsg.offer.if_type,
		&newchannel->offermsg.offer.if_instance,
		newchannel);
	if (!newchannel->device_obj)
		goto err_deq_chan;

	newchannel->device_obj->device_id = newchannel->device_id;
	 
	ret = vmbus_device_register(newchannel->device_obj);

	if (ret != 0) {
		pr_err("unable to add child device object (relid %d)\n",
			newchannel->offermsg.child_relid);
		goto err_deq_chan;
	}

	newchannel->probe_done = true;
	return;

err_deq_chan:
	mutex_lock(&vmbus_connection.channel_mutex);

	 
	newchannel->probe_done = true;

	if (primary_channel == NULL)
		list_del(&newchannel->listentry);
	else
		list_del(&newchannel->sc_list);

	 
	vmbus_channel_unmap_relid(newchannel);

	mutex_unlock(&vmbus_connection.channel_mutex);

	vmbus_release_relid(newchannel->offermsg.child_relid);

	free_channel(newchannel);
}

 
static void vmbus_process_offer(struct vmbus_channel *newchannel)
{
	struct vmbus_channel *channel;
	struct workqueue_struct *wq;
	bool fnew = true;

	 
	cpus_read_lock();

	 
	mutex_lock(&vmbus_connection.channel_mutex);

	list_for_each_entry(channel, &vmbus_connection.chn_list, listentry) {
		if (guid_equal(&channel->offermsg.offer.if_type,
			       &newchannel->offermsg.offer.if_type) &&
		    guid_equal(&channel->offermsg.offer.if_instance,
			       &newchannel->offermsg.offer.if_instance)) {
			fnew = false;
			newchannel->primary_channel = channel;
			break;
		}
	}

	init_vp_index(newchannel);

	 
	if (is_hvsock_channel(newchannel) || is_sub_channel(newchannel))
		atomic_inc(&vmbus_connection.nr_chan_close_on_suspend);

	 
	atomic_dec(&vmbus_connection.offer_in_progress);

	if (fnew) {
		list_add_tail(&newchannel->listentry,
			      &vmbus_connection.chn_list);
	} else {
		 
		if (newchannel->offermsg.offer.sub_channel_index == 0) {
			mutex_unlock(&vmbus_connection.channel_mutex);
			cpus_read_unlock();
			 
			kfree(newchannel);
			WARN_ON_ONCE(1);
			return;
		}
		 
		list_add_tail(&newchannel->sc_list, &channel->sc_list);
	}

	vmbus_channel_map_relid(newchannel);

	mutex_unlock(&vmbus_connection.channel_mutex);
	cpus_read_unlock();

	 
	INIT_WORK(&newchannel->add_channel_work, vmbus_add_channel_work);
	wq = fnew ? vmbus_connection.handle_primary_chan_wq :
		    vmbus_connection.handle_sub_chan_wq;
	queue_work(wq, &newchannel->add_channel_work);
}

 
static bool hv_cpuself_used(u32 cpu, struct vmbus_channel *chn)
{
	struct vmbus_channel *primary = chn->primary_channel;
	struct vmbus_channel *sc;

	lockdep_assert_held(&vmbus_connection.channel_mutex);

	if (!primary)
		return false;

	if (primary->target_cpu == cpu)
		return true;

	list_for_each_entry(sc, &primary->sc_list, sc_list)
		if (sc != chn && sc->target_cpu == cpu)
			return true;

	return false;
}

 
static int next_numa_node_id;

 
static void init_vp_index(struct vmbus_channel *channel)
{
	bool perf_chn = hv_is_perf_channel(channel);
	u32 i, ncpu = num_online_cpus();
	cpumask_var_t available_mask;
	struct cpumask *allocated_mask;
	const struct cpumask *hk_mask = housekeeping_cpumask(HK_TYPE_MANAGED_IRQ);
	u32 target_cpu;
	int numa_node;

	if (!perf_chn ||
	    !alloc_cpumask_var(&available_mask, GFP_KERNEL) ||
	    cpumask_empty(hk_mask)) {
		 
		channel->target_cpu = VMBUS_CONNECT_CPU;
		if (perf_chn)
			hv_set_allocated_cpu(VMBUS_CONNECT_CPU);
		return;
	}

	for (i = 1; i <= ncpu + 1; i++) {
		while (true) {
			numa_node = next_numa_node_id++;
			if (numa_node == nr_node_ids) {
				next_numa_node_id = 0;
				continue;
			}
			if (cpumask_empty(cpumask_of_node(numa_node)))
				continue;
			break;
		}
		allocated_mask = &hv_context.hv_numa_map[numa_node];

retry:
		cpumask_xor(available_mask, allocated_mask, cpumask_of_node(numa_node));
		cpumask_and(available_mask, available_mask, hk_mask);

		if (cpumask_empty(available_mask)) {
			 
			cpumask_clear(allocated_mask);
			goto retry;
		}

		target_cpu = cpumask_first(available_mask);
		cpumask_set_cpu(target_cpu, allocated_mask);

		if (channel->offermsg.offer.sub_channel_index >= ncpu ||
		    i > ncpu || !hv_cpuself_used(target_cpu, channel))
			break;
	}

	channel->target_cpu = target_cpu;

	free_cpumask_var(available_mask);
}

#define UNLOAD_DELAY_UNIT_MS	10		 
#define UNLOAD_WAIT_MS		(100*1000)	 
#define UNLOAD_WAIT_LOOPS	(UNLOAD_WAIT_MS/UNLOAD_DELAY_UNIT_MS)
#define UNLOAD_MSG_MS		(5*1000)	 
#define UNLOAD_MSG_LOOPS	(UNLOAD_MSG_MS/UNLOAD_DELAY_UNIT_MS)

static void vmbus_wait_for_unload(void)
{
	int cpu;
	void *page_addr;
	struct hv_message *msg;
	struct vmbus_channel_message_header *hdr;
	u32 message_type, i;

	 
	for (i = 1; i <= UNLOAD_WAIT_LOOPS; i++) {
		if (completion_done(&vmbus_connection.unload_event))
			goto completed;

		for_each_present_cpu(cpu) {
			struct hv_per_cpu_context *hv_cpu
				= per_cpu_ptr(hv_context.cpu_context, cpu);

			 
			page_addr = hv_cpu->synic_message_page;
			if (!page_addr)
				continue;

			msg = (struct hv_message *)page_addr
				+ VMBUS_MESSAGE_SINT;

			message_type = READ_ONCE(msg->header.message_type);
			if (message_type == HVMSG_NONE)
				continue;

			hdr = (struct vmbus_channel_message_header *)
				msg->u.payload;

			if (hdr->msgtype == CHANNELMSG_UNLOAD_RESPONSE)
				complete(&vmbus_connection.unload_event);

			vmbus_signal_eom(msg, message_type);
		}

		 
		if (!(i % UNLOAD_MSG_LOOPS))
			pr_notice("Waiting for VMBus UNLOAD to complete\n");

		mdelay(UNLOAD_DELAY_UNIT_MS);
	}
	pr_err("Continuing even though VMBus UNLOAD did not complete\n");

completed:
	 
	for_each_present_cpu(cpu) {
		struct hv_per_cpu_context *hv_cpu
			= per_cpu_ptr(hv_context.cpu_context, cpu);

		page_addr = hv_cpu->synic_message_page;
		if (!page_addr)
			continue;

		msg = (struct hv_message *)page_addr + VMBUS_MESSAGE_SINT;
		msg->header.message_type = HVMSG_NONE;
	}
}

 
static void vmbus_unload_response(struct vmbus_channel_message_header *hdr)
{
	 
	complete(&vmbus_connection.unload_event);
}

void vmbus_initiate_unload(bool crash)
{
	struct vmbus_channel_message_header hdr;

	if (xchg(&vmbus_connection.conn_state, DISCONNECTED) == DISCONNECTED)
		return;

	 
	if (vmbus_proto_version < VERSION_WIN8_1)
		return;

	reinit_completion(&vmbus_connection.unload_event);
	memset(&hdr, 0, sizeof(struct vmbus_channel_message_header));
	hdr.msgtype = CHANNELMSG_UNLOAD;
	vmbus_post_msg(&hdr, sizeof(struct vmbus_channel_message_header),
		       !crash);

	 
	if (!crash)
		wait_for_completion(&vmbus_connection.unload_event);
	else
		vmbus_wait_for_unload();
}

static void check_ready_for_resume_event(void)
{
	 
	if (atomic_dec_and_test(&vmbus_connection.nr_chan_fixup_on_resume))
		complete(&vmbus_connection.ready_for_resume_event);
}

static void vmbus_setup_channel_state(struct vmbus_channel *channel,
				      struct vmbus_channel_offer_channel *offer)
{
	 
	channel->sig_event = VMBUS_EVENT_CONNECTION_ID;

	channel->is_dedicated_interrupt =
			(offer->is_dedicated_interrupt != 0);
	channel->sig_event = offer->connection_id;

	memcpy(&channel->offermsg, offer,
	       sizeof(struct vmbus_channel_offer_channel));
	channel->monitor_grp = (u8)offer->monitorid / 32;
	channel->monitor_bit = (u8)offer->monitorid % 32;
	channel->device_id = hv_get_dev_type(channel);
}

 
static struct vmbus_channel *
find_primary_channel_by_offer(const struct vmbus_channel_offer_channel *offer)
{
	struct vmbus_channel *channel = NULL, *iter;
	const guid_t *inst1, *inst2;

	 
	if (offer->offer.sub_channel_index != 0)
		return NULL;

	mutex_lock(&vmbus_connection.channel_mutex);

	list_for_each_entry(iter, &vmbus_connection.chn_list, listentry) {
		inst1 = &iter->offermsg.offer.if_instance;
		inst2 = &offer->offer.if_instance;

		if (guid_equal(inst1, inst2)) {
			channel = iter;
			break;
		}
	}

	mutex_unlock(&vmbus_connection.channel_mutex);

	return channel;
}

static bool vmbus_is_valid_offer(const struct vmbus_channel_offer_channel *offer)
{
	const guid_t *guid = &offer->offer.if_type;
	u16 i;

	if (!hv_is_isolation_supported())
		return true;

	if (is_hvsock_offer(offer))
		return true;

	for (i = 0; i < ARRAY_SIZE(vmbus_devs); i++) {
		if (guid_equal(guid, &vmbus_devs[i].guid))
			return vmbus_devs[i].allowed_in_isolated;
	}
	return false;
}

 
static void vmbus_onoffer(struct vmbus_channel_message_header *hdr)
{
	struct vmbus_channel_offer_channel *offer;
	struct vmbus_channel *oldchannel, *newchannel;
	size_t offer_sz;

	offer = (struct vmbus_channel_offer_channel *)hdr;

	trace_vmbus_onoffer(offer);

	if (!vmbus_is_valid_offer(offer)) {
		pr_err_ratelimited("Invalid offer %d from the host supporting isolation\n",
				   offer->child_relid);
		atomic_dec(&vmbus_connection.offer_in_progress);
		return;
	}

	oldchannel = find_primary_channel_by_offer(offer);

	if (oldchannel != NULL) {
		 

		 
		mutex_lock(&vmbus_connection.channel_mutex);

		atomic_dec(&vmbus_connection.offer_in_progress);

		WARN_ON(oldchannel->offermsg.child_relid != INVALID_RELID);
		 
		oldchannel->offermsg.child_relid = offer->child_relid;

		offer_sz = sizeof(*offer);
		if (memcmp(offer, &oldchannel->offermsg, offer_sz) != 0) {
			 
			pr_debug("vmbus offer changed: relid=%d\n",
				 offer->child_relid);

			print_hex_dump_debug("Old vmbus offer: ",
					     DUMP_PREFIX_OFFSET, 16, 4,
					     &oldchannel->offermsg, offer_sz,
					     false);
			print_hex_dump_debug("New vmbus offer: ",
					     DUMP_PREFIX_OFFSET, 16, 4,
					     offer, offer_sz, false);

			 
			vmbus_setup_channel_state(oldchannel, offer);
		}

		 
		vmbus_channel_map_relid(oldchannel);
		check_ready_for_resume_event();

		mutex_unlock(&vmbus_connection.channel_mutex);
		return;
	}

	 
	newchannel = alloc_channel();
	if (!newchannel) {
		vmbus_release_relid(offer->child_relid);
		atomic_dec(&vmbus_connection.offer_in_progress);
		pr_err("Unable to allocate channel object\n");
		return;
	}

	vmbus_setup_channel_state(newchannel, offer);

	vmbus_process_offer(newchannel);
}

static void check_ready_for_suspend_event(void)
{
	 
	if (atomic_dec_and_test(&vmbus_connection.nr_chan_close_on_suspend))
		complete(&vmbus_connection.ready_for_suspend_event);
}

 
static void vmbus_onoffer_rescind(struct vmbus_channel_message_header *hdr)
{
	struct vmbus_channel_rescind_offer *rescind;
	struct vmbus_channel *channel;
	struct device *dev;
	bool clean_up_chan_for_suspend;

	rescind = (struct vmbus_channel_rescind_offer *)hdr;

	trace_vmbus_onoffer_rescind(rescind);

	 

	while (atomic_read(&vmbus_connection.offer_in_progress) != 0) {
		 
		msleep(1);
	}

	mutex_lock(&vmbus_connection.channel_mutex);
	channel = relid2channel(rescind->child_relid);
	if (channel != NULL) {
		 
		if (channel->rescind_ref) {
			mutex_unlock(&vmbus_connection.channel_mutex);
			return;
		}
		channel->rescind_ref = true;
	}
	mutex_unlock(&vmbus_connection.channel_mutex);

	if (channel == NULL) {
		 
		return;
	}

	clean_up_chan_for_suspend = is_hvsock_channel(channel) ||
				    is_sub_channel(channel);
	 
	vmbus_reset_channel_cb(channel);

	 
	vmbus_rescind_cleanup(channel);
	while (READ_ONCE(channel->probe_done) == false) {
		 
		msleep(1);
	}

	 

	if (channel->device_obj) {
		if (channel->chn_rescind_callback) {
			channel->chn_rescind_callback(channel);

			if (clean_up_chan_for_suspend)
				check_ready_for_suspend_event();

			return;
		}
		 
		dev = get_device(&channel->device_obj->device);
		if (dev) {
			vmbus_device_unregister(channel->device_obj);
			put_device(dev);
		}
	} else if (channel->primary_channel != NULL) {
		 
		mutex_lock(&vmbus_connection.channel_mutex);
		if (channel->state == CHANNEL_OPEN_STATE) {
			 
			hv_process_channel_removal(channel);
		} else {
			complete(&channel->rescind_event);
		}
		mutex_unlock(&vmbus_connection.channel_mutex);
	}

	 

	if (clean_up_chan_for_suspend)
		check_ready_for_suspend_event();
}

void vmbus_hvsock_device_unregister(struct vmbus_channel *channel)
{
	BUG_ON(!is_hvsock_channel(channel));

	 
	while (!READ_ONCE(channel->probe_done) || !READ_ONCE(channel->rescind))
		msleep(1);

	vmbus_device_unregister(channel->device_obj);
}
EXPORT_SYMBOL_GPL(vmbus_hvsock_device_unregister);


 
static void vmbus_onoffers_delivered(
			struct vmbus_channel_message_header *hdr)
{
}

 
static void vmbus_onopen_result(struct vmbus_channel_message_header *hdr)
{
	struct vmbus_channel_open_result *result;
	struct vmbus_channel_msginfo *msginfo;
	struct vmbus_channel_message_header *requestheader;
	struct vmbus_channel_open_channel *openmsg;
	unsigned long flags;

	result = (struct vmbus_channel_open_result *)hdr;

	trace_vmbus_onopen_result(result);

	 
	spin_lock_irqsave(&vmbus_connection.channelmsg_lock, flags);

	list_for_each_entry(msginfo, &vmbus_connection.chn_msg_list,
				msglistentry) {
		requestheader =
			(struct vmbus_channel_message_header *)msginfo->msg;

		if (requestheader->msgtype == CHANNELMSG_OPENCHANNEL) {
			openmsg =
			(struct vmbus_channel_open_channel *)msginfo->msg;
			if (openmsg->child_relid == result->child_relid &&
			    openmsg->openid == result->openid) {
				memcpy(&msginfo->response.open_result,
				       result,
				       sizeof(
					struct vmbus_channel_open_result));
				complete(&msginfo->waitevent);
				break;
			}
		}
	}
	spin_unlock_irqrestore(&vmbus_connection.channelmsg_lock, flags);
}

 
static void vmbus_ongpadl_created(struct vmbus_channel_message_header *hdr)
{
	struct vmbus_channel_gpadl_created *gpadlcreated;
	struct vmbus_channel_msginfo *msginfo;
	struct vmbus_channel_message_header *requestheader;
	struct vmbus_channel_gpadl_header *gpadlheader;
	unsigned long flags;

	gpadlcreated = (struct vmbus_channel_gpadl_created *)hdr;

	trace_vmbus_ongpadl_created(gpadlcreated);

	 
	spin_lock_irqsave(&vmbus_connection.channelmsg_lock, flags);

	list_for_each_entry(msginfo, &vmbus_connection.chn_msg_list,
				msglistentry) {
		requestheader =
			(struct vmbus_channel_message_header *)msginfo->msg;

		if (requestheader->msgtype == CHANNELMSG_GPADL_HEADER) {
			gpadlheader =
			(struct vmbus_channel_gpadl_header *)requestheader;

			if ((gpadlcreated->child_relid ==
			     gpadlheader->child_relid) &&
			    (gpadlcreated->gpadl == gpadlheader->gpadl)) {
				memcpy(&msginfo->response.gpadl_created,
				       gpadlcreated,
				       sizeof(
					struct vmbus_channel_gpadl_created));
				complete(&msginfo->waitevent);
				break;
			}
		}
	}
	spin_unlock_irqrestore(&vmbus_connection.channelmsg_lock, flags);
}

 
static void vmbus_onmodifychannel_response(struct vmbus_channel_message_header *hdr)
{
	struct vmbus_channel_modifychannel_response *response;
	struct vmbus_channel_msginfo *msginfo;
	unsigned long flags;

	response = (struct vmbus_channel_modifychannel_response *)hdr;

	trace_vmbus_onmodifychannel_response(response);

	 
	spin_lock_irqsave(&vmbus_connection.channelmsg_lock, flags);

	list_for_each_entry(msginfo, &vmbus_connection.chn_msg_list, msglistentry) {
		struct vmbus_channel_message_header *responseheader =
				(struct vmbus_channel_message_header *)msginfo->msg;

		if (responseheader->msgtype == CHANNELMSG_MODIFYCHANNEL) {
			struct vmbus_channel_modifychannel *modifymsg;

			modifymsg = (struct vmbus_channel_modifychannel *)msginfo->msg;
			if (modifymsg->child_relid == response->child_relid) {
				memcpy(&msginfo->response.modify_response, response,
				       sizeof(*response));
				complete(&msginfo->waitevent);
				break;
			}
		}
	}
	spin_unlock_irqrestore(&vmbus_connection.channelmsg_lock, flags);
}

 
static void vmbus_ongpadl_torndown(
			struct vmbus_channel_message_header *hdr)
{
	struct vmbus_channel_gpadl_torndown *gpadl_torndown;
	struct vmbus_channel_msginfo *msginfo;
	struct vmbus_channel_message_header *requestheader;
	struct vmbus_channel_gpadl_teardown *gpadl_teardown;
	unsigned long flags;

	gpadl_torndown = (struct vmbus_channel_gpadl_torndown *)hdr;

	trace_vmbus_ongpadl_torndown(gpadl_torndown);

	 
	spin_lock_irqsave(&vmbus_connection.channelmsg_lock, flags);

	list_for_each_entry(msginfo, &vmbus_connection.chn_msg_list,
				msglistentry) {
		requestheader =
			(struct vmbus_channel_message_header *)msginfo->msg;

		if (requestheader->msgtype == CHANNELMSG_GPADL_TEARDOWN) {
			gpadl_teardown =
			(struct vmbus_channel_gpadl_teardown *)requestheader;

			if (gpadl_torndown->gpadl == gpadl_teardown->gpadl) {
				memcpy(&msginfo->response.gpadl_torndown,
				       gpadl_torndown,
				       sizeof(
					struct vmbus_channel_gpadl_torndown));
				complete(&msginfo->waitevent);
				break;
			}
		}
	}
	spin_unlock_irqrestore(&vmbus_connection.channelmsg_lock, flags);
}

 
static void vmbus_onversion_response(
		struct vmbus_channel_message_header *hdr)
{
	struct vmbus_channel_msginfo *msginfo;
	struct vmbus_channel_message_header *requestheader;
	struct vmbus_channel_version_response *version_response;
	unsigned long flags;

	version_response = (struct vmbus_channel_version_response *)hdr;

	trace_vmbus_onversion_response(version_response);

	spin_lock_irqsave(&vmbus_connection.channelmsg_lock, flags);

	list_for_each_entry(msginfo, &vmbus_connection.chn_msg_list,
				msglistentry) {
		requestheader =
			(struct vmbus_channel_message_header *)msginfo->msg;

		if (requestheader->msgtype ==
		    CHANNELMSG_INITIATE_CONTACT) {
			memcpy(&msginfo->response.version_response,
			      version_response,
			      sizeof(struct vmbus_channel_version_response));
			complete(&msginfo->waitevent);
		}
	}
	spin_unlock_irqrestore(&vmbus_connection.channelmsg_lock, flags);
}

 
const struct vmbus_channel_message_table_entry
channel_message_table[CHANNELMSG_COUNT] = {
	{ CHANNELMSG_INVALID,			0, NULL, 0},
	{ CHANNELMSG_OFFERCHANNEL,		0, vmbus_onoffer,
		sizeof(struct vmbus_channel_offer_channel)},
	{ CHANNELMSG_RESCIND_CHANNELOFFER,	0, vmbus_onoffer_rescind,
		sizeof(struct vmbus_channel_rescind_offer) },
	{ CHANNELMSG_REQUESTOFFERS,		0, NULL, 0},
	{ CHANNELMSG_ALLOFFERS_DELIVERED,	1, vmbus_onoffers_delivered, 0},
	{ CHANNELMSG_OPENCHANNEL,		0, NULL, 0},
	{ CHANNELMSG_OPENCHANNEL_RESULT,	1, vmbus_onopen_result,
		sizeof(struct vmbus_channel_open_result)},
	{ CHANNELMSG_CLOSECHANNEL,		0, NULL, 0},
	{ CHANNELMSG_GPADL_HEADER,		0, NULL, 0},
	{ CHANNELMSG_GPADL_BODY,		0, NULL, 0},
	{ CHANNELMSG_GPADL_CREATED,		1, vmbus_ongpadl_created,
		sizeof(struct vmbus_channel_gpadl_created)},
	{ CHANNELMSG_GPADL_TEARDOWN,		0, NULL, 0},
	{ CHANNELMSG_GPADL_TORNDOWN,		1, vmbus_ongpadl_torndown,
		sizeof(struct vmbus_channel_gpadl_torndown) },
	{ CHANNELMSG_RELID_RELEASED,		0, NULL, 0},
	{ CHANNELMSG_INITIATE_CONTACT,		0, NULL, 0},
	{ CHANNELMSG_VERSION_RESPONSE,		1, vmbus_onversion_response,
		sizeof(struct vmbus_channel_version_response)},
	{ CHANNELMSG_UNLOAD,			0, NULL, 0},
	{ CHANNELMSG_UNLOAD_RESPONSE,		1, vmbus_unload_response, 0},
	{ CHANNELMSG_18,			0, NULL, 0},
	{ CHANNELMSG_19,			0, NULL, 0},
	{ CHANNELMSG_20,			0, NULL, 0},
	{ CHANNELMSG_TL_CONNECT_REQUEST,	0, NULL, 0},
	{ CHANNELMSG_MODIFYCHANNEL,		0, NULL, 0},
	{ CHANNELMSG_TL_CONNECT_RESULT,		0, NULL, 0},
	{ CHANNELMSG_MODIFYCHANNEL_RESPONSE,	1, vmbus_onmodifychannel_response,
		sizeof(struct vmbus_channel_modifychannel_response)},
};

 
void vmbus_onmessage(struct vmbus_channel_message_header *hdr)
{
	trace_vmbus_on_message(hdr);

	 
	channel_message_table[hdr->msgtype].message_handler(hdr);
}

 
int vmbus_request_offers(void)
{
	struct vmbus_channel_message_header *msg;
	struct vmbus_channel_msginfo *msginfo;
	int ret;

	msginfo = kzalloc(sizeof(*msginfo) +
			  sizeof(struct vmbus_channel_message_header),
			  GFP_KERNEL);
	if (!msginfo)
		return -ENOMEM;

	msg = (struct vmbus_channel_message_header *)msginfo->msg;

	msg->msgtype = CHANNELMSG_REQUESTOFFERS;

	ret = vmbus_post_msg(msg, sizeof(struct vmbus_channel_message_header),
			     true);

	trace_vmbus_request_offers(ret);

	if (ret != 0) {
		pr_err("Unable to request offers - %d\n", ret);

		goto cleanup;
	}

cleanup:
	kfree(msginfo);

	return ret;
}

void vmbus_set_sc_create_callback(struct vmbus_channel *primary_channel,
				void (*sc_cr_cb)(struct vmbus_channel *new_sc))
{
	primary_channel->sc_creation_callback = sc_cr_cb;
}
EXPORT_SYMBOL_GPL(vmbus_set_sc_create_callback);

void vmbus_set_chn_rescind_callback(struct vmbus_channel *channel,
		void (*chn_rescind_cb)(struct vmbus_channel *))
{
	channel->chn_rescind_callback = chn_rescind_cb;
}
EXPORT_SYMBOL_GPL(vmbus_set_chn_rescind_callback);
