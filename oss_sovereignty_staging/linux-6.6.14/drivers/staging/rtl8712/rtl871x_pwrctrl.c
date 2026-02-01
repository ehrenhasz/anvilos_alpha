
 

#define _RTL871X_PWRCTRL_C_

#include "osdep_service.h"
#include "drv_types.h"
#include "osdep_intf.h"

#define RTL8712_SDIO_LOCAL_BASE 0X10100000
#define SDIO_HCPWM (RTL8712_SDIO_LOCAL_BASE + 0x0081)

void r8712_set_rpwm(struct _adapter *padapter, u8 val8)
{
	u8	rpwm;
	struct pwrctrl_priv *pwrpriv = &padapter->pwrctrlpriv;

	if (pwrpriv->rpwm == val8) {
		if (pwrpriv->rpwm_retry == 0)
			return;
	}
	if (padapter->driver_stopped || padapter->surprise_removed)
		return;
	rpwm = val8 | pwrpriv->tog;
	switch (val8) {
	case PS_STATE_S1:
		pwrpriv->cpwm = val8;
		break;
	case PS_STATE_S2: 
	case PS_STATE_S3:
	case PS_STATE_S4:
		pwrpriv->cpwm = val8;
		break;
	default:
		break;
	}
	pwrpriv->rpwm_retry = 0;
	pwrpriv->rpwm = val8;
	r8712_write8(padapter, 0x1025FE58, rpwm);
	pwrpriv->tog += 0x80;
}

void r8712_set_ps_mode(struct _adapter *padapter, uint ps_mode, uint smart_ps)
{
	struct pwrctrl_priv *pwrpriv = &padapter->pwrctrlpriv;

	if (ps_mode > PM_Card_Disable)
		return;
	 
	if (ps_mode == PS_MODE_ACTIVE)
		smart_ps = 0;
	if ((pwrpriv->pwr_mode != ps_mode) || (pwrpriv->smart_ps != smart_ps)) {
		if (pwrpriv->pwr_mode == PS_MODE_ACTIVE)
			pwrpriv->bSleep = true;
		else
			pwrpriv->bSleep = false;
		pwrpriv->pwr_mode = ps_mode;
		pwrpriv->smart_ps = smart_ps;
		schedule_work(&pwrpriv->SetPSModeWorkItem);
	}
}

 
void r8712_cpwm_int_hdl(struct _adapter *padapter,
			struct reportpwrstate_parm *preportpwrstate)
{
	struct pwrctrl_priv *pwrpriv = &(padapter->pwrctrlpriv);
	struct cmd_priv	*pcmdpriv = &(padapter->cmdpriv);

	if (pwrpriv->cpwm_tog == ((preportpwrstate->state) & 0x80))
		return;
	del_timer(&padapter->pwrctrlpriv.rpwm_check_timer);
	mutex_lock(&pwrpriv->mutex_lock);
	pwrpriv->cpwm = (preportpwrstate->state) & 0xf;
	if (pwrpriv->cpwm >= PS_STATE_S2) {
		if (pwrpriv->alives & CMD_ALIVE)
			complete(&(pcmdpriv->cmd_queue_comp));
	}
	pwrpriv->cpwm_tog = (preportpwrstate->state) & 0x80;
	mutex_unlock(&pwrpriv->mutex_lock);
}

static inline void register_task_alive(struct pwrctrl_priv *pwrctrl, uint tag)
{
		pwrctrl->alives |= tag;
}

static inline void unregister_task_alive(struct pwrctrl_priv *pwrctrl, uint tag)
{
	if (pwrctrl->alives & tag)
		pwrctrl->alives ^= tag;
}

static void _rpwm_check_handler (struct _adapter *padapter)
{
	struct pwrctrl_priv *pwrpriv = &padapter->pwrctrlpriv;

	if (padapter->driver_stopped || padapter->surprise_removed)
		return;
	if (pwrpriv->cpwm != pwrpriv->rpwm)
		schedule_work(&pwrpriv->rpwm_workitem);
}

