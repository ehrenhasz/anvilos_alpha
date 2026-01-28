


#ifndef _DRIVERS_SLIMBUS_H
#define _DRIVERS_SLIMBUS_H
#include <linux/module.h>
#include <linux/device.h>
#include <linux/mutex.h>
#include <linux/completion.h>
#include <linux/slimbus.h>


#define SLIM_CL_PER_SUPERFRAME		6144
#define SLIM_CL_PER_SUPERFRAME_DIV8	(SLIM_CL_PER_SUPERFRAME >> 3)


#define SLIM_MSG_MT_CORE			0x0
#define SLIM_MSG_MT_DEST_REFERRED_USER		0x2
#define SLIM_MSG_MT_SRC_REFERRED_USER		0x6


#define SLIM_MSG_MT_MASK	GENMASK(2, 0)
#define SLIM_MSG_MT_SHIFT	5
#define SLIM_MSG_RL_MASK	GENMASK(4, 0)
#define SLIM_MSG_RL_SHIFT	0
#define SLIM_MSG_MC_MASK	GENMASK(6, 0)
#define SLIM_MSG_MC_SHIFT	0
#define SLIM_MSG_DT_MASK	GENMASK(1, 0)
#define SLIM_MSG_DT_SHIFT	4

#define SLIM_HEADER_GET_MT(b)	((b >> SLIM_MSG_MT_SHIFT) & SLIM_MSG_MT_MASK)
#define SLIM_HEADER_GET_RL(b)	((b >> SLIM_MSG_RL_SHIFT) & SLIM_MSG_RL_MASK)
#define SLIM_HEADER_GET_MC(b)	((b >> SLIM_MSG_MC_SHIFT) & SLIM_MSG_MC_MASK)
#define SLIM_HEADER_GET_DT(b)	((b >> SLIM_MSG_DT_SHIFT) & SLIM_MSG_DT_MASK)


#define SLIM_MSG_MC_REPORT_PRESENT               0x1
#define SLIM_MSG_MC_ASSIGN_LOGICAL_ADDRESS       0x2
#define SLIM_MSG_MC_REPORT_ABSENT                0xF


#define SLIM_MSG_MC_CONNECT_SOURCE		0x10
#define SLIM_MSG_MC_CONNECT_SINK		0x11
#define SLIM_MSG_MC_DISCONNECT_PORT		0x14
#define SLIM_MSG_MC_CHANGE_CONTENT		0x18


#define SLIM_MSG_MC_BEGIN_RECONFIGURATION        0x40
#define SLIM_MSG_MC_NEXT_PAUSE_CLOCK             0x4A
#define SLIM_MSG_MC_NEXT_DEFINE_CHANNEL          0x50
#define SLIM_MSG_MC_NEXT_DEFINE_CONTENT          0x51
#define SLIM_MSG_MC_NEXT_ACTIVATE_CHANNEL        0x54
#define SLIM_MSG_MC_NEXT_DEACTIVATE_CHANNEL      0x55
#define SLIM_MSG_MC_NEXT_REMOVE_CHANNEL          0x58
#define SLIM_MSG_MC_RECONFIGURE_NOW              0x5F


#define SLIM_CLK_FAST				0
#define SLIM_CLK_CONST_PHASE			1
#define SLIM_CLK_UNSPECIFIED			2


#define SLIM_MSG_DEST_LOGICALADDR	0
#define SLIM_MSG_DEST_ENUMADDR		1
#define	SLIM_MSG_DEST_BROADCAST		3


#define SLIM_MAX_CLK_GEAR		10
#define SLIM_MIN_CLK_GEAR		1
#define SLIM_SLOT_LEN_BITS		4


#define SLIM_CHANNEL_CONTENT_FL		BIT(7)


#define SLIM_CL_PER_SUPERFRAME		6144
#define SLIM_SLOTS_PER_SUPERFRAME	(SLIM_CL_PER_SUPERFRAME >> 2)
#define SLIM_SL_PER_SUPERFRAME		(SLIM_CL_PER_SUPERFRAME >> 2)

#define SLIM_LA_MANAGER 0xFF

#define SLIM_MAX_TIDS			256

struct slim_framer {
	struct device		dev;
	struct slim_eaddr	e_addr;
	int			rootfreq;
	int			superfreq;
};

#define to_slim_framer(d) container_of(d, struct slim_framer, dev)


struct slim_msg_txn {
	u8			rl;
	u8			mt;
	u8			mc;
	u8			dt;
	u16			ec;
	u8			tid;
	u8			la;
	struct slim_val_inf	*msg;
	struct	completion	*comp;
};


#define DEFINE_SLIM_LDEST_TXN(name, mc, rl, la, msg) \
	struct slim_msg_txn name = { rl, 0, mc, SLIM_MSG_DEST_LOGICALADDR, 0,\
					0, la, msg, }

#define DEFINE_SLIM_BCAST_TXN(name, mc, rl, la, msg) \
	struct slim_msg_txn name = { rl, 0, mc, SLIM_MSG_DEST_BROADCAST, 0,\
					0, la, msg, }

#define DEFINE_SLIM_EDEST_TXN(name, mc, rl, la, msg) \
	struct slim_msg_txn name = { rl, 0, mc, SLIM_MSG_DEST_ENUMADDR, 0,\
					0, la, msg, }

enum slim_clk_state {
	SLIM_CLK_ACTIVE,
	SLIM_CLK_ENTERING_PAUSE,
	SLIM_CLK_PAUSED,
};


