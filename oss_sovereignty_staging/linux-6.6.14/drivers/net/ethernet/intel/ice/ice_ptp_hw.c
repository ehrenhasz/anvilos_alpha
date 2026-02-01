
 

#include <linux/delay.h>
#include "ice_common.h"
#include "ice_ptp_hw.h"
#include "ice_ptp_consts.h"
#include "ice_cgu_regs.h"

 

 
u8 ice_get_ptp_src_clock_index(struct ice_hw *hw)
{
	return hw->func_caps.ts_func_info.tmr_index_assoc;
}

 
static u64 ice_ptp_read_src_incval(struct ice_hw *hw)
{
	u32 lo, hi;
	u8 tmr_idx;

	tmr_idx = ice_get_ptp_src_clock_index(hw);

	lo = rd32(hw, GLTSYN_INCVAL_L(tmr_idx));
	hi = rd32(hw, GLTSYN_INCVAL_H(tmr_idx));

	return ((u64)(hi & INCVAL_HIGH_M) << 32) | lo;
}

 
static void ice_ptp_src_cmd(struct ice_hw *hw, enum ice_ptp_tmr_cmd cmd)
{
	u32 cmd_val;
	u8 tmr_idx;

	tmr_idx = ice_get_ptp_src_clock_index(hw);
	cmd_val = tmr_idx << SEL_CPK_SRC;

	switch (cmd) {
	case INIT_TIME:
		cmd_val |= GLTSYN_CMD_INIT_TIME;
		break;
	case INIT_INCVAL:
		cmd_val |= GLTSYN_CMD_INIT_INCVAL;
		break;
	case ADJ_TIME:
		cmd_val |= GLTSYN_CMD_ADJ_TIME;
		break;
	case ADJ_TIME_AT_TIME:
		cmd_val |= GLTSYN_CMD_ADJ_INIT_TIME;
		break;
	case READ_TIME:
		cmd_val |= GLTSYN_CMD_READ_TIME;
		break;
	case ICE_PTP_NOP:
		break;
	}

	wr32(hw, GLTSYN_CMD, cmd_val);
}

 
static void ice_ptp_exec_tmr_cmd(struct ice_hw *hw)
{
	wr32(hw, GLTSYN_CMD_SYNC, SYNC_EXEC_CMD);
	ice_flush(hw);
}

 

 
static void
ice_fill_phy_msg_e822(struct ice_sbq_msg_input *msg, u8 port, u16 offset)
{
	int phy_port, phy, quadtype;

	phy_port = port % ICE_PORTS_PER_PHY;
	phy = port / ICE_PORTS_PER_PHY;
	quadtype = (port / ICE_PORTS_PER_QUAD) % ICE_NUM_QUAD_TYPE;

	if (quadtype == 0) {
		msg->msg_addr_low = P_Q0_L(P_0_BASE + offset, phy_port);
		msg->msg_addr_high = P_Q0_H(P_0_BASE + offset, phy_port);
	} else {
		msg->msg_addr_low = P_Q1_L(P_4_BASE + offset, phy_port);
		msg->msg_addr_high = P_Q1_H(P_4_BASE + offset, phy_port);
	}

	if (phy == 0)
		msg->dest_dev = rmn_0;
	else if (phy == 1)
		msg->dest_dev = rmn_1;
	else
		msg->dest_dev = rmn_2;
}

 
static bool ice_is_64b_phy_reg_e822(u16 low_addr, u16 *high_addr)
{
	switch (low_addr) {
	case P_REG_PAR_PCS_TX_OFFSET_L:
		*high_addr = P_REG_PAR_PCS_TX_OFFSET_U;
		return true;
	case P_REG_PAR_PCS_RX_OFFSET_L:
		*high_addr = P_REG_PAR_PCS_RX_OFFSET_U;
		return true;
	case P_REG_PAR_TX_TIME_L:
		*high_addr = P_REG_PAR_TX_TIME_U;
		return true;
	case P_REG_PAR_RX_TIME_L:
		*high_addr = P_REG_PAR_RX_TIME_U;
		return true;
	case P_REG_TOTAL_TX_OFFSET_L:
		*high_addr = P_REG_TOTAL_TX_OFFSET_U;
		return true;
	case P_REG_TOTAL_RX_OFFSET_L:
		*high_addr = P_REG_TOTAL_RX_OFFSET_U;
		return true;
	case P_REG_UIX66_10G_40G_L:
		*high_addr = P_REG_UIX66_10G_40G_U;
		return true;
	case P_REG_UIX66_25G_100G_L:
		*high_addr = P_REG_UIX66_25G_100G_U;
		return true;
	case P_REG_TX_CAPTURE_L:
		*high_addr = P_REG_TX_CAPTURE_U;
		return true;
	case P_REG_RX_CAPTURE_L:
		*high_addr = P_REG_RX_CAPTURE_U;
		return true;
	case P_REG_TX_TIMER_INC_PRE_L:
		*high_addr = P_REG_TX_TIMER_INC_PRE_U;
		return true;
	case P_REG_RX_TIMER_INC_PRE_L:
		*high_addr = P_REG_RX_TIMER_INC_PRE_U;
		return true;
	default:
		return false;
	}
}

 
static bool ice_is_40b_phy_reg_e822(u16 low_addr, u16 *high_addr)
{
	switch (low_addr) {
	case P_REG_TIMETUS_L:
		*high_addr = P_REG_TIMETUS_U;
		return true;
	case P_REG_PAR_RX_TUS_L:
		*high_addr = P_REG_PAR_RX_TUS_U;
		return true;
	case P_REG_PAR_TX_TUS_L:
		*high_addr = P_REG_PAR_TX_TUS_U;
		return true;
	case P_REG_PCS_RX_TUS_L:
		*high_addr = P_REG_PCS_RX_TUS_U;
		return true;
	case P_REG_PCS_TX_TUS_L:
		*high_addr = P_REG_PCS_TX_TUS_U;
		return true;
	case P_REG_DESK_PAR_RX_TUS_L:
		*high_addr = P_REG_DESK_PAR_RX_TUS_U;
		return true;
	case P_REG_DESK_PAR_TX_TUS_L:
		*high_addr = P_REG_DESK_PAR_TX_TUS_U;
		return true;
	case P_REG_DESK_PCS_RX_TUS_L:
		*high_addr = P_REG_DESK_PCS_RX_TUS_U;
		return true;
	case P_REG_DESK_PCS_TX_TUS_L:
		*high_addr = P_REG_DESK_PCS_TX_TUS_U;
		return true;
	default:
		return false;
	}
}

 
static int
ice_read_phy_reg_e822(struct ice_hw *hw, u8 port, u16 offset, u32 *val)
{
	struct ice_sbq_msg_input msg = {0};
	int err;

	ice_fill_phy_msg_e822(&msg, port, offset);
	msg.opcode = ice_sbq_msg_rd;

	err = ice_sbq_rw_reg(hw, &msg);
	if (err) {
		ice_debug(hw, ICE_DBG_PTP, "Failed to send message to PHY, err %d\n",
			  err);
		return err;
	}

	*val = msg.data;

	return 0;
}

 
static int
ice_read_64b_phy_reg_e822(struct ice_hw *hw, u8 port, u16 low_addr, u64 *val)
{
	u32 low, high;
	u16 high_addr;
	int err;

	 
	if (!ice_is_64b_phy_reg_e822(low_addr, &high_addr)) {
		ice_debug(hw, ICE_DBG_PTP, "Invalid 64b register addr 0x%08x\n",
			  low_addr);
		return -EINVAL;
	}

	err = ice_read_phy_reg_e822(hw, port, low_addr, &low);
	if (err) {
		ice_debug(hw, ICE_DBG_PTP, "Failed to read from low register 0x%08x\n, err %d",
			  low_addr, err);
		return err;
	}

	err = ice_read_phy_reg_e822(hw, port, high_addr, &high);
	if (err) {
		ice_debug(hw, ICE_DBG_PTP, "Failed to read from high register 0x%08x\n, err %d",
			  high_addr, err);
		return err;
	}

	*val = (u64)high << 32 | low;

	return 0;
}

 
static int
ice_write_phy_reg_e822(struct ice_hw *hw, u8 port, u16 offset, u32 val)
{
	struct ice_sbq_msg_input msg = {0};
	int err;

	ice_fill_phy_msg_e822(&msg, port, offset);
	msg.opcode = ice_sbq_msg_wr;
	msg.data = val;

	err = ice_sbq_rw_reg(hw, &msg);
	if (err) {
		ice_debug(hw, ICE_DBG_PTP, "Failed to send message to PHY, err %d\n",
			  err);
		return err;
	}

	return 0;
}

 
static int
ice_write_40b_phy_reg_e822(struct ice_hw *hw, u8 port, u16 low_addr, u64 val)
{
	u32 low, high;
	u16 high_addr;
	int err;

	 
	if (!ice_is_40b_phy_reg_e822(low_addr, &high_addr)) {
		ice_debug(hw, ICE_DBG_PTP, "Invalid 40b register addr 0x%08x\n",
			  low_addr);
		return -EINVAL;
	}

	low = (u32)(val & P_REG_40B_LOW_M);
	high = (u32)(val >> P_REG_40B_HIGH_S);

	err = ice_write_phy_reg_e822(hw, port, low_addr, low);
	if (err) {
		ice_debug(hw, ICE_DBG_PTP, "Failed to write to low register 0x%08x\n, err %d",
			  low_addr, err);
		return err;
	}

	err = ice_write_phy_reg_e822(hw, port, high_addr, high);
	if (err) {
		ice_debug(hw, ICE_DBG_PTP, "Failed to write to high register 0x%08x\n, err %d",
			  high_addr, err);
		return err;
	}

	return 0;
}

 
static int
ice_write_64b_phy_reg_e822(struct ice_hw *hw, u8 port, u16 low_addr, u64 val)
{
	u32 low, high;
	u16 high_addr;
	int err;

	 
	if (!ice_is_64b_phy_reg_e822(low_addr, &high_addr)) {
		ice_debug(hw, ICE_DBG_PTP, "Invalid 64b register addr 0x%08x\n",
			  low_addr);
		return -EINVAL;
	}

	low = lower_32_bits(val);
	high = upper_32_bits(val);

	err = ice_write_phy_reg_e822(hw, port, low_addr, low);
	if (err) {
		ice_debug(hw, ICE_DBG_PTP, "Failed to write to low register 0x%08x\n, err %d",
			  low_addr, err);
		return err;
	}

	err = ice_write_phy_reg_e822(hw, port, high_addr, high);
	if (err) {
		ice_debug(hw, ICE_DBG_PTP, "Failed to write to high register 0x%08x\n, err %d",
			  high_addr, err);
		return err;
	}

	return 0;
}

 
static void
ice_fill_quad_msg_e822(struct ice_sbq_msg_input *msg, u8 quad, u16 offset)
{
	u32 addr;

	msg->dest_dev = rmn_0;

	if ((quad % ICE_NUM_QUAD_TYPE) == 0)
		addr = Q_0_BASE + offset;
	else
		addr = Q_1_BASE + offset;

	msg->msg_addr_low = lower_16_bits(addr);
	msg->msg_addr_high = upper_16_bits(addr);
}

 
int
ice_read_quad_reg_e822(struct ice_hw *hw, u8 quad, u16 offset, u32 *val)
{
	struct ice_sbq_msg_input msg = {0};
	int err;

	if (quad >= ICE_MAX_QUAD)
		return -EINVAL;

	ice_fill_quad_msg_e822(&msg, quad, offset);
	msg.opcode = ice_sbq_msg_rd;

	err = ice_sbq_rw_reg(hw, &msg);
	if (err) {
		ice_debug(hw, ICE_DBG_PTP, "Failed to send message to PHY, err %d\n",
			  err);
		return err;
	}

	*val = msg.data;

	return 0;
}

 
int
ice_write_quad_reg_e822(struct ice_hw *hw, u8 quad, u16 offset, u32 val)
{
	struct ice_sbq_msg_input msg = {0};
	int err;

	if (quad >= ICE_MAX_QUAD)
		return -EINVAL;

	ice_fill_quad_msg_e822(&msg, quad, offset);
	msg.opcode = ice_sbq_msg_wr;
	msg.data = val;

	err = ice_sbq_rw_reg(hw, &msg);
	if (err) {
		ice_debug(hw, ICE_DBG_PTP, "Failed to send message to PHY, err %d\n",
			  err);
		return err;
	}

	return 0;
}

 
static int
ice_read_phy_tstamp_e822(struct ice_hw *hw, u8 quad, u8 idx, u64 *tstamp)
{
	u16 lo_addr, hi_addr;
	u32 lo, hi;
	int err;

	lo_addr = (u16)TS_L(Q_REG_TX_MEMORY_BANK_START, idx);
	hi_addr = (u16)TS_H(Q_REG_TX_MEMORY_BANK_START, idx);

	err = ice_read_quad_reg_e822(hw, quad, lo_addr, &lo);
	if (err) {
		ice_debug(hw, ICE_DBG_PTP, "Failed to read low PTP timestamp register, err %d\n",
			  err);
		return err;
	}

	err = ice_read_quad_reg_e822(hw, quad, hi_addr, &hi);
	if (err) {
		ice_debug(hw, ICE_DBG_PTP, "Failed to read high PTP timestamp register, err %d\n",
			  err);
		return err;
	}

	 
	*tstamp = ((u64)hi) << TS_PHY_HIGH_S | ((u64)lo & TS_PHY_LOW_M);

	return 0;
}

 
static int
ice_clear_phy_tstamp_e822(struct ice_hw *hw, u8 quad, u8 idx)
{
	u16 lo_addr, hi_addr;
	int err;

	lo_addr = (u16)TS_L(Q_REG_TX_MEMORY_BANK_START, idx);
	hi_addr = (u16)TS_H(Q_REG_TX_MEMORY_BANK_START, idx);

	err = ice_write_quad_reg_e822(hw, quad, lo_addr, 0);
	if (err) {
		ice_debug(hw, ICE_DBG_PTP, "Failed to clear low PTP timestamp register, err %d\n",
			  err);
		return err;
	}

	err = ice_write_quad_reg_e822(hw, quad, hi_addr, 0);
	if (err) {
		ice_debug(hw, ICE_DBG_PTP, "Failed to clear high PTP timestamp register, err %d\n",
			  err);
		return err;
	}

	return 0;
}

 
void ice_ptp_reset_ts_memory_quad_e822(struct ice_hw *hw, u8 quad)
{
	ice_write_quad_reg_e822(hw, quad, Q_REG_TS_CTRL, Q_REG_TS_CTRL_M);
	ice_write_quad_reg_e822(hw, quad, Q_REG_TS_CTRL, ~(u32)Q_REG_TS_CTRL_M);
}

 
static void ice_ptp_reset_ts_memory_e822(struct ice_hw *hw)
{
	unsigned int quad;

	for (quad = 0; quad < ICE_MAX_QUAD; quad++)
		ice_ptp_reset_ts_memory_quad_e822(hw, quad);
}

 
static int
ice_read_cgu_reg_e822(struct ice_hw *hw, u32 addr, u32 *val)
{
	struct ice_sbq_msg_input cgu_msg;
	int err;

	cgu_msg.opcode = ice_sbq_msg_rd;
	cgu_msg.dest_dev = cgu;
	cgu_msg.msg_addr_low = addr;
	cgu_msg.msg_addr_high = 0x0;

	err = ice_sbq_rw_reg(hw, &cgu_msg);
	if (err) {
		ice_debug(hw, ICE_DBG_PTP, "Failed to read CGU register 0x%04x, err %d\n",
			  addr, err);
		return err;
	}

	*val = cgu_msg.data;

	return err;
}

 
static int
ice_write_cgu_reg_e822(struct ice_hw *hw, u32 addr, u32 val)
{
	struct ice_sbq_msg_input cgu_msg;
	int err;

	cgu_msg.opcode = ice_sbq_msg_wr;
	cgu_msg.dest_dev = cgu;
	cgu_msg.msg_addr_low = addr;
	cgu_msg.msg_addr_high = 0x0;
	cgu_msg.data = val;

	err = ice_sbq_rw_reg(hw, &cgu_msg);
	if (err) {
		ice_debug(hw, ICE_DBG_PTP, "Failed to write CGU register 0x%04x, err %d\n",
			  addr, err);
		return err;
	}

	return err;
}

 
static const char *ice_clk_freq_str(u8 clk_freq)
{
	switch ((enum ice_time_ref_freq)clk_freq) {
	case ICE_TIME_REF_FREQ_25_000:
		return "25 MHz";
	case ICE_TIME_REF_FREQ_122_880:
		return "122.88 MHz";
	case ICE_TIME_REF_FREQ_125_000:
		return "125 MHz";
	case ICE_TIME_REF_FREQ_153_600:
		return "153.6 MHz";
	case ICE_TIME_REF_FREQ_156_250:
		return "156.25 MHz";
	case ICE_TIME_REF_FREQ_245_760:
		return "245.76 MHz";
	default:
		return "Unknown";
	}
}

 
static const char *ice_clk_src_str(u8 clk_src)
{
	switch ((enum ice_clk_src)clk_src) {
	case ICE_CLK_SRC_TCX0:
		return "TCX0";
	case ICE_CLK_SRC_TIME_REF:
		return "TIME_REF";
	default:
		return "Unknown";
	}
}

 
static int
ice_cfg_cgu_pll_e822(struct ice_hw *hw, enum ice_time_ref_freq clk_freq,
		     enum ice_clk_src clk_src)
{
	union tspll_ro_bwm_lf bwm_lf;
	union nac_cgu_dword19 dw19;
	union nac_cgu_dword22 dw22;
	union nac_cgu_dword24 dw24;
	union nac_cgu_dword9 dw9;
	int err;

