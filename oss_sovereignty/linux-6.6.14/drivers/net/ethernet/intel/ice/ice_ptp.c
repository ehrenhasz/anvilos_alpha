
 

#include "ice.h"
#include "ice_lib.h"
#include "ice_trace.h"

#define E810_OUT_PROP_DELAY_NS 1

#define UNKNOWN_INCVAL_E822 0x100000000ULL

static const struct ptp_pin_desc ice_pin_desc_e810t[] = {
	 
	{ "GNSS",  GNSS, PTP_PF_EXTTS, 0, { 0, } },
	{ "SMA1",  SMA1, PTP_PF_NONE, 1, { 0, } },
	{ "U.FL1", UFL1, PTP_PF_NONE, 1, { 0, } },
	{ "SMA2",  SMA2, PTP_PF_NONE, 2, { 0, } },
	{ "U.FL2", UFL2, PTP_PF_NONE, 2, { 0, } },
};

 
static int
ice_get_sma_config_e810t(struct ice_hw *hw, struct ptp_pin_desc *ptp_pins)
{
	u8 data, i;
	int status;

	 
	status = ice_read_sma_ctrl_e810t(hw, &data);
	if (status)
		return status;

	 
	for (i = 0; i < NUM_PTP_PINS_E810T; i++) {
		snprintf(ptp_pins[i].name, sizeof(ptp_pins[i].name),
			 "%s", ice_pin_desc_e810t[i].name);
		ptp_pins[i].index = ice_pin_desc_e810t[i].index;
		ptp_pins[i].func = ice_pin_desc_e810t[i].func;
		ptp_pins[i].chan = ice_pin_desc_e810t[i].chan;
	}

	 
	switch (data & ICE_SMA1_MASK_E810T) {
	case ICE_SMA1_MASK_E810T:
	default:
		ptp_pins[SMA1].func = PTP_PF_NONE;
		ptp_pins[UFL1].func = PTP_PF_NONE;
		break;
	case ICE_SMA1_DIR_EN_E810T:
		ptp_pins[SMA1].func = PTP_PF_PEROUT;
		ptp_pins[UFL1].func = PTP_PF_NONE;
		break;
	case ICE_SMA1_TX_EN_E810T:
		ptp_pins[SMA1].func = PTP_PF_EXTTS;
		ptp_pins[UFL1].func = PTP_PF_NONE;
		break;
	case 0:
		ptp_pins[SMA1].func = PTP_PF_EXTTS;
		ptp_pins[UFL1].func = PTP_PF_PEROUT;
		break;
	}

	 
	switch (data & ICE_SMA2_MASK_E810T) {
	case ICE_SMA2_MASK_E810T:
	default:
		ptp_pins[SMA2].func = PTP_PF_NONE;
		ptp_pins[UFL2].func = PTP_PF_NONE;
		break;
	case (ICE_SMA2_TX_EN_E810T | ICE_SMA2_UFL2_RX_DIS_E810T):
		ptp_pins[SMA2].func = PTP_PF_EXTTS;
		ptp_pins[UFL2].func = PTP_PF_NONE;
		break;
	case (ICE_SMA2_DIR_EN_E810T | ICE_SMA2_UFL2_RX_DIS_E810T):
		ptp_pins[SMA2].func = PTP_PF_PEROUT;
		ptp_pins[UFL2].func = PTP_PF_NONE;
		break;
	case (ICE_SMA2_DIR_EN_E810T | ICE_SMA2_TX_EN_E810T):
		ptp_pins[SMA2].func = PTP_PF_NONE;
		ptp_pins[UFL2].func = PTP_PF_EXTTS;
		break;
	case ICE_SMA2_DIR_EN_E810T:
		ptp_pins[SMA2].func = PTP_PF_PEROUT;
		ptp_pins[UFL2].func = PTP_PF_EXTTS;
		break;
	}

	return 0;
}

 
static int
ice_ptp_set_sma_config_e810t(struct ice_hw *hw,
			     const struct ptp_pin_desc *ptp_pins)
{
	int status;
	u8 data;

	 
	if (ptp_pins[SMA1].func == PTP_PF_PEROUT &&
	    ptp_pins[UFL1].func == PTP_PF_PEROUT)
		return -EINVAL;

	 
	if (ptp_pins[SMA2].func == PTP_PF_EXTTS &&
	    ptp_pins[UFL2].func == PTP_PF_EXTTS)
		return -EINVAL;

	 
	status = ice_read_sma_ctrl_e810t(hw, &data);
	if (status)
		return status;

	 
	data &= ~ICE_SMA1_MASK_E810T;
	if (ptp_pins[SMA1].func == PTP_PF_NONE &&
	    ptp_pins[UFL1].func == PTP_PF_NONE) {
		dev_info(ice_hw_to_dev(hw), "SMA1 + U.FL1 disabled");
		data |= ICE_SMA1_MASK_E810T;
	} else if (ptp_pins[SMA1].func == PTP_PF_EXTTS &&
		   ptp_pins[UFL1].func == PTP_PF_NONE) {
		dev_info(ice_hw_to_dev(hw), "SMA1 RX");
		data |= ICE_SMA1_TX_EN_E810T;
	} else if (ptp_pins[SMA1].func == PTP_PF_NONE &&
		   ptp_pins[UFL1].func == PTP_PF_PEROUT) {
		 
		dev_info(ice_hw_to_dev(hw), "SMA1 RX + U.FL1 TX");
	} else if (ptp_pins[SMA1].func == PTP_PF_EXTTS &&
		   ptp_pins[UFL1].func == PTP_PF_PEROUT) {
		dev_info(ice_hw_to_dev(hw), "SMA1 RX + U.FL1 TX");
	} else if (ptp_pins[SMA1].func == PTP_PF_PEROUT &&
		   ptp_pins[UFL1].func == PTP_PF_NONE) {
		dev_info(ice_hw_to_dev(hw), "SMA1 TX");
		data |= ICE_SMA1_DIR_EN_E810T;
	}

	data &= ~ICE_SMA2_MASK_E810T;
	if (ptp_pins[SMA2].func == PTP_PF_NONE &&
	    ptp_pins[UFL2].func == PTP_PF_NONE) {
		dev_info(ice_hw_to_dev(hw), "SMA2 + U.FL2 disabled");
		data |= ICE_SMA2_MASK_E810T;
	} else if (ptp_pins[SMA2].func == PTP_PF_EXTTS &&
			ptp_pins[UFL2].func == PTP_PF_NONE) {
		dev_info(ice_hw_to_dev(hw), "SMA2 RX");
		data |= (ICE_SMA2_TX_EN_E810T |
			 ICE_SMA2_UFL2_RX_DIS_E810T);
	} else if (ptp_pins[SMA2].func == PTP_PF_NONE &&
		   ptp_pins[UFL2].func == PTP_PF_EXTTS) {
		dev_info(ice_hw_to_dev(hw), "UFL2 RX");
		data |= (ICE_SMA2_DIR_EN_E810T | ICE_SMA2_TX_EN_E810T);
	} else if (ptp_pins[SMA2].func == PTP_PF_PEROUT &&
		   ptp_pins[UFL2].func == PTP_PF_NONE) {
		dev_info(ice_hw_to_dev(hw), "SMA2 TX");
		data |= (ICE_SMA2_DIR_EN_E810T |
			 ICE_SMA2_UFL2_RX_DIS_E810T);
	} else if (ptp_pins[SMA2].func == PTP_PF_PEROUT &&
		   ptp_pins[UFL2].func == PTP_PF_EXTTS) {
		dev_info(ice_hw_to_dev(hw), "SMA2 TX + U.FL2 RX");
		data |= ICE_SMA2_DIR_EN_E810T;
	}

	return ice_write_sma_ctrl_e810t(hw, data);
}

 
static int
ice_ptp_set_sma_e810t(struct ptp_clock_info *info, unsigned int pin,
		      enum ptp_pin_function func)
{
	struct ptp_pin_desc ptp_pins[NUM_PTP_PINS_E810T];
	struct ice_pf *pf = ptp_info_to_pf(info);
	struct ice_hw *hw = &pf->hw;
	int err;

	if (pin < SMA1 || func > PTP_PF_PEROUT)
		return -EOPNOTSUPP;

	err = ice_get_sma_config_e810t(hw, ptp_pins);
	if (err)
		return err;

	 
	if (pin == SMA1 && ptp_pins[UFL1].func == func)
		ptp_pins[UFL1].func = PTP_PF_NONE;
	if (pin == UFL1 && ptp_pins[SMA1].func == func)
		ptp_pins[SMA1].func = PTP_PF_NONE;

