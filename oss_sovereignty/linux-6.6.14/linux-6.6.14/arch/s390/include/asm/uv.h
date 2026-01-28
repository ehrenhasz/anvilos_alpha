#ifndef _ASM_S390_UV_H
#define _ASM_S390_UV_H
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/bug.h>
#include <linux/sched.h>
#include <asm/page.h>
#include <asm/gmap.h>
#define UVC_CC_OK	0
#define UVC_CC_ERROR	1
#define UVC_CC_BUSY	2
#define UVC_CC_PARTIAL	3
#define UVC_RC_EXECUTED		0x0001
#define UVC_RC_INV_CMD		0x0002
#define UVC_RC_INV_STATE	0x0003
#define UVC_RC_INV_LEN		0x0005
#define UVC_RC_NO_RESUME	0x0007
#define UVC_RC_NEED_DESTROY	0x8000
#define UVC_CMD_QUI			0x0001
#define UVC_CMD_INIT_UV			0x000f
#define UVC_CMD_CREATE_SEC_CONF		0x0100
#define UVC_CMD_DESTROY_SEC_CONF	0x0101
#define UVC_CMD_DESTROY_SEC_CONF_FAST	0x0102
#define UVC_CMD_CREATE_SEC_CPU		0x0120
#define UVC_CMD_DESTROY_SEC_CPU		0x0121
#define UVC_CMD_CONV_TO_SEC_STOR	0x0200
#define UVC_CMD_CONV_FROM_SEC_STOR	0x0201
#define UVC_CMD_DESTR_SEC_STOR		0x0202
#define UVC_CMD_SET_SEC_CONF_PARAMS	0x0300
#define UVC_CMD_UNPACK_IMG		0x0301
#define UVC_CMD_VERIFY_IMG		0x0302
#define UVC_CMD_CPU_RESET		0x0310
#define UVC_CMD_CPU_RESET_INITIAL	0x0311
#define UVC_CMD_PREPARE_RESET		0x0320
#define UVC_CMD_CPU_RESET_CLEAR		0x0321
#define UVC_CMD_CPU_SET_STATE		0x0330
#define UVC_CMD_SET_UNSHARE_ALL		0x0340
#define UVC_CMD_PIN_PAGE_SHARED		0x0341
#define UVC_CMD_UNPIN_PAGE_SHARED	0x0342
#define UVC_CMD_DUMP_INIT		0x0400
#define UVC_CMD_DUMP_CONF_STOR_STATE	0x0401
#define UVC_CMD_DUMP_CPU		0x0402
#define UVC_CMD_DUMP_COMPLETE		0x0403
#define UVC_CMD_SET_SHARED_ACCESS	0x1000
#define UVC_CMD_REMOVE_SHARED_ACCESS	0x1001
#define UVC_CMD_RETR_ATTEST		0x1020
#define UVC_CMD_ADD_SECRET		0x1031
#define UVC_CMD_LIST_SECRETS		0x1033
#define UVC_CMD_LOCK_SECRETS		0x1034
enum uv_cmds_inst {
	BIT_UVC_CMD_QUI = 0,
	BIT_UVC_CMD_INIT_UV = 1,
	BIT_UVC_CMD_CREATE_SEC_CONF = 2,
	BIT_UVC_CMD_DESTROY_SEC_CONF = 3,
	BIT_UVC_CMD_CREATE_SEC_CPU = 4,
	BIT_UVC_CMD_DESTROY_SEC_CPU = 5,
	BIT_UVC_CMD_CONV_TO_SEC_STOR = 6,
	BIT_UVC_CMD_CONV_FROM_SEC_STOR = 7,
	BIT_UVC_CMD_SET_SHARED_ACCESS = 8,
	BIT_UVC_CMD_REMOVE_SHARED_ACCESS = 9,
	BIT_UVC_CMD_SET_SEC_PARMS = 11,
	BIT_UVC_CMD_UNPACK_IMG = 13,
	BIT_UVC_CMD_VERIFY_IMG = 14,
	BIT_UVC_CMD_CPU_RESET = 15,
	BIT_UVC_CMD_CPU_RESET_INITIAL = 16,
	BIT_UVC_CMD_CPU_SET_STATE = 17,
	BIT_UVC_CMD_PREPARE_RESET = 18,
	BIT_UVC_CMD_CPU_PERFORM_CLEAR_RESET = 19,
	BIT_UVC_CMD_UNSHARE_ALL = 20,
	BIT_UVC_CMD_PIN_PAGE_SHARED = 21,
	BIT_UVC_CMD_UNPIN_PAGE_SHARED = 22,
	BIT_UVC_CMD_DESTROY_SEC_CONF_FAST = 23,
	BIT_UVC_CMD_DUMP_INIT = 24,
	BIT_UVC_CMD_DUMP_CONFIG_STOR_STATE = 25,
	BIT_UVC_CMD_DUMP_CPU = 26,
	BIT_UVC_CMD_DUMP_COMPLETE = 27,
	BIT_UVC_CMD_RETR_ATTEST = 28,
	BIT_UVC_CMD_ADD_SECRET = 29,
	BIT_UVC_CMD_LIST_SECRETS = 30,
	BIT_UVC_CMD_LOCK_SECRETS = 31,
};
enum uv_feat_ind {
	BIT_UV_FEAT_MISC = 0,
	BIT_UV_FEAT_AIV = 1,
	BIT_UV_FEAT_AP = 4,
	BIT_UV_FEAT_AP_INTR = 5,
};
struct uv_cb_header {
	u16 len;
	u16 cmd;	 
	u16 rc;		 
	u16 rrc;	 
} __packed __aligned(8);
struct uv_cb_qui {
	struct uv_cb_header header;		 
	u64 reserved08;				 
	u64 inst_calls_list[4];			 
	u64 reserved30[2];			 
	u64 uv_base_stor_len;			 
	u64 reserved48;				 
	u64 conf_base_phys_stor_len;		 
	u64 conf_base_virt_stor_len;		 
	u64 conf_virt_var_stor_len;		 
	u64 cpu_stor_len;			 
	u32 reserved70[3];			 
	u32 max_num_sec_conf;			 
	u64 max_guest_stor_addr;		 
	u8  reserved88[0x9e - 0x88];		 
	u16 max_guest_cpu_id;			 
	u64 uv_feature_indications;		 
	u64 reserveda8;				 
	u64 supp_se_hdr_versions;		 
	u64 supp_se_hdr_pcf;			 
	u64 reservedc0;				 
	u64 conf_dump_storage_state_len;	 
	u64 conf_dump_finalize_len;		 
	u64 reservedd8;				 
	u64 supp_att_req_hdr_ver;		 
	u64 supp_att_pflags;			 
	u64 reservedf0;				 
	u64 supp_add_secret_req_ver;		 
	u64 supp_add_secret_pcf;		 
	u64 supp_secret_types;			 
	u16 max_secrets;			 
	u8 reserved112[0x120 - 0x112];		 
} __packed __aligned(8);
struct uv_cb_init {
	struct uv_cb_header header;
	u64 reserved08[2];
	u64 stor_origin;
	u64 stor_len;
	u64 reserved28[4];
} __packed __aligned(8);
struct uv_cb_cgc {
	struct uv_cb_header header;
	u64 reserved08[2];
	u64 guest_handle;
	u64 conf_base_stor_origin;
	u64 conf_virt_stor_origin;
	u8  reserved30[6];
	union {
		struct {
			u16 : 14;
			u16 ap_instr_intr : 1;
			u16 ap_allow_instr : 1;
		};
		u16 raw;
	} flags;
	u64 guest_stor_origin;
	u64 guest_stor_len;
	u64 guest_sca;
	u64 guest_asce;
	u64 reserved58[5];
} __packed __aligned(8);
struct uv_cb_csc {
	struct uv_cb_header header;
	u64 reserved08[2];
	u64 cpu_handle;
	u64 guest_handle;
	u64 stor_origin;
	u8  reserved30[6];
	u16 num;
	u64 state_origin;
	u64 reserved40[4];
} __packed __aligned(8);
struct uv_cb_cts {
	struct uv_cb_header header;
	u64 reserved08[2];
	u64 guest_handle;
	u64 gaddr;
} __packed __aligned(8);
struct uv_cb_cfs {
	struct uv_cb_header header;
	u64 reserved08[2];
	u64 paddr;
} __packed __aligned(8);
struct uv_cb_ssc {
	struct uv_cb_header header;
	u64 reserved08[2];
	u64 guest_handle;
	u64 sec_header_origin;
	u32 sec_header_len;
	u32 reserved2c;
	u64 reserved30[4];
} __packed __aligned(8);
struct uv_cb_unp {
	struct uv_cb_header header;
	u64 reserved08[2];
	u64 guest_handle;
	u64 gaddr;
	u64 tweak[2];
	u64 reserved38[3];
} __packed __aligned(8);
#define PV_CPU_STATE_OPR	1
#define PV_CPU_STATE_STP	2
#define PV_CPU_STATE_CHKSTP	3
#define PV_CPU_STATE_OPR_LOAD	5
struct uv_cb_cpu_set_state {
	struct uv_cb_header header;
	u64 reserved08[2];
	u64 cpu_handle;
	u8  reserved20[7];
	u8  state;
	u64 reserved28[5];
};
struct uv_cb_nodata {
	struct uv_cb_header header;
	u64 reserved08[2];
	u64 handle;
	u64 reserved20[4];
} __packed __aligned(8);
struct uv_cb_destroy_fast {
	struct uv_cb_header header;
	u64 reserved08[2];
	u64 handle;
	u64 reserved20[5];
} __packed __aligned(8);
struct uv_cb_share {
	struct uv_cb_header header;
	u64 reserved08[3];
	u64 paddr;
	u64 reserved28;
} __packed __aligned(8);
struct uv_cb_attest {
	struct uv_cb_header header;	 
	u64 reserved08[2];		 
	u64 arcb_addr;			 
	u64 cont_token;			 
	u8  reserved28[6];		 
	u16 user_data_len;		 
	u8  user_data[256];		 
	u32 reserved130[3];		 
	u32 meas_len;			 
	u64 meas_addr;			 
	u8  config_uid[16];		 
	u32 reserved158;		 
	u32 add_data_len;		 
	u64 add_data_addr;		 
	u64 reserved168[4];		 
} __packed __aligned(8);
struct uv_cb_dump_cpu {
	struct uv_cb_header header;
	u64 reserved08[2];
	u64 cpu_handle;
	u64 dump_area_origin;
	u64 reserved28[5];
} __packed __aligned(8);
struct uv_cb_dump_stor_state {
	struct uv_cb_header header;
	u64 reserved08[2];
	u64 config_handle;
	u64 dump_area_origin;
	u64 gaddr;
	u64 reserved28[4];
} __packed __aligned(8);
struct uv_cb_dump_complete {
	struct uv_cb_header header;
	u64 reserved08[2];
	u64 config_handle;
	u64 dump_area_origin;
	u64 reserved30[5];
} __packed __aligned(8);
struct uv_cb_guest_addr {
	struct uv_cb_header header;
	u64 reserved08[3];
	u64 addr;
	u64 reserved28[4];
} __packed __aligned(8);
static inline int __uv_call(unsigned long r1, unsigned long r2)
{
	int cc;
	asm volatile(
		"	.insn rrf,0xB9A40000,%[r1],%[r2],0,0\n"
		"	ipm	%[cc]\n"
		"	srl	%[cc],28\n"
		: [cc] "=d" (cc)
		: [r1] "a" (r1), [r2] "a" (r2)
		: "memory", "cc");
	return cc;
}
static inline int uv_call(unsigned long r1, unsigned long r2)
{
	int cc;
	do {
		cc = __uv_call(r1, r2);
	} while (cc > 1);
	return cc;
}
static inline int uv_call_sched(unsigned long r1, unsigned long r2)
{
	int cc;
	do {
		cc = __uv_call(r1, r2);
		cond_resched();
	} while (cc > 1);
	return cc;
}
static inline int uv_cmd_nodata(u64 handle, u16 cmd, u16 *rc, u16 *rrc)
{
	struct uv_cb_nodata uvcb = {
		.header.cmd = cmd,
		.header.len = sizeof(uvcb),
		.handle = handle,
	};
	int cc;
	WARN(!handle, "No handle provided to Ultravisor call cmd %x\n", cmd);
	cc = uv_call_sched(0, (u64)&uvcb);
	*rc = uvcb.header.rc;
	*rrc = uvcb.header.rrc;
	return cc ? -EINVAL : 0;
}
struct uv_info {
	unsigned long inst_calls_list[4];
	unsigned long uv_base_stor_len;
	unsigned long guest_base_stor_len;
	unsigned long guest_virt_base_stor_len;
	unsigned long guest_virt_var_stor_len;
	unsigned long guest_cpu_stor_len;
	unsigned long max_sec_stor_addr;
	unsigned int max_num_sec_conf;
	unsigned short max_guest_cpu_id;
	unsigned long uv_feature_indications;
	unsigned long supp_se_hdr_ver;
	unsigned long supp_se_hdr_pcf;
	unsigned long conf_dump_storage_state_len;
	unsigned long conf_dump_finalize_len;
	unsigned long supp_att_req_hdr_ver;
	unsigned long supp_att_pflags;
	unsigned long supp_add_secret_req_ver;
	unsigned long supp_add_secret_pcf;
	unsigned long supp_secret_types;
	unsigned short max_secrets;
};
extern struct uv_info uv_info;
static inline bool uv_has_feature(u8 feature_bit)
{
	if (feature_bit >= sizeof(uv_info.uv_feature_indications) * 8)
		return false;
	return test_bit_inv(feature_bit, &uv_info.uv_feature_indications);
}
#ifdef CONFIG_PROTECTED_VIRTUALIZATION_GUEST
extern int prot_virt_guest;
static inline int is_prot_virt_guest(void)
{
	return prot_virt_guest;
}
static inline int share(unsigned long addr, u16 cmd)
{
	struct uv_cb_share uvcb = {
		.header.cmd = cmd,
		.header.len = sizeof(uvcb),
		.paddr = addr
	};
	if (!is_prot_virt_guest())
		return -EOPNOTSUPP;
	BUG_ON(addr & ~PAGE_MASK);
	if (!uv_call(0, (u64)&uvcb))
		return 0;
	return -EINVAL;
}
static inline int uv_set_shared(unsigned long addr)
{
	return share(addr, UVC_CMD_SET_SHARED_ACCESS);
}
static inline int uv_remove_shared(unsigned long addr)
{
	return share(addr, UVC_CMD_REMOVE_SHARED_ACCESS);
}
#else
#define is_prot_virt_guest() 0
static inline int uv_set_shared(unsigned long addr) { return 0; }
static inline int uv_remove_shared(unsigned long addr) { return 0; }
#endif
#if IS_ENABLED(CONFIG_KVM)
extern int prot_virt_host;
static inline int is_prot_virt_host(void)
{
	return prot_virt_host;
}
int uv_pin_shared(unsigned long paddr);
int gmap_make_secure(struct gmap *gmap, unsigned long gaddr, void *uvcb);
int gmap_destroy_page(struct gmap *gmap, unsigned long gaddr);
int uv_destroy_owned_page(unsigned long paddr);
int uv_convert_from_secure(unsigned long paddr);
int uv_convert_owned_from_secure(unsigned long paddr);
int gmap_convert_to_secure(struct gmap *gmap, unsigned long gaddr);
void setup_uv(void);
#else
#define is_prot_virt_host() 0
static inline void setup_uv(void) {}
static inline int uv_pin_shared(unsigned long paddr)
{
	return 0;
}
static inline int uv_destroy_owned_page(unsigned long paddr)
{
	return 0;
}
static inline int uv_convert_from_secure(unsigned long paddr)
{
	return 0;
}
static inline int uv_convert_owned_from_secure(unsigned long paddr)
{
	return 0;
}
#endif
#endif  
