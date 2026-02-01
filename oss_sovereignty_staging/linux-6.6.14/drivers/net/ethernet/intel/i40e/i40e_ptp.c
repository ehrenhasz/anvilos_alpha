
 

#include "i40e.h"
#include <linux/ptp_classify.h>
#include <linux/posix-clock.h>

 
#define I40E_PTP_40GB_INCVAL		0x0199999999ULL
#define I40E_PTP_10GB_INCVAL_MULT	2
#define I40E_PTP_5GB_INCVAL_MULT	2
#define I40E_PTP_1GB_INCVAL_MULT	20
#define I40E_ISGN			0x80000000

#define I40E_PRTTSYN_CTL1_TSYNTYPE_V1  BIT(I40E_PRTTSYN_CTL1_TSYNTYPE_SHIFT)
#define I40E_PRTTSYN_CTL1_TSYNTYPE_V2  (2 << \
					I40E_PRTTSYN_CTL1_TSYNTYPE_SHIFT)
#define I40E_SUBDEV_ID_25G_PTP_PIN	0xB

enum i40e_ptp_pin {
	SDP3_2 = 0,
	SDP3_3,
	GPIO_4
};

enum i40e_can_set_pins_t {
	CANT_DO_PINS = -1,
	CAN_SET_PINS,
	CAN_DO_PINS
};

static struct ptp_pin_desc sdp_desc[] = {
	 
	{"SDP3_2", SDP3_2, PTP_PF_NONE, 0},
	{"SDP3_3", SDP3_3, PTP_PF_NONE, 1},
	{"GPIO_4", GPIO_4, PTP_PF_NONE, 1},
};

enum i40e_ptp_gpio_pin_state {
	end = -2,
	invalid,
	off,
	in_A,
	in_B,
	out_A,
	out_B,
};

static const char * const i40e_ptp_gpio_pin_state2str[] = {
	"off", "in_A", "in_B", "out_A", "out_B"
};

enum i40e_ptp_led_pin_state {
	led_end = -2,
	low = 0,
	high,
};

struct i40e_ptp_pins_settings {
	enum i40e_ptp_gpio_pin_state sdp3_2;
	enum i40e_ptp_gpio_pin_state sdp3_3;
	enum i40e_ptp_gpio_pin_state gpio_4;
	enum i40e_ptp_led_pin_state led2_0;
	enum i40e_ptp_led_pin_state led2_1;
	enum i40e_ptp_led_pin_state led3_0;
	enum i40e_ptp_led_pin_state led3_1;
};

static const struct i40e_ptp_pins_settings
	i40e_ptp_pin_led_allowed_states[] = {
	{off,	off,	off,		high,	high,	high,	high},
	{off,	in_A,	off,		high,	high,	high,	low},
	{off,	out_A,	off,		high,	low,	high,	high},
	{off,	in_B,	off,		high,	high,	high,	low},
	{off,	out_B,	off,		high,	low,	high,	high},
	{in_A,	off,	off,		high,	high,	high,	low},
	{in_A,	in_B,	off,		high,	high,	high,	low},
	{in_A,	out_B,	off,		high,	low,	high,	high},
	{out_A,	off,	off,		high,	low,	high,	high},
	{out_A,	in_B,	off,		high,	low,	high,	high},
	{in_B,	off,	off,		high,	high,	high,	low},
	{in_B,	in_A,	off,		high,	high,	high,	low},
	{in_B,	out_A,	off,		high,	low,	high,	high},
	{out_B,	off,	off,		high,	low,	high,	high},
	{out_B,	in_A,	off,		high,	low,	high,	high},
	{off,	off,	in_A,		high,	high,	low,	high},
	{off,	out_A,	in_A,		high,	low,	low,	high},
	{off,	in_B,	in_A,		high,	high,	low,	low},
	{off,	out_B,	in_A,		high,	low,	low,	high},
	{out_A,	off,	in_A,		high,	low,	low,	high},
	{out_A,	in_B,	in_A,		high,	low,	low,	high},
	{in_B,	off,	in_A,		high,	high,	low,	low},
	{in_B,	out_A,	in_A,		high,	low,	low,	high},
	{out_B,	off,	in_A,		high,	low,	low,	high},
	{off,	off,	out_A,		low,	high,	high,	high},
	{off,	in_A,	out_A,		low,	high,	high,	low},
	{off,	in_B,	out_A,		low,	high,	high,	low},
	{off,	out_B,	out_A,		low,	low,	high,	high},
	{in_A,	off,	out_A,		low,	high,	high,	low},
	{in_A,	in_B,	out_A,		low,	high,	high,	low},
	{in_A,	out_B,	out_A,		low,	low,	high,	high},
	{in_B,	off,	out_A,		low,	high,	high,	low},
	{in_B,	in_A,	out_A,		low,	high,	high,	low},
	{out_B,	off,	out_A,		low,	low,	high,	high},
	{out_B,	in_A,	out_A,		low,	low,	high,	high},
	{off,	off,	in_B,		high,	high,	low,	high},
	{off,	in_A,	in_B,		high,	high,	low,	low},
	{off,	out_A,	in_B,		high,	low,	low,	high},
	{off,	out_B,	in_B,		high,	low,	low,	high},
	{in_A,	off,	in_B,		high,	high,	low,	low},
	{in_A,	out_B,	in_B,		high,	low,	low,	high},
	{out_A,	off,	in_B,		high,	low,	low,	high},
	{out_B,	off,	in_B,		high,	low,	low,	high},
	{out_B,	in_A,	in_B,		high,	low,	low,	high},
	{off,	off,	out_B,		low,	high,	high,	high},
	{off,	in_A,	out_B,		low,	high,	high,	low},
	{off,	out_A,	out_B,		low,	low,	high,	high},
	{off,	in_B,	out_B,		low,	high,	high,	low},
	{in_A,	off,	out_B,		low,	high,	high,	low},
	{in_A,	in_B,	out_B,		low,	high,	high,	low},
	{out_A,	off,	out_B,		low,	low,	high,	high},
	{out_A,	in_B,	out_B,		low,	low,	high,	high},
	{in_B,	off,	out_B,		low,	high,	high,	low},
	{in_B,	in_A,	out_B,		low,	high,	high,	low},
	{in_B,	out_A,	out_B,		low,	low,	high,	high},
	{end,	end,	end,	led_end, led_end, led_end, led_end}
};

