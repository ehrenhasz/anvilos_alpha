
 

#include "ice_common.h"
#include "ice_flex_pipe.h"
#include "ice_flow.h"
#include "ice.h"

static const u32 ice_sect_lkup[ICE_BLK_COUNT][ICE_SECT_COUNT] = {
	 
	{
		ICE_SID_XLT0_SW,
		ICE_SID_XLT_KEY_BUILDER_SW,
		ICE_SID_XLT1_SW,
		ICE_SID_XLT2_SW,
		ICE_SID_PROFID_TCAM_SW,
		ICE_SID_PROFID_REDIR_SW,
		ICE_SID_FLD_VEC_SW,
		ICE_SID_CDID_KEY_BUILDER_SW,
		ICE_SID_CDID_REDIR_SW
	},

	 
	{
		ICE_SID_XLT0_ACL,
		ICE_SID_XLT_KEY_BUILDER_ACL,
		ICE_SID_XLT1_ACL,
		ICE_SID_XLT2_ACL,
		ICE_SID_PROFID_TCAM_ACL,
		ICE_SID_PROFID_REDIR_ACL,
		ICE_SID_FLD_VEC_ACL,
		ICE_SID_CDID_KEY_BUILDER_ACL,
		ICE_SID_CDID_REDIR_ACL
	},

	 
	{
		ICE_SID_XLT0_FD,
		ICE_SID_XLT_KEY_BUILDER_FD,
		ICE_SID_XLT1_FD,
		ICE_SID_XLT2_FD,
		ICE_SID_PROFID_TCAM_FD,
		ICE_SID_PROFID_REDIR_FD,
		ICE_SID_FLD_VEC_FD,
		ICE_SID_CDID_KEY_BUILDER_FD,
		ICE_SID_CDID_REDIR_FD
	},

	 
	{
		ICE_SID_XLT0_RSS,
		ICE_SID_XLT_KEY_BUILDER_RSS,
		ICE_SID_XLT1_RSS,
		ICE_SID_XLT2_RSS,
		ICE_SID_PROFID_TCAM_RSS,
		ICE_SID_PROFID_REDIR_RSS,
		ICE_SID_FLD_VEC_RSS,
		ICE_SID_CDID_KEY_BUILDER_RSS,
		ICE_SID_CDID_REDIR_RSS
	},

	 
	{
		ICE_SID_XLT0_PE,
		ICE_SID_XLT_KEY_BUILDER_PE,
		ICE_SID_XLT1_PE,
		ICE_SID_XLT2_PE,
		ICE_SID_PROFID_TCAM_PE,
		ICE_SID_PROFID_REDIR_PE,
		ICE_SID_FLD_VEC_PE,
		ICE_SID_CDID_KEY_BUILDER_PE,
		ICE_SID_CDID_REDIR_PE
	}
};

 
static u32 ice_sect_id(enum ice_block blk, enum ice_sect sect)
{
	return ice_sect_lkup[blk][sect];
}

 
bool ice_hw_ptype_ena(struct ice_hw *hw, u16 ptype)
{
	return ptype < ICE_FLOW_PTYPE_MAX &&
	       test_bit(ptype, hw->hw_ptype);
}

 

#define ICE_DC_KEY	0x1	 
#define ICE_DC_KEYINV	0x1
#define ICE_NM_KEY	0x0	 
#define ICE_NM_KEYINV	0x0
#define ICE_0_KEY	0x1	 
#define ICE_0_KEYINV	0x0
#define ICE_1_KEY	0x0	 
#define ICE_1_KEYINV	0x1

 
static int
ice_gen_key_word(u8 val, u8 valid, u8 dont_care, u8 nvr_mtch, u8 *key,
		 u8 *key_inv)
{
	u8 in_key = *key, in_key_inv = *key_inv;
	u8 i;

	 
	if ((dont_care ^ nvr_mtch) != (dont_care | nvr_mtch))
		return -EIO;

	*key = 0;
	*key_inv = 0;

	 
	for (i = 0; i < 8; i++) {
		*key >>= 1;
		*key_inv >>= 1;

		if (!(valid & 0x1)) {  
			*key |= (in_key & 0x1) << 7;
			*key_inv |= (in_key_inv & 0x1) << 7;
		} else if (dont_care & 0x1) {  
			*key |= ICE_DC_KEY << 7;
			*key_inv |= ICE_DC_KEYINV << 7;
		} else if (nvr_mtch & 0x1) {  
			*key |= ICE_NM_KEY << 7;
			*key_inv |= ICE_NM_KEYINV << 7;
		} else if (val & 0x01) {  
			*key |= ICE_1_KEY << 7;
			*key_inv |= ICE_1_KEYINV << 7;
		} else {  
			*key |= ICE_0_KEY << 7;
			*key_inv |= ICE_0_KEYINV << 7;
		}

		dont_care >>= 1;
		nvr_mtch >>= 1;
		valid >>= 1;
		val >>= 1;
		in_key >>= 1;
		in_key_inv >>= 1;
	}

	return 0;
}

 
static bool ice_bits_max_set(const u8 *mask, u16 size, u16 max)
{
	u16 count = 0;
	u16 i;

	 
	for (i = 0; i < size; i++) {
		 
		if (!mask[i])
			continue;

		 
		if (count == max)
			return false;

		 
		count += hweight8(mask[i]);
		if (count > max)
			return false;
	}

	return true;
}

 
static int
ice_set_key(u8 *key, u16 size, u8 *val, u8 *upd, u8 *dc, u8 *nm, u16 off,
	    u16 len)
{
	u16 half_size;
	u16 i;

	 
	if (size % 2)
		return -EIO;

	half_size = size / 2;
	if (off + len > half_size)
		return -EIO;

	 
#define ICE_NVR_MTCH_BITS_MAX	1
	if (nm && !ice_bits_max_set(nm, len, ICE_NVR_MTCH_BITS_MAX))
		return -EIO;

	for (i = 0; i < len; i++)
		if (ice_gen_key_word(val[i], upd ? upd[i] : 0xff,
				     dc ? dc[i] : 0, nm ? nm[i] : 0,
				     key + off + i, key + half_size + off + i))
			return -EIO;

	return 0;
}

 
int
ice_acquire_change_lock(struct ice_hw *hw, enum ice_aq_res_access_type access)
{
	return ice_acquire_res(hw, ICE_CHANGE_LOCK_RES_ID, access,
			       ICE_CHANGE_LOCK_TIMEOUT);
}

 
void ice_release_change_lock(struct ice_hw *hw)
{
	ice_release_res(hw, ICE_CHANGE_LOCK_RES_ID);
}

 
bool
ice_get_open_tunnel_port(struct ice_hw *hw, u16 *port,
			 enum ice_tunnel_type type)
{
	bool res = false;
	u16 i;

	mutex_lock(&hw->tnl_lock);

	for (i = 0; i < hw->tnl.count && i < ICE_TUNNEL_MAX_ENTRIES; i++)
		if (hw->tnl.tbl[i].valid && hw->tnl.tbl[i].port &&
		    (type == TNL_LAST || type == hw->tnl.tbl[i].type)) {
			*port = hw->tnl.tbl[i].port;
			res = true;
			break;
		}

	mutex_unlock(&hw->tnl_lock);

	return res;
}

 
static int
ice_upd_dvm_boost_entry(struct ice_hw *hw, struct ice_dvm_entry *entry)
{
	struct ice_boost_tcam_section *sect_rx, *sect_tx;
	int status = -ENOSPC;
	struct ice_buf_build *bld;
	u8 val, dc, nm;

	bld = ice_pkg_buf_alloc(hw);
	if (!bld)
		return -ENOMEM;

	 
	if (ice_pkg_buf_reserve_section(bld, 2))
		goto ice_upd_dvm_boost_entry_err;

	sect_rx = ice_pkg_buf_alloc_section(bld, ICE_SID_RXPARSER_BOOST_TCAM,
					    struct_size(sect_rx, tcam, 1));
	if (!sect_rx)
		goto ice_upd_dvm_boost_entry_err;
	sect_rx->count = cpu_to_le16(1);

	sect_tx = ice_pkg_buf_alloc_section(bld, ICE_SID_TXPARSER_BOOST_TCAM,
					    struct_size(sect_tx, tcam, 1));
	if (!sect_tx)
		goto ice_upd_dvm_boost_entry_err;
	sect_tx->count = cpu_to_le16(1);

	 
	memcpy(sect_rx->tcam, entry->boost_entry, sizeof(*sect_rx->tcam));

	 
	if (entry->enable) {
		 
		val = 0x00;
		dc = 0xFF;
		nm = 0x00;
	} else {
		 
		val = 0x00;
		dc = 0xF7;
		nm = 0x08;
	}

	ice_set_key((u8 *)&sect_rx->tcam[0].key, sizeof(sect_rx->tcam[0].key),
		    &val, NULL, &dc, &nm, 0, sizeof(u8));

	 
	memcpy(sect_tx->tcam, sect_rx->tcam, sizeof(*sect_tx->tcam));

	status = ice_update_pkg_no_lock(hw, ice_pkg_buf(bld), 1);

ice_upd_dvm_boost_entry_err:
	ice_pkg_buf_free(hw, bld);

	return status;
}

 
int ice_set_dvm_boost_entries(struct ice_hw *hw)
{
	u16 i;

	for (i = 0; i < hw->dvm_upd.count; i++) {
		int status;

		status = ice_upd_dvm_boost_entry(hw, &hw->dvm_upd.tbl[i]);
		if (status)
			return status;
	}

	return 0;
}

 
static u16 ice_tunnel_idx_to_entry(struct ice_hw *hw, enum ice_tunnel_type type,
				   u16 idx)
{
	u16 i;

	for (i = 0; i < hw->tnl.count && i < ICE_TUNNEL_MAX_ENTRIES; i++)
		if (hw->tnl.tbl[i].valid &&
		    hw->tnl.tbl[i].type == type &&
		    idx-- == 0)
			return i;

	WARN_ON_ONCE(1);
	return 0;
}

 
static int
ice_create_tunnel(struct ice_hw *hw, u16 index,
		  enum ice_tunnel_type type, u16 port)
{
	struct ice_boost_tcam_section *sect_rx, *sect_tx;
	struct ice_buf_build *bld;
	int status = -ENOSPC;

	mutex_lock(&hw->tnl_lock);

	bld = ice_pkg_buf_alloc(hw);
	if (!bld) {
		status = -ENOMEM;
		goto ice_create_tunnel_end;
	}

	 
	if (ice_pkg_buf_reserve_section(bld, 2))
		goto ice_create_tunnel_err;

	sect_rx = ice_pkg_buf_alloc_section(bld, ICE_SID_RXPARSER_BOOST_TCAM,
					    struct_size(sect_rx, tcam, 1));
	if (!sect_rx)
		goto ice_create_tunnel_err;
	sect_rx->count = cpu_to_le16(1);

	sect_tx = ice_pkg_buf_alloc_section(bld, ICE_SID_TXPARSER_BOOST_TCAM,
					    struct_size(sect_tx, tcam, 1));
	if (!sect_tx)
		goto ice_create_tunnel_err;
	sect_tx->count = cpu_to_le16(1);

	 
	memcpy(sect_rx->tcam, hw->tnl.tbl[index].boost_entry,
	       sizeof(*sect_rx->tcam));

	 
	ice_set_key((u8 *)&sect_rx->tcam[0].key, sizeof(sect_rx->tcam[0].key),
		    (u8 *)&port, NULL, NULL, NULL,
		    (u16)offsetof(struct ice_boost_key_value, hv_dst_port_key),
		    sizeof(sect_rx->tcam[0].key.key.hv_dst_port_key));

	 
	memcpy(sect_tx->tcam, sect_rx->tcam, sizeof(*sect_tx->tcam));

	status = ice_update_pkg(hw, ice_pkg_buf(bld), 1);
	if (!status)
		hw->tnl.tbl[index].port = port;

ice_create_tunnel_err:
	ice_pkg_buf_free(hw, bld);

ice_create_tunnel_end:
	mutex_unlock(&hw->tnl_lock);

	return status;
}

 
static int
ice_destroy_tunnel(struct ice_hw *hw, u16 index, enum ice_tunnel_type type,
		   u16 port)
{
	struct ice_boost_tcam_section *sect_rx, *sect_tx;
	struct ice_buf_build *bld;
	int status = -ENOSPC;

	mutex_lock(&hw->tnl_lock);

	if (WARN_ON(!hw->tnl.tbl[index].valid ||
		    hw->tnl.tbl[index].type != type ||
		    hw->tnl.tbl[index].port != port)) {
		status = -EIO;
		goto ice_destroy_tunnel_end;
	}

	bld = ice_pkg_buf_alloc(hw);
	if (!bld) {
		status = -ENOMEM;
		goto ice_destroy_tunnel_end;
	}

	 
	if (ice_pkg_buf_reserve_section(bld, 2))
		goto ice_destroy_tunnel_err;

	sect_rx = ice_pkg_buf_alloc_section(bld, ICE_SID_RXPARSER_BOOST_TCAM,
					    struct_size(sect_rx, tcam, 1));
	if (!sect_rx)
		goto ice_destroy_tunnel_err;
	sect_rx->count = cpu_to_le16(1);

	sect_tx = ice_pkg_buf_alloc_section(bld, ICE_SID_TXPARSER_BOOST_TCAM,
					    struct_size(sect_tx, tcam, 1));
	if (!sect_tx)
		goto ice_destroy_tunnel_err;
	sect_tx->count = cpu_to_le16(1);

	 
	memcpy(sect_rx->tcam, hw->tnl.tbl[index].boost_entry,
	       sizeof(*sect_rx->tcam));
	memcpy(sect_tx->tcam, hw->tnl.tbl[index].boost_entry,
	       sizeof(*sect_tx->tcam));

	status = ice_update_pkg(hw, ice_pkg_buf(bld), 1);
	if (!status)
		hw->tnl.tbl[index].port = 0;

ice_destroy_tunnel_err:
	ice_pkg_buf_free(hw, bld);

ice_destroy_tunnel_end:
	mutex_unlock(&hw->tnl_lock);

	return status;
}

