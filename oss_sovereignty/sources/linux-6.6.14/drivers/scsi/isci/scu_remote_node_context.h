

#ifndef __SCU_REMOTE_NODE_CONTEXT_HEADER__
#define __SCU_REMOTE_NODE_CONTEXT_HEADER__




struct ssp_remote_node_context {
	

	
	u32 remote_node_index:12;
	u32 reserved0_1:4;

	
	u32 remote_node_port_width:4;

	
	u32 logical_port_index:3;
	u32 reserved0_2:5;

	
	u32 nexus_loss_timer_enable:1;

	
	u32 check_bit:1;

	
	u32 is_valid:1;

	
	u32 is_remote_node_context:1;

	

	
	u32 remote_sas_address_lo;

	
	u32 remote_sas_address_hi;

	
	
	u32 function_number:8;
	u32 reserved3_1:8;

	
	u32 arbitration_wait_time:16;

	
	
	u32 connection_occupancy_timeout:16;

	
	u32 connection_inactivity_timeout:16;

	
	
	u32 initial_arbitration_wait_time:16;

	
	u32 oaf_connection_rate:4;

	
	u32 oaf_features:4;

	
	u32 oaf_source_zone_group:8;

	
	
	u32 oaf_more_compatibility_features;

	
	u32 reserved7;

};


struct stp_remote_node_context {
	
	u32 data[8];

};


union scu_remote_node_context {
	
	struct ssp_remote_node_context ssp;

	
	struct stp_remote_node_context stp;

};

#endif 
