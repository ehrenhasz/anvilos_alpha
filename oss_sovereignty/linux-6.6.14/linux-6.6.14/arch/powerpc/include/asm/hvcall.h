#ifndef _ASM_POWERPC_HVCALL_H
#define _ASM_POWERPC_HVCALL_H
#ifdef __KERNEL__
#define HVSC			.long 0x44000022
#define H_SUCCESS	0
#define H_BUSY		1	 
#define H_CLOSED	2	 
#define H_NOT_AVAILABLE 3
#define H_CONSTRAINED	4	 
#define H_PARTIAL       5
#define H_IN_PROGRESS	14	 
#define H_PAGE_REGISTERED 15
#define H_PARTIAL_STORE   16
#define H_PENDING	17	 
#define H_CONTINUE	18	 
#define H_LONG_BUSY_START_RANGE		9900   
#define H_LONG_BUSY_ORDER_1_MSEC	9900   
#define H_LONG_BUSY_ORDER_10_MSEC	9901   
#define H_LONG_BUSY_ORDER_100_MSEC 	9902   
#define H_LONG_BUSY_ORDER_1_SEC		9903   
#define H_LONG_BUSY_ORDER_10_SEC	9904   
#define H_LONG_BUSY_ORDER_100_SEC	9905   
#define H_LONG_BUSY_END_RANGE		9905   
#define H_TOO_HARD	9999
#define H_HARDWARE	-1	 
#define H_FUNCTION	-2	 
#define H_PRIVILEGE	-3	 
#define H_PARAMETER	-4	 
#define H_BAD_MODE	-5	 
#define H_PTEG_FULL	-6	 
#define H_NOT_FOUND	-7	 
#define H_RESERVED_DABR	-8	 
#define H_NO_MEM	-9
#define H_AUTHORITY	-10
#define H_PERMISSION	-11
#define H_DROPPED	-12
#define H_SOURCE_PARM	-13
#define H_DEST_PARM	-14
#define H_REMOTE_PARM	-15
#define H_RESOURCE	-16
#define H_ADAPTER_PARM  -17
#define H_RH_PARM       -18
#define H_RCQ_PARM      -19
#define H_SCQ_PARM      -20
#define H_EQ_PARM       -21
#define H_RT_PARM       -22
#define H_ST_PARM       -23
#define H_SIGT_PARM     -24
#define H_TOKEN_PARM    -25
#define H_MLENGTH_PARM  -27
#define H_MEM_PARM      -28
#define H_MEM_ACCESS_PARM -29
#define H_ATTR_PARM     -30
#define H_PORT_PARM     -31
#define H_MCG_PARM      -32
#define H_VL_PARM       -33
#define H_TSIZE_PARM    -34
#define H_TRACE_PARM    -35
#define H_MASK_PARM     -37
#define H_MCG_FULL      -38
#define H_ALIAS_EXIST   -39
#define H_P_COUNTER     -40
#define H_TABLE_FULL    -41
#define H_ALT_TABLE     -42
#define H_MR_CONDITION  -43
#define H_NOT_ENOUGH_RESOURCES -44
#define H_R_STATE       -45
#define H_RESCINDED     -46
#define H_ABORTED	-54
#define H_P2		-55
#define H_P3		-56
#define H_P4		-57
#define H_P5		-58
#define H_P6		-59
#define H_P7		-60
#define H_P8		-61
#define H_P9		-62
#define H_NOOP		-63
#define H_TOO_BIG	-64
#define H_UNSUPPORTED	-67
#define H_OVERLAP	-68
#define H_INTERRUPT	-69
#define H_BAD_DATA	-70
#define H_NOT_ACTIVE	-71
#define H_SG_LIST	-72
#define H_OP_MODE	-73
#define H_COP_HW	-74
#define H_STATE		-75
#define H_IN_USE	-77
#define H_UNSUPPORTED_FLAG_START	-256
#define H_UNSUPPORTED_FLAG_END		-511
#define H_MULTI_THREADS_ACTIVE	-9005
#define H_OUTSTANDING_COP_OPS	-9006
#define H_IS_LONG_BUSY(x)  ((x >= H_LONG_BUSY_START_RANGE) \
			     && (x <= H_LONG_BUSY_END_RANGE))
