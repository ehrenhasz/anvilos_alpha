#ifndef _ICE_DDP_H_
#define _ICE_DDP_H_
#include "ice_type.h"
#define ICE_PKG_SUPP_VER_MAJ 1
#define ICE_PKG_SUPP_VER_MNR 3
#define ICE_PKG_FMT_VER_MAJ 1
#define ICE_PKG_FMT_VER_MNR 0
#define ICE_PKG_FMT_VER_UPD 0
#define ICE_PKG_FMT_VER_DFT 0
#define ICE_PKG_CNT 4
#define ICE_FV_OFFSET_INVAL 0x1FF
struct ice_fv_word {
	u8 prot_id;
	u16 off;  
	u8 resvrd;
} __packed;
#define ICE_MAX_NUM_PROFILES 256
#define ICE_MAX_FV_WORDS 48
struct ice_fv {
	struct ice_fv_word ew[ICE_MAX_FV_WORDS];
};
enum ice_ddp_state {
	ICE_DDP_PKG_SUCCESS = 0,
	ICE_DDP_PKG_ALREADY_LOADED = -1,
	ICE_DDP_PKG_SAME_VERSION_ALREADY_LOADED = -2,
	ICE_DDP_PKG_ALREADY_LOADED_NOT_SUPPORTED = -3,
	ICE_DDP_PKG_COMPATIBLE_ALREADY_LOADED = -4,
	ICE_DDP_PKG_FW_MISMATCH = -5,
	ICE_DDP_PKG_INVALID_FILE = -6,
	ICE_DDP_PKG_FILE_VERSION_TOO_HIGH = -7,
	ICE_DDP_PKG_FILE_VERSION_TOO_LOW = -8,
	ICE_DDP_PKG_FILE_SIGNATURE_INVALID = -9,
	ICE_DDP_PKG_FILE_REVISION_TOO_LOW = -10,
	ICE_DDP_PKG_LOAD_ERROR = -11,
	ICE_DDP_PKG_ERR = -12
};
struct ice_pkg_hdr {
	struct ice_pkg_ver pkg_format_ver;
	__le32 seg_count;
	__le32 seg_offset[];
};
struct ice_generic_seg_hdr {
#define SEGMENT_TYPE_METADATA 0x00000001
#define SEGMENT_TYPE_ICE 0x00000010
	__le32 seg_type;
	struct ice_pkg_ver seg_format_ver;
	__le32 seg_size;
	char seg_id[ICE_PKG_NAME_SIZE];
};
union ice_device_id {
	struct {
		__le16 device_id;
		__le16 vendor_id;
	} dev_vend_id;
	__le32 id;
};
struct ice_device_id_entry {
	union ice_device_id device;
	union ice_device_id sub_device;
};
struct ice_seg {
	struct ice_generic_seg_hdr hdr;
	__le32 device_table_count;
	struct ice_device_id_entry device_table[];
};
struct ice_nvm_table {
	__le32 table_count;
	__le32 vers[];
};
struct ice_buf {
#define ICE_PKG_BUF_SIZE 4096
	u8 buf[ICE_PKG_BUF_SIZE];
};
struct ice_buf_table {
	__le32 buf_count;
	struct ice_buf buf_array[];
};
struct ice_run_time_cfg_seg {
	struct ice_generic_seg_hdr hdr;
	u8 rsvd[8];
	struct ice_buf_table buf_table;
};
struct ice_global_metadata_seg {
	struct ice_generic_seg_hdr hdr;
	struct ice_pkg_ver pkg_ver;
	__le32 rsvd;
	char pkg_name[ICE_PKG_NAME_SIZE];
};
#define ICE_MIN_S_OFF 12
#define ICE_MAX_S_OFF 4095
#define ICE_MIN_S_SZ 1
#define ICE_MAX_S_SZ 4084
struct ice_section_entry {
	__le32 type;
	__le16 offset;
	__le16 size;
};
#define ICE_MIN_S_COUNT 1
#define ICE_MAX_S_COUNT 511
#define ICE_MIN_S_DATA_END 12
#define ICE_MAX_S_DATA_END 4096
#define ICE_METADATA_BUF 0x80000000
struct ice_buf_hdr {
	__le16 section_count;
	__le16 data_end;
	struct ice_section_entry section_entry[];
};
#define ICE_MAX_ENTRIES_IN_BUF(hd_sz, ent_sz)                                 \
	((ICE_PKG_BUF_SIZE -                                                  \
	  struct_size_t(struct ice_buf_hdr,  section_entry, 1) - (hd_sz)) / \
	 (ent_sz))
