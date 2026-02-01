
 

#include "ice_common.h"
#include "ice.h"
#include "ice_ddp.h"

 
#define ICE_DVM_PRE "BOOST_MAC_VLAN_DVM"  
#define ICE_SVM_PRE "BOOST_MAC_VLAN_SVM"  

 
#define ICE_TNL_PRE "TNL_"
static const struct ice_tunnel_type_scan tnls[] = {
	{ TNL_VXLAN, "TNL_VXLAN_PF" },
	{ TNL_GENEVE, "TNL_GENEVE_PF" },
	{ TNL_LAST, "" }
};

 
static enum ice_ddp_state ice_verify_pkg(struct ice_pkg_hdr *pkg, u32 len)
{
	u32 seg_count;
	u32 i;

	if (len < struct_size(pkg, seg_offset, 1))
		return ICE_DDP_PKG_INVALID_FILE;

	if (pkg->pkg_format_ver.major != ICE_PKG_FMT_VER_MAJ ||
	    pkg->pkg_format_ver.minor != ICE_PKG_FMT_VER_MNR ||
	    pkg->pkg_format_ver.update != ICE_PKG_FMT_VER_UPD ||
	    pkg->pkg_format_ver.draft != ICE_PKG_FMT_VER_DFT)
		return ICE_DDP_PKG_INVALID_FILE;

	 
	seg_count = le32_to_cpu(pkg->seg_count);
	if (seg_count < 1)
		return ICE_DDP_PKG_INVALID_FILE;

	 
	if (len < struct_size(pkg, seg_offset, seg_count))
		return ICE_DDP_PKG_INVALID_FILE;

	 
	for (i = 0; i < seg_count; i++) {
		u32 off = le32_to_cpu(pkg->seg_offset[i]);
		struct ice_generic_seg_hdr *seg;

		 
		if (len < off + sizeof(*seg))
			return ICE_DDP_PKG_INVALID_FILE;

		seg = (struct ice_generic_seg_hdr *)((u8 *)pkg + off);

		 
		if (len < off + le32_to_cpu(seg->seg_size))
			return ICE_DDP_PKG_INVALID_FILE;
	}

	return ICE_DDP_PKG_SUCCESS;
}

 
void ice_free_seg(struct ice_hw *hw)
{
	if (hw->pkg_copy) {
		devm_kfree(ice_hw_to_dev(hw), hw->pkg_copy);
		hw->pkg_copy = NULL;
		hw->pkg_size = 0;
	}
	hw->seg = NULL;
}

 
static enum ice_ddp_state ice_chk_pkg_version(struct ice_pkg_ver *pkg_ver)
{
	if (pkg_ver->major > ICE_PKG_SUPP_VER_MAJ ||
	    (pkg_ver->major == ICE_PKG_SUPP_VER_MAJ &&
	     pkg_ver->minor > ICE_PKG_SUPP_VER_MNR))
		return ICE_DDP_PKG_FILE_VERSION_TOO_HIGH;
	else if (pkg_ver->major < ICE_PKG_SUPP_VER_MAJ ||
		 (pkg_ver->major == ICE_PKG_SUPP_VER_MAJ &&
		  pkg_ver->minor < ICE_PKG_SUPP_VER_MNR))
		return ICE_DDP_PKG_FILE_VERSION_TOO_LOW;

	return ICE_DDP_PKG_SUCCESS;
}

 
static struct ice_buf_hdr *ice_pkg_val_buf(struct ice_buf *buf)
{
	struct ice_buf_hdr *hdr;
	u16 section_count;
	u16 data_end;

	hdr = (struct ice_buf_hdr *)buf->buf;
	 
	section_count = le16_to_cpu(hdr->section_count);
	if (section_count < ICE_MIN_S_COUNT || section_count > ICE_MAX_S_COUNT)
		return NULL;

	data_end = le16_to_cpu(hdr->data_end);
	if (data_end < ICE_MIN_S_DATA_END || data_end > ICE_MAX_S_DATA_END)
		return NULL;

	return hdr;
}

 
static struct ice_buf_table *ice_find_buf_table(struct ice_seg *ice_seg)
{
	struct ice_nvm_table *nvms = (struct ice_nvm_table *)
		(ice_seg->device_table + le32_to_cpu(ice_seg->device_table_count));

	return (__force struct ice_buf_table *)(nvms->vers +
						le32_to_cpu(nvms->table_count));
}

 
static struct ice_buf_hdr *ice_pkg_enum_buf(struct ice_seg *ice_seg,
					    struct ice_pkg_enum *state)
{
	if (ice_seg) {
		state->buf_table = ice_find_buf_table(ice_seg);
		if (!state->buf_table)
			return NULL;

		state->buf_idx = 0;
		return ice_pkg_val_buf(state->buf_table->buf_array);
	}

	if (++state->buf_idx < le32_to_cpu(state->buf_table->buf_count))
		return ice_pkg_val_buf(state->buf_table->buf_array +
				       state->buf_idx);
	else
		return NULL;
}

 
static bool ice_pkg_advance_sect(struct ice_seg *ice_seg,
				 struct ice_pkg_enum *state)
{
	if (!ice_seg && !state->buf)
		return false;

	if (!ice_seg && state->buf)
		if (++state->sect_idx < le16_to_cpu(state->buf->section_count))
			return true;

