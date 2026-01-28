#include <sys/dbuf.h>
#include <sys/dmu.h>
#include <sys/dmu_impl.h>
#include <sys/dmu_objset.h>
#include <sys/dmu_tx.h>
#include <sys/dnode.h>
#include <sys/zap.h>
#include <sys/zfeature.h>
#include <sys/dsl_dataset.h>
uint_t dmu_object_alloc_chunk_shift = 7;
static uint64_t
dmu_object_alloc_impl(objset_t *os, dmu_object_type_t ot, int blocksize,
    int indirect_blockshift, dmu_object_type_t bonustype, int bonuslen,
    int dnodesize, dnode_t **allocated_dnode, const void *tag, dmu_tx_t *tx)
{
	uint64_t object;
	uint64_t L1_dnode_count = DNODES_PER_BLOCK <<
	    (DMU_META_DNODE(os)->dn_indblkshift - SPA_BLKPTRSHIFT);
	dnode_t *dn = NULL;
	int dn_slots = dnodesize >> DNODE_SHIFT;
	boolean_t restarted = B_FALSE;
	uint64_t *cpuobj = NULL;
	uint_t dnodes_per_chunk = 1 << dmu_object_alloc_chunk_shift;
	int error;
	cpuobj = &os->os_obj_next_percpu[CPU_SEQID_UNSTABLE %
	    os->os_obj_next_percpu_len];
	if (dn_slots == 0) {
		dn_slots = DNODE_MIN_SLOTS;
	} else {
		ASSERT3S(dn_slots, >=, DNODE_MIN_SLOTS);
		ASSERT3S(dn_slots, <=, DNODE_MAX_SLOTS);
	}
	if (dnodes_per_chunk < DNODES_PER_BLOCK)
		dnodes_per_chunk = DNODES_PER_BLOCK;
	if (dnodes_per_chunk > L1_dnode_count)
		dnodes_per_chunk = L1_dnode_count;
	if (allocated_dnode != NULL) {
		ASSERT3P(tag, !=, NULL);
	} else {
		ASSERT3P(tag, ==, NULL);
		tag = FTAG;
	}
	object = *cpuobj;
	for (;;) {
		if ((P2PHASE(object, dnodes_per_chunk) == 0) ||
		    (P2PHASE(object + dn_slots - 1, dnodes_per_chunk) <
		    dn_slots)) {
			DNODE_STAT_BUMP(dnode_alloc_next_chunk);
			mutex_enter(&os->os_obj_lock);
			ASSERT0(P2PHASE(os->os_obj_next_chunk,
			    dnodes_per_chunk));
			object = os->os_obj_next_chunk;
			if (P2PHASE(object, L1_dnode_count) == 0) {
				uint64_t offset;
				uint64_t blkfill;
				int minlvl;
				if (os->os_rescan_dnodes) {
					offset = 0;
					os->os_rescan_dnodes = B_FALSE;
				} else {
					offset = object << DNODE_SHIFT;
				}
				blkfill = restarted ? 1 : DNODES_PER_BLOCK >> 2;
				minlvl = restarted ? 1 : 2;
				restarted = B_TRUE;
				error = dnode_next_offset(DMU_META_DNODE(os),
				    DNODE_FIND_HOLE, &offset, minlvl,
				    blkfill, 0);
				if (error == 0) {
					object = offset >> DNODE_SHIFT;
				}
			}
			os->os_obj_next_chunk =
			    P2ALIGN(object, dnodes_per_chunk) +
			    dnodes_per_chunk;
			(void) atomic_swap_64(cpuobj, object);
			mutex_exit(&os->os_obj_lock);
		}
		object = atomic_add_64_nv(cpuobj, dn_slots) - dn_slots;
		error = dnode_hold_impl(os, object, DNODE_MUST_BE_FREE,
		    dn_slots, tag, &dn);
		if (error == 0) {
			rw_enter(&dn->dn_struct_rwlock, RW_WRITER);
			if (dn->dn_type == DMU_OT_NONE) {
				dnode_allocate(dn, ot, blocksize,
				    indirect_blockshift, bonustype,
				    bonuslen, dn_slots, tx);
				rw_exit(&dn->dn_struct_rwlock);
				dmu_tx_add_new_object(tx, dn);
				if (allocated_dnode != NULL)
					*allocated_dnode = dn;
				else
					dnode_rele(dn, tag);
				return (object);
			}
			rw_exit(&dn->dn_struct_rwlock);
			dnode_rele(dn, tag);
			DNODE_STAT_BUMP(dnode_alloc_race);
		}
		if (dmu_object_next(os, &object, B_TRUE, 0) != 0) {
			object = P2ROUNDUP(object + 1, DNODES_PER_BLOCK);
			DNODE_STAT_BUMP(dnode_alloc_next_block);
		}
		(void) atomic_swap_64(cpuobj, object);
	}
}
uint64_t
dmu_object_alloc(objset_t *os, dmu_object_type_t ot, int blocksize,
    dmu_object_type_t bonustype, int bonuslen, dmu_tx_t *tx)
{
	return dmu_object_alloc_impl(os, ot, blocksize, 0, bonustype,
	    bonuslen, 0, NULL, NULL, tx);
}
uint64_t
dmu_object_alloc_ibs(objset_t *os, dmu_object_type_t ot, int blocksize,
    int indirect_blockshift, dmu_object_type_t bonustype, int bonuslen,
    dmu_tx_t *tx)
{
	return dmu_object_alloc_impl(os, ot, blocksize, indirect_blockshift,
	    bonustype, bonuslen, 0, NULL, NULL, tx);
}
uint64_t
dmu_object_alloc_dnsize(objset_t *os, dmu_object_type_t ot, int blocksize,
    dmu_object_type_t bonustype, int bonuslen, int dnodesize, dmu_tx_t *tx)
{
	return (dmu_object_alloc_impl(os, ot, blocksize, 0, bonustype,
	    bonuslen, dnodesize, NULL, NULL, tx));
}
uint64_t
dmu_object_alloc_hold(objset_t *os, dmu_object_type_t ot, int blocksize,
    int indirect_blockshift, dmu_object_type_t bonustype, int bonuslen,
    int dnodesize, dnode_t **allocated_dnode, const void *tag, dmu_tx_t *tx)
{
	return (dmu_object_alloc_impl(os, ot, blocksize, indirect_blockshift,
	    bonustype, bonuslen, dnodesize, allocated_dnode, tag, tx));
}
int
dmu_object_claim(objset_t *os, uint64_t object, dmu_object_type_t ot,
    int blocksize, dmu_object_type_t bonustype, int bonuslen, dmu_tx_t *tx)
{
	return (dmu_object_claim_dnsize(os, object, ot, blocksize, bonustype,
	    bonuslen, 0, tx));
}
int
dmu_object_claim_dnsize(objset_t *os, uint64_t object, dmu_object_type_t ot,
    int blocksize, dmu_object_type_t bonustype, int bonuslen,
    int dnodesize, dmu_tx_t *tx)
{
	dnode_t *dn;
	int dn_slots = dnodesize >> DNODE_SHIFT;
	int err;
	if (dn_slots == 0)
		dn_slots = DNODE_MIN_SLOTS;
	ASSERT3S(dn_slots, >=, DNODE_MIN_SLOTS);
	ASSERT3S(dn_slots, <=, DNODE_MAX_SLOTS);
	if (object == DMU_META_DNODE_OBJECT && !dmu_tx_private_ok(tx))
		return (SET_ERROR(EBADF));
	err = dnode_hold_impl(os, object, DNODE_MUST_BE_FREE, dn_slots,
	    FTAG, &dn);
	if (err)
		return (err);
	dnode_allocate(dn, ot, blocksize, 0, bonustype, bonuslen, dn_slots, tx);
	dmu_tx_add_new_object(tx, dn);
	dnode_rele(dn, FTAG);
	return (0);
}
int
dmu_object_reclaim(objset_t *os, uint64_t object, dmu_object_type_t ot,
    int blocksize, dmu_object_type_t bonustype, int bonuslen, dmu_tx_t *tx)
{
	return (dmu_object_reclaim_dnsize(os, object, ot, blocksize, bonustype,
	    bonuslen, DNODE_MIN_SIZE, B_FALSE, tx));
}
int
dmu_object_reclaim_dnsize(objset_t *os, uint64_t object, dmu_object_type_t ot,
    int blocksize, dmu_object_type_t bonustype, int bonuslen, int dnodesize,
    boolean_t keep_spill, dmu_tx_t *tx)
{
	dnode_t *dn;
	int dn_slots = dnodesize >> DNODE_SHIFT;
	int err;
	if (dn_slots == 0)
		dn_slots = DNODE_MIN_SLOTS;
	if (object == DMU_META_DNODE_OBJECT)
		return (SET_ERROR(EBADF));
	err = dnode_hold_impl(os, object, DNODE_MUST_BE_ALLOCATED, 0,
	    FTAG, &dn);
	if (err)
		return (err);
	dnode_reallocate(dn, ot, blocksize, bonustype, bonuslen, dn_slots,
	    keep_spill, tx);
	dnode_rele(dn, FTAG);
	return (err);
}
int
dmu_object_rm_spill(objset_t *os, uint64_t object, dmu_tx_t *tx)
{
	dnode_t *dn;
	int err;
	err = dnode_hold_impl(os, object, DNODE_MUST_BE_ALLOCATED, 0,
	    FTAG, &dn);
	if (err)
		return (err);
	rw_enter(&dn->dn_struct_rwlock, RW_WRITER);
	if (dn->dn_phys->dn_flags & DNODE_FLAG_SPILL_BLKPTR) {
		dbuf_rm_spill(dn, tx);
		dnode_rm_spill(dn, tx);
	}
	rw_exit(&dn->dn_struct_rwlock);
	dnode_rele(dn, FTAG);
	return (err);
}
int
dmu_object_free(objset_t *os, uint64_t object, dmu_tx_t *tx)
{
	dnode_t *dn;
	int err;
	ASSERT(object != DMU_META_DNODE_OBJECT || dmu_tx_private_ok(tx));
	err = dnode_hold_impl(os, object, DNODE_MUST_BE_ALLOCATED, 0,
	    FTAG, &dn);
	if (err)
		return (err);
	ASSERT(dn->dn_type != DMU_OT_NONE);
	dnode_free_range(dn, 0, DMU_OBJECT_END, tx);
	dnode_free(dn, tx);
	dnode_rele(dn, FTAG);
	return (0);
}
int
dmu_object_next(objset_t *os, uint64_t *objectp, boolean_t hole, uint64_t txg)
{
	uint64_t offset;
	uint64_t start_obj;
	struct dsl_dataset *ds = os->os_dsl_dataset;
	int error;
	if (*objectp == 0) {
		start_obj = 1;
	} else if (ds && dsl_dataset_feature_is_active(ds,
	    SPA_FEATURE_LARGE_DNODE)) {
		uint64_t i = *objectp + 1;
		uint64_t last_obj = *objectp | (DNODES_PER_BLOCK - 1);
		dmu_object_info_t doi;
		while (i <= last_obj) {
			if (i == 0)
				return (SET_ERROR(ESRCH));
			error = dmu_object_info(os, i, &doi);
			if (error == ENOENT) {
				if (hole) {
					*objectp = i;
					return (0);
				} else {
					i++;
				}
			} else if (error == EEXIST) {
				i++;
			} else if (error == 0) {
				if (hole) {
					i += doi.doi_dnodesize >> DNODE_SHIFT;
				} else {
					*objectp = i;
					return (0);
				}
			} else {
				return (error);
			}
		}
		start_obj = i;
	} else {
		start_obj = *objectp + 1;
	}
	offset = start_obj << DNODE_SHIFT;
	error = dnode_next_offset(DMU_META_DNODE(os),
	    (hole ? DNODE_FIND_HOLE : 0), &offset, 0, DNODES_PER_BLOCK, txg);
	*objectp = offset >> DNODE_SHIFT;
	return (error);
}
void
dmu_object_zapify(objset_t *mos, uint64_t object, dmu_object_type_t old_type,
    dmu_tx_t *tx)
{
	dnode_t *dn;
	ASSERT(dmu_tx_is_syncing(tx));
	VERIFY0(dnode_hold(mos, object, FTAG, &dn));
	if (dn->dn_type == DMU_OTN_ZAP_METADATA) {
		dnode_rele(dn, FTAG);
		return;
	}
	ASSERT3U(dn->dn_type, ==, old_type);
	ASSERT0(dn->dn_maxblkid);
	mzap_create_impl(dn, 0, 0, tx);
	dn->dn_next_type[tx->tx_txg & TXG_MASK] = dn->dn_type =
	    DMU_OTN_ZAP_METADATA;
	dnode_setdirty(dn, tx);
	dnode_rele(dn, FTAG);
	spa_feature_incr(dmu_objset_spa(mos),
	    SPA_FEATURE_EXTENSIBLE_DATASET, tx);
}
void
dmu_object_free_zapified(objset_t *mos, uint64_t object, dmu_tx_t *tx)
{
	dnode_t *dn;
	dmu_object_type_t t;
	ASSERT(dmu_tx_is_syncing(tx));
	VERIFY0(dnode_hold(mos, object, FTAG, &dn));
	t = dn->dn_type;
	dnode_rele(dn, FTAG);
	if (t == DMU_OTN_ZAP_METADATA) {
		spa_feature_decr(dmu_objset_spa(mos),
		    SPA_FEATURE_EXTENSIBLE_DATASET, tx);
	}
	VERIFY0(dmu_object_free(mos, object, tx));
}
EXPORT_SYMBOL(dmu_object_alloc);
EXPORT_SYMBOL(dmu_object_alloc_ibs);
EXPORT_SYMBOL(dmu_object_alloc_dnsize);
EXPORT_SYMBOL(dmu_object_alloc_hold);
EXPORT_SYMBOL(dmu_object_claim);
EXPORT_SYMBOL(dmu_object_claim_dnsize);
EXPORT_SYMBOL(dmu_object_reclaim);
EXPORT_SYMBOL(dmu_object_reclaim_dnsize);
EXPORT_SYMBOL(dmu_object_rm_spill);
EXPORT_SYMBOL(dmu_object_free);
EXPORT_SYMBOL(dmu_object_next);
EXPORT_SYMBOL(dmu_object_zapify);
EXPORT_SYMBOL(dmu_object_free_zapified);
ZFS_MODULE_PARAM(zfs, , dmu_object_alloc_chunk_shift, UINT, ZMOD_RW,
	"CPU-specific allocator grabs 2^N objects at once");