static int i40e_ptp_set_pins(struct i40e_pf *pf,
			     struct i40e_ptp_pins_settings *pins);

 
static void i40e_ptp_extts0_work(struct work_struct *work)
{
	struct i40e_pf *pf = container_of(work, struct i40e_pf,
					  ptp_extts0_work);
	struct i40e_hw *hw = &pf->hw;
	struct ptp_clock_event event;
	u32 hi, lo;

	 
	lo = rd32(hw, I40E_PRTTSYN_EVNT_L(0));
	hi = rd32(hw, I40E_PRTTSYN_EVNT_H(0));

	event.timestamp = (((u64)hi) << 32) | lo;

	event.type = PTP_CLOCK_EXTTS;
	event.index = hw->pf_id;

	 
	ptp_clock_event(pf->ptp_clock, &event);
}

 
static bool i40e_is_ptp_pin_dev(struct i40e_hw *hw)
{
	return hw->device_id == I40E_DEV_ID_25G_SFP28 &&
	       hw->subsystem_device_id == I40E_SUBDEV_ID_25G_PTP_PIN;
}

 
static enum i40e_can_set_pins_t i40e_can_set_pins(struct i40e_pf *pf)
{
	if (!i40e_is_ptp_pin_dev(&pf->hw)) {
		dev_warn(&pf->pdev->dev,
			 "PTP external clock not supported.\n");
		return CANT_DO_PINS;
	}

	if (!pf->ptp_pins) {
		dev_warn(&pf->pdev->dev,
			 "PTP PIN manipulation not allowed.\n");
		return CANT_DO_PINS;
	}

	if (pf->hw.pf_id) {
		dev_warn(&pf->pdev->dev,
			 "PTP PINs should be accessed via PF0.\n");
		return CAN_DO_PINS;
	}

	return CAN_SET_PINS;
}

 
static void i40_ptp_reset_timing_events(struct i40e_pf *pf)
{
	u32 i;

	spin_lock_bh(&pf->ptp_rx_lock);
	for (i = 0; i <= I40E_PRTTSYN_RXTIME_L_MAX_INDEX; i++) {
		 
		rd32(&pf->hw, I40E_PRTTSYN_RXTIME_L(i));
		rd32(&pf->hw, I40E_PRTTSYN_RXTIME_H(i));
		pf->latch_events[i] = 0;
	}
	 
	rd32(&pf->hw, I40E_PRTTSYN_TXTIME_L);
	rd32(&pf->hw, I40E_PRTTSYN_TXTIME_H);

	pf->tx_hwtstamp_timeouts = 0;
	pf->tx_hwtstamp_skipped = 0;
	pf->rx_hwtstamp_cleared = 0;
	pf->latch_event_flags = 0;
	spin_unlock_bh(&pf->ptp_rx_lock);
}

 
static int i40e_ptp_verify(struct ptp_clock_info *ptp, unsigned int pin,
			   enum ptp_pin_function func, unsigned int chan)
{
	switch (func) {
	case PTP_PF_NONE:
	case PTP_PF_EXTTS:
	case PTP_PF_PEROUT:
		break;
	case PTP_PF_PHYSYNC:
		return -EOPNOTSUPP;
	}
	return 0;
}

 
static void i40e_ptp_read(struct i40e_pf *pf, struct timespec64 *ts,
			  struct ptp_system_timestamp *sts)
{
	struct i40e_hw *hw = &pf->hw;
	u32 hi, lo;
	u64 ns;

	 
	ptp_read_system_prets(sts);
	lo = rd32(hw, I40E_PRTTSYN_TIME_L);
	ptp_read_system_postts(sts);
	hi = rd32(hw, I40E_PRTTSYN_TIME_H);

	ns = (((u64)hi) << 32) | lo;