int ice_udp_tunnel_set_port(struct net_device *netdev, unsigned int table,
			    unsigned int idx, struct udp_tunnel_info *ti)
{
	struct ice_netdev_priv *np = netdev_priv(netdev);
	struct ice_vsi *vsi = np->vsi;
	struct ice_pf *pf = vsi->back;
	enum ice_tunnel_type tnl_type;
	int status;
	u16 index;

	tnl_type = ti->type == UDP_TUNNEL_TYPE_VXLAN ? TNL_VXLAN : TNL_GENEVE;
	index = ice_tunnel_idx_to_entry(&pf->hw, tnl_type, idx);

	status = ice_create_tunnel(&pf->hw, index, tnl_type, ntohs(ti->port));
	if (status) {
		netdev_err(netdev, "Error adding UDP tunnel - %d\n",
			   status);
		return -EIO;
	}

	udp_tunnel_nic_set_port_priv(netdev, table, idx, index);
	return 0;
}

int ice_udp_tunnel_unset_port(struct net_device *netdev, unsigned int table,
			      unsigned int idx, struct udp_tunnel_info *ti)
{
	struct ice_netdev_priv *np = netdev_priv(netdev);
	struct ice_vsi *vsi = np->vsi;
	struct ice_pf *pf = vsi->back;
	enum ice_tunnel_type tnl_type;
	int status;

	tnl_type = ti->type == UDP_TUNNEL_TYPE_VXLAN ? TNL_VXLAN : TNL_GENEVE;

	status = ice_destroy_tunnel(&pf->hw, ti->hw_priv, tnl_type,
				    ntohs(ti->port));
	if (status) {
		netdev_err(netdev, "Error removing UDP tunnel - %d\n",
			   status);
		return -EIO;
	}

	return 0;
}

 
int
ice_find_prot_off(struct ice_hw *hw, enum ice_block blk, u8 prof, u16 fv_idx,
		  u8 *prot, u16 *off)
{
	struct ice_fv_word *fv_ext;

	if (prof >= hw->blk[blk].es.count)
		return -EINVAL;

	if (fv_idx >= hw->blk[blk].es.fvw)
		return -EINVAL;

	fv_ext = hw->blk[blk].es.t + (prof * hw->blk[blk].es.fvw);

	*prot = fv_ext[fv_idx].prot_id;
	*off = fv_ext[fv_idx].off;

	return 0;
}

 

 
static int
ice_ptg_find_ptype(struct ice_hw *hw, enum ice_block blk, u16 ptype, u8 *ptg)
{
	if (ptype >= ICE_XLT1_CNT || !ptg)
		return -EINVAL;

	*ptg = hw->blk[blk].xlt1.ptypes[ptype].ptg;
	return 0;
}

 
static void ice_ptg_alloc_val(struct ice_hw *hw, enum ice_block blk, u8 ptg)
{
	hw->blk[blk].xlt1.ptg_tbl[ptg].in_use = true;
}

 
static int
ice_ptg_remove_ptype(struct ice_hw *hw, enum ice_block blk, u16 ptype, u8 ptg)
{
	struct ice_ptg_ptype **ch;
	struct ice_ptg_ptype *p;

	if (ptype > ICE_XLT1_CNT - 1)
		return -EINVAL;

	if (!hw->blk[blk].xlt1.ptg_tbl[ptg].in_use)
		return -ENOENT;

	 
	if (!hw->blk[blk].xlt1.ptg_tbl[ptg].first_ptype)
		return -EIO;

	 
	p = hw->blk[blk].xlt1.ptg_tbl[ptg].first_ptype;
	ch = &hw->blk[blk].xlt1.ptg_tbl[ptg].first_ptype;
	while (p) {
		if (ptype == (p - hw->blk[blk].xlt1.ptypes)) {
			*ch = p->next_ptype;
			break;
		}

		ch = &p->next_ptype;
		p = p->next_ptype;
	}

	hw->blk[blk].xlt1.ptypes[ptype].ptg = ICE_DEFAULT_PTG;
	hw->blk[blk].xlt1.ptypes[ptype].next_ptype = NULL;

	return 0;
}

 
static int
ice_ptg_add_mv_ptype(struct ice_hw *hw, enum ice_block blk, u16 ptype, u8 ptg)
{
	u8 original_ptg;
	int status;

	if (ptype > ICE_XLT1_CNT - 1)
		return -EINVAL;

	if (!hw->blk[blk].xlt1.ptg_tbl[ptg].in_use && ptg != ICE_DEFAULT_PTG)
		return -ENOENT;

	status = ice_ptg_find_ptype(hw, blk, ptype, &original_ptg);
	if (status)
		return status;

	 
	if (original_ptg == ptg)
		return 0;

	 
	if (original_ptg != ICE_DEFAULT_PTG)
		ice_ptg_remove_ptype(hw, blk, ptype, original_ptg);

	 
	if (ptg == ICE_DEFAULT_PTG)
		return 0;

	 
	hw->blk[blk].xlt1.ptypes[ptype].next_ptype =
		hw->blk[blk].xlt1.ptg_tbl[ptg].first_ptype;
	hw->blk[blk].xlt1.ptg_tbl[ptg].first_ptype =
		&hw->blk[blk].xlt1.ptypes[ptype];

	hw->blk[blk].xlt1.ptypes[ptype].ptg = ptg;
	hw->blk[blk].xlt1.t[ptype] = ptg;

	return 0;
}

 
struct ice_blk_size_details {
	u16 xlt1;			 
	u16 xlt2;			 
	u16 prof_tcam;			 
	u16 prof_id;			 
	u8 prof_cdid_bits;		 
	u16 prof_redir;			 
	u16 es;				 
	u16 fvw;			 
	u8 overwrite;			 
	u8 reverse;			 
};

static const struct ice_blk_size_details blk_sizes[ICE_BLK_COUNT] = {
	 
	 
	 
	  { ICE_XLT1_CNT, ICE_XLT2_CNT, 512, 256,   0,  256, 256,  48,
		    false, false },
	  { ICE_XLT1_CNT, ICE_XLT2_CNT, 512, 128,   0,  128, 128,  32,
		    false, false },
	  { ICE_XLT1_CNT, ICE_XLT2_CNT, 512, 128,   0,  128, 128,  24,
		    false, true  },
	  { ICE_XLT1_CNT, ICE_XLT2_CNT, 512, 128,   0,  128, 128,  24,
		    true,  true  },
	  { ICE_XLT1_CNT, ICE_XLT2_CNT,  64,  32,   0,   32,  32,  24,
		    false, false },
};

enum ice_sid_all {
	ICE_SID_XLT1_OFF = 0,
	ICE_SID_XLT2_OFF,
	ICE_SID_PR_OFF,
	ICE_SID_PR_REDIR_OFF,
	ICE_SID_ES_OFF,
	ICE_SID_OFF_COUNT,
};

 

 
static bool
ice_match_prop_lst(struct list_head *list1, struct list_head *list2)
{
	struct ice_vsig_prof *tmp1;
	struct ice_vsig_prof *tmp2;
	u16 chk_count = 0;
	u16 count = 0;

	 
	list_for_each_entry(tmp1, list1, list)
		count++;
	list_for_each_entry(tmp2, list2, list)
		chk_count++;
	if (!count || count != chk_count)
		return false;

	tmp1 = list_first_entry(list1, struct ice_vsig_prof, list);
	tmp2 = list_first_entry(list2, struct ice_vsig_prof, list);

	 
	while (count--) {
		if (tmp2->profile_cookie != tmp1->profile_cookie)
			return false;

		tmp1 = list_next_entry(tmp1, list);
		tmp2 = list_next_entry(tmp2, list);
	}

	return true;
}

 

 
static int
ice_vsig_find_vsi(struct ice_hw *hw, enum ice_block blk, u16 vsi, u16 *vsig)
{
	if (!vsig || vsi >= ICE_MAX_VSI)
		return -EINVAL;

	 
	*vsig = hw->blk[blk].xlt2.vsis[vsi].vsig;

	return 0;
}

 
static u16 ice_vsig_alloc_val(struct ice_hw *hw, enum ice_block blk, u16 vsig)
{
	u16 idx = vsig & ICE_VSIG_IDX_M;

	if (!hw->blk[blk].xlt2.vsig_tbl[idx].in_use) {
		INIT_LIST_HEAD(&hw->blk[blk].xlt2.vsig_tbl[idx].prop_lst);
		hw->blk[blk].xlt2.vsig_tbl[idx].in_use = true;
	}

	return ICE_VSIG_VALUE(idx, hw->pf_id);
}

 
static u16 ice_vsig_alloc(struct ice_hw *hw, enum ice_block blk)
{
	u16 i;

	for (i = 1; i < ICE_MAX_VSIGS; i++)
		if (!hw->blk[blk].xlt2.vsig_tbl[i].in_use)
			return ice_vsig_alloc_val(hw, blk, i);

	return ICE_DEFAULT_VSIG;
}

 
static int
ice_find_dup_props_vsig(struct ice_hw *hw, enum ice_block blk,
			struct list_head *chs, u16 *vsig)
{
	struct ice_xlt2 *xlt2 = &hw->blk[blk].xlt2;
	u16 i;

	for (i = 0; i < xlt2->count; i++)
		if (xlt2->vsig_tbl[i].in_use &&
		    ice_match_prop_lst(chs, &xlt2->vsig_tbl[i].prop_lst)) {
			*vsig = ICE_VSIG_VALUE(i, hw->pf_id);
			return 0;
		}

	return -ENOENT;
}

 
static int ice_vsig_free(struct ice_hw *hw, enum ice_block blk, u16 vsig)
{
	struct ice_vsig_prof *dtmp, *del;
	struct ice_vsig_vsi *vsi_cur;
	u16 idx;

	idx = vsig & ICE_VSIG_IDX_M;
	if (idx >= ICE_MAX_VSIGS)
		return -EINVAL;

	if (!hw->blk[blk].xlt2.vsig_tbl[idx].in_use)
		return -ENOENT;

	hw->blk[blk].xlt2.vsig_tbl[idx].in_use = false;

	vsi_cur = hw->blk[blk].xlt2.vsig_tbl[idx].first_vsi;
	 
	if (vsi_cur) {
		 
		do {
			struct ice_vsig_vsi *tmp = vsi_cur->next_vsi;

			vsi_cur->vsig = ICE_DEFAULT_VSIG;
			vsi_cur->changed = 1;
			vsi_cur->next_vsi = NULL;
			vsi_cur = tmp;
		} while (vsi_cur);

		 
		hw->blk[blk].xlt2.vsig_tbl[idx].first_vsi = NULL;
	}

	 
	list_for_each_entry_safe(del, dtmp,
				 &hw->blk[blk].xlt2.vsig_tbl[idx].prop_lst,
				 list) {
		list_del(&del->list);
		devm_kfree(ice_hw_to_dev(hw), del);
	}

	 
	INIT_LIST_HEAD(&hw->blk[blk].xlt2.vsig_tbl[idx].prop_lst);

	return 0;
}

 
static int
ice_vsig_remove_vsi(struct ice_hw *hw, enum ice_block blk, u16 vsi, u16 vsig)
{
	struct ice_vsig_vsi **vsi_head, *vsi_cur, *vsi_tgt;
	u16 idx;

	idx = vsig & ICE_VSIG_IDX_M;

	if (vsi >= ICE_MAX_VSI || idx >= ICE_MAX_VSIGS)
		return -EINVAL;

	if (!hw->blk[blk].xlt2.vsig_tbl[idx].in_use)
		return -ENOENT;

	 
	if (idx == ICE_DEFAULT_VSIG)
		return 0;

	vsi_head = &hw->blk[blk].xlt2.vsig_tbl[idx].first_vsi;
	if (!(*vsi_head))
		return -EIO;

	vsi_tgt = &hw->blk[blk].xlt2.vsis[vsi];
	vsi_cur = (*vsi_head);

	 
	while (vsi_cur) {
		if (vsi_tgt == vsi_cur) {
			(*vsi_head) = vsi_cur->next_vsi;
			break;
		}
		vsi_head = &vsi_cur->next_vsi;
		vsi_cur = vsi_cur->next_vsi;
	}

	 
	if (!vsi_cur)
		return -ENOENT;

	vsi_cur->vsig = ICE_DEFAULT_VSIG;
	vsi_cur->changed = 1;
	vsi_cur->next_vsi = NULL;

	return 0;
}

 
static int
ice_vsig_add_mv_vsi(struct ice_hw *hw, enum ice_block blk, u16 vsi, u16 vsig)
{
	struct ice_vsig_vsi *tmp;
	u16 orig_vsig, idx;
	int status;

	idx = vsig & ICE_VSIG_IDX_M;

	if (vsi >= ICE_MAX_VSI || idx >= ICE_MAX_VSIGS)
		return -EINVAL;

	 
	if (!hw->blk[blk].xlt2.vsig_tbl[idx].in_use &&
	    vsig != ICE_DEFAULT_VSIG)
		return -ENOENT;

	status = ice_vsig_find_vsi(hw, blk, vsi, &orig_vsig);
	if (status)
		return status;

	 
	if (orig_vsig == vsig)
		return 0;

	if (orig_vsig != ICE_DEFAULT_VSIG) {
		 
		status = ice_vsig_remove_vsi(hw, blk, vsi, orig_vsig);
		if (status)
			return status;
	}

	if (idx == ICE_DEFAULT_VSIG)
		return 0;

	 
	hw->blk[blk].xlt2.vsis[vsi].vsig = vsig;
	hw->blk[blk].xlt2.vsis[vsi].changed = 1;

	 
	tmp = hw->blk[blk].xlt2.vsig_tbl[idx].first_vsi;
	hw->blk[blk].xlt2.vsig_tbl[idx].first_vsi =
		&hw->blk[blk].xlt2.vsis[vsi];
	hw->blk[blk].xlt2.vsis[vsi].next_vsi = tmp;
	hw->blk[blk].xlt2.t[vsi] = vsig;

	return 0;
}

 
static bool
ice_prof_has_mask_idx(struct ice_hw *hw, enum ice_block blk, u8 prof, u16 idx,
		      u16 mask)
{
	bool expect_no_mask = false;
	bool found = false;
	bool match = false;
	u16 i;

	 
	if (mask == 0 || mask == 0xffff)
		expect_no_mask = true;

	 
	for (i = hw->blk[blk].masks.first; i < hw->blk[blk].masks.first +
	     hw->blk[blk].masks.count; i++)
		if (hw->blk[blk].es.mask_ena[prof] & BIT(i))
			if (hw->blk[blk].masks.masks[i].in_use &&
			    hw->blk[blk].masks.masks[i].idx == idx) {
				found = true;
				if (hw->blk[blk].masks.masks[i].mask == mask)
					match = true;
				break;
			}

	if (expect_no_mask) {
		if (found)
			return false;
	} else {
		if (!match)
			return false;
	}

	return true;
}

 
static bool
ice_prof_has_mask(struct ice_hw *hw, enum ice_block blk, u8 prof, u16 *masks)
{
	u16 i;

	 
	for (i = 0; i < hw->blk[blk].es.fvw; i++)
		if (!ice_prof_has_mask_idx(hw, blk, prof, i, masks[i]))
			return false;

	return true;
}

 
static int
ice_find_prof_id_with_mask(struct ice_hw *hw, enum ice_block blk,
			   struct ice_fv_word *fv, u16 *masks, u8 *prof_id)
{
	struct ice_es *es = &hw->blk[blk].es;
	u8 i;

	 
	if (blk == ICE_BLK_FD)
		return -ENOENT;

