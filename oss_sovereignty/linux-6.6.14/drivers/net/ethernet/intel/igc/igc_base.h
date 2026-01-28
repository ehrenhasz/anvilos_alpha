#ifndef _IGC_BASE_H_
#define _IGC_BASE_H_
void igc_rx_fifo_flush_base(struct igc_hw *hw);
void igc_power_down_phy_copper_base(struct igc_hw *hw);
bool igc_is_device_id_i225(struct igc_hw *hw);
bool igc_is_device_id_i226(struct igc_hw *hw);
union igc_adv_tx_desc {
	struct {
		__le64 buffer_addr;     
		__le32 cmd_type_len;
		__le32 olinfo_status;
	} read;
	struct {
		__le64 rsvd;        
		__le32 nxtseq_seed;
		__le32 status;
	} wb;
};
struct igc_adv_tx_context_desc {
	__le32 vlan_macip_lens;
	__le32 launch_time;
	__le32 type_tucmd_mlhl;
	__le32 mss_l4len_idx;
};
#define IGC_ADVTXD_MAC_TSTAMP	0x00080000  
#define IGC_ADVTXD_TSTAMP_REG_1	0x00010000  
#define IGC_ADVTXD_TSTAMP_REG_2	0x00020000  
#define IGC_ADVTXD_TSTAMP_REG_3	0x00030000  
#define IGC_ADVTXD_DTYP_CTXT	0x00200000  
#define IGC_ADVTXD_DTYP_DATA	0x00300000  
#define IGC_ADVTXD_DCMD_EOP	0x01000000  
#define IGC_ADVTXD_DCMD_IFCS	0x02000000  
#define IGC_ADVTXD_DCMD_RS	0x08000000  
#define IGC_ADVTXD_DCMD_DEXT	0x20000000  
#define IGC_ADVTXD_DCMD_VLE	0x40000000  
#define IGC_ADVTXD_DCMD_TSE	0x80000000  
#define IGC_ADVTXD_PAYLEN_SHIFT	14  
#define IGC_RAR_ENTRIES		16
union igc_adv_rx_desc {
	struct {
		__le64 pkt_addr;  
		__le64 hdr_addr;  
	} read;
	struct {
		struct {
			union {
				__le32 data;
				struct {
					__le16 pkt_info;  
					__le16 hdr_info;
				} hs_rss;
			} lo_dword;
			union {
				__le32 rss;  
				struct {
					__le16 ip_id;  
					__le16 csum;  
				} csum_ip;
			} hi_dword;
		} lower;
		struct {
			__le32 status_error;  
			__le16 length;  
			__le16 vlan;  
		} upper;
	} wb;   
};
#define IGC_TXDCTL_QUEUE_ENABLE	0x02000000  
#define IGC_TXDCTL_SWFLUSH	0x04000000  
#define IGC_RXDCTL_QUEUE_ENABLE	0x02000000  
#define IGC_RXDCTL_SWFLUSH		0x04000000  
#define IGC_SRRCTL_BSIZEPKT_MASK	GENMASK(6, 0)
#define IGC_SRRCTL_BSIZEPKT(x)		FIELD_PREP(IGC_SRRCTL_BSIZEPKT_MASK, \
					(x) / 1024)  
#define IGC_SRRCTL_BSIZEHDR_MASK	GENMASK(13, 8)
#define IGC_SRRCTL_BSIZEHDR(x)		FIELD_PREP(IGC_SRRCTL_BSIZEHDR_MASK, \
					(x) / 64)  
#define IGC_SRRCTL_DESCTYPE_MASK	GENMASK(27, 25)
#define IGC_SRRCTL_DESCTYPE_ADV_ONEBUF	FIELD_PREP(IGC_SRRCTL_DESCTYPE_MASK, 1)
#endif  