	if (clk_freq >= NUM_ICE_TIME_REF_FREQ) {
		dev_warn(ice_hw_to_dev(hw), "Invalid TIME_REF frequency %u\n",
			 clk_freq);
		return -EINVAL;
	}

	if (clk_src >= NUM_ICE_CLK_SRC) {
		dev_warn(ice_hw_to_dev(hw), "Invalid clock source %u\n",
			 clk_src);
		return -EINVAL;
	}

	if (clk_src == ICE_CLK_SRC_TCX0 &&
	    clk_freq != ICE_TIME_REF_FREQ_25_000) {
		dev_warn(ice_hw_to_dev(hw),
			 "TCX0 only supports 25 MHz frequency\n");
		return -EINVAL;
	}

	err = ice_read_cgu_reg_e822(hw, NAC_CGU_DWORD9, &dw9.val);
	if (err)
		return err;

	err = ice_read_cgu_reg_e822(hw, NAC_CGU_DWORD24, &dw24.val);
	if (err)
		return err;

	err = ice_read_cgu_reg_e822(hw, TSPLL_RO_BWM_LF, &bwm_lf.val);
	if (err)
		return err;

	 
	ice_debug(hw, ICE_DBG_PTP, "Current CGU configuration -- %s, clk_src %s, clk_freq %s, PLL %s\n",
		  dw24.field.ts_pll_enable ? "enabled" : "disabled",
		  ice_clk_src_str(dw24.field.time_ref_sel),
		  ice_clk_freq_str(dw9.field.time_ref_freq_sel),
		  bwm_lf.field.plllock_true_lock_cri ? "locked" : "unlocked");

	 
	if (dw24.field.ts_pll_enable) {
		dw24.field.ts_pll_enable = 0;

		err = ice_write_cgu_reg_e822(hw, NAC_CGU_DWORD24, dw24.val);
		if (err)
			return err;
	}

	 
	dw9.field.time_ref_freq_sel = clk_freq;
	err = ice_write_cgu_reg_e822(hw, NAC_CGU_DWORD9, dw9.val);
	if (err)
		return err;

	 
	err = ice_read_cgu_reg_e822(hw, NAC_CGU_DWORD19, &dw19.val);
	if (err)
		return err;

