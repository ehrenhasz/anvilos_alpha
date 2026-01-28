#ifndef _MLX4_EN_PORT_H_
#define _MLX4_EN_PORT_H_
#define SET_PORT_PROMISC_SHIFT	31
#define SET_PORT_MC_PROMISC_SHIFT	30
#define MLX4_EN_NUM_TC		8
#define VLAN_FLTR_SIZE	128
struct mlx4_set_vlan_fltr_mbox {
	__be32 entry[VLAN_FLTR_SIZE];
};
enum {
	MLX4_MCAST_CONFIG       = 0,
	MLX4_MCAST_DISABLE      = 1,
	MLX4_MCAST_ENABLE       = 2,
};
enum mlx4_link_mode {
	MLX4_1000BASE_CX_SGMII	 = 0,
	MLX4_1000BASE_KX	 = 1,
	MLX4_10GBASE_CX4	 = 2,
	MLX4_10GBASE_KX4	 = 3,
	MLX4_10GBASE_KR		 = 4,
	MLX4_20GBASE_KR2	 = 5,
	MLX4_40GBASE_CR4	 = 6,
	MLX4_40GBASE_KR4	 = 7,
	MLX4_56GBASE_KR4	 = 8,
	MLX4_10GBASE_CR		 = 12,
	MLX4_10GBASE_SR		 = 13,
	MLX4_40GBASE_SR4	 = 15,
	MLX4_56GBASE_CR4	 = 17,
	MLX4_56GBASE_SR4	 = 18,
	MLX4_100BASE_TX		 = 24,
	MLX4_1000BASE_T		 = 25,
	MLX4_10GBASE_T		 = 26,
};
#define MLX4_PROT_MASK(link_mode) (1<<link_mode)
enum {
	MLX4_EN_100M_SPEED	= 0x04,
	MLX4_EN_10G_SPEED_XAUI	= 0x00,
	MLX4_EN_10G_SPEED_XFI	= 0x01,
	MLX4_EN_1G_SPEED	= 0x02,
	MLX4_EN_20G_SPEED	= 0x08,
	MLX4_EN_40G_SPEED	= 0x40,
	MLX4_EN_56G_SPEED	= 0x20,
	MLX4_EN_OTHER_SPEED	= 0x0f,
};
struct mlx4_en_query_port_context {
	u8 link_up;
#define MLX4_EN_LINK_UP_MASK	0x80
#define MLX4_EN_ANC_MASK	0x40
	u8 autoneg;
#define MLX4_EN_AUTONEG_MASK	0x80
	__be16 mtu;
	u8 reserved2;
	u8 link_speed;
#define MLX4_EN_SPEED_MASK	0x6f
	u16 reserved3[5];
	__be64 mac;
	u8 transceiver;
};
struct mlx4_en_stat_out_mbox {
	__be64 R64_prio_0;
	__be64 R64_prio_1;
	__be64 R64_prio_2;
	__be64 R64_prio_3;
	__be64 R64_prio_4;
	__be64 R64_prio_5;
	__be64 R64_prio_6;
	__be64 R64_prio_7;
	__be64 R64_novlan;
	__be64 R127_prio_0;
	__be64 R127_prio_1;
	__be64 R127_prio_2;
	__be64 R127_prio_3;
	__be64 R127_prio_4;
	__be64 R127_prio_5;
	__be64 R127_prio_6;
	__be64 R127_prio_7;
	__be64 R127_novlan;
	__be64 R255_prio_0;
	__be64 R255_prio_1;
	__be64 R255_prio_2;
	__be64 R255_prio_3;
	__be64 R255_prio_4;
	__be64 R255_prio_5;
	__be64 R255_prio_6;
	__be64 R255_prio_7;
	__be64 R255_novlan;
	__be64 R511_prio_0;
	__be64 R511_prio_1;
	__be64 R511_prio_2;
	__be64 R511_prio_3;
	__be64 R511_prio_4;
	__be64 R511_prio_5;
	__be64 R511_prio_6;
	__be64 R511_prio_7;
	__be64 R511_novlan;
	__be64 R1023_prio_0;
	__be64 R1023_prio_1;
	__be64 R1023_prio_2;
	__be64 R1023_prio_3;
	__be64 R1023_prio_4;
	__be64 R1023_prio_5;
	__be64 R1023_prio_6;
	__be64 R1023_prio_7;
	__be64 R1023_novlan;
	__be64 R1518_prio_0;
	__be64 R1518_prio_1;
	__be64 R1518_prio_2;
	__be64 R1518_prio_3;
	__be64 R1518_prio_4;
	__be64 R1518_prio_5;
	__be64 R1518_prio_6;
	__be64 R1518_prio_7;
	__be64 R1518_novlan;
	__be64 R1522_prio_0;
	__be64 R1522_prio_1;
	__be64 R1522_prio_2;
	__be64 R1522_prio_3;
	__be64 R1522_prio_4;
	__be64 R1522_prio_5;
	__be64 R1522_prio_6;
	__be64 R1522_prio_7;
	__be64 R1522_novlan;
	__be64 R1548_prio_0;
	__be64 R1548_prio_1;
	__be64 R1548_prio_2;
	__be64 R1548_prio_3;
	__be64 R1548_prio_4;
	__be64 R1548_prio_5;
	__be64 R1548_prio_6;
	__be64 R1548_prio_7;
	__be64 R1548_novlan;
	__be64 R2MTU_prio_0;
	__be64 R2MTU_prio_1;
	__be64 R2MTU_prio_2;
	__be64 R2MTU_prio_3;
	__be64 R2MTU_prio_4;
	__be64 R2MTU_prio_5;
	__be64 R2MTU_prio_6;
	__be64 R2MTU_prio_7;
	__be64 R2MTU_novlan;
	__be64 RGIANT_prio_0;
	__be64 RGIANT_prio_1;
	__be64 RGIANT_prio_2;
	__be64 RGIANT_prio_3;
	__be64 RGIANT_prio_4;
	__be64 RGIANT_prio_5;
	__be64 RGIANT_prio_6;
	__be64 RGIANT_prio_7;
	__be64 RGIANT_novlan;
	__be64 RBCAST_prio_0;
	__be64 RBCAST_prio_1;
	__be64 RBCAST_prio_2;
	__be64 RBCAST_prio_3;
	__be64 RBCAST_prio_4;
	__be64 RBCAST_prio_5;
	__be64 RBCAST_prio_6;
	__be64 RBCAST_prio_7;
	__be64 RBCAST_novlan;
	__be64 MCAST_prio_0;
	__be64 MCAST_prio_1;
	__be64 MCAST_prio_2;
	__be64 MCAST_prio_3;
	__be64 MCAST_prio_4;
	__be64 MCAST_prio_5;
	__be64 MCAST_prio_6;
	__be64 MCAST_prio_7;
	__be64 MCAST_novlan;
	__be64 RTOTG_prio_0;
	__be64 RTOTG_prio_1;
	__be64 RTOTG_prio_2;
	__be64 RTOTG_prio_3;
	__be64 RTOTG_prio_4;
	__be64 RTOTG_prio_5;
	__be64 RTOTG_prio_6;
	__be64 RTOTG_prio_7;
	__be64 RTOTG_novlan;
	__be64 RTTLOCT_prio_0;
	__be64 RTTLOCT_NOFRM_prio_0;
	__be64 ROCT_prio_0;
	__be64 RTTLOCT_prio_1;
	__be64 RTTLOCT_NOFRM_prio_1;
	__be64 ROCT_prio_1;
	__be64 RTTLOCT_prio_2;
	__be64 RTTLOCT_NOFRM_prio_2;
	__be64 ROCT_prio_2;
	__be64 RTTLOCT_prio_3;
	__be64 RTTLOCT_NOFRM_prio_3;
	__be64 ROCT_prio_3;
	__be64 RTTLOCT_prio_4;
	__be64 RTTLOCT_NOFRM_prio_4;
	__be64 ROCT_prio_4;
	__be64 RTTLOCT_prio_5;
	__be64 RTTLOCT_NOFRM_prio_5;
	__be64 ROCT_prio_5;
	__be64 RTTLOCT_prio_6;
	__be64 RTTLOCT_NOFRM_prio_6;
	__be64 ROCT_prio_6;
	__be64 RTTLOCT_prio_7;
	__be64 RTTLOCT_NOFRM_prio_7;
	__be64 ROCT_prio_7;
	__be64 RTTLOCT_novlan;
	__be64 RTTLOCT_NOFRM_novlan;
	__be64 ROCT_novlan;
	__be64 RTOT_prio_0;
	__be64 R1Q_prio_0;
	__be64 reserved1;
	__be64 RTOT_prio_1;
	__be64 R1Q_prio_1;
	__be64 reserved2;
	__be64 RTOT_prio_2;
	__be64 R1Q_prio_2;
	__be64 reserved3;
	__be64 RTOT_prio_3;
	__be64 R1Q_prio_3;
	__be64 reserved4;
	__be64 RTOT_prio_4;
	__be64 R1Q_prio_4;
	__be64 reserved5;
	__be64 RTOT_prio_5;
	__be64 R1Q_prio_5;
	__be64 reserved6;
	__be64 RTOT_prio_6;
	__be64 R1Q_prio_6;
	__be64 reserved7;
	__be64 RTOT_prio_7;
	__be64 R1Q_prio_7;
	__be64 reserved8;
	__be64 RTOT_novlan;
	__be64 R1Q_novlan;
	__be64 reserved9;
	__be64 RCNTL;
	__be64 reserved10;
	__be64 reserved11;
	__be64 reserved12;
	__be64 RInRangeLengthErr;
	__be64 ROutRangeLengthErr;
	__be64 RFrmTooLong;
	__be64 PCS;
	__be64 T64_prio_0;
	__be64 T64_prio_1;
	__be64 T64_prio_2;
	__be64 T64_prio_3;
	__be64 T64_prio_4;
	__be64 T64_prio_5;
	__be64 T64_prio_6;
	__be64 T64_prio_7;
	__be64 T64_novlan;
	__be64 T64_loopbk;
	__be64 T127_prio_0;
	__be64 T127_prio_1;
	__be64 T127_prio_2;
	__be64 T127_prio_3;
	__be64 T127_prio_4;
	__be64 T127_prio_5;
	__be64 T127_prio_6;
	__be64 T127_prio_7;
	__be64 T127_novlan;
	__be64 T127_loopbk;
	__be64 T255_prio_0;
	__be64 T255_prio_1;
	__be64 T255_prio_2;
	__be64 T255_prio_3;
	__be64 T255_prio_4;
	__be64 T255_prio_5;
	__be64 T255_prio_6;
	__be64 T255_prio_7;
	__be64 T255_novlan;
	__be64 T255_loopbk;
	__be64 T511_prio_0;
	__be64 T511_prio_1;
	__be64 T511_prio_2;
	__be64 T511_prio_3;
	__be64 T511_prio_4;
	__be64 T511_prio_5;
	__be64 T511_prio_6;
	__be64 T511_prio_7;
	__be64 T511_novlan;
	__be64 T511_loopbk;
	__be64 T1023_prio_0;
	__be64 T1023_prio_1;
	__be64 T1023_prio_2;
	__be64 T1023_prio_3;
	__be64 T1023_prio_4;
	__be64 T1023_prio_5;
	__be64 T1023_prio_6;
	__be64 T1023_prio_7;
	__be64 T1023_novlan;
	__be64 T1023_loopbk;
	__be64 T1518_prio_0;
	__be64 T1518_prio_1;
	__be64 T1518_prio_2;
	__be64 T1518_prio_3;
	__be64 T1518_prio_4;
	__be64 T1518_prio_5;
	__be64 T1518_prio_6;
	__be64 T1518_prio_7;
	__be64 T1518_novlan;
	__be64 T1518_loopbk;
	__be64 T1522_prio_0;
	__be64 T1522_prio_1;
	__be64 T1522_prio_2;
	__be64 T1522_prio_3;
	__be64 T1522_prio_4;
	__be64 T1522_prio_5;
	__be64 T1522_prio_6;
	__be64 T1522_prio_7;
	__be64 T1522_novlan;
	__be64 T1522_loopbk;
	__be64 T1548_prio_0;
	__be64 T1548_prio_1;
	__be64 T1548_prio_2;
	__be64 T1548_prio_3;
	__be64 T1548_prio_4;
	__be64 T1548_prio_5;
	__be64 T1548_prio_6;
	__be64 T1548_prio_7;
	__be64 T1548_novlan;
	__be64 T1548_loopbk;
	__be64 T2MTU_prio_0;
	__be64 T2MTU_prio_1;
	__be64 T2MTU_prio_2;
	__be64 T2MTU_prio_3;
	__be64 T2MTU_prio_4;
	__be64 T2MTU_prio_5;
	__be64 T2MTU_prio_6;
	__be64 T2MTU_prio_7;
	__be64 T2MTU_novlan;
	__be64 T2MTU_loopbk;
	__be64 TGIANT_prio_0;
	__be64 TGIANT_prio_1;
	__be64 TGIANT_prio_2;
	__be64 TGIANT_prio_3;
	__be64 TGIANT_prio_4;
	__be64 TGIANT_prio_5;
	__be64 TGIANT_prio_6;
	__be64 TGIANT_prio_7;
	__be64 TGIANT_novlan;
	__be64 TGIANT_loopbk;
	__be64 TBCAST_prio_0;
	__be64 TBCAST_prio_1;
	__be64 TBCAST_prio_2;
	__be64 TBCAST_prio_3;
	__be64 TBCAST_prio_4;
	__be64 TBCAST_prio_5;
	__be64 TBCAST_prio_6;
	__be64 TBCAST_prio_7;
	__be64 TBCAST_novlan;
	__be64 TBCAST_loopbk;
	__be64 TMCAST_prio_0;
	__be64 TMCAST_prio_1;
	__be64 TMCAST_prio_2;
	__be64 TMCAST_prio_3;
	__be64 TMCAST_prio_4;
	__be64 TMCAST_prio_5;
	__be64 TMCAST_prio_6;
	__be64 TMCAST_prio_7;
	__be64 TMCAST_novlan;
	__be64 TMCAST_loopbk;
	__be64 TTOTG_prio_0;
	__be64 TTOTG_prio_1;
	__be64 TTOTG_prio_2;
	__be64 TTOTG_prio_3;
	__be64 TTOTG_prio_4;
	__be64 TTOTG_prio_5;
	__be64 TTOTG_prio_6;
	__be64 TTOTG_prio_7;
	__be64 TTOTG_novlan;
	__be64 TTOTG_loopbk;
	__be64 TTTLOCT_prio_0;
	__be64 TTTLOCT_NOFRM_prio_0;
	__be64 TOCT_prio_0;
	__be64 TTTLOCT_prio_1;
	__be64 TTTLOCT_NOFRM_prio_1;
	__be64 TOCT_prio_1;
	__be64 TTTLOCT_prio_2;
	__be64 TTTLOCT_NOFRM_prio_2;
	__be64 TOCT_prio_2;
	__be64 TTTLOCT_prio_3;
	__be64 TTTLOCT_NOFRM_prio_3;
	__be64 TOCT_prio_3;
	__be64 TTTLOCT_prio_4;
	__be64 TTTLOCT_NOFRM_prio_4;
	__be64 TOCT_prio_4;
	__be64 TTTLOCT_prio_5;
	__be64 TTTLOCT_NOFRM_prio_5;
	__be64 TOCT_prio_5;
	__be64 TTTLOCT_prio_6;
	__be64 TTTLOCT_NOFRM_prio_6;
	__be64 TOCT_prio_6;
	__be64 TTTLOCT_prio_7;
	__be64 TTTLOCT_NOFRM_prio_7;
	__be64 TOCT_prio_7;
	__be64 TTTLOCT_novlan;
	__be64 TTTLOCT_NOFRM_novlan;
	__be64 TOCT_novlan;
	__be64 TTTLOCT_loopbk;
	__be64 TTTLOCT_NOFRM_loopbk;
	__be64 TOCT_loopbk;
	__be64 TTOT_prio_0;
	__be64 T1Q_prio_0;
	__be64 reserved13;
	__be64 TTOT_prio_1;
	__be64 T1Q_prio_1;
	__be64 reserved14;
	__be64 TTOT_prio_2;
	__be64 T1Q_prio_2;
	__be64 reserved15;
	__be64 TTOT_prio_3;
	__be64 T1Q_prio_3;
	__be64 reserved16;
	__be64 TTOT_prio_4;
	__be64 T1Q_prio_4;
	__be64 reserved17;
	__be64 TTOT_prio_5;
	__be64 T1Q_prio_5;
	__be64 reserved18;
	__be64 TTOT_prio_6;
	__be64 T1Q_prio_6;
	__be64 reserved19;
	__be64 TTOT_prio_7;
	__be64 T1Q_prio_7;
	__be64 reserved20;
	__be64 TTOT_novlan;
	__be64 T1Q_novlan;
	__be64 reserved21;
	__be64 TTOT_loopbk;
	__be64 T1Q_loopbk;
	__be64 reserved22;
	__be32 RJBBR;
	__be32 RCRC;
	__be32 RRUNT;
	__be32 RSHORT;
	__be32 RDROP;
	__be32 RdropOvflw;
	__be32 RdropLength;
	__be32 RTOTFRMS;
	__be32 TDROP;
};
#endif
