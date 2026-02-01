
 

#include "mcs.h"
#include "mcs_reg.h"

static struct mcs_ops cnf10kb_mcs_ops   = {
	.mcs_set_hw_capabilities	= cnf10kb_mcs_set_hw_capabilities,
	.mcs_parser_cfg			= cnf10kb_mcs_parser_cfg,
	.mcs_tx_sa_mem_map_write	= cnf10kb_mcs_tx_sa_mem_map_write,
	.mcs_rx_sa_mem_map_write	= cnf10kb_mcs_rx_sa_mem_map_write,
	.mcs_flowid_secy_map		= cnf10kb_mcs_flowid_secy_map,
	.mcs_bbe_intr_handler		= cnf10kb_mcs_bbe_intr_handler,
	.mcs_pab_intr_handler		= cnf10kb_mcs_pab_intr_handler,
};

struct mcs_ops *cnf10kb_get_mac_ops(void)
{
	return &cnf10kb_mcs_ops;
}

void cnf10kb_mcs_set_hw_capabilities(struct mcs *mcs)
{
	struct hwinfo *hw = mcs->hw;

	hw->tcam_entries = 64;		 
	hw->secy_entries  = 64;		 
	hw->sc_entries = 64;		 
	hw->sa_entries = 128;		 
	hw->lmac_cnt = 4;		 
	hw->mcs_x2p_intf = 1;		 
	hw->mcs_blks = 7;		 
	hw->ip_vec = MCS_CNF10KB_INT_VEC_IP;  
}

void cnf10kb_mcs_parser_cfg(struct mcs *mcs)
{
	u64 reg, val;

	 
	val = (0x8100ull & 0xFFFF) | BIT_ULL(20) | BIT_ULL(22);

	reg = MCSX_PEX_RX_SLAVE_CUSTOM_TAGX(0);
	mcs_reg_write(mcs, reg, val);

	reg = MCSX_PEX_TX_SLAVE_CUSTOM_TAGX(0);
	mcs_reg_write(mcs, reg, val);

	 
	val = (0x88a8ull & 0xFFFF) | BIT_ULL(20) | BIT_ULL(23);

	 
	reg = MCSX_PEX_RX_SLAVE_CUSTOM_TAGX(1);
	mcs_reg_write(mcs, reg, val);

	 
	reg = MCSX_PEX_TX_SLAVE_CUSTOM_TAGX(1);
	mcs_reg_write(mcs, reg, val);

	 
	val = BIT_ULL(0) | BIT_ULL(1) | BIT_ULL(12);

	reg = MCSX_PEX_RX_SLAVE_ETYPE_ENABLE;
	mcs_reg_write(mcs, reg, val);

	reg = MCSX_PEX_TX_SLAVE_ETYPE_ENABLE;
	mcs_reg_write(mcs, reg, val);
}

void cnf10kb_mcs_flowid_secy_map(struct mcs *mcs, struct secy_mem_map *map, int dir)
{
	u64 reg, val;

	val = (map->secy & 0x3F) | (map->ctrl_pkt & 0x1) << 6;
	if (dir == MCS_RX) {
		reg = MCSX_CPM_RX_SLAVE_SECY_MAP_MEMX(map->flow_id);
	} else {
		reg = MCSX_CPM_TX_SLAVE_SECY_MAP_MEM_0X(map->flow_id);
		mcs_reg_write(mcs, reg, map->sci);
		val |= (map->sc & 0x3F) << 7;
		reg = MCSX_CPM_TX_SLAVE_SECY_MAP_MEM_1X(map->flow_id);
	}

	mcs_reg_write(mcs, reg, val);
}

void cnf10kb_mcs_tx_sa_mem_map_write(struct mcs *mcs, struct mcs_tx_sc_sa_map *map)
{
	u64 reg, val;

	val = (map->sa_index0 & 0x7F) | (map->sa_index1 & 0x7F) << 7;

	reg = MCSX_CPM_TX_SLAVE_SA_MAP_MEM_0X(map->sc_id);
	mcs_reg_write(mcs, reg, val);

	reg = MCSX_CPM_TX_SLAVE_AUTO_REKEY_ENABLE_0;
	val = mcs_reg_read(mcs, reg);

	if (map->rekey_ena)
		val |= BIT_ULL(map->sc_id);
	else
		val &= ~BIT_ULL(map->sc_id);

	mcs_reg_write(mcs, reg, val);

	mcs_reg_write(mcs, MCSX_CPM_TX_SLAVE_SA_INDEX0_VLDX(map->sc_id), map->sa_index0_vld);
	mcs_reg_write(mcs, MCSX_CPM_TX_SLAVE_SA_INDEX1_VLDX(map->sc_id), map->sa_index1_vld);

	mcs_reg_write(mcs, MCSX_CPM_TX_SLAVE_TX_SA_ACTIVEX(map->sc_id), map->tx_sa_active);
}

void cnf10kb_mcs_rx_sa_mem_map_write(struct mcs *mcs, struct mcs_rx_sc_sa_map *map)
{
	u64 val, reg;

	val = (map->sa_index & 0x7F) | (map->sa_in_use << 7);

	reg = MCSX_CPM_RX_SLAVE_SA_MAP_MEMX((4 * map->sc_id) + map->an);
	mcs_reg_write(mcs, reg, val);
}

