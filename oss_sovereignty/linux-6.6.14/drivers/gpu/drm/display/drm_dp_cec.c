
 

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>

#include <media/cec.h>

#include <drm/display/drm_dp_helper.h>
#include <drm/drm_connector.h>
#include <drm/drm_device.h>

 

 

 
#define NEVER_UNREG_DELAY 1000
static unsigned int drm_dp_cec_unregister_delay = 1;
module_param(drm_dp_cec_unregister_delay, uint, 0600);
MODULE_PARM_DESC(drm_dp_cec_unregister_delay,
		 "CEC unregister delay in seconds, 0: no delay, >= 1000: never unregister");

static int drm_dp_cec_adap_enable(struct cec_adapter *adap, bool enable)
{
	struct drm_dp_aux *aux = cec_get_drvdata(adap);
	u32 val = enable ? DP_CEC_TUNNELING_ENABLE : 0;
	ssize_t err = 0;

	err = drm_dp_dpcd_writeb(aux, DP_CEC_TUNNELING_CONTROL, val);
	return (enable && err < 0) ? err : 0;
}

static int drm_dp_cec_adap_log_addr(struct cec_adapter *adap, u8 addr)
{
	struct drm_dp_aux *aux = cec_get_drvdata(adap);
	 
	u16 la_mask = 1 << CEC_LOG_ADDR_BROADCAST;
	u8 mask[2];
	ssize_t err;

	if (addr != CEC_LOG_ADDR_INVALID)
		la_mask |= adap->log_addrs.log_addr_mask | (1 << addr);
	mask[0] = la_mask & 0xff;
	mask[1] = la_mask >> 8;
	err = drm_dp_dpcd_write(aux, DP_CEC_LOGICAL_ADDRESS_MASK, mask, 2);
	return (addr != CEC_LOG_ADDR_INVALID && err < 0) ? err : 0;
}

static int drm_dp_cec_adap_transmit(struct cec_adapter *adap, u8 attempts,
				    u32 signal_free_time, struct cec_msg *msg)
{
	struct drm_dp_aux *aux = cec_get_drvdata(adap);
	unsigned int retries = min(5, attempts - 1);
	ssize_t err;

	err = drm_dp_dpcd_write(aux, DP_CEC_TX_MESSAGE_BUFFER,
				msg->msg, msg->len);
	if (err < 0)
		return err;

	err = drm_dp_dpcd_writeb(aux, DP_CEC_TX_MESSAGE_INFO,
				 (msg->len - 1) | (retries << 4) |
				 DP_CEC_TX_MESSAGE_SEND);
	return err < 0 ? err : 0;
}

static int drm_dp_cec_adap_monitor_all_enable(struct cec_adapter *adap,
					      bool enable)
{
	struct drm_dp_aux *aux = cec_get_drvdata(adap);
	ssize_t err;
	u8 val;

	if (!(adap->capabilities & CEC_CAP_MONITOR_ALL))
		return 0;

	err = drm_dp_dpcd_readb(aux, DP_CEC_TUNNELING_CONTROL, &val);
	if (err >= 0) {
		if (enable)
			val |= DP_CEC_SNOOPING_ENABLE;
		else
			val &= ~DP_CEC_SNOOPING_ENABLE;
		err = drm_dp_dpcd_writeb(aux, DP_CEC_TUNNELING_CONTROL, val);
	}
	return (enable && err < 0) ? err : 0;
}

static void drm_dp_cec_adap_status(struct cec_adapter *adap,
				   struct seq_file *file)
{
	struct drm_dp_aux *aux = cec_get_drvdata(adap);
	struct drm_dp_desc desc;
	struct drm_dp_dpcd_ident *id = &desc.ident;

	if (drm_dp_read_desc(aux, &desc, true))
		return;
	seq_printf(file, "OUI: %*phD\n",
		   (int)sizeof(id->oui), id->oui);
	seq_printf(file, "ID: %*pE\n",
		   (int)strnlen(id->device_id, sizeof(id->device_id)),
		   id->device_id);
	seq_printf(file, "HW Rev: %d.%d\n", id->hw_rev >> 4, id->hw_rev & 0xf);
	 