	for (i = 0; i < (u8)es->count; i++) {
		u16 off = i * es->fvw;

		if (memcmp(&es->t[off], fv, es->fvw * sizeof(*fv)))
			continue;

		 
		if (masks && !ice_prof_has_mask(hw, blk, i, masks))
			continue;

		*prof_id = i;
		return 0;
	}

	return -ENOENT;
}

 
static bool ice_prof_id_rsrc_type(enum ice_block blk, u16 *rsrc_type)
{
	switch (blk) {
	case ICE_BLK_FD:
		*rsrc_type = ICE_AQC_RES_TYPE_FD_PROF_BLDR_PROFID;
		break;
	case ICE_BLK_RSS:
		*rsrc_type = ICE_AQC_RES_TYPE_HASH_PROF_BLDR_PROFID;
		break;
	default:
		return false;
	}
	return true;
}

 
static bool ice_tcam_ent_rsrc_type(enum ice_block blk, u16 *rsrc_type)
{
	switch (blk) {
	case ICE_BLK_FD:
		*rsrc_type = ICE_AQC_RES_TYPE_FD_PROF_BLDR_TCAM;
		break;
	case ICE_BLK_RSS:
		*rsrc_type = ICE_AQC_RES_TYPE_HASH_PROF_BLDR_TCAM;
		break;
	default:
		return false;
	}
	return true;
}

 
static int
ice_alloc_tcam_ent(struct ice_hw *hw, enum ice_block blk, bool btm,
		   u16 *tcam_idx)
{
	u16 res_type;

	if (!ice_tcam_ent_rsrc_type(blk, &res_type))
		return -EINVAL;

	return ice_alloc_hw_res(hw, res_type, 1, btm, tcam_idx);
}

 
static int
ice_free_tcam_ent(struct ice_hw *hw, enum ice_block blk, u16 tcam_idx)
{
	u16 res_type;

	if (!ice_tcam_ent_rsrc_type(blk, &res_type))
		return -EINVAL;

	return ice_free_hw_res(hw, res_type, 1, &tcam_idx);
}

 
static int ice_alloc_prof_id(struct ice_hw *hw, enum ice_block blk, u8 *prof_id)
{
	u16 res_type;
	u16 get_prof;
	int status;

	if (!ice_prof_id_rsrc_type(blk, &res_type))
		return -EINVAL;

	status = ice_alloc_hw_res(hw, res_type, 1, false, &get_prof);
	if (!status)
		*prof_id = (u8)get_prof;

	return status;
}

 
static int ice_free_prof_id(struct ice_hw *hw, enum ice_block blk, u8 prof_id)
{
	u16 tmp_prof_id = (u16)prof_id;
	u16 res_type;

	if (!ice_prof_id_rsrc_type(blk, &res_type))
		return -EINVAL;

	return ice_free_hw_res(hw, res_type, 1, &tmp_prof_id);
}

 
static int ice_prof_inc_ref(struct ice_hw *hw, enum ice_block blk, u8 prof_id)
{
	if (prof_id > hw->blk[blk].es.count)
		return -EINVAL;

	hw->blk[blk].es.ref_count[prof_id]++;

	return 0;
}

 
static void
ice_write_prof_mask_reg(struct ice_hw *hw, enum ice_block blk, u16 mask_idx,
			u16 idx, u16 mask)
{
	u32 offset;
	u32 val;

	switch (blk) {
	case ICE_BLK_RSS:
		offset = GLQF_HMASK(mask_idx);
		val = (idx << GLQF_HMASK_MSK_INDEX_S) & GLQF_HMASK_MSK_INDEX_M;
		val |= (mask << GLQF_HMASK_MASK_S) & GLQF_HMASK_MASK_M;
		break;
	case ICE_BLK_FD:
		offset = GLQF_FDMASK(mask_idx);
		val = (idx << GLQF_FDMASK_MSK_INDEX_S) & GLQF_FDMASK_MSK_INDEX_M;
		val |= (mask << GLQF_FDMASK_MASK_S) & GLQF_FDMASK_MASK_M;
		break;
	default:
		ice_debug(hw, ICE_DBG_PKG, "No profile masks for block %d\n",
			  blk);
		return;
	}

	wr32(hw, offset, val);
	ice_debug(hw, ICE_DBG_PKG, "write mask, blk %d (%d): %x = %x\n",
		  blk, idx, offset, val);
}

 
static void
ice_write_prof_mask_enable_res(struct ice_hw *hw, enum ice_block blk,
			       u16 prof_id, u32 enable_mask)
{
	u32 offset;

	switch (blk) {
	case ICE_BLK_RSS:
		offset = GLQF_HMASK_SEL(prof_id);
		break;
	case ICE_BLK_FD:
		offset = GLQF_FDMASK_SEL(prof_id);
		break;
	default:
		ice_debug(hw, ICE_DBG_PKG, "No profile masks for block %d\n",
			  blk);
		return;
	}

	wr32(hw, offset, enable_mask);
	ice_debug(hw, ICE_DBG_PKG, "write mask enable, blk %d (%d): %x = %x\n",
		  blk, prof_id, offset, enable_mask);
}

 
static void ice_init_prof_masks(struct ice_hw *hw, enum ice_block blk)
{
	u16 per_pf;
	u16 i;

	mutex_init(&hw->blk[blk].masks.lock);

	per_pf = ICE_PROF_MASK_COUNT / hw->dev_caps.num_funcs;

	hw->blk[blk].masks.count = per_pf;
	hw->blk[blk].masks.first = hw->pf_id * per_pf;

	memset(hw->blk[blk].masks.masks, 0, sizeof(hw->blk[blk].masks.masks));

	for (i = hw->blk[blk].masks.first;
	     i < hw->blk[blk].masks.first + hw->blk[blk].masks.count; i++)
		ice_write_prof_mask_reg(hw, blk, i, 0, 0);
}

 
static void ice_init_all_prof_masks(struct ice_hw *hw)
{
	ice_init_prof_masks(hw, ICE_BLK_RSS);
	ice_init_prof_masks(hw, ICE_BLK_FD);
}

 
static int
ice_alloc_prof_mask(struct ice_hw *hw, enum ice_block blk, u16 idx, u16 mask,
		    u16 *mask_idx)
{
	bool found_unused = false, found_copy = false;
	u16 unused_idx = 0, copy_idx = 0;
	int status = -ENOSPC;
	u16 i;

	if (blk != ICE_BLK_RSS && blk != ICE_BLK_FD)
		return -EINVAL;

	mutex_lock(&hw->blk[blk].masks.lock);

	for (i = hw->blk[blk].masks.first;
	     i < hw->blk[blk].masks.first + hw->blk[blk].masks.count; i++)
		if (hw->blk[blk].masks.masks[i].in_use) {
			 
			if (hw->blk[blk].masks.masks[i].mask == mask &&
			    hw->blk[blk].masks.masks[i].idx == idx) {
				found_copy = true;
				copy_idx = i;
				break;
			}
		} else {
			 
			if (!found_unused) {
				found_unused = true;
				unused_idx = i;
			}
		}

	if (found_copy)
		i = copy_idx;
	else if (found_unused)
		i = unused_idx;
	else
		goto err_ice_alloc_prof_mask;

	 
	if (found_unused) {
		hw->blk[blk].masks.masks[i].in_use = true;
		hw->blk[blk].masks.masks[i].mask = mask;
		hw->blk[blk].masks.masks[i].idx = idx;
		hw->blk[blk].masks.masks[i].ref = 0;
		ice_write_prof_mask_reg(hw, blk, i, idx, mask);
	}

	hw->blk[blk].masks.masks[i].ref++;
	*mask_idx = i;
	status = 0;

err_ice_alloc_prof_mask:
	mutex_unlock(&hw->blk[blk].masks.lock);

	return status;
}

 
static int
ice_free_prof_mask(struct ice_hw *hw, enum ice_block blk, u16 mask_idx)
{
	if (blk != ICE_BLK_RSS && blk != ICE_BLK_FD)
		return -EINVAL;

	if (!(mask_idx >= hw->blk[blk].masks.first &&
	      mask_idx < hw->blk[blk].masks.first + hw->blk[blk].masks.count))
		return -ENOENT;

	mutex_lock(&hw->blk[blk].masks.lock);

	if (!hw->blk[blk].masks.masks[mask_idx].in_use)
		goto exit_ice_free_prof_mask;

	if (hw->blk[blk].masks.masks[mask_idx].ref > 1) {
		hw->blk[blk].masks.masks[mask_idx].ref--;
		goto exit_ice_free_prof_mask;
	}

	 
	hw->blk[blk].masks.masks[mask_idx].in_use = false;
	hw->blk[blk].masks.masks[mask_idx].mask = 0;
	hw->blk[blk].masks.masks[mask_idx].idx = 0;

	 
	ice_debug(hw, ICE_DBG_PKG, "Free mask, blk %d, mask %d\n", blk,
		  mask_idx);
	ice_write_prof_mask_reg(hw, blk, mask_idx, 0, 0);

exit_ice_free_prof_mask:
	mutex_unlock(&hw->blk[blk].masks.lock);

	return 0;
}

 
static int
ice_free_prof_masks(struct ice_hw *hw, enum ice_block blk, u16 prof_id)
{
	u32 mask_bm;
	u16 i;

	if (blk != ICE_BLK_RSS && blk != ICE_BLK_FD)
		return -EINVAL;

	mask_bm = hw->blk[blk].es.mask_ena[prof_id];
	for (i = 0; i < BITS_PER_BYTE * sizeof(mask_bm); i++)
		if (mask_bm & BIT(i))
			ice_free_prof_mask(hw, blk, i);

	return 0;
}

 
static void ice_shutdown_prof_masks(struct ice_hw *hw, enum ice_block blk)
{
	u16 i;

	mutex_lock(&hw->blk[blk].masks.lock);

	for (i = hw->blk[blk].masks.first;
	     i < hw->blk[blk].masks.first + hw->blk[blk].masks.count; i++) {
		ice_write_prof_mask_reg(hw, blk, i, 0, 0);

		hw->blk[blk].masks.masks[i].in_use = false;
		hw->blk[blk].masks.masks[i].idx = 0;
		hw->blk[blk].masks.masks[i].mask = 0;
	}

	mutex_unlock(&hw->blk[blk].masks.lock);
	mutex_destroy(&hw->blk[blk].masks.lock);
}

 
static void ice_shutdown_all_prof_masks(struct ice_hw *hw)
{
	ice_shutdown_prof_masks(hw, ICE_BLK_RSS);
	ice_shutdown_prof_masks(hw, ICE_BLK_FD);
}

 
static int
ice_update_prof_masking(struct ice_hw *hw, enum ice_block blk, u16 prof_id,
			u16 *masks)
{
	bool err = false;
	u32 ena_mask = 0;
	u16 idx;
	u16 i;

	 
	if (blk != ICE_BLK_RSS && blk != ICE_BLK_FD)
		return 0;

	for (i = 0; i < hw->blk[blk].es.fvw; i++)
		if (masks[i] && masks[i] != 0xFFFF) {
			if (!ice_alloc_prof_mask(hw, blk, i, masks[i], &idx)) {
				ena_mask |= BIT(idx);
			} else {
				 
				err = true;
				break;
			}
		}

	if (err) {
		 
		for (i = 0; i < BITS_PER_BYTE * sizeof(ena_mask); i++)
			if (ena_mask & BIT(i))
				ice_free_prof_mask(hw, blk, i);

		return -EIO;
	}

	 
	ice_write_prof_mask_enable_res(hw, blk, prof_id, ena_mask);

	 
	hw->blk[blk].es.mask_ena[prof_id] = ena_mask;

	return 0;
}

 
static void
ice_write_es(struct ice_hw *hw, enum ice_block blk, u8 prof_id,
	     struct ice_fv_word *fv)
{
	u16 off;