	if (pin == SMA2 && ptp_pins[UFL2].func == func)
		ptp_pins[UFL2].func = PTP_PF_NONE;
	if (pin == UFL2 && ptp_pins[SMA2].func == func)
		ptp_pins[SMA2].func = PTP_PF_NONE;

	 
	ptp_pins[pin].func = func;

	return ice_ptp_set_sma_config_e810t(hw, ptp_pins);
}

 
static int
ice_verify_pin_e810t(struct ptp_clock_info *info, unsigned int pin,
		     enum ptp_pin_function func, unsigned int chan)
{
	 
	if (chan != ice_pin_desc_e810t[pin].chan)
		return -EOPNOTSUPP;

	 
	switch (func) {
	case PTP_PF_NONE:
		break;
	case PTP_PF_EXTTS:
		if (pin == UFL1)
			return -EOPNOTSUPP;
		break;
	case PTP_PF_PEROUT:
		if (pin == UFL2 || pin == GNSS)
			return -EOPNOTSUPP;
		break;
	case PTP_PF_PHYSYNC:
		return -EOPNOTSUPP;
	}

	return ice_ptp_set_sma_e810t(info, pin, func);
}

 
static void ice_set_tx_tstamp(struct ice_pf *pf, bool on)
{
	struct ice_vsi *vsi;
	u32 val;
	u16 i;

	vsi = ice_get_main_vsi(pf);
	if (!vsi)
		return;

	 
	ice_for_each_txq(vsi, i) {
		if (!vsi->tx_rings[i])
			continue;
		vsi->tx_rings[i]->ptp_tx = on;
	}

	 
	val = rd32(&pf->hw, PFINT_OICR_ENA);
	if (on)
		val |= PFINT_OICR_TSYN_TX_M;
	else
		val &= ~PFINT_OICR_TSYN_TX_M;
	wr32(&pf->hw, PFINT_OICR_ENA, val);

	pf->ptp.tstamp_config.tx_type = on ? HWTSTAMP_TX_ON : HWTSTAMP_TX_OFF;
}

 
static void ice_set_rx_tstamp(struct ice_pf *pf, bool on)
{
	struct ice_vsi *vsi;
	u16 i;

	vsi = ice_get_main_vsi(pf);
	if (!vsi)
		return;

	 
	ice_for_each_rxq(vsi, i) {
		if (!vsi->rx_rings[i])
			continue;
		vsi->rx_rings[i]->ptp_rx = on;
	}

	pf->ptp.tstamp_config.rx_filter = on ? HWTSTAMP_FILTER_ALL :
					       HWTSTAMP_FILTER_NONE;
}

 
void ice_ptp_cfg_timestamp(struct ice_pf *pf, bool ena)
{
	ice_set_tx_tstamp(pf, ena);
	ice_set_rx_tstamp(pf, ena);
}

 
int ice_get_ptp_clock_index(struct ice_pf *pf)
{
	struct device *dev = ice_pf_to_dev(pf);
	enum ice_aqc_driver_params param_idx;
	struct ice_hw *hw = &pf->hw;
	u8 tmr_idx;
	u32 value;
	int err;

	 
	if (pf->ptp.clock)
		return ptp_clock_index(pf->ptp.clock);

	tmr_idx = hw->func_caps.ts_func_info.tmr_index_assoc;
	if (!tmr_idx)
		param_idx = ICE_AQC_DRIVER_PARAM_CLK_IDX_TMR0;
	else
		param_idx = ICE_AQC_DRIVER_PARAM_CLK_IDX_TMR1;

	err = ice_aq_get_driver_param(hw, param_idx, &value, NULL);
	if (err) {
		dev_err(dev, "Failed to read PTP clock index parameter, err %d aq_err %s\n",
			err, ice_aq_str(hw->adminq.sq_last_status));
		return -1;
	}

	 
	if (!(value & PTP_SHARED_CLK_IDX_VALID))
		return -1;

	return value & ~PTP_SHARED_CLK_IDX_VALID;
}

 
static void ice_set_ptp_clock_index(struct ice_pf *pf)
{
	struct device *dev = ice_pf_to_dev(pf);
	enum ice_aqc_driver_params param_idx;
	struct ice_hw *hw = &pf->hw;
	u8 tmr_idx;
	u32 value;
	int err;

	if (!pf->ptp.clock)
		return;

	tmr_idx = hw->func_caps.ts_func_info.tmr_index_assoc;
	if (!tmr_idx)
		param_idx = ICE_AQC_DRIVER_PARAM_CLK_IDX_TMR0;
	else
		param_idx = ICE_AQC_DRIVER_PARAM_CLK_IDX_TMR1;

	value = (u32)ptp_clock_index(pf->ptp.clock);
	if (value > INT_MAX) {
		dev_err(dev, "PTP Clock index is too large to store\n");
		return;
	}
	value |= PTP_SHARED_CLK_IDX_VALID;

	err = ice_aq_set_driver_param(hw, param_idx, value, NULL);
	if (err) {
		dev_err(dev, "Failed to set PTP clock index parameter, err %d aq_err %s\n",
			err, ice_aq_str(hw->adminq.sq_last_status));
	}
}

 
static void ice_clear_ptp_clock_index(struct ice_pf *pf)
{
	struct device *dev = ice_pf_to_dev(pf);
	enum ice_aqc_driver_params param_idx;
	struct ice_hw *hw = &pf->hw;
	u8 tmr_idx;
	int err;

	 
	if (!hw->func_caps.ts_func_info.src_tmr_owned)
		return;

	tmr_idx = hw->func_caps.ts_func_info.tmr_index_assoc;
	if (!tmr_idx)
		param_idx = ICE_AQC_DRIVER_PARAM_CLK_IDX_TMR0;
	else
		param_idx = ICE_AQC_DRIVER_PARAM_CLK_IDX_TMR1;

	err = ice_aq_set_driver_param(hw, param_idx, 0, NULL);
	if (err) {
		dev_dbg(dev, "Failed to clear PTP clock index parameter, err %d aq_err %s\n",
			err, ice_aq_str(hw->adminq.sq_last_status));
	}
}

 
static u64
ice_ptp_read_src_clk_reg(struct ice_pf *pf, struct ptp_system_timestamp *sts)
{
	struct ice_hw *hw = &pf->hw;
	u32 hi, lo, lo2;
	u8 tmr_idx;

	tmr_idx = ice_get_ptp_src_clock_index(hw);
	 
	ptp_read_system_prets(sts);

	lo = rd32(hw, GLTSYN_TIME_L(tmr_idx));

	 
	ptp_read_system_postts(sts);

	hi = rd32(hw, GLTSYN_TIME_H(tmr_idx));
	lo2 = rd32(hw, GLTSYN_TIME_L(tmr_idx));

	if (lo2 < lo) {
		 
		ptp_read_system_prets(sts);
		lo = rd32(hw, GLTSYN_TIME_L(tmr_idx));
		ptp_read_system_postts(sts);
		hi = rd32(hw, GLTSYN_TIME_H(tmr_idx));
	}

	return ((u64)hi << 32) | lo;
}

 
static u64 ice_ptp_extend_32b_ts(u64 cached_phc_time, u32 in_tstamp)
{
	u32 delta, phc_time_lo;
	u64 ns;

	 
	phc_time_lo = (u32)cached_phc_time;

	 
	delta = (in_tstamp - phc_time_lo);

	 
	if (delta > (U32_MAX / 2)) {
		 
		delta = (phc_time_lo - in_tstamp);
		ns = cached_phc_time - delta;
	} else {
		ns = cached_phc_time + delta;
	}

	return ns;
}

 
static u64 ice_ptp_extend_40b_ts(struct ice_pf *pf, u64 in_tstamp)
{
	const u64 mask = GENMASK_ULL(31, 0);
	unsigned long discard_time;

	 
	discard_time = pf->ptp.cached_phc_jiffies + msecs_to_jiffies(2000);
	if (time_is_before_jiffies(discard_time)) {
		pf->ptp.tx_hwtstamp_discarded++;
		return 0;
	}

	return ice_ptp_extend_32b_ts(pf->ptp.cached_phc_time,
				     (in_tstamp >> 8) & mask);
}

 
static bool
ice_ptp_is_tx_tracker_up(struct ice_ptp_tx *tx)
{
	lockdep_assert_held(&tx->lock);

	return tx->init && !tx->calibrating;
}

 
static void ice_ptp_process_tx_tstamp(struct ice_ptp_tx *tx)
{
	struct ice_ptp_port *ptp_port;
	struct ice_pf *pf;
	struct ice_hw *hw;
	u64 tstamp_ready;
	bool link_up;
	int err;
	u8 idx;

	if (!tx->init)
		return;

	ptp_port = container_of(tx, struct ice_ptp_port, tx);
	pf = ptp_port_to_pf(ptp_port);
	hw = &pf->hw;

	 
	err = ice_get_phy_tx_tstamp_ready(hw, tx->block, &tstamp_ready);
	if (err)
		return;

	 
	link_up = ptp_port->link_up;

	for_each_set_bit(idx, tx->in_use, tx->len) {
		struct skb_shared_hwtstamps shhwtstamps = {};
		u8 phy_idx = idx + tx->offset;
		u64 raw_tstamp = 0, tstamp;
		bool drop_ts = !link_up;
		struct sk_buff *skb;

		 
		if (time_is_before_jiffies(tx->tstamps[idx].start + 2 * HZ)) {
			drop_ts = true;

			 
			pf->ptp.tx_hwtstamp_timeouts++;
		}

		 
		if (!(tstamp_ready & BIT_ULL(phy_idx))) {
			if (drop_ts)
				goto skip_ts_read;

			continue;
		}

		ice_trace(tx_tstamp_fw_req, tx->tstamps[idx].skb, idx);

		err = ice_read_phy_tstamp(hw, tx->block, phy_idx, &raw_tstamp);
		if (err && !drop_ts)
			continue;

		ice_trace(tx_tstamp_fw_done, tx->tstamps[idx].skb, idx);

		 
		if (!drop_ts && tx->verify_cached &&
		    raw_tstamp == tx->tstamps[idx].cached_tstamp)
			continue;

		 
		if (!(raw_tstamp & ICE_PTP_TS_VALID))
			drop_ts = true;

skip_ts_read:
		spin_lock(&tx->lock);
		if (tx->verify_cached && raw_tstamp)
			tx->tstamps[idx].cached_tstamp = raw_tstamp;
		clear_bit(idx, tx->in_use);
		skb = tx->tstamps[idx].skb;
		tx->tstamps[idx].skb = NULL;
		if (test_and_clear_bit(idx, tx->stale))
			drop_ts = true;
		spin_unlock(&tx->lock);

		 
		if (!skb)
			continue;

		if (drop_ts) {
			dev_kfree_skb_any(skb);
			continue;
		}

		 
		tstamp = ice_ptp_extend_40b_ts(pf, raw_tstamp);
		if (tstamp) {
			shhwtstamps.hwtstamp = ns_to_ktime(tstamp);
			ice_trace(tx_tstamp_complete, skb, idx);
		}

		skb_tstamp_tx(skb, &shhwtstamps);
		dev_kfree_skb_any(skb);
	}
}

 
static enum ice_tx_tstamp_work ice_ptp_tx_tstamp(struct ice_ptp_tx *tx)
{
	bool more_timestamps;

