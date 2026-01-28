#ifndef __NCSI_PKT_H__
#define __NCSI_PKT_H__
struct ncsi_pkt_hdr {
	unsigned char mc_id;         
	unsigned char revision;      
	unsigned char reserved;      
	unsigned char id;            
	unsigned char type;          
	unsigned char channel;       
	__be16        length;        
	__be32        reserved1[2];  
};
struct ncsi_cmd_pkt_hdr {
	struct ncsi_pkt_hdr common;  
};
struct ncsi_rsp_pkt_hdr {
	struct ncsi_pkt_hdr common;  
	__be16              code;    
	__be16              reason;  
};
struct ncsi_aen_pkt_hdr {
	struct ncsi_pkt_hdr common;        
	unsigned char       reserved2[3];  
	unsigned char       type;          
};
struct ncsi_cmd_pkt {
	struct ncsi_cmd_pkt_hdr cmd;       
	__be32                  checksum;  
	unsigned char           pad[26];
};
struct ncsi_rsp_pkt {
	struct ncsi_rsp_pkt_hdr rsp;       
	__be32                  checksum;  
	unsigned char           pad[22];
};
struct ncsi_cmd_sp_pkt {
	struct ncsi_cmd_pkt_hdr cmd;             
	unsigned char           reserved[3];     
	unsigned char           hw_arbitration;  
	__be32                  checksum;        
	unsigned char           pad[22];
};
struct ncsi_cmd_dc_pkt {
	struct ncsi_cmd_pkt_hdr cmd;          
	unsigned char           reserved[3];  
	unsigned char           ald;          
	__be32                  checksum;     
	unsigned char           pad[22];
};
struct ncsi_cmd_rc_pkt {
	struct ncsi_cmd_pkt_hdr cmd;       
	__be32                  reserved;  
	__be32                  checksum;  
	unsigned char           pad[22];
};
struct ncsi_cmd_ae_pkt {
	struct ncsi_cmd_pkt_hdr cmd;          
	unsigned char           reserved[3];  
	unsigned char           mc_id;        
	__be32                  mode;         
	__be32                  checksum;     
	unsigned char           pad[18];
};
struct ncsi_cmd_sl_pkt {
	struct ncsi_cmd_pkt_hdr cmd;       
	__be32                  mode;      
	__be32                  oem_mode;  
	__be32                  checksum;  
	unsigned char           pad[18];
};
struct ncsi_cmd_svf_pkt {
	struct ncsi_cmd_pkt_hdr cmd;        
	__be16                  reserved;   
	__be16                  vlan;       
	__be16                  reserved1;  
	unsigned char           index;      
	unsigned char           enable;     
	__be32                  checksum;   
	unsigned char           pad[18];
};
struct ncsi_cmd_ev_pkt {
	struct ncsi_cmd_pkt_hdr cmd;          
	unsigned char           reserved[3];  
	unsigned char           mode;         
	__be32                  checksum;     
	unsigned char           pad[22];
};
struct ncsi_cmd_sma_pkt {
	struct ncsi_cmd_pkt_hdr cmd;       
	unsigned char           mac[6];    
	unsigned char           index;     
	unsigned char           at_e;      
	__be32                  checksum;  
	unsigned char           pad[18];
};
struct ncsi_cmd_ebf_pkt {
	struct ncsi_cmd_pkt_hdr cmd;       
	__be32                  mode;      
	__be32                  checksum;  
	unsigned char           pad[22];
};
struct ncsi_cmd_egmf_pkt {
	struct ncsi_cmd_pkt_hdr cmd;       
	__be32                  mode;      
	__be32                  checksum;  
	unsigned char           pad[22];
};
struct ncsi_cmd_snfc_pkt {
	struct ncsi_cmd_pkt_hdr cmd;          
	unsigned char           reserved[3];  
	unsigned char           mode;         
	__be32                  checksum;     
	unsigned char           pad[22];
};
struct ncsi_cmd_oem_pkt {
	struct ncsi_cmd_pkt_hdr cmd;          
	__be32                  mfr_id;       
	unsigned char           data[];       
};
struct ncsi_rsp_oem_pkt {
	struct ncsi_rsp_pkt_hdr rsp;          
	__be32                  mfr_id;       
	unsigned char           data[];       
};
struct ncsi_rsp_oem_mlx_pkt {
	unsigned char           cmd_rev;      
	unsigned char           cmd;          
	unsigned char           param;        
	unsigned char           optional;     
	unsigned char           data[];       
};
struct ncsi_rsp_oem_bcm_pkt {
	unsigned char           ver;          
	unsigned char           type;         
	__be16                  len;          
	unsigned char           data[];       
};
struct ncsi_rsp_oem_intel_pkt {
	unsigned char           cmd;          
	unsigned char           data[];       
};
struct ncsi_rsp_gls_pkt {
	struct ncsi_rsp_pkt_hdr rsp;         
	__be32                  status;      
	__be32                  other;       
	__be32                  oem_status;  
	__be32                  checksum;
	unsigned char           pad[10];
};
struct ncsi_rsp_gvi_pkt {
	struct ncsi_rsp_pkt_hdr rsp;           
	unsigned char           major;         
	unsigned char           minor;         
	unsigned char           update;        
	unsigned char           alpha1;        
	unsigned char           reserved[3];   
	unsigned char           alpha2;        
	unsigned char           fw_name[12];   
	__be32                  fw_version;    
	__be16                  pci_ids[4];    
	__be32                  mf_id;         
	__be32                  checksum;
};
struct ncsi_rsp_gc_pkt {
	struct ncsi_rsp_pkt_hdr rsp;          
	__be32                  cap;          
	__be32                  bc_cap;       
	__be32                  mc_cap;       
	__be32                  buf_cap;      
	__be32                  aen_cap;      
	unsigned char           vlan_cnt;     
	unsigned char           mixed_cnt;    
	unsigned char           mc_cnt;       
	unsigned char           uc_cnt;       
	unsigned char           reserved[2];  
	unsigned char           vlan_mode;    
	unsigned char           channel_cnt;  
	__be32                  checksum;     
};
struct ncsi_rsp_gp_pkt {
	struct ncsi_rsp_pkt_hdr rsp;           
	unsigned char           mac_cnt;       
	unsigned char           reserved[2];   
	unsigned char           mac_enable;    
	unsigned char           vlan_cnt;      
	unsigned char           reserved1;     
	__be16                  vlan_enable;   
	__be32                  link_mode;     
	__be32                  bc_mode;       
	__be32                  valid_modes;   
	unsigned char           vlan_mode;     
	unsigned char           fc_mode;       
	unsigned char           reserved2[2];  
	__be32                  aen_mode;      
	unsigned char           mac[6];        
	__be16                  vlan;          
	__be32                  checksum;      
};
struct ncsi_rsp_gcps_pkt {
	struct ncsi_rsp_pkt_hdr rsp;             
	__be32                  cnt_hi;          
	__be32                  cnt_lo;          
	__be32                  rx_bytes;        
	__be32                  tx_bytes;        
	__be32                  rx_uc_pkts;      
	__be32                  rx_mc_pkts;      
	__be32                  rx_bc_pkts;      
	__be32                  tx_uc_pkts;      
	__be32                  tx_mc_pkts;      
	__be32                  tx_bc_pkts;      
	__be32                  fcs_err;         
	__be32                  align_err;       
	__be32                  false_carrier;   
	__be32                  runt_pkts;       
	__be32                  jabber_pkts;     
	__be32                  rx_pause_xon;    
	__be32                  rx_pause_xoff;   
	__be32                  tx_pause_xon;    
	__be32                  tx_pause_xoff;   
	__be32                  tx_s_collision;  
	__be32                  tx_m_collision;  
	__be32                  l_collision;     
	__be32                  e_collision;     
	__be32                  rx_ctl_frames;   
	__be32                  rx_64_frames;    
	__be32                  rx_127_frames;   
	__be32                  rx_255_frames;   
	__be32                  rx_511_frames;   
	__be32                  rx_1023_frames;  
	__be32                  rx_1522_frames;  
	__be32                  rx_9022_frames;  
	__be32                  tx_64_frames;    
	__be32                  tx_127_frames;   
	__be32                  tx_255_frames;   
	__be32                  tx_511_frames;   
	__be32                  tx_1023_frames;  
	__be32                  tx_1522_frames;  
	__be32                  tx_9022_frames;  
	__be32                  rx_valid_bytes;  
	__be32                  rx_runt_pkts;    
	__be32                  rx_jabber_pkts;  
	__be32                  checksum;        
};
struct ncsi_rsp_gns_pkt {
	struct ncsi_rsp_pkt_hdr rsp;            
	__be32                  rx_cmds;        
	__be32                  dropped_cmds;   
	__be32                  cmd_type_errs;  
	__be32                  cmd_csum_errs;  
	__be32                  rx_pkts;        
	__be32                  tx_pkts;        
	__be32                  tx_aen_pkts;    
	__be32                  checksum;       
};
struct ncsi_rsp_gnpts_pkt {
	struct ncsi_rsp_pkt_hdr rsp;             
	__be32                  tx_pkts;         
	__be32                  tx_dropped;      
	__be32                  tx_channel_err;  
	__be32                  tx_us_err;       
	__be32                  rx_pkts;         
	__be32                  rx_dropped;      
	__be32                  rx_channel_err;  
	__be32                  rx_us_err;       
	__be32                  rx_os_err;       
	__be32                  checksum;        
};
struct ncsi_rsp_gps_pkt {
	struct ncsi_rsp_pkt_hdr rsp;       
	__be32                  status;    
	__be32                  checksum;
};
struct ncsi_rsp_gpuuid_pkt {
	struct ncsi_rsp_pkt_hdr rsp;       
	unsigned char           uuid[16];  
	__be32                  checksum;
};
struct ncsi_aen_lsc_pkt {
	struct ncsi_aen_pkt_hdr aen;         
	__be32                  status;      
	__be32                  oem_status;  
	__be32                  checksum;    
	unsigned char           pad[14];
};
struct ncsi_aen_cr_pkt {
	struct ncsi_aen_pkt_hdr aen;       
	__be32                  checksum;  
	unsigned char           pad[22];
};
struct ncsi_aen_hncdsc_pkt {
	struct ncsi_aen_pkt_hdr aen;       
	__be32                  status;    
	__be32                  checksum;  
	unsigned char           pad[18];
};
#define NCSI_PKT_REVISION	0x01
#define NCSI_PKT_CMD_CIS	0x00  
#define NCSI_PKT_CMD_SP		0x01  
#define NCSI_PKT_CMD_DP		0x02  
#define NCSI_PKT_CMD_EC		0x03  
#define NCSI_PKT_CMD_DC		0x04  
#define NCSI_PKT_CMD_RC		0x05  
#define NCSI_PKT_CMD_ECNT	0x06  
#define NCSI_PKT_CMD_DCNT	0x07  
#define NCSI_PKT_CMD_AE		0x08  
#define NCSI_PKT_CMD_SL		0x09  
#define NCSI_PKT_CMD_GLS	0x0a  
#define NCSI_PKT_CMD_SVF	0x0b  
#define NCSI_PKT_CMD_EV		0x0c  
#define NCSI_PKT_CMD_DV		0x0d  
#define NCSI_PKT_CMD_SMA	0x0e  
#define NCSI_PKT_CMD_EBF	0x10  
#define NCSI_PKT_CMD_DBF	0x11  
#define NCSI_PKT_CMD_EGMF	0x12  
#define NCSI_PKT_CMD_DGMF	0x13  
#define NCSI_PKT_CMD_SNFC	0x14  
#define NCSI_PKT_CMD_GVI	0x15  
#define NCSI_PKT_CMD_GC		0x16  
#define NCSI_PKT_CMD_GP		0x17  
#define NCSI_PKT_CMD_GCPS	0x18  
#define NCSI_PKT_CMD_GNS	0x19  
#define NCSI_PKT_CMD_GNPTS	0x1a  
#define NCSI_PKT_CMD_GPS	0x1b  
#define NCSI_PKT_CMD_OEM	0x50  
#define NCSI_PKT_CMD_PLDM	0x51  
#define NCSI_PKT_CMD_GPUUID	0x52  
#define NCSI_PKT_CMD_QPNPR	0x56  
#define NCSI_PKT_CMD_SNPR	0x57  
#define NCSI_PKT_RSP_CIS	(NCSI_PKT_CMD_CIS    + 0x80)
#define NCSI_PKT_RSP_SP		(NCSI_PKT_CMD_SP     + 0x80)
#define NCSI_PKT_RSP_DP		(NCSI_PKT_CMD_DP     + 0x80)
#define NCSI_PKT_RSP_EC		(NCSI_PKT_CMD_EC     + 0x80)
#define NCSI_PKT_RSP_DC		(NCSI_PKT_CMD_DC     + 0x80)
#define NCSI_PKT_RSP_RC		(NCSI_PKT_CMD_RC     + 0x80)
#define NCSI_PKT_RSP_ECNT	(NCSI_PKT_CMD_ECNT   + 0x80)
#define NCSI_PKT_RSP_DCNT	(NCSI_PKT_CMD_DCNT   + 0x80)
#define NCSI_PKT_RSP_AE		(NCSI_PKT_CMD_AE     + 0x80)
#define NCSI_PKT_RSP_SL		(NCSI_PKT_CMD_SL     + 0x80)
#define NCSI_PKT_RSP_GLS	(NCSI_PKT_CMD_GLS    + 0x80)
#define NCSI_PKT_RSP_SVF	(NCSI_PKT_CMD_SVF    + 0x80)
#define NCSI_PKT_RSP_EV		(NCSI_PKT_CMD_EV     + 0x80)
#define NCSI_PKT_RSP_DV		(NCSI_PKT_CMD_DV     + 0x80)
#define NCSI_PKT_RSP_SMA	(NCSI_PKT_CMD_SMA    + 0x80)
#define NCSI_PKT_RSP_EBF	(NCSI_PKT_CMD_EBF    + 0x80)
#define NCSI_PKT_RSP_DBF	(NCSI_PKT_CMD_DBF    + 0x80)
#define NCSI_PKT_RSP_EGMF	(NCSI_PKT_CMD_EGMF   + 0x80)
#define NCSI_PKT_RSP_DGMF	(NCSI_PKT_CMD_DGMF   + 0x80)
#define NCSI_PKT_RSP_SNFC	(NCSI_PKT_CMD_SNFC   + 0x80)
#define NCSI_PKT_RSP_GVI	(NCSI_PKT_CMD_GVI    + 0x80)
#define NCSI_PKT_RSP_GC		(NCSI_PKT_CMD_GC     + 0x80)
#define NCSI_PKT_RSP_GP		(NCSI_PKT_CMD_GP     + 0x80)
#define NCSI_PKT_RSP_GCPS	(NCSI_PKT_CMD_GCPS   + 0x80)
#define NCSI_PKT_RSP_GNS	(NCSI_PKT_CMD_GNS    + 0x80)
#define NCSI_PKT_RSP_GNPTS	(NCSI_PKT_CMD_GNPTS  + 0x80)
#define NCSI_PKT_RSP_GPS	(NCSI_PKT_CMD_GPS    + 0x80)
#define NCSI_PKT_RSP_OEM	(NCSI_PKT_CMD_OEM    + 0x80)
#define NCSI_PKT_RSP_PLDM	(NCSI_PKT_CMD_PLDM   + 0x80)
#define NCSI_PKT_RSP_GPUUID	(NCSI_PKT_CMD_GPUUID + 0x80)
#define NCSI_PKT_RSP_QPNPR	(NCSI_PKT_CMD_QPNPR   + 0x80)
#define NCSI_PKT_RSP_SNPR	(NCSI_PKT_CMD_SNPR   + 0x80)
#define NCSI_PKT_RSP_C_COMPLETED	0x0000  
#define NCSI_PKT_RSP_C_FAILED		0x0001  
#define NCSI_PKT_RSP_C_UNAVAILABLE	0x0002  
#define NCSI_PKT_RSP_C_UNSUPPORTED	0x0003  
#define NCSI_PKT_RSP_R_NO_ERROR		0x0000  
#define NCSI_PKT_RSP_R_INTERFACE	0x0001  
#define NCSI_PKT_RSP_R_PARAM		0x0002  
#define NCSI_PKT_RSP_R_CHANNEL		0x0003  
#define NCSI_PKT_RSP_R_PACKAGE		0x0004  
#define NCSI_PKT_RSP_R_LENGTH		0x0005  
#define NCSI_PKT_RSP_R_UNKNOWN		0x7fff  
#define NCSI_PKT_AEN		0xFF  
#define NCSI_PKT_AEN_LSC	0x00  
#define NCSI_PKT_AEN_CR		0x01  
#define NCSI_PKT_AEN_HNCDSC	0x02  
#endif  