	off = prof_id * hw->blk[blk].es.fvw;
	if (!fv) {
		memset(&hw->blk[blk].es.t[off], 0,
		       hw->blk[blk].es.fvw * sizeof(*fv));
		hw->blk[blk].es.written[prof_id] = false;
	} else {
		memcpy(&hw->blk[blk].es.t[off], fv,
		       hw->blk[blk].es.fvw * sizeof(*fv));
	}
}

 
static int
ice_prof_dec_ref(struct ice_hw *hw, enum ice_block blk, u8 prof_id)
{
	if (prof_id > hw->blk[blk].es.count)
		return -EINVAL;

	if (hw->blk[blk].es.ref_count[prof_id] > 0) {
		if (!--hw->blk[blk].es.ref_count[prof_id]) {
			ice_write_es(hw, blk, prof_id, NULL);
			ice_free_prof_masks(hw, blk, prof_id);
			return ice_free_prof_id(hw, blk, prof_id);
		}
	}

	return 0;
}

 
static const u32 ice_blk_sids[ICE_BLK_COUNT][ICE_SID_OFF_COUNT] = {
	 
	{	ICE_SID_XLT1_SW,
		ICE_SID_XLT2_SW,
		ICE_SID_PROFID_TCAM_SW,
		ICE_SID_PROFID_REDIR_SW,
		ICE_SID_FLD_VEC_SW
	},

	 
	{	ICE_SID_XLT1_ACL,
		ICE_SID_XLT2_ACL,
		ICE_SID_PROFID_TCAM_ACL,
		ICE_SID_PROFID_REDIR_ACL,
		ICE_SID_FLD_VEC_ACL
	},

	 
	{	ICE_SID_XLT1_FD,
		ICE_SID_XLT2_FD,
		ICE_SID_PROFID_TCAM_FD,
		ICE_SID_PROFID_REDIR_FD,
		ICE_SID_FLD_VEC_FD
	},

	 
	{	ICE_SID_XLT1_RSS,
		ICE_SID_XLT2_RSS,
		ICE_SID_PROFID_TCAM_RSS,
		ICE_SID_PROFID_REDIR_RSS,
		ICE_SID_FLD_VEC_RSS
	},

	 
	{	ICE_SID_XLT1_PE,
		ICE_SID_XLT2_PE,
		ICE_SID_PROFID_TCAM_PE,
		ICE_SID_PROFID_REDIR_PE,
		ICE_SID_FLD_VEC_PE
	}
};

 
static void ice_init_sw_xlt1_db(struct ice_hw *hw, enum ice_block blk)
{
	u16 pt;

	for (pt = 0; pt < hw->blk[blk].xlt1.count; pt++) {
		u8 ptg;

		ptg = hw->blk[blk].xlt1.t[pt];
		if (ptg != ICE_DEFAULT_PTG) {
			ice_ptg_alloc_val(hw, blk, ptg);
			ice_ptg_add_mv_ptype(hw, blk, pt, ptg);
		}
	}
}

 
static void ice_init_sw_xlt2_db(struct ice_hw *hw, enum ice_block blk)
{
	u16 vsi;

	for (vsi = 0; vsi < hw->blk[blk].xlt2.count; vsi++) {
		u16 vsig;

		vsig = hw->blk[blk].xlt2.t[vsi];
		if (vsig) {
			ice_vsig_alloc_val(hw, blk, vsig);
			ice_vsig_add_mv_vsi(hw, blk, vsi, vsig);
			 
			hw->blk[blk].xlt2.vsis[vsi].changed = 0;
		}
	}
}

 
static void ice_init_sw_db(struct ice_hw *hw)
{
	u16 i;

	for (i = 0; i < ICE_BLK_COUNT; i++) {
		ice_init_sw_xlt1_db(hw, (enum ice_block)i);
		ice_init_sw_xlt2_db(hw, (enum ice_block)i);
	}
}

 
static void ice_fill_tbl(struct ice_hw *hw, enum ice_block block_id, u32 sid)
{
	u32 dst_len, sect_len, offset = 0;
	struct ice_prof_redir_section *pr;
	struct ice_prof_id_section *pid;
	struct ice_xlt1_section *xlt1;
	struct ice_xlt2_section *xlt2;
	struct ice_sw_fv_section *es;
	struct ice_pkg_enum state;
	u8 *src, *dst;
	void *sect;

	 
	if (!hw->seg) {
		ice_debug(hw, ICE_DBG_PKG, "hw->seg is NULL, tables are not filled\n");
		return;
	}

	memset(&state, 0, sizeof(state));

	sect = ice_pkg_enum_section(hw->seg, &state, sid);

	while (sect) {
		switch (sid) {
		case ICE_SID_XLT1_SW:
		case ICE_SID_XLT1_FD:
		case ICE_SID_XLT1_RSS:
		case ICE_SID_XLT1_ACL:
		case ICE_SID_XLT1_PE:
			xlt1 = sect;
			src = xlt1->value;
			sect_len = le16_to_cpu(xlt1->count) *
				sizeof(*hw->blk[block_id].xlt1.t);
			dst = hw->blk[block_id].xlt1.t;
			dst_len = hw->blk[block_id].xlt1.count *
				sizeof(*hw->blk[block_id].xlt1.t);
			break;
		case ICE_SID_XLT2_SW:
		case ICE_SID_XLT2_FD:
		case ICE_SID_XLT2_RSS:
		case ICE_SID_XLT2_ACL:
		case ICE_SID_XLT2_PE:
			xlt2 = sect;
			src = (__force u8 *)xlt2->value;
			sect_len = le16_to_cpu(xlt2->count) *
				sizeof(*hw->blk[block_id].xlt2.t);
			dst = (u8 *)hw->blk[block_id].xlt2.t;
			dst_len = hw->blk[block_id].xlt2.count *
				sizeof(*hw->blk[block_id].xlt2.t);
			break;
		case ICE_SID_PROFID_TCAM_SW:
		case ICE_SID_PROFID_TCAM_FD:
		case ICE_SID_PROFID_TCAM_RSS:
		case ICE_SID_PROFID_TCAM_ACL:
		case ICE_SID_PROFID_TCAM_PE:
			pid = sect;
			src = (u8 *)pid->entry;
			sect_len = le16_to_cpu(pid->count) *
				sizeof(*hw->blk[block_id].prof.t);
			dst = (u8 *)hw->blk[block_id].prof.t;
			dst_len = hw->blk[block_id].prof.count *
				sizeof(*hw->blk[block_id].prof.t);
			break;
		case ICE_SID_PROFID_REDIR_SW:
		case ICE_SID_PROFID_REDIR_FD:
		case ICE_SID_PROFID_REDIR_RSS:
		case ICE_SID_PROFID_REDIR_ACL:
		case ICE_SID_PROFID_REDIR_PE:
			pr = sect;
			src = pr->redir_value;
			sect_len = le16_to_cpu(pr->count) *
				sizeof(*hw->blk[block_id].prof_redir.t);
			dst = hw->blk[block_id].prof_redir.t;
			dst_len = hw->blk[block_id].prof_redir.count *
				sizeof(*hw->blk[block_id].prof_redir.t);
			break;
		case ICE_SID_FLD_VEC_SW:
		case ICE_SID_FLD_VEC_FD:
		case ICE_SID_FLD_VEC_RSS:
		case ICE_SID_FLD_VEC_ACL:
		case ICE_SID_FLD_VEC_PE:
			es = sect;
			src = (u8 *)es->fv;
			sect_len = (u32)(le16_to_cpu(es->count) *
					 hw->blk[block_id].es.fvw) *
				sizeof(*hw->blk[block_id].es.t);
			dst = (u8 *)hw->blk[block_id].es.t;
			dst_len = (u32)(hw->blk[block_id].es.count *
					hw->blk[block_id].es.fvw) *
				sizeof(*hw->blk[block_id].es.t);
			break;
		default:
			return;
		}

		 
		if (offset > dst_len)
			return;

		 
		if ((offset + sect_len) > dst_len)
			sect_len = dst_len - offset;

		memcpy(dst + offset, src, sect_len);
		offset += sect_len;
		sect = ice_pkg_enum_section(NULL, &state, sid);
	}
}

 
void ice_fill_blk_tbls(struct ice_hw *hw)
{
	u8 i;

	for (i = 0; i < ICE_BLK_COUNT; i++) {
		enum ice_block blk_id = (enum ice_block)i;

		ice_fill_tbl(hw, blk_id, hw->blk[blk_id].xlt1.sid);
		ice_fill_tbl(hw, blk_id, hw->blk[blk_id].xlt2.sid);
		ice_fill_tbl(hw, blk_id, hw->blk[blk_id].prof.sid);
		ice_fill_tbl(hw, blk_id, hw->blk[blk_id].prof_redir.sid);
		ice_fill_tbl(hw, blk_id, hw->blk[blk_id].es.sid);
	}

	ice_init_sw_db(hw);
}

 
static void ice_free_prof_map(struct ice_hw *hw, u8 blk_idx)
{
	struct ice_es *es = &hw->blk[blk_idx].es;
	struct ice_prof_map *del, *tmp;

	mutex_lock(&es->prof_map_lock);
	list_for_each_entry_safe(del, tmp, &es->prof_map, list) {
		list_del(&del->list);
		devm_kfree(ice_hw_to_dev(hw), del);
	}
	INIT_LIST_HEAD(&es->prof_map);
	mutex_unlock(&es->prof_map_lock);
}

 
static void ice_free_flow_profs(struct ice_hw *hw, u8 blk_idx)
{
	struct ice_flow_prof *p, *tmp;

	mutex_lock(&hw->fl_profs_locks[blk_idx]);
	list_for_each_entry_safe(p, tmp, &hw->fl_profs[blk_idx], l_entry) {
		struct ice_flow_entry *e, *t;

		list_for_each_entry_safe(e, t, &p->entries, l_entry)
			ice_flow_rem_entry(hw, (enum ice_block)blk_idx,
					   ICE_FLOW_ENTRY_HNDL(e));

		list_del(&p->l_entry);

		mutex_destroy(&p->entries_lock);
		devm_kfree(ice_hw_to_dev(hw), p);
	}
	mutex_unlock(&hw->fl_profs_locks[blk_idx]);

	 
	INIT_LIST_HEAD(&hw->fl_profs[blk_idx]);
}

 
static void ice_free_vsig_tbl(struct ice_hw *hw, enum ice_block blk)
{
	u16 i;

	if (!hw->blk[blk].xlt2.vsig_tbl)
		return;

	for (i = 1; i < ICE_MAX_VSIGS; i++)
		if (hw->blk[blk].xlt2.vsig_tbl[i].in_use)
			ice_vsig_free(hw, blk, i);
}

 
void ice_free_hw_tbls(struct ice_hw *hw)
{
	struct ice_rss_cfg *r, *rt;
	u8 i;

	for (i = 0; i < ICE_BLK_COUNT; i++) {
		if (hw->blk[i].is_list_init) {
			struct ice_es *es = &hw->blk[i].es;

			ice_free_prof_map(hw, i);
			mutex_destroy(&es->prof_map_lock);

			ice_free_flow_profs(hw, i);
			mutex_destroy(&hw->fl_profs_locks[i]);

			hw->blk[i].is_list_init = false;
		}
		ice_free_vsig_tbl(hw, (enum ice_block)i);
		devm_kfree(ice_hw_to_dev(hw), hw->blk[i].xlt1.ptypes);
		devm_kfree(ice_hw_to_dev(hw), hw->blk[i].xlt1.ptg_tbl);
		devm_kfree(ice_hw_to_dev(hw), hw->blk[i].xlt1.t);
		devm_kfree(ice_hw_to_dev(hw), hw->blk[i].xlt2.t);
		devm_kfree(ice_hw_to_dev(hw), hw->blk[i].xlt2.vsig_tbl);
		devm_kfree(ice_hw_to_dev(hw), hw->blk[i].xlt2.vsis);
		devm_kfree(ice_hw_to_dev(hw), hw->blk[i].prof.t);
		devm_kfree(ice_hw_to_dev(hw), hw->blk[i].prof_redir.t);
		devm_kfree(ice_hw_to_dev(hw), hw->blk[i].es.t);
		devm_kfree(ice_hw_to_dev(hw), hw->blk[i].es.ref_count);
		devm_kfree(ice_hw_to_dev(hw), hw->blk[i].es.written);
		devm_kfree(ice_hw_to_dev(hw), hw->blk[i].es.mask_ena);
	}

	list_for_each_entry_safe(r, rt, &hw->rss_list_head, l_entry) {
		list_del(&r->l_entry);
		devm_kfree(ice_hw_to_dev(hw), r);
	}
	mutex_destroy(&hw->rss_locks);
	ice_shutdown_all_prof_masks(hw);
	memset(hw->blk, 0, sizeof(hw->blk));
}

 
static void ice_init_flow_profs(struct ice_hw *hw, u8 blk_idx)
{
	mutex_init(&hw->fl_profs_locks[blk_idx]);
	INIT_LIST_HEAD(&hw->fl_profs[blk_idx]);
}

 
void ice_clear_hw_tbls(struct ice_hw *hw)
{
	u8 i;

	for (i = 0; i < ICE_BLK_COUNT; i++) {
		struct ice_prof_redir *prof_redir = &hw->blk[i].prof_redir;
		struct ice_prof_tcam *prof = &hw->blk[i].prof;
		struct ice_xlt1 *xlt1 = &hw->blk[i].xlt1;
		struct ice_xlt2 *xlt2 = &hw->blk[i].xlt2;
		struct ice_es *es = &hw->blk[i].es;

		if (hw->blk[i].is_list_init) {
			ice_free_prof_map(hw, i);
			ice_free_flow_profs(hw, i);
		}

		ice_free_vsig_tbl(hw, (enum ice_block)i);

		memset(xlt1->ptypes, 0, xlt1->count * sizeof(*xlt1->ptypes));
		memset(xlt1->ptg_tbl, 0,
		       ICE_MAX_PTGS * sizeof(*xlt1->ptg_tbl));
		memset(xlt1->t, 0, xlt1->count * sizeof(*xlt1->t));

		memset(xlt2->vsis, 0, xlt2->count * sizeof(*xlt2->vsis));
		memset(xlt2->vsig_tbl, 0,
		       xlt2->count * sizeof(*xlt2->vsig_tbl));
		memset(xlt2->t, 0, xlt2->count * sizeof(*xlt2->t));

		memset(prof->t, 0, prof->count * sizeof(*prof->t));
		memset(prof_redir->t, 0,
		       prof_redir->count * sizeof(*prof_redir->t));

		memset(es->t, 0, es->count * sizeof(*es->t) * es->fvw);
		memset(es->ref_count, 0, es->count * sizeof(*es->ref_count));
		memset(es->written, 0, es->count * sizeof(*es->written));
		memset(es->mask_ena, 0, es->count * sizeof(*es->mask_ena));
	}
}

 
int ice_init_hw_tbls(struct ice_hw *hw)
{
	u8 i;

	mutex_init(&hw->rss_locks);
	INIT_LIST_HEAD(&hw->rss_list_head);
	ice_init_all_prof_masks(hw);
	for (i = 0; i < ICE_BLK_COUNT; i++) {
		struct ice_prof_redir *prof_redir = &hw->blk[i].prof_redir;
		struct ice_prof_tcam *prof = &hw->blk[i].prof;
		struct ice_xlt1 *xlt1 = &hw->blk[i].xlt1;
		struct ice_xlt2 *xlt2 = &hw->blk[i].xlt2;
		struct ice_es *es = &hw->blk[i].es;
		u16 j;

		if (hw->blk[i].is_list_init)
			continue;

		ice_init_flow_profs(hw, i);
		mutex_init(&es->prof_map_lock);
		INIT_LIST_HEAD(&es->prof_map);
		hw->blk[i].is_list_init = true;

		hw->blk[i].overwrite = blk_sizes[i].overwrite;
		es->reverse = blk_sizes[i].reverse;

		xlt1->sid = ice_blk_sids[i][ICE_SID_XLT1_OFF];
		xlt1->count = blk_sizes[i].xlt1;

		xlt1->ptypes = devm_kcalloc(ice_hw_to_dev(hw), xlt1->count,
					    sizeof(*xlt1->ptypes), GFP_KERNEL);

		if (!xlt1->ptypes)
			goto err;

		xlt1->ptg_tbl = devm_kcalloc(ice_hw_to_dev(hw), ICE_MAX_PTGS,
					     sizeof(*xlt1->ptg_tbl),
					     GFP_KERNEL);

		if (!xlt1->ptg_tbl)
			goto err;

		xlt1->t = devm_kcalloc(ice_hw_to_dev(hw), xlt1->count,
				       sizeof(*xlt1->t), GFP_KERNEL);
		if (!xlt1->t)
			goto err;

		xlt2->sid = ice_blk_sids[i][ICE_SID_XLT2_OFF];
		xlt2->count = blk_sizes[i].xlt2;

		xlt2->vsis = devm_kcalloc(ice_hw_to_dev(hw), xlt2->count,
					  sizeof(*xlt2->vsis), GFP_KERNEL);

		if (!xlt2->vsis)
			goto err;

		xlt2->vsig_tbl = devm_kcalloc(ice_hw_to_dev(hw), xlt2->count,
					      sizeof(*xlt2->vsig_tbl),
					      GFP_KERNEL);
		if (!xlt2->vsig_tbl)
			goto err;

		for (j = 0; j < xlt2->count; j++)
			INIT_LIST_HEAD(&xlt2->vsig_tbl[j].prop_lst);

		xlt2->t = devm_kcalloc(ice_hw_to_dev(hw), xlt2->count,
				       sizeof(*xlt2->t), GFP_KERNEL);
		if (!xlt2->t)
			goto err;

		prof->sid = ice_blk_sids[i][ICE_SID_PR_OFF];
		prof->count = blk_sizes[i].prof_tcam;
		prof->max_prof_id = blk_sizes[i].prof_id;
		prof->cdid_bits = blk_sizes[i].prof_cdid_bits;
		prof->t = devm_kcalloc(ice_hw_to_dev(hw), prof->count,
				       sizeof(*prof->t), GFP_KERNEL);

		if (!prof->t)
			goto err;

		prof_redir->sid = ice_blk_sids[i][ICE_SID_PR_REDIR_OFF];
		prof_redir->count = blk_sizes[i].prof_redir;
		prof_redir->t = devm_kcalloc(ice_hw_to_dev(hw),
					     prof_redir->count,
					     sizeof(*prof_redir->t),
					     GFP_KERNEL);

		if (!prof_redir->t)
			goto err;

		es->sid = ice_blk_sids[i][ICE_SID_ES_OFF];
		es->count = blk_sizes[i].es;
		es->fvw = blk_sizes[i].fvw;
		es->t = devm_kcalloc(ice_hw_to_dev(hw),
				     (u32)(es->count * es->fvw),
				     sizeof(*es->t), GFP_KERNEL);
		if (!es->t)
			goto err;

		es->ref_count = devm_kcalloc(ice_hw_to_dev(hw), es->count,
					     sizeof(*es->ref_count),
					     GFP_KERNEL);
		if (!es->ref_count)
			goto err;

		es->written = devm_kcalloc(ice_hw_to_dev(hw), es->count,
					   sizeof(*es->written), GFP_KERNEL);
		if (!es->written)
			goto err;

		es->mask_ena = devm_kcalloc(ice_hw_to_dev(hw), es->count,
					    sizeof(*es->mask_ena), GFP_KERNEL);
		if (!es->mask_ena)
			goto err;
	}
	return 0;

err:
	ice_free_hw_tbls(hw);
	return -ENOMEM;
}

 
static int
ice_prof_gen_key(struct ice_hw *hw, enum ice_block blk, u8 ptg, u16 vsig,
		 u8 cdid, u16 flags, u8 vl_msk[ICE_TCAM_KEY_VAL_SZ],
		 u8 dc_msk[ICE_TCAM_KEY_VAL_SZ], u8 nm_msk[ICE_TCAM_KEY_VAL_SZ],
		 u8 key[ICE_TCAM_KEY_SZ])
{
	struct ice_prof_id_key inkey;

	inkey.xlt1 = ptg;
	inkey.xlt2_cdid = cpu_to_le16(vsig);
	inkey.flags = cpu_to_le16(flags);

	switch (hw->blk[blk].prof.cdid_bits) {
	case 0:
		break;
	case 2:
#define ICE_CD_2_M 0xC000U
#define ICE_CD_2_S 14
		inkey.xlt2_cdid &= ~cpu_to_le16(ICE_CD_2_M);
		inkey.xlt2_cdid |= cpu_to_le16(BIT(cdid) << ICE_CD_2_S);
		break;
	case 4:
#define ICE_CD_4_M 0xF000U
#define ICE_CD_4_S 12
		inkey.xlt2_cdid &= ~cpu_to_le16(ICE_CD_4_M);
		inkey.xlt2_cdid |= cpu_to_le16(BIT(cdid) << ICE_CD_4_S);
		break;
	case 8:
#define ICE_CD_8_M 0xFF00U
#define ICE_CD_8_S 16
		inkey.xlt2_cdid &= ~cpu_to_le16(ICE_CD_8_M);
		inkey.xlt2_cdid |= cpu_to_le16(BIT(cdid) << ICE_CD_8_S);
		break;
	default:
		ice_debug(hw, ICE_DBG_PKG, "Error in profile config\n");
		break;
	}

	return ice_set_key(key, ICE_TCAM_KEY_SZ, (u8 *)&inkey, vl_msk, dc_msk,
			   nm_msk, 0, ICE_TCAM_KEY_SZ / 2);
}

 
static int
ice_tcam_write_entry(struct ice_hw *hw, enum ice_block blk, u16 idx,
		     u8 prof_id, u8 ptg, u16 vsig, u8 cdid, u16 flags,
		     u8 vl_msk[ICE_TCAM_KEY_VAL_SZ],
		     u8 dc_msk[ICE_TCAM_KEY_VAL_SZ],
		     u8 nm_msk[ICE_TCAM_KEY_VAL_SZ])
{
	struct ice_prof_tcam_entry;
	int status;

	status = ice_prof_gen_key(hw, blk, ptg, vsig, cdid, flags, vl_msk,
				  dc_msk, nm_msk, hw->blk[blk].prof.t[idx].key);
	if (!status) {
		hw->blk[blk].prof.t[idx].addr = cpu_to_le16(idx);
		hw->blk[blk].prof.t[idx].prof_id = prof_id;
	}

	return status;
}

 
static int
ice_vsig_get_ref(struct ice_hw *hw, enum ice_block blk, u16 vsig, u16 *refs)
{
	u16 idx = vsig & ICE_VSIG_IDX_M;
	struct ice_vsig_vsi *ptr;

	*refs = 0;

	if (!hw->blk[blk].xlt2.vsig_tbl[idx].in_use)
		return -ENOENT;

	ptr = hw->blk[blk].xlt2.vsig_tbl[idx].first_vsi;
	while (ptr) {
		(*refs)++;
		ptr = ptr->next_vsi;
	}

	return 0;
}

 
static bool
ice_has_prof_vsig(struct ice_hw *hw, enum ice_block blk, u16 vsig, u64 hdl)
{
	u16 idx = vsig & ICE_VSIG_IDX_M;
	struct ice_vsig_prof *ent;

	list_for_each_entry(ent, &hw->blk[blk].xlt2.vsig_tbl[idx].prop_lst,
			    list)
		if (ent->profile_cookie == hdl)
			return true;

	ice_debug(hw, ICE_DBG_INIT, "Characteristic list for VSI group %d not found.\n",
		  vsig);
	return false;
}

 
static int
ice_prof_bld_es(struct ice_hw *hw, enum ice_block blk,
		struct ice_buf_build *bld, struct list_head *chgs)
{
	u16 vec_size = hw->blk[blk].es.fvw * sizeof(struct ice_fv_word);
	struct ice_chs_chg *tmp;