#define ICE_SID_METADATA 1
#define ICE_SID_XLT0_SW 10
#define ICE_SID_XLT_KEY_BUILDER_SW 11
#define ICE_SID_XLT1_SW 12
#define ICE_SID_XLT2_SW 13
#define ICE_SID_PROFID_TCAM_SW 14
#define ICE_SID_PROFID_REDIR_SW 15
#define ICE_SID_FLD_VEC_SW 16
#define ICE_SID_CDID_KEY_BUILDER_SW 17
struct ice_meta_sect {
	struct ice_pkg_ver ver;
#define ICE_META_SECT_NAME_SIZE 28
	char name[ICE_META_SECT_NAME_SIZE];
	__le32 track_id;
};
#define ICE_SID_CDID_REDIR_SW 18
#define ICE_SID_XLT0_ACL 20
#define ICE_SID_XLT_KEY_BUILDER_ACL 21
#define ICE_SID_XLT1_ACL 22
#define ICE_SID_XLT2_ACL 23
#define ICE_SID_PROFID_TCAM_ACL 24
#define ICE_SID_PROFID_REDIR_ACL 25
#define ICE_SID_FLD_VEC_ACL 26
#define ICE_SID_CDID_KEY_BUILDER_ACL 27
#define ICE_SID_CDID_REDIR_ACL 28
#define ICE_SID_XLT0_FD 30
#define ICE_SID_XLT_KEY_BUILDER_FD 31
#define ICE_SID_XLT1_FD 32
#define ICE_SID_XLT2_FD 33
#define ICE_SID_PROFID_TCAM_FD 34
#define ICE_SID_PROFID_REDIR_FD 35
#define ICE_SID_FLD_VEC_FD 36
#define ICE_SID_CDID_KEY_BUILDER_FD 37
#define ICE_SID_CDID_REDIR_FD 38
#define ICE_SID_XLT0_RSS 40
#define ICE_SID_XLT_KEY_BUILDER_RSS 41
#define ICE_SID_XLT1_RSS 42
#define ICE_SID_XLT2_RSS 43
#define ICE_SID_PROFID_TCAM_RSS 44
#define ICE_SID_PROFID_REDIR_RSS 45
#define ICE_SID_FLD_VEC_RSS 46
#define ICE_SID_CDID_KEY_BUILDER_RSS 47
#define ICE_SID_CDID_REDIR_RSS 48
#define ICE_SID_RXPARSER_MARKER_PTYPE 55
#define ICE_SID_RXPARSER_BOOST_TCAM 56
#define ICE_SID_RXPARSER_METADATA_INIT 58
#define ICE_SID_TXPARSER_BOOST_TCAM 66
#define ICE_SID_XLT0_PE 80
#define ICE_SID_XLT_KEY_BUILDER_PE 81
#define ICE_SID_XLT1_PE 82
#define ICE_SID_XLT2_PE 83
#define ICE_SID_PROFID_TCAM_PE 84
#define ICE_SID_PROFID_REDIR_PE 85
#define ICE_SID_FLD_VEC_PE 86
#define ICE_SID_CDID_KEY_BUILDER_PE 87
#define ICE_SID_CDID_REDIR_PE 88
#define ICE_SID_LBL_FIRST 0x80000010
#define ICE_SID_LBL_RXPARSER_TMEM 0x80000018
#define ICE_SID_LBL_LAST 0x80000038
#define ICE_SID_TX_5_LAYER_TOPO 0x10
enum ice_block {
	ICE_BLK_SW = 0,
	ICE_BLK_ACL,
	ICE_BLK_FD,
	ICE_BLK_RSS,
	ICE_BLK_PE,
	ICE_BLK_COUNT
};
enum ice_sect {
	ICE_XLT0 = 0,
	ICE_XLT_KB,
	ICE_XLT1,
	ICE_XLT2,
	ICE_PROF_TCAM,
	ICE_PROF_REDIR,
	ICE_VEC_TBL,
	ICE_CDID_KB,
	ICE_CDID_REDIR,
	ICE_SECT_COUNT
};
struct ice_label {
	__le16 value;
#define ICE_PKG_LABEL_SIZE 64
	char name[ICE_PKG_LABEL_SIZE];
};
struct ice_label_section {
	__le16 count;
	struct ice_label label[];
};
#define ICE_MAX_LABELS_IN_BUF                                             \
	ICE_MAX_ENTRIES_IN_BUF(struct_size_t(struct ice_label_section,  \
					   label, 1) -                    \
				       sizeof(struct ice_label),          \
			       sizeof(struct ice_label))