	dw19.field.tspll_fbdiv_intgr = e822_cgu_params[clk_freq].feedback_div;
	dw19.field.tspll_ndivratio = 1;

	err = ice_write_cgu_reg_e822(hw, NAC_CGU_DWORD19, dw19.val);
	if (err)
		return err;

	 
	err = ice_read_cgu_reg_e822(hw, NAC_CGU_DWORD22, &dw22.val);
	if (err)
		return err;

	dw22.field.time1588clk_div = e822_cgu_params[clk_freq].post_pll_div;
	dw22.field.time1588clk_sel_div2 = 0;

	err = ice_write_cgu_reg_e822(hw, NAC_CGU_DWORD22, dw22.val);
	if (err)
		return err;

	 
	err = ice_read_cgu_reg_e822(hw, NAC_CGU_DWORD24, &dw24.val);
	if (err)
		return err;

	dw24.field.ref1588_ck_div = e822_cgu_params[clk_freq].refclk_pre_div;
	dw24.field.tspll_fbdiv_frac = e822_cgu_params[clk_freq].frac_n_div;
	dw24.field.time_ref_sel = clk_src;

	err = ice_write_cgu_reg_e822(hw, NAC_CGU_DWORD24, dw24.val);
	if (err)
		return err;

	 
	dw24.field.ts_pll_enable = 1;

	err = ice_write_cgu_reg_e822(hw, NAC_CGU_DWORD24, dw24.val);
	if (err)
		return err;

	 
	usleep_range(1000, 5000);

	err = ice_read_cgu_reg_e822(hw, TSPLL_RO_BWM_LF, &bwm_lf.val);
	if (err)
		return err;

	if (!bwm_lf.field.plllock_true_lock_cri) {
		dev_warn(ice_hw_to_dev(hw), "CGU PLL failed to lock\n");
		return -EBUSY;
	}

	 
	ice_debug(hw, ICE_DBG_PTP, "New CGU configuration -- %s, clk_src %s, clk_freq %s, PLL %s\n",
		  dw24.field.ts_pll_enable ? "enabled" : "disabled",
		  ice_clk_src_str(dw24.field.time_ref_sel),
		  ice_clk_freq_str(dw9.field.time_ref_freq_sel),
		  bwm_lf.field.plllock_true_lock_cri ? "locked" : "unlocked");

	return 0;
}

 
static int ice_init_cgu_e822(struct ice_hw *hw)
{
	struct ice_ts_func_info *ts_info = &hw->func_caps.ts_func_info;
	union tspll_cntr_bist_settings cntr_bist;
	int err;

	err = ice_read_cgu_reg_e822(hw, TSPLL_CNTR_BIST_SETTINGS,
				    &cntr_bist.val);
	if (err)
		return err;

	 
	cntr_bist.field.i_plllock_sel_0 = 0;
	cntr_bist.field.i_plllock_sel_1 = 0;

	err = ice_write_cgu_reg_e822(hw, TSPLL_CNTR_BIST_SETTINGS,
				     cntr_bist.val);
	if (err)
		return err;

	 
	err = ice_cfg_cgu_pll_e822(hw, ts_info->time_ref,
				   (enum ice_clk_src)ts_info->clk_src);
	if (err)
		return err;

	return 0;
}

 
static int ice_ptp_set_vernier_wl(struct ice_hw *hw)
{
	u8 port;

	for (port = 0; port < ICE_NUM_EXTERNAL_PORTS; port++) {
		int err;

		err = ice_write_phy_reg_e822(hw, port, P_REG_WL,
					     PTP_VERNIER_WL);
		if (err) {
			ice_debug(hw, ICE_DBG_PTP, "Failed to set vernier window length for port %u, err %d\n",
				  port, err);
			return err;
		}
	}

	return 0;
}

 
static int ice_ptp_init_phc_e822(struct ice_hw *hw)
{
	int err;
	u32 regval;

	 
#define PF_SB_REM_DEV_CTL_SWITCH_READ BIT(1)
#define PF_SB_REM_DEV_CTL_PHY0 BIT(2)
	regval = rd32(hw, PF_SB_REM_DEV_CTL);
	regval |= (PF_SB_REM_DEV_CTL_SWITCH_READ |
		   PF_SB_REM_DEV_CTL_PHY0);
	wr32(hw, PF_SB_REM_DEV_CTL, regval);

	 
	err = ice_init_cgu_e822(hw);
	if (err)
		return err;

	 
	return ice_ptp_set_vernier_wl(hw);
}

 
static int
ice_ptp_prep_phy_time_e822(struct ice_hw *hw, u32 time)
{
	u64 phy_time;
	u8 port;
	int err;

	 
	phy_time = (u64)time << 32;

	for (port = 0; port < ICE_NUM_EXTERNAL_PORTS; port++) {
		 
		err = ice_write_64b_phy_reg_e822(hw, port,
						 P_REG_TX_TIMER_INC_PRE_L,
						 phy_time);
		if (err)
			goto exit_err;

		 
		err = ice_write_64b_phy_reg_e822(hw, port,
						 P_REG_RX_TIMER_INC_PRE_L,
						 phy_time);
		if (err)
			goto exit_err;
	}

	return 0;

exit_err:
	ice_debug(hw, ICE_DBG_PTP, "Failed to write init time for port %u, err %d\n",
		  port, err);

	return err;
}

 
static int
ice_ptp_prep_port_adj_e822(struct ice_hw *hw, u8 port, s64 time)
{
	u32 l_time, u_time;
	int err;

	l_time = lower_32_bits(time);
	u_time = upper_32_bits(time);

	 
	err = ice_write_phy_reg_e822(hw, port, P_REG_TX_TIMER_INC_PRE_L,
				     l_time);
	if (err)
		goto exit_err;

	err = ice_write_phy_reg_e822(hw, port, P_REG_TX_TIMER_INC_PRE_U,
				     u_time);
	if (err)
		goto exit_err;

	 
	err = ice_write_phy_reg_e822(hw, port, P_REG_RX_TIMER_INC_PRE_L,
				     l_time);
	if (err)
		goto exit_err;

	err = ice_write_phy_reg_e822(hw, port, P_REG_RX_TIMER_INC_PRE_U,
				     u_time);
	if (err)
		goto exit_err;

	return 0;

exit_err:
	ice_debug(hw, ICE_DBG_PTP, "Failed to write time adjust for port %u, err %d\n",
		  port, err);
	return err;
}

 
static int
ice_ptp_prep_phy_adj_e822(struct ice_hw *hw, s32 adj)
{
	s64 cycles;
	u8 port;

	 
	if (adj > 0)
		cycles = (s64)adj << 32;
	else
		cycles = -(((s64)-adj) << 32);

	for (port = 0; port < ICE_NUM_EXTERNAL_PORTS; port++) {
		int err;

		err = ice_ptp_prep_port_adj_e822(hw, port, cycles);
		if (err)
			return err;
	}

	return 0;
}

 
static int
ice_ptp_prep_phy_incval_e822(struct ice_hw *hw, u64 incval)
{
	int err;
	u8 port;

	for (port = 0; port < ICE_NUM_EXTERNAL_PORTS; port++) {
		err = ice_write_40b_phy_reg_e822(hw, port, P_REG_TIMETUS_L,
						 incval);
		if (err)
			goto exit_err;
	}

	return 0;

exit_err:
	ice_debug(hw, ICE_DBG_PTP, "Failed to write incval for port %u, err %d\n",
		  port, err);

	return err;
}

 
static int
ice_ptp_read_port_capture(struct ice_hw *hw, u8 port, u64 *tx_ts, u64 *rx_ts)
{
	int err;

	 
	err = ice_read_64b_phy_reg_e822(hw, port, P_REG_TX_CAPTURE_L, tx_ts);
	if (err) {
		ice_debug(hw, ICE_DBG_PTP, "Failed to read REG_TX_CAPTURE, err %d\n",
			  err);
		return err;
	}

	ice_debug(hw, ICE_DBG_PTP, "tx_init = 0x%016llx\n",
		  (unsigned long long)*tx_ts);

	 
	err = ice_read_64b_phy_reg_e822(hw, port, P_REG_RX_CAPTURE_L, rx_ts);
	if (err) {
		ice_debug(hw, ICE_DBG_PTP, "Failed to read RX_CAPTURE, err %d\n",
			  err);
		return err;
	}

	ice_debug(hw, ICE_DBG_PTP, "rx_init = 0x%016llx\n",
		  (unsigned long long)*rx_ts);

	return 0;
}

 
static int
ice_ptp_write_port_cmd_e822(struct ice_hw *hw, u8 port, enum ice_ptp_tmr_cmd cmd)
{
	u32 cmd_val, val;
	u8 tmr_idx;
	int err;

	tmr_idx = ice_get_ptp_src_clock_index(hw);
	cmd_val = tmr_idx << SEL_PHY_SRC;
	switch (cmd) {
	case INIT_TIME:
		cmd_val |= PHY_CMD_INIT_TIME;
		break;
	case INIT_INCVAL:
		cmd_val |= PHY_CMD_INIT_INCVAL;
		break;
	case ADJ_TIME:
		cmd_val |= PHY_CMD_ADJ_TIME;
		break;
	case READ_TIME:
		cmd_val |= PHY_CMD_READ_TIME;
		break;
	case ADJ_TIME_AT_TIME:
		cmd_val |= PHY_CMD_ADJ_TIME_AT_TIME;
		break;
	case ICE_PTP_NOP:
		break;
	}

	 
	 
	err = ice_read_phy_reg_e822(hw, port, P_REG_TX_TMR_CMD, &val);
	if (err) {
		ice_debug(hw, ICE_DBG_PTP, "Failed to read TX_TMR_CMD, err %d\n",
			  err);
		return err;
	}

	 
	val &= ~TS_CMD_MASK;
	val |= cmd_val;

	err = ice_write_phy_reg_e822(hw, port, P_REG_TX_TMR_CMD, val);
	if (err) {
		ice_debug(hw, ICE_DBG_PTP, "Failed to write back TX_TMR_CMD, err %d\n",
			  err);
		return err;
	}

	 
	 
	err = ice_read_phy_reg_e822(hw, port, P_REG_RX_TMR_CMD, &val);
	if (err) {
		ice_debug(hw, ICE_DBG_PTP, "Failed to read RX_TMR_CMD, err %d\n",
			  err);
		return err;
	}

	 
	val &= ~TS_CMD_MASK;
	val |= cmd_val;

	err = ice_write_phy_reg_e822(hw, port, P_REG_RX_TMR_CMD, val);
	if (err) {
		ice_debug(hw, ICE_DBG_PTP, "Failed to write back RX_TMR_CMD, err %d\n",
			  err);
		return err;
	}

	return 0;
}

 
static int
ice_ptp_one_port_cmd(struct ice_hw *hw, u8 configured_port,
		     enum ice_ptp_tmr_cmd configured_cmd)
{
	u8 port;

