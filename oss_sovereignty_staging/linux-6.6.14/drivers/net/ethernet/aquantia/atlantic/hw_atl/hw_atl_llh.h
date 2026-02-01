 
 

 

#ifndef HW_ATL_LLH_H
#define HW_ATL_LLH_H

#include <linux/types.h>

struct aq_hw_s;

 
void hw_atl_ts_reset_set(struct aq_hw_s *aq_hw, u32 val);

 
void hw_atl_ts_power_down_set(struct aq_hw_s *aq_hw, u32 val);

 
u32 hw_atl_ts_power_down_get(struct aq_hw_s *aq_hw);

 
u32 hw_atl_ts_ready_get(struct aq_hw_s *aq_hw);

 
u32 hw_atl_ts_ready_latch_high_get(struct aq_hw_s *aq_hw);

 
u32 hw_atl_ts_data_get(struct aq_hw_s *aq_hw);

 

 
void hw_atl_reg_glb_cpu_sem_set(struct aq_hw_s *aq_hw,	u32 glb_cpu_sem,
				u32 semaphore);

 
u32 hw_atl_reg_glb_cpu_sem_get(struct aq_hw_s *aq_hw, u32 semaphore);

 
void hw_atl_glb_glb_reg_res_dis_set(struct aq_hw_s *aq_hw, u32 glb_reg_res_dis);

 
void hw_atl_glb_soft_res_set(struct aq_hw_s *aq_hw, u32 soft_res);

 
u32 hw_atl_glb_soft_res_get(struct aq_hw_s *aq_hw);

 

u32 hw_atl_rpb_rx_dma_drop_pkt_cnt_get(struct aq_hw_s *aq_hw);

 
u64 hw_atl_stats_rx_dma_good_octet_counter_get(struct aq_hw_s *aq_hw);

 
u64 hw_atl_stats_rx_dma_good_pkt_counter_get(struct aq_hw_s *aq_hw);

 
u64 hw_atl_stats_tx_dma_good_octet_counter_get(struct aq_hw_s *aq_hw);

 
u64 hw_atl_stats_tx_dma_good_pkt_counter_get(struct aq_hw_s *aq_hw);

 
u32 hw_atl_reg_mac_msm_rx_errs_cnt_get(struct aq_hw_s *aq_hw);

 
u32 hw_atl_reg_mac_msm_rx_ucst_frm_cnt_get(struct aq_hw_s *aq_hw);

 
u32 hw_atl_reg_mac_msm_rx_mcst_frm_cnt_get(struct aq_hw_s *aq_hw);

 
u32 hw_atl_reg_mac_msm_rx_bcst_frm_cnt_get(struct aq_hw_s *aq_hw);

 
u32 hw_atl_reg_mac_msm_rx_bcst_octets_counter1get(struct aq_hw_s *aq_hw);

 
u32 hw_atl_reg_mac_msm_rx_ucst_octets_counter0get(struct aq_hw_s *aq_hw);

 
u32 hw_atl_reg_mac_msm_tx_errs_cnt_get(struct aq_hw_s *aq_hw);

 
u32 hw_atl_reg_mac_msm_tx_ucst_frm_cnt_get(struct aq_hw_s *aq_hw);

 
u32 hw_atl_reg_mac_msm_tx_mcst_frm_cnt_get(struct aq_hw_s *aq_hw);

 
u32 hw_atl_reg_mac_msm_tx_bcst_frm_cnt_get(struct aq_hw_s *aq_hw);

 
u32 hw_atl_reg_mac_msm_tx_mcst_octets_counter1get(struct aq_hw_s *aq_hw);

 
u32 hw_atl_reg_mac_msm_tx_bcst_octets_counter1get(struct aq_hw_s *aq_hw);

 
u32 hw_atl_reg_mac_msm_tx_ucst_octets_counter0get(struct aq_hw_s *aq_hw);

 
u32 hw_atl_reg_glb_mif_id_get(struct aq_hw_s *aq_hw);

 

 
void hw_atl_itr_irq_auto_masklsw_set(struct aq_hw_s *aq_hw,
				     u32 irq_auto_masklsw);

 
