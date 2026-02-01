
 

#include <linux/types.h>
#include <asm/byteorder.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/mutex.h>
#include <linux/pci.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/vmalloc.h>
#include <linux/etherdevice.h>
#include <linux/qed/qed_chain.h>
#include <linux/qed/qed_if.h>
#include "qed.h"
#include "qed_cxt.h"
#include "qed_dcbx.h"
#include "qed_dev_api.h"
#include "qed_fcoe.h"
#include "qed_hsi.h"
#include "qed_iro_hsi.h"
#include "qed_hw.h"
#include "qed_init_ops.h"
#include "qed_int.h"
#include "qed_iscsi.h"
#include "qed_ll2.h"
#include "qed_mcp.h"
#include "qed_ooo.h"
#include "qed_reg_addr.h"
#include "qed_sp.h"
#include "qed_sriov.h"
#include "qed_vf.h"
#include "qed_rdma.h"
#include "qed_nvmetcp.h"

static DEFINE_SPINLOCK(qm_lock);

 
 
struct qed_db_recovery_entry {
	struct list_head list_entry;
	void __iomem *db_addr;
	void *db_data;
	enum qed_db_rec_width db_width;
	enum qed_db_rec_space db_space;
	u8 hwfn_idx;
};

 
static void qed_db_recovery_dp_entry(struct qed_hwfn *p_hwfn,
				     struct qed_db_recovery_entry *db_entry,
				     char *action)
{
	DP_VERBOSE(p_hwfn,
		   QED_MSG_SPQ,
		   "(%s: db_entry %p, addr %p, data %p, width %s, %s space, hwfn %d)\n",
		   action,
		   db_entry,
		   db_entry->db_addr,
		   db_entry->db_data,
		   db_entry->db_width == DB_REC_WIDTH_32B ? "32b" : "64b",
		   db_entry->db_space == DB_REC_USER ? "user" : "kernel",
		   db_entry->hwfn_idx);
}

 
static bool qed_db_rec_sanity(struct qed_dev *cdev,
			      void __iomem *db_addr,
			      enum qed_db_rec_width db_width,
			      void *db_data)
{
	u32 width = (db_width == DB_REC_WIDTH_32B) ? 32 : 64;

	 
	if (db_addr < cdev->doorbells ||
	    (u8 __iomem *)db_addr + width >
	    (u8 __iomem *)cdev->doorbells + cdev->db_size) {
		WARN(true,
		     "Illegal doorbell address: %p. Legal range for doorbell addresses is [%p..%p]\n",
		     db_addr,
		     cdev->doorbells,
		     (u8 __iomem *)cdev->doorbells + cdev->db_size);
		return false;
	}

	 
	if (!db_data) {
		WARN(true, "Illegal doorbell data pointer: %p", db_data);
		return false;
	}

	return true;
}

 
static struct qed_hwfn *qed_db_rec_find_hwfn(struct qed_dev *cdev,
					     void __iomem *db_addr)
{
	struct qed_hwfn *p_hwfn;

	 
	if (cdev->num_hwfns > 1)
		p_hwfn = db_addr < cdev->hwfns[1].doorbells ?
		    &cdev->hwfns[0] : &cdev->hwfns[1];
	else
		p_hwfn = QED_LEADING_HWFN(cdev);

	return p_hwfn;
}

 
int qed_db_recovery_add(struct qed_dev *cdev,
			void __iomem *db_addr,
			void *db_data,
			enum qed_db_rec_width db_width,
			enum qed_db_rec_space db_space)
{
	struct qed_db_recovery_entry *db_entry;
	struct qed_hwfn *p_hwfn;

	 
	if (IS_VF(cdev)) {
		DP_VERBOSE(cdev,
			   QED_MSG_IOV, "db recovery - skipping VF doorbell\n");
		return 0;
	}

	 
	if (!qed_db_rec_sanity(cdev, db_addr, db_width, db_data))
		return -EINVAL;

	 
	p_hwfn = qed_db_rec_find_hwfn(cdev, db_addr);

	 
	db_entry = kzalloc(sizeof(*db_entry), GFP_KERNEL);
	if (!db_entry) {
		DP_NOTICE(cdev, "Failed to allocate a db recovery entry\n");
		return -ENOMEM;
	}

	 
	db_entry->db_addr = db_addr;
	db_entry->db_data = db_data;
	db_entry->db_width = db_width;
	db_entry->db_space = db_space;
	db_entry->hwfn_idx = p_hwfn->my_id;

	 
	qed_db_recovery_dp_entry(p_hwfn, db_entry, "Adding");

	 
	spin_lock_bh(&p_hwfn->db_recovery_info.lock);
	list_add_tail(&db_entry->list_entry, &p_hwfn->db_recovery_info.list);
	spin_unlock_bh(&p_hwfn->db_recovery_info.lock);

	return 0;
}

 
int qed_db_recovery_del(struct qed_dev *cdev,
			void __iomem *db_addr, void *db_data)
{
	struct qed_db_recovery_entry *db_entry = NULL;
	struct qed_hwfn *p_hwfn;
	int rc = -EINVAL;

	 
	if (IS_VF(cdev)) {
		DP_VERBOSE(cdev,
			   QED_MSG_IOV, "db recovery - skipping VF doorbell\n");
		return 0;
	}

	 
	p_hwfn = qed_db_rec_find_hwfn(cdev, db_addr);

	 
	spin_lock_bh(&p_hwfn->db_recovery_info.lock);
	list_for_each_entry(db_entry,
			    &p_hwfn->db_recovery_info.list, list_entry) {
		 
		if (db_entry->db_data == db_data) {
			qed_db_recovery_dp_entry(p_hwfn, db_entry, "Deleting");
			list_del(&db_entry->list_entry);
			rc = 0;
			break;
		}
	}

	spin_unlock_bh(&p_hwfn->db_recovery_info.lock);

	if (rc == -EINVAL)

		DP_NOTICE(p_hwfn,
			  "Failed to find element in list. Key (db_data addr) was %p. db_addr was %p\n",
			  db_data, db_addr);
	else
		kfree(db_entry);

	return rc;
}

 
static int qed_db_recovery_setup(struct qed_hwfn *p_hwfn)
{
	DP_VERBOSE(p_hwfn, QED_MSG_SPQ, "Setting up db recovery\n");

	 
	if (!p_hwfn->cdev->db_size) {
		DP_ERR(p_hwfn->cdev, "db_size not set\n");
		return -EINVAL;
	}

	INIT_LIST_HEAD(&p_hwfn->db_recovery_info.list);
	spin_lock_init(&p_hwfn->db_recovery_info.lock);
	p_hwfn->db_recovery_info.db_recovery_counter = 0;

	return 0;
}

 
static void qed_db_recovery_teardown(struct qed_hwfn *p_hwfn)
{
	struct qed_db_recovery_entry *db_entry = NULL;

	DP_VERBOSE(p_hwfn, QED_MSG_SPQ, "Tearing down db recovery\n");
	if (!list_empty(&p_hwfn->db_recovery_info.list)) {
		DP_VERBOSE(p_hwfn,
			   QED_MSG_SPQ,
			   "Doorbell Recovery teardown found the doorbell recovery list was not empty (Expected in disorderly driver unload (e.g. recovery) otherwise this probably means some flow forgot to db_recovery_del). Prepare to purge doorbell recovery list...\n");
		while (!list_empty(&p_hwfn->db_recovery_info.list)) {
			db_entry =
			    list_first_entry(&p_hwfn->db_recovery_info.list,
					     struct qed_db_recovery_entry,
					     list_entry);
			qed_db_recovery_dp_entry(p_hwfn, db_entry, "Purging");
			list_del(&db_entry->list_entry);
			kfree(db_entry);
		}
	}
	p_hwfn->db_recovery_info.db_recovery_counter = 0;
}

 
void qed_db_recovery_dp(struct qed_hwfn *p_hwfn)
{
	struct qed_db_recovery_entry *db_entry = NULL;

	DP_NOTICE(p_hwfn,
		  "Displaying doorbell recovery database. Counter was %d\n",
		  p_hwfn->db_recovery_info.db_recovery_counter);

	 
	spin_lock_bh(&p_hwfn->db_recovery_info.lock);
	list_for_each_entry(db_entry,
			    &p_hwfn->db_recovery_info.list, list_entry) {
		qed_db_recovery_dp_entry(p_hwfn, db_entry, "Printing");
	}

	spin_unlock_bh(&p_hwfn->db_recovery_info.lock);
}

 
static void qed_db_recovery_ring(struct qed_hwfn *p_hwfn,
				 struct qed_db_recovery_entry *db_entry)
{
	 
	if (db_entry->db_width == DB_REC_WIDTH_32B) {
		DP_VERBOSE(p_hwfn, QED_MSG_SPQ,
			   "ringing doorbell address %p data %x\n",
			   db_entry->db_addr,
			   *(u32 *)db_entry->db_data);
	} else {
		DP_VERBOSE(p_hwfn, QED_MSG_SPQ,
			   "ringing doorbell address %p data %llx\n",
			   db_entry->db_addr,
			   *(u64 *)(db_entry->db_data));
	}

	 
	if (!qed_db_rec_sanity(p_hwfn->cdev, db_entry->db_addr,
			       db_entry->db_width, db_entry->db_data))
		return;

	 
	wmb();

	 
	if (db_entry->db_width == DB_REC_WIDTH_32B)
		DIRECT_REG_WR(db_entry->db_addr,
			      *(u32 *)(db_entry->db_data));
	else
		DIRECT_REG_WR64(db_entry->db_addr,
				*(u64 *)(db_entry->db_data));

	 
	wmb();
}

 
void qed_db_recovery_execute(struct qed_hwfn *p_hwfn)
{
	struct qed_db_recovery_entry *db_entry = NULL;

	DP_NOTICE(p_hwfn, "Executing doorbell recovery. Counter was %d\n",
		  p_hwfn->db_recovery_info.db_recovery_counter);

	 
	p_hwfn->db_recovery_info.db_recovery_counter++;

	 
	spin_lock_bh(&p_hwfn->db_recovery_info.lock);
	list_for_each_entry(db_entry,
			    &p_hwfn->db_recovery_info.list, list_entry)
		qed_db_recovery_ring(p_hwfn, db_entry);
	spin_unlock_bh(&p_hwfn->db_recovery_info.lock);
}

 

 

enum qed_llh_filter_type {
	QED_LLH_FILTER_TYPE_MAC,
	QED_LLH_FILTER_TYPE_PROTOCOL,
};

struct qed_llh_mac_filter {
	u8 addr[ETH_ALEN];
};

struct qed_llh_protocol_filter {
	enum qed_llh_prot_filter_type_t type;
	u16 source_port_or_eth_type;
	u16 dest_port;
};

union qed_llh_filter {
	struct qed_llh_mac_filter mac;
	struct qed_llh_protocol_filter protocol;
};

struct qed_llh_filter_info {
	bool b_enabled;
	u32 ref_cnt;
	enum qed_llh_filter_type type;
	union qed_llh_filter filter;
};

struct qed_llh_info {
	 
	u8 num_ppfid;

#define MAX_NUM_PPFID   8
	u8 ppfid_array[MAX_NUM_PPFID];

	 
	struct qed_llh_filter_info **pp_filters;
};

static void qed_llh_free(struct qed_dev *cdev)
{
	struct qed_llh_info *p_llh_info = cdev->p_llh_info;
	u32 i;

	if (p_llh_info) {
		if (p_llh_info->pp_filters)
			for (i = 0; i < p_llh_info->num_ppfid; i++)
				kfree(p_llh_info->pp_filters[i]);

		kfree(p_llh_info->pp_filters);
	}

	kfree(p_llh_info);
	cdev->p_llh_info = NULL;
}

static int qed_llh_alloc(struct qed_dev *cdev)
{
	struct qed_llh_info *p_llh_info;
	u32 size, i;

	p_llh_info = kzalloc(sizeof(*p_llh_info), GFP_KERNEL);
	if (!p_llh_info)
		return -ENOMEM;
	cdev->p_llh_info = p_llh_info;

	for (i = 0; i < MAX_NUM_PPFID; i++) {
		if (!(cdev->ppfid_bitmap & (0x1 << i)))
			continue;

		p_llh_info->ppfid_array[p_llh_info->num_ppfid] = i;
		DP_VERBOSE(cdev, QED_MSG_SP, "ppfid_array[%d] = %u\n",
			   p_llh_info->num_ppfid, i);
		p_llh_info->num_ppfid++;
	}

	size = p_llh_info->num_ppfid * sizeof(*p_llh_info->pp_filters);
	p_llh_info->pp_filters = kzalloc(size, GFP_KERNEL);
	if (!p_llh_info->pp_filters)
		return -ENOMEM;

	size = NIG_REG_LLH_FUNC_FILTER_EN_SIZE *
	    sizeof(**p_llh_info->pp_filters);
	for (i = 0; i < p_llh_info->num_ppfid; i++) {
		p_llh_info->pp_filters[i] = kzalloc(size, GFP_KERNEL);
		if (!p_llh_info->pp_filters[i])
			return -ENOMEM;
	}

	return 0;
}

static int qed_llh_shadow_sanity(struct qed_dev *cdev,
				 u8 ppfid, u8 filter_idx, const char *action)
{
	struct qed_llh_info *p_llh_info = cdev->p_llh_info;

	if (ppfid >= p_llh_info->num_ppfid) {
		DP_NOTICE(cdev,
			  "LLH shadow [%s]: using ppfid %d while only %d ppfids are available\n",
			  action, ppfid, p_llh_info->num_ppfid);
		return -EINVAL;
	}

	if (filter_idx >= NIG_REG_LLH_FUNC_FILTER_EN_SIZE) {
		DP_NOTICE(cdev,
			  "LLH shadow [%s]: using filter_idx %d while only %d filters are available\n",
			  action, filter_idx, NIG_REG_LLH_FUNC_FILTER_EN_SIZE);
		return -EINVAL;
	}

	return 0;
}

#define QED_LLH_INVALID_FILTER_IDX      0xff

static int
qed_llh_shadow_search_filter(struct qed_dev *cdev,
			     u8 ppfid,
			     union qed_llh_filter *p_filter, u8 *p_filter_idx)
{
	struct qed_llh_info *p_llh_info = cdev->p_llh_info;
	struct qed_llh_filter_info *p_filters;
	int rc;
	u8 i;

	rc = qed_llh_shadow_sanity(cdev, ppfid, 0, "search");
	if (rc)
		return rc;

	*p_filter_idx = QED_LLH_INVALID_FILTER_IDX;

	p_filters = p_llh_info->pp_filters[ppfid];
	for (i = 0; i < NIG_REG_LLH_FUNC_FILTER_EN_SIZE; i++) {
		if (!memcmp(p_filter, &p_filters[i].filter,
			    sizeof(*p_filter))) {
			*p_filter_idx = i;
			break;
		}
	}

	return 0;
}

static int
qed_llh_shadow_get_free_idx(struct qed_dev *cdev, u8 ppfid, u8 *p_filter_idx)
{
	struct qed_llh_info *p_llh_info = cdev->p_llh_info;
	struct qed_llh_filter_info *p_filters;
	int rc;
	u8 i;

	rc = qed_llh_shadow_sanity(cdev, ppfid, 0, "get_free_idx");
	if (rc)
		return rc;

	*p_filter_idx = QED_LLH_INVALID_FILTER_IDX;

	p_filters = p_llh_info->pp_filters[ppfid];
	for (i = 0; i < NIG_REG_LLH_FUNC_FILTER_EN_SIZE; i++) {
		if (!p_filters[i].b_enabled) {
			*p_filter_idx = i;
			break;
		}
	}

	return 0;
}

static int
__qed_llh_shadow_add_filter(struct qed_dev *cdev,
			    u8 ppfid,
			    u8 filter_idx,
			    enum qed_llh_filter_type type,
			    union qed_llh_filter *p_filter, u32 *p_ref_cnt)
{
	struct qed_llh_info *p_llh_info = cdev->p_llh_info;
	struct qed_llh_filter_info *p_filters;
	int rc;

	rc = qed_llh_shadow_sanity(cdev, ppfid, filter_idx, "add");
	if (rc)
		return rc;

	p_filters = p_llh_info->pp_filters[ppfid];
	if (!p_filters[filter_idx].ref_cnt) {
		p_filters[filter_idx].b_enabled = true;
		p_filters[filter_idx].type = type;
		memcpy(&p_filters[filter_idx].filter, p_filter,
		       sizeof(p_filters[filter_idx].filter));
	}

	*p_ref_cnt = ++p_filters[filter_idx].ref_cnt;

	return 0;
}

static int
qed_llh_shadow_add_filter(struct qed_dev *cdev,
			  u8 ppfid,
			  enum qed_llh_filter_type type,
			  union qed_llh_filter *p_filter,
			  u8 *p_filter_idx, u32 *p_ref_cnt)
{
	int rc;

	 
	rc = qed_llh_shadow_search_filter(cdev, ppfid, p_filter, p_filter_idx);
	if (rc)
		return rc;

	 
	if (*p_filter_idx == QED_LLH_INVALID_FILTER_IDX) {
		rc = qed_llh_shadow_get_free_idx(cdev, ppfid, p_filter_idx);
		if (rc)
			return rc;
	}

	 
	if (*p_filter_idx == QED_LLH_INVALID_FILTER_IDX) {
		DP_NOTICE(cdev,
			  "Failed to find an empty LLH filter to utilize [ppfid %d]\n",
			  ppfid);
		return -EINVAL;
	}

	return __qed_llh_shadow_add_filter(cdev, ppfid, *p_filter_idx, type,
					   p_filter, p_ref_cnt);
}

static int
__qed_llh_shadow_remove_filter(struct qed_dev *cdev,
			       u8 ppfid, u8 filter_idx, u32 *p_ref_cnt)
{
	struct qed_llh_info *p_llh_info = cdev->p_llh_info;
	struct qed_llh_filter_info *p_filters;
	int rc;

	rc = qed_llh_shadow_sanity(cdev, ppfid, filter_idx, "remove");
	if (rc)
		return rc;

	p_filters = p_llh_info->pp_filters[ppfid];
	if (!p_filters[filter_idx].ref_cnt) {
		DP_NOTICE(cdev,
			  "LLH shadow: trying to remove a filter with ref_cnt=0\n");
		return -EINVAL;
	}

	*p_ref_cnt = --p_filters[filter_idx].ref_cnt;
	if (!p_filters[filter_idx].ref_cnt)
		memset(&p_filters[filter_idx],
		       0, sizeof(p_filters[filter_idx]));

	return 0;
}

static int
qed_llh_shadow_remove_filter(struct qed_dev *cdev,
			     u8 ppfid,
			     union qed_llh_filter *p_filter,
			     u8 *p_filter_idx, u32 *p_ref_cnt)
{
	int rc;

	rc = qed_llh_shadow_search_filter(cdev, ppfid, p_filter, p_filter_idx);
	if (rc)
		return rc;

	 
	if (*p_filter_idx == QED_LLH_INVALID_FILTER_IDX) {
		DP_NOTICE(cdev, "Failed to find a filter in the LLH shadow\n");
		return -EINVAL;
	}

	return __qed_llh_shadow_remove_filter(cdev, ppfid, *p_filter_idx,
					      p_ref_cnt);
}

static int qed_llh_abs_ppfid(struct qed_dev *cdev, u8 ppfid, u8 *p_abs_ppfid)
{
	struct qed_llh_info *p_llh_info = cdev->p_llh_info;

	if (ppfid >= p_llh_info->num_ppfid) {
		DP_NOTICE(cdev,
			  "ppfid %d is not valid, available indices are 0..%d\n",
			  ppfid, p_llh_info->num_ppfid - 1);
		*p_abs_ppfid = 0;
		return -EINVAL;
	}

	*p_abs_ppfid = p_llh_info->ppfid_array[ppfid];

	return 0;
}

static int
qed_llh_set_engine_affin(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt)
{
	struct qed_dev *cdev = p_hwfn->cdev;
	enum qed_eng eng;
	u8 ppfid;
	int rc;

	rc = qed_mcp_get_engine_config(p_hwfn, p_ptt);
	if (rc != 0 && rc != -EOPNOTSUPP) {
		DP_NOTICE(p_hwfn,
			  "Failed to get the engine affinity configuration\n");
		return rc;
	}

	 
	if (QED_IS_ROCE_PERSONALITY(p_hwfn)) {
		eng = cdev->fir_affin ? QED_ENG1 : QED_ENG0;
		rc = qed_llh_set_roce_affinity(cdev, eng);
		if (rc) {
			DP_NOTICE(cdev,
				  "Failed to set the RoCE engine affinity\n");
			return rc;
		}

		DP_VERBOSE(cdev,
			   QED_MSG_SP,
			   "LLH: Set the engine affinity of RoCE packets as %d\n",
			   eng);
	}

	 
	if (QED_IS_FCOE_PERSONALITY(p_hwfn) || QED_IS_ISCSI_PERSONALITY(p_hwfn) ||
	    QED_IS_NVMETCP_PERSONALITY(p_hwfn))
		eng = cdev->fir_affin ? QED_ENG1 : QED_ENG0;
	else			 
		eng = QED_BOTH_ENG;

	for (ppfid = 0; ppfid < cdev->p_llh_info->num_ppfid; ppfid++) {
		rc = qed_llh_set_ppfid_affinity(cdev, ppfid, eng);
		if (rc) {
			DP_NOTICE(cdev,
				  "Failed to set the engine affinity of ppfid %d\n",
				  ppfid);
			return rc;
		}
	}

	DP_VERBOSE(cdev, QED_MSG_SP,
		   "LLH: Set the engine affinity of non-RoCE packets as %d\n",
		   eng);

	return 0;
}

static int qed_llh_hw_init_pf(struct qed_hwfn *p_hwfn,
			      struct qed_ptt *p_ptt)
{
	struct qed_dev *cdev = p_hwfn->cdev;
	u8 ppfid, abs_ppfid;
	int rc;

	for (ppfid = 0; ppfid < cdev->p_llh_info->num_ppfid; ppfid++) {
		u32 addr;

		rc = qed_llh_abs_ppfid(cdev, ppfid, &abs_ppfid);
		if (rc)
			return rc;

		addr = NIG_REG_LLH_PPFID2PFID_TBL_0 + abs_ppfid * 0x4;
		qed_wr(p_hwfn, p_ptt, addr, p_hwfn->rel_pf_id);
	}

	if (test_bit(QED_MF_LLH_MAC_CLSS, &cdev->mf_bits) &&
	    !QED_IS_FCOE_PERSONALITY(p_hwfn)) {
		rc = qed_llh_add_mac_filter(cdev, 0,
					    p_hwfn->hw_info.hw_mac_addr);
		if (rc)
			DP_NOTICE(cdev,
				  "Failed to add an LLH filter with the primary MAC\n");
	}

	if (QED_IS_CMT(cdev)) {
		rc = qed_llh_set_engine_affin(p_hwfn, p_ptt);
		if (rc)
			return rc;
	}

	return 0;
}

u8 qed_llh_get_num_ppfid(struct qed_dev *cdev)
{
	return cdev->p_llh_info->num_ppfid;
}

#define NIG_REG_PPF_TO_ENGINE_SEL_ROCE_MASK             0x3
#define NIG_REG_PPF_TO_ENGINE_SEL_ROCE_SHIFT            0
#define NIG_REG_PPF_TO_ENGINE_SEL_NON_ROCE_MASK         0x3
#define NIG_REG_PPF_TO_ENGINE_SEL_NON_ROCE_SHIFT        2

int qed_llh_set_ppfid_affinity(struct qed_dev *cdev, u8 ppfid, enum qed_eng eng)
{
	struct qed_hwfn *p_hwfn = QED_LEADING_HWFN(cdev);
	struct qed_ptt *p_ptt = qed_ptt_acquire(p_hwfn);
	u32 addr, val, eng_sel;
	u8 abs_ppfid;
	int rc = 0;

	if (!p_ptt)
		return -EAGAIN;

	if (!QED_IS_CMT(cdev))
		goto out;

	rc = qed_llh_abs_ppfid(cdev, ppfid, &abs_ppfid);
	if (rc)
		goto out;

	switch (eng) {
	case QED_ENG0:
		eng_sel = 0;
		break;
	case QED_ENG1:
		eng_sel = 1;
		break;
	case QED_BOTH_ENG:
		eng_sel = 2;
		break;
	default:
		DP_NOTICE(cdev, "Invalid affinity value for ppfid [%d]\n", eng);
		rc = -EINVAL;
		goto out;
	}

	addr = NIG_REG_PPF_TO_ENGINE_SEL + abs_ppfid * 0x4;
	val = qed_rd(p_hwfn, p_ptt, addr);
	SET_FIELD(val, NIG_REG_PPF_TO_ENGINE_SEL_NON_ROCE, eng_sel);
	qed_wr(p_hwfn, p_ptt, addr, val);

	 
	if (!ppfid && QED_IS_IWARP_PERSONALITY(p_hwfn))
		cdev->iwarp_affin = (eng == QED_ENG1) ? 1 : 0;
out:
	qed_ptt_release(p_hwfn, p_ptt);

	return rc;
}