	state->buf = ice_pkg_enum_buf(ice_seg, state);
	if (!state->buf)
		return false;

	 
	state->sect_idx = 0;
	return true;
}

 
void *ice_pkg_enum_section(struct ice_seg *ice_seg, struct ice_pkg_enum *state,
			   u32 sect_type)
{
	u16 offset, size;

	if (ice_seg)
		state->type = sect_type;

	if (!ice_pkg_advance_sect(ice_seg, state))
		return NULL;

	 
	while (state->buf->section_entry[state->sect_idx].type !=
	       cpu_to_le32(state->type))
		if (!ice_pkg_advance_sect(NULL, state))
			return NULL;

	 
	offset = le16_to_cpu(state->buf->section_entry[state->sect_idx].offset);
	if (offset < ICE_MIN_S_OFF || offset > ICE_MAX_S_OFF)
		return NULL;

	size = le16_to_cpu(state->buf->section_entry[state->sect_idx].size);
	if (size < ICE_MIN_S_SZ || size > ICE_MAX_S_SZ)
		return NULL;

	 
	if (offset + size > ICE_PKG_BUF_SIZE)
		return NULL;

	state->sect_type =
		le32_to_cpu(state->buf->section_entry[state->sect_idx].type);

	 
	state->sect =
		((u8 *)state->buf) +
		le16_to_cpu(state->buf->section_entry[state->sect_idx].offset);

	return state->sect;
}

 
static void *ice_pkg_enum_entry(struct ice_seg *ice_seg,
				struct ice_pkg_enum *state, u32 sect_type,
				u32 *offset,
				void *(*handler)(u32 sect_type, void *section,
						 u32 index, u32 *offset))
{
	void *entry;

	if (ice_seg) {
		if (!handler)
			return NULL;

		if (!ice_pkg_enum_section(ice_seg, state, sect_type))
			return NULL;

		state->entry_idx = 0;
		state->handler = handler;
	} else {
		state->entry_idx++;
	}

	if (!state->handler)
		return NULL;

	 
	entry = state->handler(state->sect_type, state->sect, state->entry_idx,
			       offset);
	if (!entry) {
		 
		if (!ice_pkg_enum_section(NULL, state, 0))
			return NULL;

		state->entry_idx = 0;
		entry = state->handler(state->sect_type, state->sect,
				       state->entry_idx, offset);
	}

	return entry;
}

 
static void *ice_sw_fv_handler(u32 sect_type, void *section, u32 index,
			       u32 *offset)
{
	struct ice_sw_fv_section *fv_section = section;

	if (!section || sect_type != ICE_SID_FLD_VEC_SW)
		return NULL;
	if (index >= le16_to_cpu(fv_section->count))
		return NULL;
	if (offset)
		 
		*offset = le16_to_cpu(fv_section->base_offset) + index;
	return fv_section->fv + index;
}

 
static int ice_get_prof_index_max(struct ice_hw *hw)
{
	u16 prof_index = 0, j, max_prof_index = 0;
	struct ice_pkg_enum state;
	struct ice_seg *ice_seg;
	bool flag = false;
	struct ice_fv *fv;
	u32 offset;

	memset(&state, 0, sizeof(state));

	if (!hw->seg)
		return -EINVAL;

	ice_seg = hw->seg;

	do {
		fv = ice_pkg_enum_entry(ice_seg, &state, ICE_SID_FLD_VEC_SW,
					&offset, ice_sw_fv_handler);
		if (!fv)
			break;
		ice_seg = NULL;

		 
		for (j = 0; j < hw->blk[ICE_BLK_SW].es.fvw; j++)
			if (fv->ew[j].prot_id != ICE_PROT_INVALID ||
			    fv->ew[j].off != ICE_FV_OFFSET_INVAL)
				flag = true;
		if (flag && prof_index > max_prof_index)
			max_prof_index = prof_index;

		prof_index++;
		flag = false;
	} while (fv);

	hw->switch_info->max_used_prof_index = max_prof_index;

	return 0;
}

 
static enum ice_ddp_state ice_get_ddp_pkg_state(struct ice_hw *hw,
						bool already_loaded)
{
	if (hw->pkg_ver.major == hw->active_pkg_ver.major &&
	    hw->pkg_ver.minor == hw->active_pkg_ver.minor &&
	    hw->pkg_ver.update == hw->active_pkg_ver.update &&
	    hw->pkg_ver.draft == hw->active_pkg_ver.draft &&
	    !memcmp(hw->pkg_name, hw->active_pkg_name, sizeof(hw->pkg_name))) {
		if (already_loaded)
			return ICE_DDP_PKG_SAME_VERSION_ALREADY_LOADED;
		else
			return ICE_DDP_PKG_SUCCESS;
	} else if (hw->active_pkg_ver.major != ICE_PKG_SUPP_VER_MAJ ||
		   hw->active_pkg_ver.minor != ICE_PKG_SUPP_VER_MNR) {
		return ICE_DDP_PKG_ALREADY_LOADED_NOT_SUPPORTED;
	} else if (hw->active_pkg_ver.major == ICE_PKG_SUPP_VER_MAJ &&
		   hw->active_pkg_ver.minor == ICE_PKG_SUPP_VER_MNR) {
		return ICE_DDP_PKG_COMPATIBLE_ALREADY_LOADED;
	} else {
		return ICE_DDP_PKG_ERR;
	}
}

 
static void ice_init_pkg_regs(struct ice_hw *hw)
{
#define ICE_SW_BLK_INP_MASK_L 0xFFFFFFFF
#define ICE_SW_BLK_INP_MASK_H 0x0000FFFF
#define ICE_SW_BLK_IDX 0

	 
	wr32(hw, GL_PREEXT_L2_PMASK0(ICE_SW_BLK_IDX), ICE_SW_BLK_INP_MASK_L);
	wr32(hw, GL_PREEXT_L2_PMASK1(ICE_SW_BLK_IDX), ICE_SW_BLK_INP_MASK_H);
}

 
static void *ice_marker_ptype_tcam_handler(u32 sect_type, void *section,
					   u32 index, u32 *offset)
{
	struct ice_marker_ptype_tcam_section *marker_ptype;

	if (sect_type != ICE_SID_RXPARSER_MARKER_PTYPE)
		return NULL;

	if (index > ICE_MAX_MARKER_PTYPE_TCAMS_IN_BUF)
		return NULL;

	if (offset)
		*offset = 0;

	marker_ptype = section;
	if (index >= le16_to_cpu(marker_ptype->count))
		return NULL;

	return marker_ptype->tcam + index;
}

 
static void ice_add_dvm_hint(struct ice_hw *hw, u16 val, bool enable)
{
	if (hw->dvm_upd.count < ICE_DVM_MAX_ENTRIES) {
		hw->dvm_upd.tbl[hw->dvm_upd.count].boost_addr = val;
		hw->dvm_upd.tbl[hw->dvm_upd.count].enable = enable;
		hw->dvm_upd.count++;
	}
}

 
static void ice_add_tunnel_hint(struct ice_hw *hw, char *label_name, u16 val)
{
	if (hw->tnl.count < ICE_TUNNEL_MAX_ENTRIES) {
		u16 i;

		for (i = 0; tnls[i].type != TNL_LAST; i++) {
			size_t len = strlen(tnls[i].label_prefix);

			 
			if (strncmp(label_name, tnls[i].label_prefix, len))
				continue;

			 
			if ((label_name[len] - '0') == hw->pf_id) {
				hw->tnl.tbl[hw->tnl.count].type = tnls[i].type;
				hw->tnl.tbl[hw->tnl.count].valid = false;
				hw->tnl.tbl[hw->tnl.count].boost_addr = val;
				hw->tnl.tbl[hw->tnl.count].port = 0;
				hw->tnl.count++;
				break;
			}
		}
	}
}

 
static void *ice_label_enum_handler(u32 __always_unused sect_type,
				    void *section, u32 index, u32 *offset)
{
	struct ice_label_section *labels;

	if (!section)
		return NULL;

	if (index > ICE_MAX_LABELS_IN_BUF)
		return NULL;

	if (offset)
		*offset = 0;

	labels = section;
	if (index >= le16_to_cpu(labels->count))
		return NULL;

	return labels->label + index;
}

 
static char *ice_enum_labels(struct ice_seg *ice_seg, u32 type,
			     struct ice_pkg_enum *state, u16 *value)
{
	struct ice_label *label;

	 
	if (type && !(type >= ICE_SID_LBL_FIRST && type <= ICE_SID_LBL_LAST))
		return NULL;