	list_for_each_entry(tmp, chgs, list_entry)
		if (tmp->type == ICE_PTG_ES_ADD && tmp->add_prof) {
			u16 off = tmp->prof_id * hw->blk[blk].es.fvw;
			struct ice_pkg_es *p;
			u32 id;

			id = ice_sect_id(blk, ICE_VEC_TBL);
			p = ice_pkg_buf_alloc_section(bld, id,
						      struct_size(p, es, 1) +
						      vec_size -
						      sizeof(p->es[0]));

			if (!p)
				return -ENOSPC;

			p->count = cpu_to_le16(1);
			p->offset = cpu_to_le16(tmp->prof_id);

			memcpy(p->es, &hw->blk[blk].es.t[off], vec_size);
		}

	return 0;
}

 
static int
ice_prof_bld_tcam(struct ice_hw *hw, enum ice_block blk,
		  struct ice_buf_build *bld, struct list_head *chgs)
{
	struct ice_chs_chg *tmp;

	list_for_each_entry(tmp, chgs, list_entry)
		if (tmp->type == ICE_TCAM_ADD && tmp->add_tcam_idx) {
			struct ice_prof_id_section *p;
			u32 id;

			id = ice_sect_id(blk, ICE_PROF_TCAM);
			p = ice_pkg_buf_alloc_section(bld, id,
						      struct_size(p, entry, 1));

			if (!p)
				return -ENOSPC;

			p->count = cpu_to_le16(1);
			p->entry[0].addr = cpu_to_le16(tmp->tcam_idx);
			p->entry[0].prof_id = tmp->prof_id;

			memcpy(p->entry[0].key,
			       &hw->blk[blk].prof.t[tmp->tcam_idx].key,
			       sizeof(hw->blk[blk].prof.t->key));
		}

	return 0;
}

 
static int
ice_prof_bld_xlt1(enum ice_block blk, struct ice_buf_build *bld,
		  struct list_head *chgs)
{
	struct ice_chs_chg *tmp;

	list_for_each_entry(tmp, chgs, list_entry)
		if (tmp->type == ICE_PTG_ES_ADD && tmp->add_ptg) {
			struct ice_xlt1_section *p;
			u32 id;

			id = ice_sect_id(blk, ICE_XLT1);
			p = ice_pkg_buf_alloc_section(bld, id,
						      struct_size(p, value, 1));

			if (!p)
				return -ENOSPC;

			p->count = cpu_to_le16(1);
			p->offset = cpu_to_le16(tmp->ptype);
			p->value[0] = tmp->ptg;
		}

	return 0;
}

 
static int
ice_prof_bld_xlt2(enum ice_block blk, struct ice_buf_build *bld,
		  struct list_head *chgs)
{
	struct ice_chs_chg *tmp;