int qed_llh_set_roce_affinity(struct qed_dev *cdev, enum qed_eng eng)
{
	struct qed_hwfn *p_hwfn = QED_LEADING_HWFN(cdev);
	struct qed_ptt *p_ptt = qed_ptt_acquire(p_hwfn);
	u32 addr, val, eng_sel;
	u8 ppfid, abs_ppfid;
	int rc = 0;

	if (!p_ptt)
		return -EAGAIN;

	if (!QED_IS_CMT(cdev))
		goto out;

	switch (eng) {
	case QED_ENG0:
		eng_sel = 0;
		break;
	case QED_ENG1:
		eng_sel = 1;
		break;
	case QED_BOTH_ENG:
		eng_sel = 2;
		qed_wr(p_hwfn, p_ptt, NIG_REG_LLH_ENG_CLS_ROCE_QP_SEL,
		       0xf);   
		break;
	default:
		DP_NOTICE(cdev, "Invalid affinity value for RoCE [%d]\n", eng);
		rc = -EINVAL;
		goto out;
	}

	for (ppfid = 0; ppfid < cdev->p_llh_info->num_ppfid; ppfid++) {
		rc = qed_llh_abs_ppfid(cdev, ppfid, &abs_ppfid);
		if (rc)
			goto out;

		addr = NIG_REG_PPF_TO_ENGINE_SEL + abs_ppfid * 0x4;
		val = qed_rd(p_hwfn, p_ptt, addr);
		SET_FIELD(val, NIG_REG_PPF_TO_ENGINE_SEL_ROCE, eng_sel);
		qed_wr(p_hwfn, p_ptt, addr, val);
	}
out:
	qed_ptt_release(p_hwfn, p_ptt);

	return rc;
}

struct qed_llh_filter_details {
	u64 value;
	u32 mode;
	u32 protocol_type;
	u32 hdr_sel;
	u32 enable;
};

static int
qed_llh_access_filter(struct qed_hwfn *p_hwfn,
		      struct qed_ptt *p_ptt,
		      u8 abs_ppfid,
		      u8 filter_idx,
		      struct qed_llh_filter_details *p_details)
{
	struct qed_dmae_params params = {0};
	u32 addr;
	u8 pfid;
	int rc;

	 
	if (QED_IS_BB(p_hwfn->cdev))
		pfid = abs_ppfid;
	else
		pfid = abs_ppfid * p_hwfn->cdev->num_ports_in_engine +
		    MFW_PORT(p_hwfn);

	 
	if (!p_details->enable) {
		qed_fid_pretend(p_hwfn, p_ptt,
				pfid << PXP_PRETEND_CONCRETE_FID_PFID_SHIFT);

		addr = NIG_REG_LLH_FUNC_FILTER_EN + filter_idx * 0x4;
		qed_wr(p_hwfn, p_ptt, addr, p_details->enable);

		qed_fid_pretend(p_hwfn, p_ptt,
				p_hwfn->rel_pf_id <<
				PXP_PRETEND_CONCRETE_FID_PFID_SHIFT);
	}

	 
	addr = NIG_REG_LLH_FUNC_FILTER_VALUE + 2 * filter_idx * 0x4;

	SET_FIELD(params.flags, QED_DMAE_PARAMS_DST_PF_VALID, 0x1);
	params.dst_pfid = pfid;
	rc = qed_dmae_host2grc(p_hwfn,
			       p_ptt,
			       (u64)(uintptr_t)&p_details->value,
			       addr, 2  ,
			       &params);
	if (rc)
		return rc;

	qed_fid_pretend(p_hwfn, p_ptt,
			pfid << PXP_PRETEND_CONCRETE_FID_PFID_SHIFT);

	 
	addr = NIG_REG_LLH_FUNC_FILTER_MODE + filter_idx * 0x4;
	qed_wr(p_hwfn, p_ptt, addr, p_details->mode);

	 
	addr = NIG_REG_LLH_FUNC_FILTER_PROTOCOL_TYPE + filter_idx * 0x4;
	qed_wr(p_hwfn, p_ptt, addr, p_details->protocol_type);

	 
	addr = NIG_REG_LLH_FUNC_FILTER_HDR_SEL + filter_idx * 0x4;
	qed_wr(p_hwfn, p_ptt, addr, p_details->hdr_sel);

	 
	if (p_details->enable) {
		addr = NIG_REG_LLH_FUNC_FILTER_EN + filter_idx * 0x4;
		qed_wr(p_hwfn, p_ptt, addr, p_details->enable);
	}

	qed_fid_pretend(p_hwfn, p_ptt,
			p_hwfn->rel_pf_id <<
			PXP_PRETEND_CONCRETE_FID_PFID_SHIFT);

	return 0;
}

static int
qed_llh_add_filter(struct qed_hwfn *p_hwfn,
		   struct qed_ptt *p_ptt,
		   u8 abs_ppfid,
		   u8 filter_idx, u8 filter_prot_type, u32 high, u32 low)
{
	struct qed_llh_filter_details filter_details;

	filter_details.enable = 1;
	filter_details.value = ((u64)high << 32) | low;
	filter_details.hdr_sel = 0;
	filter_details.protocol_type = filter_prot_type;
	 
	filter_details.mode = filter_prot_type ? 1 : 0;

	return qed_llh_access_filter(p_hwfn, p_ptt, abs_ppfid, filter_idx,
				     &filter_details);
}

static int
qed_llh_remove_filter(struct qed_hwfn *p_hwfn,
		      struct qed_ptt *p_ptt, u8 abs_ppfid, u8 filter_idx)
{
	struct qed_llh_filter_details filter_details = {0};

	return qed_llh_access_filter(p_hwfn, p_ptt, abs_ppfid, filter_idx,
				     &filter_details);
}

int qed_llh_add_mac_filter(struct qed_dev *cdev,
			   u8 ppfid, const u8 mac_addr[ETH_ALEN])
{
	struct qed_hwfn *p_hwfn = QED_LEADING_HWFN(cdev);
	struct qed_ptt *p_ptt = qed_ptt_acquire(p_hwfn);
	union qed_llh_filter filter = {};
	u8 filter_idx, abs_ppfid = 0;
	u32 high, low, ref_cnt;
	int rc = 0;

	if (!p_ptt)
		return -EAGAIN;

	if (!test_bit(QED_MF_LLH_MAC_CLSS, &cdev->mf_bits))
		goto out;

	memcpy(filter.mac.addr, mac_addr, ETH_ALEN);
	rc = qed_llh_shadow_add_filter(cdev, ppfid,
				       QED_LLH_FILTER_TYPE_MAC,
				       &filter, &filter_idx, &ref_cnt);
	if (rc)
		goto err;

	 
	if (ref_cnt == 1) {
		rc = qed_llh_abs_ppfid(cdev, ppfid, &abs_ppfid);
		if (rc)
			goto err;

		high = mac_addr[1] | (mac_addr[0] << 8);
		low = mac_addr[5] | (mac_addr[4] << 8) | (mac_addr[3] << 16) |
		      (mac_addr[2] << 24);
		rc = qed_llh_add_filter(p_hwfn, p_ptt, abs_ppfid, filter_idx,
					0, high, low);
		if (rc)
			goto err;
	}

	DP_VERBOSE(cdev,
		   QED_MSG_SP,
		   "LLH: Added MAC filter [%pM] to ppfid %hhd [abs %hhd] at idx %hhd [ref_cnt %d]\n",
		   mac_addr, ppfid, abs_ppfid, filter_idx, ref_cnt);

	goto out;

err:	DP_NOTICE(cdev,
		  "LLH: Failed to add MAC filter [%pM] to ppfid %hhd\n",
		  mac_addr, ppfid);
out:
	qed_ptt_release(p_hwfn, p_ptt);

	return rc;
}

static int
qed_llh_protocol_filter_stringify(struct qed_dev *cdev,
				  enum qed_llh_prot_filter_type_t type,
				  u16 source_port_or_eth_type,
				  u16 dest_port, u8 *str, size_t str_len)
{
	switch (type) {
	case QED_LLH_FILTER_ETHERTYPE:
		snprintf(str, str_len, "Ethertype 0x%04x",
			 source_port_or_eth_type);
		break;
	case QED_LLH_FILTER_TCP_SRC_PORT:
		snprintf(str, str_len, "TCP src port 0x%04x",
			 source_port_or_eth_type);
		break;
	case QED_LLH_FILTER_UDP_SRC_PORT:
		snprintf(str, str_len, "UDP src port 0x%04x",
			 source_port_or_eth_type);
		break;
	case QED_LLH_FILTER_TCP_DEST_PORT:
		snprintf(str, str_len, "TCP dst port 0x%04x", dest_port);
		break;
	case QED_LLH_FILTER_UDP_DEST_PORT:
		snprintf(str, str_len, "UDP dst port 0x%04x", dest_port);
		break;
	case QED_LLH_FILTER_TCP_SRC_AND_DEST_PORT:
		snprintf(str, str_len, "TCP src/dst ports 0x%04x/0x%04x",
			 source_port_or_eth_type, dest_port);
		break;
	case QED_LLH_FILTER_UDP_SRC_AND_DEST_PORT:
		snprintf(str, str_len, "UDP src/dst ports 0x%04x/0x%04x",
			 source_port_or_eth_type, dest_port);
		break;
	default:
		DP_NOTICE(cdev,
			  "Non valid LLH protocol filter type %d\n", type);
		return -EINVAL;
	}

	return 0;
}

static int
qed_llh_protocol_filter_to_hilo(struct qed_dev *cdev,
				enum qed_llh_prot_filter_type_t type,
				u16 source_port_or_eth_type,
				u16 dest_port, u32 *p_high, u32 *p_low)
{
	*p_high = 0;
	*p_low = 0;

	switch (type) {
	case QED_LLH_FILTER_ETHERTYPE:
		*p_high = source_port_or_eth_type;
		break;
	case QED_LLH_FILTER_TCP_SRC_PORT:
	case QED_LLH_FILTER_UDP_SRC_PORT:
		*p_low = source_port_or_eth_type << 16;
		break;
	case QED_LLH_FILTER_TCP_DEST_PORT:
	case QED_LLH_FILTER_UDP_DEST_PORT:
		*p_low = dest_port;
		break;
	case QED_LLH_FILTER_TCP_SRC_AND_DEST_PORT:
	case QED_LLH_FILTER_UDP_SRC_AND_DEST_PORT:
		*p_low = (source_port_or_eth_type << 16) | dest_port;
		break;
	default:
		DP_NOTICE(cdev,
			  "Non valid LLH protocol filter type %d\n", type);
		return -EINVAL;
	}

	return 0;
}

int
qed_llh_add_protocol_filter(struct qed_dev *cdev,
			    u8 ppfid,
			    enum qed_llh_prot_filter_type_t type,
			    u16 source_port_or_eth_type, u16 dest_port)
{
	struct qed_hwfn *p_hwfn = QED_LEADING_HWFN(cdev);
	struct qed_ptt *p_ptt = qed_ptt_acquire(p_hwfn);
	u8 filter_idx, abs_ppfid, str[32], type_bitmap;
	union qed_llh_filter filter = {};
	u32 high, low, ref_cnt;
	int rc = 0;

	if (!p_ptt)
		return -EAGAIN;

	if (!test_bit(QED_MF_LLH_PROTO_CLSS, &cdev->mf_bits))
		goto out;

	rc = qed_llh_protocol_filter_stringify(cdev, type,
					       source_port_or_eth_type,
					       dest_port, str, sizeof(str));
	if (rc)
		goto err;

	filter.protocol.type = type;
	filter.protocol.source_port_or_eth_type = source_port_or_eth_type;
	filter.protocol.dest_port = dest_port;
	rc = qed_llh_shadow_add_filter(cdev,
				       ppfid,
				       QED_LLH_FILTER_TYPE_PROTOCOL,
				       &filter, &filter_idx, &ref_cnt);
	if (rc)
		goto err;

	rc = qed_llh_abs_ppfid(cdev, ppfid, &abs_ppfid);
	if (rc)
		goto err;

	 
	if (ref_cnt == 1) {
		rc = qed_llh_protocol_filter_to_hilo(cdev, type,
						     source_port_or_eth_type,
						     dest_port, &high, &low);
		if (rc)
			goto err;

		type_bitmap = 0x1 << type;
		rc = qed_llh_add_filter(p_hwfn, p_ptt, abs_ppfid,
					filter_idx, type_bitmap, high, low);
		if (rc)
			goto err;
	}

	DP_VERBOSE(cdev,
		   QED_MSG_SP,
		   "LLH: Added protocol filter [%s] to ppfid %hhd [abs %hhd] at idx %hhd [ref_cnt %d]\n",
		   str, ppfid, abs_ppfid, filter_idx, ref_cnt);

	goto out;

err:	DP_NOTICE(p_hwfn,
		  "LLH: Failed to add protocol filter [%s] to ppfid %hhd\n",
		  str, ppfid);
out:
	qed_ptt_release(p_hwfn, p_ptt);

	return rc;
}

void qed_llh_remove_mac_filter(struct qed_dev *cdev,
			       u8 ppfid, u8 mac_addr[ETH_ALEN])
{
	struct qed_hwfn *p_hwfn = QED_LEADING_HWFN(cdev);
	struct qed_ptt *p_ptt = qed_ptt_acquire(p_hwfn);
	union qed_llh_filter filter = {};
	u8 filter_idx, abs_ppfid;
	int rc = 0;
	u32 ref_cnt;

	if (!p_ptt)
		return;

	if (!test_bit(QED_MF_LLH_MAC_CLSS, &cdev->mf_bits))
		goto out;

	if (QED_IS_NVMETCP_PERSONALITY(p_hwfn))
		return;

	ether_addr_copy(filter.mac.addr, mac_addr);
	rc = qed_llh_shadow_remove_filter(cdev, ppfid, &filter, &filter_idx,
					  &ref_cnt);
	if (rc)
		goto err;

	rc = qed_llh_abs_ppfid(cdev, ppfid, &abs_ppfid);
	if (rc)
		goto err;

	 
	if (!ref_cnt) {
		rc = qed_llh_remove_filter(p_hwfn, p_ptt, abs_ppfid,
					   filter_idx);
		if (rc)
			goto err;
	}

	DP_VERBOSE(cdev,
		   QED_MSG_SP,
		   "LLH: Removed MAC filter [%pM] from ppfid %hhd [abs %hhd] at idx %hhd [ref_cnt %d]\n",
		   mac_addr, ppfid, abs_ppfid, filter_idx, ref_cnt);

	goto out;

err:	DP_NOTICE(cdev,
		  "LLH: Failed to remove MAC filter [%pM] from ppfid %hhd\n",
		  mac_addr, ppfid);
out:
	qed_ptt_release(p_hwfn, p_ptt);
}

void qed_llh_remove_protocol_filter(struct qed_dev *cdev,
				    u8 ppfid,
				    enum qed_llh_prot_filter_type_t type,
				    u16 source_port_or_eth_type, u16 dest_port)
{
	struct qed_hwfn *p_hwfn = QED_LEADING_HWFN(cdev);
	struct qed_ptt *p_ptt = qed_ptt_acquire(p_hwfn);
	u8 filter_idx, abs_ppfid, str[32];
	union qed_llh_filter filter = {};
	int rc = 0;
	u32 ref_cnt;

	if (!p_ptt)
		return;

	if (!test_bit(QED_MF_LLH_PROTO_CLSS, &cdev->mf_bits))
		goto out;

	rc = qed_llh_protocol_filter_stringify(cdev, type,
					       source_port_or_eth_type,
					       dest_port, str, sizeof(str));
	if (rc)
		goto err;

	filter.protocol.type = type;
	filter.protocol.source_port_or_eth_type = source_port_or_eth_type;
	filter.protocol.dest_port = dest_port;
	rc = qed_llh_shadow_remove_filter(cdev, ppfid, &filter, &filter_idx,
					  &ref_cnt);
	if (rc)
		goto err;

	rc = qed_llh_abs_ppfid(cdev, ppfid, &abs_ppfid);
	if (rc)
		goto err;

	 
	if (!ref_cnt) {
		rc = qed_llh_remove_filter(p_hwfn, p_ptt, abs_ppfid,
					   filter_idx);
		if (rc)
			goto err;
	}

	DP_VERBOSE(cdev,
		   QED_MSG_SP,
		   "LLH: Removed protocol filter [%s] from ppfid %hhd [abs %hhd] at idx %hhd [ref_cnt %d]\n",
		   str, ppfid, abs_ppfid, filter_idx, ref_cnt);

	goto out;

err:	DP_NOTICE(cdev,
		  "LLH: Failed to remove protocol filter [%s] from ppfid %hhd\n",
		  str, ppfid);
out:
	qed_ptt_release(p_hwfn, p_ptt);
}

 

#define QED_MIN_DPIS            (4)
#define QED_MIN_PWM_REGION      (QED_WID_SIZE * QED_MIN_DPIS)

static u32 qed_hw_bar_size(struct qed_hwfn *p_hwfn,
			   struct qed_ptt *p_ptt, enum BAR_ID bar_id)
{
	u32 bar_reg = (bar_id == BAR_ID_0 ?
		       PGLUE_B_REG_PF_BAR0_SIZE : PGLUE_B_REG_PF_BAR1_SIZE);
	u32 val;

	if (IS_VF(p_hwfn->cdev))
		return qed_vf_hw_bar_size(p_hwfn, bar_id);

	val = qed_rd(p_hwfn, p_ptt, bar_reg);
	if (val)
		return 1 << (val + 15);

	 
	if (p_hwfn->cdev->num_hwfns > 1) {
		DP_INFO(p_hwfn,
			"BAR size not configured. Assuming BAR size of 256kB for GRC and 512kB for DB\n");
			return BAR_ID_0 ? 256 * 1024 : 512 * 1024;
	} else {
		DP_INFO(p_hwfn,
			"BAR size not configured. Assuming BAR size of 512kB for GRC and 512kB for DB\n");
			return 512 * 1024;
	}
}

void qed_init_dp(struct qed_dev *cdev, u32 dp_module, u8 dp_level)
{
	u32 i;

	cdev->dp_level = dp_level;
	cdev->dp_module = dp_module;
	for (i = 0; i < MAX_HWFNS_PER_DEVICE; i++) {
		struct qed_hwfn *p_hwfn = &cdev->hwfns[i];

		p_hwfn->dp_level = dp_level;
		p_hwfn->dp_module = dp_module;
	}
}

void qed_init_struct(struct qed_dev *cdev)
{
	u8 i;

	for (i = 0; i < MAX_HWFNS_PER_DEVICE; i++) {
		struct qed_hwfn *p_hwfn = &cdev->hwfns[i];

		p_hwfn->cdev = cdev;
		p_hwfn->my_id = i;
		p_hwfn->b_active = false;

		mutex_init(&p_hwfn->dmae_info.mutex);
	}

	 
	cdev->hwfns[0].b_active = true;

	 
	cdev->cache_shift = 7;
}

static void qed_qm_info_free(struct qed_hwfn *p_hwfn)
{
	struct qed_qm_info *qm_info = &p_hwfn->qm_info;

	kfree(qm_info->qm_pq_params);
	qm_info->qm_pq_params = NULL;
	kfree(qm_info->qm_vport_params);
	qm_info->qm_vport_params = NULL;
	kfree(qm_info->qm_port_params);
	qm_info->qm_port_params = NULL;
	kfree(qm_info->wfq_data);
	qm_info->wfq_data = NULL;
}

static void qed_dbg_user_data_free(struct qed_hwfn *p_hwfn)
{
	kfree(p_hwfn->dbg_user_info);
	p_hwfn->dbg_user_info = NULL;
}

void qed_resc_free(struct qed_dev *cdev)
{
	struct qed_rdma_info *rdma_info;
	struct qed_hwfn *p_hwfn;
	int i;

	if (IS_VF(cdev)) {
		for_each_hwfn(cdev, i)
			qed_l2_free(&cdev->hwfns[i]);
		return;
	}

	kfree(cdev->fw_data);
	cdev->fw_data = NULL;

	kfree(cdev->reset_stats);
	cdev->reset_stats = NULL;

	qed_llh_free(cdev);

	for_each_hwfn(cdev, i) {
		p_hwfn = cdev->hwfns + i;
		rdma_info = p_hwfn->p_rdma_info;

		qed_cxt_mngr_free(p_hwfn);
		qed_qm_info_free(p_hwfn);
		qed_spq_free(p_hwfn);
		qed_eq_free(p_hwfn);
		qed_consq_free(p_hwfn);
		qed_int_free(p_hwfn);
#ifdef CONFIG_QED_LL2
		qed_ll2_free(p_hwfn);
#endif
		if (p_hwfn->hw_info.personality == QED_PCI_FCOE)
			qed_fcoe_free(p_hwfn);

		if (p_hwfn->hw_info.personality == QED_PCI_ISCSI) {
			qed_iscsi_free(p_hwfn);
			qed_ooo_free(p_hwfn);
		}

		if (p_hwfn->hw_info.personality == QED_PCI_NVMETCP) {
			qed_nvmetcp_free(p_hwfn);
			qed_ooo_free(p_hwfn);
		}

		if (QED_IS_RDMA_PERSONALITY(p_hwfn) && rdma_info) {
			qed_spq_unregister_async_cb(p_hwfn, rdma_info->proto);
			qed_rdma_info_free(p_hwfn);
		}

		qed_spq_unregister_async_cb(p_hwfn, PROTOCOLID_COMMON);
		qed_iov_free(p_hwfn);
		qed_l2_free(p_hwfn);
		qed_dmae_info_free(p_hwfn);
		qed_dcbx_info_free(p_hwfn);
		qed_dbg_user_data_free(p_hwfn);
		qed_fw_overlay_mem_free(p_hwfn, &p_hwfn->fw_overlay_mem);

		 
		qed_db_recovery_teardown(p_hwfn);
	}
}

 
#define ACTIVE_TCS_BMAP 0x9f
#define ACTIVE_TCS_BMAP_4PORT_K2 0xf

 
static u32 qed_get_pq_flags(struct qed_hwfn *p_hwfn)
{
	u32 flags;

	 
	flags = PQ_FLAGS_LB;

	 
	if (IS_QED_SRIOV(p_hwfn->cdev))
		flags |= PQ_FLAGS_VFS;

	 
	switch (p_hwfn->hw_info.personality) {
	case QED_PCI_ETH:
		flags |= PQ_FLAGS_MCOS;
		break;
	case QED_PCI_FCOE:
		flags |= PQ_FLAGS_OFLD;
		break;
	case QED_PCI_ISCSI:
	case QED_PCI_NVMETCP:
		flags |= PQ_FLAGS_ACK | PQ_FLAGS_OOO | PQ_FLAGS_OFLD;
		break;
	case QED_PCI_ETH_ROCE:
		flags |= PQ_FLAGS_MCOS | PQ_FLAGS_OFLD | PQ_FLAGS_LLT;
		if (IS_QED_MULTI_TC_ROCE(p_hwfn))
			flags |= PQ_FLAGS_MTC;
		break;
	case QED_PCI_ETH_IWARP:
		flags |= PQ_FLAGS_MCOS | PQ_FLAGS_ACK | PQ_FLAGS_OOO |
		    PQ_FLAGS_OFLD;
		break;
	default:
		DP_ERR(p_hwfn,
		       "unknown personality %d\n", p_hwfn->hw_info.personality);
		return 0;
	}

	return flags;
}

 
static u8 qed_init_qm_get_num_tcs(struct qed_hwfn *p_hwfn)
{
	return p_hwfn->hw_info.num_hw_tc;
}

static u16 qed_init_qm_get_num_vfs(struct qed_hwfn *p_hwfn)
{
	return IS_QED_SRIOV(p_hwfn->cdev) ?
	       p_hwfn->cdev->p_iov_info->total_vfs : 0;
}

static u8 qed_init_qm_get_num_mtc_tcs(struct qed_hwfn *p_hwfn)
{
	u32 pq_flags = qed_get_pq_flags(p_hwfn);

	if (!(PQ_FLAGS_MTC & pq_flags))
		return 1;

	return qed_init_qm_get_num_tcs(p_hwfn);
}

#define NUM_DEFAULT_RLS 1

static u16 qed_init_qm_get_num_pf_rls(struct qed_hwfn *p_hwfn)
{
	u16 num_pf_rls, num_vfs = qed_init_qm_get_num_vfs(p_hwfn);

	 
	num_pf_rls = (u16)min_t(u32, RESC_NUM(p_hwfn, QED_RL),
				RESC_NUM(p_hwfn, QED_VPORT));

	 
	if (num_pf_rls < num_vfs + NUM_DEFAULT_RLS)
		return 0;

	 
	num_pf_rls -= num_vfs + NUM_DEFAULT_RLS;

	return num_pf_rls;
}