void hw_atl_itr_irq_map_en_rx_set(struct aq_hw_s *aq_hw, u32 irq_map_en_rx,
				  u32 rx);

 
void hw_atl_itr_irq_map_en_tx_set(struct aq_hw_s *aq_hw, u32 irq_map_en_tx,
				  u32 tx);

 
void hw_atl_itr_irq_map_rx_set(struct aq_hw_s *aq_hw, u32 irq_map_rx, u32 rx);

 
void hw_atl_itr_irq_map_tx_set(struct aq_hw_s *aq_hw, u32 irq_map_tx, u32 tx);

 
void hw_atl_itr_irq_msk_clearlsw_set(struct aq_hw_s *aq_hw,
				     u32 irq_msk_clearlsw);

 
void hw_atl_itr_irq_msk_setlsw_set(struct aq_hw_s *aq_hw, u32 irq_msk_setlsw);

 
void hw_atl_itr_irq_reg_res_dis_set(struct aq_hw_s *aq_hw, u32 irq_reg_res_dis);

 
void hw_atl_itr_irq_status_clearlsw_set(struct aq_hw_s *aq_hw,
					u32 irq_status_clearlsw);

 
u32 hw_atl_itr_irq_statuslsw_get(struct aq_hw_s *aq_hw);

 
u32 hw_atl_itr_res_irq_get(struct aq_hw_s *aq_hw);

 
void hw_atl_itr_res_irq_set(struct aq_hw_s *aq_hw, u32 res_irq);

 
void hw_atl_itr_rsc_en_set(struct aq_hw_s *aq_hw, u32 enable);

 
void hw_atl_itr_rsc_delay_set(struct aq_hw_s *aq_hw, u32 delay);

 

 
void hw_atl_rdm_cpu_id_set(struct aq_hw_s *aq_hw, u32 cpuid, u32 dca);

 
void hw_atl_rdm_rx_dca_en_set(struct aq_hw_s *aq_hw, u32 rx_dca_en);

 
void hw_atl_rdm_rx_dca_mode_set(struct aq_hw_s *aq_hw, u32 rx_dca_mode);

 
void hw_atl_rdm_rx_desc_data_buff_size_set(struct aq_hw_s *aq_hw,
					   u32 rx_desc_data_buff_size,
				    u32 descriptor);

 
void hw_atl_rdm_rx_desc_dca_en_set(struct aq_hw_s *aq_hw, u32 rx_desc_dca_en,
				   u32 dca);

 
void hw_atl_rdm_rx_desc_en_set(struct aq_hw_s *aq_hw, u32 rx_desc_en,
			       u32 descriptor);

 
void hw_atl_rdm_rx_desc_head_splitting_set(struct aq_hw_s *aq_hw,
					   u32 rx_desc_head_splitting,
				    u32 descriptor);

 
u32 hw_atl_rdm_rx_desc_head_ptr_get(struct aq_hw_s *aq_hw, u32 descriptor);

 
void hw_atl_rdm_rx_desc_len_set(struct aq_hw_s *aq_hw, u32 rx_desc_len,
				u32 descriptor);

 
void hw_atl_rdm_rx_desc_wr_wb_irq_en_set(struct aq_hw_s *aq_hw,
					 u32 rx_desc_wr_wb_irq_en);

 
void hw_atl_rdm_rx_head_dca_en_set(struct aq_hw_s *aq_hw, u32 rx_head_dca_en,
				   u32 dca);

 
void hw_atl_rdm_rx_pld_dca_en_set(struct aq_hw_s *aq_hw, u32 rx_pld_dca_en,
				  u32 dca);

 
void hw_atl_rdm_rx_desc_head_buff_size_set(struct aq_hw_s *aq_hw,
					   u32 rx_desc_head_buff_size,
					   u32 descriptor);

 
void hw_atl_rdm_rx_desc_res_set(struct aq_hw_s *aq_hw, u32 rx_desc_res,
				u32 descriptor);

 
void hw_atl_rdm_rdm_intr_moder_en_set(struct aq_hw_s *aq_hw,
				      u32 rdm_intr_moder_en);

 

 
void hw_atl_reg_gen_irq_map_set(struct aq_hw_s *aq_hw, u32 gen_intr_map,
				u32 regidx);

 
