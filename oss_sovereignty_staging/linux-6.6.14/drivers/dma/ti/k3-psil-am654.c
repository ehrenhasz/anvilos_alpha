
 

#include <linux/kernel.h>

#include "k3-psil-priv.h"

#define PSIL_PDMA_XY_TR(x)				\
	{						\
		.thread_id = x,				\
		.ep_config = {				\
			.ep_type = PSIL_EP_PDMA_XY,	\
		},					\
	}

#define PSIL_PDMA_XY_PKT(x)				\
	{						\
		.thread_id = x,				\
		.ep_config = {				\
			.ep_type = PSIL_EP_PDMA_XY,	\
			.pkt_mode = 1,			\
		},					\
	}

#define PSIL_ETHERNET(x)				\
	{						\
		.thread_id = x,				\
		.ep_config = {				\
			.ep_type = PSIL_EP_NATIVE,	\
			.pkt_mode = 1,			\
			.needs_epib = 1,		\
			.psd_size = 16,			\
		},					\
	}

#define PSIL_SA2UL(x, tx)				\
	{						\
		.thread_id = x,				\
		.ep_config = {				\
			.ep_type = PSIL_EP_NATIVE,	\
			.pkt_mode = 1,			\
			.needs_epib = 1,		\
			.psd_size = 64,			\
			.notdpkt = tx,			\
		},					\
	}

 
static struct psil_ep am654_src_ep_map[] = {
	 
	PSIL_SA2UL(0x4000, 0),
	PSIL_SA2UL(0x4001, 0),
	PSIL_SA2UL(0x4002, 0),
	PSIL_SA2UL(0x4003, 0),
	 
	PSIL_ETHERNET(0x4100),
	PSIL_ETHERNET(0x4101),
	PSIL_ETHERNET(0x4102),
	PSIL_ETHERNET(0x4103),
	 
	PSIL_ETHERNET(0x4200),
	PSIL_ETHERNET(0x4201),
	PSIL_ETHERNET(0x4202),
	PSIL_ETHERNET(0x4203),
	 
	PSIL_ETHERNET(0x4300),
	PSIL_ETHERNET(0x4301),
	PSIL_ETHERNET(0x4302),
	PSIL_ETHERNET(0x4303),
	 
	PSIL_PDMA_XY_TR(0x4400),
	PSIL_PDMA_XY_TR(0x4401),
	PSIL_PDMA_XY_TR(0x4402),
	 
	PSIL_PDMA_XY_PKT(0x4500),
	PSIL_PDMA_XY_PKT(0x4501),
	PSIL_PDMA_XY_PKT(0x4502),
	PSIL_PDMA_XY_PKT(0x4503),
	PSIL_PDMA_XY_PKT(0x4504),
	PSIL_PDMA_XY_PKT(0x4505),
	PSIL_PDMA_XY_PKT(0x4506),
	PSIL_PDMA_XY_PKT(0x4507),
	PSIL_PDMA_XY_PKT(0x4508),
	PSIL_PDMA_XY_PKT(0x4509),
	PSIL_PDMA_XY_PKT(0x450a),
	PSIL_PDMA_XY_PKT(0x450b),
	PSIL_PDMA_XY_PKT(0x450c),
	PSIL_PDMA_XY_PKT(0x450d),
	PSIL_PDMA_XY_PKT(0x450e),
	PSIL_PDMA_XY_PKT(0x450f),
	PSIL_PDMA_XY_PKT(0x4510),
	PSIL_PDMA_XY_PKT(0x4511),
	PSIL_PDMA_XY_PKT(0x4512),
	PSIL_PDMA_XY_PKT(0x4513),
	 
	PSIL_PDMA_XY_PKT(0x4514),
	PSIL_PDMA_XY_PKT(0x4515),
	PSIL_PDMA_XY_PKT(0x4516),
	 
	PSIL_ETHERNET(0x7000),
	 
	PSIL_PDMA_XY_TR(0x7100),
	PSIL_PDMA_XY_TR(0x7101),
	PSIL_PDMA_XY_TR(0x7102),
	PSIL_PDMA_XY_TR(0x7103),
	 
	PSIL_PDMA_XY_PKT(0x7200),
	PSIL_PDMA_XY_PKT(0x7201),
	PSIL_PDMA_XY_PKT(0x7202),
	PSIL_PDMA_XY_PKT(0x7203),
	PSIL_PDMA_XY_PKT(0x7204),
	PSIL_PDMA_XY_PKT(0x7205),
	PSIL_PDMA_XY_PKT(0x7206),
	PSIL_PDMA_XY_PKT(0x7207),
	PSIL_PDMA_XY_PKT(0x7208),
	PSIL_PDMA_XY_PKT(0x7209),
	PSIL_PDMA_XY_PKT(0x720a),
	PSIL_PDMA_XY_PKT(0x720b),
	 
	PSIL_PDMA_XY_PKT(0x7212),
};

 
static struct psil_ep am654_dst_ep_map[] = {
	 
	PSIL_SA2UL(0xc000, 1),
	PSIL_SA2UL(0xc001, 1),
	 
	PSIL_ETHERNET(0xc100),
	PSIL_ETHERNET(0xc101),
	PSIL_ETHERNET(0xc102),
	PSIL_ETHERNET(0xc103),
	PSIL_ETHERNET(0xc104),
	PSIL_ETHERNET(0xc105),
	PSIL_ETHERNET(0xc106),
	PSIL_ETHERNET(0xc107),
	 
	PSIL_ETHERNET(0xc200),
	PSIL_ETHERNET(0xc201),
	PSIL_ETHERNET(0xc202),
	PSIL_ETHERNET(0xc203),
	PSIL_ETHERNET(0xc204),
	PSIL_ETHERNET(0xc205),
	PSIL_ETHERNET(0xc206),
	PSIL_ETHERNET(0xc207),
	 
	PSIL_ETHERNET(0xc300),
	PSIL_ETHERNET(0xc301),
	PSIL_ETHERNET(0xc302),
	PSIL_ETHERNET(0xc303),
	PSIL_ETHERNET(0xc304),
	PSIL_ETHERNET(0xc305),
	PSIL_ETHERNET(0xc306),
	PSIL_ETHERNET(0xc307),
	 
	PSIL_ETHERNET(0xf000),
	PSIL_ETHERNET(0xf001),
	PSIL_ETHERNET(0xf002),
	PSIL_ETHERNET(0xf003),
	PSIL_ETHERNET(0xf004),
	PSIL_ETHERNET(0xf005),
	PSIL_ETHERNET(0xf006),
	PSIL_ETHERNET(0xf007),
};

struct psil_ep_map am654_ep_map = {
	.name = "am654",
	.src = am654_src_ep_map,
	.src_count = ARRAY_SIZE(am654_src_ep_map),
	.dst = am654_dst_ep_map,
	.dst_count = ARRAY_SIZE(am654_dst_ep_map),
};