struct ice_sw_fv_section {
	__le16 count;
	__le16 base_offset;
	struct ice_fv fv[];
};
struct ice_sw_fv_list_entry {
	struct list_head list_entry;
	u32 profile_id;
	struct ice_fv *fv_ptr;
};
struct ice_boost_key_value {
#define ICE_BOOST_REMAINING_HV_KEY 15
	u8 remaining_hv_key[ICE_BOOST_REMAINING_HV_KEY];
	__le16 hv_dst_port_key;
	__le16 hv_src_port_key;
	u8 tcam_search_key;
} __packed;
struct ice_boost_key {
	struct ice_boost_key_value key;
	struct ice_boost_key_value key2;
};
struct ice_boost_tcam_entry {
	__le16 addr;
	__le16 reserved;
	struct ice_boost_key key;
	u8 boost_hit_index_group;
#define ICE_BOOST_BIT_FIELDS 43
	u8 bit_fields[ICE_BOOST_BIT_FIELDS];
};
struct ice_boost_tcam_section {
	__le16 count;
	__le16 reserved;
	struct ice_boost_tcam_entry tcam[];
};
#define ICE_MAX_BST_TCAMS_IN_BUF                                               \
	ICE_MAX_ENTRIES_IN_BUF(struct_size_t(struct ice_boost_tcam_section,  \
					   tcam, 1) -                          \
				       sizeof(struct ice_boost_tcam_entry),    \
			       sizeof(struct ice_boost_tcam_entry))
struct ice_marker_ptype_tcam_entry {
#define ICE_MARKER_PTYPE_TCAM_ADDR_MAX 1024
	__le16 addr;
	__le16 ptype;
	u8 keys[20];
};
struct ice_marker_ptype_tcam_section {
	__le16 count;
	__le16 reserved;
	struct ice_marker_ptype_tcam_entry tcam[];
};
#define ICE_MAX_MARKER_PTYPE_TCAMS_IN_BUF                                    \
	ICE_MAX_ENTRIES_IN_BUF(struct_size_t(struct ice_marker_ptype_tcam_section,  tcam, \
			    1) -                                             \
			sizeof(struct ice_marker_ptype_tcam_entry),          \
		sizeof(struct ice_marker_ptype_tcam_entry))
struct ice_xlt1_section {
	__le16 count;
	__le16 offset;
	u8 value[];
};
struct ice_xlt2_section {
	__le16 count;
	__le16 offset;
	__le16 value[];
};
struct ice_prof_redir_section {
	__le16 count;
	__le16 offset;
	u8 redir_value[];
};
struct ice_buf_build {
	struct ice_buf buf;
	u16 reserved_section_table_entries;
};
struct ice_pkg_enum {
	struct ice_buf_table *buf_table;
	u32 buf_idx;
	u32 type;
	struct ice_buf_hdr *buf;
	u32 sect_idx;
	void *sect;
	u32 sect_type;
	u32 entry_idx;
	void *(*handler)(u32 sect_type, void *section, u32 index, u32 *offset);
};
int ice_aq_upload_section(struct ice_hw *hw, struct ice_buf_hdr *pkg_buf,
			  u16 buf_size, struct ice_sq_cd *cd);
void *ice_pkg_buf_alloc_section(struct ice_buf_build *bld, u32 type, u16 size);
struct ice_buf_build *ice_pkg_buf_alloc(struct ice_hw *hw);
int ice_update_pkg_no_lock(struct ice_hw *hw, struct ice_buf *bufs, u32 count);
int ice_update_pkg(struct ice_hw *hw, struct ice_buf *bufs, u32 count);
int ice_pkg_buf_reserve_section(struct ice_buf_build *bld, u16 count);
u16 ice_pkg_buf_get_active_sections(struct ice_buf_build *bld);
void *ice_pkg_enum_section(struct ice_seg *ice_seg, struct ice_pkg_enum *state,
			   u32 sect_type);
#endif