	if (!tx->init)
		return ICE_TX_TSTAMP_WORK_DONE;

	 
	ice_ptp_process_tx_tstamp(tx);

	 
	spin_lock(&tx->lock);
	more_timestamps = tx->init && !bitmap_empty(tx->in_use, tx->len);
	spin_unlock(&tx->lock);

	if (more_timestamps)
		return ICE_TX_TSTAMP_WORK_PENDING;

	return ICE_TX_TSTAMP_WORK_DONE;
}

 
static int
ice_ptp_alloc_tx_tracker(struct ice_ptp_tx *tx)
{
	unsigned long *in_use, *stale;
	struct ice_tx_tstamp *tstamps;

	tstamps = kcalloc(tx->len, sizeof(*tstamps), GFP_KERNEL);
	in_use = bitmap_zalloc(tx->len, GFP_KERNEL);
	stale = bitmap_zalloc(tx->len, GFP_KERNEL);

	if (!tstamps || !in_use || !stale) {
		kfree(tstamps);
		bitmap_free(in_use);
		bitmap_free(stale);

		return -ENOMEM;
	}

	tx->tstamps = tstamps;
	tx->in_use = in_use;
	tx->stale = stale;
	tx->init = 1;

	spin_lock_init(&tx->lock);

	return 0;
}

 
static void
ice_ptp_flush_tx_tracker(struct ice_pf *pf, struct ice_ptp_tx *tx)
{
	struct ice_hw *hw = &pf->hw;
	u64 tstamp_ready;
	int err;
	u8 idx;

	err = ice_get_phy_tx_tstamp_ready(hw, tx->block, &tstamp_ready);
	if (err) {
		dev_dbg(ice_pf_to_dev(pf), "Failed to get the Tx tstamp ready bitmap for block %u, err %d\n",
			tx->block, err);

		 
		tstamp_ready = 0;
	}

	for_each_set_bit(idx, tx->in_use, tx->len) {
		u8 phy_idx = idx + tx->offset;
		struct sk_buff *skb;

		 
		if (!hw->reset_ongoing && (tstamp_ready & BIT_ULL(phy_idx)))
			ice_clear_phy_tstamp(hw, tx->block, phy_idx);

		spin_lock(&tx->lock);
		skb = tx->tstamps[idx].skb;
		tx->tstamps[idx].skb = NULL;
		clear_bit(idx, tx->in_use);
		clear_bit(idx, tx->stale);
		spin_unlock(&tx->lock);

		 
		pf->ptp.tx_hwtstamp_flushed++;

		 
		dev_kfree_skb_any(skb);
	}
}

 
static void
ice_ptp_mark_tx_tracker_stale(struct ice_ptp_tx *tx)
{
	spin_lock(&tx->lock);
	bitmap_or(tx->stale, tx->stale, tx->in_use, tx->len);
	spin_unlock(&tx->lock);
}

 
static void
ice_ptp_release_tx_tracker(struct ice_pf *pf, struct ice_ptp_tx *tx)
{
	spin_lock(&tx->lock);
	tx->init = 0;
	spin_unlock(&tx->lock);

	 
	synchronize_irq(pf->oicr_irq.virq);

	ice_ptp_flush_tx_tracker(pf, tx);

	kfree(tx->tstamps);
	tx->tstamps = NULL;

	bitmap_free(tx->in_use);
	tx->in_use = NULL;

	bitmap_free(tx->stale);
	tx->stale = NULL;

	tx->len = 0;
}

 
static int
ice_ptp_init_tx_e822(struct ice_pf *pf, struct ice_ptp_tx *tx, u8 port)
{
	tx->block = port / ICE_PORTS_PER_QUAD;
	tx->offset = (port % ICE_PORTS_PER_QUAD) * INDEX_PER_PORT_E822;
	tx->len = INDEX_PER_PORT_E822;
	tx->verify_cached = 0;

	return ice_ptp_alloc_tx_tracker(tx);
}

 
static int
ice_ptp_init_tx_e810(struct ice_pf *pf, struct ice_ptp_tx *tx)
{
	tx->block = pf->hw.port_info->lport;
	tx->offset = 0;
	tx->len = INDEX_PER_PORT_E810;
	 
	tx->verify_cached = 1;

	return ice_ptp_alloc_tx_tracker(tx);
}

 
static int ice_ptp_update_cached_phctime(struct ice_pf *pf)
{
	struct device *dev = ice_pf_to_dev(pf);
	unsigned long update_before;
	u64 systime;
	int i;

	update_before = pf->ptp.cached_phc_jiffies + msecs_to_jiffies(2000);
	if (pf->ptp.cached_phc_time &&
	    time_is_before_jiffies(update_before)) {
		unsigned long time_taken = jiffies - pf->ptp.cached_phc_jiffies;

		dev_warn(dev, "%u msecs passed between update to cached PHC time\n",
			 jiffies_to_msecs(time_taken));
		pf->ptp.late_cached_phc_updates++;
	}

	 
	systime = ice_ptp_read_src_clk_reg(pf, NULL);

	 
	WRITE_ONCE(pf->ptp.cached_phc_time, systime);
	WRITE_ONCE(pf->ptp.cached_phc_jiffies, jiffies);

	if (test_and_set_bit(ICE_CFG_BUSY, pf->state))
		return -EAGAIN;

	ice_for_each_vsi(pf, i) {
		struct ice_vsi *vsi = pf->vsi[i];
		int j;

		if (!vsi)
			continue;

		if (vsi->type != ICE_VSI_PF)
			continue;

		ice_for_each_rxq(vsi, j) {
			if (!vsi->rx_rings[j])
				continue;
			WRITE_ONCE(vsi->rx_rings[j]->cached_phctime, systime);
		}
	}
	clear_bit(ICE_CFG_BUSY, pf->state);

	return 0;
}

 
static void ice_ptp_reset_cached_phctime(struct ice_pf *pf)
{
	struct device *dev = ice_pf_to_dev(pf);
	int err;

	 
	err = ice_ptp_update_cached_phctime(pf);
	if (err) {
		 
		dev_warn(dev, "%s: ICE_CFG_BUSY, unable to immediately update cached PHC time\n",
			 __func__);

		 
		kthread_queue_delayed_work(pf->ptp.kworker, &pf->ptp.work,
					   msecs_to_jiffies(10));
	}

	 
	ice_ptp_mark_tx_tracker_stale(&pf->ptp.port.tx);
}

 
static void
ice_ptp_read_time(struct ice_pf *pf, struct timespec64 *ts,
		  struct ptp_system_timestamp *sts)
{
	u64 time_ns = ice_ptp_read_src_clk_reg(pf, sts);