	*ts = ns_to_timespec64(ns);
}

 
static void i40e_ptp_write(struct i40e_pf *pf, const struct timespec64 *ts)
{
	struct i40e_hw *hw = &pf->hw;
	u64 ns = timespec64_to_ns(ts);

	 
	wr32(hw, I40E_PRTTSYN_TIME_L, ns & 0xFFFFFFFF);
	wr32(hw, I40E_PRTTSYN_TIME_H, ns >> 32);
}

 
static void i40e_ptp_convert_to_hwtstamp(struct skb_shared_hwtstamps *hwtstamps,
					 u64 timestamp)
{
	memset(hwtstamps, 0, sizeof(*hwtstamps));

	hwtstamps->hwtstamp = ns_to_ktime(timestamp);
}

 
static int i40e_ptp_adjfine(struct ptp_clock_info *ptp, long scaled_ppm)
{
	struct i40e_pf *pf = container_of(ptp, struct i40e_pf, ptp_caps);
	struct i40e_hw *hw = &pf->hw;
	u64 adj, base_adj;

	smp_mb();  
	base_adj = I40E_PTP_40GB_INCVAL * READ_ONCE(pf->ptp_adj_mult);

	adj = adjust_by_scaled_ppm(base_adj, scaled_ppm);

	wr32(hw, I40E_PRTTSYN_INC_L, adj & 0xFFFFFFFF);
	wr32(hw, I40E_PRTTSYN_INC_H, adj >> 32);

	return 0;
}

 
static void i40e_ptp_set_1pps_signal_hw(struct i40e_pf *pf)
{
	struct i40e_hw *hw = &pf->hw;
	struct timespec64 now;
	u64 ns;

	wr32(hw, I40E_PRTTSYN_AUX_0(1), 0);
	wr32(hw, I40E_PRTTSYN_AUX_1(1), I40E_PRTTSYN_AUX_1_INSTNT);
	wr32(hw, I40E_PRTTSYN_AUX_0(1), I40E_PRTTSYN_AUX_0_OUT_ENABLE);

	i40e_ptp_read(pf, &now, NULL);
	now.tv_sec += I40E_PTP_2_SEC_DELAY;
	now.tv_nsec = 0;
	ns = timespec64_to_ns(&now);

	 
	wr32(hw, I40E_PRTTSYN_TGT_L(1), ns & 0xFFFFFFFF);
	 
	wr32(hw, I40E_PRTTSYN_TGT_H(1), ns >> 32);
	wr32(hw, I40E_PRTTSYN_CLKO(1), I40E_PTP_HALF_SECOND);
	wr32(hw, I40E_PRTTSYN_AUX_1(1), I40E_PRTTSYN_AUX_1_INSTNT);
	wr32(hw, I40E_PRTTSYN_AUX_0(1),
	     I40E_PRTTSYN_AUX_0_OUT_ENABLE_CLK_MOD);
}

 
static int i40e_ptp_adjtime(struct ptp_clock_info *ptp, s64 delta)
{
	struct i40e_pf *pf = container_of(ptp, struct i40e_pf, ptp_caps);
	struct i40e_hw *hw = &pf->hw;

	mutex_lock(&pf->tmreg_lock);

	if (delta > -999999900LL && delta < 999999900LL) {
		int neg_adj = 0;
		u32 timadj;
		u64 tohw;

		if (delta < 0) {
			neg_adj = 1;
			tohw = -delta;
		} else {
			tohw = delta;
		}

		timadj = tohw & 0x3FFFFFFF;
		if (neg_adj)
			timadj |= I40E_ISGN;
		wr32(hw, I40E_PRTTSYN_ADJ, timadj);
	} else {
		struct timespec64 then, now;

		then = ns_to_timespec64(delta);
		i40e_ptp_read(pf, &now, NULL);
		now = timespec64_add(now, then);
		i40e_ptp_write(pf, (const struct timespec64 *)&now);
		i40e_ptp_set_1pps_signal_hw(pf);
	}

	mutex_unlock(&pf->tmreg_lock);

	return 0;
}

 
static int i40e_ptp_gettimex(struct ptp_clock_info *ptp, struct timespec64 *ts,
			     struct ptp_system_timestamp *sts)
{
	struct i40e_pf *pf = container_of(ptp, struct i40e_pf, ptp_caps);

	mutex_lock(&pf->tmreg_lock);
	i40e_ptp_read(pf, ts, sts);
	mutex_unlock(&pf->tmreg_lock);

	return 0;
}

 
static int i40e_ptp_settime(struct ptp_clock_info *ptp,
			    const struct timespec64 *ts)
{
	struct i40e_pf *pf = container_of(ptp, struct i40e_pf, ptp_caps);

	mutex_lock(&pf->tmreg_lock);
	i40e_ptp_write(pf, ts);
	mutex_unlock(&pf->tmreg_lock);

	return 0;
}

 
static int i40e_pps_configure(struct ptp_clock_info *ptp,
			      struct ptp_clock_request *rq,
			      int on)
{
	struct i40e_pf *pf = container_of(ptp, struct i40e_pf, ptp_caps);

	if (!!on)
		i40e_ptp_set_1pps_signal_hw(pf);

	return 0;
}

 
static enum i40e_ptp_gpio_pin_state i40e_pin_state(int index, int func)
{
	enum i40e_ptp_gpio_pin_state state = off;

	if (index == 0 && func == PTP_PF_EXTTS)
		state = in_A;
	if (index == 1 && func == PTP_PF_EXTTS)
		state = in_B;
	if (index == 0 && func == PTP_PF_PEROUT)
		state = out_A;
	if (index == 1 && func == PTP_PF_PEROUT)
		state = out_B;

	return state;
}

 
static int i40e_ptp_enable_pin(struct i40e_pf *pf, unsigned int chan,
			       enum ptp_pin_function func, int on)
{
	enum i40e_ptp_gpio_pin_state *pin = NULL;
	struct i40e_ptp_pins_settings pins;
	int pin_index;

	 
	if (pf->hw.pf_id)
		return 0;

	 
	pins.sdp3_2 = pf->ptp_pins->sdp3_2;
	pins.sdp3_3 = pf->ptp_pins->sdp3_3;
	pins.gpio_4 = pf->ptp_pins->gpio_4;

	 
	if (on) {
		pin_index = ptp_find_pin(pf->ptp_clock, func, chan);
		if (pin_index < 0)
			return -EBUSY;

		switch (pin_index) {
		case SDP3_2:
			pin = &pins.sdp3_2;
			break;
		case SDP3_3:
			pin = &pins.sdp3_3;
			break;
		case GPIO_4:
			pin = &pins.gpio_4;
			break;
		default:
			return -EINVAL;
		}

		*pin = i40e_pin_state(chan, func);
	} else {
		pins.sdp3_2 = off;
		pins.sdp3_3 = off;
		pins.gpio_4 = off;
	}