	for (port = 0; port < ICE_NUM_EXTERNAL_PORTS; port++) {
		enum ice_ptp_tmr_cmd cmd;
		int err;

		if (port == configured_port)
			cmd = configured_cmd;
		else
			cmd = ICE_PTP_NOP;

		err = ice_ptp_write_port_cmd_e822(hw, port, cmd);
		if (err)
			return err;
	}

	return 0;
}

 
static int
ice_ptp_port_cmd_e822(struct ice_hw *hw, enum ice_ptp_tmr_cmd cmd)
{
	u8 port;

	for (port = 0; port < ICE_NUM_EXTERNAL_PORTS; port++) {
		int err;

		err = ice_ptp_write_port_cmd_e822(hw, port, cmd);
		if (err)
			return err;
	}

	return 0;
}

 

 
static int
ice_phy_get_speed_and_fec_e822(struct ice_hw *hw, u8 port,
			       enum ice_ptp_link_spd *link_out,
			       enum ice_ptp_fec_mode *fec_out)
{
	enum ice_ptp_link_spd link;
	enum ice_ptp_fec_mode fec;
	u32 serdes;
	int err;

	err = ice_read_phy_reg_e822(hw, port, P_REG_LINK_SPEED, &serdes);
	if (err) {
		ice_debug(hw, ICE_DBG_PTP, "Failed to read serdes info\n");
		return err;
	}

	 
	fec = (enum ice_ptp_fec_mode)P_REG_LINK_SPEED_FEC_MODE(serdes);

	serdes &= P_REG_LINK_SPEED_SERDES_M;

	 
	if (fec == ICE_PTP_FEC_MODE_RS_FEC) {
		switch (serdes) {
		case ICE_PTP_SERDES_25G:
			link = ICE_PTP_LNK_SPD_25G_RS;
			break;
		case ICE_PTP_SERDES_50G:
			link = ICE_PTP_LNK_SPD_50G_RS;
			break;
		case ICE_PTP_SERDES_100G:
			link = ICE_PTP_LNK_SPD_100G_RS;
			break;
		default:
			return -EIO;
		}
	} else {
		switch (serdes) {
		case ICE_PTP_SERDES_1G:
			link = ICE_PTP_LNK_SPD_1G;
			break;
		case ICE_PTP_SERDES_10G:
			link = ICE_PTP_LNK_SPD_10G;
			break;
		case ICE_PTP_SERDES_25G:
			link = ICE_PTP_LNK_SPD_25G;
			break;
		case ICE_PTP_SERDES_40G:
			link = ICE_PTP_LNK_SPD_40G;
			break;
		case ICE_PTP_SERDES_50G:
			link = ICE_PTP_LNK_SPD_50G;
			break;
		default:
			return -EIO;
		}
	}

	if (link_out)
		*link_out = link;
	if (fec_out)
		*fec_out = fec;