struct slim_sched {
	enum slim_clk_state	clk_state;
	struct completion	pause_comp;
	struct mutex		m_reconf;
};


enum slim_port_direction {
	SLIM_PORT_SINK = 0,
	SLIM_PORT_SOURCE,
};

enum slim_port_state {
	SLIM_PORT_DISCONNECTED = 0,
	SLIM_PORT_UNCONFIGURED,
	SLIM_PORT_CONFIGURED,
};


enum slim_channel_state {
	SLIM_CH_STATE_DISCONNECTED = 0,
	SLIM_CH_STATE_ALLOCATED,
	SLIM_CH_STATE_ASSOCIATED,
	SLIM_CH_STATE_DEFINED,
	SLIM_CH_STATE_CONTENT_DEFINED,
	SLIM_CH_STATE_ACTIVE,
	SLIM_CH_STATE_REMOVED,
};


enum slim_ch_data_fmt {
	SLIM_CH_DATA_FMT_NOT_DEFINED = 0,
	SLIM_CH_DATA_FMT_LPCM_AUDIO = 1,
	SLIM_CH_DATA_FMT_IEC61937_COMP_AUDIO = 2,
	SLIM_CH_DATA_FMT_PACKED_PDM_AUDIO = 3,
};


enum slim_ch_aux_bit_fmt {
	SLIM_CH_AUX_FMT_NOT_APPLICABLE = 0,
	SLIM_CH_AUX_FMT_ZCUV_TUNNEL_IEC60958 = 1,
	SLIM_CH_AUX_FMT_USER_DEFINED = 0xF,
};


struct slim_channel {
	int id;
	int prrate;
	int seg_dist;
	enum slim_ch_data_fmt data_fmt;
	enum slim_ch_aux_bit_fmt aux_fmt;
	enum slim_channel_state state;
};


struct slim_port {
	int id;
	enum slim_port_direction direction;
	enum slim_port_state state;
	struct slim_channel ch;
};


enum slim_transport_protocol {
	SLIM_PROTO_ISO = 0,
	SLIM_PROTO_PUSH,
	SLIM_PROTO_PULL,
	SLIM_PROTO_LOCKED,
	SLIM_PROTO_ASYNC_SMPLX,
	SLIM_PROTO_ASYNC_HALF_DUP,
	SLIM_PROTO_EXT_SMPLX,
	SLIM_PROTO_EXT_HALF_DUP,
};


struct slim_stream_runtime {
	const char *name;
	struct slim_device *dev;
	int direction;
	enum slim_transport_protocol prot;
	unsigned int rate;
	unsigned int bps;
	unsigned int ratem;
	int num_ports;
	struct slim_port *ports;
	struct list_head node;
};


struct slim_controller {
	struct device		*dev;
	unsigned int		id;
	char			name[SLIMBUS_NAME_SIZE];
	int			min_cg;
	int			max_cg;
	int			clkgear;
	struct ida		laddr_ida;
	struct slim_framer	*a_framer;
	struct mutex		lock;
	struct list_head	devices;
	struct idr		tid_idr;
	spinlock_t		txn_lock;
	struct slim_sched	sched;
	int			(*xfer_msg)(struct slim_controller *ctrl,
					    struct slim_msg_txn *tx);
	int			(*set_laddr)(struct slim_controller *ctrl,
					     struct slim_eaddr *ea, u8 laddr);
	int			(*get_laddr)(struct slim_controller *ctrl,
					     struct slim_eaddr *ea, u8 *laddr);
	int		(*enable_stream)(struct slim_stream_runtime *rt);
	int		(*disable_stream)(struct slim_stream_runtime *rt);
	int			(*wakeup)(struct slim_controller *ctrl);
};

int slim_device_report_present(struct slim_controller *ctrl,
			       struct slim_eaddr *e_addr, u8 *laddr);
void slim_report_absent(struct slim_device *sbdev);
int slim_register_controller(struct slim_controller *ctrl);
int slim_unregister_controller(struct slim_controller *ctrl);
void slim_msg_response(struct slim_controller *ctrl, u8 *reply, u8 tid, u8 l);
int slim_do_transfer(struct slim_controller *ctrl, struct slim_msg_txn *txn);
int slim_ctrl_clk_pause(struct slim_controller *ctrl, bool wakeup, u8 restart);
int slim_alloc_txn_tid(struct slim_controller *ctrl, struct slim_msg_txn *txn);
void slim_free_txn_tid(struct slim_controller *ctrl, struct slim_msg_txn *txn);

static inline bool slim_tid_txn(u8 mt, u8 mc)
{
	return (mt == SLIM_MSG_MT_CORE &&
		(mc == SLIM_MSG_MC_REQUEST_INFORMATION ||
		 mc == SLIM_MSG_MC_REQUEST_CLEAR_INFORMATION ||
		 mc == SLIM_MSG_MC_REQUEST_VALUE ||
		 mc == SLIM_MSG_MC_REQUEST_CHANGE_VALUE));
}

static inline bool slim_ec_txn(u8 mt, u8 mc)
{
	return (mt == SLIM_MSG_MT_CORE &&
		((mc >= SLIM_MSG_MC_REQUEST_INFORMATION &&
		  mc <= SLIM_MSG_MC_REPORT_INFORMATION) ||
		 (mc >= SLIM_MSG_MC_REQUEST_VALUE &&
		  mc <= SLIM_MSG_MC_CHANGE_VALUE)));
}
#endif 