int mcs_set_force_clk_en(struct mcs *mcs, bool set)
{
	unsigned long timeout = jiffies + usecs_to_jiffies(2000);
	u64 val;

	val = mcs_reg_read(mcs, MCSX_MIL_GLOBAL);

	if (set) {
		val |= BIT_ULL(4);
		mcs_reg_write(mcs, MCSX_MIL_GLOBAL, val);

		 
		while (!(mcs_reg_read(mcs, MCSX_MIL_IP_GBL_STATUS) & BIT_ULL(0))) {
			if (time_after(jiffies, timeout)) {
				dev_err(mcs->dev, "MCS set force clk enable failed\n");
				break;
			}
		}
	} else {
		val &= ~BIT_ULL(4);
		mcs_reg_write(mcs, MCSX_MIL_GLOBAL, val);
	}

	return 0;
}

 
void cnf10kb_mcs_tx_pn_thresh_reached_handler(struct mcs *mcs)
{
	struct mcs_intr_event event;
	struct rsrc_bmap *sc_bmap;
	unsigned long rekey_ena;
	u64 val, sa_status;
	int sc;

	sc_bmap = &mcs->tx.sc;

	event.mcs_id = mcs->mcs_id;
	event.intr_mask = MCS_CPM_TX_PN_THRESH_REACHED_INT;

	rekey_ena = mcs_reg_read(mcs, MCSX_CPM_TX_SLAVE_AUTO_REKEY_ENABLE_0);

	for_each_set_bit(sc, sc_bmap->bmap, mcs->hw->sc_entries) {
		 
		if (!test_bit(sc, &rekey_ena))
			continue;
		sa_status = mcs_reg_read(mcs, MCSX_CPM_TX_SLAVE_TX_SA_ACTIVEX(sc));
		 
		if (sa_status == mcs->tx_sa_active[sc])
			continue;

		 
		val = mcs_reg_read(mcs, MCSX_CPM_TX_SLAVE_SA_MAP_MEM_0X(sc));
		if (sa_status)
			event.sa_id = val & 0x7F;
		else
			event.sa_id = (val >> 7) & 0x7F;

		event.pcifunc = mcs->tx.sa2pf_map[event.sa_id];
		mcs_add_intr_wq_entry(mcs, &event);
	}
}

void cnf10kb_mcs_tx_pn_wrapped_handler(struct mcs *mcs)
{
	struct mcs_intr_event event = { 0 };
	struct rsrc_bmap *sc_bmap;
	u64 val;
	int sc;

	sc_bmap = &mcs->tx.sc;

	event.mcs_id = mcs->mcs_id;
	event.intr_mask = MCS_CPM_TX_PACKET_XPN_EQ0_INT;

	for_each_set_bit(sc, sc_bmap->bmap, mcs->hw->sc_entries) {
		val = mcs_reg_read(mcs, MCSX_CPM_TX_SLAVE_SA_MAP_MEM_0X(sc));

		if (mcs->tx_sa_active[sc])
			 
			event.sa_id = (val >> 7) & 0x7F;
		else
			 
			event.sa_id = val & 0x7F;

		event.pcifunc = mcs->tx.sa2pf_map[event.sa_id];
		mcs_add_intr_wq_entry(mcs, &event);
	}
}

void cnf10kb_mcs_bbe_intr_handler(struct mcs *mcs, u64 intr,
				  enum mcs_direction dir)
{
	struct mcs_intr_event event = { 0 };
	int i;

	if (!(intr & MCS_BBE_INT_MASK))
		return;

	event.mcs_id = mcs->mcs_id;
	event.pcifunc = mcs->pf_map[0];

	for (i = 0; i < MCS_MAX_BBE_INT; i++) {
		if (!(intr & BIT_ULL(i)))
			continue;

		 
		if (intr & 0xFULL)
			event.intr_mask = (dir == MCS_RX) ?
					  MCS_BBE_RX_DFIFO_OVERFLOW_INT :
					  MCS_BBE_TX_DFIFO_OVERFLOW_INT;
		else
			event.intr_mask = (dir == MCS_RX) ?
					  MCS_BBE_RX_PLFIFO_OVERFLOW_INT :
					  MCS_BBE_TX_PLFIFO_OVERFLOW_INT;

		 
		event.lmac_id = i & 0x3ULL;
		mcs_add_intr_wq_entry(mcs, &event);
	}
}

void cnf10kb_mcs_pab_intr_handler(struct mcs *mcs, u64 intr,
				  enum mcs_direction dir)
{
	struct mcs_intr_event event = { 0 };
	int i;

	if (!(intr & MCS_PAB_INT_MASK))
		return;

	event.mcs_id = mcs->mcs_id;
	event.pcifunc = mcs->pf_map[0];

	for (i = 0; i < MCS_MAX_PAB_INT; i++) {
		if (!(intr & BIT_ULL(i)))
			continue;

		event.intr_mask = (dir == MCS_RX) ?
				  MCS_PAB_RX_CHAN_OVERFLOW_INT :
				  MCS_PAB_TX_CHAN_OVERFLOW_INT;

		 
		event.lmac_id = i;
		mcs_add_intr_wq_entry(mcs, &event);
	}
}