	label = ice_pkg_enum_entry(ice_seg, state, type, NULL,
				   ice_label_enum_handler);
	if (!label)
		return NULL;

	*value = le16_to_cpu(label->value);
	return label->name;
}

 
static void *ice_boost_tcam_handler(u32 sect_type, void *section, u32 index,
				    u32 *offset)
{
	struct ice_boost_tcam_section *boost;

	if (!section)
		return NULL;

	if (sect_type != ICE_SID_RXPARSER_BOOST_TCAM)
		return NULL;

	if (index > ICE_MAX_BST_TCAMS_IN_BUF)
		return NULL;

	if (offset)
		*offset = 0;

	boost = section;
	if (index >= le16_to_cpu(boost->count))
		return NULL;

	return boost->tcam + index;
}

 
static int ice_find_boost_entry(struct ice_seg *ice_seg, u16 addr,
				struct ice_boost_tcam_entry **entry)
{
	struct ice_boost_tcam_entry *tcam;
	struct ice_pkg_enum state;

	memset(&state, 0, sizeof(state));

	if (!ice_seg)
		return -EINVAL;

	do {
		tcam = ice_pkg_enum_entry(ice_seg, &state,
					  ICE_SID_RXPARSER_BOOST_TCAM, NULL,
					  ice_boost_tcam_handler);
		if (tcam && le16_to_cpu(tcam->addr) == addr) {
			*entry = tcam;
			return 0;
		}

		ice_seg = NULL;
	} while (tcam);

	*entry = NULL;
	return -EIO;
}

 
bool ice_is_init_pkg_successful(enum ice_ddp_state state)
{
	switch (state) {
	case ICE_DDP_PKG_SUCCESS:
	case ICE_DDP_PKG_SAME_VERSION_ALREADY_LOADED:
	case ICE_DDP_PKG_COMPATIBLE_ALREADY_LOADED:
		return true;
	default:
		return false;
	}
}

 
struct ice_buf_build *ice_pkg_buf_alloc(struct ice_hw *hw)
{
	struct ice_buf_build *bld;
	struct ice_buf_hdr *buf;

	bld = devm_kzalloc(ice_hw_to_dev(hw), sizeof(*bld), GFP_KERNEL);
	if (!bld)
		return NULL;

	buf = (struct ice_buf_hdr *)bld;
	buf->data_end =
		cpu_to_le16(offsetof(struct ice_buf_hdr, section_entry));
	return bld;
}

static bool ice_is_gtp_u_profile(u16 prof_idx)
{
	return (prof_idx >= ICE_PROFID_IPV6_GTPU_TEID &&
		prof_idx <= ICE_PROFID_IPV6_GTPU_IPV6_TCP_INNER) ||
	       prof_idx == ICE_PROFID_IPV4_GTPU_TEID;
}

static bool ice_is_gtp_c_profile(u16 prof_idx)
{
	switch (prof_idx) {
	case ICE_PROFID_IPV4_GTPC_TEID:
	case ICE_PROFID_IPV4_GTPC_NO_TEID:
	case ICE_PROFID_IPV6_GTPC_TEID:
	case ICE_PROFID_IPV6_GTPC_NO_TEID:
		return true;
	default:
		return false;
	}
}

 
static enum ice_prof_type ice_get_sw_prof_type(struct ice_hw *hw,
					       struct ice_fv *fv, u32 prof_idx)
{
	u16 i;

	if (ice_is_gtp_c_profile(prof_idx))
		return ICE_PROF_TUN_GTPC;

	if (ice_is_gtp_u_profile(prof_idx))
		return ICE_PROF_TUN_GTPU;

	for (i = 0; i < hw->blk[ICE_BLK_SW].es.fvw; i++) {
		 
		if (fv->ew[i].prot_id == (u8)ICE_PROT_UDP_OF &&
		    fv->ew[i].off == ICE_VNI_OFFSET)
			return ICE_PROF_TUN_UDP;

		 
		if (fv->ew[i].prot_id == (u8)ICE_PROT_GRE_OF)
			return ICE_PROF_TUN_GRE;
	}