u32 hw_atl_reg_gen_irq_status_get(struct aq_hw_s *aq_hw);

 
void hw_atl_reg_irq_glb_ctl_set(struct aq_hw_s *aq_hw, u32 intr_glb_ctl);

 
void hw_atl_reg_irq_thr_set(struct aq_hw_s *aq_hw, u32 intr_thr, u32 throttle);

 
void hw_atl_reg_rx_dma_desc_base_addresslswset(struct aq_hw_s *aq_hw,
					       u32 rx_dma_desc_base_addrlsw,
					u32 descriptor);

 
void hw_atl_reg_rx_dma_desc_base_addressmswset(struct aq_hw_s *aq_hw,
					       u32 rx_dma_desc_base_addrmsw,
					u32 descriptor);

 
u32 hw_atl_reg_rx_dma_desc_status_get(struct aq_hw_s *aq_hw, u32 descriptor);

 
void hw_atl_reg_rx_dma_desc_tail_ptr_set(struct aq_hw_s *aq_hw,
					 u32 rx_dma_desc_tail_ptr,
				  u32 descriptor);

 
void hw_atl_reg_rx_flr_mcst_flr_msk_set(struct aq_hw_s *aq_hw,
					u32 rx_flr_mcst_flr_msk);

 
void hw_atl_reg_rx_flr_mcst_flr_set(struct aq_hw_s *aq_hw, u32 rx_flr_mcst_flr,
				    u32 filter);

 
void hw_atl_reg_rx_flr_rss_control1set(struct aq_hw_s *aq_hw,
				       u32 rx_flr_rss_control1);

 
void hw_atl_reg_rx_flr_control2_set(struct aq_hw_s *aq_hw, u32 rx_flr_control2);

 
void hw_atl_reg_rx_intr_moder_ctrl_set(struct aq_hw_s *aq_hw,
				       u32 rx_intr_moderation_ctl,
				u32 queue);

 
void hw_atl_reg_tx_dma_debug_ctl_set(struct aq_hw_s *aq_hw,
				     u32 tx_dma_debug_ctl);

 
void hw_atl_reg_tx_dma_desc_base_addresslswset(struct aq_hw_s *aq_hw,
					       u32 tx_dma_desc_base_addrlsw,
					u32 descriptor);

 
void hw_atl_reg_tx_dma_desc_base_addressmswset(struct aq_hw_s *aq_hw,
					       u32 tx_dma_desc_base_addrmsw,
					u32 descriptor);

 
void hw_atl_reg_tx_dma_desc_tail_ptr_set(struct aq_hw_s *aq_hw,
					 u32 tx_dma_desc_tail_ptr,
					 u32 descriptor);

 
void hw_atl_reg_tx_intr_moder_ctrl_set(struct aq_hw_s *aq_hw,
				       u32 tx_intr_moderation_ctl,
				       u32 queue);

 
void hw_atl_reg_glb_cpu_scratch_scp_set(struct aq_hw_s *aq_hw,
					u32 glb_cpu_scratch_scp,
					u32 scratch_scp);

 

 
void hw_atl_rpb_dma_sys_lbk_set(struct aq_hw_s *aq_hw, u32 dma_sys_lbk);

 
void hw_atl_rpb_dma_net_lbk_set(struct aq_hw_s *aq_hw, u32 dma_net_lbk);

 
void hw_atl_rpb_rpf_rx_traf_class_mode_set(struct aq_hw_s *aq_hw,
					   u32 rx_traf_class_mode);

 
u32 hw_atl_rpb_rpf_rx_traf_class_mode_get(struct aq_hw_s *aq_hw);

 
void hw_atl_rpb_rx_buff_en_set(struct aq_hw_s *aq_hw, u32 rx_buff_en);

 
void hw_atl_rpb_rx_buff_hi_threshold_per_tc_set(struct aq_hw_s *aq_hw,
						u32 rx_buff_hi_threshold_per_tc,
						u32 buffer);

 
void hw_atl_rpb_rx_buff_lo_threshold_per_tc_set(struct aq_hw_s *aq_hw,
						u32 rx_buff_lo_threshold_per_tc,
					 u32 buffer);

 