	list_for_each_entry(tmp, chgs, list_entry) {
		struct ice_xlt2_section *p;
		u32 id;

		switch (tmp->type) {
		case ICE_VSIG_ADD:
		case ICE_VSI_MOVE:
		case ICE_VSIG_REM:
			id = ice_sect_id(blk, ICE_XLT2);
			p = ice_pkg_buf_alloc_section(bld, id,
						      struct_size(p, value, 1));

			if (!p)
				return -ENOSPC;

			p->count = cpu_to_le16(1);
			p->offset = cpu_to_le16(tmp->vsi);
			p->value[0] = cpu_to_le16(tmp->vsig);
			break;
		default:
			break;
		}
	}

	return 0;
}

 
static int
ice_upd_prof_hw(struct ice_hw *hw, enum ice_block blk,
		struct list_head *chgs)
{
	struct ice_buf_build *b;
	struct ice_chs_chg *tmp;
	u16 pkg_sects;
	u16 xlt1 = 0;
	u16 xlt2 = 0;
	u16 tcam = 0;
	u16 es = 0;
	int status;
	u16 sects;

	 
	list_for_each_entry(tmp, chgs, list_entry) {
		switch (tmp->type) {
		case ICE_PTG_ES_ADD:
			if (tmp->add_ptg)
				xlt1++;
			if (tmp->add_prof)
				es++;
			break;
		case ICE_TCAM_ADD:
			tcam++;
			break;
		case ICE_VSIG_ADD:
		case ICE_VSI_MOVE:
		case ICE_VSIG_REM:
			xlt2++;
			break;
		default:
			break;
		}
	}
	sects = xlt1 + xlt2 + tcam + es;

	if (!sects)
		return 0;

	 
	b = ice_pkg_buf_alloc(hw);
	if (!b)
		return -ENOMEM;

	status = ice_pkg_buf_reserve_section(b, sects);
	if (status)
		goto error_tmp;

	 
	if (es) {
		status = ice_prof_bld_es(hw, blk, b, chgs);
		if (status)
			goto error_tmp;
	}

	if (tcam) {
		status = ice_prof_bld_tcam(hw, blk, b, chgs);
		if (status)
			goto error_tmp;
	}

	if (xlt1) {
		status = ice_prof_bld_xlt1(blk, b, chgs);
		if (status)
			goto error_tmp;
	}

	if (xlt2) {
		status = ice_prof_bld_xlt2(blk, b, chgs);
		if (status)
			goto error_tmp;
	}

	 
	pkg_sects = ice_pkg_buf_get_active_sections(b);
	if (!pkg_sects || pkg_sects != sects) {
		status = -EINVAL;
		goto error_tmp;
	}

	 
	status = ice_update_pkg(hw, ice_pkg_buf(b), 1);
	if (status == -EIO)
		ice_debug(hw, ICE_DBG_INIT, "Unable to update HW profile\n");

error_tmp:
	ice_pkg_buf_free(hw, b);
	return status;
}

 
static void ice_update_fd_mask(struct ice_hw *hw, u16 prof_id, u32 mask_sel)
{
	wr32(hw, GLQF_FDMASK_SEL(prof_id), mask_sel);

	ice_debug(hw, ICE_DBG_INIT, "fd mask(%d): %x = %x\n", prof_id,
		  GLQF_FDMASK_SEL(prof_id), mask_sel);
}

struct ice_fd_src_dst_pair {
	u8 prot_id;
	u8 count;
	u16 off;
};

static const struct ice_fd_src_dst_pair ice_fd_pairs[] = {
	 
	{ ICE_PROT_IPV4_OF_OR_S, 2, 12 },
	{ ICE_PROT_IPV4_OF_OR_S, 2, 16 },

	{ ICE_PROT_IPV4_IL, 2, 12 },
	{ ICE_PROT_IPV4_IL, 2, 16 },

	{ ICE_PROT_IPV6_OF_OR_S, 8, 8 },
	{ ICE_PROT_IPV6_OF_OR_S, 8, 24 },

	{ ICE_PROT_IPV6_IL, 8, 8 },
	{ ICE_PROT_IPV6_IL, 8, 24 },

	{ ICE_PROT_TCP_IL, 1, 0 },
	{ ICE_PROT_TCP_IL, 1, 2 },

	{ ICE_PROT_UDP_OF, 1, 0 },
	{ ICE_PROT_UDP_OF, 1, 2 },

	{ ICE_PROT_UDP_IL_OR_S, 1, 0 },
	{ ICE_PROT_UDP_IL_OR_S, 1, 2 },

	{ ICE_PROT_SCTP_IL, 1, 0 },
	{ ICE_PROT_SCTP_IL, 1, 2 }
};

#define ICE_FD_SRC_DST_PAIR_COUNT	ARRAY_SIZE(ice_fd_pairs)

 
static int
ice_update_fd_swap(struct ice_hw *hw, u16 prof_id, struct ice_fv_word *es)
{
	DECLARE_BITMAP(pair_list, ICE_FD_SRC_DST_PAIR_COUNT);
	u8 pair_start[ICE_FD_SRC_DST_PAIR_COUNT] = { 0 };
#define ICE_FD_FV_NOT_FOUND (-2)
	s8 first_free = ICE_FD_FV_NOT_FOUND;
	u8 used[ICE_MAX_FV_WORDS] = { 0 };
	s8 orig_free, si;
	u32 mask_sel = 0;
	u8 i, j, k;

	bitmap_zero(pair_list, ICE_FD_SRC_DST_PAIR_COUNT);

	 

	 
	for (i = 0; i < hw->blk[ICE_BLK_FD].es.fvw; i++) {
		 
		if (first_free == ICE_FD_FV_NOT_FOUND && es[i].prot_id !=
		    ICE_PROT_INVALID)
			first_free = i - 1;

		for (j = 0; j < ICE_FD_SRC_DST_PAIR_COUNT; j++)
			if (es[i].prot_id == ice_fd_pairs[j].prot_id &&
			    es[i].off == ice_fd_pairs[j].off) {
				__set_bit(j, pair_list);
				pair_start[j] = i;
			}
	}

	orig_free = first_free;

	 
	for (i = 0; i < ICE_FD_SRC_DST_PAIR_COUNT; i += 2) {
		u8 bit1 = test_bit(i + 1, pair_list);
		u8 bit0 = test_bit(i, pair_list);

		if (bit0 ^ bit1) {
			u8 index;

			 
			if (!bit0)
				index = i;
			else
				index = i + 1;

			 
			if (first_free + 1 < (s8)ice_fd_pairs[index].count)
				return -ENOSPC;

			 
			for (k = 0; k < ice_fd_pairs[index].count; k++) {
				es[first_free - k].prot_id =
					ice_fd_pairs[index].prot_id;
				es[first_free - k].off =
					ice_fd_pairs[index].off + (k * 2);

				if (k > first_free)
					return -EIO;

				 
				mask_sel |= BIT(first_free - k);
			}

			pair_start[index] = first_free;
			first_free -= ice_fd_pairs[index].count;
		}
	}

	 
	si = hw->blk[ICE_BLK_FD].es.fvw - 1;
	while (si >= 0) {
		u8 indexes_used = 1;

		 
#define ICE_SWAP_VALID	0x80
		used[si] = si | ICE_SWAP_VALID;

		if (orig_free == ICE_FD_FV_NOT_FOUND || si <= orig_free) {
			si -= indexes_used;
			continue;
		}

		 
		for (j = 0; j < ICE_FD_SRC_DST_PAIR_COUNT; j++)
			if (es[si].prot_id == ice_fd_pairs[j].prot_id &&
			    es[si].off == ice_fd_pairs[j].off) {
				u8 idx;

				 
				idx = j + ((j % 2) ? -1 : 1);

				indexes_used = ice_fd_pairs[idx].count;
				for (k = 0; k < indexes_used; k++) {
					used[si - k] = (pair_start[idx] - k) |
						ICE_SWAP_VALID;
				}

				break;
			}

		si -= indexes_used;
	}

	 
	for (j = 0; j < hw->blk[ICE_BLK_FD].es.fvw / 4; j++) {
		u32 raw_swap = 0;
		u32 raw_in = 0;

		for (k = 0; k < 4; k++) {
			u8 idx;

			idx = (j * 4) + k;
			if (used[idx] && !(mask_sel & BIT(idx))) {
				raw_swap |= used[idx] << (k * BITS_PER_BYTE);
#define ICE_INSET_DFLT 0x9f
				raw_in |= ICE_INSET_DFLT << (k * BITS_PER_BYTE);
			}
		}

		 
		wr32(hw, GLQF_FDSWAP(prof_id, j), raw_swap);

		ice_debug(hw, ICE_DBG_INIT, "swap wr(%d, %d): %x = %08x\n",
			  prof_id, j, GLQF_FDSWAP(prof_id, j), raw_swap);

		 
		wr32(hw, GLQF_FDINSET(prof_id, j), raw_in);

		ice_debug(hw, ICE_DBG_INIT, "inset wr(%d, %d): %x = %08x\n",
			  prof_id, j, GLQF_FDINSET(prof_id, j), raw_in);
	}

	 
	ice_update_fd_mask(hw, prof_id, 0);

	return 0;
}

 
static const struct ice_ptype_attrib_info ice_ptype_attributes[] = {
	{ ICE_GTP_PDU_EH,	ICE_GTP_PDU_FLAG_MASK },
	{ ICE_GTP_SESSION,	ICE_GTP_FLAGS_MASK },
	{ ICE_GTP_DOWNLINK,	ICE_GTP_FLAGS_MASK },
	{ ICE_GTP_UPLINK,	ICE_GTP_FLAGS_MASK },
};

 
static void
ice_get_ptype_attrib_info(enum ice_ptype_attrib_type type,
			  struct ice_ptype_attrib_info *info)
{
	*info = ice_ptype_attributes[type];
}

 
static int
ice_add_prof_attrib(struct ice_prof_map *prof, u8 ptg, u16 ptype,
		    const struct ice_ptype_attributes *attr, u16 attr_cnt)
{
	bool found = false;
	u16 i;

	for (i = 0; i < attr_cnt; i++)
		if (attr[i].ptype == ptype) {
			found = true;

			prof->ptg[prof->ptg_cnt] = ptg;
			ice_get_ptype_attrib_info(attr[i].attrib,
						  &prof->attr[prof->ptg_cnt]);

			if (++prof->ptg_cnt >= ICE_MAX_PTG_PER_PROFILE)
				return -ENOSPC;
		}

	if (!found)
		return -ENOENT;

	return 0;
}

 
int
ice_add_prof(struct ice_hw *hw, enum ice_block blk, u64 id, u8 ptypes[],
	     const struct ice_ptype_attributes *attr, u16 attr_cnt,
	     struct ice_fv_word *es, u16 *masks)
{
	u32 bytes = DIV_ROUND_UP(ICE_FLOW_PTYPE_MAX, BITS_PER_BYTE);
	DECLARE_BITMAP(ptgs_used, ICE_XLT1_CNT);
	struct ice_prof_map *prof;
	u8 byte = 0;
	u8 prof_id;
	int status;

	bitmap_zero(ptgs_used, ICE_XLT1_CNT);

	mutex_lock(&hw->blk[blk].es.prof_map_lock);

	 
	status = ice_find_prof_id_with_mask(hw, blk, es, masks, &prof_id);
	if (status) {
		 
		status = ice_alloc_prof_id(hw, blk, &prof_id);
		if (status)
			goto err_ice_add_prof;
		if (blk == ICE_BLK_FD) {
			 
			status = ice_update_fd_swap(hw, prof_id, es);
			if (status)
				goto err_ice_add_prof;
		}
		status = ice_update_prof_masking(hw, blk, prof_id, masks);
		if (status)
			goto err_ice_add_prof;

		 
		ice_write_es(hw, blk, prof_id, es);
	}

	ice_prof_inc_ref(hw, blk, prof_id);

	 
	prof = devm_kzalloc(ice_hw_to_dev(hw), sizeof(*prof), GFP_KERNEL);
	if (!prof) {
		status = -ENOMEM;
		goto err_ice_add_prof;
	}

	prof->profile_cookie = id;
	prof->prof_id = prof_id;
	prof->ptg_cnt = 0;
	prof->context = 0;

	 
	while (bytes && prof->ptg_cnt < ICE_MAX_PTG_PER_PROFILE) {
		u8 bit;

		if (!ptypes[byte]) {
			bytes--;
			byte++;
			continue;
		}

		 
		for_each_set_bit(bit, (unsigned long *)&ptypes[byte],
				 BITS_PER_BYTE) {
			u16 ptype;
			u8 ptg;

			ptype = byte * BITS_PER_BYTE + bit;

			 
			if (ice_ptg_find_ptype(hw, blk, ptype, &ptg))
				continue;

			 
			if (test_bit(ptg, ptgs_used))
				continue;

			__set_bit(ptg, ptgs_used);
			 
			status = ice_add_prof_attrib(prof, ptg, ptype,
						     attr, attr_cnt);
			if (status == -ENOSPC)
				break;
			if (status) {
				 
				prof->ptg[prof->ptg_cnt] = ptg;
				prof->attr[prof->ptg_cnt].flags = 0;
				prof->attr[prof->ptg_cnt].mask = 0;

				if (++prof->ptg_cnt >=
				    ICE_MAX_PTG_PER_PROFILE)
					break;
			}
		}

		bytes--;
		byte++;
	}

