#ifndef GAUDI_FW_IF_H
#define GAUDI_FW_IF_H
#define GAUDI_EVENT_QUEUE_MSI_IDX	8
#define GAUDI_NIC_PORT1_MSI_IDX		10
#define GAUDI_NIC_PORT3_MSI_IDX		12
#define GAUDI_NIC_PORT5_MSI_IDX		14
#define GAUDI_NIC_PORT7_MSI_IDX		16
#define GAUDI_NIC_PORT9_MSI_IDX		18
#define UBOOT_FW_OFFSET			0x100000	 
#define LINUX_FW_OFFSET			0x800000	 
#define HBM_TEMP_ADJUST_COEFF		6
enum gaudi_nic_axi_error {
	RXB,
	RXE,
	TXS,
	TXE,
	QPC_RESP,
	NON_AXI_ERR,
	TMR,
};
struct eq_nic_sei_event {
	__u8 axi_error_cause;
	__u8 id;
	__u8 pad[6];
};
struct gaudi_nic_status {
	__u32 port;
	__u32 bad_format_cnt;
	__u32 responder_out_of_sequence_psn_cnt;
	__u32 high_ber_reinit;
	__u32 correctable_err_cnt;
	__u32 uncorrectable_err_cnt;
	__u32 retraining_cnt;
	__u8 up;
	__u8 pcs_link;
	__u8 phy_ready;
	__u8 auto_neg;
	__u32 timeout_retransmission_cnt;
	__u32 high_ber_cnt;
};
struct gaudi_cold_rst_data {
	union {
		struct {
			u32 spsram_init_done : 1;
			u32 reserved : 31;
		};
		__le32 data;
	};
};
#define GAUDI_PLL_FREQ_LOW		200000000  
#endif  
