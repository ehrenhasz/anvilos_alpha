enum test_commands {
	CMD_STOP,		 
	CMD_START,		 
	CMD_ECHO,		 
	CMD_ACK,		 
	CMD_GET_XDP_CAP,	 
	CMD_GET_STATS,		 
};
#define DUT_CTRL_PORT	12345
#define DUT_ECHO_PORT	12346
struct tlv_hdr {
	__be16 type;
	__be16 len;
	__u8 data[];
};