#define H_LARGE_PAGE		(1UL<<(63-16))
#define H_EXACT			(1UL<<(63-24))	 
#define H_R_XLATE		(1UL<<(63-25))	 
#define H_READ_4		(1UL<<(63-26))	 
#define H_PAGE_STATE_CHANGE	(1UL<<(63-28))
#define H_PAGE_UNUSED		((1UL<<(63-29)) | (1UL<<(63-30)))
#define H_PAGE_SET_UNUSED	(H_PAGE_STATE_CHANGE | H_PAGE_UNUSED)
#define H_PAGE_SET_LOANED	(H_PAGE_SET_UNUSED | (1UL<<(63-31)))
#define H_PAGE_SET_ACTIVE	H_PAGE_STATE_CHANGE
#define H_AVPN			(1UL<<(63-32))	 
#define H_ANDCOND		(1UL<<(63-33))
#define H_LOCAL			(1UL<<(63-35))
#define H_ICACHE_INVALIDATE	(1UL<<(63-40))	 
#define H_ICACHE_SYNCHRONIZE	(1UL<<(63-41))	 
#define H_COALESCE_CAND	(1UL<<(63-42))	 
#define H_ZERO_PAGE		(1UL<<(63-48))	 
#define H_COPY_PAGE		(1UL<<(63-49))
#define H_N			(1UL<<(63-61))
#define H_PP1			(1UL<<(63-62))
#define H_PP2			(1UL<<(63-63))
#define H_VPA_FUNC_SHIFT	(63-18)	 
#define H_VPA_FUNC_MASK		7UL
#define H_VPA_REG_VPA		1UL	 
#define H_VPA_REG_DTL		2UL	 
#define H_VPA_REG_SLB		3UL	 
#define H_VPA_DEREG_VPA		5UL	 
#define H_VPA_DEREG_DTL		6UL	 
#define H_VPA_DEREG_SLB		7UL	 
#define H_VASI_INVALID          0
#define H_VASI_ENABLED          1
#define H_VASI_ABORTED          2
#define H_VASI_SUSPENDING       3
#define H_VASI_SUSPENDED        4
#define H_VASI_RESUMED          5
#define H_VASI_COMPLETED        6
#define H_VASI_SIGNAL_CANCEL    1
#define H_VASI_SIGNAL_ABORT     2
#define H_VASI_SIGNAL_SUSPEND   3
#define H_VASI_SIGNAL_COMPLETE  4
#define H_VASI_SIGNAL_ENABLE    5
#define H_VASI_SIGNAL_FAILOVER  6
#define H_CB_ALIGNMENT          4096
#define H_REMOVE		0x04
#define H_ENTER			0x08
#define H_READ			0x0c
#define H_CLEAR_MOD		0x10
#define H_CLEAR_REF		0x14
#define H_PROTECT		0x18
#define H_GET_TCE		0x1c
#define H_PUT_TCE		0x20
#define H_SET_SPRG0		0x24
#define H_SET_DABR		0x28
#define H_PAGE_INIT		0x2c
#define H_SET_ASR		0x30
#define H_ASR_ON		0x34
#define H_ASR_OFF		0x38
#define H_LOGICAL_CI_LOAD	0x3c
#define H_LOGICAL_CI_STORE	0x40
#define H_LOGICAL_CACHE_LOAD	0x44
#define H_LOGICAL_CACHE_STORE	0x48
#define H_LOGICAL_ICBI		0x4c
#define H_LOGICAL_DCBF		0x50
#define H_GET_TERM_CHAR		0x54
#define H_PUT_TERM_CHAR		0x58
#define H_REAL_TO_LOGICAL	0x5c
#define H_HYPERVISOR_DATA	0x60
#define H_EOI			0x64
#define H_CPPR			0x68
#define H_IPI			0x6c
#define H_IPOLL			0x70
#define H_XIRR			0x74
#define H_PERFMON		0x7c
#define H_MIGRATE_DMA		0x78
#define H_REGISTER_VPA		0xDC
#define H_CEDE			0xE0
#define H_CONFER		0xE4
#define H_PROD			0xE8
#define H_GET_PPP		0xEC
#define H_SET_PPP		0xF0
#define H_PURR			0xF4
#define H_PIC			0xF8
#define H_REG_CRQ		0xFC
#define H_FREE_CRQ		0x100
#define H_VIO_SIGNAL		0x104
#define H_SEND_CRQ		0x108
#define H_COPY_RDMA		0x110
#define H_REGISTER_LOGICAL_LAN	0x114
#define H_FREE_LOGICAL_LAN	0x118
#define H_ADD_LOGICAL_LAN_BUFFER 0x11C
#define H_SEND_LOGICAL_LAN	0x120
#define H_BULK_REMOVE		0x124
#define H_MULTICAST_CTRL	0x130
#define H_SET_XDABR		0x134
#define H_STUFF_TCE		0x138
#define H_PUT_TCE_INDIRECT	0x13C
#define H_CHANGE_LOGICAL_LAN_MAC 0x14C
#define H_VTERM_PARTNER_INFO	0x150
#define H_REGISTER_VTERM	0x154
#define H_FREE_VTERM		0x158
#define H_RESET_EVENTS          0x15C
#define H_ALLOC_RESOURCE        0x160
#define H_FREE_RESOURCE         0x164
#define H_MODIFY_QP             0x168
#define H_QUERY_QP              0x16C
#define H_REREGISTER_PMR        0x170
#define H_REGISTER_SMR          0x174
#define H_QUERY_MR              0x178
#define H_QUERY_MW              0x17C
#define H_QUERY_HCA             0x180
#define H_QUERY_PORT            0x184
#define H_MODIFY_PORT           0x188
#define H_DEFINE_AQP1           0x18C
#define H_GET_TRACE_BUFFER      0x190
#define H_DEFINE_AQP0           0x194
#define H_RESIZE_MR             0x198
#define H_ATTACH_MCQP           0x19C
#define H_DETACH_MCQP           0x1A0
#define H_CREATE_RPT            0x1A4
#define H_REMOVE_RPT            0x1A8
#define H_REGISTER_RPAGES       0x1AC
#define H_DISABLE_AND_GET       0x1B0
#define H_ERROR_DATA            0x1B4
#define H_GET_HCA_INFO          0x1B8
#define H_GET_PERF_COUNT        0x1BC
#define H_MANAGE_TRACE          0x1C0
#define H_GET_CPU_CHARACTERISTICS 0x1C8
#define H_FREE_LOGICAL_LAN_BUFFER 0x1D4
#define H_QUERY_INT_STATE       0x1E4
#define H_POLL_PENDING		0x1D8
#define H_ILLAN_ATTRIBUTES	0x244
#define H_MODIFY_HEA_QP		0x250
#define H_QUERY_HEA_QP		0x254
#define H_QUERY_HEA		0x258
#define H_QUERY_HEA_PORT	0x25C
#define H_MODIFY_HEA_PORT	0x260
#define H_REG_BCMC		0x264
#define H_DEREG_BCMC		0x268
#define H_REGISTER_HEA_RPAGES	0x26C
#define H_DISABLE_AND_GET_HEA	0x270
#define H_GET_HEA_INFO		0x274
#define H_ALLOC_HEA_RESOURCE	0x278
#define H_ADD_CONN		0x284
#define H_DEL_CONN		0x288
#define H_JOIN			0x298
#define H_VASI_SIGNAL           0x2A0
#define H_VASI_STATE            0x2A4
#define H_VIOCTL		0x2A8
#define H_ENABLE_CRQ		0x2B0
#define H_GET_EM_PARMS		0x2B8
#define H_SET_MPP		0x2D0
#define H_GET_MPP		0x2D4
#define H_REG_SUB_CRQ		0x2DC
#define H_HOME_NODE_ASSOCIATIVITY 0x2EC
#define H_FREE_SUB_CRQ		0x2E0
#define H_SEND_SUB_CRQ		0x2E4
#define H_SEND_SUB_CRQ_INDIRECT	0x2E8
#define H_BEST_ENERGY		0x2F4
#define H_XIRR_X		0x2FC
#define H_RANDOM		0x300
#define H_COP			0x304
#define H_GET_MPP_X		0x314
#define H_SET_MODE		0x31C
#define H_BLOCK_REMOVE		0x328
#define H_CLEAR_HPT		0x358
#define H_REQUEST_VMC		0x360
#define H_RESIZE_HPT_PREPARE	0x36C
#define H_RESIZE_HPT_COMMIT	0x370
#define H_REGISTER_PROC_TBL	0x37C
#define H_SIGNAL_SYS_RESET	0x380
#define H_ALLOCATE_VAS_WINDOW	0x388
#define H_MODIFY_VAS_WINDOW	0x38C
#define H_DEALLOCATE_VAS_WINDOW	0x390
#define H_QUERY_VAS_WINDOW	0x394
#define H_QUERY_VAS_CAPABILITIES	0x398
#define H_QUERY_NX_CAPABILITIES	0x39C
#define H_GET_NX_FAULT		0x3A0
#define H_INT_GET_SOURCE_INFO   0x3A8
#define H_INT_SET_SOURCE_CONFIG 0x3AC
#define H_INT_GET_SOURCE_CONFIG 0x3B0
#define H_INT_GET_QUEUE_INFO    0x3B4
#define H_INT_SET_QUEUE_CONFIG  0x3B8
#define H_INT_GET_QUEUE_CONFIG  0x3BC
#define H_INT_SET_OS_REPORTING_LINE 0x3C0
#define H_INT_GET_OS_REPORTING_LINE 0x3C4
#define H_INT_ESB               0x3C8
#define H_INT_SYNC              0x3CC
#define H_INT_RESET             0x3D0
#define H_SCM_READ_METADATA     0x3E4
#define H_SCM_WRITE_METADATA    0x3E8
#define H_SCM_BIND_MEM          0x3EC
#define H_SCM_UNBIND_MEM        0x3F0
#define H_SCM_QUERY_BLOCK_MEM_BINDING 0x3F4
#define H_SCM_QUERY_LOGICAL_MEM_BINDING 0x3F8
#define H_SCM_UNBIND_ALL        0x3FC
#define H_SCM_HEALTH            0x400
#define H_SCM_PERFORMANCE_STATS 0x418
#define H_PKS_GET_CONFIG	0x41C
#define H_PKS_SET_PASSWORD	0x420
#define H_PKS_GEN_PASSWORD	0x424
#define H_PKS_WRITE_OBJECT	0x42C
#define H_PKS_GEN_KEY		0x430
#define H_PKS_READ_OBJECT	0x434
#define H_PKS_REMOVE_OBJECT	0x438
#define H_PKS_CONFIRM_OBJECT_FLUSHED	0x43C
#define H_RPT_INVALIDATE	0x448
#define H_SCM_FLUSH		0x44C
#define H_GET_ENERGY_SCALE_INFO	0x450
#define H_PKS_SIGNED_UPDATE	0x454
#define H_WATCHDOG		0x45C
#define MAX_HCALL_OPCODE	H_WATCHDOG
#define H_UNBIND_SCOPE_ALL (0x1)
#define H_UNBIND_SCOPE_DRC (0x2)
#define H_GET_VIOA_DUMP_SIZE	0x01
#define H_GET_VIOA_DUMP		0x02
#define H_GET_ILLAN_NUM_VLAN_IDS 0x03
#define H_GET_ILLAN_VLAN_ID_LIST 0x04
#define H_GET_ILLAN_SWITCH_ID	0x05
#define H_DISABLE_MIGRATION	0x06
#define H_ENABLE_MIGRATION	0x07
#define H_GET_PARTNER_INFO	0x08
#define H_GET_PARTNER_WWPN_LIST	0x09
#define H_DISABLE_ALL_VIO_INTS	0x0A
#define H_DISABLE_VIO_INTERRUPT	0x0B
#define H_ENABLE_VIO_INTERRUPT	0x0C
#define H_GET_SESSION_TOKEN	0x19
#define H_SESSION_ERR_DETECTED	0x1A
#define H_RTAS			0xf000
#define H_LOGICAL_MEMOP  0xF001
#define H_CAS            0XF002
#define H_UPDATE_DT      0XF003
#define H_GET_24X7_CATALOG_PAGE	0xF078
#define H_GET_24X7_DATA		0xF07C
#define H_GET_PERF_COUNTER_INFO	0xF080
#define H_SET_PARTITION_TABLE	0xF800
#define H_ENTER_NESTED		0xF804
#define H_TLB_INVALIDATE	0xF808
#define H_COPY_TOFROM_GUEST	0xF80C
#define H_PAGE_IN_SHARED        0x1
#define H_SVM_PAGE_IN		0xEF00
#define H_SVM_PAGE_OUT		0xEF04
#define H_SVM_INIT_START	0xEF08
#define H_SVM_INIT_DONE		0xEF0C
#define H_SVM_INIT_ABORT	0xEF14
#define H_SET_MODE_RESOURCE_SET_CIABR		1
#define H_SET_MODE_RESOURCE_SET_DAWR0		2
#define H_SET_MODE_RESOURCE_ADDR_TRANS_MODE	3
#define H_SET_MODE_RESOURCE_LE			4
#define H_SET_MODE_RESOURCE_SET_DAWR1		5
#define H_SIGNAL_SYS_RESET_ALL			-1
#define H_SIGNAL_SYS_RESET_ALL_OTHERS		-2
#define H_CPU_CHAR_SPEC_BAR_ORI31	(1ull << 63)  
#define H_CPU_CHAR_BCCTRL_SERIALISED	(1ull << 62)  
#define H_CPU_CHAR_L1D_FLUSH_ORI30	(1ull << 61)  
#define H_CPU_CHAR_L1D_FLUSH_TRIG2	(1ull << 60)  
#define H_CPU_CHAR_L1D_THREAD_PRIV	(1ull << 59)  
#define H_CPU_CHAR_BRANCH_HINTS_HONORED	(1ull << 58)  
#define H_CPU_CHAR_THREAD_RECONFIG_CTRL	(1ull << 57)  
#define H_CPU_CHAR_COUNT_CACHE_DISABLED	(1ull << 56)  
#define H_CPU_CHAR_BCCTR_FLUSH_ASSIST	(1ull << 54)  
#define H_CPU_CHAR_BCCTR_LINK_FLUSH_ASSIST (1ull << 52)  
#define H_CPU_BEHAV_FAVOUR_SECURITY	(1ull << 63)  
#define H_CPU_BEHAV_L1D_FLUSH_PR	(1ull << 62)  
#define H_CPU_BEHAV_BNDS_CHK_SPEC_BAR	(1ull << 61)  
#define H_CPU_BEHAV_FAVOUR_SECURITY_H	(1ull << 60)  
#define H_CPU_BEHAV_FLUSH_COUNT_CACHE	(1ull << 58)  
#define H_CPU_BEHAV_FLUSH_LINK_STACK	(1ull << 57)  
#define H_CPU_BEHAV_NO_L1D_FLUSH_ENTRY	(1ull << 56)  
#define H_CPU_BEHAV_NO_L1D_FLUSH_UACCESS (1ull << 55)  
#define H_CPU_BEHAV_NO_STF_BARRIER	(1ull << 54)  
#define PROC_TABLE_OP_MASK	0x18
#define PROC_TABLE_DEREG	0x10
#define PROC_TABLE_NEW		0x18
#define PROC_TABLE_TYPE_MASK	0x06
#define PROC_TABLE_HPT_SLB	0x00
#define PROC_TABLE_HPT_PT	0x02
#define PROC_TABLE_RADIX	0x04
#define PROC_TABLE_GTSE		0x01
#define H_RPTI_TYPE_NESTED	0x0001	 
#define H_RPTI_TYPE_TLB		0x0002	 
#define H_RPTI_TYPE_PWC		0x0004	 
#define H_RPTI_TYPE_PRT		0x0008
#define H_RPTI_TYPE_PAT		0x0008
#define H_RPTI_TYPE_ALL		(H_RPTI_TYPE_TLB | H_RPTI_TYPE_PWC | \
				 H_RPTI_TYPE_PRT)