	*ts = ns_to_timespec64(time_ns);
}

 
static int ice_ptp_write_init(struct ice_pf *pf, struct timespec64 *ts)
{
	u64 ns = timespec64_to_ns(ts);
	struct ice_hw *hw = &pf->hw;

	return ice_ptp_init_time(hw, ns);
}

 
static int ice_ptp_write_adj(struct ice_pf *pf, s32 adj)
{
	struct ice_hw *hw = &pf->hw;

	return ice_ptp_adj_clock(hw, adj);
}

 
static u64 ice_base_incval(struct ice_pf *pf)
{
	struct ice_hw *hw = &pf->hw;
	u64 incval;

	if (ice_is_e810(hw))
		incval = ICE_PTP_NOMINAL_INCVAL_E810;
	else if (ice_e822_time_ref(hw) < NUM_ICE_TIME_REF_FREQ)
		incval = ice_e822_nominal_incval(ice_e822_time_ref(hw));
	else
		incval = UNKNOWN_INCVAL_E822;

	dev_dbg(ice_pf_to_dev(pf), "PTP: using base increment value of 0x%016llx\n",
		incval);

	return incval;
}

 
static int ice_ptp_check_tx_fifo(struct ice_ptp_port *port)
{
	int quad = port->port_num / ICE_PORTS_PER_QUAD;
	int offs = port->port_num % ICE_PORTS_PER_QUAD;
	struct ice_pf *pf;
	struct ice_hw *hw;
	u32 val, phy_sts;
	int err;

	pf = ptp_port_to_pf(port);
	hw = &pf->hw;

	if (port->tx_fifo_busy_cnt == FIFO_OK)
		return 0;

	 
	if (offs == 0 || offs == 1)
		err = ice_read_quad_reg_e822(hw, quad, Q_REG_FIFO01_STATUS,
					     &val);
	else
		err = ice_read_quad_reg_e822(hw, quad, Q_REG_FIFO23_STATUS,
					     &val);

	if (err) {
		dev_err(ice_pf_to_dev(pf), "PTP failed to check port %d Tx FIFO, err %d\n",
			port->port_num, err);
		return err;
	}

	if (offs & 0x1)
		phy_sts = (val & Q_REG_FIFO13_M) >> Q_REG_FIFO13_S;
	else
		phy_sts = (val & Q_REG_FIFO02_M) >> Q_REG_FIFO02_S;

	if (phy_sts & FIFO_EMPTY) {
		port->tx_fifo_busy_cnt = FIFO_OK;
		return 0;
	}

	port->tx_fifo_busy_cnt++;

	dev_dbg(ice_pf_to_dev(pf), "Try %d, port %d FIFO not empty\n",
		port->tx_fifo_busy_cnt, port->port_num);

	if (port->tx_fifo_busy_cnt == ICE_PTP_FIFO_NUM_CHECKS) {
		dev_dbg(ice_pf_to_dev(pf),
			"Port %d Tx FIFO still not empty; resetting quad %d\n",
			port->port_num, quad);
		ice_ptp_reset_ts_memory_quad_e822(hw, quad);
		port->tx_fifo_busy_cnt = FIFO_OK;
		return 0;
	}

	return -EAGAIN;
}

 
static void ice_ptp_wait_for_offsets(struct kthread_work *work)
{
	struct ice_ptp_port *port;
	struct ice_pf *pf;
	struct ice_hw *hw;
	int tx_err;
	int rx_err;

	port = container_of(work, struct ice_ptp_port, ov_work.work);
	pf = ptp_port_to_pf(port);
	hw = &pf->hw;

	if (ice_is_reset_in_progress(pf->state)) {
		 
		kthread_queue_delayed_work(pf->ptp.kworker,
					   &port->ov_work,
					   msecs_to_jiffies(100));
		return;
	}

	tx_err = ice_ptp_check_tx_fifo(port);
	if (!tx_err)
		tx_err = ice_phy_cfg_tx_offset_e822(hw, port->port_num);
	rx_err = ice_phy_cfg_rx_offset_e822(hw, port->port_num);
	if (tx_err || rx_err) {
		 
		kthread_queue_delayed_work(pf->ptp.kworker,
					   &port->ov_work,
					   msecs_to_jiffies(100));
		return;
	}
}

 
static int
ice_ptp_port_phy_stop(struct ice_ptp_port *ptp_port)
{
	struct ice_pf *pf = ptp_port_to_pf(ptp_port);
	u8 port = ptp_port->port_num;
	struct ice_hw *hw = &pf->hw;
	int err;

	if (ice_is_e810(hw))
		return 0;

	mutex_lock(&ptp_port->ps_lock);

	kthread_cancel_delayed_work_sync(&ptp_port->ov_work);

	err = ice_stop_phy_timer_e822(hw, port, true);
	if (err)
		dev_err(ice_pf_to_dev(pf), "PTP failed to set PHY port %d down, err %d\n",
			port, err);

	mutex_unlock(&ptp_port->ps_lock);

	return err;
}

 
static int
ice_ptp_port_phy_restart(struct ice_ptp_port *ptp_port)
{
	struct ice_pf *pf = ptp_port_to_pf(ptp_port);
	u8 port = ptp_port->port_num;
	struct ice_hw *hw = &pf->hw;
	int err;

	if (ice_is_e810(hw))
		return 0;

	if (!ptp_port->link_up)
		return ice_ptp_port_phy_stop(ptp_port);

	mutex_lock(&ptp_port->ps_lock);

	kthread_cancel_delayed_work_sync(&ptp_port->ov_work);

	 
	spin_lock(&ptp_port->tx.lock);
	ptp_port->tx.calibrating = true;
	spin_unlock(&ptp_port->tx.lock);
	ptp_port->tx_fifo_busy_cnt = 0;

	 
	err = ice_start_phy_timer_e822(hw, port);
	if (err)
		goto out_unlock;

	 
	spin_lock(&ptp_port->tx.lock);
	ptp_port->tx.calibrating = false;
	spin_unlock(&ptp_port->tx.lock);

	kthread_queue_delayed_work(pf->ptp.kworker, &ptp_port->ov_work, 0);

out_unlock:
	if (err)
		dev_err(ice_pf_to_dev(pf), "PTP failed to set PHY port %d up, err %d\n",
			port, err);

	mutex_unlock(&ptp_port->ps_lock);

	return err;
}

 
void ice_ptp_link_change(struct ice_pf *pf, u8 port, bool linkup)
{
	struct ice_ptp_port *ptp_port;

	if (!test_bit(ICE_FLAG_PTP, pf->flags))
		return;

	if (WARN_ON_ONCE(port >= ICE_NUM_EXTERNAL_PORTS))
		return;

	ptp_port = &pf->ptp.port;
	if (WARN_ON_ONCE(ptp_port->port_num != port))
		return;

	 
	ptp_port->link_up = linkup;

	 
	if (ice_is_e810(&pf->hw))
		return;

	ice_ptp_port_phy_restart(ptp_port);
}

 
static int ice_ptp_tx_ena_intr(struct ice_pf *pf, bool ena, u32 threshold)
{
	struct ice_hw *hw = &pf->hw;
	int err = 0;
	int quad;
	u32 val;

	ice_ptp_reset_ts_memory(hw);

	for (quad = 0; quad < ICE_MAX_QUAD; quad++) {
		err = ice_read_quad_reg_e822(hw, quad, Q_REG_TX_MEM_GBL_CFG,
					     &val);
		if (err)
			break;

		if (ena) {
			val |= Q_REG_TX_MEM_GBL_CFG_INTR_ENA_M;
			val &= ~Q_REG_TX_MEM_GBL_CFG_INTR_THR_M;
			val |= ((threshold << Q_REG_TX_MEM_GBL_CFG_INTR_THR_S) &
				Q_REG_TX_MEM_GBL_CFG_INTR_THR_M);
		} else {
			val &= ~Q_REG_TX_MEM_GBL_CFG_INTR_ENA_M;
		}

		err = ice_write_quad_reg_e822(hw, quad, Q_REG_TX_MEM_GBL_CFG,
					      val);
		if (err)
			break;
	}

	if (err)
		dev_err(ice_pf_to_dev(pf), "PTP failed in intr ena, err %d\n",
			err);
	return err;
}

 
static void ice_ptp_reset_phy_timestamping(struct ice_pf *pf)
{
	ice_ptp_port_phy_restart(&pf->ptp.port);
}

 
static int ice_ptp_adjfine(struct ptp_clock_info *info, long scaled_ppm)
{
	struct ice_pf *pf = ptp_info_to_pf(info);
	struct ice_hw *hw = &pf->hw;
	u64 incval;
	int err;

	incval = adjust_by_scaled_ppm(ice_base_incval(pf), scaled_ppm);
	err = ice_ptp_write_incval_locked(hw, incval);
	if (err) {
		dev_err(ice_pf_to_dev(pf), "PTP failed to set incval, err %d\n",
			err);
		return -EIO;
	}

	return 0;
}

 
void ice_ptp_extts_event(struct ice_pf *pf)
{
	struct ptp_clock_event event;
	struct ice_hw *hw = &pf->hw;
	u8 chan, tmr_idx;
	u32 hi, lo;

	tmr_idx = hw->func_caps.ts_func_info.tmr_index_owned;
	 
	for (chan = 0; chan < GLTSYN_EVNT_H_IDX_MAX; chan++) {
		 
		if (pf->ptp.ext_ts_irq & (1 << chan)) {
			lo = rd32(hw, GLTSYN_EVNT_L(chan, tmr_idx));
			hi = rd32(hw, GLTSYN_EVNT_H(chan, tmr_idx));
			event.timestamp = (((u64)hi) << 32) | lo;
			event.type = PTP_CLOCK_EXTTS;
			event.index = chan;

			 
			ptp_clock_event(pf->ptp.clock, &event);
			pf->ptp.ext_ts_irq &= ~(1 << chan);
		}
	}
}

 
static int
ice_ptp_cfg_extts(struct ice_pf *pf, bool ena, unsigned int chan, u32 gpio_pin,
		  unsigned int extts_flags)
{
	u32 func, aux_reg, gpio_reg, irq_reg;
	struct ice_hw *hw = &pf->hw;
	u8 tmr_idx;

	if (chan > (unsigned int)pf->ptp.info.n_ext_ts)
		return -EINVAL;

	tmr_idx = hw->func_caps.ts_func_info.tmr_index_owned;

	irq_reg = rd32(hw, PFINT_OICR_ENA);

	if (ena) {
		 
		irq_reg |= PFINT_OICR_TSYN_EVNT_M;
		aux_reg = GLTSYN_AUX_IN_0_INT_ENA_M;

#define GLTSYN_AUX_IN_0_EVNTLVL_RISING_EDGE	BIT(0)
#define GLTSYN_AUX_IN_0_EVNTLVL_FALLING_EDGE	BIT(1)

		 
		if (extts_flags & PTP_FALLING_EDGE)
			aux_reg |= GLTSYN_AUX_IN_0_EVNTLVL_FALLING_EDGE;
		if (extts_flags & PTP_RISING_EDGE)
			aux_reg |= GLTSYN_AUX_IN_0_EVNTLVL_RISING_EDGE;

		 
		func = 1 + chan + (tmr_idx * 3);
		gpio_reg = ((func << GLGEN_GPIO_CTL_PIN_FUNC_S) &
			    GLGEN_GPIO_CTL_PIN_FUNC_M);
		pf->ptp.ext_ts_chan |= (1 << chan);
	} else {
		 
		aux_reg = 0;
		gpio_reg = 0;
		pf->ptp.ext_ts_chan &= ~(1 << chan);
		if (!pf->ptp.ext_ts_chan)
			irq_reg &= ~PFINT_OICR_TSYN_EVNT_M;
	}

	wr32(hw, PFINT_OICR_ENA, irq_reg);
	wr32(hw, GLTSYN_AUX_IN(chan, tmr_idx), aux_reg);
	wr32(hw, GLGEN_GPIO_CTL(gpio_pin), gpio_reg);

	return 0;
}

 
static int ice_ptp_cfg_clkout(struct ice_pf *pf, unsigned int chan,
			      struct ice_perout_channel *config, bool store)
{
	u64 current_time, period, start_time, phase;
	struct ice_hw *hw = &pf->hw;
	u32 func, val, gpio_pin;
	u8 tmr_idx;