void hw_atl_rpb_rx_flow_ctl_mode_set(struct aq_hw_s *aq_hw,
				     u32 rx_flow_ctl_mode);

 
void hw_atl_rpb_rx_pkt_buff_size_per_tc_set(struct aq_hw_s *aq_hw,
					    u32 rx_pkt_buff_size_per_tc,
					    u32 buffer);

 
void hw_atl_rdm_rx_dma_desc_cache_init_tgl(struct aq_hw_s *aq_hw);

 
u32 hw_atl_rdm_rx_dma_desc_cache_init_done_get(struct aq_hw_s *aq_hw);

 
void hw_atl_rpb_rx_xoff_en_per_tc_set(struct aq_hw_s *aq_hw,
				      u32 rx_xoff_en_per_tc,
				      u32 buffer);

 

 
void hw_atl_rpfl2broadcast_count_threshold_set(struct aq_hw_s *aq_hw,
					       u32 l2broadcast_count_threshold);

 
void hw_atl_rpfl2broadcast_en_set(struct aq_hw_s *aq_hw, u32 l2broadcast_en);

 
void hw_atl_rpfl2broadcast_flr_act_set(struct aq_hw_s *aq_hw,
				       u32 l2broadcast_flr_act);

 
void hw_atl_rpfl2multicast_flr_en_set(struct aq_hw_s *aq_hw,
				      u32 l2multicast_flr_en,
				      u32 filter);

 
u32 hw_atl_rpfl2promiscuous_mode_en_get(struct aq_hw_s *aq_hw);

 
void hw_atl_rpfl2promiscuous_mode_en_set(struct aq_hw_s *aq_hw,
					 u32 l2promiscuous_mode_en);

 
void hw_atl_rpfl2unicast_flr_act_set(struct aq_hw_s *aq_hw,
				     u32 l2unicast_flr_act,
				     u32 filter);

 
void hw_atl_rpfl2_uc_flr_en_set(struct aq_hw_s *aq_hw, u32 l2unicast_flr_en,
				u32 filter);

 
void hw_atl_rpfl2unicast_dest_addresslsw_set(struct aq_hw_s *aq_hw,
					     u32 l2unicast_dest_addresslsw,
				      u32 filter);

 
void hw_atl_rpfl2unicast_dest_addressmsw_set(struct aq_hw_s *aq_hw,
					     u32 l2unicast_dest_addressmsw,
				      u32 filter);

 
void hw_atl_rpfl2_accept_all_mc_packets_set(struct aq_hw_s *aq_hw,
					    u32 l2_accept_all_mc_packets);

 
void hw_atl_rpf_rpb_user_priority_tc_map_set(struct aq_hw_s *aq_hw,
					     u32 user_priority_tc_map, u32 tc);

 
void hw_atl_rpf_rss_key_addr_set(struct aq_hw_s *aq_hw, u32 rss_key_addr);

 
void hw_atl_rpf_rss_key_wr_data_set(struct aq_hw_s *aq_hw, u32 rss_key_wr_data);

 
u32 hw_atl_rpf_rss_key_wr_en_get(struct aq_hw_s *aq_hw);

 
void hw_atl_rpf_rss_key_wr_en_set(struct aq_hw_s *aq_hw, u32 rss_key_wr_en);

 
void hw_atl_rpf_rss_redir_tbl_addr_set(struct aq_hw_s *aq_hw,
				       u32 rss_redir_tbl_addr);

 
void hw_atl_rpf_rss_redir_tbl_wr_data_set(struct aq_hw_s *aq_hw,
					  u32 rss_redir_tbl_wr_data);

 
u32 hw_atl_rpf_rss_redir_wr_en_get(struct aq_hw_s *aq_hw);

 
void hw_atl_rpf_rss_redir_wr_en_set(struct aq_hw_s *aq_hw, u32 rss_redir_wr_en);

 
void hw_atl_rpf_tpo_to_rpf_sys_lbk_set(struct aq_hw_s *aq_hw,
				       u32 tpo_to_rpf_sys_lbk);

 