	seq_printf(file, "FW/SW Rev: %d.%d (0x%02x.0x%02x)\n",
		   id->sw_major_rev, id->sw_minor_rev,
		   id->sw_major_rev, id->sw_minor_rev);
}

static const struct cec_adap_ops drm_dp_cec_adap_ops = {
	.adap_enable = drm_dp_cec_adap_enable,
	.adap_log_addr = drm_dp_cec_adap_log_addr,
	.adap_transmit = drm_dp_cec_adap_transmit,
	.adap_monitor_all_enable = drm_dp_cec_adap_monitor_all_enable,
	.adap_status = drm_dp_cec_adap_status,
};

static int drm_dp_cec_received(struct drm_dp_aux *aux)
{
	struct cec_adapter *adap = aux->cec.adap;
	struct cec_msg msg;
	u8 rx_msg_info;
	ssize_t err;

	err = drm_dp_dpcd_readb(aux, DP_CEC_RX_MESSAGE_INFO, &rx_msg_info);
	if (err < 0)
		return err;

	if (!(rx_msg_info & DP_CEC_RX_MESSAGE_ENDED))
		return 0;

	msg.len = (rx_msg_info & DP_CEC_RX_MESSAGE_LEN_MASK) + 1;
	err = drm_dp_dpcd_read(aux, DP_CEC_RX_MESSAGE_BUFFER, msg.msg, msg.len);
	if (err < 0)
		return err;

	cec_received_msg(adap, &msg);
	return 0;
}

static void drm_dp_cec_handle_irq(struct drm_dp_aux *aux)
{
	struct cec_adapter *adap = aux->cec.adap;
	u8 flags;

	if (drm_dp_dpcd_readb(aux, DP_CEC_TUNNELING_IRQ_FLAGS, &flags) < 0)
		return;

	if (flags & DP_CEC_RX_MESSAGE_INFO_VALID)
		drm_dp_cec_received(aux);

	if (flags & DP_CEC_TX_MESSAGE_SENT)
		cec_transmit_attempt_done(adap, CEC_TX_STATUS_OK);
	else if (flags & DP_CEC_TX_LINE_ERROR)
		cec_transmit_attempt_done(adap, CEC_TX_STATUS_ERROR |
						CEC_TX_STATUS_MAX_RETRIES);
	else if (flags &
		 (DP_CEC_TX_ADDRESS_NACK_ERROR | DP_CEC_TX_DATA_NACK_ERROR))
		cec_transmit_attempt_done(adap, CEC_TX_STATUS_NACK |
						CEC_TX_STATUS_MAX_RETRIES);
	drm_dp_dpcd_writeb(aux, DP_CEC_TUNNELING_IRQ_FLAGS, flags);
}

 
void drm_dp_cec_irq(struct drm_dp_aux *aux)
{
	u8 cec_irq;
	int ret;

	 
	if (!aux->transfer)
		return;

	mutex_lock(&aux->cec.lock);
	if (!aux->cec.adap)
		goto unlock;

	ret = drm_dp_dpcd_readb(aux, DP_DEVICE_SERVICE_IRQ_VECTOR_ESI1,
				&cec_irq);
	if (ret < 0 || !(cec_irq & DP_CEC_IRQ))
		goto unlock;

	drm_dp_cec_handle_irq(aux);
	drm_dp_dpcd_writeb(aux, DP_DEVICE_SERVICE_IRQ_VECTOR_ESI1, DP_CEC_IRQ);
unlock:
	mutex_unlock(&aux->cec.lock);
}
EXPORT_SYMBOL(drm_dp_cec_irq);