	tmr_idx = hw->func_caps.ts_func_info.tmr_index_owned;

	 
	wr32(hw, GLTSYN_AUX_OUT(chan, tmr_idx), 0);

	 
	if (!config || !config->ena) {
		wr32(hw, GLTSYN_CLKO(chan, tmr_idx), 0);
		wr32(hw, GLTSYN_TGT_L(chan, tmr_idx), 0);
		wr32(hw, GLTSYN_TGT_H(chan, tmr_idx), 0);

		val = GLGEN_GPIO_CTL_PIN_DIR_M;
		gpio_pin = pf->ptp.perout_channels[chan].gpio_pin;
		wr32(hw, GLGEN_GPIO_CTL(gpio_pin), val);

		 
		if (store)
			memset(&pf->ptp.perout_channels[chan], 0,
			       sizeof(struct ice_perout_channel));

		return 0;
	}
	period = config->period;
	start_time = config->start_time;
	div64_u64_rem(start_time, period, &phase);
	gpio_pin = config->gpio_pin;

	 
	if (period & 0x1) {
		dev_err(ice_pf_to_dev(pf), "CLK Period must be an even value\n");
		goto err;
	}

	period >>= 1;

	 
#define MIN_PULSE 3
	if (period <= MIN_PULSE || period > U32_MAX) {
		dev_err(ice_pf_to_dev(pf), "CLK Period must be > %d && < 2^33",
			MIN_PULSE * 2);
		goto err;
	}

	wr32(hw, GLTSYN_CLKO(chan, tmr_idx), lower_32_bits(period));

	 
	current_time = ice_ptp_read_src_clk_reg(pf, NULL);

	 
	if (start_time < current_time)
		start_time = div64_u64(current_time + NSEC_PER_SEC - 1,
				       NSEC_PER_SEC) * NSEC_PER_SEC + phase;

	if (ice_is_e810(hw))
		start_time -= E810_OUT_PROP_DELAY_NS;
	else
		start_time -= ice_e822_pps_delay(ice_e822_time_ref(hw));

	 
	wr32(hw, GLTSYN_TGT_L(chan, tmr_idx), lower_32_bits(start_time));
	wr32(hw, GLTSYN_TGT_H(chan, tmr_idx), upper_32_bits(start_time));

	 
	val = GLTSYN_AUX_OUT_0_OUT_ENA_M | GLTSYN_AUX_OUT_0_OUTMOD_M;
	wr32(hw, GLTSYN_AUX_OUT(chan, tmr_idx), val);

	 
	func = 8 + chan + (tmr_idx * 4);
	val = GLGEN_GPIO_CTL_PIN_DIR_M |
	      ((func << GLGEN_GPIO_CTL_PIN_FUNC_S) & GLGEN_GPIO_CTL_PIN_FUNC_M);
	wr32(hw, GLGEN_GPIO_CTL(gpio_pin), val);

	 
	if (store) {
		memcpy(&pf->ptp.perout_channels[chan], config,
		       sizeof(struct ice_perout_channel));
		pf->ptp.perout_channels[chan].start_time = phase;
	}

	return 0;
err:
	dev_err(ice_pf_to_dev(pf), "PTP failed to cfg per_clk\n");
	return -EFAULT;
}

 
static void ice_ptp_disable_all_clkout(struct ice_pf *pf)
{
	uint i;

	for (i = 0; i < pf->ptp.info.n_per_out; i++)
		if (pf->ptp.perout_channels[i].ena)
			ice_ptp_cfg_clkout(pf, i, NULL, false);
}

 
static void ice_ptp_enable_all_clkout(struct ice_pf *pf)
{
	uint i;

	for (i = 0; i < pf->ptp.info.n_per_out; i++)
		if (pf->ptp.perout_channels[i].ena)
			ice_ptp_cfg_clkout(pf, i, &pf->ptp.perout_channels[i],
					   false);
}

 
static int
ice_ptp_gpio_enable_e810(struct ptp_clock_info *info,
			 struct ptp_clock_request *rq, int on)
{
	struct ice_pf *pf = ptp_info_to_pf(info);
	struct ice_perout_channel clk_cfg = {0};
	bool sma_pres = false;
	unsigned int chan;
	u32 gpio_pin;
	int err;

	if (ice_is_feature_supported(pf, ICE_F_SMA_CTRL))
		sma_pres = true;