static u16 qed_init_qm_get_num_vports(struct qed_hwfn *p_hwfn)
{
	u32 pq_flags = qed_get_pq_flags(p_hwfn);

	 
	return (!!(PQ_FLAGS_RLS & pq_flags)) *
	       qed_init_qm_get_num_pf_rls(p_hwfn) +
	       (!!(PQ_FLAGS_VFS & pq_flags)) *
	       qed_init_qm_get_num_vfs(p_hwfn) + 1;
}

 
static u16 qed_init_qm_get_num_pqs(struct qed_hwfn *p_hwfn)
{
	u32 pq_flags = qed_get_pq_flags(p_hwfn);

	return (!!(PQ_FLAGS_RLS & pq_flags)) *
	       qed_init_qm_get_num_pf_rls(p_hwfn) +
	       (!!(PQ_FLAGS_MCOS & pq_flags)) *
	       qed_init_qm_get_num_tcs(p_hwfn) +
	       (!!(PQ_FLAGS_LB & pq_flags)) + (!!(PQ_FLAGS_OOO & pq_flags)) +
	       (!!(PQ_FLAGS_ACK & pq_flags)) +
	       (!!(PQ_FLAGS_OFLD & pq_flags)) *
	       qed_init_qm_get_num_mtc_tcs(p_hwfn) +
	       (!!(PQ_FLAGS_LLT & pq_flags)) *
	       qed_init_qm_get_num_mtc_tcs(p_hwfn) +
	       (!!(PQ_FLAGS_VFS & pq_flags)) * qed_init_qm_get_num_vfs(p_hwfn);
}

 
static void qed_init_qm_params(struct qed_hwfn *p_hwfn)
{
	struct qed_qm_info *qm_info = &p_hwfn->qm_info;
	bool four_port;

	 
	qm_info->start_pq = (u16)RESC_START(p_hwfn, QED_PQ);
	qm_info->start_vport = (u8)RESC_START(p_hwfn, QED_VPORT);

	 
	qm_info->vport_rl_en = true;
	qm_info->vport_wfq_en = true;

	 
	four_port = p_hwfn->cdev->num_ports_in_engine == MAX_NUM_PORTS_K2;

	 
	qm_info->max_phys_tcs_per_port = four_port ? NUM_PHYS_TCS_4PORT_K2 :
						     NUM_OF_PHYS_TCS;

	 
	if (!qm_info->ooo_tc)
		qm_info->ooo_tc = four_port ? DCBX_TCP_OOO_K2_4PORT_TC :
					      DCBX_TCP_OOO_TC;
}

 
static void qed_init_qm_vport_params(struct qed_hwfn *p_hwfn)
{
	struct qed_qm_info *qm_info = &p_hwfn->qm_info;
	u8 i;

	 
	for (i = 0; i < qed_init_qm_get_num_vports(p_hwfn); i++)
		qm_info->qm_vport_params[i].wfq = 1;
}

 
static void qed_init_qm_port_params(struct qed_hwfn *p_hwfn)
{
	 
	u8 i, active_phys_tcs, num_ports = p_hwfn->cdev->num_ports_in_engine;
	struct qed_dev *cdev = p_hwfn->cdev;

	 
	active_phys_tcs = num_ports == MAX_NUM_PORTS_K2 ?
			  ACTIVE_TCS_BMAP_4PORT_K2 :
			  ACTIVE_TCS_BMAP;

	for (i = 0; i < num_ports; i++) {
		struct init_qm_port_params *p_qm_port =
		    &p_hwfn->qm_info.qm_port_params[i];
		u16 pbf_max_cmd_lines;

		p_qm_port->active = 1;
		p_qm_port->active_phys_tcs = active_phys_tcs;
		pbf_max_cmd_lines = (u16)NUM_OF_PBF_CMD_LINES(cdev);
		p_qm_port->num_pbf_cmd_lines = pbf_max_cmd_lines / num_ports;
		p_qm_port->num_btb_blocks = NUM_OF_BTB_BLOCKS(cdev) / num_ports;
	}
}

 
static void qed_init_qm_reset_params(struct qed_hwfn *p_hwfn)
{
	struct qed_qm_info *qm_info = &p_hwfn->qm_info;

	qm_info->num_pqs = 0;
	qm_info->num_vports = 0;
	qm_info->num_pf_rls = 0;
	qm_info->num_vf_pqs = 0;
	qm_info->first_vf_pq = 0;
	qm_info->first_mcos_pq = 0;
	qm_info->first_rl_pq = 0;
}

static void qed_init_qm_advance_vport(struct qed_hwfn *p_hwfn)
{
	struct qed_qm_info *qm_info = &p_hwfn->qm_info;

	qm_info->num_vports++;

	if (qm_info->num_vports > qed_init_qm_get_num_vports(p_hwfn))
		DP_ERR(p_hwfn,
		       "vport overflow! qm_info->num_vports %d, qm_init_get_num_vports() %d\n",
		       qm_info->num_vports, qed_init_qm_get_num_vports(p_hwfn));
}

 

 
#define PQ_INIT_SHARE_VPORT     BIT(0)
#define PQ_INIT_PF_RL           BIT(1)
#define PQ_INIT_VF_RL           BIT(2)

 
#define PQ_INIT_DEFAULT_WRR_GROUP       1
#define PQ_INIT_DEFAULT_TC              0

void qed_hw_info_set_offload_tc(struct qed_hw_info *p_info, u8 tc)
{
	p_info->offload_tc = tc;
	p_info->offload_tc_set = true;
}

static bool qed_is_offload_tc_set(struct qed_hwfn *p_hwfn)
{
	return p_hwfn->hw_info.offload_tc_set;
}

static u32 qed_get_offload_tc(struct qed_hwfn *p_hwfn)
{
	if (qed_is_offload_tc_set(p_hwfn))
		return p_hwfn->hw_info.offload_tc;

	return PQ_INIT_DEFAULT_TC;
}

static void qed_init_qm_pq(struct qed_hwfn *p_hwfn,
			   struct qed_qm_info *qm_info,
			   u8 tc, u32 pq_init_flags)
{
	u16 pq_idx = qm_info->num_pqs, max_pq = qed_init_qm_get_num_pqs(p_hwfn);

	if (pq_idx > max_pq)
		DP_ERR(p_hwfn,
		       "pq overflow! pq %d, max pq %d\n", pq_idx, max_pq);

	 
	qm_info->qm_pq_params[pq_idx].port_id = p_hwfn->port_id;
	qm_info->qm_pq_params[pq_idx].vport_id = qm_info->start_vport +
	    qm_info->num_vports;
	qm_info->qm_pq_params[pq_idx].tc_id = tc;
	qm_info->qm_pq_params[pq_idx].wrr_group = PQ_INIT_DEFAULT_WRR_GROUP;
	qm_info->qm_pq_params[pq_idx].rl_valid =
	    (pq_init_flags & PQ_INIT_PF_RL || pq_init_flags & PQ_INIT_VF_RL);

	 
	qm_info->num_pqs++;
	if (!(pq_init_flags & PQ_INIT_SHARE_VPORT))
		qm_info->num_vports++;

	if (pq_init_flags & PQ_INIT_PF_RL)
		qm_info->num_pf_rls++;

	if (qm_info->num_vports > qed_init_qm_get_num_vports(p_hwfn))
		DP_ERR(p_hwfn,
		       "vport overflow! qm_info->num_vports %d, qm_init_get_num_vports() %d\n",
		       qm_info->num_vports, qed_init_qm_get_num_vports(p_hwfn));

	if (qm_info->num_pf_rls > qed_init_qm_get_num_pf_rls(p_hwfn))
		DP_ERR(p_hwfn,
		       "rl overflow! qm_info->num_pf_rls %d, qm_init_get_num_pf_rls() %d\n",
		       qm_info->num_pf_rls, qed_init_qm_get_num_pf_rls(p_hwfn));
}

 
static u16 *qed_init_qm_get_idx_from_flags(struct qed_hwfn *p_hwfn,
					   unsigned long pq_flags)
{
	struct qed_qm_info *qm_info = &p_hwfn->qm_info;

	 
	if (bitmap_weight(&pq_flags,
			  sizeof(pq_flags) * BITS_PER_BYTE) > 1) {
		DP_ERR(p_hwfn, "requested multiple pq flags 0x%lx\n", pq_flags);
		goto err;
	}

	if (!(qed_get_pq_flags(p_hwfn) & pq_flags)) {
		DP_ERR(p_hwfn, "pq flag 0x%lx is not set\n", pq_flags);
		goto err;
	}

	switch (pq_flags) {
	case PQ_FLAGS_RLS:
		return &qm_info->first_rl_pq;
	case PQ_FLAGS_MCOS:
		return &qm_info->first_mcos_pq;
	case PQ_FLAGS_LB:
		return &qm_info->pure_lb_pq;
	case PQ_FLAGS_OOO:
		return &qm_info->ooo_pq;
	case PQ_FLAGS_ACK:
		return &qm_info->pure_ack_pq;
	case PQ_FLAGS_OFLD:
		return &qm_info->first_ofld_pq;
	case PQ_FLAGS_LLT:
		return &qm_info->first_llt_pq;
	case PQ_FLAGS_VFS:
		return &qm_info->first_vf_pq;
	default:
		goto err;
	}

err:
	return &qm_info->start_pq;
}

 
static void qed_init_qm_set_idx(struct qed_hwfn *p_hwfn,
				u32 pq_flags, u16 pq_val)
{
	u16 *base_pq_idx = qed_init_qm_get_idx_from_flags(p_hwfn, pq_flags);

	*base_pq_idx = p_hwfn->qm_info.start_pq + pq_val;
}

 
u16 qed_get_cm_pq_idx(struct qed_hwfn *p_hwfn, u32 pq_flags)
{
	u16 *base_pq_idx = qed_init_qm_get_idx_from_flags(p_hwfn, pq_flags);

	return *base_pq_idx + CM_TX_PQ_BASE;
}

u16 qed_get_cm_pq_idx_mcos(struct qed_hwfn *p_hwfn, u8 tc)
{
	u8 max_tc = qed_init_qm_get_num_tcs(p_hwfn);

	if (max_tc == 0) {
		DP_ERR(p_hwfn, "pq with flag 0x%lx do not exist\n",
		       PQ_FLAGS_MCOS);
		return p_hwfn->qm_info.start_pq;
	}

	if (tc > max_tc)
		DP_ERR(p_hwfn, "tc %d must be smaller than %d\n", tc, max_tc);

	return qed_get_cm_pq_idx(p_hwfn, PQ_FLAGS_MCOS) + (tc % max_tc);
}

u16 qed_get_cm_pq_idx_vf(struct qed_hwfn *p_hwfn, u16 vf)
{
	u16 max_vf = qed_init_qm_get_num_vfs(p_hwfn);

	if (max_vf == 0) {
		DP_ERR(p_hwfn, "pq with flag 0x%lx do not exist\n",
		       PQ_FLAGS_VFS);
		return p_hwfn->qm_info.start_pq;
	}

	if (vf > max_vf)
		DP_ERR(p_hwfn, "vf %d must be smaller than %d\n", vf, max_vf);

	return qed_get_cm_pq_idx(p_hwfn, PQ_FLAGS_VFS) + (vf % max_vf);
}

u16 qed_get_cm_pq_idx_ofld_mtc(struct qed_hwfn *p_hwfn, u8 tc)
{
	u16 first_ofld_pq, pq_offset;

	first_ofld_pq = qed_get_cm_pq_idx(p_hwfn, PQ_FLAGS_OFLD);
	pq_offset = (tc < qed_init_qm_get_num_mtc_tcs(p_hwfn)) ?
		    tc : PQ_INIT_DEFAULT_TC;

	return first_ofld_pq + pq_offset;
}

u16 qed_get_cm_pq_idx_llt_mtc(struct qed_hwfn *p_hwfn, u8 tc)
{
	u16 first_llt_pq, pq_offset;

	first_llt_pq = qed_get_cm_pq_idx(p_hwfn, PQ_FLAGS_LLT);
	pq_offset = (tc < qed_init_qm_get_num_mtc_tcs(p_hwfn)) ?
		    tc : PQ_INIT_DEFAULT_TC;

	return first_llt_pq + pq_offset;
}

 
static void qed_init_qm_lb_pq(struct qed_hwfn *p_hwfn)
{
	struct qed_qm_info *qm_info = &p_hwfn->qm_info;

	if (!(qed_get_pq_flags(p_hwfn) & PQ_FLAGS_LB))
		return;

	qed_init_qm_set_idx(p_hwfn, PQ_FLAGS_LB, qm_info->num_pqs);
	qed_init_qm_pq(p_hwfn, qm_info, PURE_LB_TC, PQ_INIT_SHARE_VPORT);
}

static void qed_init_qm_ooo_pq(struct qed_hwfn *p_hwfn)
{
	struct qed_qm_info *qm_info = &p_hwfn->qm_info;

	if (!(qed_get_pq_flags(p_hwfn) & PQ_FLAGS_OOO))
		return;

	qed_init_qm_set_idx(p_hwfn, PQ_FLAGS_OOO, qm_info->num_pqs);
	qed_init_qm_pq(p_hwfn, qm_info, qm_info->ooo_tc, PQ_INIT_SHARE_VPORT);
}

static void qed_init_qm_pure_ack_pq(struct qed_hwfn *p_hwfn)
{
	struct qed_qm_info *qm_info = &p_hwfn->qm_info;

	if (!(qed_get_pq_flags(p_hwfn) & PQ_FLAGS_ACK))
		return;

	qed_init_qm_set_idx(p_hwfn, PQ_FLAGS_ACK, qm_info->num_pqs);
	qed_init_qm_pq(p_hwfn, qm_info, qed_get_offload_tc(p_hwfn),
		       PQ_INIT_SHARE_VPORT);
}

static void qed_init_qm_mtc_pqs(struct qed_hwfn *p_hwfn)
{
	u8 num_tcs = qed_init_qm_get_num_mtc_tcs(p_hwfn);
	struct qed_qm_info *qm_info = &p_hwfn->qm_info;
	u8 tc;

	 
	for (tc = 0; tc < num_tcs; tc++)
		qed_init_qm_pq(p_hwfn, qm_info,
			       qed_is_offload_tc_set(p_hwfn) ?
			       p_hwfn->hw_info.offload_tc : tc,
			       PQ_INIT_SHARE_VPORT);
}

static void qed_init_qm_offload_pq(struct qed_hwfn *p_hwfn)
{
	struct qed_qm_info *qm_info = &p_hwfn->qm_info;

	if (!(qed_get_pq_flags(p_hwfn) & PQ_FLAGS_OFLD))
		return;

	qed_init_qm_set_idx(p_hwfn, PQ_FLAGS_OFLD, qm_info->num_pqs);
	qed_init_qm_mtc_pqs(p_hwfn);
}

static void qed_init_qm_low_latency_pq(struct qed_hwfn *p_hwfn)
{
	struct qed_qm_info *qm_info = &p_hwfn->qm_info;

	if (!(qed_get_pq_flags(p_hwfn) & PQ_FLAGS_LLT))
		return;

	qed_init_qm_set_idx(p_hwfn, PQ_FLAGS_LLT, qm_info->num_pqs);
	qed_init_qm_mtc_pqs(p_hwfn);
}

static void qed_init_qm_mcos_pqs(struct qed_hwfn *p_hwfn)
{
	struct qed_qm_info *qm_info = &p_hwfn->qm_info;
	u8 tc_idx;

	if (!(qed_get_pq_flags(p_hwfn) & PQ_FLAGS_MCOS))
		return;

	qed_init_qm_set_idx(p_hwfn, PQ_FLAGS_MCOS, qm_info->num_pqs);
	for (tc_idx = 0; tc_idx < qed_init_qm_get_num_tcs(p_hwfn); tc_idx++)
		qed_init_qm_pq(p_hwfn, qm_info, tc_idx, PQ_INIT_SHARE_VPORT);
}

static void qed_init_qm_vf_pqs(struct qed_hwfn *p_hwfn)
{
	struct qed_qm_info *qm_info = &p_hwfn->qm_info;
	u16 vf_idx, num_vfs = qed_init_qm_get_num_vfs(p_hwfn);

	if (!(qed_get_pq_flags(p_hwfn) & PQ_FLAGS_VFS))
		return;

	qed_init_qm_set_idx(p_hwfn, PQ_FLAGS_VFS, qm_info->num_pqs);
	qm_info->num_vf_pqs = num_vfs;
	for (vf_idx = 0; vf_idx < num_vfs; vf_idx++)
		qed_init_qm_pq(p_hwfn,
			       qm_info, PQ_INIT_DEFAULT_TC, PQ_INIT_VF_RL);
}

static void qed_init_qm_rl_pqs(struct qed_hwfn *p_hwfn)
{
	u16 pf_rls_idx, num_pf_rls = qed_init_qm_get_num_pf_rls(p_hwfn);
	struct qed_qm_info *qm_info = &p_hwfn->qm_info;

	if (!(qed_get_pq_flags(p_hwfn) & PQ_FLAGS_RLS))
		return;

	qed_init_qm_set_idx(p_hwfn, PQ_FLAGS_RLS, qm_info->num_pqs);
	for (pf_rls_idx = 0; pf_rls_idx < num_pf_rls; pf_rls_idx++)
		qed_init_qm_pq(p_hwfn, qm_info, qed_get_offload_tc(p_hwfn),
			       PQ_INIT_PF_RL);
}

static void qed_init_qm_pq_params(struct qed_hwfn *p_hwfn)
{
	 
	qed_init_qm_rl_pqs(p_hwfn);

	 
	qed_init_qm_mcos_pqs(p_hwfn);

	 
	qed_init_qm_lb_pq(p_hwfn);

	 
	qed_init_qm_ooo_pq(p_hwfn);

	 
	qed_init_qm_pure_ack_pq(p_hwfn);

	 
	qed_init_qm_offload_pq(p_hwfn);

	 
	qed_init_qm_low_latency_pq(p_hwfn);

	 
	qed_init_qm_advance_vport(p_hwfn);

	 
	qed_init_qm_vf_pqs(p_hwfn);
}

 
static int qed_init_qm_sanity(struct qed_hwfn *p_hwfn)
{
	if (qed_init_qm_get_num_vports(p_hwfn) > RESC_NUM(p_hwfn, QED_VPORT)) {
		DP_ERR(p_hwfn, "requested amount of vports exceeds resource\n");
		return -EINVAL;
	}

	if (qed_init_qm_get_num_pqs(p_hwfn) <= RESC_NUM(p_hwfn, QED_PQ))
		return 0;

	if (QED_IS_ROCE_PERSONALITY(p_hwfn)) {
		p_hwfn->hw_info.multi_tc_roce_en = false;
		DP_NOTICE(p_hwfn,
			  "multi-tc roce was disabled to reduce requested amount of pqs\n");
		if (qed_init_qm_get_num_pqs(p_hwfn) <= RESC_NUM(p_hwfn, QED_PQ))
			return 0;
	}

	DP_ERR(p_hwfn, "requested amount of pqs exceeds resource\n");
	return -EINVAL;
}

static void qed_dp_init_qm_params(struct qed_hwfn *p_hwfn)
{
	struct qed_qm_info *qm_info = &p_hwfn->qm_info;
	struct init_qm_vport_params *vport;
	struct init_qm_port_params *port;
	struct init_qm_pq_params *pq;
	int i, tc;

	 
	DP_VERBOSE(p_hwfn,
		   NETIF_MSG_HW,
		   "qm init top level params: start_pq %d, start_vport %d, pure_lb_pq %d, offload_pq %d, llt_pq %d, pure_ack_pq %d\n",
		   qm_info->start_pq,
		   qm_info->start_vport,
		   qm_info->pure_lb_pq,
		   qm_info->first_ofld_pq,
		   qm_info->first_llt_pq,
		   qm_info->pure_ack_pq);
	DP_VERBOSE(p_hwfn,
		   NETIF_MSG_HW,
		   "ooo_pq %d, first_vf_pq %d, num_pqs %d, num_vf_pqs %d, num_vports %d, max_phys_tcs_per_port %d\n",
		   qm_info->ooo_pq,
		   qm_info->first_vf_pq,
		   qm_info->num_pqs,
		   qm_info->num_vf_pqs,
		   qm_info->num_vports, qm_info->max_phys_tcs_per_port);
	DP_VERBOSE(p_hwfn,
		   NETIF_MSG_HW,
		   "pf_rl_en %d, pf_wfq_en %d, vport_rl_en %d, vport_wfq_en %d, pf_wfq %d, pf_rl %d, num_pf_rls %d, pq_flags %x\n",
		   qm_info->pf_rl_en,
		   qm_info->pf_wfq_en,
		   qm_info->vport_rl_en,
		   qm_info->vport_wfq_en,
		   qm_info->pf_wfq,
		   qm_info->pf_rl,
		   qm_info->num_pf_rls, qed_get_pq_flags(p_hwfn));

	 
	for (i = 0; i < p_hwfn->cdev->num_ports_in_engine; i++) {
		port = &(qm_info->qm_port_params[i]);
		DP_VERBOSE(p_hwfn,
			   NETIF_MSG_HW,
			   "port idx %d, active %d, active_phys_tcs %d, num_pbf_cmd_lines %d, num_btb_blocks %d, reserved %d\n",
			   i,
			   port->active,
			   port->active_phys_tcs,
			   port->num_pbf_cmd_lines,
			   port->num_btb_blocks, port->reserved);
	}

	 
	for (i = 0; i < qm_info->num_vports; i++) {
		vport = &(qm_info->qm_vport_params[i]);
		DP_VERBOSE(p_hwfn,
			   NETIF_MSG_HW,
			   "vport idx %d, wfq %d, first_tx_pq_id [ ",
			   qm_info->start_vport + i, vport->wfq);
		for (tc = 0; tc < NUM_OF_TCS; tc++)
			DP_VERBOSE(p_hwfn,
				   NETIF_MSG_HW,
				   "%d ", vport->first_tx_pq_id[tc]);
		DP_VERBOSE(p_hwfn, NETIF_MSG_HW, "]\n");
	}

	 
	for (i = 0; i < qm_info->num_pqs; i++) {
		pq = &(qm_info->qm_pq_params[i]);
		DP_VERBOSE(p_hwfn,
			   NETIF_MSG_HW,
			   "pq idx %d, port %d, vport_id %d, tc %d, wrr_grp %d, rl_valid %d rl_id %d\n",
			   qm_info->start_pq + i,
			   pq->port_id,
			   pq->vport_id,
			   pq->tc_id, pq->wrr_group, pq->rl_valid, pq->rl_id);
	}
}

static void qed_init_qm_info(struct qed_hwfn *p_hwfn)
{
	 
	qed_init_qm_reset_params(p_hwfn);

	 
	qed_init_qm_params(p_hwfn);

	 
	qed_init_qm_port_params(p_hwfn);

	 
	qed_init_qm_vport_params(p_hwfn);

	 
	qed_init_qm_pq_params(p_hwfn);

	 
	qed_dp_init_qm_params(p_hwfn);
}

 
int qed_qm_reconf(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt)
{
	struct qed_qm_info *qm_info = &p_hwfn->qm_info;
	bool b_rc;
	int rc;

	 
	qed_init_qm_info(p_hwfn);

	 
	spin_lock_bh(&qm_lock);
	b_rc = qed_send_qm_stop_cmd(p_hwfn, p_ptt, false, true,
				    qm_info->start_pq, qm_info->num_pqs);
	spin_unlock_bh(&qm_lock);
	if (!b_rc)
		return -EINVAL;

	 
	qed_qm_init_pf(p_hwfn, p_ptt, false);

	 
	rc = qed_init_run(p_hwfn, p_ptt, PHASE_QM_PF, p_hwfn->rel_pf_id,
			  p_hwfn->hw_info.hw_mode);
	if (rc)
		return rc;

	 
	spin_lock_bh(&qm_lock);
	b_rc = qed_send_qm_stop_cmd(p_hwfn, p_ptt, true, true,
				    qm_info->start_pq, qm_info->num_pqs);
	spin_unlock_bh(&qm_lock);
	if (!b_rc)
		return -EINVAL;

	return 0;
}

static int qed_alloc_qm_data(struct qed_hwfn *p_hwfn)
{
	struct qed_qm_info *qm_info = &p_hwfn->qm_info;
	int rc;

	rc = qed_init_qm_sanity(p_hwfn);
	if (rc)
		goto alloc_err;

	qm_info->qm_pq_params = kcalloc(qed_init_qm_get_num_pqs(p_hwfn),
					sizeof(*qm_info->qm_pq_params),
					GFP_KERNEL);
	if (!qm_info->qm_pq_params)
		goto alloc_err;

	qm_info->qm_vport_params = kcalloc(qed_init_qm_get_num_vports(p_hwfn),
					   sizeof(*qm_info->qm_vport_params),
					   GFP_KERNEL);
	if (!qm_info->qm_vport_params)
		goto alloc_err;

	qm_info->qm_port_params = kcalloc(p_hwfn->cdev->num_ports_in_engine,
					  sizeof(*qm_info->qm_port_params),
					  GFP_KERNEL);
	if (!qm_info->qm_port_params)
		goto alloc_err;

	qm_info->wfq_data = kcalloc(qed_init_qm_get_num_vports(p_hwfn),
				    sizeof(*qm_info->wfq_data),
				    GFP_KERNEL);
	if (!qm_info->wfq_data)
		goto alloc_err;

	return 0;

alloc_err:
	DP_NOTICE(p_hwfn, "Failed to allocate memory for QM params\n");
	qed_qm_info_free(p_hwfn);
	return -ENOMEM;
}