static void SetPSModeWorkItemCallback(struct work_struct *work)
{
	struct pwrctrl_priv *pwrpriv = container_of(work,
				       struct pwrctrl_priv, SetPSModeWorkItem);
	struct _adapter *padapter = container_of(pwrpriv,
				    struct _adapter, pwrctrlpriv);
	if (!pwrpriv->bSleep) {
		mutex_lock(&pwrpriv->mutex_lock);
		if (pwrpriv->pwr_mode == PS_MODE_ACTIVE)
			r8712_set_rpwm(padapter, PS_STATE_S4);
		mutex_unlock(&pwrpriv->mutex_lock);
	}
}

static void rpwm_workitem_callback(struct work_struct *work)
{
	struct pwrctrl_priv *pwrpriv = container_of(work,
				       struct pwrctrl_priv, rpwm_workitem);
	struct _adapter *padapter = container_of(pwrpriv,
				    struct _adapter, pwrctrlpriv);
	if (pwrpriv->cpwm != pwrpriv->rpwm) {
		mutex_lock(&pwrpriv->mutex_lock);
		r8712_read8(padapter, SDIO_HCPWM);
		pwrpriv->rpwm_retry = 1;
		r8712_set_rpwm(padapter, pwrpriv->rpwm);
		mutex_unlock(&pwrpriv->mutex_lock);
	}
}

static void rpwm_check_handler (struct timer_list *t)
{
	struct _adapter *adapter =
		from_timer(adapter, t, pwrctrlpriv.rpwm_check_timer);

	_rpwm_check_handler(adapter);
}

void r8712_init_pwrctrl_priv(struct _adapter *padapter)
{
	struct pwrctrl_priv *pwrctrlpriv = &padapter->pwrctrlpriv;

	memset((unsigned char *)pwrctrlpriv, 0, sizeof(struct pwrctrl_priv));
	mutex_init(&pwrctrlpriv->mutex_lock);
	pwrctrlpriv->cpwm = PS_STATE_S4;
	pwrctrlpriv->pwr_mode = PS_MODE_ACTIVE;
	pwrctrlpriv->smart_ps = 0;
	pwrctrlpriv->tog = 0x80;
 
	r8712_write8(padapter, 0x1025FE58, 0);
	INIT_WORK(&pwrctrlpriv->SetPSModeWorkItem, SetPSModeWorkItemCallback);
	INIT_WORK(&pwrctrlpriv->rpwm_workitem, rpwm_workitem_callback);
	timer_setup(&pwrctrlpriv->rpwm_check_timer, rpwm_check_handler, 0);
}

 
int r8712_register_cmd_alive(struct _adapter *padapter)
{
	int res = 0;
	struct pwrctrl_priv *pwrctrl = &padapter->pwrctrlpriv;

	mutex_lock(&pwrctrl->mutex_lock);
	register_task_alive(pwrctrl, CMD_ALIVE);
	if (pwrctrl->cpwm < PS_STATE_S2) {
		r8712_set_rpwm(padapter, PS_STATE_S3);
		res = -EINVAL;
	}
	mutex_unlock(&pwrctrl->mutex_lock);
	return res;
}

 
void r8712_unregister_cmd_alive(struct _adapter *padapter)
{
	struct pwrctrl_priv *pwrctrl = &padapter->pwrctrlpriv;

	mutex_lock(&pwrctrl->mutex_lock);
	unregister_task_alive(pwrctrl, CMD_ALIVE);
	if ((pwrctrl->cpwm > PS_STATE_S2) &&
	   (pwrctrl->pwr_mode > PS_MODE_ACTIVE)) {
		if ((pwrctrl->alives == 0) &&
		    (check_fwstate(&padapter->mlmepriv,
		     _FW_UNDER_LINKING) != true)) {
			r8712_set_rpwm(padapter, PS_STATE_S0);
		}
	}
	mutex_unlock(&pwrctrl->mutex_lock);
}

void r8712_flush_rwctrl_works(struct _adapter *padapter)
{
	struct pwrctrl_priv *pwrctrl = &padapter->pwrctrlpriv;

	flush_work(&pwrctrl->SetPSModeWorkItem);
	flush_work(&pwrctrl->rpwm_workitem);
}