	return i40e_ptp_set_pins(pf, &pins) ? -EINVAL : 0;
}

 
static int i40e_ptp_feature_enable(struct ptp_clock_info *ptp,
				   struct ptp_clock_request *rq,
				   int on)
{
	struct i40e_pf *pf = container_of(ptp, struct i40e_pf, ptp_caps);

	enum ptp_pin_function func;
	unsigned int chan;

	 
	switch (rq->type) {
	case PTP_CLK_REQ_EXTTS:
		func = PTP_PF_EXTTS;
		chan = rq->extts.index;
		break;
	case PTP_CLK_REQ_PEROUT:
		func = PTP_PF_PEROUT;
		chan = rq->perout.index;
		break;
	case PTP_CLK_REQ_PPS:
		return i40e_pps_configure(ptp, rq, on);
	default:
		return -EOPNOTSUPP;
	}

	return i40e_ptp_enable_pin(pf, chan, func, on);
}

 
static u32 i40e_ptp_get_rx_events(struct i40e_pf *pf)
{
	struct i40e_hw *hw = &pf->hw;
	u32 prttsyn_stat, new_latch_events;
	int  i;

	prttsyn_stat = rd32(hw, I40E_PRTTSYN_STAT_1);
	new_latch_events = prttsyn_stat & ~pf->latch_event_flags;

	 
	for (i = 0; i < 4; i++) {
		if (new_latch_events & BIT(i))
			pf->latch_events[i] = jiffies;
	}

	 
	pf->latch_event_flags = prttsyn_stat;

	return prttsyn_stat;
}

 
void i40e_ptp_rx_hang(struct i40e_pf *pf)
{
	struct i40e_hw *hw = &pf->hw;
	unsigned int i, cleared = 0;

	 
	if (!(pf->flags & I40E_FLAG_PTP) || !pf->ptp_rx)
		return;

	spin_lock_bh(&pf->ptp_rx_lock);

	 
	i40e_ptp_get_rx_events(pf);

	 
	for (i = 0; i < 4; i++) {
		if ((pf->latch_event_flags & BIT(i)) &&
		    time_is_before_jiffies(pf->latch_events[i] + HZ)) {
			rd32(hw, I40E_PRTTSYN_RXTIME_H(i));
			pf->latch_event_flags &= ~BIT(i);
			cleared++;
		}
	}

	spin_unlock_bh(&pf->ptp_rx_lock);

	 
	if (cleared > 2)
		dev_dbg(&pf->pdev->dev,
			"Dropped %d missed RXTIME timestamp events\n",
			cleared);

	 
	pf->rx_hwtstamp_cleared += cleared;
}

 
void i40e_ptp_tx_hang(struct i40e_pf *pf)
{
	struct sk_buff *skb;

	if (!(pf->flags & I40E_FLAG_PTP) || !pf->ptp_tx)
		return;

	 
	if (!test_bit(__I40E_PTP_TX_IN_PROGRESS, pf->state))
		return;

	 
	if (time_is_before_jiffies(pf->ptp_tx_start + HZ)) {
		skb = pf->ptp_tx_skb;
		pf->ptp_tx_skb = NULL;
		clear_bit_unlock(__I40E_PTP_TX_IN_PROGRESS, pf->state);

		 
		dev_kfree_skb_any(skb);
		pf->tx_hwtstamp_timeouts++;
	}
}

 
void i40e_ptp_tx_hwtstamp(struct i40e_pf *pf)
{
	struct skb_shared_hwtstamps shhwtstamps;
	struct sk_buff *skb = pf->ptp_tx_skb;
	struct i40e_hw *hw = &pf->hw;
	u32 hi, lo;
	u64 ns;

	if (!(pf->flags & I40E_FLAG_PTP) || !pf->ptp_tx)
		return;

	 
	if (!pf->ptp_tx_skb)
		return;

	lo = rd32(hw, I40E_PRTTSYN_TXTIME_L);
	hi = rd32(hw, I40E_PRTTSYN_TXTIME_H);

	ns = (((u64)hi) << 32) | lo;
	i40e_ptp_convert_to_hwtstamp(&shhwtstamps, ns);

	 
	pf->ptp_tx_skb = NULL;
	clear_bit_unlock(__I40E_PTP_TX_IN_PROGRESS, pf->state);

	 
	skb_tstamp_tx(skb, &shhwtstamps);
	dev_kfree_skb_any(skb);
}

 
void i40e_ptp_rx_hwtstamp(struct i40e_pf *pf, struct sk_buff *skb, u8 index)
{
	u32 prttsyn_stat, hi, lo;
	struct i40e_hw *hw;
	u64 ns;

	 
	if (!(pf->flags & I40E_FLAG_PTP) || !pf->ptp_rx)
		return;

	hw = &pf->hw;

	spin_lock_bh(&pf->ptp_rx_lock);

	 
	prttsyn_stat = i40e_ptp_get_rx_events(pf);

	 
	if (!(prttsyn_stat & BIT(index))) {
		spin_unlock_bh(&pf->ptp_rx_lock);
		return;
	}

	 
	pf->latch_event_flags &= ~BIT(index);

	lo = rd32(hw, I40E_PRTTSYN_RXTIME_L(index));
	hi = rd32(hw, I40E_PRTTSYN_RXTIME_H(index));

	spin_unlock_bh(&pf->ptp_rx_lock);

	ns = (((u64)hi) << 32) | lo;

	i40e_ptp_convert_to_hwtstamp(skb_hwtstamps(skb), ns);
}

 
void i40e_ptp_set_increment(struct i40e_pf *pf)
{
	struct i40e_link_status *hw_link_info;
	struct i40e_hw *hw = &pf->hw;
	u64 incval;
	u32 mult;

	hw_link_info = &hw->phy.link_info;

	i40e_aq_get_link_info(&pf->hw, true, NULL, NULL);

	switch (hw_link_info->link_speed) {
	case I40E_LINK_SPEED_10GB:
		mult = I40E_PTP_10GB_INCVAL_MULT;
		break;
	case I40E_LINK_SPEED_5GB:
		mult = I40E_PTP_5GB_INCVAL_MULT;
		break;
	case I40E_LINK_SPEED_1GB:
		mult = I40E_PTP_1GB_INCVAL_MULT;
		break;
	case I40E_LINK_SPEED_100MB:
	{
		static int warn_once;

		if (!warn_once) {
			dev_warn(&pf->pdev->dev,
				 "1588 functionality is not supported at 100 Mbps. Stopping the PHC.\n");
			warn_once++;
		}
		mult = 0;
		break;
	}
	case I40E_LINK_SPEED_40GB:
	default:
		mult = 1;
		break;
	}

	 
	incval = I40E_PTP_40GB_INCVAL * mult;

	 
	wr32(hw, I40E_PRTTSYN_INC_L, incval & 0xFFFFFFFF);
	wr32(hw, I40E_PRTTSYN_INC_H, incval >> 32);

	 
	WRITE_ONCE(pf->ptp_adj_mult, mult);
	smp_mb();  
}

 
int i40e_ptp_get_ts_config(struct i40e_pf *pf, struct ifreq *ifr)
{
	struct hwtstamp_config *config = &pf->tstamp_config;

	if (!(pf->flags & I40E_FLAG_PTP))
		return -EOPNOTSUPP;

	return copy_to_user(ifr->ifr_data, config, sizeof(*config)) ?
		-EFAULT : 0;
}

 
static void i40e_ptp_free_pins(struct i40e_pf *pf)
{
	if (i40e_is_ptp_pin_dev(&pf->hw)) {
		kfree(pf->ptp_pins);
		kfree(pf->ptp_caps.pin_config);
		pf->ptp_pins = NULL;
	}
}

 
static void i40e_ptp_set_pin_hw(struct i40e_hw *hw,
				unsigned int pin,
				enum i40e_ptp_gpio_pin_state state)
{
	switch (state) {
	case off:
		wr32(hw, I40E_GLGEN_GPIO_CTL(pin), 0);
		break;
	case in_A:
		wr32(hw, I40E_GLGEN_GPIO_CTL(pin),
		     I40E_GLGEN_GPIO_CTL_PORT_0_IN_TIMESYNC_0);
		break;
	case in_B:
		wr32(hw, I40E_GLGEN_GPIO_CTL(pin),
		     I40E_GLGEN_GPIO_CTL_PORT_1_IN_TIMESYNC_0);
		break;
	case out_A:
		wr32(hw, I40E_GLGEN_GPIO_CTL(pin),
		     I40E_GLGEN_GPIO_CTL_PORT_0_OUT_TIMESYNC_1);
		break;
	case out_B:
		wr32(hw, I40E_GLGEN_GPIO_CTL(pin),
		     I40E_GLGEN_GPIO_CTL_PORT_1_OUT_TIMESYNC_1);
		break;
	default:
		break;
	}
}

 
static void i40e_ptp_set_led_hw(struct i40e_hw *hw,
				unsigned int led,
				enum i40e_ptp_led_pin_state state)
{
	switch (state) {
	case low:
		wr32(hw, I40E_GLGEN_GPIO_SET,
		     I40E_GLGEN_GPIO_SET_DRV_SDP_DATA | led);
		break;
	case high:
		wr32(hw, I40E_GLGEN_GPIO_SET,
		     I40E_GLGEN_GPIO_SET_DRV_SDP_DATA |
		     I40E_GLGEN_GPIO_SET_SDP_DATA_HI | led);
		break;
	default:
		break;
	}
}

 
static void i40e_ptp_init_leds_hw(struct i40e_hw *hw)
{
	wr32(hw, I40E_GLGEN_GPIO_CTL(I40E_LED2_0),
	     I40E_GLGEN_GPIO_CTL_LED_INIT);
	wr32(hw, I40E_GLGEN_GPIO_CTL(I40E_LED2_1),
	     I40E_GLGEN_GPIO_CTL_LED_INIT);
	wr32(hw, I40E_GLGEN_GPIO_CTL(I40E_LED3_0),
	     I40E_GLGEN_GPIO_CTL_LED_INIT);
	wr32(hw, I40E_GLGEN_GPIO_CTL(I40E_LED3_1),
	     I40E_GLGEN_GPIO_CTL_LED_INIT);
}

 
static void i40e_ptp_set_pins_hw(struct i40e_pf *pf)
{
	const struct i40e_ptp_pins_settings *pins = pf->ptp_pins;
	struct i40e_hw *hw = &pf->hw;

	 
	i40e_ptp_set_pin_hw(hw, I40E_SDP3_2, off);
	i40e_ptp_set_pin_hw(hw, I40E_SDP3_3, off);
	i40e_ptp_set_pin_hw(hw, I40E_GPIO_4, off);

	i40e_ptp_set_pin_hw(hw, I40E_SDP3_2, pins->sdp3_2);
	i40e_ptp_set_pin_hw(hw, I40E_SDP3_3, pins->sdp3_3);
	i40e_ptp_set_pin_hw(hw, I40E_GPIO_4, pins->gpio_4);

	i40e_ptp_set_led_hw(hw, I40E_LED2_0, pins->led2_0);
	i40e_ptp_set_led_hw(hw, I40E_LED2_1, pins->led2_1);
	i40e_ptp_set_led_hw(hw, I40E_LED3_0, pins->led3_0);
	i40e_ptp_set_led_hw(hw, I40E_LED3_1, pins->led3_1);

	dev_info(&pf->pdev->dev,
		 "PTP configuration set to: SDP3_2: %s,  SDP3_3: %s,  GPIO_4: %s.\n",
		 i40e_ptp_gpio_pin_state2str[pins->sdp3_2],
		 i40e_ptp_gpio_pin_state2str[pins->sdp3_3],
		 i40e_ptp_gpio_pin_state2str[pins->gpio_4]);
}

 
static int i40e_ptp_set_pins(struct i40e_pf *pf,
			     struct i40e_ptp_pins_settings *pins)
{
	enum i40e_can_set_pins_t pin_caps = i40e_can_set_pins(pf);
	int i = 0;