	return ICE_PROF_NON_TUN;
}

 
void ice_get_sw_fv_bitmap(struct ice_hw *hw, enum ice_prof_type req_profs,
			  unsigned long *bm)
{
	struct ice_pkg_enum state;
	struct ice_seg *ice_seg;
	struct ice_fv *fv;

	if (req_profs == ICE_PROF_ALL) {
		bitmap_set(bm, 0, ICE_MAX_NUM_PROFILES);
		return;
	}

	memset(&state, 0, sizeof(state));
	bitmap_zero(bm, ICE_MAX_NUM_PROFILES);
	ice_seg = hw->seg;
	do {
		enum ice_prof_type prof_type;
		u32 offset;

		fv = ice_pkg_enum_entry(ice_seg, &state, ICE_SID_FLD_VEC_SW,
					&offset, ice_sw_fv_handler);
		ice_seg = NULL;

		if (fv) {
			 
			prof_type = ice_get_sw_prof_type(hw, fv, offset);

			if (req_profs & prof_type)
				set_bit((u16)offset, bm);
		}
	} while (fv);
}

 
int ice_get_sw_fv_list(struct ice_hw *hw, struct ice_prot_lkup_ext *lkups,
		       unsigned long *bm, struct list_head *fv_list)
{
	struct ice_sw_fv_list_entry *fvl;
	struct ice_sw_fv_list_entry *tmp;
	struct ice_pkg_enum state;
	struct ice_seg *ice_seg;
	struct ice_fv *fv;
	u32 offset;

	memset(&state, 0, sizeof(state));

	if (!lkups->n_val_words || !hw->seg)
		return -EINVAL;

	ice_seg = hw->seg;
	do {
		u16 i;

		fv = ice_pkg_enum_entry(ice_seg, &state, ICE_SID_FLD_VEC_SW,
					&offset, ice_sw_fv_handler);
		if (!fv)
			break;
		ice_seg = NULL;

		 
		if (!test_bit((u16)offset, bm))
			continue;

		for (i = 0; i < lkups->n_val_words; i++) {
			int j;

			for (j = 0; j < hw->blk[ICE_BLK_SW].es.fvw; j++)
				if (fv->ew[j].prot_id ==
					    lkups->fv_words[i].prot_id &&
				    fv->ew[j].off == lkups->fv_words[i].off)
					break;
			if (j >= hw->blk[ICE_BLK_SW].es.fvw)
				break;
			if (i + 1 == lkups->n_val_words) {
				fvl = devm_kzalloc(ice_hw_to_dev(hw),
						   sizeof(*fvl), GFP_KERNEL);
				if (!fvl)
					goto err;
				fvl->fv_ptr = fv;
				fvl->profile_id = offset;
				list_add(&fvl->list_entry, fv_list);
				break;
			}
		}
	} while (fv);
	if (list_empty(fv_list)) {
		dev_warn(ice_hw_to_dev(hw),
			 "Required profiles not found in currently loaded DDP package");
		return -EIO;
	}

	return 0;

err:
	list_for_each_entry_safe(fvl, tmp, fv_list, list_entry) {
		list_del(&fvl->list_entry);
		devm_kfree(ice_hw_to_dev(hw), fvl);
	}

	return -ENOMEM;
}

 
void ice_init_prof_result_bm(struct ice_hw *hw)
{
	struct ice_pkg_enum state;
	struct ice_seg *ice_seg;
	struct ice_fv *fv;

	memset(&state, 0, sizeof(state));

	if (!hw->seg)
		return;

	ice_seg = hw->seg;
	do {
		u32 off;
		u16 i;

		fv = ice_pkg_enum_entry(ice_seg, &state, ICE_SID_FLD_VEC_SW,
					&off, ice_sw_fv_handler);
		ice_seg = NULL;
		if (!fv)
			break;

		bitmap_zero(hw->switch_info->prof_res_bm[off],
			    ICE_MAX_FV_WORDS);

		 
		for (i = 1; i < ICE_MAX_FV_WORDS; i++)
			if (fv->ew[i].prot_id == ICE_PROT_INVALID &&
			    fv->ew[i].off == ICE_FV_OFFSET_INVAL)
				set_bit(i, hw->switch_info->prof_res_bm[off]);
	} while (fv);
}

 
void ice_pkg_buf_free(struct ice_hw *hw, struct ice_buf_build *bld)
{
	devm_kfree(ice_hw_to_dev(hw), bld);
}

 
int ice_pkg_buf_reserve_section(struct ice_buf_build *bld, u16 count)
{
	struct ice_buf_hdr *buf;
	u16 section_count;
	u16 data_end;

	if (!bld)
		return -EINVAL;

	buf = (struct ice_buf_hdr *)&bld->buf;

	 
	section_count = le16_to_cpu(buf->section_count);
	if (section_count > 0)
		return -EIO;

	if (bld->reserved_section_table_entries + count > ICE_MAX_S_COUNT)
		return -EIO;
	bld->reserved_section_table_entries += count;

	data_end = le16_to_cpu(buf->data_end) +
		   flex_array_size(buf, section_entry, count);
	buf->data_end = cpu_to_le16(data_end);

	return 0;
}

 
void *ice_pkg_buf_alloc_section(struct ice_buf_build *bld, u32 type, u16 size)
{
	struct ice_buf_hdr *buf;
	u16 sect_count;
	u16 data_end;

	if (!bld || !type || !size)
		return NULL;

	buf = (struct ice_buf_hdr *)&bld->buf;

	 
	data_end = le16_to_cpu(buf->data_end);

	 
	data_end = ALIGN(data_end, 4);

	if ((data_end + size) > ICE_MAX_S_DATA_END)
		return NULL;

	 
	sect_count = le16_to_cpu(buf->section_count);
	if (sect_count < bld->reserved_section_table_entries) {
		void *section_ptr = ((u8 *)buf) + data_end;

		buf->section_entry[sect_count].offset = cpu_to_le16(data_end);
		buf->section_entry[sect_count].size = cpu_to_le16(size);
		buf->section_entry[sect_count].type = cpu_to_le32(type);

		data_end += size;
		buf->data_end = cpu_to_le16(data_end);

		buf->section_count = cpu_to_le16(sect_count + 1);
		return section_ptr;
	}

	 
	return NULL;
}

 
struct ice_buf_build *ice_pkg_buf_alloc_single_section(struct ice_hw *hw,
						       u32 type, u16 size,
						       void **section)
{
	struct ice_buf_build *buf;

	if (!section)
		return NULL;

	buf = ice_pkg_buf_alloc(hw);
	if (!buf)
		return NULL;

	if (ice_pkg_buf_reserve_section(buf, 1))
		goto ice_pkg_buf_alloc_single_section_err;

	*section = ice_pkg_buf_alloc_section(buf, type, size);
	if (!*section)
		goto ice_pkg_buf_alloc_single_section_err;

	return buf;

ice_pkg_buf_alloc_single_section_err:
	ice_pkg_buf_free(hw, buf);
	return NULL;
}

 
u16 ice_pkg_buf_get_active_sections(struct ice_buf_build *bld)
{
	struct ice_buf_hdr *buf;

	if (!bld)
		return 0;

	buf = (struct ice_buf_hdr *)&bld->buf;
	return le16_to_cpu(buf->section_count);
}

 
struct ice_buf *ice_pkg_buf(struct ice_buf_build *bld)
{
	if (!bld)
		return NULL;