int qed_resc_alloc(struct qed_dev *cdev)
{
	u32 rdma_tasks, excess_tasks;
	u32 line_count;
	int i, rc = 0;

	if (IS_VF(cdev)) {
		for_each_hwfn(cdev, i) {
			rc = qed_l2_alloc(&cdev->hwfns[i]);
			if (rc)
				return rc;
		}
		return rc;
	}

	cdev->fw_data = kzalloc(sizeof(*cdev->fw_data), GFP_KERNEL);
	if (!cdev->fw_data)
		return -ENOMEM;

	for_each_hwfn(cdev, i) {
		struct qed_hwfn *p_hwfn = &cdev->hwfns[i];
		u32 n_eqes, num_cons;

		 
		rc = qed_db_recovery_setup(p_hwfn);
		if (rc)
			goto alloc_err;

		 
		rc = qed_cxt_mngr_alloc(p_hwfn);
		if (rc)
			goto alloc_err;

		 
		rc = qed_cxt_set_pf_params(p_hwfn, RDMA_MAX_TIDS);
		if (rc)
			goto alloc_err;

		rc = qed_alloc_qm_data(p_hwfn);
		if (rc)
			goto alloc_err;

		 
		qed_init_qm_info(p_hwfn);

		 
		rc = qed_cxt_cfg_ilt_compute(p_hwfn, &line_count);
		if (rc) {
			DP_NOTICE(p_hwfn,
				  "too many ILT lines; re-computing with less lines\n");
			 
			excess_tasks =
			    qed_cxt_cfg_ilt_compute_excess(p_hwfn, line_count);
			if (!excess_tasks)
				goto alloc_err;

			rdma_tasks = RDMA_MAX_TIDS - excess_tasks;
			rc = qed_cxt_set_pf_params(p_hwfn, rdma_tasks);
			if (rc)
				goto alloc_err;

			rc = qed_cxt_cfg_ilt_compute(p_hwfn, &line_count);
			if (rc) {
				DP_ERR(p_hwfn,
				       "failed ILT compute. Requested too many lines: %u\n",
				       line_count);

				goto alloc_err;
			}
		}

		 
		rc = qed_cxt_tables_alloc(p_hwfn);
		if (rc)
			goto alloc_err;

		 
		rc = qed_spq_alloc(p_hwfn);
		if (rc)
			goto alloc_err;

		 
		p_hwfn->p_dpc_ptt = qed_get_reserved_ptt(p_hwfn,
							 RESERVED_PTT_DPC);

		rc = qed_int_alloc(p_hwfn, p_hwfn->p_main_ptt);
		if (rc)
			goto alloc_err;

		rc = qed_iov_alloc(p_hwfn);
		if (rc)
			goto alloc_err;

		 
		n_eqes = qed_chain_get_capacity(&p_hwfn->p_spq->chain);
		if (QED_IS_RDMA_PERSONALITY(p_hwfn)) {
			u32 n_srq = qed_cxt_get_total_srq_count(p_hwfn);
			enum protocol_type rdma_proto;

			if (QED_IS_ROCE_PERSONALITY(p_hwfn))
				rdma_proto = PROTOCOLID_ROCE;
			else
				rdma_proto = PROTOCOLID_IWARP;

			num_cons = qed_cxt_get_proto_cid_count(p_hwfn,
							       rdma_proto,
							       NULL) * 2;
			 
			n_eqes += num_cons + 2 * MAX_NUM_VFS_BB + n_srq;
		} else if (p_hwfn->hw_info.personality == QED_PCI_ISCSI ||
			   p_hwfn->hw_info.personality == QED_PCI_NVMETCP) {
			num_cons =
			    qed_cxt_get_proto_cid_count(p_hwfn,
							PROTOCOLID_TCP_ULP,
							NULL);
			n_eqes += 2 * num_cons;
		}

		if (n_eqes > 0xFFFF) {
			DP_ERR(p_hwfn,
			       "Cannot allocate 0x%x EQ elements. The maximum of a u16 chain is 0x%x\n",
			       n_eqes, 0xFFFF);
			goto alloc_no_mem;
		}

		rc = qed_eq_alloc(p_hwfn, (u16)n_eqes);
		if (rc)
			goto alloc_err;

		rc = qed_consq_alloc(p_hwfn);
		if (rc)
			goto alloc_err;

		rc = qed_l2_alloc(p_hwfn);
		if (rc)
			goto alloc_err;

#ifdef CONFIG_QED_LL2
		if (p_hwfn->using_ll2) {
			rc = qed_ll2_alloc(p_hwfn);
			if (rc)
				goto alloc_err;
		}
#endif

		if (p_hwfn->hw_info.personality == QED_PCI_FCOE) {
			rc = qed_fcoe_alloc(p_hwfn);
			if (rc)
				goto alloc_err;
		}

		if (p_hwfn->hw_info.personality == QED_PCI_ISCSI) {
			rc = qed_iscsi_alloc(p_hwfn);
			if (rc)
				goto alloc_err;
			rc = qed_ooo_alloc(p_hwfn);
			if (rc)
				goto alloc_err;
		}

		if (p_hwfn->hw_info.personality == QED_PCI_NVMETCP) {
			rc = qed_nvmetcp_alloc(p_hwfn);
			if (rc)
				goto alloc_err;
			rc = qed_ooo_alloc(p_hwfn);
			if (rc)
				goto alloc_err;
		}

		if (QED_IS_RDMA_PERSONALITY(p_hwfn)) {
			rc = qed_rdma_info_alloc(p_hwfn);
			if (rc)
				goto alloc_err;
		}

		 
		rc = qed_dmae_info_alloc(p_hwfn);
		if (rc)
			goto alloc_err;

		 
		rc = qed_dcbx_info_alloc(p_hwfn);
		if (rc)
			goto alloc_err;

		rc = qed_dbg_alloc_user_data(p_hwfn, &p_hwfn->dbg_user_info);
		if (rc)
			goto alloc_err;
	}

	rc = qed_llh_alloc(cdev);
	if (rc) {
		DP_NOTICE(cdev,
			  "Failed to allocate memory for the llh_info structure\n");
		goto alloc_err;
	}

	cdev->reset_stats = kzalloc(sizeof(*cdev->reset_stats), GFP_KERNEL);
	if (!cdev->reset_stats)
		goto alloc_no_mem;

	return 0;

alloc_no_mem:
	rc = -ENOMEM;
alloc_err:
	qed_resc_free(cdev);
	return rc;
}

static int qed_fw_err_handler(struct qed_hwfn *p_hwfn,
			      u8 opcode,
			      u16 echo,
			      union event_ring_data *data, u8 fw_return_code)
{
	if (fw_return_code != COMMON_ERR_CODE_ERROR)
		goto eqe_unexpected;

	if (data->err_data.recovery_scope == ERR_SCOPE_FUNC &&
	    le16_to_cpu(data->err_data.entity_id) >= MAX_NUM_PFS) {
		qed_sriov_vfpf_malicious(p_hwfn, &data->err_data);
		return 0;
	}

eqe_unexpected:
	DP_ERR(p_hwfn,
	       "Skipping unexpected eqe 0x%02x, FW return code 0x%x, echo 0x%x\n",
	       opcode, fw_return_code, echo);
	return -EINVAL;
}

static int qed_common_eqe_event(struct qed_hwfn *p_hwfn,
				u8 opcode,
				__le16 echo,
				union event_ring_data *data,
				u8 fw_return_code)
{
	switch (opcode) {
	case COMMON_EVENT_VF_PF_CHANNEL:
	case COMMON_EVENT_VF_FLR:
		return qed_sriov_eqe_event(p_hwfn, opcode, echo, data,
					   fw_return_code);
	case COMMON_EVENT_FW_ERROR:
		return qed_fw_err_handler(p_hwfn, opcode,
					  le16_to_cpu(echo), data,
					  fw_return_code);
	default:
		DP_INFO(p_hwfn->cdev, "Unknown eqe event 0x%02x, echo 0x%x\n",
			opcode, echo);
		return -EINVAL;
	}
}

void qed_resc_setup(struct qed_dev *cdev)
{
	int i;

	if (IS_VF(cdev)) {
		for_each_hwfn(cdev, i)
			qed_l2_setup(&cdev->hwfns[i]);
		return;
	}

	for_each_hwfn(cdev, i) {
		struct qed_hwfn *p_hwfn = &cdev->hwfns[i];

		qed_cxt_mngr_setup(p_hwfn);
		qed_spq_setup(p_hwfn);
		qed_eq_setup(p_hwfn);
		qed_consq_setup(p_hwfn);

		 
		qed_mcp_read_mb(p_hwfn, p_hwfn->p_main_ptt);
		memcpy(p_hwfn->mcp_info->mfw_mb_shadow,
		       p_hwfn->mcp_info->mfw_mb_cur,
		       p_hwfn->mcp_info->mfw_mb_length);

		qed_int_setup(p_hwfn, p_hwfn->p_main_ptt);

		qed_l2_setup(p_hwfn);
		qed_iov_setup(p_hwfn);
		qed_spq_register_async_cb(p_hwfn, PROTOCOLID_COMMON,
					  qed_common_eqe_event);
#ifdef CONFIG_QED_LL2
		if (p_hwfn->using_ll2)
			qed_ll2_setup(p_hwfn);
#endif
		if (p_hwfn->hw_info.personality == QED_PCI_FCOE)
			qed_fcoe_setup(p_hwfn);

		if (p_hwfn->hw_info.personality == QED_PCI_ISCSI) {
			qed_iscsi_setup(p_hwfn);
			qed_ooo_setup(p_hwfn);
		}

		if (p_hwfn->hw_info.personality == QED_PCI_NVMETCP) {
			qed_nvmetcp_setup(p_hwfn);
			qed_ooo_setup(p_hwfn);
		}
	}
}

#define FINAL_CLEANUP_POLL_CNT          (100)
#define FINAL_CLEANUP_POLL_TIME         (10)
int qed_final_cleanup(struct qed_hwfn *p_hwfn,
		      struct qed_ptt *p_ptt, u16 id, bool is_vf)
{
	u32 command = 0, addr, count = FINAL_CLEANUP_POLL_CNT;
	int rc = -EBUSY;

	addr = GET_GTT_REG_ADDR(GTT_BAR0_MAP_REG_USDM_RAM,
				USTORM_FLR_FINAL_ACK, p_hwfn->rel_pf_id);
	if (is_vf)
		id += 0x10;

	command |= X_FINAL_CLEANUP_AGG_INT <<
		SDM_AGG_INT_COMP_PARAMS_AGG_INT_INDEX_SHIFT;
	command |= 1 << SDM_AGG_INT_COMP_PARAMS_AGG_VECTOR_ENABLE_SHIFT;
	command |= id << SDM_AGG_INT_COMP_PARAMS_AGG_VECTOR_BIT_SHIFT;
	command |= SDM_COMP_TYPE_AGG_INT << SDM_OP_GEN_COMP_TYPE_SHIFT;

	 
	if (REG_RD(p_hwfn, addr)) {
		DP_NOTICE(p_hwfn,
			  "Unexpected; Found final cleanup notification before initiating final cleanup\n");
		REG_WR(p_hwfn, addr, 0);
	}

	DP_VERBOSE(p_hwfn, QED_MSG_IOV,
		   "Sending final cleanup for PFVF[%d] [Command %08x]\n",
		   id, command);

	qed_wr(p_hwfn, p_ptt, XSDM_REG_OPERATION_GEN, command);

	 
	while (!REG_RD(p_hwfn, addr) && count--)
		msleep(FINAL_CLEANUP_POLL_TIME);

	if (REG_RD(p_hwfn, addr))
		rc = 0;
	else
		DP_NOTICE(p_hwfn,
			  "Failed to receive FW final cleanup notification\n");

	 
	REG_WR(p_hwfn, addr, 0);

	return rc;
}

static int qed_calc_hw_mode(struct qed_hwfn *p_hwfn)
{
	int hw_mode = 0;

	if (QED_IS_BB_B0(p_hwfn->cdev)) {
		hw_mode |= 1 << MODE_BB;
	} else if (QED_IS_AH(p_hwfn->cdev)) {
		hw_mode |= 1 << MODE_K2;
	} else {
		DP_NOTICE(p_hwfn, "Unknown chip type %#x\n",
			  p_hwfn->cdev->type);
		return -EINVAL;
	}

	switch (p_hwfn->cdev->num_ports_in_engine) {
	case 1:
		hw_mode |= 1 << MODE_PORTS_PER_ENG_1;
		break;
	case 2:
		hw_mode |= 1 << MODE_PORTS_PER_ENG_2;
		break;
	case 4:
		hw_mode |= 1 << MODE_PORTS_PER_ENG_4;
		break;
	default:
		DP_NOTICE(p_hwfn, "num_ports_in_engine = %d not supported\n",
			  p_hwfn->cdev->num_ports_in_engine);
		return -EINVAL;
	}

	if (test_bit(QED_MF_OVLAN_CLSS, &p_hwfn->cdev->mf_bits))
		hw_mode |= 1 << MODE_MF_SD;
	else
		hw_mode |= 1 << MODE_MF_SI;

	hw_mode |= 1 << MODE_ASIC;

	if (p_hwfn->cdev->num_hwfns > 1)
		hw_mode |= 1 << MODE_100G;

	p_hwfn->hw_info.hw_mode = hw_mode;

	DP_VERBOSE(p_hwfn, (NETIF_MSG_PROBE | NETIF_MSG_IFUP),
		   "Configuring function for hw_mode: 0x%08x\n",
		   p_hwfn->hw_info.hw_mode);

	return 0;
}

 
static void qed_init_cau_rt_data(struct qed_dev *cdev)
{
	u32 offset = CAU_REG_SB_VAR_MEMORY_RT_OFFSET;
	int i, igu_sb_id;

	for_each_hwfn(cdev, i) {
		struct qed_hwfn *p_hwfn = &cdev->hwfns[i];
		struct qed_igu_info *p_igu_info;
		struct qed_igu_block *p_block;
		struct cau_sb_entry sb_entry;

		p_igu_info = p_hwfn->hw_info.p_igu_info;

		for (igu_sb_id = 0;
		     igu_sb_id < QED_MAPPING_MEMORY_SIZE(cdev); igu_sb_id++) {
			p_block = &p_igu_info->entry[igu_sb_id];

			if (!p_block->is_pf)
				continue;

			qed_init_cau_sb_entry(p_hwfn, &sb_entry,
					      p_block->function_id, 0, 0);
			STORE_RT_REG_AGG(p_hwfn, offset + igu_sb_id * 2,
					 sb_entry);
		}
	}
}

static void qed_init_cache_line_size(struct qed_hwfn *p_hwfn,
				     struct qed_ptt *p_ptt)
{
	u32 val, wr_mbs, cache_line_size;

	val = qed_rd(p_hwfn, p_ptt, PSWRQ2_REG_WR_MBS0);
	switch (val) {
	case 0:
		wr_mbs = 128;
		break;
	case 1:
		wr_mbs = 256;
		break;
	case 2:
		wr_mbs = 512;
		break;
	default:
		DP_INFO(p_hwfn,
			"Unexpected value of PSWRQ2_REG_WR_MBS0 [0x%x]. Avoid configuring PGLUE_B_REG_CACHE_LINE_SIZE.\n",
			val);
		return;
	}

	cache_line_size = min_t(u32, L1_CACHE_BYTES, wr_mbs);
	switch (cache_line_size) {
	case 32:
		val = 0;
		break;
	case 64:
		val = 1;
		break;
	case 128:
		val = 2;
		break;
	case 256:
		val = 3;
		break;
	default:
		DP_INFO(p_hwfn,
			"Unexpected value of cache line size [0x%x]. Avoid configuring PGLUE_B_REG_CACHE_LINE_SIZE.\n",
			cache_line_size);
	}

	if (wr_mbs < L1_CACHE_BYTES)
		DP_INFO(p_hwfn,
			"The cache line size for padding is suboptimal for performance [OS cache line size 0x%x, wr mbs 0x%x]\n",
			L1_CACHE_BYTES, wr_mbs);

	STORE_RT_REG(p_hwfn, PGLUE_REG_B_CACHE_LINE_SIZE_RT_OFFSET, val);
	if (val > 0) {
		STORE_RT_REG(p_hwfn, PSWRQ2_REG_DRAM_ALIGN_WR_RT_OFFSET, val);
		STORE_RT_REG(p_hwfn, PSWRQ2_REG_DRAM_ALIGN_RD_RT_OFFSET, val);
	}
}

static int qed_hw_init_common(struct qed_hwfn *p_hwfn,
			      struct qed_ptt *p_ptt, int hw_mode)
{
	struct qed_qm_info *qm_info = &p_hwfn->qm_info;
	struct qed_qm_common_rt_init_params *params;
	struct qed_dev *cdev = p_hwfn->cdev;
	u8 vf_id, max_num_vfs;
	u16 num_pfs, pf_id;
	u32 concrete_fid;
	int rc = 0;

	params = kzalloc(sizeof(*params), GFP_KERNEL);
	if (!params) {
		DP_NOTICE(p_hwfn->cdev,
			  "Failed to allocate common init params\n");

		return -ENOMEM;
	}

	qed_init_cau_rt_data(cdev);

	 
	qed_gtt_init(p_hwfn);

	if (p_hwfn->mcp_info) {
		if (p_hwfn->mcp_info->func_info.bandwidth_max)
			qm_info->pf_rl_en = true;
		if (p_hwfn->mcp_info->func_info.bandwidth_min)
			qm_info->pf_wfq_en = true;
	}

	params->max_ports_per_engine = p_hwfn->cdev->num_ports_in_engine;
	params->max_phys_tcs_per_port = qm_info->max_phys_tcs_per_port;
	params->pf_rl_en = qm_info->pf_rl_en;
	params->pf_wfq_en = qm_info->pf_wfq_en;
	params->global_rl_en = qm_info->vport_rl_en;
	params->vport_wfq_en = qm_info->vport_wfq_en;
	params->port_params = qm_info->qm_port_params;

	qed_qm_common_rt_init(p_hwfn, params);

	qed_cxt_hw_init_common(p_hwfn);

	qed_init_cache_line_size(p_hwfn, p_ptt);

	rc = qed_init_run(p_hwfn, p_ptt, PHASE_ENGINE, ANY_PHASE_ID, hw_mode);
	if (rc)
		goto out;

	qed_wr(p_hwfn, p_ptt, PSWRQ2_REG_L2P_VALIDATE_VFID, 0);
	qed_wr(p_hwfn, p_ptt, PGLUE_B_REG_USE_CLIENTID_IN_TAG, 1);

	if (QED_IS_BB(p_hwfn->cdev)) {
		num_pfs = NUM_OF_ENG_PFS(p_hwfn->cdev);
		for (pf_id = 0; pf_id < num_pfs; pf_id++) {
			qed_fid_pretend(p_hwfn, p_ptt, pf_id);
			qed_wr(p_hwfn, p_ptt, PRS_REG_SEARCH_ROCE, 0x0);
			qed_wr(p_hwfn, p_ptt, PRS_REG_SEARCH_TCP, 0x0);
		}
		 
		qed_fid_pretend(p_hwfn, p_ptt, p_hwfn->rel_pf_id);
	}

	max_num_vfs = QED_IS_AH(cdev) ? MAX_NUM_VFS_K2 : MAX_NUM_VFS_BB;
	for (vf_id = 0; vf_id < max_num_vfs; vf_id++) {
		concrete_fid = qed_vfid_to_concrete(p_hwfn, vf_id);
		qed_fid_pretend(p_hwfn, p_ptt, (u16)concrete_fid);
		qed_wr(p_hwfn, p_ptt, CCFC_REG_STRONG_ENABLE_VF, 0x1);
		qed_wr(p_hwfn, p_ptt, CCFC_REG_WEAK_ENABLE_VF, 0x0);
		qed_wr(p_hwfn, p_ptt, TCFC_REG_STRONG_ENABLE_VF, 0x1);
		qed_wr(p_hwfn, p_ptt, TCFC_REG_WEAK_ENABLE_VF, 0x0);
	}
	 
	qed_fid_pretend(p_hwfn, p_ptt, p_hwfn->rel_pf_id);

out:
	kfree(params);

	return rc;
}

static int
qed_hw_init_dpi_size(struct qed_hwfn *p_hwfn,
		     struct qed_ptt *p_ptt, u32 pwm_region_size, u32 n_cpus)
{
	u32 dpi_bit_shift, dpi_count, dpi_page_size;
	u32 min_dpis;
	u32 n_wids;

	 
	n_wids = max_t(u32, QED_MIN_WIDS, n_cpus);
	dpi_page_size = QED_WID_SIZE * roundup_pow_of_two(n_wids);
	dpi_page_size = (dpi_page_size + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
	dpi_bit_shift = ilog2(dpi_page_size / 4096);
	dpi_count = pwm_region_size / dpi_page_size;

	min_dpis = p_hwfn->pf_params.rdma_pf_params.min_dpis;
	min_dpis = max_t(u32, QED_MIN_DPIS, min_dpis);

	p_hwfn->dpi_size = dpi_page_size;
	p_hwfn->dpi_count = dpi_count;

	qed_wr(p_hwfn, p_ptt, DORQ_REG_PF_DPI_BIT_SHIFT, dpi_bit_shift);

	if (dpi_count < min_dpis)
		return -EINVAL;

	return 0;
}

enum QED_ROCE_EDPM_MODE {
	QED_ROCE_EDPM_MODE_ENABLE = 0,
	QED_ROCE_EDPM_MODE_FORCE_ON = 1,
	QED_ROCE_EDPM_MODE_DISABLE = 2,
};

bool qed_edpm_enabled(struct qed_hwfn *p_hwfn)
{
	if (p_hwfn->dcbx_no_edpm || p_hwfn->db_bar_no_edpm)
		return false;

	return true;
}

static int
qed_hw_init_pf_doorbell_bar(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt)
{
	u32 pwm_regsize, norm_regsize;
	u32 non_pwm_conn, min_addr_reg1;
	u32 db_bar_size, n_cpus = 1;
	u32 roce_edpm_mode;
	u32 pf_dems_shift;
	int rc = 0;
	u8 cond;

	db_bar_size = qed_hw_bar_size(p_hwfn, p_ptt, BAR_ID_1);
	if (p_hwfn->cdev->num_hwfns > 1)
		db_bar_size /= 2;

	 
	non_pwm_conn = qed_cxt_get_proto_cid_start(p_hwfn, PROTOCOLID_CORE) +
		       qed_cxt_get_proto_cid_count(p_hwfn, PROTOCOLID_CORE,
						   NULL) +
		       qed_cxt_get_proto_cid_count(p_hwfn, PROTOCOLID_ETH,
						   NULL);
	norm_regsize = roundup(QED_PF_DEMS_SIZE * non_pwm_conn, PAGE_SIZE);
	min_addr_reg1 = norm_regsize / 4096;
	pwm_regsize = db_bar_size - norm_regsize;

	 
	if (db_bar_size < norm_regsize) {
		DP_ERR(p_hwfn->cdev,
		       "Doorbell BAR size 0x%x is too small (normal region is 0x%0x )\n",
		       db_bar_size, norm_regsize);
		return -EINVAL;
	}

	if (pwm_regsize < QED_MIN_PWM_REGION) {
		DP_ERR(p_hwfn->cdev,
		       "PWM region size 0x%0x is too small. Should be at least 0x%0x (Doorbell BAR size is 0x%x and normal region size is 0x%0x)\n",
		       pwm_regsize,
		       QED_MIN_PWM_REGION, db_bar_size, norm_regsize);
		return -EINVAL;
	}

	 
	roce_edpm_mode = p_hwfn->pf_params.rdma_pf_params.roce_edpm_mode;
	if ((roce_edpm_mode == QED_ROCE_EDPM_MODE_ENABLE) ||
	    ((roce_edpm_mode == QED_ROCE_EDPM_MODE_FORCE_ON))) {
		 
		n_cpus = num_present_cpus();
		rc = qed_hw_init_dpi_size(p_hwfn, p_ptt, pwm_regsize, n_cpus);
	}

	cond = (rc && (roce_edpm_mode == QED_ROCE_EDPM_MODE_ENABLE)) ||
	       (roce_edpm_mode == QED_ROCE_EDPM_MODE_DISABLE);
	if (cond || p_hwfn->dcbx_no_edpm) {
		 
		n_cpus = 1;
		rc = qed_hw_init_dpi_size(p_hwfn, p_ptt, pwm_regsize, n_cpus);

		if (cond)
			qed_rdma_dpm_bar(p_hwfn, p_ptt);
	}

	p_hwfn->wid_count = (u16)n_cpus;

	DP_INFO(p_hwfn,
		"doorbell bar: normal_region_size=%d, pwm_region_size=%d, dpi_size=%d, dpi_count=%d, roce_edpm=%s, page_size=%lu\n",
		norm_regsize,
		pwm_regsize,
		p_hwfn->dpi_size,
		p_hwfn->dpi_count,
		(!qed_edpm_enabled(p_hwfn)) ?
		"disabled" : "enabled", PAGE_SIZE);

	if (rc) {
		DP_ERR(p_hwfn,
		       "Failed to allocate enough DPIs. Allocated %d but the current minimum is %d.\n",
		       p_hwfn->dpi_count,
		       p_hwfn->pf_params.rdma_pf_params.min_dpis);
		return -EINVAL;
	}

	p_hwfn->dpi_start_offset = norm_regsize;

	 
	pf_dems_shift = ilog2(QED_PF_DEMS_SIZE / 4);
	qed_wr(p_hwfn, p_ptt, DORQ_REG_PF_ICID_BIT_SHIFT_NORM, pf_dems_shift);
	qed_wr(p_hwfn, p_ptt, DORQ_REG_PF_MIN_ADDR_REG1, min_addr_reg1);

	return 0;
}

static int qed_hw_init_port(struct qed_hwfn *p_hwfn,
			    struct qed_ptt *p_ptt, int hw_mode)
{
	int rc = 0;

	 
	if (!QED_IS_CMT(p_hwfn->cdev) || !IS_LEAD_HWFN(p_hwfn))
		STORE_RT_REG(p_hwfn, NIG_REG_BRB_GATE_DNTFWD_PORT_RT_OFFSET, 0);

	rc = qed_init_run(p_hwfn, p_ptt, PHASE_PORT, p_hwfn->port_id, hw_mode);
	if (rc)
		return rc;