void hw_atl_rpf_vlan_inner_etht_set(struct aq_hw_s *aq_hw, u32 vlan_inner_etht);

 
void hw_atl_rpf_vlan_outer_etht_set(struct aq_hw_s *aq_hw, u32 vlan_outer_etht);

 
void hw_atl_rpf_vlan_prom_mode_en_set(struct aq_hw_s *aq_hw,
				      u32 vlan_prom_mode_en);

 
u32 hw_atl_rpf_vlan_prom_mode_en_get(struct aq_hw_s *aq_hw);

 
void hw_atl_rpf_vlan_untagged_act_set(struct aq_hw_s *aq_hw,
				      u32 vlan_untagged_act);

 
void hw_atl_rpf_vlan_accept_untagged_packets_set(struct aq_hw_s *aq_hw,
						 u32 vlan_acc_untagged_packets);

 
void hw_atl_rpf_vlan_flr_en_set(struct aq_hw_s *aq_hw, u32 vlan_flr_en,
				u32 filter);

 
void hw_atl_rpf_vlan_flr_act_set(struct aq_hw_s *aq_hw, u32 vlan_filter_act,
				 u32 filter);

 
void hw_atl_rpf_vlan_id_flr_set(struct aq_hw_s *aq_hw, u32 vlan_id_flr,
				u32 filter);

 
void hw_atl_rpf_vlan_rxq_en_flr_set(struct aq_hw_s *aq_hw, u32 vlan_rxq_en,
				    u32 filter);

 
void hw_atl_rpf_vlan_rxq_flr_set(struct aq_hw_s *aq_hw, u32 vlan_rxq,
				 u32 filter);

 
void hw_atl_rpf_etht_flr_en_set(struct aq_hw_s *aq_hw, u32 etht_flr_en,
				u32 filter);

 
void hw_atl_rpf_etht_user_priority_en_set(struct aq_hw_s *aq_hw,
					  u32 etht_user_priority_en,
					  u32 filter);

 
void hw_atl_rpf_etht_rx_queue_en_set(struct aq_hw_s *aq_hw,
				     u32 etht_rx_queue_en,
				     u32 filter);

 
void hw_atl_rpf_etht_rx_queue_set(struct aq_hw_s *aq_hw, u32 etht_rx_queue,
				  u32 filter);

 
void hw_atl_rpf_etht_user_priority_set(struct aq_hw_s *aq_hw,
				       u32 etht_user_priority,
				       u32 filter);

 
void hw_atl_rpf_etht_mgt_queue_set(struct aq_hw_s *aq_hw, u32 etht_mgt_queue,
				   u32 filter);

 
void hw_atl_rpf_etht_flr_act_set(struct aq_hw_s *aq_hw, u32 etht_flr_act,
				 u32 filter);

 
void hw_atl_rpf_etht_flr_set(struct aq_hw_s *aq_hw, u32 etht_flr, u32 filter);

 
void hw_atl_rpf_l4_spd_set(struct aq_hw_s *aq_hw, u32 val, u32 filter);

 
void hw_atl_rpf_l4_dpd_set(struct aq_hw_s *aq_hw, u32 val, u32 filter);

 

 
void hw_atl_rpo_ipv4header_crc_offload_en_set(struct aq_hw_s *aq_hw,
					      u32 ipv4header_crc_offload_en);

 
void hw_atl_rpo_rx_desc_vlan_stripping_set(struct aq_hw_s *aq_hw,
					   u32 rx_desc_vlan_stripping,
					   u32 descriptor);

void hw_atl_rpo_outer_vlan_tag_mode_set(void *context,
					u32 outervlantagmode);

u32 hw_atl_rpo_outer_vlan_tag_mode_get(void *context);

 
void hw_atl_rpo_tcp_udp_crc_offload_en_set(struct aq_hw_s *aq_hw,
					   u32 tcp_udp_crc_offload_en);

 
void hw_atl_rpo_lro_patch_optimization_en_set(struct aq_hw_s *aq_hw,
					      u32 lro_patch_optimization_en);

 
void hw_atl_rpo_lro_en_set(struct aq_hw_s *aq_hw, u32 lro_en);

 
void hw_atl_rpo_lro_qsessions_lim_set(struct aq_hw_s *aq_hw,
				      u32 lro_qsessions_lim);

 