static bool drm_dp_cec_cap(struct drm_dp_aux *aux, u8 *cec_cap)
{
	u8 cap = 0;

	if (drm_dp_dpcd_readb(aux, DP_CEC_TUNNELING_CAPABILITY, &cap) != 1 ||
	    !(cap & DP_CEC_TUNNELING_CAPABLE))
		return false;
	if (cec_cap)
		*cec_cap = cap;
	return true;
}

 
static void drm_dp_cec_unregister_work(struct work_struct *work)
{
	struct drm_dp_aux *aux = container_of(work, struct drm_dp_aux,
					      cec.unregister_work.work);

	mutex_lock(&aux->cec.lock);
	cec_unregister_adapter(aux->cec.adap);
	aux->cec.adap = NULL;
	mutex_unlock(&aux->cec.lock);
}

 
void drm_dp_cec_set_edid(struct drm_dp_aux *aux, const struct edid *edid)
{
	struct drm_connector *connector = aux->cec.connector;
	u32 cec_caps = CEC_CAP_DEFAULTS | CEC_CAP_NEEDS_HPD |
		       CEC_CAP_CONNECTOR_INFO;
	struct cec_connector_info conn_info;
	unsigned int num_las = 1;
	u8 cap;

	 
	if (!aux->transfer)
		return;

#ifndef CONFIG_MEDIA_CEC_RC
	 
	cec_caps &= ~CEC_CAP_RC;
#endif
	cancel_delayed_work_sync(&aux->cec.unregister_work);

	mutex_lock(&aux->cec.lock);
	if (!drm_dp_cec_cap(aux, &cap)) {
		 
		cec_unregister_adapter(aux->cec.adap);
		aux->cec.adap = NULL;
		goto unlock;
	}

	if (cap & DP_CEC_SNOOPING_CAPABLE)
		cec_caps |= CEC_CAP_MONITOR_ALL;
	if (cap & DP_CEC_MULTIPLE_LA_CAPABLE)
		num_las = CEC_MAX_LOG_ADDRS;

	if (aux->cec.adap) {
		if (aux->cec.adap->capabilities == cec_caps &&
		    aux->cec.adap->available_log_addrs == num_las) {
			 
			cec_s_phys_addr_from_edid(aux->cec.adap, edid);
			goto unlock;
		}
		 
		cec_unregister_adapter(aux->cec.adap);
	}

	 
	aux->cec.adap = cec_allocate_adapter(&drm_dp_cec_adap_ops,
					     aux, connector->name, cec_caps,
					     num_las);
	if (IS_ERR(aux->cec.adap)) {
		aux->cec.adap = NULL;
		goto unlock;
	}

	cec_fill_conn_info_from_drm(&conn_info, connector);
	cec_s_conn_info(aux->cec.adap, &conn_info);

	if (cec_register_adapter(aux->cec.adap, connector->dev->dev)) {
		cec_delete_adapter(aux->cec.adap);
		aux->cec.adap = NULL;
	} else {
		 
		cec_s_phys_addr_from_edid(aux->cec.adap, edid);
	}
unlock:
	mutex_unlock(&aux->cec.lock);
}
EXPORT_SYMBOL(drm_dp_cec_set_edid);

 
void drm_dp_cec_unset_edid(struct drm_dp_aux *aux)
{
	 
	if (!aux->transfer)
		return;

	cancel_delayed_work_sync(&aux->cec.unregister_work);

	mutex_lock(&aux->cec.lock);
	if (!aux->cec.adap)
		goto unlock;

	cec_phys_addr_invalidate(aux->cec.adap);
	 
	if (drm_dp_cec_unregister_delay < NEVER_UNREG_DELAY &&
	    !drm_dp_cec_cap(aux, NULL)) {
		 
		schedule_delayed_work(&aux->cec.unregister_work,
				      drm_dp_cec_unregister_delay * HZ);
	}
unlock:
	mutex_unlock(&aux->cec.lock);
}
EXPORT_SYMBOL(drm_dp_cec_unset_edid);

 
void drm_dp_cec_register_connector(struct drm_dp_aux *aux,
				   struct drm_connector *connector)
{
	WARN_ON(aux->cec.adap);
	if (WARN_ON(!aux->transfer))
		return;
	aux->cec.connector = connector;
	INIT_DELAYED_WORK(&aux->cec.unregister_work,
			  drm_dp_cec_unregister_work);
}
EXPORT_SYMBOL(drm_dp_cec_register_connector);

 
void drm_dp_cec_unregister_connector(struct drm_dp_aux *aux)
{
	if (!aux->cec.adap)
		return;
	cancel_delayed_work_sync(&aux->cec.unregister_work);
	cec_unregister_adapter(aux->cec.adap);
	aux->cec.adap = NULL;
}
EXPORT_SYMBOL(drm_dp_cec_unregister_connector);