	qed_wr(p_hwfn, p_ptt, PGLUE_B_REG_MASTER_WRITE_PAD_ENABLE, 0);

	return 0;
}

static int qed_hw_init_pf(struct qed_hwfn *p_hwfn,
			  struct qed_ptt *p_ptt,
			  struct qed_tunnel_info *p_tunn,
			  int hw_mode,
			  bool b_hw_start,
			  enum qed_int_mode int_mode,
			  bool allow_npar_tx_switch)
{
	u8 rel_pf_id = p_hwfn->rel_pf_id;
	int rc = 0;

	if (p_hwfn->mcp_info) {
		struct qed_mcp_function_info *p_info;

		p_info = &p_hwfn->mcp_info->func_info;
		if (p_info->bandwidth_min)
			p_hwfn->qm_info.pf_wfq = p_info->bandwidth_min;

		 
		p_hwfn->qm_info.pf_rl = 100000;
	}

	qed_cxt_hw_init_pf(p_hwfn, p_ptt);

	qed_int_igu_init_rt(p_hwfn);

	 
	if (hw_mode & BIT(MODE_MF_SD)) {
		DP_VERBOSE(p_hwfn, NETIF_MSG_HW, "Configuring LLH_FUNC_TAG\n");
		STORE_RT_REG(p_hwfn, NIG_REG_LLH_FUNC_TAG_EN_RT_OFFSET, 1);
		STORE_RT_REG(p_hwfn, NIG_REG_LLH_FUNC_TAG_VALUE_RT_OFFSET,
			     p_hwfn->hw_info.ovlan);

		DP_VERBOSE(p_hwfn, NETIF_MSG_HW,
			   "Configuring LLH_FUNC_FILTER_HDR_SEL\n");
		STORE_RT_REG(p_hwfn, NIG_REG_LLH_FUNC_FILTER_HDR_SEL_RT_OFFSET,
			     1);
	}

	 
	if (hw_mode & BIT(MODE_MF_SI)) {
		DP_VERBOSE(p_hwfn, NETIF_MSG_HW,
			   "Configuring TAGMAC_CLS_TYPE\n");
		STORE_RT_REG(p_hwfn,
			     NIG_REG_LLH_FUNC_TAGMAC_CLS_TYPE_RT_OFFSET, 1);
	}

	 
	STORE_RT_REG(p_hwfn, PRS_REG_SEARCH_TCP_RT_OFFSET,
		     ((p_hwfn->hw_info.personality == QED_PCI_ISCSI) ||
			 (p_hwfn->hw_info.personality == QED_PCI_NVMETCP)) ? 1 : 0);
	STORE_RT_REG(p_hwfn, PRS_REG_SEARCH_FCOE_RT_OFFSET,
		     (p_hwfn->hw_info.personality == QED_PCI_FCOE) ? 1 : 0);
	STORE_RT_REG(p_hwfn, PRS_REG_SEARCH_ROCE_RT_OFFSET, 0);

	 
	rc = qed_dmae_sanity(p_hwfn, p_ptt, "pf_phase");
	if (rc)
		return rc;

	 
	rc = qed_init_run(p_hwfn, p_ptt, PHASE_PF, rel_pf_id, hw_mode);
	if (rc)
		return rc;

	 
	rc = qed_init_run(p_hwfn, p_ptt, PHASE_QM_PF, rel_pf_id, hw_mode);
	if (rc)
		return rc;

	qed_fw_overlay_init_ram(p_hwfn, p_ptt, p_hwfn->fw_overlay_mem);

	 
	qed_int_igu_init_pure_rt(p_hwfn, p_ptt, true, true);

	rc = qed_hw_init_pf_doorbell_bar(p_hwfn, p_ptt);
	if (rc)
		return rc;

	 
	if (IS_LEAD_HWFN(p_hwfn)) {
		rc = qed_llh_hw_init_pf(p_hwfn, p_ptt);
		if (rc)
			return rc;
	}

	if (b_hw_start) {
		 
		qed_int_igu_enable(p_hwfn, p_ptt, int_mode);

		 
		rc = qed_sp_pf_start(p_hwfn, p_ptt, p_tunn,
				     allow_npar_tx_switch);
		if (rc) {
			DP_NOTICE(p_hwfn, "Function start ramrod failed\n");
			return rc;
		}
		if (p_hwfn->hw_info.personality == QED_PCI_FCOE) {
			qed_wr(p_hwfn, p_ptt, PRS_REG_SEARCH_TAG1, BIT(2));
			qed_wr(p_hwfn, p_ptt,
			       PRS_REG_PKT_LEN_STAT_TAGS_NOT_COUNTED_FIRST,
			       0x100);
		}
	}
	return rc;
}

int qed_pglueb_set_pfid_enable(struct qed_hwfn *p_hwfn,
			       struct qed_ptt *p_ptt, bool b_enable)
{
	u32 delay_idx = 0, val, set_val = b_enable ? 1 : 0;

	 
	qed_wr(p_hwfn, p_ptt, PGLUE_B_REG_INTERNAL_PFID_ENABLE_MASTER, set_val);

	 
	for (delay_idx = 0; delay_idx < 20000; delay_idx++) {
		val = qed_rd(p_hwfn, p_ptt,
			     PGLUE_B_REG_INTERNAL_PFID_ENABLE_MASTER);
		if (val == set_val)
			break;

		usleep_range(50, 60);
	}

	if (val != set_val) {
		DP_NOTICE(p_hwfn,
			  "PFID_ENABLE_MASTER wasn't changed after a second\n");
		return -EAGAIN;
	}

	return 0;
}

static void qed_reset_mb_shadow(struct qed_hwfn *p_hwfn,
				struct qed_ptt *p_main_ptt)
{
	 
	qed_mcp_read_mb(p_hwfn, p_main_ptt);
	memcpy(p_hwfn->mcp_info->mfw_mb_shadow,
	       p_hwfn->mcp_info->mfw_mb_cur, p_hwfn->mcp_info->mfw_mb_length);
}

static void
qed_fill_load_req_params(struct qed_load_req_params *p_load_req,
			 struct qed_drv_load_params *p_drv_load)
{
	memset(p_load_req, 0, sizeof(*p_load_req));

	p_load_req->drv_role = p_drv_load->is_crash_kernel ?
			       QED_DRV_ROLE_KDUMP : QED_DRV_ROLE_OS;
	p_load_req->timeout_val = p_drv_load->mfw_timeout_val;
	p_load_req->avoid_eng_reset = p_drv_load->avoid_eng_reset;
	p_load_req->override_force_load = p_drv_load->override_force_load;
}

static int qed_vf_start(struct qed_hwfn *p_hwfn,
			struct qed_hw_init_params *p_params)
{
	if (p_params->p_tunn) {
		qed_vf_set_vf_start_tunn_update_param(p_params->p_tunn);
		qed_vf_pf_tunnel_param_update(p_hwfn, p_params->p_tunn);
	}

	p_hwfn->b_int_enabled = true;

	return 0;
}

static void qed_pglueb_clear_err(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt)
{
	qed_wr(p_hwfn, p_ptt, PGLUE_B_REG_WAS_ERROR_PF_31_0_CLR,
	       BIT(p_hwfn->abs_pf_id));
}

int qed_hw_init(struct qed_dev *cdev, struct qed_hw_init_params *p_params)
{
	struct qed_load_req_params load_req_params;
	u32 load_code, resp, param, drv_mb_param;
	bool b_default_mtu = true;
	struct qed_hwfn *p_hwfn;
	const u32 *fw_overlays;
	u32 fw_overlays_len;
	u16 ether_type;
	int rc = 0, i;

	if ((p_params->int_mode == QED_INT_MODE_MSI) && (cdev->num_hwfns > 1)) {
		DP_NOTICE(cdev, "MSI mode is not supported for CMT devices\n");
		return -EINVAL;
	}

	if (IS_PF(cdev)) {
		rc = qed_init_fw_data(cdev, p_params->bin_fw_data);
		if (rc)
			return rc;
	}

	for_each_hwfn(cdev, i) {
		p_hwfn = &cdev->hwfns[i];

		 
		if (!p_hwfn->hw_info.mtu) {
			p_hwfn->hw_info.mtu = 1500;
			b_default_mtu = false;
		}

		if (IS_VF(cdev)) {
			qed_vf_start(p_hwfn, p_params);
			continue;
		}

		 
		p_hwfn->mcp_info->mcp_handling_status = 0;

		rc = qed_calc_hw_mode(p_hwfn);
		if (rc)
			return rc;

		if (IS_PF(cdev) && (test_bit(QED_MF_8021Q_TAGGING,
					     &cdev->mf_bits) ||
				    test_bit(QED_MF_8021AD_TAGGING,
					     &cdev->mf_bits))) {
			if (test_bit(QED_MF_8021Q_TAGGING, &cdev->mf_bits))
				ether_type = ETH_P_8021Q;
			else
				ether_type = ETH_P_8021AD;
			STORE_RT_REG(p_hwfn, PRS_REG_TAG_ETHERTYPE_0_RT_OFFSET,
				     ether_type);
			STORE_RT_REG(p_hwfn, NIG_REG_TAG_ETHERTYPE_0_RT_OFFSET,
				     ether_type);
			STORE_RT_REG(p_hwfn, PBF_REG_TAG_ETHERTYPE_0_RT_OFFSET,
				     ether_type);
			STORE_RT_REG(p_hwfn, DORQ_REG_TAG1_ETHERTYPE_RT_OFFSET,
				     ether_type);
		}

		qed_fill_load_req_params(&load_req_params,
					 p_params->p_drv_load_params);
		rc = qed_mcp_load_req(p_hwfn, p_hwfn->p_main_ptt,
				      &load_req_params);
		if (rc) {
			DP_NOTICE(p_hwfn, "Failed sending a LOAD_REQ command\n");
			return rc;
		}

		load_code = load_req_params.load_code;
		DP_VERBOSE(p_hwfn, QED_MSG_SP,
			   "Load request was sent. Load code: 0x%x\n",
			   load_code);

		 
		cdev->recov_in_prog = false;

		qed_mcp_set_capabilities(p_hwfn, p_hwfn->p_main_ptt);

		qed_reset_mb_shadow(p_hwfn, p_hwfn->p_main_ptt);

		 
		if (load_code != FW_MSG_CODE_DRV_LOAD_ENGINE) {
			rc = qed_final_cleanup(p_hwfn, p_hwfn->p_main_ptt,
					       p_hwfn->rel_pf_id, false);
			if (rc) {
				qed_hw_err_notify(p_hwfn, p_hwfn->p_main_ptt,
						  QED_HW_ERR_RAMROD_FAIL,
						  "Final cleanup failed\n");
				goto load_err;
			}
		}

		 
		qed_pglueb_rbc_attn_handler(p_hwfn, p_hwfn->p_main_ptt, true);

		 
		rc = qed_pglueb_set_pfid_enable(p_hwfn, p_hwfn->p_main_ptt,
						true);
		if (rc)
			goto load_err;

		 
		qed_pglueb_clear_err(p_hwfn, p_hwfn->p_main_ptt);

		fw_overlays = cdev->fw_data->fw_overlays;
		fw_overlays_len = cdev->fw_data->fw_overlays_len;
		p_hwfn->fw_overlay_mem =
		    qed_fw_overlay_mem_alloc(p_hwfn, fw_overlays,
					     fw_overlays_len);
		if (!p_hwfn->fw_overlay_mem) {
			DP_NOTICE(p_hwfn,
				  "Failed to allocate fw overlay memory\n");
			rc = -ENOMEM;
			goto load_err;
		}

		switch (load_code) {
		case FW_MSG_CODE_DRV_LOAD_ENGINE:
			rc = qed_hw_init_common(p_hwfn, p_hwfn->p_main_ptt,
						p_hwfn->hw_info.hw_mode);
			if (rc)
				break;
			fallthrough;
		case FW_MSG_CODE_DRV_LOAD_PORT:
			rc = qed_hw_init_port(p_hwfn, p_hwfn->p_main_ptt,
					      p_hwfn->hw_info.hw_mode);
			if (rc)
				break;

			fallthrough;
		case FW_MSG_CODE_DRV_LOAD_FUNCTION:
			rc = qed_hw_init_pf(p_hwfn, p_hwfn->p_main_ptt,
					    p_params->p_tunn,
					    p_hwfn->hw_info.hw_mode,
					    p_params->b_hw_start,
					    p_params->int_mode,
					    p_params->allow_npar_tx_switch);
			break;
		default:
			DP_NOTICE(p_hwfn,
				  "Unexpected load code [0x%08x]", load_code);
			rc = -EINVAL;
			break;
		}

		if (rc) {
			DP_NOTICE(p_hwfn,
				  "init phase failed for loadcode 0x%x (rc %d)\n",
				  load_code, rc);
			goto load_err;
		}

		rc = qed_mcp_load_done(p_hwfn, p_hwfn->p_main_ptt);
		if (rc)
			return rc;

		 
		DP_VERBOSE(p_hwfn,
			   QED_MSG_DCB,
			   "sending phony dcbx set command to trigger DCBx attention handling\n");
		rc = qed_mcp_cmd(p_hwfn, p_hwfn->p_main_ptt,
				 DRV_MSG_CODE_SET_DCBX,
				 1 << DRV_MB_PARAM_DCBX_NOTIFY_SHIFT,
				 &resp, &param);
		if (rc) {
			DP_NOTICE(p_hwfn,
				  "Failed to send DCBX attention request\n");
			return rc;
		}

		p_hwfn->hw_init_done = true;
	}

	if (IS_PF(cdev)) {
		p_hwfn = QED_LEADING_HWFN(cdev);

		 
		DP_VERBOSE(p_hwfn,
			   QED_MSG_SPQ,
			   "Sending GET_OEM_UPDATES command to trigger stag/bandwidth attention handling\n");
		drv_mb_param = 1 << DRV_MB_PARAM_DUMMY_OEM_UPDATES_OFFSET;
		rc = qed_mcp_cmd(p_hwfn, p_hwfn->p_main_ptt,
				 DRV_MSG_CODE_GET_OEM_UPDATES,
				 drv_mb_param, &resp, &param);
		if (rc)
			DP_NOTICE(p_hwfn,
				  "Failed to send GET_OEM_UPDATES attention request\n");

		drv_mb_param = STORM_FW_VERSION;
		rc = qed_mcp_cmd(p_hwfn, p_hwfn->p_main_ptt,
				 DRV_MSG_CODE_OV_UPDATE_STORM_FW_VER,
				 drv_mb_param, &load_code, &param);
		if (rc)
			DP_INFO(p_hwfn, "Failed to update firmware version\n");

		if (!b_default_mtu) {
			rc = qed_mcp_ov_update_mtu(p_hwfn, p_hwfn->p_main_ptt,
						   p_hwfn->hw_info.mtu);
			if (rc)
				DP_INFO(p_hwfn,
					"Failed to update default mtu\n");
		}

		rc = qed_mcp_ov_update_driver_state(p_hwfn,
						    p_hwfn->p_main_ptt,
						  QED_OV_DRIVER_STATE_DISABLED);
		if (rc)
			DP_INFO(p_hwfn, "Failed to update driver state\n");

		rc = qed_mcp_ov_update_eswitch(p_hwfn, p_hwfn->p_main_ptt,
					       QED_OV_ESWITCH_NONE);
		if (rc)
			DP_INFO(p_hwfn, "Failed to update eswitch mode\n");
	}

	return 0;

load_err:
	 
	qed_mcp_load_done(p_hwfn, p_hwfn->p_main_ptt);
	return rc;
}

#define QED_HW_STOP_RETRY_LIMIT (10)
static void qed_hw_timers_stop(struct qed_dev *cdev,
			       struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt)
{
	int i;

	 
	qed_wr(p_hwfn, p_ptt, TM_REG_PF_ENABLE_CONN, 0x0);
	qed_wr(p_hwfn, p_ptt, TM_REG_PF_ENABLE_TASK, 0x0);

	if (cdev->recov_in_prog)
		return;

	for (i = 0; i < QED_HW_STOP_RETRY_LIMIT; i++) {
		if ((!qed_rd(p_hwfn, p_ptt,
			     TM_REG_PF_SCAN_ACTIVE_CONN)) &&
		    (!qed_rd(p_hwfn, p_ptt, TM_REG_PF_SCAN_ACTIVE_TASK)))
			break;

		 
		usleep_range(1000, 2000);
	}

	if (i < QED_HW_STOP_RETRY_LIMIT)
		return;

	DP_NOTICE(p_hwfn,
		  "Timers linear scans are not over [Connection %02x Tasks %02x]\n",
		  (u8)qed_rd(p_hwfn, p_ptt, TM_REG_PF_SCAN_ACTIVE_CONN),
		  (u8)qed_rd(p_hwfn, p_ptt, TM_REG_PF_SCAN_ACTIVE_TASK));
}

void qed_hw_timers_stop_all(struct qed_dev *cdev)
{
	int j;

	for_each_hwfn(cdev, j) {
		struct qed_hwfn *p_hwfn = &cdev->hwfns[j];
		struct qed_ptt *p_ptt = p_hwfn->p_main_ptt;

		qed_hw_timers_stop(cdev, p_hwfn, p_ptt);
	}
}

int qed_hw_stop(struct qed_dev *cdev)
{
	struct qed_hwfn *p_hwfn;
	struct qed_ptt *p_ptt;
	int rc, rc2 = 0;
	int j;

	for_each_hwfn(cdev, j) {
		p_hwfn = &cdev->hwfns[j];
		p_ptt = p_hwfn->p_main_ptt;

		DP_VERBOSE(p_hwfn, NETIF_MSG_IFDOWN, "Stopping hw/fw\n");

		if (IS_VF(cdev)) {
			qed_vf_pf_int_cleanup(p_hwfn);
			rc = qed_vf_pf_reset(p_hwfn);
			if (rc) {
				DP_NOTICE(p_hwfn,
					  "qed_vf_pf_reset failed. rc = %d.\n",
					  rc);
				rc2 = -EINVAL;
			}
			continue;
		}

		 
		p_hwfn->hw_init_done = false;

		 
		if (!cdev->recov_in_prog) {
			rc = qed_mcp_unload_req(p_hwfn, p_ptt);
			if (rc) {
				DP_NOTICE(p_hwfn,
					  "Failed sending a UNLOAD_REQ command. rc = %d.\n",
					  rc);
				rc2 = -EINVAL;
			}
		}

		qed_slowpath_irq_sync(p_hwfn);

		 
		rc = qed_sp_pf_stop(p_hwfn);
		if (rc) {
			DP_NOTICE(p_hwfn,
				  "Failed to close PF against FW [rc = %d]. Continue to stop HW to prevent illegal host access by the device.\n",
				  rc);
			rc2 = -EINVAL;
		}

		qed_wr(p_hwfn, p_ptt,
		       NIG_REG_RX_LLH_BRB_GATE_DNTFWD_PERPF, 0x1);

		qed_wr(p_hwfn, p_ptt, PRS_REG_SEARCH_TCP, 0x0);
		qed_wr(p_hwfn, p_ptt, PRS_REG_SEARCH_UDP, 0x0);
		qed_wr(p_hwfn, p_ptt, PRS_REG_SEARCH_FCOE, 0x0);
		qed_wr(p_hwfn, p_ptt, PRS_REG_SEARCH_ROCE, 0x0);
		qed_wr(p_hwfn, p_ptt, PRS_REG_SEARCH_OPENFLOW, 0x0);

		qed_hw_timers_stop(cdev, p_hwfn, p_ptt);

		 
		qed_int_igu_disable_int(p_hwfn, p_ptt);

		qed_wr(p_hwfn, p_ptt, IGU_REG_LEADING_EDGE_LATCH, 0);
		qed_wr(p_hwfn, p_ptt, IGU_REG_TRAILING_EDGE_LATCH, 0);

		qed_int_igu_init_pure_rt(p_hwfn, p_ptt, false, true);

		 
		usleep_range(1000, 2000);

		 
		qed_wr(p_hwfn, p_ptt, DORQ_REG_PF_DB_ENABLE, 0);
		qed_wr(p_hwfn, p_ptt, QM_REG_PF_EN, 0);

		if (IS_LEAD_HWFN(p_hwfn) &&
		    test_bit(QED_MF_LLH_MAC_CLSS, &cdev->mf_bits) &&
		    !QED_IS_FCOE_PERSONALITY(p_hwfn))
			qed_llh_remove_mac_filter(cdev, 0,
						  p_hwfn->hw_info.hw_mac_addr);

		if (!cdev->recov_in_prog) {
			rc = qed_mcp_unload_done(p_hwfn, p_ptt);
			if (rc) {
				DP_NOTICE(p_hwfn,
					  "Failed sending a UNLOAD_DONE command. rc = %d.\n",
					  rc);
				rc2 = -EINVAL;
			}
		}
	}

	if (IS_PF(cdev) && !cdev->recov_in_prog) {
		p_hwfn = QED_LEADING_HWFN(cdev);
		p_ptt = QED_LEADING_HWFN(cdev)->p_main_ptt;

		 
		rc = qed_pglueb_set_pfid_enable(p_hwfn, p_ptt, false);
		if (rc) {
			DP_NOTICE(p_hwfn,
				  "qed_pglueb_set_pfid_enable() failed. rc = %d.\n",
				  rc);
			rc2 = -EINVAL;
		}
	}

	return rc2;
}

int qed_hw_stop_fastpath(struct qed_dev *cdev)
{
	int j;

	for_each_hwfn(cdev, j) {
		struct qed_hwfn *p_hwfn = &cdev->hwfns[j];
		struct qed_ptt *p_ptt;

		if (IS_VF(cdev)) {
			qed_vf_pf_int_cleanup(p_hwfn);
			continue;
		}
		p_ptt = qed_ptt_acquire(p_hwfn);
		if (!p_ptt)
			return -EAGAIN;

		DP_VERBOSE(p_hwfn,
			   NETIF_MSG_IFDOWN, "Shutting down the fastpath\n");

		qed_wr(p_hwfn, p_ptt,
		       NIG_REG_RX_LLH_BRB_GATE_DNTFWD_PERPF, 0x1);

		qed_wr(p_hwfn, p_ptt, PRS_REG_SEARCH_TCP, 0x0);
		qed_wr(p_hwfn, p_ptt, PRS_REG_SEARCH_UDP, 0x0);
		qed_wr(p_hwfn, p_ptt, PRS_REG_SEARCH_FCOE, 0x0);
		qed_wr(p_hwfn, p_ptt, PRS_REG_SEARCH_ROCE, 0x0);
		qed_wr(p_hwfn, p_ptt, PRS_REG_SEARCH_OPENFLOW, 0x0);

		qed_int_igu_init_pure_rt(p_hwfn, p_ptt, false, false);

		 
		usleep_range(1000, 2000);
		qed_ptt_release(p_hwfn, p_ptt);
	}

	return 0;
}

int qed_hw_start_fastpath(struct qed_hwfn *p_hwfn)
{
	struct qed_ptt *p_ptt;

	if (IS_VF(p_hwfn->cdev))
		return 0;

	p_ptt = qed_ptt_acquire(p_hwfn);
	if (!p_ptt)
		return -EAGAIN;

	if (p_hwfn->p_rdma_info &&
	    p_hwfn->p_rdma_info->active && p_hwfn->b_rdma_enabled_in_prs)
		qed_wr(p_hwfn, p_ptt, p_hwfn->rdma_prs_search_reg, 0x1);

	 
	qed_wr(p_hwfn, p_ptt, NIG_REG_RX_LLH_BRB_GATE_DNTFWD_PERPF, 0x0);
	qed_ptt_release(p_hwfn, p_ptt);

	return 0;
}

 
static void qed_hw_hwfn_free(struct qed_hwfn *p_hwfn)
{
	qed_ptt_pool_free(p_hwfn);
	kfree(p_hwfn->hw_info.p_igu_info);
	p_hwfn->hw_info.p_igu_info = NULL;
}

 
static void qed_hw_hwfn_prepare(struct qed_hwfn *p_hwfn)
{
	 
	if (QED_IS_AH(p_hwfn->cdev)) {
		qed_wr(p_hwfn, p_hwfn->p_main_ptt,
		       PGLUE_B_REG_PGL_ADDR_E8_F0_K2, 0);
		qed_wr(p_hwfn, p_hwfn->p_main_ptt,
		       PGLUE_B_REG_PGL_ADDR_EC_F0_K2, 0);
		qed_wr(p_hwfn, p_hwfn->p_main_ptt,
		       PGLUE_B_REG_PGL_ADDR_F0_F0_K2, 0);
		qed_wr(p_hwfn, p_hwfn->p_main_ptt,
		       PGLUE_B_REG_PGL_ADDR_F4_F0_K2, 0);
	} else {
		qed_wr(p_hwfn, p_hwfn->p_main_ptt,
		       PGLUE_B_REG_PGL_ADDR_88_F0_BB, 0);
		qed_wr(p_hwfn, p_hwfn->p_main_ptt,
		       PGLUE_B_REG_PGL_ADDR_8C_F0_BB, 0);
		qed_wr(p_hwfn, p_hwfn->p_main_ptt,
		       PGLUE_B_REG_PGL_ADDR_90_F0_BB, 0);
		qed_wr(p_hwfn, p_hwfn->p_main_ptt,
		       PGLUE_B_REG_PGL_ADDR_94_F0_BB, 0);
	}

	 
	qed_pglueb_clear_err(p_hwfn, p_hwfn->p_main_ptt);

	 
	qed_wr(p_hwfn, p_hwfn->p_main_ptt,
	       PGLUE_B_REG_INTERNAL_PFID_ENABLE_TARGET_READ, 1);
}

static void get_function_id(struct qed_hwfn *p_hwfn)
{
	 
	p_hwfn->hw_info.opaque_fid = (u16)REG_RD(p_hwfn,
						 PXP_PF_ME_OPAQUE_ADDR);

	p_hwfn->hw_info.concrete_fid = REG_RD(p_hwfn, PXP_PF_ME_CONCRETE_ADDR);

	p_hwfn->abs_pf_id = (p_hwfn->hw_info.concrete_fid >> 16) & 0xf;
	p_hwfn->rel_pf_id = GET_FIELD(p_hwfn->hw_info.concrete_fid,
				      PXP_CONCRETE_FID_PFID);
	p_hwfn->port_id = GET_FIELD(p_hwfn->hw_info.concrete_fid,
				    PXP_CONCRETE_FID_PORT);

	DP_VERBOSE(p_hwfn, NETIF_MSG_PROBE,
		   "Read ME register: Concrete 0x%08x Opaque 0x%04x\n",
		   p_hwfn->hw_info.concrete_fid, p_hwfn->hw_info.opaque_fid);
}

static void qed_hw_set_feat(struct qed_hwfn *p_hwfn)
{
	u32 *feat_num = p_hwfn->hw_info.feat_num;
	struct qed_sb_cnt_info sb_cnt;
	u32 non_l2_sbs = 0;

	memset(&sb_cnt, 0, sizeof(sb_cnt));
	qed_int_get_num_sbs(p_hwfn, &sb_cnt);

	if (IS_ENABLED(CONFIG_QED_RDMA) &&
	    QED_IS_RDMA_PERSONALITY(p_hwfn)) {
		 
		feat_num[QED_RDMA_CNQ] =
			min_t(u32, sb_cnt.cnt / 2,
			      RESC_NUM(p_hwfn, QED_RDMA_CNQ_RAM));

		non_l2_sbs = feat_num[QED_RDMA_CNQ];
	}
	if (QED_IS_L2_PERSONALITY(p_hwfn)) {
		 
		feat_num[QED_VF_L2_QUE] = min_t(u32,
						RESC_NUM(p_hwfn, QED_L2_QUEUE),
						sb_cnt.iov_cnt);
		feat_num[QED_PF_L2_QUE] = min_t(u32,
						sb_cnt.cnt - non_l2_sbs,
						RESC_NUM(p_hwfn,
							 QED_L2_QUEUE) -
						FEAT_NUM(p_hwfn,
							 QED_VF_L2_QUE));
	}

	if (QED_IS_FCOE_PERSONALITY(p_hwfn))
		feat_num[QED_FCOE_CQ] =  min_t(u32, sb_cnt.cnt,
					       RESC_NUM(p_hwfn,
							QED_CMDQS_CQS));

	if (QED_IS_ISCSI_PERSONALITY(p_hwfn))
		feat_num[QED_ISCSI_CQ] = min_t(u32, sb_cnt.cnt,
					       RESC_NUM(p_hwfn,
							QED_CMDQS_CQS));

	if (QED_IS_NVMETCP_PERSONALITY(p_hwfn))
		feat_num[QED_NVMETCP_CQ] = min_t(u32, sb_cnt.cnt,
						 RESC_NUM(p_hwfn,
							  QED_CMDQS_CQS));

	DP_VERBOSE(p_hwfn,
		   NETIF_MSG_PROBE,
		   "#PF_L2_QUEUES=%d VF_L2_QUEUES=%d #ROCE_CNQ=%d FCOE_CQ=%d ISCSI_CQ=%d NVMETCP_CQ=%d #SBS=%d\n",
		   (int)FEAT_NUM(p_hwfn, QED_PF_L2_QUE),
		   (int)FEAT_NUM(p_hwfn, QED_VF_L2_QUE),
		   (int)FEAT_NUM(p_hwfn, QED_RDMA_CNQ),
		   (int)FEAT_NUM(p_hwfn, QED_FCOE_CQ),
		   (int)FEAT_NUM(p_hwfn, QED_ISCSI_CQ),
		   (int)FEAT_NUM(p_hwfn, QED_NVMETCP_CQ),
		   (int)sb_cnt.cnt);
}

const char *qed_hw_get_resc_name(enum qed_resources res_id)
{
	switch (res_id) {
	case QED_L2_QUEUE:
		return "L2_QUEUE";
	case QED_VPORT:
		return "VPORT";
	case QED_RSS_ENG:
		return "RSS_ENG";
	case QED_PQ:
		return "PQ";
	case QED_RL:
		return "RL";
	case QED_MAC:
		return "MAC";
	case QED_VLAN:
		return "VLAN";
	case QED_RDMA_CNQ_RAM:
		return "RDMA_CNQ_RAM";
	case QED_ILT:
		return "ILT";
	case QED_LL2_RAM_QUEUE:
		return "LL2_RAM_QUEUE";
	case QED_LL2_CTX_QUEUE:
		return "LL2_CTX_QUEUE";
	case QED_CMDQS_CQS:
		return "CMDQS_CQS";
	case QED_RDMA_STATS_QUEUE:
		return "RDMA_STATS_QUEUE";
	case QED_BDQ:
		return "BDQ";
	case QED_SB:
		return "SB";
	default:
		return "UNKNOWN_RESOURCE";
	}
}

static int
__qed_hw_set_soft_resc_size(struct qed_hwfn *p_hwfn,
			    struct qed_ptt *p_ptt,
			    enum qed_resources res_id,
			    u32 resc_max_val, u32 *p_mcp_resp)
{
	int rc;

	rc = qed_mcp_set_resc_max_val(p_hwfn, p_ptt, res_id,
				      resc_max_val, p_mcp_resp);
	if (rc) {
		DP_NOTICE(p_hwfn,
			  "MFW response failure for a max value setting of resource %d [%s]\n",
			  res_id, qed_hw_get_resc_name(res_id));
		return rc;
	}

	if (*p_mcp_resp != FW_MSG_CODE_RESOURCE_ALLOC_OK)
		DP_INFO(p_hwfn,
			"Failed to set the max value of resource %d [%s]. mcp_resp = 0x%08x.\n",
			res_id, qed_hw_get_resc_name(res_id), *p_mcp_resp);

	return 0;
}

static u32 qed_hsi_def_val[][MAX_CHIP_IDS] = {
	{MAX_NUM_VFS_BB, MAX_NUM_VFS_K2},
	{MAX_NUM_L2_QUEUES_BB, MAX_NUM_L2_QUEUES_K2},
	{MAX_NUM_PORTS_BB, MAX_NUM_PORTS_K2},
	{MAX_SB_PER_PATH_BB, MAX_SB_PER_PATH_K2,},
	{MAX_NUM_PFS_BB, MAX_NUM_PFS_K2},
	{MAX_NUM_VPORTS_BB, MAX_NUM_VPORTS_K2},
	{ETH_RSS_ENGINE_NUM_BB, ETH_RSS_ENGINE_NUM_K2},
	{MAX_QM_TX_QUEUES_BB, MAX_QM_TX_QUEUES_K2},
	{PXP_NUM_ILT_RECORDS_BB, PXP_NUM_ILT_RECORDS_K2},
	{RDMA_NUM_STATISTIC_COUNTERS_BB, RDMA_NUM_STATISTIC_COUNTERS_K2},
	{MAX_QM_GLOBAL_RLS, MAX_QM_GLOBAL_RLS},
	{PBF_MAX_CMD_LINES, PBF_MAX_CMD_LINES},
	{BTB_MAX_BLOCKS_BB, BTB_MAX_BLOCKS_K2},
};

u32 qed_get_hsi_def_val(struct qed_dev *cdev, enum qed_hsi_def_type type)
{
	enum chip_ids chip_id = QED_IS_BB(cdev) ? CHIP_BB : CHIP_K2;

	if (type >= QED_NUM_HSI_DEFS) {
		DP_ERR(cdev, "Unexpected HSI definition type [%d]\n", type);
		return 0;
	}

	return qed_hsi_def_val[type][chip_id];
}

static int
qed_hw_set_soft_resc_size(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt)
{
	u32 resc_max_val, mcp_resp;
	u8 res_id;
	int rc;

	for (res_id = 0; res_id < QED_MAX_RESC; res_id++) {
		switch (res_id) {
		case QED_LL2_RAM_QUEUE:
			resc_max_val = MAX_NUM_LL2_RX_RAM_QUEUES;
			break;
		case QED_LL2_CTX_QUEUE:
			resc_max_val = MAX_NUM_LL2_RX_CTX_QUEUES;
			break;
		case QED_RDMA_CNQ_RAM:
			 
			resc_max_val = NUM_OF_GLOBAL_QUEUES;
			break;
		case QED_RDMA_STATS_QUEUE:
			resc_max_val =
			    NUM_OF_RDMA_STATISTIC_COUNTERS(p_hwfn->cdev);
			break;
		case QED_BDQ:
			resc_max_val = BDQ_NUM_RESOURCES;
			break;
		default:
			continue;
		}

		rc = __qed_hw_set_soft_resc_size(p_hwfn, p_ptt, res_id,
						 resc_max_val, &mcp_resp);
		if (rc)
			return rc;

		 
		if (mcp_resp == FW_MSG_CODE_UNSUPPORTED)
			return -EINVAL;
	}

	return 0;
}

static
int qed_hw_get_dflt_resc(struct qed_hwfn *p_hwfn,
			 enum qed_resources res_id,
			 u32 *p_resc_num, u32 *p_resc_start)
{
	u8 num_funcs = p_hwfn->num_funcs_on_engine;
	struct qed_dev *cdev = p_hwfn->cdev;

	switch (res_id) {
	case QED_L2_QUEUE:
		*p_resc_num = NUM_OF_L2_QUEUES(cdev) / num_funcs;
		break;
	case QED_VPORT:
		*p_resc_num = NUM_OF_VPORTS(cdev) / num_funcs;
		break;
	case QED_RSS_ENG:
		*p_resc_num = NUM_OF_RSS_ENGINES(cdev) / num_funcs;
		break;
	case QED_PQ:
		*p_resc_num = NUM_OF_QM_TX_QUEUES(cdev) / num_funcs;
		*p_resc_num &= ~0x7;	 
		break;
	case QED_RL:
		*p_resc_num = NUM_OF_QM_GLOBAL_RLS(cdev) / num_funcs;
		break;
	case QED_MAC:
	case QED_VLAN:
		 
		*p_resc_num = ETH_NUM_MAC_FILTERS / num_funcs;
		break;
	case QED_ILT:
		*p_resc_num = NUM_OF_PXP_ILT_RECORDS(cdev) / num_funcs;
		break;
	case QED_LL2_RAM_QUEUE:
		*p_resc_num = MAX_NUM_LL2_RX_RAM_QUEUES / num_funcs;
		break;
	case QED_LL2_CTX_QUEUE:
		*p_resc_num = MAX_NUM_LL2_RX_CTX_QUEUES / num_funcs;
		break;
	case QED_RDMA_CNQ_RAM:
	case QED_CMDQS_CQS:
		 
		*p_resc_num = NUM_OF_GLOBAL_QUEUES / num_funcs;
		break;
	case QED_RDMA_STATS_QUEUE:
		*p_resc_num = NUM_OF_RDMA_STATISTIC_COUNTERS(cdev) / num_funcs;
		break;
	case QED_BDQ:
		if (p_hwfn->hw_info.personality != QED_PCI_ISCSI &&
		    p_hwfn->hw_info.personality != QED_PCI_FCOE &&
			p_hwfn->hw_info.personality != QED_PCI_NVMETCP)
			*p_resc_num = 0;
		else
			*p_resc_num = 1;
		break;
	case QED_SB:
		 
		*p_resc_num = 0;
		break;
	default:
		return -EINVAL;
	}

	switch (res_id) {
	case QED_BDQ:
		if (!*p_resc_num)
			*p_resc_start = 0;
		else if (p_hwfn->cdev->num_ports_in_engine == 4)
			*p_resc_start = p_hwfn->port_id;
		else if (p_hwfn->hw_info.personality == QED_PCI_ISCSI ||
			 p_hwfn->hw_info.personality == QED_PCI_NVMETCP)
			*p_resc_start = p_hwfn->port_id;
		else if (p_hwfn->hw_info.personality == QED_PCI_FCOE)
			*p_resc_start = p_hwfn->port_id + 2;
		break;
	default:
		*p_resc_start = *p_resc_num * p_hwfn->enabled_func_idx;
		break;
	}

	return 0;
}

static int __qed_hw_set_resc_info(struct qed_hwfn *p_hwfn,
				  enum qed_resources res_id)
{
	u32 dflt_resc_num = 0, dflt_resc_start = 0;
	u32 mcp_resp, *p_resc_num, *p_resc_start;
	int rc;