void hw_atl_rpo_lro_total_desc_lim_set(struct aq_hw_s *aq_hw,
				       u32 lro_total_desc_lim);

 
void hw_atl_rpo_lro_min_pay_of_first_pkt_set(struct aq_hw_s *aq_hw,
					     u32 lro_min_pld_of_first_pkt);

 
void hw_atl_rpo_lro_pkt_lim_set(struct aq_hw_s *aq_hw, u32 lro_packet_lim);

 
void hw_atl_rpo_lro_max_num_of_descriptors_set(struct aq_hw_s *aq_hw,
					       u32 lro_max_desc_num, u32 lro);

 
void hw_atl_rpo_lro_time_base_divider_set(struct aq_hw_s *aq_hw,
					  u32 lro_time_base_divider);

 
void hw_atl_rpo_lro_inactive_interval_set(struct aq_hw_s *aq_hw,
					  u32 lro_inactive_interval);

 
void hw_atl_rpo_lro_max_coalescing_interval_set(struct aq_hw_s *aq_hw,
						u32 lro_max_coal_interval);

 

 
void hw_atl_rx_rx_reg_res_dis_set(struct aq_hw_s *aq_hw, u32 rx_reg_res_dis);

 

 
void hw_atl_tdm_cpu_id_set(struct aq_hw_s *aq_hw, u32 cpuid, u32 dca);

 
void hw_atl_tdm_large_send_offload_en_set(struct aq_hw_s *aq_hw,
					  u32 large_send_offload_en);

 
void hw_atl_tdm_tx_desc_en_set(struct aq_hw_s *aq_hw, u32 tx_desc_en,
			       u32 descriptor);

 
void hw_atl_tdm_tx_dca_en_set(struct aq_hw_s *aq_hw, u32 tx_dca_en);

 
void hw_atl_tdm_tx_dca_mode_set(struct aq_hw_s *aq_hw, u32 tx_dca_mode);

 
void hw_atl_tdm_tx_desc_dca_en_set(struct aq_hw_s *aq_hw, u32 tx_desc_dca_en,
				   u32 dca);

 
u32 hw_atl_tdm_tx_desc_head_ptr_get(struct aq_hw_s *aq_hw, u32 descriptor);

 
void hw_atl_tdm_tx_desc_len_set(struct aq_hw_s *aq_hw, u32 tx_desc_len,
				u32 descriptor);

 
void hw_atl_tdm_tx_desc_wr_wb_irq_en_set(struct aq_hw_s *aq_hw,
					 u32 tx_desc_wr_wb_irq_en);

 
void hw_atl_tdm_tx_desc_wr_wb_threshold_set(struct aq_hw_s *aq_hw,
					    u32 tx_desc_wr_wb_threshold,
				     u32 descriptor);

 
void hw_atl_tdm_tdm_intr_moder_en_set(struct aq_hw_s *aq_hw,
				      u32 tdm_irq_moderation_en);
 

 
void hw_atl_thm_lso_tcp_flag_of_first_pkt_set(struct aq_hw_s *aq_hw,
					      u32 lso_tcp_flag_of_first_pkt);

 
void hw_atl_thm_lso_tcp_flag_of_last_pkt_set(struct aq_hw_s *aq_hw,
					     u32 lso_tcp_flag_of_last_pkt);

 
void hw_atl_thm_lso_tcp_flag_of_middle_pkt_set(struct aq_hw_s *aq_hw,
					       u32 lso_tcp_flag_of_middle_pkt);

 

 
void hw_atl_tpb_tps_tx_tc_mode_set(struct aq_hw_s *aq_hw,
				   u32 tx_traf_class_mode);

 
u32 hw_atl_tpb_tps_tx_tc_mode_get(struct aq_hw_s *aq_hw);

 
void hw_atl_tpb_tx_buff_en_set(struct aq_hw_s *aq_hw, u32 tx_buff_en);

 
void hw_atl_tpb_tx_buff_hi_threshold_per_tc_set(struct aq_hw_s *aq_hw,
						u32 tx_buff_hi_threshold_per_tc,
					 u32 buffer);

 