	if (pin_caps == CANT_DO_PINS)
		return -EOPNOTSUPP;
	else if (pin_caps == CAN_DO_PINS)
		return 0;

	if (pins->sdp3_2 == invalid)
		pins->sdp3_2 = pf->ptp_pins->sdp3_2;
	if (pins->sdp3_3 == invalid)
		pins->sdp3_3 = pf->ptp_pins->sdp3_3;
	if (pins->gpio_4 == invalid)
		pins->gpio_4 = pf->ptp_pins->gpio_4;
	while (i40e_ptp_pin_led_allowed_states[i].sdp3_2 != end) {
		if (pins->sdp3_2 == i40e_ptp_pin_led_allowed_states[i].sdp3_2 &&
		    pins->sdp3_3 == i40e_ptp_pin_led_allowed_states[i].sdp3_3 &&
		    pins->gpio_4 == i40e_ptp_pin_led_allowed_states[i].gpio_4) {
			pins->led2_0 =
				i40e_ptp_pin_led_allowed_states[i].led2_0;
			pins->led2_1 =
				i40e_ptp_pin_led_allowed_states[i].led2_1;
			pins->led3_0 =
				i40e_ptp_pin_led_allowed_states[i].led3_0;
			pins->led3_1 =
				i40e_ptp_pin_led_allowed_states[i].led3_1;
			break;
		}
		i++;
	}
	if (i40e_ptp_pin_led_allowed_states[i].sdp3_2 == end) {
		dev_warn(&pf->pdev->dev,
			 "Unsupported PTP pin configuration: SDP3_2: %s,  SDP3_3: %s,  GPIO_4: %s.\n",
			 i40e_ptp_gpio_pin_state2str[pins->sdp3_2],
			 i40e_ptp_gpio_pin_state2str[pins->sdp3_3],
			 i40e_ptp_gpio_pin_state2str[pins->gpio_4]);

		return -EPERM;
	}
	memcpy(pf->ptp_pins, pins, sizeof(*pins));
	i40e_ptp_set_pins_hw(pf);
	i40_ptp_reset_timing_events(pf);