	switch (rq->type) {
	case PTP_CLK_REQ_PEROUT:
		chan = rq->perout.index;
		if (sma_pres) {
			if (chan == ice_pin_desc_e810t[SMA1].chan)
				clk_cfg.gpio_pin = GPIO_20;
			else if (chan == ice_pin_desc_e810t[SMA2].chan)
				clk_cfg.gpio_pin = GPIO_22;
			else
				return -1;
		} else if (ice_is_e810t(&pf->hw)) {
			if (chan == 0)
				clk_cfg.gpio_pin = GPIO_20;
			else
				clk_cfg.gpio_pin = GPIO_22;
		} else if (chan == PPS_CLK_GEN_CHAN) {
			clk_cfg.gpio_pin = PPS_PIN_INDEX;
		} else {
			clk_cfg.gpio_pin = chan;
		}

		clk_cfg.period = ((rq->perout.period.sec * NSEC_PER_SEC) +
				   rq->perout.period.nsec);
		clk_cfg.start_time = ((rq->perout.start.sec * NSEC_PER_SEC) +
				       rq->perout.start.nsec);
		clk_cfg.ena = !!on;

		err = ice_ptp_cfg_clkout(pf, chan, &clk_cfg, true);
		break;
	case PTP_CLK_REQ_EXTTS:
		chan = rq->extts.index;
		if (sma_pres) {
			if (chan < ice_pin_desc_e810t[SMA2].chan)
				gpio_pin = GPIO_21;
			else
				gpio_pin = GPIO_23;
		} else if (ice_is_e810t(&pf->hw)) {
			if (chan == 0)
				gpio_pin = GPIO_21;
			else
				gpio_pin = GPIO_23;
		} else {
			gpio_pin = chan;
		}

		err = ice_ptp_cfg_extts(pf, !!on, chan, gpio_pin,
					rq->extts.flags);
		break;
	default:
		return -EOPNOTSUPP;
	}

	return err;
}

 
static int ice_ptp_gpio_enable_e823(struct ptp_clock_info *info,
				    struct ptp_clock_request *rq, int on)
{
	struct ice_pf *pf = ptp_info_to_pf(info);
	struct ice_perout_channel clk_cfg = {0};
	int err;

	switch (rq->type) {
	case PTP_CLK_REQ_PPS:
		clk_cfg.gpio_pin = PPS_PIN_INDEX;
		clk_cfg.period = NSEC_PER_SEC;
		clk_cfg.ena = !!on;

		err = ice_ptp_cfg_clkout(pf, PPS_CLK_GEN_CHAN, &clk_cfg, true);
		break;
	case PTP_CLK_REQ_EXTTS:
		err = ice_ptp_cfg_extts(pf, !!on, rq->extts.index,
					TIME_SYNC_PIN_INDEX, rq->extts.flags);
		break;
	default:
		return -EOPNOTSUPP;
	}

	return err;
}

 
static int
ice_ptp_gettimex64(struct ptp_clock_info *info, struct timespec64 *ts,
		   struct ptp_system_timestamp *sts)
{
	struct ice_pf *pf = ptp_info_to_pf(info);
	struct ice_hw *hw = &pf->hw;

	if (!ice_ptp_lock(hw)) {
		dev_err(ice_pf_to_dev(pf), "PTP failed to get time\n");
		return -EBUSY;
	}

	ice_ptp_read_time(pf, ts, sts);
	ice_ptp_unlock(hw);

	return 0;
}

 
static int
ice_ptp_settime64(struct ptp_clock_info *info, const struct timespec64 *ts)
{
	struct ice_pf *pf = ptp_info_to_pf(info);
	struct timespec64 ts64 = *ts;
	struct ice_hw *hw = &pf->hw;
	int err;

	 
	if (pf->ptp.port.link_up)
		ice_ptp_port_phy_stop(&pf->ptp.port);

	if (!ice_ptp_lock(hw)) {
		err = -EBUSY;
		goto exit;
	}

	 
	ice_ptp_disable_all_clkout(pf);

	err = ice_ptp_write_init(pf, &ts64);
	ice_ptp_unlock(hw);

	if (!err)
		ice_ptp_reset_cached_phctime(pf);

	 
	ice_ptp_enable_all_clkout(pf);

	 
	if (pf->ptp.port.link_up)
		ice_ptp_port_phy_restart(&pf->ptp.port);
exit:
	if (err) {
		dev_err(ice_pf_to_dev(pf), "PTP failed to set time %d\n", err);
		return err;
	}

	return 0;
}

 
static int ice_ptp_adjtime_nonatomic(struct ptp_clock_info *info, s64 delta)
{
	struct timespec64 now, then;
	int ret;

	then = ns_to_timespec64(delta);
	ret = ice_ptp_gettimex64(info, &now, NULL);
	if (ret)
		return ret;
	now = timespec64_add(now, then);

	return ice_ptp_settime64(info, (const struct timespec64 *)&now);
}

 
static int ice_ptp_adjtime(struct ptp_clock_info *info, s64 delta)
{
	struct ice_pf *pf = ptp_info_to_pf(info);
	struct ice_hw *hw = &pf->hw;
	struct device *dev;
	int err;

	dev = ice_pf_to_dev(pf);

	 
	if (delta > S32_MAX || delta < S32_MIN) {
		dev_dbg(dev, "delta = %lld, adjtime non-atomic\n", delta);
		return ice_ptp_adjtime_nonatomic(info, delta);
	}

	if (!ice_ptp_lock(hw)) {
		dev_err(dev, "PTP failed to acquire semaphore in adjtime\n");
		return -EBUSY;
	}

	 
	ice_ptp_disable_all_clkout(pf);

	err = ice_ptp_write_adj(pf, delta);

	 
	ice_ptp_enable_all_clkout(pf);

	ice_ptp_unlock(hw);

	if (err) {
		dev_err(dev, "PTP failed to adjust time, err %d\n", err);
		return err;
	}

	ice_ptp_reset_cached_phctime(pf);

	return 0;
}

#ifdef CONFIG_ICE_HWTS
 
static int
ice_ptp_get_syncdevicetime(ktime_t *device,
			   struct system_counterval_t *system,
			   void *ctx)
{
	struct ice_pf *pf = (struct ice_pf *)ctx;
	struct ice_hw *hw = &pf->hw;
	u32 hh_lock, hh_art_ctl;
	int i;

	 
	hh_lock = rd32(hw, PFHH_SEM + (PFTSYN_SEM_BYTES * hw->pf_id));
	if (hh_lock & PFHH_SEM_BUSY_M) {
		dev_err(ice_pf_to_dev(pf), "PTP failed to get hh lock\n");
		return -EFAULT;
	}

	 
	hh_art_ctl = rd32(hw, GLHH_ART_CTL);
	hh_art_ctl = hh_art_ctl | GLHH_ART_CTL_ACTIVE_M;
	wr32(hw, GLHH_ART_CTL, hh_art_ctl);

#define MAX_HH_LOCK_TRIES 100

	for (i = 0; i < MAX_HH_LOCK_TRIES; i++) {
		 
		hh_art_ctl = rd32(hw, GLHH_ART_CTL);
		if (hh_art_ctl & GLHH_ART_CTL_ACTIVE_M) {
			udelay(1);
			continue;
		} else {
			u32 hh_ts_lo, hh_ts_hi, tmr_idx;
			u64 hh_ts;

			tmr_idx = hw->func_caps.ts_func_info.tmr_index_assoc;
			 
			hh_ts_lo = rd32(hw, GLHH_ART_TIME_L);
			hh_ts_hi = rd32(hw, GLHH_ART_TIME_H);
			hh_ts = ((u64)hh_ts_hi << 32) | hh_ts_lo;
			*system = convert_art_ns_to_tsc(hh_ts);
			 
			hh_ts_lo = rd32(hw, GLTSYN_HHTIME_L(tmr_idx));
			hh_ts_hi = rd32(hw, GLTSYN_HHTIME_H(tmr_idx));
			hh_ts = ((u64)hh_ts_hi << 32) | hh_ts_lo;
			*device = ns_to_ktime(hh_ts);
			break;
		}
	}
	 
	hh_lock = rd32(hw, PFHH_SEM + (PFTSYN_SEM_BYTES * hw->pf_id));
	hh_lock = hh_lock & ~PFHH_SEM_BUSY_M;
	wr32(hw, PFHH_SEM + (PFTSYN_SEM_BYTES * hw->pf_id), hh_lock);

	if (i == MAX_HH_LOCK_TRIES)
		return -ETIMEDOUT;

	return 0;
}

 
static int
ice_ptp_getcrosststamp_e822(struct ptp_clock_info *info,
			    struct system_device_crosststamp *cts)
{
	struct ice_pf *pf = ptp_info_to_pf(info);

