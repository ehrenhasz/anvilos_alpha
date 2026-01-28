

#ifndef __T4_VALUES_H__
#define __T4_VALUES_H__






#define RXPKTCPLMODE_SPLIT_X		1

#define INGPCIEBOUNDARY_SHIFT_X		5
#define INGPCIEBOUNDARY_32B_X		0

#define INGPADBOUNDARY_SHIFT_X		5

#define T6_INGPADBOUNDARY_SHIFT_X	3
#define T6_INGPADBOUNDARY_8B_X		0
#define T6_INGPADBOUNDARY_32B_X		2

#define INGPADBOUNDARY_32B_X		0


#define INGPACKBOUNDARY_SHIFT_X		5
#define INGPACKBOUNDARY_16B_X		0
#define INGPACKBOUNDARY_64B_X		1


#define SGE_TIMERREGS			6
#define TIMERREG_COUNTER0_X		0

#define FETCHBURSTMIN_64B_X		2
#define FETCHBURSTMIN_128B_X		3


#define FETCHBURSTMIN_64B_T6_X		0
#define FETCHBURSTMIN_128B_T6_X		1

#define FETCHBURSTMAX_256B_X		2
#define FETCHBURSTMAX_512B_X		3

#define HOSTFCMODE_INGRESS_QUEUE_X	1
#define HOSTFCMODE_STATUS_PAGE_X	2

#define CIDXFLUSHTHRESH_32_X		5
#define CIDXFLUSHTHRESH_128_X		7

#define UPDATEDELIVERY_INTERRUPT_X	1

#define RSPD_TYPE_FLBUF_X		0
#define RSPD_TYPE_CPL_X			1
#define RSPD_TYPE_INTR_X		2


#define CONMCTXT_CNGTPMODE_S		19
#define CONMCTXT_CNGTPMODE_V(x)		((x) << CONMCTXT_CNGTPMODE_S)
#define CONMCTXT_CNGCHMAP_S		0
#define CONMCTXT_CNGCHMAP_V(x)		((x) << CONMCTXT_CNGCHMAP_S)
#define CONMCTXT_CNGTPMODE_CHANNEL_X	2
#define CONMCTXT_CNGTPMODE_QUEUE_X	1


#define SGE_UDB_SIZE		128
#define SGE_UDB_KDOORBELL	8
#define SGE_UDB_GTS		20
#define SGE_UDB_WCDOORBELL	64


#define X_MBOWNER_FW			1
#define X_MBOWNER_PL			2


#define WINDOW_SHIFT_X		10
#define PCIEOFST_SHIFT_X	10


#define FT_FCOE_W                       1
#define FT_PORT_W                       3
#define FT_VNIC_ID_W                    17
#define FT_VLAN_W                       17
#define FT_TOS_W                        8
#define FT_PROTOCOL_W                   8
#define FT_ETHERTYPE_W                  16
#define FT_MACMATCH_W                   9
#define FT_MPSHITTYPE_W                 3
#define FT_FRAGMENTATION_W              1


#define FT_VLAN_VLD_S                   16
#define FT_VLAN_VLD_V(x)                ((x) << FT_VLAN_VLD_S)
#define FT_VLAN_VLD_F                   FT_VLAN_VLD_V(1U)

#define FT_VNID_ID_VF_S                 0
#define FT_VNID_ID_VF_V(x)              ((x) << FT_VNID_ID_VF_S)

#define FT_VNID_ID_PF_S                 7
#define FT_VNID_ID_PF_V(x)              ((x) << FT_VNID_ID_PF_S)

#define FT_VNID_ID_VLD_S                16
#define FT_VNID_ID_VLD_V(x)             ((x) << FT_VNID_ID_VLD_S)

#endif 