void hw_atl_tpb_tx_buff_lo_threshold_per_tc_set(struct aq_hw_s *aq_hw,
						u32 tx_buff_lo_threshold_per_tc,
					 u32 buffer);

 
void hw_atl_tpb_tx_dma_sys_lbk_en_set(struct aq_hw_s *aq_hw, u32 tx_dma_sys_lbk_en);

 
void hw_atl_tpb_tx_dma_net_lbk_en_set(struct aq_hw_s *aq_hw,
				      u32 tx_dma_net_lbk_en);

 
void hw_atl_tpb_tx_tx_clk_gate_en_set(struct aq_hw_s *aq_hw,
				      u32 tx_clk_gate_en);

 
void hw_atl_tpb_tx_pkt_buff_size_per_tc_set(struct aq_hw_s *aq_hw,
					    u32 tx_pkt_buff_size_per_tc,
					    u32 buffer);

 
void hw_atl_tpb_tx_path_scp_ins_en_set(struct aq_hw_s *aq_hw, u32 tx_path_scp_ins_en);

 

 
void hw_atl_tpo_ipv4header_crc_offload_en_set(struct aq_hw_s *aq_hw,
					      u32 ipv4header_crc_offload_en);

 
void hw_atl_tpo_tcp_udp_crc_offload_en_set(struct aq_hw_s *aq_hw,
					   u32 tcp_udp_crc_offload_en);

 
void hw_atl_tpo_tx_pkt_sys_lbk_en_set(struct aq_hw_s *aq_hw,
				      u32 tx_pkt_sys_lbk_en);

 

 
void hw_atl_tps_tx_pkt_shed_data_arb_mode_set(struct aq_hw_s *aq_hw,
					      u32 tx_pkt_shed_data_arb_mode);

 
void hw_atl_tps_tx_pkt_shed_desc_rate_curr_time_res_set(struct aq_hw_s *aq_hw,
							u32 curr_time_res);

 
void hw_atl_tps_tx_pkt_shed_desc_rate_lim_set(struct aq_hw_s *aq_hw,
					      u32 tx_pkt_shed_desc_rate_lim);

 
void hw_atl_tps_tx_pkt_shed_desc_tc_arb_mode_set(struct aq_hw_s *aq_hw,
						 u32 arb_mode);

 
void hw_atl_tps_tx_pkt_shed_desc_tc_max_credit_set(struct aq_hw_s *aq_hw,
						   const u32 tc,
						   const u32 max_credit);

 
void hw_atl_tps_tx_pkt_shed_desc_tc_weight_set(struct aq_hw_s *aq_hw,
					       const u32 tc,
					       const u32 weight);

 
void hw_atl_tps_tx_pkt_shed_desc_vm_arb_mode_set(struct aq_hw_s *aq_hw,
						 u32 arb_mode);

 
void hw_atl_tps_tx_pkt_shed_tc_data_max_credit_set(struct aq_hw_s *aq_hw,
						   const u32 tc,
						   const u32 max_credit);

 
void hw_atl_tps_tx_pkt_shed_tc_data_weight_set(struct aq_hw_s *aq_hw,
					       const u32 tc,
					       const u32 weight);

 
void hw_atl_tps_tx_desc_rate_mode_set(struct aq_hw_s *aq_hw,
				      const u32 rate_mode);

 
void hw_atl_tps_tx_desc_rate_en_set(struct aq_hw_s *aq_hw, const u32 desc,
				    const u32 enable);

 
void hw_atl_tps_tx_desc_rate_x_set(struct aq_hw_s *aq_hw, const u32 desc,
				   const u32 rate_int);

 
void hw_atl_tps_tx_desc_rate_y_set(struct aq_hw_s *aq_hw, const u32 desc,
				   const u32 rate_frac);

 

 
void hw_atl_tx_tx_reg_res_dis_set(struct aq_hw_s *aq_hw, u32 tx_reg_res_dis);

 

 
u32 hw_atl_msm_reg_access_status_get(struct aq_hw_s *aq_hw);

 
void hw_atl_msm_reg_addr_for_indirect_addr_set(struct aq_hw_s *aq_hw,
					       u32 reg_addr_for_indirect_addr);

 