	return 0;
}

 
int i40e_ptp_alloc_pins(struct i40e_pf *pf)
{
	if (!i40e_is_ptp_pin_dev(&pf->hw))
		return 0;

	pf->ptp_pins =
		kzalloc(sizeof(struct i40e_ptp_pins_settings), GFP_KERNEL);

	if (!pf->ptp_pins) {
		dev_warn(&pf->pdev->dev, "Cannot allocate memory for PTP pins structure.\n");
		return -ENOMEM;
	}

	pf->ptp_pins->sdp3_2 = off;
	pf->ptp_pins->sdp3_3 = off;
	pf->ptp_pins->gpio_4 = off;
	pf->ptp_pins->led2_0 = high;
	pf->ptp_pins->led2_1 = high;
	pf->ptp_pins->led3_0 = high;
	pf->ptp_pins->led3_1 = high;

	 
	if (pf->hw.pf_id)
		return 0;

	i40e_ptp_init_leds_hw(&pf->hw);
	i40e_ptp_set_pins_hw(pf);

	return 0;
}

 
static int i40e_ptp_set_timestamp_mode(struct i40e_pf *pf,
				       struct hwtstamp_config *config)
{
	struct i40e_hw *hw = &pf->hw;
	u32 tsyntype, regval;

	 
	regval = rd32(hw, I40E_PRTTSYN_AUX_0(0));
	 
	regval &= 0;
	regval |= (1 << I40E_PRTTSYN_AUX_0_EVNTLVL_SHIFT);
	 
	wr32(hw, I40E_PRTTSYN_AUX_0(0), regval);

	 
	regval = rd32(hw, I40E_PRTTSYN_CTL0);
	regval |= 1 << I40E_PRTTSYN_CTL0_EVENT_INT_ENA_SHIFT;
	wr32(hw, I40E_PRTTSYN_CTL0, regval);

	INIT_WORK(&pf->ptp_extts0_work, i40e_ptp_extts0_work);

	switch (config->tx_type) {
	case HWTSTAMP_TX_OFF:
		pf->ptp_tx = false;
		break;
	case HWTSTAMP_TX_ON:
		pf->ptp_tx = true;
		break;
	default:
		return -ERANGE;
	}