	return &bld->buf;
}

static enum ice_ddp_state ice_map_aq_err_to_ddp_state(enum ice_aq_err aq_err)
{
	switch (aq_err) {
	case ICE_AQ_RC_ENOSEC:
	case ICE_AQ_RC_EBADSIG:
		return ICE_DDP_PKG_FILE_SIGNATURE_INVALID;
	case ICE_AQ_RC_ESVN:
		return ICE_DDP_PKG_FILE_REVISION_TOO_LOW;
	case ICE_AQ_RC_EBADMAN:
	case ICE_AQ_RC_EBADBUF:
		return ICE_DDP_PKG_LOAD_ERROR;
	default:
		return ICE_DDP_PKG_ERR;
	}
}

 
static int ice_acquire_global_cfg_lock(struct ice_hw *hw,
				       enum ice_aq_res_access_type access)
{
	int status;

	status = ice_acquire_res(hw, ICE_GLOBAL_CFG_LOCK_RES_ID, access,
				 ICE_GLOBAL_CFG_LOCK_TIMEOUT);

	if (!status)
		mutex_lock(&ice_global_cfg_lock_sw);
	else if (status == -EALREADY)
		ice_debug(hw, ICE_DBG_PKG,
			  "Global config lock: No work to do\n");

	return status;
}

 
static void ice_release_global_cfg_lock(struct ice_hw *hw)
{
	mutex_unlock(&ice_global_cfg_lock_sw);
	ice_release_res(hw, ICE_GLOBAL_CFG_LOCK_RES_ID);
}

 
static int
ice_aq_download_pkg(struct ice_hw *hw, struct ice_buf_hdr *pkg_buf,
		    u16 buf_size, bool last_buf, u32 *error_offset,
		    u32 *error_info, struct ice_sq_cd *cd)
{
	struct ice_aqc_download_pkg *cmd;
	struct ice_aq_desc desc;
	int status;

	if (error_offset)
		*error_offset = 0;
	if (error_info)
		*error_info = 0;

	cmd = &desc.params.download_pkg;
	ice_fill_dflt_direct_cmd_desc(&desc, ice_aqc_opc_download_pkg);
	desc.flags |= cpu_to_le16(ICE_AQ_FLAG_RD);

	if (last_buf)
		cmd->flags |= ICE_AQC_DOWNLOAD_PKG_LAST_BUF;

	status = ice_aq_send_cmd(hw, &desc, pkg_buf, buf_size, cd);
	if (status == -EIO) {
		 
		struct ice_aqc_download_pkg_resp *resp;

		resp = (struct ice_aqc_download_pkg_resp *)pkg_buf;
		if (error_offset)
			*error_offset = le32_to_cpu(resp->error_offset);
		if (error_info)
			*error_info = le32_to_cpu(resp->error_info);
	}

	return status;
}

 
static enum ice_ddp_state ice_dwnld_cfg_bufs(struct ice_hw *hw,
					     struct ice_buf *bufs, u32 count)
{
	enum ice_ddp_state state = ICE_DDP_PKG_SUCCESS;
	struct ice_buf_hdr *bh;
	enum ice_aq_err err;
	u32 offset, info, i;
	int status;

	if (!bufs || !count)
		return ICE_DDP_PKG_ERR;

	 
	bh = (struct ice_buf_hdr *)bufs;
	if (le32_to_cpu(bh->section_entry[0].type) & ICE_METADATA_BUF)
		return ICE_DDP_PKG_SUCCESS;

	status = ice_acquire_global_cfg_lock(hw, ICE_RES_WRITE);
	if (status) {
		if (status == -EALREADY)
			return ICE_DDP_PKG_ALREADY_LOADED;
		return ice_map_aq_err_to_ddp_state(hw->adminq.sq_last_status);
	}

	for (i = 0; i < count; i++) {
		bool last = ((i + 1) == count);

		if (!last) {
			 
			bh = (struct ice_buf_hdr *)(bufs + i + 1);

			 
			if (le16_to_cpu(bh->section_count))
				if (le32_to_cpu(bh->section_entry[0].type) &
				    ICE_METADATA_BUF)
					last = true;
		}

		bh = (struct ice_buf_hdr *)(bufs + i);

		status = ice_aq_download_pkg(hw, bh, ICE_PKG_BUF_SIZE, last,
					     &offset, &info, NULL);

		 
		if (status) {
			ice_debug(hw, ICE_DBG_PKG,
				  "Pkg download failed: err %d off %d inf %d\n",
				  status, offset, info);
			err = hw->adminq.sq_last_status;
			state = ice_map_aq_err_to_ddp_state(err);
			break;
		}

		if (last)
			break;
	}

