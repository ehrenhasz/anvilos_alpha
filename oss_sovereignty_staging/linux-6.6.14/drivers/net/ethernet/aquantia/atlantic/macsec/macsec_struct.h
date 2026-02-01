 
 

#ifndef _MACSEC_STRUCT_H_
#define _MACSEC_STRUCT_H_

 
struct aq_mss_egress_ctlf_record {
	 
	u32 sa_da[2];
	 
	u32 eth_type;
	 
	u32 match_mask;
	 
	u32 match_type;
	 
	u32 action;
};

 
struct aq_mss_egress_class_record {
	 
	u32 vlan_id;
	 
	u32 vlan_up;
	 
	u32 vlan_valid;
	 
	u32 byte3;
	 
	u32 byte2;
	 
	u32 byte1;
	 
	u32 byte0;
	 
	u32 tci;
	 
	u32 sci[2];
	 
	u32 eth_type;
	 
	u32 snap[2];
	 
	u32 llc;
	 
	u32 mac_sa[2];
	 
	u32 mac_da[2];
	 
	u32 pn;
	 
	u32 byte3_location;
	 
	u32 byte3_mask;
	 
	u32 byte2_location;
	 
	u32 byte2_mask;
	 
	u32 byte1_location;
	 
	u32 byte1_mask;
	 
	u32 byte0_location;
	 
	u32 byte0_mask;
	 
	u32 vlan_id_mask;
	 
	u32 vlan_up_mask;
	 
	u32 vlan_valid_mask;
	 
	u32 tci_mask;
	 
	u32 sci_mask;
	 
	u32 eth_type_mask;
	 
	u32 snap_mask;
	 
	u32 llc_mask;
	 
	u32 sa_mask;
	 
	u32 da_mask;
	 
	u32 pn_mask;
	 
	u32 eight02dot2;
	 
	u32 tci_sc;
	 
	u32 tci_87543;
	 
	u32 exp_sectag_en;
	 
	u32 sc_idx;
	 
	u32 sc_sa;
	 
	u32 debug;
	 
	u32 action;
	 
	u32 valid;
};

 
struct aq_mss_egress_sc_record {
	 
	u32 start_time;
	 
	u32 stop_time;
	 
	u32 curr_an;
	 
	u32 an_roll;
	 
	u32 tci;
	 
	u32 enc_off;
	 
	u32 protect;
	 
	u32 recv;
	 
	u32 fresh;
	 
	u32 sak_len;
	 
	u32 valid;
};

 
struct aq_mss_egress_sa_record {
	 
	u32 start_time;
	 
	u32 stop_time;
	 
	u32 next_pn;
	 
	u32 sat_pn;
	 
	u32 fresh;
	 
	u32 valid;
};

 
struct aq_mss_egress_sakey_record {
	 
	u32 key[8];
};

 
struct aq_mss_ingress_prectlf_record {
	 
	u32 sa_da[2];
	 
	u32 eth_type;
	 
	u32 match_mask;
	 
	u32 match_type;
	 
	u32 action;
};

 
struct aq_mss_ingress_preclass_record {
	 
	u32 sci[2];
	 
	u32 tci;
	 
	u32 encr_offset;
	 
	u32 eth_type;
	 
	u32 snap[2];
	 
	u32 llc;
	 
	u32 mac_sa[2];
	 
	u32 mac_da[2];
	 
	u32 lpbk_packet;
	 
	u32 an_mask;
	 
	u32 tci_mask;
	 
	u32 sci_mask;
	 
	u32 eth_type_mask;
	 
	u32 snap_mask;
	 
	u32 llc_mask;
	 
	u32 _802_2_encapsulate;
	 
	u32 sa_mask;
	 
	u32 da_mask;
	 
	u32 lpbk_mask;
	 
	u32 sc_idx;
	 
	u32 proc_dest;
	 
	u32 action;
	 
	u32 ctrl_unctrl;
	 
	u32 sci_from_table;
	 
	u32 reserved;
	 
	u32 valid;
};

 
struct aq_mss_ingress_sc_record {
	 
	u32 stop_time;
	 
	u32 start_time;
	 
	u32 validate_frames;
	 
	u32 replay_protect;
	 
	u32 anti_replay_window;
	 
	u32 receiving;
	 
	u32 fresh;
	 