	return get_device_system_crosststamp(ice_ptp_get_syncdevicetime,
					     pf, NULL, cts);
}
#endif  

 
int ice_ptp_get_ts_config(struct ice_pf *pf, struct ifreq *ifr)
{
	struct hwtstamp_config *config;

	if (!test_bit(ICE_FLAG_PTP, pf->flags))
		return -EIO;

	config = &pf->ptp.tstamp_config;

	return copy_to_user(ifr->ifr_data, config, sizeof(*config)) ?
		-EFAULT : 0;
}

 
static int
ice_ptp_set_timestamp_mode(struct ice_pf *pf, struct hwtstamp_config *config)
{
	switch (config->tx_type) {
	case HWTSTAMP_TX_OFF:
		ice_set_tx_tstamp(pf, false);
		break;
	case HWTSTAMP_TX_ON:
		ice_set_tx_tstamp(pf, true);
		break;
	default:
		return -ERANGE;
	}

	switch (config->rx_filter) {
	case HWTSTAMP_FILTER_NONE:
		ice_set_rx_tstamp(pf, false);
		break;
	case HWTSTAMP_FILTER_PTP_V1_L4_EVENT:
	case HWTSTAMP_FILTER_PTP_V1_L4_SYNC:
	case HWTSTAMP_FILTER_PTP_V1_L4_DELAY_REQ:
	case HWTSTAMP_FILTER_PTP_V2_EVENT:
	case HWTSTAMP_FILTER_PTP_V2_L2_EVENT:
	case HWTSTAMP_FILTER_PTP_V2_L4_EVENT:
	case HWTSTAMP_FILTER_PTP_V2_SYNC:
	case HWTSTAMP_FILTER_PTP_V2_L2_SYNC:
	case HWTSTAMP_FILTER_PTP_V2_L4_SYNC:
	case HWTSTAMP_FILTER_PTP_V2_DELAY_REQ:
	case HWTSTAMP_FILTER_PTP_V2_L2_DELAY_REQ:
	case HWTSTAMP_FILTER_PTP_V2_L4_DELAY_REQ:
	case HWTSTAMP_FILTER_NTP_ALL:
	case HWTSTAMP_FILTER_ALL:
		ice_set_rx_tstamp(pf, true);
		break;
	default:
		return -ERANGE;
	}

	return 0;
}

 
int ice_ptp_set_ts_config(struct ice_pf *pf, struct ifreq *ifr)
{
	struct hwtstamp_config config;
	int err;

	if (!test_bit(ICE_FLAG_PTP, pf->flags))
		return -EAGAIN;

	if (copy_from_user(&config, ifr->ifr_data, sizeof(config)))
		return -EFAULT;

	err = ice_ptp_set_timestamp_mode(pf, &config);
	if (err)
		return err;

	 
	config = pf->ptp.tstamp_config;

	return copy_to_user(ifr->ifr_data, &config, sizeof(config)) ?
		-EFAULT : 0;
}

 
void
ice_ptp_rx_hwtstamp(struct ice_rx_ring *rx_ring,
		    union ice_32b_rx_flex_desc *rx_desc, struct sk_buff *skb)
{
	struct skb_shared_hwtstamps *hwtstamps;
	u64 ts_ns, cached_time;
	u32 ts_high;

	if (!(rx_desc->wb.time_stamp_low & ICE_PTP_TS_VALID))
		return;

	cached_time = READ_ONCE(rx_ring->cached_phctime);

	 
	if (!cached_time)
		return;

	 
	ts_high = le32_to_cpu(rx_desc->wb.flex_ts.ts_high);
	ts_ns = ice_ptp_extend_32b_ts(cached_time, ts_high);

	hwtstamps = skb_hwtstamps(skb);
	memset(hwtstamps, 0, sizeof(*hwtstamps));
	hwtstamps->hwtstamp = ns_to_ktime(ts_ns);
}

 
static void
ice_ptp_disable_sma_pins_e810t(struct ice_pf *pf, struct ptp_clock_info *info)
{
	struct device *dev = ice_pf_to_dev(pf);

	dev_warn(dev, "Failed to configure E810-T SMA pin control\n");

	info->enable = NULL;
	info->verify = NULL;
	info->n_pins = 0;
	info->n_ext_ts = 0;
	info->n_per_out = 0;
}

 
static void
ice_ptp_setup_sma_pins_e810t(struct ice_pf *pf, struct ptp_clock_info *info)
{
	struct device *dev = ice_pf_to_dev(pf);
	int err;

	 
	info->pin_config = devm_kcalloc(dev, info->n_pins,
					sizeof(*info->pin_config), GFP_KERNEL);
	if (!info->pin_config) {
		ice_ptp_disable_sma_pins_e810t(pf, info);
		return;
	}

	 
	err = ice_get_sma_config_e810t(&pf->hw, info->pin_config);
	if (err)
		ice_ptp_disable_sma_pins_e810t(pf, info);
}

 
static void
ice_ptp_setup_pins_e810(struct ice_pf *pf, struct ptp_clock_info *info)
{
	if (ice_is_feature_supported(pf, ICE_F_SMA_CTRL)) {
		info->n_ext_ts = N_EXT_TS_E810;
		info->n_per_out = N_PER_OUT_E810T;
		info->n_pins = NUM_PTP_PINS_E810T;
		info->verify = ice_verify_pin_e810t;

		 
		ice_ptp_setup_sma_pins_e810t(pf, info);
	} else if (ice_is_e810t(&pf->hw)) {
		info->n_ext_ts = N_EXT_TS_NO_SMA_E810T;
		info->n_per_out = N_PER_OUT_NO_SMA_E810T;
	} else {
		info->n_per_out = N_PER_OUT_E810;
		info->n_ext_ts = N_EXT_TS_E810;
	}
}

 
static void
ice_ptp_setup_pins_e823(struct ice_pf *pf, struct ptp_clock_info *info)
{
	info->pps = 1;
	info->n_per_out = 0;
	info->n_ext_ts = 1;
}

 
static void
ice_ptp_set_funcs_e822(struct ice_pf *pf, struct ptp_clock_info *info)
{
#ifdef CONFIG_ICE_HWTS
	if (boot_cpu_has(X86_FEATURE_ART) &&
	    boot_cpu_has(X86_FEATURE_TSC_KNOWN_FREQ))
		info->getcrosststamp = ice_ptp_getcrosststamp_e822;
#endif  
}

 
static void
ice_ptp_set_funcs_e810(struct ice_pf *pf, struct ptp_clock_info *info)
{
	info->enable = ice_ptp_gpio_enable_e810;
	ice_ptp_setup_pins_e810(pf, info);
}

 
static void
ice_ptp_set_funcs_e823(struct ice_pf *pf, struct ptp_clock_info *info)
{
	info->enable = ice_ptp_gpio_enable_e823;
	ice_ptp_setup_pins_e823(pf, info);
}

 
static void ice_ptp_set_caps(struct ice_pf *pf)
{
	struct ptp_clock_info *info = &pf->ptp.info;
	struct device *dev = ice_pf_to_dev(pf);

	snprintf(info->name, sizeof(info->name) - 1, "%s-%s-clk",
		 dev_driver_string(dev), dev_name(dev));
	info->owner = THIS_MODULE;
	info->max_adj = 100000000;
	info->adjtime = ice_ptp_adjtime;
	info->adjfine = ice_ptp_adjfine;
	info->gettimex64 = ice_ptp_gettimex64;
	info->settime64 = ice_ptp_settime64;

	if (ice_is_e810(&pf->hw))
		ice_ptp_set_funcs_e810(pf, info);
	else if (ice_is_e823(&pf->hw))
		ice_ptp_set_funcs_e823(pf, info);
	else
		ice_ptp_set_funcs_e822(pf, info);
}

 
static long ice_ptp_create_clock(struct ice_pf *pf)
{
	struct ptp_clock_info *info;
	struct ptp_clock *clock;
	struct device *dev;

	 
	if (pf->ptp.clock)
		return 0;

	ice_ptp_set_caps(pf);

	info = &pf->ptp.info;
	dev = ice_pf_to_dev(pf);

	 
	clock = ptp_clock_register(info, dev);
	if (IS_ERR(clock))
		return PTR_ERR(clock);

	pf->ptp.clock = clock;

	return 0;
}

 
s8 ice_ptp_request_ts(struct ice_ptp_tx *tx, struct sk_buff *skb)
{
	u8 idx;

	spin_lock(&tx->lock);

	 
	if (!ice_ptp_is_tx_tracker_up(tx)) {
		spin_unlock(&tx->lock);
		return -1;
	}

	 
	idx = find_first_zero_bit(tx->in_use, tx->len);
	if (idx < tx->len) {
		 
		set_bit(idx, tx->in_use);
		clear_bit(idx, tx->stale);
		tx->tstamps[idx].start = jiffies;
		tx->tstamps[idx].skb = skb_get(skb);
		skb_shinfo(skb)->tx_flags |= SKBTX_IN_PROGRESS;
		ice_trace(tx_tstamp_request, skb, idx);
	}

	spin_unlock(&tx->lock);

	 
	if (idx >= tx->len)
		return -1;
	else
		return idx + tx->offset;
}

 
enum ice_tx_tstamp_work ice_ptp_process_ts(struct ice_pf *pf)
{
	return ice_ptp_tx_tstamp(&pf->ptp.port.tx);
}