	if (!status) {
		status = ice_set_vlan_mode(hw);
		if (status)
			ice_debug(hw, ICE_DBG_PKG,
				  "Failed to set VLAN mode: err %d\n", status);
	}

	ice_release_global_cfg_lock(hw);

	return state;
}

 
static int ice_aq_get_pkg_info_list(struct ice_hw *hw,
				    struct ice_aqc_get_pkg_info_resp *pkg_info,
				    u16 buf_size, struct ice_sq_cd *cd)
{
	struct ice_aq_desc desc;

	ice_fill_dflt_direct_cmd_desc(&desc, ice_aqc_opc_get_pkg_info_list);

	return ice_aq_send_cmd(hw, &desc, pkg_info, buf_size, cd);
}

 
static enum ice_ddp_state ice_download_pkg(struct ice_hw *hw,
					   struct ice_seg *ice_seg)
{
	struct ice_buf_table *ice_buf_tbl;
	int status;

	ice_debug(hw, ICE_DBG_PKG, "Segment format version: %d.%d.%d.%d\n",
		  ice_seg->hdr.seg_format_ver.major,
		  ice_seg->hdr.seg_format_ver.minor,
		  ice_seg->hdr.seg_format_ver.update,
		  ice_seg->hdr.seg_format_ver.draft);

	ice_debug(hw, ICE_DBG_PKG, "Seg: type 0x%X, size %d, name %s\n",
		  le32_to_cpu(ice_seg->hdr.seg_type),
		  le32_to_cpu(ice_seg->hdr.seg_size), ice_seg->hdr.seg_id);

	ice_buf_tbl = ice_find_buf_table(ice_seg);

	ice_debug(hw, ICE_DBG_PKG, "Seg buf count: %d\n",
		  le32_to_cpu(ice_buf_tbl->buf_count));

	status = ice_dwnld_cfg_bufs(hw, ice_buf_tbl->buf_array,
				    le32_to_cpu(ice_buf_tbl->buf_count));

	ice_post_pkg_dwnld_vlan_mode_cfg(hw);

	return status;
}

 
static int ice_aq_update_pkg(struct ice_hw *hw, struct ice_buf_hdr *pkg_buf,
			     u16 buf_size, bool last_buf, u32 *error_offset,
			     u32 *error_info, struct ice_sq_cd *cd)
{
	struct ice_aqc_download_pkg *cmd;
	struct ice_aq_desc desc;
	int status;

	if (error_offset)
		*error_offset = 0;
	if (error_info)
		*error_info = 0;

	cmd = &desc.params.download_pkg;
	ice_fill_dflt_direct_cmd_desc(&desc, ice_aqc_opc_update_pkg);
	desc.flags |= cpu_to_le16(ICE_AQ_FLAG_RD);

	if (last_buf)
		cmd->flags |= ICE_AQC_DOWNLOAD_PKG_LAST_BUF;

	status = ice_aq_send_cmd(hw, &desc, pkg_buf, buf_size, cd);
	if (status == -EIO) {
		 
		struct ice_aqc_download_pkg_resp *resp;

		resp = (struct ice_aqc_download_pkg_resp *)pkg_buf;
		if (error_offset)
			*error_offset = le32_to_cpu(resp->error_offset);
		if (error_info)
			*error_info = le32_to_cpu(resp->error_info);
	}

	return status;
}

 
int ice_aq_upload_section(struct ice_hw *hw, struct ice_buf_hdr *pkg_buf,
			  u16 buf_size, struct ice_sq_cd *cd)
{
	struct ice_aq_desc desc;

	ice_fill_dflt_direct_cmd_desc(&desc, ice_aqc_opc_upload_section);
	desc.flags |= cpu_to_le16(ICE_AQ_FLAG_RD);

	return ice_aq_send_cmd(hw, &desc, pkg_buf, buf_size, cd);
}

 
int ice_update_pkg_no_lock(struct ice_hw *hw, struct ice_buf *bufs, u32 count)
{
	int status = 0;
	u32 i;

	for (i = 0; i < count; i++) {
		struct ice_buf_hdr *bh = (struct ice_buf_hdr *)(bufs + i);
		bool last = ((i + 1) == count);
		u32 offset, info;

		status = ice_aq_update_pkg(hw, bh, le16_to_cpu(bh->data_end),
					   last, &offset, &info, NULL);

		if (status) {
			ice_debug(hw, ICE_DBG_PKG,
				  "Update pkg failed: err %d off %d inf %d\n",
				  status, offset, info);
			break;
		}
	}

	return status;
}

 
int ice_update_pkg(struct ice_hw *hw, struct ice_buf *bufs, u32 count)
{
	int status;

	status = ice_acquire_change_lock(hw, ICE_RES_WRITE);
	if (status)
		return status;

	status = ice_update_pkg_no_lock(hw, bufs, count);

	ice_release_change_lock(hw);

	return status;
}

 
static struct ice_generic_seg_hdr *
ice_find_seg_in_pkg(struct ice_hw *hw, u32 seg_type,
		    struct ice_pkg_hdr *pkg_hdr)
{
	u32 i;

	ice_debug(hw, ICE_DBG_PKG, "Package format version: %d.%d.%d.%d\n",
		  pkg_hdr->pkg_format_ver.major, pkg_hdr->pkg_format_ver.minor,
		  pkg_hdr->pkg_format_ver.update,
		  pkg_hdr->pkg_format_ver.draft);

	 
	for (i = 0; i < le32_to_cpu(pkg_hdr->seg_count); i++) {
		struct ice_generic_seg_hdr *seg;

		seg = (struct ice_generic_seg_hdr
			       *)((u8 *)pkg_hdr +
				  le32_to_cpu(pkg_hdr->seg_offset[i]));

		if (le32_to_cpu(seg->seg_type) == seg_type)
			return seg;
	}