	switch (config->rx_filter) {
	case HWTSTAMP_FILTER_NONE:
		pf->ptp_rx = false;
		 
		tsyntype = I40E_PRTTSYN_CTL1_TSYNTYPE_V1;
		break;
	case HWTSTAMP_FILTER_PTP_V1_L4_SYNC:
	case HWTSTAMP_FILTER_PTP_V1_L4_DELAY_REQ:
	case HWTSTAMP_FILTER_PTP_V1_L4_EVENT:
		if (!(pf->hw_features & I40E_HW_PTP_L4_CAPABLE))
			return -ERANGE;
		pf->ptp_rx = true;
		tsyntype = I40E_PRTTSYN_CTL1_V1MESSTYPE0_MASK |
			   I40E_PRTTSYN_CTL1_TSYNTYPE_V1 |
			   I40E_PRTTSYN_CTL1_UDP_ENA_MASK;
		config->rx_filter = HWTSTAMP_FILTER_PTP_V1_L4_EVENT;
		break;
	case HWTSTAMP_FILTER_PTP_V2_EVENT:
	case HWTSTAMP_FILTER_PTP_V2_L4_EVENT:
	case HWTSTAMP_FILTER_PTP_V2_SYNC:
	case HWTSTAMP_FILTER_PTP_V2_L4_SYNC:
	case HWTSTAMP_FILTER_PTP_V2_DELAY_REQ:
	case HWTSTAMP_FILTER_PTP_V2_L4_DELAY_REQ:
		if (!(pf->hw_features & I40E_HW_PTP_L4_CAPABLE))
			return -ERANGE;
		fallthrough;
	case HWTSTAMP_FILTER_PTP_V2_L2_EVENT:
	case HWTSTAMP_FILTER_PTP_V2_L2_SYNC:
	case HWTSTAMP_FILTER_PTP_V2_L2_DELAY_REQ:
		pf->ptp_rx = true;
		tsyntype = I40E_PRTTSYN_CTL1_V2MESSTYPE0_MASK |
			   I40E_PRTTSYN_CTL1_TSYNTYPE_V2;
		if (pf->hw_features & I40E_HW_PTP_L4_CAPABLE) {
			tsyntype |= I40E_PRTTSYN_CTL1_UDP_ENA_MASK;
			config->rx_filter = HWTSTAMP_FILTER_PTP_V2_EVENT;
		} else {
			config->rx_filter = HWTSTAMP_FILTER_PTP_V2_L2_EVENT;
		}
		break;
	case HWTSTAMP_FILTER_NTP_ALL:
	case HWTSTAMP_FILTER_ALL:
	default:
		return -ERANGE;
	}

	 
	spin_lock_bh(&pf->ptp_rx_lock);
	rd32(hw, I40E_PRTTSYN_STAT_0);
	rd32(hw, I40E_PRTTSYN_TXTIME_H);
	rd32(hw, I40E_PRTTSYN_RXTIME_H(0));
	rd32(hw, I40E_PRTTSYN_RXTIME_H(1));
	rd32(hw, I40E_PRTTSYN_RXTIME_H(2));
	rd32(hw, I40E_PRTTSYN_RXTIME_H(3));
	pf->latch_event_flags = 0;
	spin_unlock_bh(&pf->ptp_rx_lock);

	 
	regval = rd32(hw, I40E_PRTTSYN_CTL0);
	if (pf->ptp_tx)
		regval |= I40E_PRTTSYN_CTL0_TXTIME_INT_ENA_MASK;
	else
		regval &= ~I40E_PRTTSYN_CTL0_TXTIME_INT_ENA_MASK;
	wr32(hw, I40E_PRTTSYN_CTL0, regval);

	regval = rd32(hw, I40E_PFINT_ICR0_ENA);
	if (pf->ptp_tx)
		regval |= I40E_PFINT_ICR0_ENA_TIMESYNC_MASK;
	else
		regval &= ~I40E_PFINT_ICR0_ENA_TIMESYNC_MASK;
	wr32(hw, I40E_PFINT_ICR0_ENA, regval);

	 
	regval = rd32(hw, I40E_PRTTSYN_CTL1);
	 
	regval &= I40E_PRTTSYN_CTL1_TSYNENA_MASK;
	 
	regval |= tsyntype;
	wr32(hw, I40E_PRTTSYN_CTL1, regval);