#define H_RPTI_TYPE_NESTED_ALL	(H_RPTI_TYPE_TLB | H_RPTI_TYPE_PWC | \
				 H_RPTI_TYPE_PAT)
#define H_RPTI_TARGET_CMMU		0x01  
#define H_RPTI_TARGET_CMMU_LOCAL	0x02  
#define H_RPTI_TARGET_NMMU		0x04
#define H_RPTI_PAGE_4K	0x01
#define H_RPTI_PAGE_64K	0x02
#define H_RPTI_PAGE_2M	0x04
#define H_RPTI_PAGE_1G	0x08
#define H_RPTI_PAGE_ALL (-1UL)
#ifndef __ASSEMBLY__
#include <linux/types.h>
long plpar_hcall_norets(unsigned long opcode, ...);
long plpar_hcall_norets_notrace(unsigned long opcode, ...);
#define PLPAR_HCALL_BUFSIZE 4
long plpar_hcall(unsigned long opcode, unsigned long *retbuf, ...);
long plpar_hcall_raw(unsigned long opcode, unsigned long *retbuf, ...);
#define PLPAR_HCALL9_BUFSIZE 9
long plpar_hcall9(unsigned long opcode, unsigned long *retbuf, ...);
long plpar_hcall9_raw(unsigned long opcode, unsigned long *retbuf, ...);
extern struct static_key hcall_tracepoint_key;
void __trace_hcall_entry(unsigned long opcode, unsigned long *args);
void __trace_hcall_exit(long opcode, long retval, unsigned long *retbuf);
struct hvcall_mpp_data {
	unsigned long entitled_mem;
	unsigned long mapped_mem;
	unsigned short group_num;
	unsigned short pool_num;
	unsigned char mem_weight;
	unsigned char unallocated_mem_weight;
	unsigned long unallocated_entitlement;   
	unsigned long pool_size;
	signed long loan_request;
	unsigned long backing_mem;
};
int h_get_mpp(struct hvcall_mpp_data *);
struct hvcall_mpp_x_data {
	unsigned long coalesced_bytes;
	unsigned long pool_coalesced_bytes;
	unsigned long pool_purr_cycles;
	unsigned long pool_spurr_cycles;
	unsigned long reserved[3];
};
int h_get_mpp_x(struct hvcall_mpp_x_data *mpp_x_data);
static inline unsigned int get_longbusy_msecs(int longbusy_rc)
{
	switch (longbusy_rc) {
	case H_LONG_BUSY_ORDER_1_MSEC:
		return 1;
	case H_LONG_BUSY_ORDER_10_MSEC:
		return 10;
	case H_LONG_BUSY_ORDER_100_MSEC:
		return 100;
	case H_LONG_BUSY_ORDER_1_SEC:
		return 1000;
	case H_LONG_BUSY_ORDER_10_SEC:
		return 10000;
	case H_LONG_BUSY_ORDER_100_SEC:
		return 100000;
	default:
		return 1;
	}
}
struct h_cpu_char_result {
	u64 character;
	u64 behaviour;
};
struct hv_guest_state {
	u64 version;		 
	u32 lpid;
	u32 vcpu_token;
	u64 lpcr;
	u64 pcr;
	u64 amor;
	u64 dpdes;
	u64 hfscr;
	s64 tb_offset;
	u64 dawr0;
	u64 dawrx0;
	u64 ciabr;
	u64 hdec_expiry;
	u64 purr;
	u64 spurr;
	u64 ic;
	u64 vtb;
	u64 hdar;
	u64 hdsisr;
	u64 heir;
	u64 asdr;
	u64 srr0;
	u64 srr1;
	u64 sprg[4];
	u64 pidr;
	u64 cfar;
	u64 ppr;
	u64 dawr1;
	u64 dawrx1;
};
#define HV_GUEST_STATE_VERSION	2
static inline int hv_guest_state_size(unsigned int version)
{
	switch (version) {
	case 1:
		return offsetofend(struct hv_guest_state, ppr);
	case 2:
		return offsetofend(struct hv_guest_state, dawrx1);
	default:
		return -1;
	}
}
struct hv_get_perf_counter_info_params {
	__be32 counter_request;  
	__be32 starting_index;   
	__be16 secondary_index;  
	__be16 returned_values;  
	__be32 detail_rc;  
	__be16 cv_element_size;
	__u8 counter_info_version_in;
	__u8 counter_info_version_out;
	__u8 reserved[0xC];
	__u8 counter_value[];
} __packed;
#define HGPCI_REQ_BUFFER_SIZE	4096
#define HGPCI_MAX_DATA_BYTES \
	(HGPCI_REQ_BUFFER_SIZE - sizeof(struct hv_get_perf_counter_info_params))
struct hv_gpci_request_buffer {
	struct hv_get_perf_counter_info_params params;
	uint8_t bytes[HGPCI_MAX_DATA_BYTES];
} __packed;
#endif  
#endif  
#endif  