	return NULL;
}

 
static enum ice_ddp_state ice_init_pkg_info(struct ice_hw *hw,
					    struct ice_pkg_hdr *pkg_hdr)
{
	struct ice_generic_seg_hdr *seg_hdr;

	if (!pkg_hdr)
		return ICE_DDP_PKG_ERR;

	seg_hdr = ice_find_seg_in_pkg(hw, SEGMENT_TYPE_ICE, pkg_hdr);
	if (seg_hdr) {
		struct ice_meta_sect *meta;
		struct ice_pkg_enum state;

		memset(&state, 0, sizeof(state));

		 
		meta = ice_pkg_enum_section((struct ice_seg *)seg_hdr, &state,
					    ICE_SID_METADATA);
		if (!meta) {
			ice_debug(hw, ICE_DBG_INIT,
				  "Did not find ice metadata section in package\n");
			return ICE_DDP_PKG_INVALID_FILE;
		}

		hw->pkg_ver = meta->ver;
		memcpy(hw->pkg_name, meta->name, sizeof(meta->name));

		ice_debug(hw, ICE_DBG_PKG, "Pkg: %d.%d.%d.%d, %s\n",
			  meta->ver.major, meta->ver.minor, meta->ver.update,
			  meta->ver.draft, meta->name);

		hw->ice_seg_fmt_ver = seg_hdr->seg_format_ver;
		memcpy(hw->ice_seg_id, seg_hdr->seg_id, sizeof(hw->ice_seg_id));

		ice_debug(hw, ICE_DBG_PKG, "Ice Seg: %d.%d.%d.%d, %s\n",
			  seg_hdr->seg_format_ver.major,
			  seg_hdr->seg_format_ver.minor,
			  seg_hdr->seg_format_ver.update,
			  seg_hdr->seg_format_ver.draft, seg_hdr->seg_id);
	} else {
		ice_debug(hw, ICE_DBG_INIT,
			  "Did not find ice segment in driver package\n");
		return ICE_DDP_PKG_INVALID_FILE;
	}

	return ICE_DDP_PKG_SUCCESS;
}

 
static enum ice_ddp_state ice_get_pkg_info(struct ice_hw *hw)
{
	enum ice_ddp_state state = ICE_DDP_PKG_SUCCESS;
	struct ice_aqc_get_pkg_info_resp *pkg_info;
	u16 size;
	u32 i;

	size = struct_size(pkg_info, pkg_info, ICE_PKG_CNT);
	pkg_info = kzalloc(size, GFP_KERNEL);
	if (!pkg_info)
		return ICE_DDP_PKG_ERR;

	if (ice_aq_get_pkg_info_list(hw, pkg_info, size, NULL)) {
		state = ICE_DDP_PKG_ERR;
		goto init_pkg_free_alloc;
	}

	for (i = 0; i < le32_to_cpu(pkg_info->count); i++) {
#define ICE_PKG_FLAG_COUNT 4
		char flags[ICE_PKG_FLAG_COUNT + 1] = { 0 };
		u8 place = 0;

		if (pkg_info->pkg_info[i].is_active) {
			flags[place++] = 'A';
			hw->active_pkg_ver = pkg_info->pkg_info[i].ver;
			hw->active_track_id =
				le32_to_cpu(pkg_info->pkg_info[i].track_id);
			memcpy(hw->active_pkg_name, pkg_info->pkg_info[i].name,
			       sizeof(pkg_info->pkg_info[i].name));
			hw->active_pkg_in_nvm = pkg_info->pkg_info[i].is_in_nvm;
		}
		if (pkg_info->pkg_info[i].is_active_at_boot)
			flags[place++] = 'B';
		if (pkg_info->pkg_info[i].is_modified)
			flags[place++] = 'M';
		if (pkg_info->pkg_info[i].is_in_nvm)
			flags[place++] = 'N';

		ice_debug(hw, ICE_DBG_PKG, "Pkg[%d]: %d.%d.%d.%d,%s,%s\n", i,
			  pkg_info->pkg_info[i].ver.major,
			  pkg_info->pkg_info[i].ver.minor,
			  pkg_info->pkg_info[i].ver.update,
			  pkg_info->pkg_info[i].ver.draft,
			  pkg_info->pkg_info[i].name, flags);
	}

init_pkg_free_alloc:
	kfree(pkg_info);

	return state;
}

 
static enum ice_ddp_state ice_chk_pkg_compat(struct ice_hw *hw,
					     struct ice_pkg_hdr *ospkg,
					     struct ice_seg **seg)
{
	struct ice_aqc_get_pkg_info_resp *pkg;
	enum ice_ddp_state state;
	u16 size;
	u32 i;

	 
	state = ice_chk_pkg_version(&hw->pkg_ver);
	if (state) {
		ice_debug(hw, ICE_DBG_INIT, "Package version check failed.\n");
		return state;
	}

	 
	*seg = (struct ice_seg *)ice_find_seg_in_pkg(hw, SEGMENT_TYPE_ICE,
						     ospkg);
	if (!*seg) {
		ice_debug(hw, ICE_DBG_INIT, "no ice segment in package.\n");
		return ICE_DDP_PKG_INVALID_FILE;
	}

	 
	size = struct_size(pkg, pkg_info, ICE_PKG_CNT);
	pkg = kzalloc(size, GFP_KERNEL);
	if (!pkg)
		return ICE_DDP_PKG_ERR;

	if (ice_aq_get_pkg_info_list(hw, pkg, size, NULL)) {
		state = ICE_DDP_PKG_LOAD_ERROR;
		goto fw_ddp_compat_free_alloc;
	}