	return 0;
}

 
int i40e_ptp_set_ts_config(struct i40e_pf *pf, struct ifreq *ifr)
{
	struct hwtstamp_config config;
	int err;

	if (!(pf->flags & I40E_FLAG_PTP))
		return -EOPNOTSUPP;

	if (copy_from_user(&config, ifr->ifr_data, sizeof(config)))
		return -EFAULT;

	err = i40e_ptp_set_timestamp_mode(pf, &config);
	if (err)
		return err;

	 
	pf->tstamp_config = config;

	return copy_to_user(ifr->ifr_data, &config, sizeof(config)) ?
		-EFAULT : 0;
}

 
static int i40e_init_pin_config(struct i40e_pf *pf)
{
	int i;

	pf->ptp_caps.n_pins = 3;
	pf->ptp_caps.n_ext_ts = 2;
	pf->ptp_caps.pps = 1;
	pf->ptp_caps.n_per_out = 2;

	pf->ptp_caps.pin_config = kcalloc(pf->ptp_caps.n_pins,
					  sizeof(*pf->ptp_caps.pin_config),
					  GFP_KERNEL);
	if (!pf->ptp_caps.pin_config)
		return -ENOMEM;

	for (i = 0; i < pf->ptp_caps.n_pins; i++) {
		snprintf(pf->ptp_caps.pin_config[i].name,
			 sizeof(pf->ptp_caps.pin_config[i].name),
			 "%s", sdp_desc[i].name);
		pf->ptp_caps.pin_config[i].index = sdp_desc[i].index;
		pf->ptp_caps.pin_config[i].func = PTP_PF_NONE;
		pf->ptp_caps.pin_config[i].chan = sdp_desc[i].chan;
	}

	pf->ptp_caps.verify = i40e_ptp_verify;
	pf->ptp_caps.enable = i40e_ptp_feature_enable;

	pf->ptp_caps.pps = 1;

	return 0;
}

 
static long i40e_ptp_create_clock(struct i40e_pf *pf)
{
	 
	if (!IS_ERR_OR_NULL(pf->ptp_clock))
		return 0;

	strscpy(pf->ptp_caps.name, i40e_driver_name,
		sizeof(pf->ptp_caps.name) - 1);
	pf->ptp_caps.owner = THIS_MODULE;
	pf->ptp_caps.max_adj = 999999999;
	pf->ptp_caps.adjfine = i40e_ptp_adjfine;
	pf->ptp_caps.adjtime = i40e_ptp_adjtime;
	pf->ptp_caps.gettimex64 = i40e_ptp_gettimex;
	pf->ptp_caps.settime64 = i40e_ptp_settime;
	if (i40e_is_ptp_pin_dev(&pf->hw)) {
		int err = i40e_init_pin_config(pf);

		if (err)
			return err;
	}

	 
	pf->ptp_clock = ptp_clock_register(&pf->ptp_caps, &pf->pdev->dev);
	if (IS_ERR(pf->ptp_clock))
		return PTR_ERR(pf->ptp_clock);

	 
	pf->tstamp_config.rx_filter = HWTSTAMP_FILTER_NONE;
	pf->tstamp_config.tx_type = HWTSTAMP_TX_OFF;

	 
	ktime_get_real_ts64(&pf->ptp_prev_hw_time);
	pf->ptp_reset_start = ktime_get();

	return 0;
}

 
void i40e_ptp_save_hw_time(struct i40e_pf *pf)
{
	 
	if (!(pf->flags & I40E_FLAG_PTP))
		return;

	i40e_ptp_gettimex(&pf->ptp_caps, &pf->ptp_prev_hw_time, NULL);
	 
	pf->ptp_reset_start = ktime_get();
}

 
void i40e_ptp_restore_hw_time(struct i40e_pf *pf)
{
	ktime_t delta = ktime_sub(ktime_get(), pf->ptp_reset_start);

	 
	timespec64_add_ns(&pf->ptp_prev_hw_time, ktime_to_ns(delta));

	 
	i40e_ptp_settime(&pf->ptp_caps, &pf->ptp_prev_hw_time);
}

 
void i40e_ptp_init(struct i40e_pf *pf)
{
	struct net_device *netdev = pf->vsi[pf->lan_vsi]->netdev;
	struct i40e_hw *hw = &pf->hw;
	u32 pf_id;
	long err;

	 
	pf_id = (rd32(hw, I40E_PRTTSYN_CTL0) & I40E_PRTTSYN_CTL0_PF_ID_MASK) >>
		I40E_PRTTSYN_CTL0_PF_ID_SHIFT;
	if (hw->pf_id != pf_id) {
		pf->flags &= ~I40E_FLAG_PTP;
		dev_info(&pf->pdev->dev, "%s: PTP not supported on %s\n",
			 __func__,
			 netdev->name);
		return;
	}

	mutex_init(&pf->tmreg_lock);
	spin_lock_init(&pf->ptp_rx_lock);

	 
	err = i40e_ptp_create_clock(pf);
	if (err) {
		pf->ptp_clock = NULL;
		dev_err(&pf->pdev->dev, "%s: ptp_clock_register failed\n",
			__func__);
	} else if (pf->ptp_clock) {
		u32 regval;

		if (pf->hw.debug_mask & I40E_DEBUG_LAN)
			dev_info(&pf->pdev->dev, "PHC enabled\n");
		pf->flags |= I40E_FLAG_PTP;

		 
		regval = rd32(hw, I40E_PRTTSYN_CTL0);
		regval |= I40E_PRTTSYN_CTL0_TSYNENA_MASK;
		wr32(hw, I40E_PRTTSYN_CTL0, regval);
		regval = rd32(hw, I40E_PRTTSYN_CTL1);
		regval |= I40E_PRTTSYN_CTL1_TSYNENA_MASK;
		wr32(hw, I40E_PRTTSYN_CTL1, regval);

		 
		i40e_ptp_set_increment(pf);

		 
		i40e_ptp_set_timestamp_mode(pf, &pf->tstamp_config);

		 
		i40e_ptp_restore_hw_time(pf);
	}

	i40e_ptp_set_1pps_signal_hw(pf);
}

 
void i40e_ptp_stop(struct i40e_pf *pf)
{
	struct i40e_hw *hw = &pf->hw;
	u32 regval;

	pf->flags &= ~I40E_FLAG_PTP;
	pf->ptp_tx = false;
	pf->ptp_rx = false;

	if (pf->ptp_tx_skb) {
		struct sk_buff *skb = pf->ptp_tx_skb;

		pf->ptp_tx_skb = NULL;
		clear_bit_unlock(__I40E_PTP_TX_IN_PROGRESS, pf->state);
		dev_kfree_skb_any(skb);
	}

	if (pf->ptp_clock) {
		ptp_clock_unregister(pf->ptp_clock);
		pf->ptp_clock = NULL;
		dev_info(&pf->pdev->dev, "%s: removed PHC on %s\n", __func__,
			 pf->vsi[pf->lan_vsi]->netdev->name);
	}

	if (i40e_is_ptp_pin_dev(&pf->hw)) {
		i40e_ptp_set_pin_hw(hw, I40E_SDP3_2, off);
		i40e_ptp_set_pin_hw(hw, I40E_SDP3_3, off);
		i40e_ptp_set_pin_hw(hw, I40E_GPIO_4, off);
	}

	regval = rd32(hw, I40E_PRTTSYN_AUX_0(0));
	regval &= ~I40E_PRTTSYN_AUX_0_PTPFLAG_MASK;
	wr32(hw, I40E_PRTTSYN_AUX_0(0), regval);

	 
	regval = rd32(hw, I40E_PRTTSYN_CTL0);
	regval &= ~I40E_PRTTSYN_CTL0_EVENT_INT_ENA_MASK;
	wr32(hw, I40E_PRTTSYN_CTL0, regval);

	i40e_ptp_free_pins(pf);
}
