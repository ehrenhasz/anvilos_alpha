

#ifndef __FSL_DPMAC_H
#define __FSL_DPMAC_H



struct fsl_mc_io;

int dpmac_open(struct fsl_mc_io *mc_io,
	       u32 cmd_flags,
	       int dpmac_id,
	       u16 *token);

int dpmac_close(struct fsl_mc_io *mc_io,
		u32 cmd_flags,
		u16 token);


enum dpmac_link_type {
	DPMAC_LINK_TYPE_NONE,
	DPMAC_LINK_TYPE_FIXED,
	DPMAC_LINK_TYPE_PHY,
	DPMAC_LINK_TYPE_BACKPLANE
};


enum dpmac_eth_if {
	DPMAC_ETH_IF_MII,
	DPMAC_ETH_IF_RMII,
	DPMAC_ETH_IF_SMII,
	DPMAC_ETH_IF_GMII,
	DPMAC_ETH_IF_RGMII,
	DPMAC_ETH_IF_SGMII,
	DPMAC_ETH_IF_QSGMII,
	DPMAC_ETH_IF_XAUI,
	DPMAC_ETH_IF_XFI,
	DPMAC_ETH_IF_CAUI,
	DPMAC_ETH_IF_1000BASEX,
	DPMAC_ETH_IF_USXGMII,
};


struct dpmac_attr {
	u16 id;
	u32 max_rate;
	enum dpmac_eth_if eth_if;
	enum dpmac_link_type link_type;
};

int dpmac_get_attributes(struct fsl_mc_io *mc_io,
			 u32 cmd_flags,
			 u16 token,
			 struct dpmac_attr *attr);



#define DPMAC_LINK_OPT_AUTONEG			BIT_ULL(0)
#define DPMAC_LINK_OPT_HALF_DUPLEX		BIT_ULL(1)
#define DPMAC_LINK_OPT_PAUSE			BIT_ULL(2)
#define DPMAC_LINK_OPT_ASYM_PAUSE		BIT_ULL(3)


#define DPMAC_ADVERTISED_10BASET_FULL		BIT_ULL(0)
#define DPMAC_ADVERTISED_100BASET_FULL		BIT_ULL(1)
#define DPMAC_ADVERTISED_1000BASET_FULL		BIT_ULL(2)
#define DPMAC_ADVERTISED_10000BASET_FULL	BIT_ULL(4)
#define DPMAC_ADVERTISED_2500BASEX_FULL		BIT_ULL(5)


#define DPMAC_ADVERTISED_AUTONEG		BIT_ULL(3)


struct dpmac_link_state {
	u32 rate;
	u64 options;
	int up;
	int state_valid;
	u64 supported;
	u64 advertising;
};

int dpmac_set_link_state(struct fsl_mc_io *mc_io,
			 u32 cmd_flags,
			 u16 token,
			 struct dpmac_link_state *link_state);


enum dpmac_counter_id {
	DPMAC_CNT_ING_FRAME_64,
	DPMAC_CNT_ING_FRAME_127,
	DPMAC_CNT_ING_FRAME_255,
	DPMAC_CNT_ING_FRAME_511,
	DPMAC_CNT_ING_FRAME_1023,
	DPMAC_CNT_ING_FRAME_1518,
	DPMAC_CNT_ING_FRAME_1519_MAX,
	DPMAC_CNT_ING_FRAG,
	DPMAC_CNT_ING_JABBER,
	DPMAC_CNT_ING_FRAME_DISCARD,
	DPMAC_CNT_ING_ALIGN_ERR,
	DPMAC_CNT_EGR_UNDERSIZED,
	DPMAC_CNT_ING_OVERSIZED,
	DPMAC_CNT_ING_VALID_PAUSE_FRAME,
	DPMAC_CNT_EGR_VALID_PAUSE_FRAME,
	DPMAC_CNT_ING_BYTE,
	DPMAC_CNT_ING_MCAST_FRAME,
	DPMAC_CNT_ING_BCAST_FRAME,
	DPMAC_CNT_ING_ALL_FRAME,
	DPMAC_CNT_ING_UCAST_FRAME,
	DPMAC_CNT_ING_ERR_FRAME,
	DPMAC_CNT_EGR_BYTE,
	DPMAC_CNT_EGR_MCAST_FRAME,
	DPMAC_CNT_EGR_BCAST_FRAME,
	DPMAC_CNT_EGR_UCAST_FRAME,
	DPMAC_CNT_EGR_ERR_FRAME,
	DPMAC_CNT_ING_GOOD_FRAME,
	DPMAC_CNT_EGR_GOOD_FRAME
};

int dpmac_get_counter(struct fsl_mc_io *mc_io, u32 cmd_flags, u16 token,
		      enum dpmac_counter_id id, u64 *value);

int dpmac_get_api_version(struct fsl_mc_io *mc_io, u32 cmd_flags,
			  u16 *major_ver, u16 *minor_ver);

int dpmac_set_protocol(struct fsl_mc_io *mc_io, u32 cmd_flags, u16 token,
		       enum dpmac_eth_if protocol);
#endif 