	p_resc_num = &RESC_NUM(p_hwfn, res_id);
	p_resc_start = &RESC_START(p_hwfn, res_id);

	rc = qed_hw_get_dflt_resc(p_hwfn, res_id, &dflt_resc_num,
				  &dflt_resc_start);
	if (rc) {
		DP_ERR(p_hwfn,
		       "Failed to get default amount for resource %d [%s]\n",
		       res_id, qed_hw_get_resc_name(res_id));
		return rc;
	}

	rc = qed_mcp_get_resc_info(p_hwfn, p_hwfn->p_main_ptt, res_id,
				   &mcp_resp, p_resc_num, p_resc_start);
	if (rc) {
		DP_NOTICE(p_hwfn,
			  "MFW response failure for an allocation request for resource %d [%s]\n",
			  res_id, qed_hw_get_resc_name(res_id));
		return rc;
	}

	 
	if (mcp_resp != FW_MSG_CODE_RESOURCE_ALLOC_OK) {
		DP_INFO(p_hwfn,
			"Failed to receive allocation info for resource %d [%s]. mcp_resp = 0x%x. Applying default values [%d,%d].\n",
			res_id,
			qed_hw_get_resc_name(res_id),
			mcp_resp, dflt_resc_num, dflt_resc_start);
		*p_resc_num = dflt_resc_num;
		*p_resc_start = dflt_resc_start;
		goto out;
	}

out:
	 
	if ((res_id == QED_PQ) && ((*p_resc_num % 8) || (*p_resc_start % 8))) {
		DP_INFO(p_hwfn,
			"PQs need to align by 8; Number %08x --> %08x, Start %08x --> %08x\n",
			*p_resc_num,
			(*p_resc_num) & ~0x7,
			*p_resc_start, (*p_resc_start) & ~0x7);
		*p_resc_num &= ~0x7;
		*p_resc_start &= ~0x7;
	}

	return 0;
}

static int qed_hw_set_resc_info(struct qed_hwfn *p_hwfn)
{
	int rc;
	u8 res_id;

	for (res_id = 0; res_id < QED_MAX_RESC; res_id++) {
		rc = __qed_hw_set_resc_info(p_hwfn, res_id);
		if (rc)
			return rc;
	}

	return 0;
}

static int qed_hw_get_ppfid_bitmap(struct qed_hwfn *p_hwfn,
				   struct qed_ptt *p_ptt)
{
	struct qed_dev *cdev = p_hwfn->cdev;
	u8 native_ppfid_idx;
	int rc;

	 
	if (QED_IS_BB(cdev))
		native_ppfid_idx = p_hwfn->rel_pf_id;
	else
		native_ppfid_idx = p_hwfn->rel_pf_id /
		    cdev->num_ports_in_engine;

	rc = qed_mcp_get_ppfid_bitmap(p_hwfn, p_ptt);
	if (rc != 0 && rc != -EOPNOTSUPP)
		return rc;
	else if (rc == -EOPNOTSUPP)
		cdev->ppfid_bitmap = 0x1 << native_ppfid_idx;

	if (!(cdev->ppfid_bitmap & (0x1 << native_ppfid_idx))) {
		DP_INFO(p_hwfn,
			"Fix the PPFID bitmap to include the native PPFID [native_ppfid_idx %hhd, orig_bitmap 0x%hhx]\n",
			native_ppfid_idx, cdev->ppfid_bitmap);
		cdev->ppfid_bitmap = 0x1 << native_ppfid_idx;
	}