	list_add(&prof->list, &hw->blk[blk].es.prof_map);
	status = 0;

err_ice_add_prof:
	mutex_unlock(&hw->blk[blk].es.prof_map_lock);
	return status;
}

 
static struct ice_prof_map *
ice_search_prof_id(struct ice_hw *hw, enum ice_block blk, u64 id)
{
	struct ice_prof_map *entry = NULL;
	struct ice_prof_map *map;

	list_for_each_entry(map, &hw->blk[blk].es.prof_map, list)
		if (map->profile_cookie == id) {
			entry = map;
			break;
		}

	return entry;
}

 
static u16
ice_vsig_prof_id_count(struct ice_hw *hw, enum ice_block blk, u16 vsig)
{
	u16 idx = vsig & ICE_VSIG_IDX_M, count = 0;
	struct ice_vsig_prof *p;

	list_for_each_entry(p, &hw->blk[blk].xlt2.vsig_tbl[idx].prop_lst,
			    list)
		count++;

	return count;
}

 
static int ice_rel_tcam_idx(struct ice_hw *hw, enum ice_block blk, u16 idx)
{
	 
	u8 vl_msk[ICE_TCAM_KEY_VAL_SZ] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
	u8 dc_msk[ICE_TCAM_KEY_VAL_SZ] = { 0xFE, 0xFF, 0xFF, 0xFF, 0xFF };
	u8 nm_msk[ICE_TCAM_KEY_VAL_SZ] = { 0x01, 0x00, 0x00, 0x00, 0x00 };
	int status;

	 
	status = ice_tcam_write_entry(hw, blk, idx, 0, 0, 0, 0, 0, vl_msk,
				      dc_msk, nm_msk);
	if (status)
		return status;

	 
	status = ice_free_tcam_ent(hw, blk, idx);

	return status;
}

 
static int
ice_rem_prof_id(struct ice_hw *hw, enum ice_block blk,
		struct ice_vsig_prof *prof)
{
	int status;
	u16 i;

	for (i = 0; i < prof->tcam_count; i++)
		if (prof->tcam[i].in_use) {
			prof->tcam[i].in_use = false;
			status = ice_rel_tcam_idx(hw, blk,
						  prof->tcam[i].tcam_idx);
			if (status)
				return -EIO;
		}

	return 0;
}

 
static int
ice_rem_vsig(struct ice_hw *hw, enum ice_block blk, u16 vsig,
	     struct list_head *chg)
{
	u16 idx = vsig & ICE_VSIG_IDX_M;
	struct ice_vsig_vsi *vsi_cur;
	struct ice_vsig_prof *d, *t;

	 
	list_for_each_entry_safe(d, t,
				 &hw->blk[blk].xlt2.vsig_tbl[idx].prop_lst,
				 list) {
		int status;

		status = ice_rem_prof_id(hw, blk, d);
		if (status)
			return status;

		list_del(&d->list);
		devm_kfree(ice_hw_to_dev(hw), d);
	}

	 
	vsi_cur = hw->blk[blk].xlt2.vsig_tbl[idx].first_vsi;
	 
	if (vsi_cur)
		do {
			struct ice_vsig_vsi *tmp = vsi_cur->next_vsi;
			struct ice_chs_chg *p;

			p = devm_kzalloc(ice_hw_to_dev(hw), sizeof(*p),
					 GFP_KERNEL);
			if (!p)
				return -ENOMEM;

			p->type = ICE_VSIG_REM;
			p->orig_vsig = vsig;
			p->vsig = ICE_DEFAULT_VSIG;
			p->vsi = vsi_cur - hw->blk[blk].xlt2.vsis;

			list_add(&p->list_entry, chg);

			vsi_cur = tmp;
		} while (vsi_cur);

	return ice_vsig_free(hw, blk, vsig);
}

 
static int
ice_rem_prof_id_vsig(struct ice_hw *hw, enum ice_block blk, u16 vsig, u64 hdl,
		     struct list_head *chg)
{
	u16 idx = vsig & ICE_VSIG_IDX_M;
	struct ice_vsig_prof *p, *t;

	list_for_each_entry_safe(p, t,
				 &hw->blk[blk].xlt2.vsig_tbl[idx].prop_lst,
				 list)
		if (p->profile_cookie == hdl) {
			int status;

			if (ice_vsig_prof_id_count(hw, blk, vsig) == 1)
				 
				return ice_rem_vsig(hw, blk, vsig, chg);

			status = ice_rem_prof_id(hw, blk, p);
			if (!status) {
				list_del(&p->list);
				devm_kfree(ice_hw_to_dev(hw), p);
			}
			return status;
		}

	return -ENOENT;
}

 
static int ice_rem_flow_all(struct ice_hw *hw, enum ice_block blk, u64 id)
{
	struct ice_chs_chg *del, *tmp;
	struct list_head chg;
	int status;
	u16 i;

	INIT_LIST_HEAD(&chg);

	for (i = 1; i < ICE_MAX_VSIGS; i++)
		if (hw->blk[blk].xlt2.vsig_tbl[i].in_use) {
			if (ice_has_prof_vsig(hw, blk, i, id)) {
				status = ice_rem_prof_id_vsig(hw, blk, i, id,
							      &chg);
				if (status)
					goto err_ice_rem_flow_all;
			}
		}

	status = ice_upd_prof_hw(hw, blk, &chg);

err_ice_rem_flow_all:
	list_for_each_entry_safe(del, tmp, &chg, list_entry) {
		list_del(&del->list_entry);
		devm_kfree(ice_hw_to_dev(hw), del);
	}

	return status;
}

 
int ice_rem_prof(struct ice_hw *hw, enum ice_block blk, u64 id)
{
	struct ice_prof_map *pmap;
	int status;

	mutex_lock(&hw->blk[blk].es.prof_map_lock);

	pmap = ice_search_prof_id(hw, blk, id);
	if (!pmap) {
		status = -ENOENT;
		goto err_ice_rem_prof;
	}

	 
	status = ice_rem_flow_all(hw, blk, pmap->profile_cookie);
	if (status)
		goto err_ice_rem_prof;

	 
	ice_prof_dec_ref(hw, blk, pmap->prof_id);

	list_del(&pmap->list);
	devm_kfree(ice_hw_to_dev(hw), pmap);

err_ice_rem_prof:
	mutex_unlock(&hw->blk[blk].es.prof_map_lock);
	return status;
}

 
static int
ice_get_prof(struct ice_hw *hw, enum ice_block blk, u64 hdl,
	     struct list_head *chg)
{
	struct ice_prof_map *map;
	struct ice_chs_chg *p;
	int status = 0;
	u16 i;

	mutex_lock(&hw->blk[blk].es.prof_map_lock);
	 
	map = ice_search_prof_id(hw, blk, hdl);
	if (!map) {
		status = -ENOENT;
		goto err_ice_get_prof;
	}

	for (i = 0; i < map->ptg_cnt; i++)
		if (!hw->blk[blk].es.written[map->prof_id]) {
			 
			p = devm_kzalloc(ice_hw_to_dev(hw), sizeof(*p),
					 GFP_KERNEL);
			if (!p) {
				status = -ENOMEM;
				goto err_ice_get_prof;
			}

			p->type = ICE_PTG_ES_ADD;
			p->ptype = 0;
			p->ptg = map->ptg[i];
			p->add_ptg = 0;

			p->add_prof = 1;
			p->prof_id = map->prof_id;

			hw->blk[blk].es.written[map->prof_id] = true;

			list_add(&p->list_entry, chg);
		}

err_ice_get_prof:
	mutex_unlock(&hw->blk[blk].es.prof_map_lock);
	 
	return status;
}

 
static int
ice_get_profs_vsig(struct ice_hw *hw, enum ice_block blk, u16 vsig,
		   struct list_head *lst)
{
	struct ice_vsig_prof *ent1, *ent2;
	u16 idx = vsig & ICE_VSIG_IDX_M;

	list_for_each_entry(ent1, &hw->blk[blk].xlt2.vsig_tbl[idx].prop_lst,
			    list) {
		struct ice_vsig_prof *p;

		 
		p = devm_kmemdup(ice_hw_to_dev(hw), ent1, sizeof(*p),
				 GFP_KERNEL);
		if (!p)
			goto err_ice_get_profs_vsig;

		list_add_tail(&p->list, lst);
	}

	return 0;

err_ice_get_profs_vsig:
	list_for_each_entry_safe(ent1, ent2, lst, list) {
		list_del(&ent1->list);
		devm_kfree(ice_hw_to_dev(hw), ent1);
	}

	return -ENOMEM;
}

 
static int
ice_add_prof_to_lst(struct ice_hw *hw, enum ice_block blk,
		    struct list_head *lst, u64 hdl)
{
	struct ice_prof_map *map;
	struct ice_vsig_prof *p;
	int status = 0;
	u16 i;

	mutex_lock(&hw->blk[blk].es.prof_map_lock);
	map = ice_search_prof_id(hw, blk, hdl);
	if (!map) {
		status = -ENOENT;
		goto err_ice_add_prof_to_lst;
	}

	p = devm_kzalloc(ice_hw_to_dev(hw), sizeof(*p), GFP_KERNEL);
	if (!p) {
		status = -ENOMEM;
		goto err_ice_add_prof_to_lst;
	}

	p->profile_cookie = map->profile_cookie;
	p->prof_id = map->prof_id;
	p->tcam_count = map->ptg_cnt;

	for (i = 0; i < map->ptg_cnt; i++) {
		p->tcam[i].prof_id = map->prof_id;
		p->tcam[i].tcam_idx = ICE_INVALID_TCAM;
		p->tcam[i].ptg = map->ptg[i];
	}

	list_add(&p->list, lst);

err_ice_add_prof_to_lst:
	mutex_unlock(&hw->blk[blk].es.prof_map_lock);
	return status;
}

 
static int
ice_move_vsi(struct ice_hw *hw, enum ice_block blk, u16 vsi, u16 vsig,
	     struct list_head *chg)
{
	struct ice_chs_chg *p;
	u16 orig_vsig;
	int status;

	p = devm_kzalloc(ice_hw_to_dev(hw), sizeof(*p), GFP_KERNEL);
	if (!p)
		return -ENOMEM;

	status = ice_vsig_find_vsi(hw, blk, vsi, &orig_vsig);
	if (!status)
		status = ice_vsig_add_mv_vsi(hw, blk, vsi, vsig);

	if (status) {
		devm_kfree(ice_hw_to_dev(hw), p);
		return status;
	}

	p->type = ICE_VSI_MOVE;
	p->vsi = vsi;
	p->orig_vsig = orig_vsig;
	p->vsig = vsig;

	list_add(&p->list_entry, chg);

	return 0;
}

 
static void
ice_rem_chg_tcam_ent(struct ice_hw *hw, u16 idx, struct list_head *chg)
{
	struct ice_chs_chg *pos, *tmp;

	list_for_each_entry_safe(tmp, pos, chg, list_entry)
		if (tmp->type == ICE_TCAM_ADD && tmp->tcam_idx == idx) {
			list_del(&tmp->list_entry);
			devm_kfree(ice_hw_to_dev(hw), tmp);
		}
}

 
static int
ice_prof_tcam_ena_dis(struct ice_hw *hw, enum ice_block blk, bool enable,
		      u16 vsig, struct ice_tcam_inf *tcam,
		      struct list_head *chg)
{
	struct ice_chs_chg *p;
	int status;

	u8 vl_msk[ICE_TCAM_KEY_VAL_SZ] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
	u8 dc_msk[ICE_TCAM_KEY_VAL_SZ] = { 0xFF, 0xFF, 0x00, 0x00, 0x00 };
	u8 nm_msk[ICE_TCAM_KEY_VAL_SZ] = { 0x00, 0x00, 0x00, 0x00, 0x00 };

	 
	if (!enable) {
		status = ice_rel_tcam_idx(hw, blk, tcam->tcam_idx);

		 
		ice_rem_chg_tcam_ent(hw, tcam->tcam_idx, chg);
		tcam->tcam_idx = 0;
		tcam->in_use = 0;
		return status;
	}

	 
	 
	status = ice_alloc_tcam_ent(hw, blk, tcam->attr.mask == 0,
				    &tcam->tcam_idx);
	if (status)
		return status;

	 
	p = devm_kzalloc(ice_hw_to_dev(hw), sizeof(*p), GFP_KERNEL);
	if (!p)
		return -ENOMEM;

	status = ice_tcam_write_entry(hw, blk, tcam->tcam_idx, tcam->prof_id,
				      tcam->ptg, vsig, 0, tcam->attr.flags,
				      vl_msk, dc_msk, nm_msk);
	if (status)
		goto err_ice_prof_tcam_ena_dis;

	tcam->in_use = 1;

	p->type = ICE_TCAM_ADD;
	p->add_tcam_idx = true;
	p->prof_id = tcam->prof_id;
	p->ptg = tcam->ptg;
	p->vsig = 0;
	p->tcam_idx = tcam->tcam_idx;

	 
	list_add(&p->list_entry, chg);

	return 0;

err_ice_prof_tcam_ena_dis:
	devm_kfree(ice_hw_to_dev(hw), p);
	return status;
}

 
static int
ice_adj_prof_priorities(struct ice_hw *hw, enum ice_block blk, u16 vsig,
			struct list_head *chg)
{
	DECLARE_BITMAP(ptgs_used, ICE_XLT1_CNT);
	struct ice_vsig_prof *t;
	int status;
	u16 idx;

	bitmap_zero(ptgs_used, ICE_XLT1_CNT);
	idx = vsig & ICE_VSIG_IDX_M;

	 