static void ice_ptp_periodic_work(struct kthread_work *work)
{
	struct ice_ptp *ptp = container_of(work, struct ice_ptp, work.work);
	struct ice_pf *pf = container_of(ptp, struct ice_pf, ptp);
	int err;

	if (!test_bit(ICE_FLAG_PTP, pf->flags))
		return;

	err = ice_ptp_update_cached_phctime(pf);

	 
	kthread_queue_delayed_work(ptp->kworker, &ptp->work,
				   msecs_to_jiffies(err ? 10 : 500));
}

 
void ice_ptp_reset(struct ice_pf *pf)
{
	struct ice_ptp *ptp = &pf->ptp;
	struct ice_hw *hw = &pf->hw;
	struct timespec64 ts;
	int err, itr = 1;
	u64 time_diff;

	if (test_bit(ICE_PFR_REQ, pf->state))
		goto pfr;

	if (!hw->func_caps.ts_func_info.src_tmr_owned)
		goto reset_ts;

	err = ice_ptp_init_phc(hw);
	if (err)
		goto err;

	 
	if (!ice_ptp_lock(hw)) {
		err = -EBUSY;
		goto err;
	}

	 
	err = ice_ptp_write_incval(hw, ice_base_incval(pf));
	if (err) {
		ice_ptp_unlock(hw);
		goto err;
	}

	 
	if (ptp->cached_phc_time) {
		time_diff = ktime_get_real_ns() - ptp->reset_time;
		ts = ns_to_timespec64(ptp->cached_phc_time + time_diff);
	} else {
		ts = ktime_to_timespec64(ktime_get_real());
	}
	err = ice_ptp_write_init(pf, &ts);
	if (err) {
		ice_ptp_unlock(hw);
		goto err;
	}

	 
	ice_ptp_unlock(hw);

	if (!ice_is_e810(hw)) {
		 
		err = ice_ptp_tx_ena_intr(pf, true, itr);
		if (err)
			goto err;
	}

reset_ts:
	 
	ice_ptp_reset_phy_timestamping(pf);

pfr:
	 
	if (ice_is_e810(&pf->hw)) {
		err = ice_ptp_init_tx_e810(pf, &ptp->port.tx);
	} else {
		kthread_init_delayed_work(&ptp->port.ov_work,
					  ice_ptp_wait_for_offsets);
		err = ice_ptp_init_tx_e822(pf, &ptp->port.tx,
					   ptp->port.port_num);
	}
	if (err)
		goto err;

	set_bit(ICE_FLAG_PTP, pf->flags);

	 
	kthread_queue_delayed_work(ptp->kworker, &ptp->work, 0);

	dev_info(ice_pf_to_dev(pf), "PTP reset successful\n");
	return;

err:
	dev_err(ice_pf_to_dev(pf), "PTP reset failed %d\n", err);
}

 
void ice_ptp_prepare_for_reset(struct ice_pf *pf)
{
	struct ice_ptp *ptp = &pf->ptp;
	u8 src_tmr;

	clear_bit(ICE_FLAG_PTP, pf->flags);

	 
	ice_ptp_cfg_timestamp(pf, false);

	kthread_cancel_delayed_work_sync(&ptp->work);

	if (test_bit(ICE_PFR_REQ, pf->state))
		return;

	ice_ptp_release_tx_tracker(pf, &pf->ptp.port.tx);

	 
	ice_ptp_disable_all_clkout(pf);

	src_tmr = ice_get_ptp_src_clock_index(&pf->hw);

	 
	wr32(&pf->hw, GLTSYN_ENA(src_tmr), (u32)~GLTSYN_ENA_TSYN_ENA_M);

	 
	ptp->reset_time = ktime_get_real_ns();
}

 
static int ice_ptp_init_owner(struct ice_pf *pf)
{
	struct ice_hw *hw = &pf->hw;
	struct timespec64 ts;
	int err, itr = 1;

	err = ice_ptp_init_phc(hw);
	if (err) {
		dev_err(ice_pf_to_dev(pf), "Failed to initialize PHC, err %d\n",
			err);
		return err;
	}

	 
	if (!ice_ptp_lock(hw)) {
		err = -EBUSY;
		goto err_exit;
	}

	 
	err = ice_ptp_write_incval(hw, ice_base_incval(pf));
	if (err) {
		ice_ptp_unlock(hw);
		goto err_exit;
	}

	ts = ktime_to_timespec64(ktime_get_real());
	 
	err = ice_ptp_write_init(pf, &ts);
	if (err) {
		ice_ptp_unlock(hw);
		goto err_exit;
	}

	 
	ice_ptp_unlock(hw);

	if (!ice_is_e810(hw)) {
		 
		err = ice_ptp_tx_ena_intr(pf, true, itr);
		if (err)
			goto err_exit;
	}

	 
	err = ice_ptp_create_clock(pf);
	if (err)
		goto err_clk;

	 
	ice_set_ptp_clock_index(pf);

	return 0;

err_clk:
	pf->ptp.clock = NULL;
err_exit:
	return err;
}

 
static int ice_ptp_init_work(struct ice_pf *pf, struct ice_ptp *ptp)
{
	struct kthread_worker *kworker;

	 
	kthread_init_delayed_work(&ptp->work, ice_ptp_periodic_work);

	 
	kworker = kthread_create_worker(0, "ice-ptp-%s",
					dev_name(ice_pf_to_dev(pf)));
	if (IS_ERR(kworker))
		return PTR_ERR(kworker);

	ptp->kworker = kworker;

	 
	kthread_queue_delayed_work(ptp->kworker, &ptp->work, 0);

	return 0;
}

 
static int ice_ptp_init_port(struct ice_pf *pf, struct ice_ptp_port *ptp_port)
{
	mutex_init(&ptp_port->ps_lock);

	if (ice_is_e810(&pf->hw))
		return ice_ptp_init_tx_e810(pf, &ptp_port->tx);

	kthread_init_delayed_work(&ptp_port->ov_work,
				  ice_ptp_wait_for_offsets);
	return ice_ptp_init_tx_e822(pf, &ptp_port->tx, ptp_port->port_num);
}

 
void ice_ptp_init(struct ice_pf *pf)
{
	struct ice_ptp *ptp = &pf->ptp;
	struct ice_hw *hw = &pf->hw;
	int err;

	 
	if (hw->func_caps.ts_func_info.src_tmr_owned) {
		err = ice_ptp_init_owner(pf);
		if (err)
			goto err;
	}

	ptp->port.port_num = hw->pf_id;
	err = ice_ptp_init_port(pf, &ptp->port);
	if (err)
		goto err;

	 
	ice_ptp_reset_phy_timestamping(pf);

	set_bit(ICE_FLAG_PTP, pf->flags);
	err = ice_ptp_init_work(pf, ptp);
	if (err)
		goto err;

	dev_info(ice_pf_to_dev(pf), "PTP init successful\n");
	return;

err:
	 
	if (pf->ptp.clock) {
		ptp_clock_unregister(ptp->clock);
		pf->ptp.clock = NULL;
	}
	clear_bit(ICE_FLAG_PTP, pf->flags);
	dev_err(ice_pf_to_dev(pf), "PTP failed %d\n", err);
}

 
void ice_ptp_release(struct ice_pf *pf)
{
	if (!test_bit(ICE_FLAG_PTP, pf->flags))
		return;

	 
	ice_ptp_cfg_timestamp(pf, false);

	ice_ptp_release_tx_tracker(pf, &pf->ptp.port.tx);

	clear_bit(ICE_FLAG_PTP, pf->flags);

	kthread_cancel_delayed_work_sync(&pf->ptp.work);

	ice_ptp_port_phy_stop(&pf->ptp.port);
	mutex_destroy(&pf->ptp.port.ps_lock);
	if (pf->ptp.kworker) {
		kthread_destroy_worker(pf->ptp.kworker);
		pf->ptp.kworker = NULL;
	}

	if (!pf->ptp.clock)
		return;

	 
	ice_ptp_disable_all_clkout(pf);

	ice_clear_ptp_clock_index(pf);
	ptp_clock_unregister(pf->ptp.clock);
	pf->ptp.clock = NULL;

	dev_info(ice_pf_to_dev(pf), "Removed PTP clock\n");
}