void hw_atl_msm_reg_rd_strobe_set(struct aq_hw_s *aq_hw, u32 reg_rd_strobe);

 
u32 hw_atl_msm_reg_rd_data_get(struct aq_hw_s *aq_hw);

 
void hw_atl_msm_reg_wr_data_set(struct aq_hw_s *aq_hw, u32 reg_wr_data);

 
void hw_atl_msm_reg_wr_strobe_set(struct aq_hw_s *aq_hw, u32 reg_wr_strobe);

 

 
void hw_atl_pci_pci_reg_res_dis_set(struct aq_hw_s *aq_hw, u32 pci_reg_res_dis);

 
void hw_atl_pcs_ptp_clock_read_enable(struct aq_hw_s *aq_hw,
				      u32 ptp_clock_read_enable);

u32 hw_atl_pcs_ptp_clock_get(struct aq_hw_s *aq_hw, u32 index);

 
void hw_atl_mcp_up_force_intr_set(struct aq_hw_s *aq_hw, u32 up_force_intr);

 
void hw_atl_rpfl3l4_ipv4_dest_addr_clear(struct aq_hw_s *aq_hw, u8 location);

 
void hw_atl_rpfl3l4_ipv4_src_addr_clear(struct aq_hw_s *aq_hw, u8 location);

 
void hw_atl_rpfl3l4_cmd_clear(struct aq_hw_s *aq_hw, u8 location);

 
void hw_atl_rpfl3l4_ipv6_dest_addr_clear(struct aq_hw_s *aq_hw, u8 location);

 
void hw_atl_rpfl3l4_ipv6_src_addr_clear(struct aq_hw_s *aq_hw, u8 location);

 
void hw_atl_rpfl3l4_ipv4_dest_addr_set(struct aq_hw_s *aq_hw, u8 location,
				       u32 ipv4_dest);

 
void hw_atl_rpfl3l4_ipv4_src_addr_set(struct aq_hw_s *aq_hw, u8 location,
				      u32 ipv4_src);

 
void hw_atl_rpfl3l4_cmd_set(struct aq_hw_s *aq_hw, u8 location, u32 cmd);

 
void hw_atl_rpfl3l4_ipv6_src_addr_set(struct aq_hw_s *aq_hw, u8 location,
				      u32 *ipv6_src);

 
void hw_atl_rpfl3l4_ipv6_dest_addr_set(struct aq_hw_s *aq_hw, u8 location,
				       u32 *ipv6_dest);

 
void hw_atl_glb_mdio_iface1_set(struct aq_hw_s *hw, u32 value);

 
u32 hw_atl_glb_mdio_iface1_get(struct aq_hw_s *hw);

 
void hw_atl_glb_mdio_iface2_set(struct aq_hw_s *hw, u32 value);

 
u32 hw_atl_glb_mdio_iface2_get(struct aq_hw_s *hw);

 
void hw_atl_glb_mdio_iface3_set(struct aq_hw_s *hw, u32 value);

 
u32 hw_atl_glb_mdio_iface3_get(struct aq_hw_s *hw);

 
void hw_atl_glb_mdio_iface4_set(struct aq_hw_s *hw, u32 value);

 
u32 hw_atl_glb_mdio_iface4_get(struct aq_hw_s *hw);

 
void hw_atl_glb_mdio_iface5_set(struct aq_hw_s *hw, u32 value);

 
u32 hw_atl_glb_mdio_iface5_get(struct aq_hw_s *hw);

u32 hw_atl_mdio_busy_get(struct aq_hw_s *aq_hw);

 
u32 hw_atl_sem_ram_get(struct aq_hw_s *self);

 
u32 hw_atl_sem_mdio_get(struct aq_hw_s *self);

u32 hw_atl_sem_reset1_get(struct aq_hw_s *self);
u32 hw_atl_sem_reset2_get(struct aq_hw_s *self);

 
u32 hw_atl_scrpad_get(struct aq_hw_s *aq_hw, u32 scratch_scp);

 
u32 hw_atl_scrpad12_get(struct aq_hw_s *self);

 
u32 hw_atl_scrpad25_get(struct aq_hw_s *self);

#endif  
