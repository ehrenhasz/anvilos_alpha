 
 

#ifndef HW_ATL2_LLH_H
#define HW_ATL2_LLH_H

#include <linux/types.h>

struct aq_hw_s;

 
void hw_atl2_reg_tx_intr_moder_ctrl_set(struct aq_hw_s *aq_hw,
					u32 tx_intr_moderation_ctl,
					u32 queue);

 
void hw_atl2_rpf_redirection_table2_select_set(struct aq_hw_s *aq_hw,
					       u32 select);

 
void hw_atl2_rpf_rss_hash_type_set(struct aq_hw_s *aq_hw, u32 rss_hash_type);

 
void hw_atl2_rpf_new_enable_set(struct aq_hw_s *aq_hw, u32 enable);

 
void hw_atl2_rpfl2_uc_flr_tag_set(struct aq_hw_s *aq_hw, u32 tag, u32 filter);

 
void hw_atl2_rpfl2_bc_flr_tag_set(struct aq_hw_s *aq_hw, u32 tag);

 
void hw_atl2_new_rpf_rss_redir_set(struct aq_hw_s *aq_hw, u32 tc, u32 index,
				   u32 queue);

 
void hw_atl2_rpf_vlan_flr_tag_set(struct aq_hw_s *aq_hw, u32 tag, u32 filter);

 
void hw_atl2_tpb_tx_tc_q_rand_map_en_set(struct aq_hw_s *aq_hw,
					 const u32 tc_q_rand_map_en);

 
void hw_atl2_tpb_tx_buf_clk_gate_en_set(struct aq_hw_s *aq_hw, u32 clk_gate_en);

void hw_atl2_tps_tx_pkt_shed_data_arb_mode_set(struct aq_hw_s *aq_hw,
					       const u32 data_arb_mode);

 
void hw_atl2_tps_tx_pkt_shed_tc_data_max_credit_set(struct aq_hw_s *aq_hw,
						    const u32 tc,
						    const u32 max_credit);

 
void hw_atl2_tps_tx_pkt_shed_tc_data_weight_set(struct aq_hw_s *aq_hw,
						const u32 tc,
						const u32 weight);

u32 hw_atl2_get_hw_version(struct aq_hw_s *aq_hw);

void hw_atl2_init_launchtime(struct aq_hw_s *aq_hw);

 
void hw_atl2_rpf_act_rslvr_record_set(struct aq_hw_s *aq_hw, u8 location,
				      u32 tag, u32 mask, u32 action);

 
void hw_atl2_rpf_act_rslvr_section_en_set(struct aq_hw_s *aq_hw, u32 sections);

 
void hw_atl2_mif_shared_buf_get(struct aq_hw_s *aq_hw, int offset, u32 *data,
				int len);

 
void hw_atl2_mif_shared_buf_write(struct aq_hw_s *aq_hw, int offset, u32 *data,
				  int len);

 
void hw_atl2_mif_shared_buf_read(struct aq_hw_s *aq_hw, int offset, u32 *data,
				 int len);

 
void hw_atl2_mif_host_finished_write_set(struct aq_hw_s *aq_hw, u32 finish);

 
u32 hw_atl2_mif_mcp_finished_read_get(struct aq_hw_s *aq_hw);

 
u32 hw_atl2_mif_mcp_boot_reg_get(struct aq_hw_s *aq_hw);

 
void hw_atl2_mif_mcp_boot_reg_set(struct aq_hw_s *aq_hw, u32 val);

 
u32 hw_atl2_mif_host_req_int_get(struct aq_hw_s *aq_hw);

 
void hw_atl2_mif_host_req_int_clr(struct aq_hw_s *aq_hw, u32 val);

#endif  