	return 0;
}

 
static void ice_phy_cfg_lane_e822(struct ice_hw *hw, u8 port)
{
	enum ice_ptp_link_spd link_spd;
	int err;
	u32 val;
	u8 quad;

	err = ice_phy_get_speed_and_fec_e822(hw, port, &link_spd, NULL);
	if (err) {
		ice_debug(hw, ICE_DBG_PTP, "Failed to get PHY link speed, err %d\n",
			  err);
		return;
	}

	quad = port / ICE_PORTS_PER_QUAD;

	err = ice_read_quad_reg_e822(hw, quad, Q_REG_TX_MEM_GBL_CFG, &val);
	if (err) {
		ice_debug(hw, ICE_DBG_PTP, "Failed to read TX_MEM_GLB_CFG, err %d\n",
			  err);
		return;
	}

	if (link_spd >= ICE_PTP_LNK_SPD_40G)
		val &= ~Q_REG_TX_MEM_GBL_CFG_LANE_TYPE_M;
	else
		val |= Q_REG_TX_MEM_GBL_CFG_LANE_TYPE_M;

	err = ice_write_quad_reg_e822(hw, quad, Q_REG_TX_MEM_GBL_CFG, val);
	if (err) {
		ice_debug(hw, ICE_DBG_PTP, "Failed to write back TX_MEM_GBL_CFG, err %d\n",
			  err);
		return;
	}
}

 
static int ice_phy_cfg_uix_e822(struct ice_hw *hw, u8 port)
{
	u64 cur_freq, clk_incval, tu_per_sec, uix;
	int err;

	cur_freq = ice_e822_pll_freq(ice_e822_time_ref(hw));
	clk_incval = ice_ptp_read_src_incval(hw);

	 
	tu_per_sec = (cur_freq * clk_incval) >> 8;

#define LINE_UI_10G_40G 640  
#define LINE_UI_25G_100G 256  

	 
	uix = div_u64(tu_per_sec * LINE_UI_10G_40G, 390625000);

	err = ice_write_64b_phy_reg_e822(hw, port, P_REG_UIX66_10G_40G_L,
					 uix);
	if (err) {
		ice_debug(hw, ICE_DBG_PTP, "Failed to write UIX66_10G_40G, err %d\n",
			  err);
		return err;
	}

	 
	uix = div_u64(tu_per_sec * LINE_UI_25G_100G, 390625000);

	err = ice_write_64b_phy_reg_e822(hw, port, P_REG_UIX66_25G_100G_L,
					 uix);
	if (err) {
		ice_debug(hw, ICE_DBG_PTP, "Failed to write UIX66_25G_100G, err %d\n",
			  err);
		return err;
	}

	return 0;
}

 
static int ice_phy_cfg_parpcs_e822(struct ice_hw *hw, u8 port)
{
	u64 cur_freq, clk_incval, tu_per_sec, phy_tus;
	enum ice_ptp_link_spd link_spd;
	enum ice_ptp_fec_mode fec_mode;
	int err;

	err = ice_phy_get_speed_and_fec_e822(hw, port, &link_spd, &fec_mode);
	if (err)
		return err;

	cur_freq = ice_e822_pll_freq(ice_e822_time_ref(hw));
	clk_incval = ice_ptp_read_src_incval(hw);

	 
	tu_per_sec = cur_freq * clk_incval;

	 

	 
	if (e822_vernier[link_spd].tx_par_clk)
		phy_tus = div_u64(tu_per_sec,
				  e822_vernier[link_spd].tx_par_clk);
	else
		phy_tus = 0;

	err = ice_write_40b_phy_reg_e822(hw, port, P_REG_PAR_TX_TUS_L,
					 phy_tus);
	if (err)
		return err;

	 
	if (e822_vernier[link_spd].rx_par_clk)
		phy_tus = div_u64(tu_per_sec,
				  e822_vernier[link_spd].rx_par_clk);
	else
		phy_tus = 0;

	err = ice_write_40b_phy_reg_e822(hw, port, P_REG_PAR_RX_TUS_L,
					 phy_tus);
	if (err)
		return err;

	 
	if (e822_vernier[link_spd].tx_pcs_clk)
		phy_tus = div_u64(tu_per_sec,
				  e822_vernier[link_spd].tx_pcs_clk);
	else
		phy_tus = 0;

	err = ice_write_40b_phy_reg_e822(hw, port, P_REG_PCS_TX_TUS_L,
					 phy_tus);
	if (err)
		return err;

	 
	if (e822_vernier[link_spd].rx_pcs_clk)
		phy_tus = div_u64(tu_per_sec,
				  e822_vernier[link_spd].rx_pcs_clk);
	else
		phy_tus = 0;

	err = ice_write_40b_phy_reg_e822(hw, port, P_REG_PCS_RX_TUS_L,
					 phy_tus);
	if (err)
		return err;

	 
	if (e822_vernier[link_spd].tx_desk_rsgb_par)
		phy_tus = div_u64(tu_per_sec,
				  e822_vernier[link_spd].tx_desk_rsgb_par);
	else
		phy_tus = 0;

	err = ice_write_40b_phy_reg_e822(hw, port, P_REG_DESK_PAR_TX_TUS_L,
					 phy_tus);
	if (err)
		return err;

	 
	if (e822_vernier[link_spd].rx_desk_rsgb_par)
		phy_tus = div_u64(tu_per_sec,
				  e822_vernier[link_spd].rx_desk_rsgb_par);
	else
		phy_tus = 0;

	err = ice_write_40b_phy_reg_e822(hw, port, P_REG_DESK_PAR_RX_TUS_L,
					 phy_tus);
	if (err)
		return err;

	 
	if (e822_vernier[link_spd].tx_desk_rsgb_pcs)
		phy_tus = div_u64(tu_per_sec,
				  e822_vernier[link_spd].tx_desk_rsgb_pcs);
	else
		phy_tus = 0;

	err = ice_write_40b_phy_reg_e822(hw, port, P_REG_DESK_PCS_TX_TUS_L,
					 phy_tus);
	if (err)
		return err;

	 
	if (e822_vernier[link_spd].rx_desk_rsgb_pcs)
		phy_tus = div_u64(tu_per_sec,
				  e822_vernier[link_spd].rx_desk_rsgb_pcs);
	else
		phy_tus = 0;

	return ice_write_40b_phy_reg_e822(hw, port, P_REG_DESK_PCS_RX_TUS_L,
					  phy_tus);
}

 
static u64
ice_calc_fixed_tx_offset_e822(struct ice_hw *hw, enum ice_ptp_link_spd link_spd)
{
	u64 cur_freq, clk_incval, tu_per_sec, fixed_offset;

	cur_freq = ice_e822_pll_freq(ice_e822_time_ref(hw));
	clk_incval = ice_ptp_read_src_incval(hw);

	 
	tu_per_sec = cur_freq * clk_incval;

	 
	fixed_offset = div_u64(tu_per_sec, 10000);
	fixed_offset *= e822_vernier[link_spd].tx_fixed_delay;
	fixed_offset = div_u64(fixed_offset, 10000000);

	return fixed_offset;
}

 
int ice_phy_cfg_tx_offset_e822(struct ice_hw *hw, u8 port)
{
	enum ice_ptp_link_spd link_spd;
	enum ice_ptp_fec_mode fec_mode;
	u64 total_offset, val;
	int err;
	u32 reg;

	 
	err = ice_read_phy_reg_e822(hw, port, P_REG_TX_OR, &reg);
	if (err) {
		ice_debug(hw, ICE_DBG_PTP, "Failed to read TX_OR for port %u, err %d\n",
			  port, err);
		return err;
	}

	if (reg)
		return 0;

	err = ice_read_phy_reg_e822(hw, port, P_REG_TX_OV_STATUS, &reg);
	if (err) {
		ice_debug(hw, ICE_DBG_PTP, "Failed to read TX_OV_STATUS for port %u, err %d\n",
			  port, err);
		return err;
	}

	if (!(reg & P_REG_TX_OV_STATUS_OV_M))
		return -EBUSY;

	err = ice_phy_get_speed_and_fec_e822(hw, port, &link_spd, &fec_mode);
	if (err)
		return err;

	total_offset = ice_calc_fixed_tx_offset_e822(hw, link_spd);

	 
	if (link_spd == ICE_PTP_LNK_SPD_1G ||
	    link_spd == ICE_PTP_LNK_SPD_10G ||
	    link_spd == ICE_PTP_LNK_SPD_25G ||
	    link_spd == ICE_PTP_LNK_SPD_25G_RS ||
	    link_spd == ICE_PTP_LNK_SPD_40G ||
	    link_spd == ICE_PTP_LNK_SPD_50G) {
		err = ice_read_64b_phy_reg_e822(hw, port,
						P_REG_PAR_PCS_TX_OFFSET_L,
						&val);
		if (err)
			return err;

		total_offset += val;
	}

	 
	if (link_spd == ICE_PTP_LNK_SPD_50G_RS ||
	    link_spd == ICE_PTP_LNK_SPD_100G_RS) {
		err = ice_read_64b_phy_reg_e822(hw, port,
						P_REG_PAR_TX_TIME_L,
						&val);
		if (err)
			return err;

		total_offset += val;
	}

	 
	err = ice_write_64b_phy_reg_e822(hw, port, P_REG_TOTAL_TX_OFFSET_L,
					 total_offset);
	if (err)
		return err;

	err = ice_write_phy_reg_e822(hw, port, P_REG_TX_OR, 1);
	if (err)
		return err;

	dev_info(ice_hw_to_dev(hw), "Port=%d Tx vernier offset calibration complete\n",
		 port);

	return 0;
}

 
static int
ice_phy_calc_pmd_adj_e822(struct ice_hw *hw, u8 port,
			  enum ice_ptp_link_spd link_spd,
			  enum ice_ptp_fec_mode fec_mode, u64 *pmd_adj)
{
	u64 cur_freq, clk_incval, tu_per_sec, mult, adj;
	u8 pmd_align;
	u32 val;
	int err;

	err = ice_read_phy_reg_e822(hw, port, P_REG_PMD_ALIGNMENT, &val);
	if (err) {
		ice_debug(hw, ICE_DBG_PTP, "Failed to read PMD alignment, err %d\n",
			  err);
		return err;
	}

	pmd_align = (u8)val;

	cur_freq = ice_e822_pll_freq(ice_e822_time_ref(hw));
	clk_incval = ice_ptp_read_src_incval(hw);

	 
	tu_per_sec = cur_freq * clk_incval;

	 
	if (link_spd == ICE_PTP_LNK_SPD_1G) {
		if (pmd_align == 4)
			mult = 10;
		else
			mult = (pmd_align + 6) % 10;
	} else if (link_spd == ICE_PTP_LNK_SPD_10G ||
		   link_spd == ICE_PTP_LNK_SPD_25G ||
		   link_spd == ICE_PTP_LNK_SPD_40G ||
		   link_spd == ICE_PTP_LNK_SPD_50G) {
		 
		if (pmd_align != 65 || fec_mode == ICE_PTP_FEC_MODE_CLAUSE74)
			mult = pmd_align;
		else
			mult = 0;
	} else if (link_spd == ICE_PTP_LNK_SPD_25G_RS ||
		   link_spd == ICE_PTP_LNK_SPD_50G_RS ||
		   link_spd == ICE_PTP_LNK_SPD_100G_RS) {
		if (pmd_align < 17)
			mult = pmd_align + 40;
		else
			mult = pmd_align;
	} else {
		ice_debug(hw, ICE_DBG_PTP, "Unknown link speed %d, skipping PMD adjustment\n",
			  link_spd);
		mult = 0;
	}

	 
	if (!mult) {
		*pmd_adj = 0;
		return 0;
	}

	 
	adj = div_u64(tu_per_sec, 125);
	adj *= mult;
	adj = div_u64(adj, e822_vernier[link_spd].pmd_adj_divisor);

	 
	if (link_spd == ICE_PTP_LNK_SPD_25G_RS) {
		u64 cycle_adj;
		u8 rx_cycle;

		err = ice_read_phy_reg_e822(hw, port, P_REG_RX_40_TO_160_CNT,
					    &val);
		if (err) {
			ice_debug(hw, ICE_DBG_PTP, "Failed to read 25G-RS Rx cycle count, err %d\n",
				  err);
			return err;
		}

		rx_cycle = val & P_REG_RX_40_TO_160_CNT_RXCYC_M;
		if (rx_cycle) {
			mult = (4 - rx_cycle) * 40;

			cycle_adj = div_u64(tu_per_sec, 125);
			cycle_adj *= mult;
			cycle_adj = div_u64(cycle_adj, e822_vernier[link_spd].pmd_adj_divisor);

			adj += cycle_adj;
		}
	} else if (link_spd == ICE_PTP_LNK_SPD_50G_RS) {
		u64 cycle_adj;
		u8 rx_cycle;

		err = ice_read_phy_reg_e822(hw, port, P_REG_RX_80_TO_160_CNT,
					    &val);
		if (err) {
			ice_debug(hw, ICE_DBG_PTP, "Failed to read 50G-RS Rx cycle count, err %d\n",
				  err);
			return err;
		}

		rx_cycle = val & P_REG_RX_80_TO_160_CNT_RXCYC_M;
		if (rx_cycle) {
			mult = rx_cycle * 40;

			cycle_adj = div_u64(tu_per_sec, 125);
			cycle_adj *= mult;
			cycle_adj = div_u64(cycle_adj, e822_vernier[link_spd].pmd_adj_divisor);

			adj += cycle_adj;
		}
	}

	 
	*pmd_adj = adj;