	list_for_each_entry(t, &hw->blk[blk].xlt2.vsig_tbl[idx].prop_lst,
			    list) {
		u16 i;

		for (i = 0; i < t->tcam_count; i++) {
			 
			if (test_bit(t->tcam[i].ptg, ptgs_used) &&
			    t->tcam[i].in_use) {
				 
				status = ice_prof_tcam_ena_dis(hw, blk, false,
							       vsig,
							       &t->tcam[i],
							       chg);
				if (status)
					return status;
			} else if (!test_bit(t->tcam[i].ptg, ptgs_used) &&
				   !t->tcam[i].in_use) {
				 
				status = ice_prof_tcam_ena_dis(hw, blk, true,
							       vsig,
							       &t->tcam[i],
							       chg);
				if (status)
					return status;
			}

			 
			__set_bit(t->tcam[i].ptg, ptgs_used);
		}
	}

	return 0;
}

 
static int
ice_add_prof_id_vsig(struct ice_hw *hw, enum ice_block blk, u16 vsig, u64 hdl,
		     bool rev, struct list_head *chg)
{
	 
	u8 vl_msk[ICE_TCAM_KEY_VAL_SZ] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
	u8 dc_msk[ICE_TCAM_KEY_VAL_SZ] = { 0xFF, 0xFF, 0x00, 0x00, 0x00 };
	u8 nm_msk[ICE_TCAM_KEY_VAL_SZ] = { 0x00, 0x00, 0x00, 0x00, 0x00 };
	struct ice_prof_map *map;
	struct ice_vsig_prof *t;
	struct ice_chs_chg *p;
	u16 vsig_idx, i;
	int status = 0;

	 
	if (ice_has_prof_vsig(hw, blk, vsig, hdl))
		return -EEXIST;

	 
	t = devm_kzalloc(ice_hw_to_dev(hw), sizeof(*t), GFP_KERNEL);
	if (!t)
		return -ENOMEM;

	mutex_lock(&hw->blk[blk].es.prof_map_lock);
	 
	map = ice_search_prof_id(hw, blk, hdl);
	if (!map) {
		status = -ENOENT;
		goto err_ice_add_prof_id_vsig;
	}

	t->profile_cookie = map->profile_cookie;
	t->prof_id = map->prof_id;
	t->tcam_count = map->ptg_cnt;

	 
	for (i = 0; i < map->ptg_cnt; i++) {
		u16 tcam_idx;

		 
		p = devm_kzalloc(ice_hw_to_dev(hw), sizeof(*p), GFP_KERNEL);
		if (!p) {
			status = -ENOMEM;
			goto err_ice_add_prof_id_vsig;
		}

		 
		 
		status = ice_alloc_tcam_ent(hw, blk, map->attr[i].mask == 0,
					    &tcam_idx);
		if (status) {
			devm_kfree(ice_hw_to_dev(hw), p);
			goto err_ice_add_prof_id_vsig;
		}

		t->tcam[i].ptg = map->ptg[i];
		t->tcam[i].prof_id = map->prof_id;
		t->tcam[i].tcam_idx = tcam_idx;
		t->tcam[i].attr = map->attr[i];
		t->tcam[i].in_use = true;

		p->type = ICE_TCAM_ADD;
		p->add_tcam_idx = true;
		p->prof_id = t->tcam[i].prof_id;
		p->ptg = t->tcam[i].ptg;
		p->vsig = vsig;
		p->tcam_idx = t->tcam[i].tcam_idx;

		 
		status = ice_tcam_write_entry(hw, blk, t->tcam[i].tcam_idx,
					      t->tcam[i].prof_id,
					      t->tcam[i].ptg, vsig, 0, 0,
					      vl_msk, dc_msk, nm_msk);
		if (status) {
			devm_kfree(ice_hw_to_dev(hw), p);
			goto err_ice_add_prof_id_vsig;
		}

		 
		list_add(&p->list_entry, chg);
	}

	 
	vsig_idx = vsig & ICE_VSIG_IDX_M;
	if (rev)
		list_add_tail(&t->list,
			      &hw->blk[blk].xlt2.vsig_tbl[vsig_idx].prop_lst);
	else
		list_add(&t->list,
			 &hw->blk[blk].xlt2.vsig_tbl[vsig_idx].prop_lst);

	mutex_unlock(&hw->blk[blk].es.prof_map_lock);
	return status;

err_ice_add_prof_id_vsig:
	mutex_unlock(&hw->blk[blk].es.prof_map_lock);
	 
	devm_kfree(ice_hw_to_dev(hw), t);
	return status;
}

 
static int
ice_create_prof_id_vsig(struct ice_hw *hw, enum ice_block blk, u16 vsi, u64 hdl,
			struct list_head *chg)
{
	struct ice_chs_chg *p;
	u16 new_vsig;
	int status;

	p = devm_kzalloc(ice_hw_to_dev(hw), sizeof(*p), GFP_KERNEL);
	if (!p)
		return -ENOMEM;

	new_vsig = ice_vsig_alloc(hw, blk);
	if (!new_vsig) {
		status = -EIO;
		goto err_ice_create_prof_id_vsig;
	}

	status = ice_move_vsi(hw, blk, vsi, new_vsig, chg);
	if (status)
		goto err_ice_create_prof_id_vsig;

	status = ice_add_prof_id_vsig(hw, blk, new_vsig, hdl, false, chg);
	if (status)
		goto err_ice_create_prof_id_vsig;

	p->type = ICE_VSIG_ADD;
	p->vsi = vsi;
	p->orig_vsig = ICE_DEFAULT_VSIG;
	p->vsig = new_vsig;

	list_add(&p->list_entry, chg);

	return 0;

err_ice_create_prof_id_vsig:
	 
	devm_kfree(ice_hw_to_dev(hw), p);
	return status;
}

 
static int
ice_create_vsig_from_lst(struct ice_hw *hw, enum ice_block blk, u16 vsi,
			 struct list_head *lst, u16 *new_vsig,
			 struct list_head *chg)
{
	struct ice_vsig_prof *t;
	int status;
	u16 vsig;

	vsig = ice_vsig_alloc(hw, blk);
	if (!vsig)
		return -EIO;

	status = ice_move_vsi(hw, blk, vsi, vsig, chg);
	if (status)
		return status;

	list_for_each_entry(t, lst, list) {
		 
		status = ice_add_prof_id_vsig(hw, blk, vsig, t->profile_cookie,
					      true, chg);
		if (status)
			return status;
	}

	*new_vsig = vsig;

	return 0;
}

 
static bool
ice_find_prof_vsig(struct ice_hw *hw, enum ice_block blk, u64 hdl, u16 *vsig)
{
	struct ice_vsig_prof *t;
	struct list_head lst;
	int status;

	INIT_LIST_HEAD(&lst);

	t = kzalloc(sizeof(*t), GFP_KERNEL);
	if (!t)
		return false;

	t->profile_cookie = hdl;
	list_add(&t->list, &lst);

	status = ice_find_dup_props_vsig(hw, blk, &lst, vsig);

	list_del(&t->list);
	kfree(t);

	return !status;
}

 
int
ice_add_prof_id_flow(struct ice_hw *hw, enum ice_block blk, u16 vsi, u64 hdl)
{
	struct ice_vsig_prof *tmp1, *del1;
	struct ice_chs_chg *tmp, *del;
	struct list_head union_lst;
	struct list_head chg;
	int status;
	u16 vsig;

	INIT_LIST_HEAD(&union_lst);
	INIT_LIST_HEAD(&chg);

	 
	status = ice_get_prof(hw, blk, hdl, &chg);
	if (status)
		return status;

	 
	status = ice_vsig_find_vsi(hw, blk, vsi, &vsig);
	if (!status && vsig) {
		bool only_vsi;
		u16 or_vsig;
		u16 ref;

		 
		or_vsig = vsig;

		 
		if (ice_has_prof_vsig(hw, blk, vsig, hdl)) {
			status = -EEXIST;
			goto err_ice_add_prof_id_flow;
		}

		 
		status = ice_vsig_get_ref(hw, blk, vsig, &ref);
		if (status)
			goto err_ice_add_prof_id_flow;
		only_vsi = (ref == 1);

		 
		status = ice_get_profs_vsig(hw, blk, vsig, &union_lst);
		if (status)
			goto err_ice_add_prof_id_flow;

		status = ice_add_prof_to_lst(hw, blk, &union_lst, hdl);
		if (status)
			goto err_ice_add_prof_id_flow;

		 
		status = ice_find_dup_props_vsig(hw, blk, &union_lst, &vsig);
		if (!status) {
			 
			status = ice_move_vsi(hw, blk, vsi, vsig, &chg);
			if (status)
				goto err_ice_add_prof_id_flow;

			 
			if (only_vsi) {
				status = ice_rem_vsig(hw, blk, or_vsig, &chg);
				if (status)
					goto err_ice_add_prof_id_flow;
			}
		} else if (only_vsi) {
			 
			status = ice_add_prof_id_vsig(hw, blk, vsig, hdl, false,
						      &chg);
			if (status)
				goto err_ice_add_prof_id_flow;

			 
			status = ice_adj_prof_priorities(hw, blk, vsig, &chg);
			if (status)
				goto err_ice_add_prof_id_flow;
		} else {
			 
			status = ice_create_vsig_from_lst(hw, blk, vsi,
							  &union_lst, &vsig,
							  &chg);
			if (status)
				goto err_ice_add_prof_id_flow;

			 
			status = ice_adj_prof_priorities(hw, blk, vsig, &chg);
			if (status)
				goto err_ice_add_prof_id_flow;
		}
	} else {
		 
		 
		if (ice_find_prof_vsig(hw, blk, hdl, &vsig)) {
			 
			 
			status = ice_move_vsi(hw, blk, vsi, vsig, &chg);
			if (status)
				goto err_ice_add_prof_id_flow;
		} else {
			 
			 
			status = ice_create_prof_id_vsig(hw, blk, vsi, hdl,
							 &chg);
			if (status)
				goto err_ice_add_prof_id_flow;
		}
	}

	 
	if (!status)
		status = ice_upd_prof_hw(hw, blk, &chg);

err_ice_add_prof_id_flow:
	list_for_each_entry_safe(del, tmp, &chg, list_entry) {
		list_del(&del->list_entry);
		devm_kfree(ice_hw_to_dev(hw), del);
	}

	list_for_each_entry_safe(del1, tmp1, &union_lst, list) {
		list_del(&del1->list);
		devm_kfree(ice_hw_to_dev(hw), del1);
	}

	return status;
}

 
static int
ice_rem_prof_from_list(struct ice_hw *hw, struct list_head *lst, u64 hdl)
{
	struct ice_vsig_prof *ent, *tmp;

	list_for_each_entry_safe(ent, tmp, lst, list)
		if (ent->profile_cookie == hdl) {
			list_del(&ent->list);
			devm_kfree(ice_hw_to_dev(hw), ent);
			return 0;
		}

	return -ENOENT;
}

 
int
ice_rem_prof_id_flow(struct ice_hw *hw, enum ice_block blk, u16 vsi, u64 hdl)
{
	struct ice_vsig_prof *tmp1, *del1;
	struct ice_chs_chg *tmp, *del;
	struct list_head chg, copy;
	int status;
	u16 vsig;

	INIT_LIST_HEAD(&copy);
	INIT_LIST_HEAD(&chg);

	 
	status = ice_vsig_find_vsi(hw, blk, vsi, &vsig);
	if (!status && vsig) {
		bool last_profile;
		bool only_vsi;
		u16 ref;

		 
		last_profile = ice_vsig_prof_id_count(hw, blk, vsig) == 1;
		status = ice_vsig_get_ref(hw, blk, vsig, &ref);
		if (status)
			goto err_ice_rem_prof_id_flow;
		only_vsi = (ref == 1);

		if (only_vsi) {
			 

			if (last_profile) {
				 
				status = ice_rem_vsig(hw, blk, vsig, &chg);
				if (status)
					goto err_ice_rem_prof_id_flow;
			} else {
				status = ice_rem_prof_id_vsig(hw, blk, vsig,
							      hdl, &chg);
				if (status)
					goto err_ice_rem_prof_id_flow;

				 
				status = ice_adj_prof_priorities(hw, blk, vsig,
								 &chg);
				if (status)
					goto err_ice_rem_prof_id_flow;
			}

		} else {
			 
			status = ice_get_profs_vsig(hw, blk, vsig, &copy);
			if (status)
				goto err_ice_rem_prof_id_flow;

			 
			status = ice_rem_prof_from_list(hw, &copy, hdl);
			if (status)
				goto err_ice_rem_prof_id_flow;

			if (list_empty(&copy)) {
				status = ice_move_vsi(hw, blk, vsi,
						      ICE_DEFAULT_VSIG, &chg);
				if (status)
					goto err_ice_rem_prof_id_flow;

			} else if (!ice_find_dup_props_vsig(hw, blk, &copy,
							    &vsig)) {
				 
				 
				 

				 
				status = ice_move_vsi(hw, blk, vsi, vsig, &chg);
				if (status)
					goto err_ice_rem_prof_id_flow;
			} else {
				 
				status = ice_create_vsig_from_lst(hw, blk, vsi,
								  &copy, &vsig,
								  &chg);
				if (status)
					goto err_ice_rem_prof_id_flow;

				 
				status = ice_adj_prof_priorities(hw, blk, vsig,
								 &chg);
				if (status)
					goto err_ice_rem_prof_id_flow;
			}
		}
	} else {
		status = -ENOENT;
	}

	 
	if (!status)
		status = ice_upd_prof_hw(hw, blk, &chg);

err_ice_rem_prof_id_flow:
	list_for_each_entry_safe(del, tmp, &chg, list_entry) {
		list_del(&del->list_entry);
		devm_kfree(ice_hw_to_dev(hw), del);
	}

	list_for_each_entry_safe(del1, tmp1, &copy, list) {
		list_del(&del1->list);
		devm_kfree(ice_hw_to_dev(hw), del1);
	}

	return status;
}