	return 0;
}

static int qed_hw_get_resc(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt)
{
	struct qed_resc_unlock_params resc_unlock_params;
	struct qed_resc_lock_params resc_lock_params;
	bool b_ah = QED_IS_AH(p_hwfn->cdev);
	u8 res_id;
	int rc;

	 
	qed_mcp_resc_lock_default_init(&resc_lock_params, &resc_unlock_params,
				       QED_RESC_LOCK_RESC_ALLOC, false);

	rc = qed_mcp_resc_lock(p_hwfn, p_ptt, &resc_lock_params);
	if (rc && rc != -EINVAL) {
		return rc;
	} else if (rc == -EINVAL) {
		DP_INFO(p_hwfn,
			"Skip the max values setting of the soft resources since the resource lock is not supported by the MFW\n");
	} else if (!resc_lock_params.b_granted) {
		DP_NOTICE(p_hwfn,
			  "Failed to acquire the resource lock for the resource allocation commands\n");
		return -EBUSY;
	} else {
		rc = qed_hw_set_soft_resc_size(p_hwfn, p_ptt);
		if (rc && rc != -EINVAL) {
			DP_NOTICE(p_hwfn,
				  "Failed to set the max values of the soft resources\n");
			goto unlock_and_exit;
		} else if (rc == -EINVAL) {
			DP_INFO(p_hwfn,
				"Skip the max values setting of the soft resources since it is not supported by the MFW\n");
			rc = qed_mcp_resc_unlock(p_hwfn, p_ptt,
						 &resc_unlock_params);
			if (rc)
				DP_INFO(p_hwfn,
					"Failed to release the resource lock for the resource allocation commands\n");
		}
	}

	rc = qed_hw_set_resc_info(p_hwfn);
	if (rc)
		goto unlock_and_exit;

	if (resc_lock_params.b_granted && !resc_unlock_params.b_released) {
		rc = qed_mcp_resc_unlock(p_hwfn, p_ptt, &resc_unlock_params);
		if (rc)
			DP_INFO(p_hwfn,
				"Failed to release the resource lock for the resource allocation commands\n");
	}

	 
	if (IS_LEAD_HWFN(p_hwfn)) {
		rc = qed_hw_get_ppfid_bitmap(p_hwfn, p_ptt);
		if (rc)
			return rc;
	}

	 
	if ((b_ah && (RESC_END(p_hwfn, QED_ILT) > PXP_NUM_ILT_RECORDS_K2)) ||
	    (!b_ah && (RESC_END(p_hwfn, QED_ILT) > PXP_NUM_ILT_RECORDS_BB))) {
		DP_NOTICE(p_hwfn, "Can't assign ILT pages [%08x,...,%08x]\n",
			  RESC_START(p_hwfn, QED_ILT),
			  RESC_END(p_hwfn, QED_ILT) - 1);
		return -EINVAL;
	}

	 
	if (qed_int_igu_reset_cam(p_hwfn, p_ptt))
		return -EINVAL;

	qed_hw_set_feat(p_hwfn);

	for (res_id = 0; res_id < QED_MAX_RESC; res_id++)
		DP_VERBOSE(p_hwfn, NETIF_MSG_PROBE, "%s = %d start = %d\n",
			   qed_hw_get_resc_name(res_id),
			   RESC_NUM(p_hwfn, res_id),
			   RESC_START(p_hwfn, res_id));

	return 0;

unlock_and_exit:
	if (resc_lock_params.b_granted && !resc_unlock_params.b_released)
		qed_mcp_resc_unlock(p_hwfn, p_ptt, &resc_unlock_params);
	return rc;
}

static int qed_hw_get_nvm_info(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt)
{
	u32 port_cfg_addr, link_temp, nvm_cfg_addr, device_capabilities, fld;
	u32 nvm_cfg1_offset, mf_mode, addr, generic_cont0, core_cfg;
	struct qed_mcp_link_speed_params *ext_speed;
	struct qed_mcp_link_capabilities *p_caps;
	struct qed_mcp_link_params *link;
	int i;

	 
	nvm_cfg_addr = qed_rd(p_hwfn, p_ptt, MISC_REG_GEN_PURP_CR0);

	 
	if (!nvm_cfg_addr) {
		DP_NOTICE(p_hwfn, "Shared memory not initialized\n");
		return -EINVAL;
	}

	 
	nvm_cfg1_offset = qed_rd(p_hwfn, p_ptt, nvm_cfg_addr + 4);

	addr = MCP_REG_SCRATCH + nvm_cfg1_offset +
	       offsetof(struct nvm_cfg1, glob) +
	       offsetof(struct nvm_cfg1_glob, core_cfg);

	core_cfg = qed_rd(p_hwfn, p_ptt, addr);

	switch ((core_cfg & NVM_CFG1_GLOB_NETWORK_PORT_MODE_MASK) >>
		NVM_CFG1_GLOB_NETWORK_PORT_MODE_OFFSET) {
	case NVM_CFG1_GLOB_NETWORK_PORT_MODE_BB_2X40G:
	case NVM_CFG1_GLOB_NETWORK_PORT_MODE_2X50G:
	case NVM_CFG1_GLOB_NETWORK_PORT_MODE_BB_1X100G:
	case NVM_CFG1_GLOB_NETWORK_PORT_MODE_4X10G_F:
	case NVM_CFG1_GLOB_NETWORK_PORT_MODE_BB_4X10G_E:
	case NVM_CFG1_GLOB_NETWORK_PORT_MODE_BB_4X20G:
	case NVM_CFG1_GLOB_NETWORK_PORT_MODE_1X40G:
	case NVM_CFG1_GLOB_NETWORK_PORT_MODE_2X25G:
	case NVM_CFG1_GLOB_NETWORK_PORT_MODE_2X10G:
	case NVM_CFG1_GLOB_NETWORK_PORT_MODE_1X25G:
	case NVM_CFG1_GLOB_NETWORK_PORT_MODE_4X25G:
	case NVM_CFG1_GLOB_NETWORK_PORT_MODE_AHP_2X50G_R1:
	case NVM_CFG1_GLOB_NETWORK_PORT_MODE_AHP_4X50G_R1:
	case NVM_CFG1_GLOB_NETWORK_PORT_MODE_AHP_1X100G_R2:
	case NVM_CFG1_GLOB_NETWORK_PORT_MODE_AHP_2X100G_R2:
	case NVM_CFG1_GLOB_NETWORK_PORT_MODE_AHP_1X100G_R4:
		break;
	default:
		DP_NOTICE(p_hwfn, "Unknown port mode in 0x%08x\n", core_cfg);
		break;
	}

	 
	link = &p_hwfn->mcp_info->link_input;
	p_caps = &p_hwfn->mcp_info->link_capabilities;
	port_cfg_addr = MCP_REG_SCRATCH + nvm_cfg1_offset +
			offsetof(struct nvm_cfg1, port[MFW_PORT(p_hwfn)]);
	link_temp = qed_rd(p_hwfn, p_ptt,
			   port_cfg_addr +
			   offsetof(struct nvm_cfg1_port, speed_cap_mask));
	link_temp &= NVM_CFG1_PORT_DRV_SPEED_CAPABILITY_MASK_MASK;
	link->speed.advertised_speeds = link_temp;

	p_caps->speed_capabilities = link->speed.advertised_speeds;

	link_temp = qed_rd(p_hwfn, p_ptt,
			   port_cfg_addr +
			   offsetof(struct nvm_cfg1_port, link_settings));
	switch ((link_temp & NVM_CFG1_PORT_DRV_LINK_SPEED_MASK) >>
		NVM_CFG1_PORT_DRV_LINK_SPEED_OFFSET) {
	case NVM_CFG1_PORT_DRV_LINK_SPEED_AUTONEG:
		link->speed.autoneg = true;
		break;
	case NVM_CFG1_PORT_DRV_LINK_SPEED_1G:
		link->speed.forced_speed = 1000;
		break;
	case NVM_CFG1_PORT_DRV_LINK_SPEED_10G:
		link->speed.forced_speed = 10000;
		break;
	case NVM_CFG1_PORT_DRV_LINK_SPEED_20G:
		link->speed.forced_speed = 20000;
		break;
	case NVM_CFG1_PORT_DRV_LINK_SPEED_25G:
		link->speed.forced_speed = 25000;
		break;
	case NVM_CFG1_PORT_DRV_LINK_SPEED_40G:
		link->speed.forced_speed = 40000;
		break;
	case NVM_CFG1_PORT_DRV_LINK_SPEED_50G:
		link->speed.forced_speed = 50000;
		break;
	case NVM_CFG1_PORT_DRV_LINK_SPEED_BB_100G:
		link->speed.forced_speed = 100000;
		break;
	default:
		DP_NOTICE(p_hwfn, "Unknown Speed in 0x%08x\n", link_temp);
	}

	p_caps->default_speed_autoneg = link->speed.autoneg;

	fld = GET_MFW_FIELD(link_temp, NVM_CFG1_PORT_DRV_FLOW_CONTROL);
	link->pause.autoneg = !!(fld & NVM_CFG1_PORT_DRV_FLOW_CONTROL_AUTONEG);
	link->pause.forced_rx = !!(fld & NVM_CFG1_PORT_DRV_FLOW_CONTROL_RX);
	link->pause.forced_tx = !!(fld & NVM_CFG1_PORT_DRV_FLOW_CONTROL_TX);
	link->loopback_mode = 0;

	if (p_hwfn->mcp_info->capabilities &
	    FW_MB_PARAM_FEATURE_SUPPORT_FEC_CONTROL) {
		switch (GET_MFW_FIELD(link_temp,
				      NVM_CFG1_PORT_FEC_FORCE_MODE)) {
		case NVM_CFG1_PORT_FEC_FORCE_MODE_NONE:
			p_caps->fec_default |= QED_FEC_MODE_NONE;
			break;
		case NVM_CFG1_PORT_FEC_FORCE_MODE_FIRECODE:
			p_caps->fec_default |= QED_FEC_MODE_FIRECODE;
			break;
		case NVM_CFG1_PORT_FEC_FORCE_MODE_RS:
			p_caps->fec_default |= QED_FEC_MODE_RS;
			break;
		case NVM_CFG1_PORT_FEC_FORCE_MODE_AUTO:
			p_caps->fec_default |= QED_FEC_MODE_AUTO;
			break;
		default:
			DP_VERBOSE(p_hwfn, NETIF_MSG_LINK,
				   "unknown FEC mode in 0x%08x\n", link_temp);
		}
	} else {
		p_caps->fec_default = QED_FEC_MODE_UNSUPPORTED;
	}

	link->fec = p_caps->fec_default;

	if (p_hwfn->mcp_info->capabilities & FW_MB_PARAM_FEATURE_SUPPORT_EEE) {
		link_temp = qed_rd(p_hwfn, p_ptt, port_cfg_addr +
				   offsetof(struct nvm_cfg1_port, ext_phy));
		link_temp &= NVM_CFG1_PORT_EEE_POWER_SAVING_MODE_MASK;
		link_temp >>= NVM_CFG1_PORT_EEE_POWER_SAVING_MODE_OFFSET;
		p_caps->default_eee = QED_MCP_EEE_ENABLED;
		link->eee.enable = true;
		switch (link_temp) {
		case NVM_CFG1_PORT_EEE_POWER_SAVING_MODE_DISABLED:
			p_caps->default_eee = QED_MCP_EEE_DISABLED;
			link->eee.enable = false;
			break;
		case NVM_CFG1_PORT_EEE_POWER_SAVING_MODE_BALANCED:
			p_caps->eee_lpi_timer = EEE_TX_TIMER_USEC_BALANCED_TIME;
			break;
		case NVM_CFG1_PORT_EEE_POWER_SAVING_MODE_AGGRESSIVE:
			p_caps->eee_lpi_timer =
			    EEE_TX_TIMER_USEC_AGGRESSIVE_TIME;
			break;
		case NVM_CFG1_PORT_EEE_POWER_SAVING_MODE_LOW_LATENCY:
			p_caps->eee_lpi_timer = EEE_TX_TIMER_USEC_LATENCY_TIME;
			break;
		}

		link->eee.tx_lpi_timer = p_caps->eee_lpi_timer;
		link->eee.tx_lpi_enable = link->eee.enable;
		link->eee.adv_caps = QED_EEE_1G_ADV | QED_EEE_10G_ADV;
	} else {
		p_caps->default_eee = QED_MCP_EEE_UNSUPPORTED;
	}

	if (p_hwfn->mcp_info->capabilities &
	    FW_MB_PARAM_FEATURE_SUPPORT_EXT_SPEED_FEC_CONTROL) {
		ext_speed = &link->ext_speed;

		link_temp = qed_rd(p_hwfn, p_ptt,
				   port_cfg_addr +
				   offsetof(struct nvm_cfg1_port,
					    extended_speed));

		fld = GET_MFW_FIELD(link_temp, NVM_CFG1_PORT_EXTENDED_SPEED);
		if (fld & NVM_CFG1_PORT_EXTENDED_SPEED_EXTND_SPD_AN)
			ext_speed->autoneg = true;

		ext_speed->forced_speed = 0;
		if (fld & NVM_CFG1_PORT_EXTENDED_SPEED_EXTND_SPD_1G)
			ext_speed->forced_speed |= QED_EXT_SPEED_1G;
		if (fld & NVM_CFG1_PORT_EXTENDED_SPEED_EXTND_SPD_10G)
			ext_speed->forced_speed |= QED_EXT_SPEED_10G;
		if (fld & NVM_CFG1_PORT_EXTENDED_SPEED_EXTND_SPD_20G)
			ext_speed->forced_speed |= QED_EXT_SPEED_20G;
		if (fld & NVM_CFG1_PORT_EXTENDED_SPEED_EXTND_SPD_25G)
			ext_speed->forced_speed |= QED_EXT_SPEED_25G;
		if (fld & NVM_CFG1_PORT_EXTENDED_SPEED_EXTND_SPD_40G)
			ext_speed->forced_speed |= QED_EXT_SPEED_40G;
		if (fld & NVM_CFG1_PORT_EXTENDED_SPEED_EXTND_SPD_50G_R)
			ext_speed->forced_speed |= QED_EXT_SPEED_50G_R;
		if (fld & NVM_CFG1_PORT_EXTENDED_SPEED_EXTND_SPD_50G_R2)
			ext_speed->forced_speed |= QED_EXT_SPEED_50G_R2;
		if (fld & NVM_CFG1_PORT_EXTENDED_SPEED_EXTND_SPD_100G_R2)
			ext_speed->forced_speed |= QED_EXT_SPEED_100G_R2;
		if (fld & NVM_CFG1_PORT_EXTENDED_SPEED_EXTND_SPD_100G_R4)
			ext_speed->forced_speed |= QED_EXT_SPEED_100G_R4;
		if (fld & NVM_CFG1_PORT_EXTENDED_SPEED_EXTND_SPD_100G_P4)
			ext_speed->forced_speed |= QED_EXT_SPEED_100G_P4;

		fld = GET_MFW_FIELD(link_temp,
				    NVM_CFG1_PORT_EXTENDED_SPEED_CAP);

		ext_speed->advertised_speeds = 0;
		if (fld & NVM_CFG1_PORT_EXTENDED_SPEED_CAP_EXTND_SPD_RESERVED)
			ext_speed->advertised_speeds |= QED_EXT_SPEED_MASK_RES;
		if (fld & NVM_CFG1_PORT_EXTENDED_SPEED_CAP_EXTND_SPD_1G)
			ext_speed->advertised_speeds |= QED_EXT_SPEED_MASK_1G;
		if (fld & NVM_CFG1_PORT_EXTENDED_SPEED_CAP_EXTND_SPD_10G)
			ext_speed->advertised_speeds |= QED_EXT_SPEED_MASK_10G;
		if (fld & NVM_CFG1_PORT_EXTENDED_SPEED_CAP_EXTND_SPD_20G)
			ext_speed->advertised_speeds |= QED_EXT_SPEED_MASK_20G;
		if (fld & NVM_CFG1_PORT_EXTENDED_SPEED_CAP_EXTND_SPD_25G)
			ext_speed->advertised_speeds |= QED_EXT_SPEED_MASK_25G;
		if (fld & NVM_CFG1_PORT_EXTENDED_SPEED_CAP_EXTND_SPD_40G)
			ext_speed->advertised_speeds |= QED_EXT_SPEED_MASK_40G;
		if (fld & NVM_CFG1_PORT_EXTENDED_SPEED_CAP_EXTND_SPD_50G_R)
			ext_speed->advertised_speeds |=
				QED_EXT_SPEED_MASK_50G_R;
		if (fld & NVM_CFG1_PORT_EXTENDED_SPEED_CAP_EXTND_SPD_50G_R2)
			ext_speed->advertised_speeds |=
				QED_EXT_SPEED_MASK_50G_R2;
		if (fld & NVM_CFG1_PORT_EXTENDED_SPEED_CAP_EXTND_SPD_100G_R2)
			ext_speed->advertised_speeds |=
				QED_EXT_SPEED_MASK_100G_R2;
		if (fld & NVM_CFG1_PORT_EXTENDED_SPEED_CAP_EXTND_SPD_100G_R4)
			ext_speed->advertised_speeds |=
				QED_EXT_SPEED_MASK_100G_R4;
		if (fld & NVM_CFG1_PORT_EXTENDED_SPEED_CAP_EXTND_SPD_100G_P4)
			ext_speed->advertised_speeds |=
				QED_EXT_SPEED_MASK_100G_P4;

		link_temp = qed_rd(p_hwfn, p_ptt,
				   port_cfg_addr +
				   offsetof(struct nvm_cfg1_port,
					    extended_fec_mode));
		link->ext_fec_mode = link_temp;

		p_caps->default_ext_speed_caps = ext_speed->advertised_speeds;
		p_caps->default_ext_speed = ext_speed->forced_speed;
		p_caps->default_ext_autoneg = ext_speed->autoneg;
		p_caps->default_ext_fec = link->ext_fec_mode;

		DP_VERBOSE(p_hwfn, NETIF_MSG_LINK,
			   "Read default extended link config: Speed 0x%08x, Adv. Speed 0x%08x, AN: 0x%02x, FEC: 0x%02x\n",
			   ext_speed->forced_speed,
			   ext_speed->advertised_speeds, ext_speed->autoneg,
			   p_caps->default_ext_fec);
	}

	DP_VERBOSE(p_hwfn, NETIF_MSG_LINK,
		   "Read default link: Speed 0x%08x, Adv. Speed 0x%08x, AN: 0x%02x, PAUSE AN: 0x%02x, EEE: 0x%02x [0x%08x usec], FEC: 0x%02x\n",
		   link->speed.forced_speed, link->speed.advertised_speeds,
		   link->speed.autoneg, link->pause.autoneg,
		   p_caps->default_eee, p_caps->eee_lpi_timer,
		   p_caps->fec_default);

	if (IS_LEAD_HWFN(p_hwfn)) {
		struct qed_dev *cdev = p_hwfn->cdev;

		 
		addr = MCP_REG_SCRATCH + nvm_cfg1_offset +
		       offsetof(struct nvm_cfg1, glob) +
		       offsetof(struct nvm_cfg1_glob, generic_cont0);

		generic_cont0 = qed_rd(p_hwfn, p_ptt, addr);

		mf_mode = (generic_cont0 & NVM_CFG1_GLOB_MF_MODE_MASK) >>
			  NVM_CFG1_GLOB_MF_MODE_OFFSET;

		switch (mf_mode) {
		case NVM_CFG1_GLOB_MF_MODE_MF_ALLOWED:
			cdev->mf_bits = BIT(QED_MF_OVLAN_CLSS);
			break;
		case NVM_CFG1_GLOB_MF_MODE_UFP:
			cdev->mf_bits = BIT(QED_MF_OVLAN_CLSS) |
					BIT(QED_MF_LLH_PROTO_CLSS) |
					BIT(QED_MF_UFP_SPECIFIC) |
					BIT(QED_MF_8021Q_TAGGING) |
					BIT(QED_MF_DONT_ADD_VLAN0_TAG);
			break;
		case NVM_CFG1_GLOB_MF_MODE_BD:
			cdev->mf_bits = BIT(QED_MF_OVLAN_CLSS) |
					BIT(QED_MF_LLH_PROTO_CLSS) |
					BIT(QED_MF_8021AD_TAGGING) |
					BIT(QED_MF_DONT_ADD_VLAN0_TAG);
			break;
		case NVM_CFG1_GLOB_MF_MODE_NPAR1_0:
			cdev->mf_bits = BIT(QED_MF_LLH_MAC_CLSS) |
					BIT(QED_MF_LLH_PROTO_CLSS) |
					BIT(QED_MF_LL2_NON_UNICAST) |
					BIT(QED_MF_INTER_PF_SWITCH) |
					BIT(QED_MF_DISABLE_ARFS);
			break;
		case NVM_CFG1_GLOB_MF_MODE_DEFAULT:
			cdev->mf_bits = BIT(QED_MF_LLH_MAC_CLSS) |
					BIT(QED_MF_LLH_PROTO_CLSS) |
					BIT(QED_MF_LL2_NON_UNICAST);
			if (QED_IS_BB(p_hwfn->cdev))
				cdev->mf_bits |= BIT(QED_MF_NEED_DEF_PF);
			break;
		}

		DP_INFO(p_hwfn, "Multi function mode is 0x%lx\n",
			cdev->mf_bits);

		 
		if (QED_IS_CMT(cdev))
			cdev->mf_bits |= BIT(QED_MF_DISABLE_ARFS);
	}

	DP_INFO(p_hwfn, "Multi function mode is 0x%lx\n",
		p_hwfn->cdev->mf_bits);

	 
	addr = MCP_REG_SCRATCH + nvm_cfg1_offset +
		offsetof(struct nvm_cfg1, glob) +
		offsetof(struct nvm_cfg1_glob, device_capabilities);

	device_capabilities = qed_rd(p_hwfn, p_ptt, addr);
	if (device_capabilities & NVM_CFG1_GLOB_DEVICE_CAPABILITIES_ETHERNET)
		__set_bit(QED_DEV_CAP_ETH,
			  &p_hwfn->hw_info.device_capabilities);
	if (device_capabilities & NVM_CFG1_GLOB_DEVICE_CAPABILITIES_FCOE)
		__set_bit(QED_DEV_CAP_FCOE,
			  &p_hwfn->hw_info.device_capabilities);
	if (device_capabilities & NVM_CFG1_GLOB_DEVICE_CAPABILITIES_ISCSI)
		__set_bit(QED_DEV_CAP_ISCSI,
			  &p_hwfn->hw_info.device_capabilities);
	if (device_capabilities & NVM_CFG1_GLOB_DEVICE_CAPABILITIES_ROCE)
		__set_bit(QED_DEV_CAP_ROCE,
			  &p_hwfn->hw_info.device_capabilities);

	 
	addr = MCP_REG_SCRATCH + nvm_cfg1_offset +
		offsetof(struct nvm_cfg1, glob) +
		offsetof(struct nvm_cfg1_glob, serial_number);

	for (i = 0; i < 4; i++)
		p_hwfn->hw_info.part_num[i] = qed_rd(p_hwfn, p_ptt, addr + i * 4);

	return qed_mcp_fill_shmem_func_info(p_hwfn, p_ptt);
}

static void qed_get_num_funcs(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt)
{
	u8 num_funcs, enabled_func_idx = p_hwfn->rel_pf_id;
	u32 reg_function_hide, tmp, eng_mask, low_pfs_mask;
	struct qed_dev *cdev = p_hwfn->cdev;

	num_funcs = QED_IS_AH(cdev) ? MAX_NUM_PFS_K2 : MAX_NUM_PFS_BB;

	 
	reg_function_hide = qed_rd(p_hwfn, p_ptt, MISCS_REG_FUNCTION_HIDE);

	if (reg_function_hide & 0x1) {
		if (QED_IS_BB(cdev)) {
			if (QED_PATH_ID(p_hwfn) && cdev->num_hwfns == 1) {
				num_funcs = 0;
				eng_mask = 0xaaaa;
			} else {
				num_funcs = 1;
				eng_mask = 0x5554;
			}
		} else {
			num_funcs = 1;
			eng_mask = 0xfffe;
		}

		 
		tmp = (reg_function_hide ^ 0xffffffff) & eng_mask;
		while (tmp) {
			if (tmp & 0x1)
				num_funcs++;
			tmp >>= 0x1;
		}

		 
		low_pfs_mask = (0x1 << p_hwfn->abs_pf_id) - 1;
		tmp = reg_function_hide & eng_mask & low_pfs_mask;
		while (tmp) {
			if (tmp & 0x1)
				enabled_func_idx--;
			tmp >>= 0x1;
		}
	}

	p_hwfn->num_funcs_on_engine = num_funcs;
	p_hwfn->enabled_func_idx = enabled_func_idx;

	DP_VERBOSE(p_hwfn,
		   NETIF_MSG_PROBE,
		   "PF [rel_id %d, abs_id %d] occupies index %d within the %d enabled functions on the engine\n",
		   p_hwfn->rel_pf_id,
		   p_hwfn->abs_pf_id,
		   p_hwfn->enabled_func_idx, p_hwfn->num_funcs_on_engine);
}

static void qed_hw_info_port_num(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt)
{
	u32 addr, global_offsize, global_addr, port_mode;
	struct qed_dev *cdev = p_hwfn->cdev;

	 
	if (cdev->num_hwfns > 1) {
		cdev->num_ports_in_engine = 1;
		cdev->num_ports = 1;
		return;
	}

	 
	port_mode = qed_rd(p_hwfn, p_ptt, MISC_REG_PORT_MODE);
	switch (port_mode) {
	case 0x0:
		cdev->num_ports_in_engine = 1;
		break;
	case 0x1:
		cdev->num_ports_in_engine = 2;
		break;
	case 0x2:
		cdev->num_ports_in_engine = 4;
		break;
	default:
		DP_NOTICE(p_hwfn, "Unknown port mode 0x%08x\n", port_mode);
		cdev->num_ports_in_engine = 1;	 
		break;
	}

	 
	addr = SECTION_OFFSIZE_ADDR(p_hwfn->mcp_info->public_base,
				    PUBLIC_GLOBAL);
	global_offsize = qed_rd(p_hwfn, p_ptt, addr);
	global_addr = SECTION_ADDR(global_offsize, 0);
	addr = global_addr + offsetof(struct public_global, max_ports);
	cdev->num_ports = (u8)qed_rd(p_hwfn, p_ptt, addr);
}

static void qed_get_eee_caps(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt)
{
	struct qed_mcp_link_capabilities *p_caps;
	u32 eee_status;

	p_caps = &p_hwfn->mcp_info->link_capabilities;
	if (p_caps->default_eee == QED_MCP_EEE_UNSUPPORTED)
		return;

	p_caps->eee_speed_caps = 0;
	eee_status = qed_rd(p_hwfn, p_ptt, p_hwfn->mcp_info->port_addr +
			    offsetof(struct public_port, eee_status));
	eee_status = (eee_status & EEE_SUPPORTED_SPEED_MASK) >>
			EEE_SUPPORTED_SPEED_OFFSET;

	if (eee_status & EEE_1G_SUPPORTED)
		p_caps->eee_speed_caps |= QED_EEE_1G_ADV;
	if (eee_status & EEE_10G_ADV)
		p_caps->eee_speed_caps |= QED_EEE_10G_ADV;
}

static int
qed_get_hw_info(struct qed_hwfn *p_hwfn,
		struct qed_ptt *p_ptt,
		enum qed_pci_personality personality)
{
	int rc;

	 
	if (IS_LEAD_HWFN(p_hwfn)) {
		rc = qed_iov_hw_info(p_hwfn);
		if (rc)
			return rc;
	}

	if (IS_LEAD_HWFN(p_hwfn))
		qed_hw_info_port_num(p_hwfn, p_ptt);

	qed_mcp_get_capabilities(p_hwfn, p_ptt);

	qed_hw_get_nvm_info(p_hwfn, p_ptt);

	rc = qed_int_igu_read_cam(p_hwfn, p_ptt);
	if (rc)
		return rc;

	if (qed_mcp_is_init(p_hwfn))
		ether_addr_copy(p_hwfn->hw_info.hw_mac_addr,
				p_hwfn->mcp_info->func_info.mac);
	else
		eth_random_addr(p_hwfn->hw_info.hw_mac_addr);

	if (qed_mcp_is_init(p_hwfn)) {
		if (p_hwfn->mcp_info->func_info.ovlan != QED_MCP_VLAN_UNSET)
			p_hwfn->hw_info.ovlan =
				p_hwfn->mcp_info->func_info.ovlan;

		qed_mcp_cmd_port_init(p_hwfn, p_ptt);

		qed_get_eee_caps(p_hwfn, p_ptt);

		qed_mcp_read_ufp_config(p_hwfn, p_ptt);
	}

	if (qed_mcp_is_init(p_hwfn)) {
		enum qed_pci_personality protocol;

		protocol = p_hwfn->mcp_info->func_info.protocol;
		p_hwfn->hw_info.personality = protocol;
	}

	if (QED_IS_ROCE_PERSONALITY(p_hwfn))
		p_hwfn->hw_info.multi_tc_roce_en = true;

	p_hwfn->hw_info.num_hw_tc = NUM_PHYS_TCS_4PORT_K2;
	p_hwfn->hw_info.num_active_tc = 1;

	qed_get_num_funcs(p_hwfn, p_ptt);

	if (qed_mcp_is_init(p_hwfn))
		p_hwfn->hw_info.mtu = p_hwfn->mcp_info->func_info.mtu;

	return qed_hw_get_resc(p_hwfn, p_ptt);
}

static int qed_get_dev_info(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt)
{
	struct qed_dev *cdev = p_hwfn->cdev;
	u16 device_id_mask;
	u32 tmp;

	 
	pci_read_config_word(cdev->pdev, PCI_VENDOR_ID, &cdev->vendor_id);
	pci_read_config_word(cdev->pdev, PCI_DEVICE_ID, &cdev->device_id);

	 
	device_id_mask = cdev->device_id & QED_DEV_ID_MASK;
	switch (device_id_mask) {
	case QED_DEV_ID_MASK_BB:
		cdev->type = QED_DEV_TYPE_BB;
		break;
	case QED_DEV_ID_MASK_AH:
		cdev->type = QED_DEV_TYPE_AH;
		break;
	default:
		DP_NOTICE(p_hwfn, "Unknown device id 0x%x\n", cdev->device_id);
		return -EBUSY;
	}

	cdev->chip_num = (u16)qed_rd(p_hwfn, p_ptt, MISCS_REG_CHIP_NUM);
	cdev->chip_rev = (u16)qed_rd(p_hwfn, p_ptt, MISCS_REG_CHIP_REV);

	MASK_FIELD(CHIP_REV, cdev->chip_rev);

	 
	tmp = qed_rd(p_hwfn, p_ptt, MISCS_REG_CMT_ENABLED_FOR_PAIR);

	if (tmp & (1 << p_hwfn->rel_pf_id)) {
		DP_NOTICE(cdev->hwfns, "device in CMT mode\n");
		cdev->num_hwfns = 2;
	} else {
		cdev->num_hwfns = 1;
	}

	cdev->chip_bond_id = qed_rd(p_hwfn, p_ptt,
				    MISCS_REG_CHIP_TEST_REG) >> 4;
	MASK_FIELD(CHIP_BOND_ID, cdev->chip_bond_id);
	cdev->chip_metal = (u16)qed_rd(p_hwfn, p_ptt, MISCS_REG_CHIP_METAL);
	MASK_FIELD(CHIP_METAL, cdev->chip_metal);

	DP_INFO(cdev->hwfns,
		"Chip details - %s %c%d, Num: %04x Rev: %04x Bond id: %04x Metal: %04x\n",
		QED_IS_BB(cdev) ? "BB" : "AH",
		'A' + cdev->chip_rev,
		(int)cdev->chip_metal,
		cdev->chip_num, cdev->chip_rev,
		cdev->chip_bond_id, cdev->chip_metal);

	return 0;
}

static int qed_hw_prepare_single(struct qed_hwfn *p_hwfn,
				 void __iomem *p_regview,
				 void __iomem *p_doorbells,
				 u64 db_phys_addr,
				 enum qed_pci_personality personality)
{
	struct qed_dev *cdev = p_hwfn->cdev;
	int rc = 0;

	 
	p_hwfn->regview = p_regview;
	p_hwfn->doorbells = p_doorbells;
	p_hwfn->db_phys_addr = db_phys_addr;

	if (IS_VF(p_hwfn->cdev))
		return qed_vf_hw_prepare(p_hwfn);

	 
	if (REG_RD(p_hwfn, PXP_PF_ME_OPAQUE_ADDR) == 0xffffffff) {
		DP_ERR(p_hwfn,
		       "Reading the ME register returns all Fs; Preventing further chip access\n");
		return -EINVAL;
	}

	get_function_id(p_hwfn);

	 
	rc = qed_ptt_pool_alloc(p_hwfn);
	if (rc)
		goto err0;

	 
	p_hwfn->p_main_ptt = qed_get_reserved_ptt(p_hwfn, RESERVED_PTT_MAIN);

	 
	if (!p_hwfn->my_id) {
		rc = qed_get_dev_info(p_hwfn, p_hwfn->p_main_ptt);
		if (rc)
			goto err1;
	}

	qed_hw_hwfn_prepare(p_hwfn);

	 
	rc = qed_mcp_cmd_init(p_hwfn, p_hwfn->p_main_ptt);
	if (rc) {
		DP_NOTICE(p_hwfn, "Failed initializing mcp command\n");
		goto err1;
	}

	 
	rc = qed_get_hw_info(p_hwfn, p_hwfn->p_main_ptt, personality);
	if (rc) {
		DP_NOTICE(p_hwfn, "Failed to get HW information\n");
		goto err2;
	}

	 
	if (IS_LEAD_HWFN(p_hwfn) && !cdev->recov_in_prog) {
		rc = qed_mcp_initiate_pf_flr(p_hwfn, p_hwfn->p_main_ptt);
		if (rc)
			DP_NOTICE(p_hwfn, "Failed to initiate PF FLR\n");
	}

	 
	if (IS_LEAD_HWFN(p_hwfn)) {
		rc = qed_mcp_nvm_info_populate(p_hwfn);
		if (rc) {
			DP_NOTICE(p_hwfn,
				  "Failed to populate nvm info shadow\n");
			goto err2;
		}
	}

	 
	rc = qed_init_alloc(p_hwfn);
	if (rc)
		goto err3;