	return 0;
}

 
static u64
ice_calc_fixed_rx_offset_e822(struct ice_hw *hw, enum ice_ptp_link_spd link_spd)
{
	u64 cur_freq, clk_incval, tu_per_sec, fixed_offset;

	cur_freq = ice_e822_pll_freq(ice_e822_time_ref(hw));
	clk_incval = ice_ptp_read_src_incval(hw);

	 
	tu_per_sec = cur_freq * clk_incval;

	 
	fixed_offset = div_u64(tu_per_sec, 10000);
	fixed_offset *= e822_vernier[link_spd].rx_fixed_delay;
	fixed_offset = div_u64(fixed_offset, 10000000);

	return fixed_offset;
}

 
int ice_phy_cfg_rx_offset_e822(struct ice_hw *hw, u8 port)
{
	enum ice_ptp_link_spd link_spd;
	enum ice_ptp_fec_mode fec_mode;
	u64 total_offset, pmd, val;
	int err;
	u32 reg;

	 
	err = ice_read_phy_reg_e822(hw, port, P_REG_RX_OR, &reg);
	if (err) {
		ice_debug(hw, ICE_DBG_PTP, "Failed to read RX_OR for port %u, err %d\n",
			  port, err);
		return err;
	}

	if (reg)
		return 0;

	err = ice_read_phy_reg_e822(hw, port, P_REG_RX_OV_STATUS, &reg);
	if (err) {
		ice_debug(hw, ICE_DBG_PTP, "Failed to read RX_OV_STATUS for port %u, err %d\n",
			  port, err);
		return err;
	}

	if (!(reg & P_REG_RX_OV_STATUS_OV_M))
		return -EBUSY;

	err = ice_phy_get_speed_and_fec_e822(hw, port, &link_spd, &fec_mode);
	if (err)
		return err;

	total_offset = ice_calc_fixed_rx_offset_e822(hw, link_spd);

	 
	err = ice_read_64b_phy_reg_e822(hw, port,
					P_REG_PAR_PCS_RX_OFFSET_L,
					&val);
	if (err)
		return err;

	total_offset += val;

	 
	if (link_spd == ICE_PTP_LNK_SPD_40G ||
	    link_spd == ICE_PTP_LNK_SPD_50G ||
	    link_spd == ICE_PTP_LNK_SPD_50G_RS ||
	    link_spd == ICE_PTP_LNK_SPD_100G_RS) {
		err = ice_read_64b_phy_reg_e822(hw, port,
						P_REG_PAR_RX_TIME_L,
						&val);
		if (err)
			return err;

		total_offset += val;
	}

	 
	err = ice_phy_calc_pmd_adj_e822(hw, port, link_spd, fec_mode, &pmd);
	if (err)
		return err;

	 
	if (fec_mode == ICE_PTP_FEC_MODE_RS_FEC)
		total_offset += pmd;
	else
		total_offset -= pmd;

	 
	err = ice_write_64b_phy_reg_e822(hw, port, P_REG_TOTAL_RX_OFFSET_L,
					 total_offset);
	if (err)
		return err;

	err = ice_write_phy_reg_e822(hw, port, P_REG_RX_OR, 1);
	if (err)
		return err;

	dev_info(ice_hw_to_dev(hw), "Port=%d Rx vernier offset calibration complete\n",
		 port);

	return 0;
}

 
static int
ice_read_phy_and_phc_time_e822(struct ice_hw *hw, u8 port, u64 *phy_time,
			       u64 *phc_time)
{
	u64 tx_time, rx_time;
	u32 zo, lo;
	u8 tmr_idx;
	int err;

	tmr_idx = ice_get_ptp_src_clock_index(hw);

	 
	ice_ptp_src_cmd(hw, READ_TIME);

	 
	err = ice_ptp_one_port_cmd(hw, port, READ_TIME);
	if (err)
		return err;

	 
	ice_ptp_exec_tmr_cmd(hw);

	 
	zo = rd32(hw, GLTSYN_SHTIME_0(tmr_idx));
	lo = rd32(hw, GLTSYN_SHTIME_L(tmr_idx));
	*phc_time = (u64)lo << 32 | zo;

	 
	err = ice_ptp_read_port_capture(hw, port, &tx_time, &rx_time);
	if (err)
		return err;

	 
	if (tx_time != rx_time)
		dev_warn(ice_hw_to_dev(hw),
			 "PHY port %u Tx and Rx timers do not match, tx_time 0x%016llX, rx_time 0x%016llX\n",
			 port, (unsigned long long)tx_time,
			 (unsigned long long)rx_time);

	*phy_time = tx_time;

	return 0;
}

 
static int ice_sync_phy_timer_e822(struct ice_hw *hw, u8 port)
{
	u64 phc_time, phy_time, difference;
	int err;

	if (!ice_ptp_lock(hw)) {
		ice_debug(hw, ICE_DBG_PTP, "Failed to acquire PTP semaphore\n");
		return -EBUSY;
	}

	err = ice_read_phy_and_phc_time_e822(hw, port, &phy_time, &phc_time);
	if (err)
		goto err_unlock;

	 
	difference = phc_time - phy_time;

	err = ice_ptp_prep_port_adj_e822(hw, port, (s64)difference);
	if (err)
		goto err_unlock;

	err = ice_ptp_one_port_cmd(hw, port, ADJ_TIME);
	if (err)
		goto err_unlock;

	 
	ice_ptp_src_cmd(hw, ICE_PTP_NOP);

	 
	ice_ptp_exec_tmr_cmd(hw);

	 
	err = ice_read_phy_and_phc_time_e822(hw, port, &phy_time, &phc_time);
	if (err)
		goto err_unlock;

	dev_info(ice_hw_to_dev(hw),
		 "Port %u PHY time synced to PHC: 0x%016llX, 0x%016llX\n",
		 port, (unsigned long long)phy_time,
		 (unsigned long long)phc_time);

	ice_ptp_unlock(hw);

	return 0;

err_unlock:
	ice_ptp_unlock(hw);
	return err;
}

 
int
ice_stop_phy_timer_e822(struct ice_hw *hw, u8 port, bool soft_reset)
{
	int err;
	u32 val;

	err = ice_write_phy_reg_e822(hw, port, P_REG_TX_OR, 0);
	if (err)
		return err;

	err = ice_write_phy_reg_e822(hw, port, P_REG_RX_OR, 0);
	if (err)
		return err;

	err = ice_read_phy_reg_e822(hw, port, P_REG_PS, &val);
	if (err)
		return err;

	val &= ~P_REG_PS_START_M;
	err = ice_write_phy_reg_e822(hw, port, P_REG_PS, val);
	if (err)
		return err;

	val &= ~P_REG_PS_ENA_CLK_M;
	err = ice_write_phy_reg_e822(hw, port, P_REG_PS, val);
	if (err)
		return err;

	if (soft_reset) {
		val |= P_REG_PS_SFT_RESET_M;
		err = ice_write_phy_reg_e822(hw, port, P_REG_PS, val);
		if (err)
			return err;
	}

	ice_debug(hw, ICE_DBG_PTP, "Disabled clock on PHY port %u\n", port);

	return 0;
}

 
int ice_start_phy_timer_e822(struct ice_hw *hw, u8 port)
{
	u32 lo, hi, val;
	u64 incval;
	u8 tmr_idx;
	int err;

	tmr_idx = ice_get_ptp_src_clock_index(hw);

	err = ice_stop_phy_timer_e822(hw, port, false);
	if (err)
		return err;

	ice_phy_cfg_lane_e822(hw, port);

	err = ice_phy_cfg_uix_e822(hw, port);
	if (err)
		return err;

	err = ice_phy_cfg_parpcs_e822(hw, port);
	if (err)
		return err;

	lo = rd32(hw, GLTSYN_INCVAL_L(tmr_idx));
	hi = rd32(hw, GLTSYN_INCVAL_H(tmr_idx));
	incval = (u64)hi << 32 | lo;

	err = ice_write_40b_phy_reg_e822(hw, port, P_REG_TIMETUS_L, incval);
	if (err)
		return err;

	err = ice_ptp_one_port_cmd(hw, port, INIT_INCVAL);
	if (err)
		return err;

	 
	ice_ptp_src_cmd(hw, ICE_PTP_NOP);

	ice_ptp_exec_tmr_cmd(hw);

	err = ice_read_phy_reg_e822(hw, port, P_REG_PS, &val);
	if (err)
		return err;

	val |= P_REG_PS_SFT_RESET_M;
	err = ice_write_phy_reg_e822(hw, port, P_REG_PS, val);
	if (err)
		return err;

	val |= P_REG_PS_START_M;
	err = ice_write_phy_reg_e822(hw, port, P_REG_PS, val);
	if (err)
		return err;

	val &= ~P_REG_PS_SFT_RESET_M;
	err = ice_write_phy_reg_e822(hw, port, P_REG_PS, val);
	if (err)
		return err;

	err = ice_ptp_one_port_cmd(hw, port, INIT_INCVAL);
	if (err)
		return err;

	ice_ptp_exec_tmr_cmd(hw);

	val |= P_REG_PS_ENA_CLK_M;
	err = ice_write_phy_reg_e822(hw, port, P_REG_PS, val);
	if (err)
		return err;

	val |= P_REG_PS_LOAD_OFFSET_M;
	err = ice_write_phy_reg_e822(hw, port, P_REG_PS, val);
	if (err)
		return err;

	ice_ptp_exec_tmr_cmd(hw);

	err = ice_sync_phy_timer_e822(hw, port);
	if (err)
		return err;

	ice_debug(hw, ICE_DBG_PTP, "Enabled clock on PHY port %u\n", port);

	return 0;
}

 
static int
ice_get_phy_tx_tstamp_ready_e822(struct ice_hw *hw, u8 quad, u64 *tstamp_ready)
{
	u32 hi, lo;
	int err;

	err = ice_read_quad_reg_e822(hw, quad, Q_REG_TX_MEMORY_STATUS_U, &hi);
	if (err) {
		ice_debug(hw, ICE_DBG_PTP, "Failed to read TX_MEMORY_STATUS_U for quad %u, err %d\n",
			  quad, err);
		return err;
	}

	err = ice_read_quad_reg_e822(hw, quad, Q_REG_TX_MEMORY_STATUS_L, &lo);
	if (err) {
		ice_debug(hw, ICE_DBG_PTP, "Failed to read TX_MEMORY_STATUS_L for quad %u, err %d\n",
			  quad, err);
		return err;
	}

	*tstamp_ready = (u64)hi << 32 | (u64)lo;

	return 0;
}

 

 
static int ice_read_phy_reg_e810(struct ice_hw *hw, u32 addr, u32 *val)
{
	struct ice_sbq_msg_input msg = {0};
	int err;

	msg.msg_addr_low = lower_16_bits(addr);
	msg.msg_addr_high = upper_16_bits(addr);
	msg.opcode = ice_sbq_msg_rd;
	msg.dest_dev = rmn_0;

	err = ice_sbq_rw_reg(hw, &msg);
	if (err) {
		ice_debug(hw, ICE_DBG_PTP, "Failed to send message to PHY, err %d\n",
			  err);
		return err;
	}

	*val = msg.data;

	return 0;
}

 
static int ice_write_phy_reg_e810(struct ice_hw *hw, u32 addr, u32 val)
{
	struct ice_sbq_msg_input msg = {0};
	int err;

	msg.msg_addr_low = lower_16_bits(addr);
	msg.msg_addr_high = upper_16_bits(addr);
	msg.opcode = ice_sbq_msg_wr;
	msg.dest_dev = rmn_0;
	msg.data = val;

	err = ice_sbq_rw_reg(hw, &msg);
	if (err) {
		ice_debug(hw, ICE_DBG_PTP, "Failed to send message to PHY, err %d\n",
			  err);
		return err;
	}

	return 0;
}

 
static int
ice_read_phy_tstamp_ll_e810(struct ice_hw *hw, u8 idx, u8 *hi, u32 *lo)
{
	u32 val;
	u8 i;

	 
	val = FIELD_PREP(TS_LL_READ_TS_IDX, idx) | TS_LL_READ_TS;
	wr32(hw, PF_SB_ATQBAL, val);

	 
	for (i = TS_LL_READ_RETRIES; i > 0; i--) {
		val = rd32(hw, PF_SB_ATQBAL);

		 
		if (!(FIELD_GET(TS_LL_READ_TS, val))) {
			 
			*hi = FIELD_GET(TS_LL_READ_TS_HIGH, val);

			 
			*lo = rd32(hw, PF_SB_ATQBAH) | TS_VALID;
			return 0;
		}

		udelay(10);
	}

	 
	ice_debug(hw, ICE_DBG_PTP, "Failed to read PTP timestamp using low latency read\n");
	return -EINVAL;
}

 
static int
ice_read_phy_tstamp_sbq_e810(struct ice_hw *hw, u8 lport, u8 idx, u8 *hi,
			     u32 *lo)
{
	u32 hi_addr = TS_EXT(HIGH_TX_MEMORY_BANK_START, lport, idx);
	u32 lo_addr = TS_EXT(LOW_TX_MEMORY_BANK_START, lport, idx);
	u32 lo_val, hi_val;
	int err;

	err = ice_read_phy_reg_e810(hw, lo_addr, &lo_val);
	if (err) {
		ice_debug(hw, ICE_DBG_PTP, "Failed to read low PTP timestamp register, err %d\n",
			  err);
		return err;
	}

	err = ice_read_phy_reg_e810(hw, hi_addr, &hi_val);
	if (err) {
		ice_debug(hw, ICE_DBG_PTP, "Failed to read high PTP timestamp register, err %d\n",
			  err);
		return err;
	}

	*lo = lo_val;
	*hi = (u8)hi_val;

	return 0;
}

 
static int
ice_read_phy_tstamp_e810(struct ice_hw *hw, u8 lport, u8 idx, u64 *tstamp)
{
	u32 lo = 0;
	u8 hi = 0;
	int err;

	if (hw->dev_caps.ts_dev_info.ts_ll_read)
		err = ice_read_phy_tstamp_ll_e810(hw, idx, &hi, &lo);
	else
		err = ice_read_phy_tstamp_sbq_e810(hw, lport, idx, &hi, &lo);

	if (err)
		return err;

	 
	*tstamp = ((u64)hi) << TS_HIGH_S | ((u64)lo & TS_LOW_M);

	return 0;
}

 
static int ice_clear_phy_tstamp_e810(struct ice_hw *hw, u8 lport, u8 idx)
{
	u32 lo_addr, hi_addr;
	int err;

	lo_addr = TS_EXT(LOW_TX_MEMORY_BANK_START, lport, idx);
	hi_addr = TS_EXT(HIGH_TX_MEMORY_BANK_START, lport, idx);

	err = ice_write_phy_reg_e810(hw, lo_addr, 0);
	if (err) {
		ice_debug(hw, ICE_DBG_PTP, "Failed to clear low PTP timestamp register, err %d\n",
			  err);
		return err;
	}

	err = ice_write_phy_reg_e810(hw, hi_addr, 0);
	if (err) {
		ice_debug(hw, ICE_DBG_PTP, "Failed to clear high PTP timestamp register, err %d\n",
			  err);
		return err;
	}

	return 0;
}

 
int ice_ptp_init_phy_e810(struct ice_hw *hw)
{
	u8 tmr_idx;
	int err;

	tmr_idx = hw->func_caps.ts_func_info.tmr_index_owned;
	err = ice_write_phy_reg_e810(hw, ETH_GLTSYN_ENA(tmr_idx),
				     GLTSYN_ENA_TSYN_ENA_M);
	if (err)
		ice_debug(hw, ICE_DBG_PTP, "PTP failed in ena_phy_time_syn %d\n",
			  err);

	return err;
}

 
static int ice_ptp_init_phc_e810(struct ice_hw *hw)
{
	 
	wr32(hw, GLTSYN_SYNC_DLAY, 0);

	 
	return ice_ptp_init_phy_e810(hw);
}

 
static int ice_ptp_prep_phy_time_e810(struct ice_hw *hw, u32 time)
{
	u8 tmr_idx;
	int err;

	tmr_idx = hw->func_caps.ts_func_info.tmr_index_owned;
	err = ice_write_phy_reg_e810(hw, ETH_GLTSYN_SHTIME_0(tmr_idx), 0);
	if (err) {
		ice_debug(hw, ICE_DBG_PTP, "Failed to write SHTIME_0, err %d\n",
			  err);
		return err;
	}

	err = ice_write_phy_reg_e810(hw, ETH_GLTSYN_SHTIME_L(tmr_idx), time);
	if (err) {
		ice_debug(hw, ICE_DBG_PTP, "Failed to write SHTIME_L, err %d\n",
			  err);
		return err;
	}

	return 0;
}

 
static int ice_ptp_prep_phy_adj_e810(struct ice_hw *hw, s32 adj)
{
	u8 tmr_idx;
	int err;

	tmr_idx = hw->func_caps.ts_func_info.tmr_index_owned;

	 
	err = ice_write_phy_reg_e810(hw, ETH_GLTSYN_SHADJ_L(tmr_idx), 0);
	if (err) {
		ice_debug(hw, ICE_DBG_PTP, "Failed to write adj to PHY SHADJ_L, err %d\n",
			  err);
		return err;
	}

	err = ice_write_phy_reg_e810(hw, ETH_GLTSYN_SHADJ_H(tmr_idx), adj);
	if (err) {
		ice_debug(hw, ICE_DBG_PTP, "Failed to write adj to PHY SHADJ_H, err %d\n",
			  err);
		return err;
	}

	return 0;
}

 
static int ice_ptp_prep_phy_incval_e810(struct ice_hw *hw, u64 incval)
{
	u32 high, low;
	u8 tmr_idx;
	int err;

	tmr_idx = hw->func_caps.ts_func_info.tmr_index_owned;
	low = lower_32_bits(incval);
	high = upper_32_bits(incval);

	err = ice_write_phy_reg_e810(hw, ETH_GLTSYN_SHADJ_L(tmr_idx), low);
	if (err) {
		ice_debug(hw, ICE_DBG_PTP, "Failed to write incval to PHY SHADJ_L, err %d\n",
			  err);
		return err;
	}

	err = ice_write_phy_reg_e810(hw, ETH_GLTSYN_SHADJ_H(tmr_idx), high);
	if (err) {
		ice_debug(hw, ICE_DBG_PTP, "Failed to write incval PHY SHADJ_H, err %d\n",
			  err);
		return err;
	}

	return 0;
}

 
static int ice_ptp_port_cmd_e810(struct ice_hw *hw, enum ice_ptp_tmr_cmd cmd)
{
	u32 cmd_val, val;
	int err;

	switch (cmd) {
	case INIT_TIME:
		cmd_val = GLTSYN_CMD_INIT_TIME;
		break;
	case INIT_INCVAL:
		cmd_val = GLTSYN_CMD_INIT_INCVAL;
		break;
	case ADJ_TIME:
		cmd_val = GLTSYN_CMD_ADJ_TIME;
		break;
	case READ_TIME:
		cmd_val = GLTSYN_CMD_READ_TIME;
		break;
	case ADJ_TIME_AT_TIME:
		cmd_val = GLTSYN_CMD_ADJ_INIT_TIME;
		break;
	case ICE_PTP_NOP:
		return 0;
	}

	 
	err = ice_read_phy_reg_e810(hw, ETH_GLTSYN_CMD, &val);
	if (err) {
		ice_debug(hw, ICE_DBG_PTP, "Failed to read GLTSYN_CMD, err %d\n", err);
		return err;
	}

	 
	val &= ~TS_CMD_MASK_E810;
	val |= cmd_val;

	err = ice_write_phy_reg_e810(hw, ETH_GLTSYN_CMD, val);
	if (err) {
		ice_debug(hw, ICE_DBG_PTP, "Failed to write back GLTSYN_CMD, err %d\n", err);
		return err;
	}

	return 0;
}

 
static int
ice_get_phy_tx_tstamp_ready_e810(struct ice_hw *hw, u8 port, u64 *tstamp_ready)
{
	*tstamp_ready = 0xFFFFFFFFFFFFFFFF;
	return 0;
}

 

 
static int
ice_get_pca9575_handle(struct ice_hw *hw, u16 *pca9575_handle)
{
	struct ice_aqc_get_link_topo *cmd;
	struct ice_aq_desc desc;
	int status;
	u8 idx;

	 
	if (hw->io_expander_handle) {
		*pca9575_handle = hw->io_expander_handle;
		return 0;
	}

	 
	cmd = &desc.params.get_link_topo;
	ice_fill_dflt_direct_cmd_desc(&desc, ice_aqc_opc_get_link_topo);

	 
	cmd->addr.topo_params.node_type_ctx =
		(ICE_AQC_LINK_TOPO_NODE_TYPE_M &
		 ICE_AQC_LINK_TOPO_NODE_TYPE_GPIO_CTRL);

#define SW_PCA9575_SFP_TOPO_IDX		2
#define SW_PCA9575_QSFP_TOPO_IDX	1

	 
	if (hw->device_id == ICE_DEV_ID_E810C_SFP)
		idx = SW_PCA9575_SFP_TOPO_IDX;
	else if (hw->device_id == ICE_DEV_ID_E810C_QSFP)
		idx = SW_PCA9575_QSFP_TOPO_IDX;
	else
		return -EOPNOTSUPP;

	cmd->addr.topo_params.index = idx;

	status = ice_aq_send_cmd(hw, &desc, NULL, 0, NULL);
	if (status)
		return -EOPNOTSUPP;

	 
	if (desc.params.get_link_topo.node_part_num !=
		ICE_AQC_GET_LINK_TOPO_NODE_NR_PCA9575)
		return -EOPNOTSUPP;

	 
	hw->io_expander_handle =
		le16_to_cpu(desc.params.get_link_topo.addr.handle);
	*pca9575_handle = hw->io_expander_handle;

	return 0;
}

 
int ice_read_sma_ctrl_e810t(struct ice_hw *hw, u8 *data)
{
	int status;
	u16 handle;
	u8 i;

	status = ice_get_pca9575_handle(hw, &handle);
	if (status)
		return status;

	*data = 0;

	for (i = ICE_SMA_MIN_BIT_E810T; i <= ICE_SMA_MAX_BIT_E810T; i++) {
		bool pin;

		status = ice_aq_get_gpio(hw, handle, i + ICE_PCA9575_P1_OFFSET,
					 &pin, NULL);
		if (status)
			break;
		*data |= (u8)(!pin) << i;
	}

	return status;
}

 
int ice_write_sma_ctrl_e810t(struct ice_hw *hw, u8 data)
{
	int status;
	u16 handle;
	u8 i;

	status = ice_get_pca9575_handle(hw, &handle);
	if (status)
		return status;

	for (i = ICE_SMA_MIN_BIT_E810T; i <= ICE_SMA_MAX_BIT_E810T; i++) {
		bool pin;

		pin = !(data & (1 << i));
		status = ice_aq_set_gpio(hw, handle, i + ICE_PCA9575_P1_OFFSET,
					 pin, NULL);
		if (status)
			break;
	}

	return status;
}

 
int ice_read_pca9575_reg_e810t(struct ice_hw *hw, u8 offset, u8 *data)
{
	struct ice_aqc_link_topo_addr link_topo;
	__le16 addr;
	u16 handle;
	int err;

	memset(&link_topo, 0, sizeof(link_topo));

	err = ice_get_pca9575_handle(hw, &handle);
	if (err)
		return err;

	link_topo.handle = cpu_to_le16(handle);
	link_topo.topo_params.node_type_ctx =
		FIELD_PREP(ICE_AQC_LINK_TOPO_NODE_CTX_M,
			   ICE_AQC_LINK_TOPO_NODE_CTX_PROVIDED);

	addr = cpu_to_le16((u16)offset);

	return ice_aq_read_i2c(hw, link_topo, 0, addr, 1, data, NULL);
}

 

 
bool ice_ptp_lock(struct ice_hw *hw)
{
	u32 hw_lock;
	int i;

#define MAX_TRIES 15

	for (i = 0; i < MAX_TRIES; i++) {
		hw_lock = rd32(hw, PFTSYN_SEM + (PFTSYN_SEM_BYTES * hw->pf_id));
		hw_lock = hw_lock & PFTSYN_SEM_BUSY_M;
		if (hw_lock) {
			 
			usleep_range(5000, 6000);
			continue;
		}

		break;
	}

	return !hw_lock;
}

 
void ice_ptp_unlock(struct ice_hw *hw)
{
	wr32(hw, PFTSYN_SEM + (PFTSYN_SEM_BYTES * hw->pf_id), 0);
}

 
static int ice_ptp_tmr_cmd(struct ice_hw *hw, enum ice_ptp_tmr_cmd cmd)
{
	int err;

	 
	ice_ptp_src_cmd(hw, cmd);

	 
	if (ice_is_e810(hw))
		err = ice_ptp_port_cmd_e810(hw, cmd);
	else
		err = ice_ptp_port_cmd_e822(hw, cmd);
	if (err) {
		ice_debug(hw, ICE_DBG_PTP, "Failed to prepare PHY ports for timer command %u, err %d\n",
			  cmd, err);
		return err;
	}

	 
	ice_ptp_exec_tmr_cmd(hw);

	return 0;
}

 
int ice_ptp_init_time(struct ice_hw *hw, u64 time)
{
	u8 tmr_idx;
	int err;

	tmr_idx = hw->func_caps.ts_func_info.tmr_index_owned;

	 
	wr32(hw, GLTSYN_SHTIME_L(tmr_idx), lower_32_bits(time));
	wr32(hw, GLTSYN_SHTIME_H(tmr_idx), upper_32_bits(time));
	wr32(hw, GLTSYN_SHTIME_0(tmr_idx), 0);

	 
	 
	if (ice_is_e810(hw))
		err = ice_ptp_prep_phy_time_e810(hw, time & 0xFFFFFFFF);
	else
		err = ice_ptp_prep_phy_time_e822(hw, time & 0xFFFFFFFF);
	if (err)
		return err;

	return ice_ptp_tmr_cmd(hw, INIT_TIME);
}

 
int ice_ptp_write_incval(struct ice_hw *hw, u64 incval)
{
	u8 tmr_idx;
	int err;

	tmr_idx = hw->func_caps.ts_func_info.tmr_index_owned;

	 
	wr32(hw, GLTSYN_SHADJ_L(tmr_idx), lower_32_bits(incval));
	wr32(hw, GLTSYN_SHADJ_H(tmr_idx), upper_32_bits(incval));

	if (ice_is_e810(hw))
		err = ice_ptp_prep_phy_incval_e810(hw, incval);
	else
		err = ice_ptp_prep_phy_incval_e822(hw, incval);
	if (err)
		return err;

	return ice_ptp_tmr_cmd(hw, INIT_INCVAL);
}

 
int ice_ptp_write_incval_locked(struct ice_hw *hw, u64 incval)
{
	int err;

	if (!ice_ptp_lock(hw))
		return -EBUSY;

	err = ice_ptp_write_incval(hw, incval);

	ice_ptp_unlock(hw);

	return err;
}

 
int ice_ptp_adj_clock(struct ice_hw *hw, s32 adj)
{
	u8 tmr_idx;
	int err;

	tmr_idx = hw->func_caps.ts_func_info.tmr_index_owned;

	 
	wr32(hw, GLTSYN_SHADJ_L(tmr_idx), 0);
	wr32(hw, GLTSYN_SHADJ_H(tmr_idx), adj);

	if (ice_is_e810(hw))
		err = ice_ptp_prep_phy_adj_e810(hw, adj);
	else
		err = ice_ptp_prep_phy_adj_e822(hw, adj);
	if (err)
		return err;

	return ice_ptp_tmr_cmd(hw, ADJ_TIME);
}

 
int ice_read_phy_tstamp(struct ice_hw *hw, u8 block, u8 idx, u64 *tstamp)
{
	if (ice_is_e810(hw))
		return ice_read_phy_tstamp_e810(hw, block, idx, tstamp);
	else
		return ice_read_phy_tstamp_e822(hw, block, idx, tstamp);
}

 
int ice_clear_phy_tstamp(struct ice_hw *hw, u8 block, u8 idx)
{
	if (ice_is_e810(hw))
		return ice_clear_phy_tstamp_e810(hw, block, idx);
	else
		return ice_clear_phy_tstamp_e822(hw, block, idx);
}

 
void ice_ptp_reset_ts_memory(struct ice_hw *hw)
{
	if (ice_is_e810(hw))
		return;

	ice_ptp_reset_ts_memory_e822(hw);
}

 
int ice_ptp_init_phc(struct ice_hw *hw)
{
	u8 src_idx = hw->func_caps.ts_func_info.tmr_index_owned;

	 
	wr32(hw, GLTSYN_ENA(src_idx), GLTSYN_ENA_TSYN_ENA_M);

	 
	(void)rd32(hw, GLTSYN_STAT(src_idx));

	if (ice_is_e810(hw))
		return ice_ptp_init_phc_e810(hw);
	else
		return ice_ptp_init_phc_e822(hw);
}

 
int ice_get_phy_tx_tstamp_ready(struct ice_hw *hw, u8 block, u64 *tstamp_ready)
{
	if (ice_is_e810(hw))
		return ice_get_phy_tx_tstamp_ready_e810(hw, block,
							tstamp_ready);
	else
		return ice_get_phy_tx_tstamp_ready_e822(hw, block,
							tstamp_ready);
}