	u32 an_rol;
	 
	u32 reserved;
	 
	u32 valid;
};

 
struct aq_mss_ingress_sa_record {
	 
	u32 stop_time;
	 
	u32 start_time;
	 
	u32 next_pn;
	 
	u32 sat_nextpn;
	 
	u32 in_use;
	 
	u32 fresh;
	 
	u32 reserved;
	 
	u32 valid;
};

 
struct aq_mss_ingress_sakey_record {
	 
	u32 key[8];
	 
	u32 key_len;
};

 
struct aq_mss_ingress_postclass_record {
	 
	u32 byte0;
	 
	u32 byte1;
	 
	u32 byte2;
	 
	u32 byte3;
	 
	u32 eth_type;
	 
	u32 eth_type_valid;
	 
	u32 vlan_id;
	 
	u32 vlan_up;
	 
	u32 vlan_valid;
	 
	u32 sai;
	 
	u32 sai_hit;
	 
	u32 eth_type_mask;
	 
	u32 byte3_location;
	 
	u32 byte3_mask;
	 
	u32 byte2_location;
	 
	u32 byte2_mask;
	 
	u32 byte1_location;
	 
	u32 byte1_mask;
	 
	u32 byte0_location;
	 
	u32 byte0_mask;
	 
	u32 eth_type_valid_mask;
	 
	u32 vlan_id_mask;
	 
	u32 vlan_up_mask;
	 
	u32 vlan_valid_mask;
	 
	u32 sai_mask;
	 
	u32 sai_hit_mask;
	 
	u32 firstlevel_actions;
	 
	u32 secondlevel_actions;
	 
	u32 reserved;
	 
	u32 valid;
};

 
struct aq_mss_ingress_postctlf_record {
	 
	u32 sa_da[2];
	 
	u32 eth_type;
	 
	u32 match_mask;
	 
	u32 match_type;
	 
	u32 action;
};

 
struct aq_mss_egress_sc_counters {
	 
	u32 sc_protected_pkts[2];
	 
	u32 sc_encrypted_pkts[2];
	 
	u32 sc_protected_octets[2];
	 
	u32 sc_encrypted_octets[2];
};

 
struct aq_mss_egress_sa_counters {
	 
	u32 sa_hit_drop_redirect[2];
	 
	u32 sa_protected2_pkts[2];
	 
	u32 sa_protected_pkts[2];
	 
	u32 sa_encrypted_pkts[2];
};

 
struct aq_mss_egress_common_counters {
	 
	u32 ctl_pkt[2];
	 
	u32 unknown_sa_pkts[2];
	 
	u32 untagged_pkts[2];
	 
	u32 too_long[2];
	 
	u32 ecc_error_pkts[2];
	 
	u32 unctrl_hit_drop_redir[2];
};

 
struct aq_mss_ingress_sa_counters {
	 
	u32 untagged_hit_pkts[2];
	 
	u32 ctrl_hit_drop_redir_pkts[2];
	 
	u32 not_using_sa[2];
	 
	u32 unused_sa[2];
	 
	u32 not_valid_pkts[2];
	 
	u32 invalid_pkts[2];
	 
	u32 ok_pkts[2];
	 
	u32 late_pkts[2];
	 
	u32 delayed_pkts[2];
	 
	u32 unchecked_pkts[2];
	 
	u32 validated_octets[2];
	 
	u32 decrypted_octets[2];
};

 
struct aq_mss_ingress_common_counters {
	 
	u32 ctl_pkts[2];
	 
	u32 tagged_miss_pkts[2];
	 
	u32 untagged_miss_pkts[2];
	 
	u32 notag_pkts[2];
	 
	u32 untagged_pkts[2];
	 
	u32 bad_tag_pkts[2];
	 
	u32 no_sci_pkts[2];
	 
	u32 unknown_sci_pkts[2];
	 
	u32 ctrl_prt_pass_pkts[2];
	 
	u32 unctrl_prt_pass_pkts[2];
	 
	u32 ctrl_prt_fail_pkts[2];
	 
	u32 unctrl_prt_fail_pkts[2];
	 
	u32 too_long_pkts[2];
	 
	u32 igpoc_ctl_pkts[2];
	 
	u32 ecc_error_pkts[2];
	 
	u32 unctrl_hit_drop_redir[2];
};

#endif