	for (i = 0; i < le32_to_cpu(pkg->count); i++) {
		 
		if (!pkg->pkg_info[i].is_in_nvm)
			continue;
		if ((*seg)->hdr.seg_format_ver.major !=
			    pkg->pkg_info[i].ver.major ||
		    (*seg)->hdr.seg_format_ver.minor >
			    pkg->pkg_info[i].ver.minor) {
			state = ICE_DDP_PKG_FW_MISMATCH;
			ice_debug(hw, ICE_DBG_INIT,
				  "OS package is not compatible with NVM.\n");
		}
		 
		break;
	}
fw_ddp_compat_free_alloc:
	kfree(pkg);
	return state;
}

 
static void ice_init_pkg_hints(struct ice_hw *hw, struct ice_seg *ice_seg)
{
	struct ice_pkg_enum state;
	char *label_name;
	u16 val;
	int i;

	memset(&hw->tnl, 0, sizeof(hw->tnl));
	memset(&state, 0, sizeof(state));

	if (!ice_seg)
		return;

	label_name = ice_enum_labels(ice_seg, ICE_SID_LBL_RXPARSER_TMEM, &state,
				     &val);

	while (label_name) {
		if (!strncmp(label_name, ICE_TNL_PRE, strlen(ICE_TNL_PRE)))
			 
			ice_add_tunnel_hint(hw, label_name, val);

		 
		else if (!strncmp(label_name, ICE_DVM_PRE, strlen(ICE_DVM_PRE)))
			ice_add_dvm_hint(hw, val, true);

		 
		else if (!strncmp(label_name, ICE_SVM_PRE, strlen(ICE_SVM_PRE)))
			ice_add_dvm_hint(hw, val, false);

		label_name = ice_enum_labels(NULL, 0, &state, &val);
	}

	 
	for (i = 0; i < hw->tnl.count; i++) {
		ice_find_boost_entry(ice_seg, hw->tnl.tbl[i].boost_addr,
				     &hw->tnl.tbl[i].boost_entry);
		if (hw->tnl.tbl[i].boost_entry) {
			hw->tnl.tbl[i].valid = true;
			if (hw->tnl.tbl[i].type < __TNL_TYPE_CNT)
				hw->tnl.valid_count[hw->tnl.tbl[i].type]++;
		}
	}

	 
	for (i = 0; i < hw->dvm_upd.count; i++)
		ice_find_boost_entry(ice_seg, hw->dvm_upd.tbl[i].boost_addr,
				     &hw->dvm_upd.tbl[i].boost_entry);
}

 
static void ice_fill_hw_ptype(struct ice_hw *hw)
{
	struct ice_marker_ptype_tcam_entry *tcam;
	struct ice_seg *seg = hw->seg;
	struct ice_pkg_enum state;

	bitmap_zero(hw->hw_ptype, ICE_FLOW_PTYPE_MAX);
	if (!seg)
		return;

	memset(&state, 0, sizeof(state));

	do {
		tcam = ice_pkg_enum_entry(seg, &state,
					  ICE_SID_RXPARSER_MARKER_PTYPE, NULL,
					  ice_marker_ptype_tcam_handler);
		if (tcam &&
		    le16_to_cpu(tcam->addr) < ICE_MARKER_PTYPE_TCAM_ADDR_MAX &&
		    le16_to_cpu(tcam->ptype) < ICE_FLOW_PTYPE_MAX)
			set_bit(le16_to_cpu(tcam->ptype), hw->hw_ptype);

		seg = NULL;
	} while (tcam);
}

 
enum ice_ddp_state ice_init_pkg(struct ice_hw *hw, u8 *buf, u32 len)
{
	bool already_loaded = false;
	enum ice_ddp_state state;
	struct ice_pkg_hdr *pkg;
	struct ice_seg *seg;

	if (!buf || !len)
		return ICE_DDP_PKG_ERR;

	pkg = (struct ice_pkg_hdr *)buf;
	state = ice_verify_pkg(pkg, len);
	if (state) {
		ice_debug(hw, ICE_DBG_INIT, "failed to verify pkg (err: %d)\n",
			  state);
		return state;
	}

	 
	state = ice_init_pkg_info(hw, pkg);
	if (state)
		return state;

	 
	state = ice_chk_pkg_compat(hw, pkg, &seg);
	if (state)
		return state;

	 
	ice_init_pkg_hints(hw, seg);
	state = ice_download_pkg(hw, seg);
	if (state == ICE_DDP_PKG_ALREADY_LOADED) {
		ice_debug(hw, ICE_DBG_INIT,
			  "package previously loaded - no work.\n");
		already_loaded = true;
	}

	 
	if (!state || state == ICE_DDP_PKG_ALREADY_LOADED) {
		state = ice_get_pkg_info(hw);
		if (!state)
			state = ice_get_ddp_pkg_state(hw, already_loaded);
	}

	if (ice_is_init_pkg_successful(state)) {
		hw->seg = seg;
		 
		ice_init_pkg_regs(hw);
		ice_fill_blk_tbls(hw);
		ice_fill_hw_ptype(hw);
		ice_get_prof_index_max(hw);
	} else {
		ice_debug(hw, ICE_DBG_INIT, "package load failed, %d\n", state);
	}

	return state;
}

 
enum ice_ddp_state ice_copy_and_init_pkg(struct ice_hw *hw, const u8 *buf,
					 u32 len)
{
	enum ice_ddp_state state;
	u8 *buf_copy;

	if (!buf || !len)
		return ICE_DDP_PKG_ERR;

	buf_copy = devm_kmemdup(ice_hw_to_dev(hw), buf, len, GFP_KERNEL);

	state = ice_init_pkg(hw, buf_copy, len);
	if (!ice_is_init_pkg_successful(state)) {
		 
		devm_kfree(ice_hw_to_dev(hw), buf_copy);
	} else {
		 
		hw->pkg_copy = buf_copy;
		hw->pkg_size = len;
	}

	return state;
}