	return rc;
err3:
	if (IS_LEAD_HWFN(p_hwfn))
		qed_mcp_nvm_info_free(p_hwfn);
err2:
	if (IS_LEAD_HWFN(p_hwfn))
		qed_iov_free_hw_info(p_hwfn->cdev);
	qed_mcp_free(p_hwfn);
err1:
	qed_hw_hwfn_free(p_hwfn);
err0:
	return rc;
}

int qed_hw_prepare(struct qed_dev *cdev,
		   int personality)
{
	struct qed_hwfn *p_hwfn = QED_LEADING_HWFN(cdev);
	int rc;

	 
	if (IS_PF(cdev))
		qed_init_iro_array(cdev);

	 
	rc = qed_hw_prepare_single(p_hwfn,
				   cdev->regview,
				   cdev->doorbells,
				   cdev->db_phys_addr,
				   personality);
	if (rc)
		return rc;

	personality = p_hwfn->hw_info.personality;

	 
	if (cdev->num_hwfns > 1) {
		void __iomem *p_regview, *p_doorbell;
		u64 db_phys_addr;
		u32 offset;

		 
		offset = qed_hw_bar_size(p_hwfn, p_hwfn->p_main_ptt,
					 BAR_ID_0) / 2;
		p_regview = cdev->regview + offset;

		offset = qed_hw_bar_size(p_hwfn, p_hwfn->p_main_ptt,
					 BAR_ID_1) / 2;

		p_doorbell = cdev->doorbells + offset;

		db_phys_addr = cdev->db_phys_addr + offset;

		 
		rc = qed_hw_prepare_single(&cdev->hwfns[1], p_regview,
					   p_doorbell, db_phys_addr,
					   personality);

		 
		if (rc) {
			if (IS_PF(cdev)) {
				qed_init_free(p_hwfn);
				qed_mcp_nvm_info_free(p_hwfn);
				qed_mcp_free(p_hwfn);
				qed_hw_hwfn_free(p_hwfn);
			}
		}
	}

	return rc;
}

void qed_hw_remove(struct qed_dev *cdev)
{
	struct qed_hwfn *p_hwfn = QED_LEADING_HWFN(cdev);
	int i;

	if (IS_PF(cdev))
		qed_mcp_ov_update_driver_state(p_hwfn, p_hwfn->p_main_ptt,
					       QED_OV_DRIVER_STATE_NOT_LOADED);

	for_each_hwfn(cdev, i) {
		struct qed_hwfn *p_hwfn = &cdev->hwfns[i];

		if (IS_VF(cdev)) {
			qed_vf_pf_release(p_hwfn);
			continue;
		}

		qed_init_free(p_hwfn);
		qed_hw_hwfn_free(p_hwfn);
		qed_mcp_free(p_hwfn);
	}

	qed_iov_free_hw_info(cdev);

	qed_mcp_nvm_info_free(p_hwfn);
}

int qed_fw_l2_queue(struct qed_hwfn *p_hwfn, u16 src_id, u16 *dst_id)
{
	if (src_id >= RESC_NUM(p_hwfn, QED_L2_QUEUE)) {
		u16 min, max;

		min = (u16)RESC_START(p_hwfn, QED_L2_QUEUE);
		max = min + RESC_NUM(p_hwfn, QED_L2_QUEUE);
		DP_NOTICE(p_hwfn,
			  "l2_queue id [%d] is not valid, available indices [%d - %d]\n",
			  src_id, min, max);

		return -EINVAL;
	}

	*dst_id = RESC_START(p_hwfn, QED_L2_QUEUE) + src_id;

	return 0;
}

int qed_fw_vport(struct qed_hwfn *p_hwfn, u8 src_id, u8 *dst_id)
{
	if (src_id >= RESC_NUM(p_hwfn, QED_VPORT)) {
		u8 min, max;

		min = (u8)RESC_START(p_hwfn, QED_VPORT);
		max = min + RESC_NUM(p_hwfn, QED_VPORT);
		DP_NOTICE(p_hwfn,
			  "vport id [%d] is not valid, available indices [%d - %d]\n",
			  src_id, min, max);

		return -EINVAL;
	}

	*dst_id = RESC_START(p_hwfn, QED_VPORT) + src_id;

	return 0;
}

int qed_fw_rss_eng(struct qed_hwfn *p_hwfn, u8 src_id, u8 *dst_id)
{
	if (src_id >= RESC_NUM(p_hwfn, QED_RSS_ENG)) {
		u8 min, max;

		min = (u8)RESC_START(p_hwfn, QED_RSS_ENG);
		max = min + RESC_NUM(p_hwfn, QED_RSS_ENG);
		DP_NOTICE(p_hwfn,
			  "rss_eng id [%d] is not valid, available indices [%d - %d]\n",
			  src_id, min, max);

		return -EINVAL;
	}

	*dst_id = RESC_START(p_hwfn, QED_RSS_ENG) + src_id;

	return 0;
}

static int qed_set_coalesce(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt,
			    u32 hw_addr, void *p_eth_qzone,
			    size_t eth_qzone_size, u8 timeset)
{
	struct coalescing_timeset *p_coal_timeset;

	if (p_hwfn->cdev->int_coalescing_mode != QED_COAL_MODE_ENABLE) {
		DP_NOTICE(p_hwfn, "Coalescing configuration not enabled\n");
		return -EINVAL;
	}

	p_coal_timeset = p_eth_qzone;
	memset(p_eth_qzone, 0, eth_qzone_size);
	SET_FIELD(p_coal_timeset->value, COALESCING_TIMESET_TIMESET, timeset);
	SET_FIELD(p_coal_timeset->value, COALESCING_TIMESET_VALID, 1);
	qed_memcpy_to(p_hwfn, p_ptt, hw_addr, p_eth_qzone, eth_qzone_size);

	return 0;
}

int qed_set_queue_coalesce(u16 rx_coal, u16 tx_coal, void *p_handle)
{
	struct qed_queue_cid *p_cid = p_handle;
	struct qed_hwfn *p_hwfn;
	struct qed_ptt *p_ptt;
	int rc = 0;

	p_hwfn = p_cid->p_owner;

	if (IS_VF(p_hwfn->cdev))
		return qed_vf_pf_set_coalesce(p_hwfn, rx_coal, tx_coal, p_cid);

	p_ptt = qed_ptt_acquire(p_hwfn);
	if (!p_ptt)
		return -EAGAIN;

	if (rx_coal) {
		rc = qed_set_rxq_coalesce(p_hwfn, p_ptt, rx_coal, p_cid);
		if (rc)
			goto out;
		p_hwfn->cdev->rx_coalesce_usecs = rx_coal;
	}

	if (tx_coal) {
		rc = qed_set_txq_coalesce(p_hwfn, p_ptt, tx_coal, p_cid);
		if (rc)
			goto out;
		p_hwfn->cdev->tx_coalesce_usecs = tx_coal;
	}
out:
	qed_ptt_release(p_hwfn, p_ptt);
	return rc;
}

int qed_set_rxq_coalesce(struct qed_hwfn *p_hwfn,
			 struct qed_ptt *p_ptt,
			 u16 coalesce, struct qed_queue_cid *p_cid)
{
	struct ustorm_eth_queue_zone eth_qzone;
	u8 timeset, timer_res;
	u32 address;
	int rc;

	 
	if (coalesce <= 0x7F) {
		timer_res = 0;
	} else if (coalesce <= 0xFF) {
		timer_res = 1;
	} else if (coalesce <= 0x1FF) {
		timer_res = 2;
	} else {
		DP_ERR(p_hwfn, "Invalid coalesce value - %d\n", coalesce);
		return -EINVAL;
	}
	timeset = (u8)(coalesce >> timer_res);

	rc = qed_int_set_timer_res(p_hwfn, p_ptt, timer_res,
				   p_cid->sb_igu_id, false);
	if (rc)
		goto out;

	address = BAR0_MAP_REG_USDM_RAM +
		  USTORM_ETH_QUEUE_ZONE_GTT_OFFSET(p_cid->abs.queue_id);

	rc = qed_set_coalesce(p_hwfn, p_ptt, address, &eth_qzone,
			      sizeof(struct ustorm_eth_queue_zone), timeset);
	if (rc)
		goto out;

out:
	return rc;
}

int qed_set_txq_coalesce(struct qed_hwfn *p_hwfn,
			 struct qed_ptt *p_ptt,
			 u16 coalesce, struct qed_queue_cid *p_cid)
{
	struct xstorm_eth_queue_zone eth_qzone;
	u8 timeset, timer_res;
	u32 address;
	int rc;

	 
	if (coalesce <= 0x7F) {
		timer_res = 0;
	} else if (coalesce <= 0xFF) {
		timer_res = 1;
	} else if (coalesce <= 0x1FF) {
		timer_res = 2;
	} else {
		DP_ERR(p_hwfn, "Invalid coalesce value - %d\n", coalesce);
		return -EINVAL;
	}
	timeset = (u8)(coalesce >> timer_res);

	rc = qed_int_set_timer_res(p_hwfn, p_ptt, timer_res,
				   p_cid->sb_igu_id, true);
	if (rc)
		goto out;

	address = BAR0_MAP_REG_XSDM_RAM +
		  XSTORM_ETH_QUEUE_ZONE_GTT_OFFSET(p_cid->abs.queue_id);

	rc = qed_set_coalesce(p_hwfn, p_ptt, address, &eth_qzone,
			      sizeof(struct xstorm_eth_queue_zone), timeset);
out:
	return rc;
}

 
static void qed_configure_wfq_for_all_vports(struct qed_hwfn *p_hwfn,
					     struct qed_ptt *p_ptt,
					     u32 min_pf_rate)
{
	struct init_qm_vport_params *vport_params;
	int i;

	vport_params = p_hwfn->qm_info.qm_vport_params;

	for (i = 0; i < p_hwfn->qm_info.num_vports; i++) {
		u32 wfq_speed = p_hwfn->qm_info.wfq_data[i].min_speed;

		vport_params[i].wfq = (wfq_speed * QED_WFQ_UNIT) /
						min_pf_rate;
		qed_init_vport_wfq(p_hwfn, p_ptt,
				   vport_params[i].first_tx_pq_id,
				   vport_params[i].wfq);
	}
}

static void qed_init_wfq_default_param(struct qed_hwfn *p_hwfn,
				       u32 min_pf_rate)

{
	int i;

	for (i = 0; i < p_hwfn->qm_info.num_vports; i++)
		p_hwfn->qm_info.qm_vport_params[i].wfq = 1;
}

static void qed_disable_wfq_for_all_vports(struct qed_hwfn *p_hwfn,
					   struct qed_ptt *p_ptt,
					   u32 min_pf_rate)
{
	struct init_qm_vport_params *vport_params;
	int i;

	vport_params = p_hwfn->qm_info.qm_vport_params;

	for (i = 0; i < p_hwfn->qm_info.num_vports; i++) {
		qed_init_wfq_default_param(p_hwfn, min_pf_rate);
		qed_init_vport_wfq(p_hwfn, p_ptt,
				   vport_params[i].first_tx_pq_id,
				   vport_params[i].wfq);
	}
}

 
static int qed_init_wfq_param(struct qed_hwfn *p_hwfn,
			      u16 vport_id, u32 req_rate, u32 min_pf_rate)
{
	u32 total_req_min_rate = 0, total_left_rate = 0, left_rate_per_vp = 0;
	int non_requested_count = 0, req_count = 0, i, num_vports;

	num_vports = p_hwfn->qm_info.num_vports;

	if (num_vports < 2) {
		DP_NOTICE(p_hwfn, "Unexpected num_vports: %d\n", num_vports);
		return -EINVAL;
	}

	 
	for (i = 0; i < num_vports; i++) {
		u32 tmp_speed;

		if ((i != vport_id) &&
		    p_hwfn->qm_info.wfq_data[i].configured) {
			req_count++;
			tmp_speed = p_hwfn->qm_info.wfq_data[i].min_speed;
			total_req_min_rate += tmp_speed;
		}
	}

	 
	req_count++;
	total_req_min_rate += req_rate;
	non_requested_count = num_vports - req_count;

	if (req_rate < min_pf_rate / QED_WFQ_UNIT) {
		DP_VERBOSE(p_hwfn, NETIF_MSG_LINK,
			   "Vport [%d] - Requested rate[%d Mbps] is less than one percent of configured PF min rate[%d Mbps]\n",
			   vport_id, req_rate, min_pf_rate);
		return -EINVAL;
	}

	if (num_vports > QED_WFQ_UNIT) {
		DP_VERBOSE(p_hwfn, NETIF_MSG_LINK,
			   "Number of vports is greater than %d\n",
			   QED_WFQ_UNIT);
		return -EINVAL;
	}

	if (total_req_min_rate > min_pf_rate) {
		DP_VERBOSE(p_hwfn, NETIF_MSG_LINK,
			   "Total requested min rate for all vports[%d Mbps] is greater than configured PF min rate[%d Mbps]\n",
			   total_req_min_rate, min_pf_rate);
		return -EINVAL;
	}

	total_left_rate	= min_pf_rate - total_req_min_rate;

	left_rate_per_vp = total_left_rate / non_requested_count;
	if (left_rate_per_vp <  min_pf_rate / QED_WFQ_UNIT) {
		DP_VERBOSE(p_hwfn, NETIF_MSG_LINK,
			   "Non WFQ configured vports rate [%d Mbps] is less than one percent of configured PF min rate[%d Mbps]\n",
			   left_rate_per_vp, min_pf_rate);
		return -EINVAL;
	}

	p_hwfn->qm_info.wfq_data[vport_id].min_speed = req_rate;
	p_hwfn->qm_info.wfq_data[vport_id].configured = true;

	for (i = 0; i < num_vports; i++) {
		if (p_hwfn->qm_info.wfq_data[i].configured)
			continue;

		p_hwfn->qm_info.wfq_data[i].min_speed = left_rate_per_vp;
	}

	return 0;
}

static int __qed_configure_vport_wfq(struct qed_hwfn *p_hwfn,
				     struct qed_ptt *p_ptt, u16 vp_id, u32 rate)
{
	struct qed_mcp_link_state *p_link;
	int rc = 0;

	p_link = &p_hwfn->cdev->hwfns[0].mcp_info->link_output;

	if (!p_link->min_pf_rate) {
		p_hwfn->qm_info.wfq_data[vp_id].min_speed = rate;
		p_hwfn->qm_info.wfq_data[vp_id].configured = true;
		return rc;
	}

	rc = qed_init_wfq_param(p_hwfn, vp_id, rate, p_link->min_pf_rate);

	if (!rc)
		qed_configure_wfq_for_all_vports(p_hwfn, p_ptt,
						 p_link->min_pf_rate);
	else
		DP_NOTICE(p_hwfn,
			  "Validation failed while configuring min rate\n");

	return rc;
}

static int __qed_configure_vp_wfq_on_link_change(struct qed_hwfn *p_hwfn,
						 struct qed_ptt *p_ptt,
						 u32 min_pf_rate)
{
	bool use_wfq = false;
	int rc = 0;
	u16 i;

	 
	for (i = 0; i < p_hwfn->qm_info.num_vports; i++) {
		u32 rate;

		if (!p_hwfn->qm_info.wfq_data[i].configured)
			continue;

		rate = p_hwfn->qm_info.wfq_data[i].min_speed;
		use_wfq = true;

		rc = qed_init_wfq_param(p_hwfn, i, rate, min_pf_rate);
		if (rc) {
			DP_NOTICE(p_hwfn,
				  "WFQ validation failed while configuring min rate\n");
			break;
		}
	}

	if (!rc && use_wfq)
		qed_configure_wfq_for_all_vports(p_hwfn, p_ptt, min_pf_rate);
	else
		qed_disable_wfq_for_all_vports(p_hwfn, p_ptt, min_pf_rate);

	return rc;
}

 
int qed_configure_vport_wfq(struct qed_dev *cdev, u16 vp_id, u32 rate)
{
	int i, rc = -EINVAL;

	 
	if (cdev->num_hwfns > 1) {
		DP_NOTICE(cdev,
			  "WFQ configuration is not supported for this device\n");
		return rc;
	}

	for_each_hwfn(cdev, i) {
		struct qed_hwfn *p_hwfn = &cdev->hwfns[i];
		struct qed_ptt *p_ptt;

		p_ptt = qed_ptt_acquire(p_hwfn);
		if (!p_ptt)
			return -EBUSY;

		rc = __qed_configure_vport_wfq(p_hwfn, p_ptt, vp_id, rate);

		if (rc) {
			qed_ptt_release(p_hwfn, p_ptt);
			return rc;
		}

		qed_ptt_release(p_hwfn, p_ptt);
	}

	return rc;
}

 
void qed_configure_vp_wfq_on_link_change(struct qed_dev *cdev,
					 struct qed_ptt *p_ptt, u32 min_pf_rate)
{
	int i;

	if (cdev->num_hwfns > 1) {
		DP_VERBOSE(cdev,
			   NETIF_MSG_LINK,
			   "WFQ configuration is not supported for this device\n");
		return;
	}

	for_each_hwfn(cdev, i) {
		struct qed_hwfn *p_hwfn = &cdev->hwfns[i];

		__qed_configure_vp_wfq_on_link_change(p_hwfn, p_ptt,
						      min_pf_rate);
	}
}

int __qed_configure_pf_max_bandwidth(struct qed_hwfn *p_hwfn,
				     struct qed_ptt *p_ptt,
				     struct qed_mcp_link_state *p_link,
				     u8 max_bw)
{
	int rc = 0;

	p_hwfn->mcp_info->func_info.bandwidth_max = max_bw;

	if (!p_link->line_speed && (max_bw != 100))
		return rc;

	p_link->speed = (p_link->line_speed * max_bw) / 100;
	p_hwfn->qm_info.pf_rl = p_link->speed;

	 
	if (max_bw == 100)
		p_hwfn->qm_info.pf_rl = 100000;

	rc = qed_init_pf_rl(p_hwfn, p_ptt, p_hwfn->rel_pf_id,
			    p_hwfn->qm_info.pf_rl);

	DP_VERBOSE(p_hwfn, NETIF_MSG_LINK,
		   "Configured MAX bandwidth to be %08x Mb/sec\n",
		   p_link->speed);

	return rc;
}

 
int qed_configure_pf_max_bandwidth(struct qed_dev *cdev, u8 max_bw)
{
	int i, rc = -EINVAL;

	if (max_bw < 1 || max_bw > 100) {
		DP_NOTICE(cdev, "PF max bw valid range is [1-100]\n");
		return rc;
	}

	for_each_hwfn(cdev, i) {
		struct qed_hwfn	*p_hwfn = &cdev->hwfns[i];
		struct qed_hwfn *p_lead = QED_LEADING_HWFN(cdev);
		struct qed_mcp_link_state *p_link;
		struct qed_ptt *p_ptt;

		p_link = &p_lead->mcp_info->link_output;

		p_ptt = qed_ptt_acquire(p_hwfn);
		if (!p_ptt)
			return -EBUSY;

		rc = __qed_configure_pf_max_bandwidth(p_hwfn, p_ptt,
						      p_link, max_bw);

		qed_ptt_release(p_hwfn, p_ptt);

		if (rc)
			break;
	}

	return rc;
}

int __qed_configure_pf_min_bandwidth(struct qed_hwfn *p_hwfn,
				     struct qed_ptt *p_ptt,
				     struct qed_mcp_link_state *p_link,
				     u8 min_bw)
{
	int rc = 0;

	p_hwfn->mcp_info->func_info.bandwidth_min = min_bw;
	p_hwfn->qm_info.pf_wfq = min_bw;

	if (!p_link->line_speed)
		return rc;

	p_link->min_pf_rate = (p_link->line_speed * min_bw) / 100;

	rc = qed_init_pf_wfq(p_hwfn, p_ptt, p_hwfn->rel_pf_id, min_bw);

	DP_VERBOSE(p_hwfn, NETIF_MSG_LINK,
		   "Configured MIN bandwidth to be %d Mb/sec\n",
		   p_link->min_pf_rate);

	return rc;
}

 
int qed_configure_pf_min_bandwidth(struct qed_dev *cdev, u8 min_bw)
{
	int i, rc = -EINVAL;

	if (min_bw < 1 || min_bw > 100) {
		DP_NOTICE(cdev, "PF min bw valid range is [1-100]\n");
		return rc;
	}

	for_each_hwfn(cdev, i) {
		struct qed_hwfn *p_hwfn = &cdev->hwfns[i];
		struct qed_hwfn *p_lead = QED_LEADING_HWFN(cdev);
		struct qed_mcp_link_state *p_link;
		struct qed_ptt *p_ptt;

		p_link = &p_lead->mcp_info->link_output;

		p_ptt = qed_ptt_acquire(p_hwfn);
		if (!p_ptt)
			return -EBUSY;

		rc = __qed_configure_pf_min_bandwidth(p_hwfn, p_ptt,
						      p_link, min_bw);
		if (rc) {
			qed_ptt_release(p_hwfn, p_ptt);
			return rc;
		}

		if (p_link->min_pf_rate) {
			u32 min_rate = p_link->min_pf_rate;

			rc = __qed_configure_vp_wfq_on_link_change(p_hwfn,
								   p_ptt,
								   min_rate);
		}

		qed_ptt_release(p_hwfn, p_ptt);
	}

	return rc;
}

void qed_clean_wfq_db(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt)
{
	struct qed_mcp_link_state *p_link;

	p_link = &p_hwfn->mcp_info->link_output;

	if (p_link->min_pf_rate)
		qed_disable_wfq_for_all_vports(p_hwfn, p_ptt,
					       p_link->min_pf_rate);

	memset(p_hwfn->qm_info.wfq_data, 0,
	       sizeof(*p_hwfn->qm_info.wfq_data) * p_hwfn->qm_info.num_vports);
}

int qed_device_num_ports(struct qed_dev *cdev)
{
	return cdev->num_ports;
}

void qed_set_fw_mac_addr(__le16 *fw_msb,
			 __le16 *fw_mid, __le16 *fw_lsb, u8 *mac)
{
	((u8 *)fw_msb)[0] = mac[1];
	((u8 *)fw_msb)[1] = mac[0];
	((u8 *)fw_mid)[0] = mac[3];
	((u8 *)fw_mid)[1] = mac[2];
	((u8 *)fw_lsb)[0] = mac[5];
	((u8 *)fw_lsb)[1] = mac[4];
}

static int qed_llh_shadow_remove_all_filters(struct qed_dev *cdev, u8 ppfid)
{
	struct qed_llh_info *p_llh_info = cdev->p_llh_info;
	struct qed_llh_filter_info *p_filters;
	int rc;

	rc = qed_llh_shadow_sanity(cdev, ppfid, 0, "remove_all");
	if (rc)
		return rc;

	p_filters = p_llh_info->pp_filters[ppfid];
	memset(p_filters, 0, NIG_REG_LLH_FUNC_FILTER_EN_SIZE *
	       sizeof(*p_filters));

	return 0;
}

static void qed_llh_clear_ppfid_filters(struct qed_dev *cdev, u8 ppfid)
{
	struct qed_hwfn *p_hwfn = QED_LEADING_HWFN(cdev);
	struct qed_ptt *p_ptt = qed_ptt_acquire(p_hwfn);
	u8 filter_idx, abs_ppfid;
	int rc = 0;

	if (!p_ptt)
		return;

	if (!test_bit(QED_MF_LLH_PROTO_CLSS, &cdev->mf_bits) &&
	    !test_bit(QED_MF_LLH_MAC_CLSS, &cdev->mf_bits))
		goto out;

	rc = qed_llh_abs_ppfid(cdev, ppfid, &abs_ppfid);
	if (rc)
		goto out;

	rc = qed_llh_shadow_remove_all_filters(cdev, ppfid);
	if (rc)
		goto out;

	for (filter_idx = 0; filter_idx < NIG_REG_LLH_FUNC_FILTER_EN_SIZE;
	     filter_idx++) {
		rc = qed_llh_remove_filter(p_hwfn, p_ptt,
					   abs_ppfid, filter_idx);
		if (rc)
			goto out;
	}
out:
	qed_ptt_release(p_hwfn, p_ptt);
}

int qed_llh_add_src_tcp_port_filter(struct qed_dev *cdev, u16 src_port)
{
	return qed_llh_add_protocol_filter(cdev, 0,
					   QED_LLH_FILTER_TCP_SRC_PORT,
					   src_port, QED_LLH_DONT_CARE);
}

void qed_llh_remove_src_tcp_port_filter(struct qed_dev *cdev, u16 src_port)
{
	qed_llh_remove_protocol_filter(cdev, 0,
				       QED_LLH_FILTER_TCP_SRC_PORT,
				       src_port, QED_LLH_DONT_CARE);
}

int qed_llh_add_dst_tcp_port_filter(struct qed_dev *cdev, u16 dest_port)
{
	return qed_llh_add_protocol_filter(cdev, 0,
					   QED_LLH_FILTER_TCP_DEST_PORT,
					   QED_LLH_DONT_CARE, dest_port);
}

void qed_llh_remove_dst_tcp_port_filter(struct qed_dev *cdev, u16 dest_port)
{
	qed_llh_remove_protocol_filter(cdev, 0,
				       QED_LLH_FILTER_TCP_DEST_PORT,
				       QED_LLH_DONT_CARE, dest_port);
}

void qed_llh_clear_all_filters(struct qed_dev *cdev)
{
	u8 ppfid;

	if (!test_bit(QED_MF_LLH_PROTO_CLSS, &cdev->mf_bits) &&
	    !test_bit(QED_MF_LLH_MAC_CLSS, &cdev->mf_bits))
		return;

	for (ppfid = 0; ppfid < cdev->p_llh_info->num_ppfid; ppfid++)
		qed_llh_clear_ppfid_filters(cdev, ppfid);
}
