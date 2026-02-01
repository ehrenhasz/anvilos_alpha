 
 

 

#include <sys/arc.h>
#include <sys/zio.h>
#include <sys/zfs_ioctl.h>
#include <sys/vdev_impl.h>
#include <sys/dmu_objset.h>
#include <sys/dsl_dataset.h>
#include <sys/fs/zfs.h>

uint32_t zio_injection_enabled = 0;

 
typedef struct inject_handler {
	int			zi_id;
	spa_t			*zi_spa;
	zinject_record_t	zi_record;
	uint64_t		*zi_lanes;
	int			zi_next_lane;
	list_node_t		zi_link;
} inject_handler_t;

 
static list_t inject_handlers;

 
static krwlock_t inject_lock;

 
static int inject_delay_count = 0;

 
static kmutex_t inject_delay_mtx;

 
static int inject_next_id = 1;

 
static boolean_t
freq_triggered(uint32_t frequency)
{
	 
	if (frequency == 0)
		return (B_TRUE);

	 
	uint32_t maximum = (frequency <= 100) ? 100 : ZI_PERCENTAGE_MAX;

	return (random_in_range(maximum) < frequency);
}

 
static boolean_t
zio_match_handler(const zbookmark_phys_t *zb, uint64_t type, int dva,
    zinject_record_t *record, int error)
{
	 
	if (zb->zb_objset == DMU_META_OBJSET &&
	    record->zi_objset == DMU_META_OBJSET &&
	    record->zi_object == DMU_META_DNODE_OBJECT) {
		if (record->zi_type == DMU_OT_NONE ||
		    type == record->zi_type)
			return (freq_triggered(record->zi_freq));
		else
			return (B_FALSE);
	}

	 
	if (zb->zb_objset == record->zi_objset &&
	    zb->zb_object == record->zi_object &&
	    zb->zb_level == record->zi_level &&
	    zb->zb_blkid >= record->zi_start &&
	    zb->zb_blkid <= record->zi_end &&
	    (record->zi_dvas == 0 ||
	    (dva != ZI_NO_DVA && (record->zi_dvas & (1ULL << dva)))) &&
	    error == record->zi_error) {
		return (freq_triggered(record->zi_freq));
	}

	return (B_FALSE);
}

 
void
zio_handle_panic_injection(spa_t *spa, const char *tag, uint64_t type)
{
	inject_handler_t *handler;

	rw_enter(&inject_lock, RW_READER);

	for (handler = list_head(&inject_handlers); handler != NULL;
	    handler = list_next(&inject_handlers, handler)) {

		if (spa != handler->zi_spa)
			continue;

		if (handler->zi_record.zi_type == type &&
		    strcmp(tag, handler->zi_record.zi_func) == 0)
			panic("Panic requested in function %s\n", tag);
	}

	rw_exit(&inject_lock);
}

 
int
zio_handle_decrypt_injection(spa_t *spa, const zbookmark_phys_t *zb,
    uint64_t type, int error)
{
	int ret = 0;
	inject_handler_t *handler;

	rw_enter(&inject_lock, RW_READER);

	for (handler = list_head(&inject_handlers); handler != NULL;
	    handler = list_next(&inject_handlers, handler)) {

		if (spa != handler->zi_spa ||
		    handler->zi_record.zi_cmd != ZINJECT_DECRYPT_FAULT)
			continue;

		if (zio_match_handler(zb, type, ZI_NO_DVA,
		    &handler->zi_record, error)) {
			ret = error;
			break;
		}
	}

	rw_exit(&inject_lock);
	return (ret);
}

 
static int
zio_match_dva(zio_t *zio)
{
	int i = ZI_NO_DVA;

	if (zio->io_bp != NULL && zio->io_vd != NULL &&
	    zio->io_child_type == ZIO_CHILD_VDEV) {
		for (i = BP_GET_NDVAS(zio->io_bp) - 1; i >= 0; i--) {
			dva_t *dva = &zio->io_bp->blk_dva[i];
			uint64_t off = DVA_GET_OFFSET(dva);
			vdev_t *vd = vdev_lookup_top(zio->io_spa,
			    DVA_GET_VDEV(dva));

			 
			if (zio->io_vd->vdev_ops->vdev_op_leaf)
				off += VDEV_LABEL_START_SIZE;

			if (zio->io_vd == vd && zio->io_offset == off)
				break;
		}
	}

	return (i);
}


 
int
zio_handle_fault_injection(zio_t *zio, int error)
{
	int ret = 0;
	inject_handler_t *handler;

	 
	if (zio->io_logical == NULL)
		return (0);

	 
	if (zio->io_type != ZIO_TYPE_READ)
		return (0);

	 
	if (zio->io_priority == ZIO_PRIORITY_REBUILD && error == ECKSUM)
		return (0);

	rw_enter(&inject_lock, RW_READER);

	for (handler = list_head(&inject_handlers); handler != NULL;
	    handler = list_next(&inject_handlers, handler)) {
		if (zio->io_spa != handler->zi_spa ||
		    handler->zi_record.zi_cmd != ZINJECT_DATA_FAULT)
			continue;

		 
		if (zio_match_handler(&zio->io_logical->io_bookmark,
		    zio->io_bp ? BP_GET_TYPE(zio->io_bp) : DMU_OT_NONE,
		    zio_match_dva(zio), &handler->zi_record, error)) {
			ret = error;
			break;
		}
	}

	rw_exit(&inject_lock);

	return (ret);
}

 
int
zio_handle_label_injection(zio_t *zio, int error)
{
	inject_handler_t *handler;
	vdev_t *vd = zio->io_vd;
	uint64_t offset = zio->io_offset;
	int label;
	int ret = 0;

	if (offset >= VDEV_LABEL_START_SIZE &&
	    offset < vd->vdev_psize - VDEV_LABEL_END_SIZE)
		return (0);

	rw_enter(&inject_lock, RW_READER);

	for (handler = list_head(&inject_handlers); handler != NULL;
	    handler = list_next(&inject_handlers, handler)) {
		uint64_t start = handler->zi_record.zi_start;
		uint64_t end = handler->zi_record.zi_end;

		if (handler->zi_record.zi_cmd != ZINJECT_LABEL_FAULT)
			continue;

		 
		label = vdev_label_number(vd->vdev_psize, offset);
		start = vdev_label_offset(vd->vdev_psize, label, start);
		end = vdev_label_offset(vd->vdev_psize, label, end);

		if (zio->io_vd->vdev_guid == handler->zi_record.zi_guid &&
		    (offset >= start && offset <= end)) {
			ret = error;
			break;
		}
	}
	rw_exit(&inject_lock);
	return (ret);
}

static int
zio_inject_bitflip_cb(void *data, size_t len, void *private)
{
	zio_t *zio = private;
	uint8_t *buffer = data;
	uint_t byte = random_in_range(len);

	ASSERT3U(zio->io_type, ==, ZIO_TYPE_READ);

	 
	buffer[byte] ^= 1 << random_in_range(8);

	return (1);	 
}

static int
zio_handle_device_injection_impl(vdev_t *vd, zio_t *zio, int err1, int err2)
{
	inject_handler_t *handler;
	int ret = 0;

	 
	if (zio != NULL) {
		uint64_t offset = zio->io_offset;

		if (offset < VDEV_LABEL_START_SIZE ||
		    offset >= vd->vdev_psize - VDEV_LABEL_END_SIZE)
			return (0);
	}

	rw_enter(&inject_lock, RW_READER);

	for (handler = list_head(&inject_handlers); handler != NULL;
	    handler = list_next(&inject_handlers, handler)) {

		if (handler->zi_record.zi_cmd != ZINJECT_DEVICE_FAULT)
			continue;

		if (vd->vdev_guid == handler->zi_record.zi_guid) {
			if (handler->zi_record.zi_failfast &&
			    (zio == NULL || (zio->io_flags &
			    (ZIO_FLAG_IO_RETRY | ZIO_FLAG_TRYHARD)))) {
				continue;
			}

			 
			if (zio != NULL &&
			    handler->zi_record.zi_iotype != ZIO_TYPES &&
			    handler->zi_record.zi_iotype != zio->io_type)
				continue;

			if (handler->zi_record.zi_error == err1 ||
			    handler->zi_record.zi_error == err2) {
				 
				if (!freq_triggered(handler->zi_record.zi_freq))
					continue;

				 
				if (err1 == ENXIO)
					vd->vdev_stat.vs_aux =
					    VDEV_AUX_OPEN_FAILED;

				 
				if (!handler->zi_record.zi_failfast &&
				    zio != NULL)
					zio->io_flags |= ZIO_FLAG_IO_RETRY;

				 
				if (handler->zi_record.zi_error == EILSEQ) {
					if (zio == NULL)
						break;

					 
					(void) abd_iterate_func(zio->io_abd, 0,
					    zio->io_size, zio_inject_bitflip_cb,
					    zio);
					break;
				}

				ret = handler->zi_record.zi_error;
				break;
			}
			if (handler->zi_record.zi_error == ENXIO) {
				ret = SET_ERROR(EIO);
				break;
			}
		}
	}

	rw_exit(&inject_lock);

	return (ret);
}

int
zio_handle_device_injection(vdev_t *vd, zio_t *zio, int error)
{
	return (zio_handle_device_injection_impl(vd, zio, error, INT_MAX));
}

int
zio_handle_device_injections(vdev_t *vd, zio_t *zio, int err1, int err2)
{
	return (zio_handle_device_injection_impl(vd, zio, err1, err2));
}

 
void
zio_handle_ignored_writes(zio_t *zio)
{
	inject_handler_t *handler;

	rw_enter(&inject_lock, RW_READER);

	for (handler = list_head(&inject_handlers); handler != NULL;
	    handler = list_next(&inject_handlers, handler)) {

		 
		if (zio->io_spa != handler->zi_spa ||
		    handler->zi_record.zi_cmd != ZINJECT_IGNORED_WRITES)
			continue;

		 
		if (handler->zi_record.zi_timer == 0) {
			if (handler->zi_record.zi_duration > 0)
				handler->zi_record.zi_timer = ddi_get_lbolt64();
			else
				handler->zi_record.zi_timer = zio->io_txg;
		}

		 
		if (random_in_range(100) < 60)
			zio->io_pipeline &= ~ZIO_VDEV_IO_STAGES;
		break;
	}

	rw_exit(&inject_lock);
}

void
spa_handle_ignored_writes(spa_t *spa)
{
	inject_handler_t *handler;

	if (zio_injection_enabled == 0)
		return;

	rw_enter(&inject_lock, RW_READER);

	for (handler = list_head(&inject_handlers); handler != NULL;
	    handler = list_next(&inject_handlers, handler)) {

		if (spa != handler->zi_spa ||
		    handler->zi_record.zi_cmd != ZINJECT_IGNORED_WRITES)
			continue;

		if (handler->zi_record.zi_duration > 0) {
			VERIFY(handler->zi_record.zi_timer == 0 ||
			    ddi_time_after64(
			    (int64_t)handler->zi_record.zi_timer +
			    handler->zi_record.zi_duration * hz,
			    ddi_get_lbolt64()));
		} else {
			 
			VERIFY(handler->zi_record.zi_timer == 0 ||
			    handler->zi_record.zi_timer -
			    handler->zi_record.zi_duration >=
			    spa_syncing_txg(spa));
		}
	}

	rw_exit(&inject_lock);
}

hrtime_t
zio_handle_io_delay(zio_t *zio)
{
	vdev_t *vd = zio->io_vd;
	inject_handler_t *min_handler = NULL;
	hrtime_t min_target = 0;

	rw_enter(&inject_lock, RW_READER);

	 
	IMPLY(inject_delay_count > 0, zio_injection_enabled > 0);
	IMPLY(zio_injection_enabled == 0, inject_delay_count == 0);

	 
	if (inject_delay_count == 0) {
		rw_exit(&inject_lock);
		return (0);
	}

	 
	mutex_enter(&inject_delay_mtx);

	for (inject_handler_t *handler = list_head(&inject_handlers);
	    handler != NULL; handler = list_next(&inject_handlers, handler)) {
		if (handler->zi_record.zi_cmd != ZINJECT_DELAY_IO)
			continue;

		if (!freq_triggered(handler->zi_record.zi_freq))
			continue;

		if (vd->vdev_guid != handler->zi_record.zi_guid)
			continue;

		 
		ASSERT3P(handler->zi_lanes, !=, NULL);

		 
		ASSERT3U(handler->zi_record.zi_nlanes, !=, 0);

		ASSERT3U(handler->zi_record.zi_nlanes, >,
		    handler->zi_next_lane);

		 
		hrtime_t idle = handler->zi_record.zi_timer + gethrtime();
		hrtime_t busy = handler->zi_record.zi_timer +
		    handler->zi_lanes[handler->zi_next_lane];
		hrtime_t target = MAX(idle, busy);

		if (min_handler == NULL) {
			min_handler = handler;
			min_target = target;
			continue;
		}

		ASSERT3P(min_handler, !=, NULL);
		ASSERT3U(min_target, !=, 0);

		 

		if (target < min_target) {
			min_handler = handler;
			min_target = target;
		}
	}

	 
	if (min_handler != NULL) {
		ASSERT3U(min_target, !=, 0);
		min_handler->zi_lanes[min_handler->zi_next_lane] = min_target;

		 
		min_handler->zi_next_lane = (min_handler->zi_next_lane + 1) %
		    min_handler->zi_record.zi_nlanes;
	}

	mutex_exit(&inject_delay_mtx);
	rw_exit(&inject_lock);

	return (min_target);
}

static int
zio_calculate_range(const char *pool, zinject_record_t *record)
{
	dsl_pool_t *dp;
	dsl_dataset_t *ds;
	objset_t *os = NULL;
	dnode_t *dn = NULL;
	int error;

	 
	error = dsl_pool_hold(pool, FTAG, &dp);
	if (error)
		return (error);

	error = dsl_dataset_hold_obj(dp, record->zi_objset, FTAG, &ds);
	dsl_pool_rele(dp, FTAG);
	if (error)
		return (error);

	error = dmu_objset_from_ds(ds, &os);
	dsl_dataset_rele(ds, FTAG);
	if (error)
		return (error);

	error = dnode_hold(os, record->zi_object, FTAG, &dn);
	if (error)
		return (error);

	 
	if (record->zi_start != 0 || record->zi_end != -1ULL) {
		record->zi_start >>= dn->dn_datablkshift;
		record->zi_end >>= dn->dn_datablkshift;
	}
	if (record->zi_level > 0) {
		if (record->zi_level >= dn->dn_nlevels) {
			dnode_rele(dn, FTAG);
			return (SET_ERROR(EDOM));
		}

		if (record->zi_start != 0 || record->zi_end != 0) {
			int shift = dn->dn_indblkshift - SPA_BLKPTRSHIFT;

			for (int level = record->zi_level; level > 0; level--) {
				record->zi_start >>= shift;
				record->zi_end >>= shift;
			}
		}
	}

	dnode_rele(dn, FTAG);
	return (0);
}

 
int
zio_inject_fault(char *name, int flags, int *id, zinject_record_t *record)
{
	inject_handler_t *handler;
	int error;
	spa_t *spa;

	 
	if (flags & ZINJECT_UNLOAD_SPA)
		if ((error = spa_reset(name)) != 0)
			return (error);

	if (record->zi_cmd == ZINJECT_DELAY_IO) {
		 
		if (record->zi_timer == 0 || record->zi_nlanes == 0)
			return (SET_ERROR(EINVAL));

		 
		if (record->zi_nlanes >= UINT16_MAX)
			return (SET_ERROR(EINVAL));
	}

	 
	if (flags & ZINJECT_CALC_RANGE) {
		error = zio_calculate_range(name, record);
		if (error != 0)
			return (error);
	}

	if (!(flags & ZINJECT_NULL)) {
		 
		if ((spa = spa_inject_addref(name)) == NULL)
			return (SET_ERROR(ENOENT));

		handler = kmem_alloc(sizeof (inject_handler_t), KM_SLEEP);

		handler->zi_spa = spa;
		handler->zi_record = *record;

		if (handler->zi_record.zi_cmd == ZINJECT_DELAY_IO) {
			handler->zi_lanes = kmem_zalloc(
			    sizeof (*handler->zi_lanes) *
			    handler->zi_record.zi_nlanes, KM_SLEEP);
			handler->zi_next_lane = 0;
		} else {
			handler->zi_lanes = NULL;
			handler->zi_next_lane = 0;
		}

		rw_enter(&inject_lock, RW_WRITER);

		 
		if (handler->zi_record.zi_cmd == ZINJECT_DELAY_IO) {
			ASSERT3S(inject_delay_count, >=, 0);
			inject_delay_count++;
			ASSERT3S(inject_delay_count, >, 0);
		}

		*id = handler->zi_id = inject_next_id++;
		list_insert_tail(&inject_handlers, handler);
		atomic_inc_32(&zio_injection_enabled);

		rw_exit(&inject_lock);
	}

	 
	if (flags & ZINJECT_FLUSH_ARC)
		 
		arc_flush(NULL, FALSE);

	return (0);
}

 
int
zio_inject_list_next(int *id, char *name, size_t buflen,
    zinject_record_t *record)
{
	inject_handler_t *handler;
	int ret;

	mutex_enter(&spa_namespace_lock);
	rw_enter(&inject_lock, RW_READER);

	for (handler = list_head(&inject_handlers); handler != NULL;
	    handler = list_next(&inject_handlers, handler))
		if (handler->zi_id > *id)
			break;

	if (handler) {
		*record = handler->zi_record;
		*id = handler->zi_id;
		(void) strlcpy(name, spa_name(handler->zi_spa), buflen);
		ret = 0;
	} else {
		ret = SET_ERROR(ENOENT);
	}

	rw_exit(&inject_lock);
	mutex_exit(&spa_namespace_lock);

	return (ret);
}

 
int
zio_clear_fault(int id)
{
	inject_handler_t *handler;

	rw_enter(&inject_lock, RW_WRITER);

	for (handler = list_head(&inject_handlers); handler != NULL;
	    handler = list_next(&inject_handlers, handler))
		if (handler->zi_id == id)
			break;

	if (handler == NULL) {
		rw_exit(&inject_lock);
		return (SET_ERROR(ENOENT));
	}

	if (handler->zi_record.zi_cmd == ZINJECT_DELAY_IO) {
		ASSERT3S(inject_delay_count, >, 0);
		inject_delay_count--;
		ASSERT3S(inject_delay_count, >=, 0);
	}

	list_remove(&inject_handlers, handler);
	rw_exit(&inject_lock);

	if (handler->zi_record.zi_cmd == ZINJECT_DELAY_IO) {
		ASSERT3P(handler->zi_lanes, !=, NULL);
		kmem_free(handler->zi_lanes, sizeof (*handler->zi_lanes) *
		    handler->zi_record.zi_nlanes);
	} else {
		ASSERT3P(handler->zi_lanes, ==, NULL);
	}

	spa_inject_delref(handler->zi_spa);
	kmem_free(handler, sizeof (inject_handler_t));
	atomic_dec_32(&zio_injection_enabled);

	return (0);
}

void
zio_inject_init(void)
{
	rw_init(&inject_lock, NULL, RW_DEFAULT, NULL);
	mutex_init(&inject_delay_mtx, NULL, MUTEX_DEFAULT, NULL);
	list_create(&inject_handlers, sizeof (inject_handler_t),
	    offsetof(inject_handler_t, zi_link));
}

void
zio_inject_fini(void)
{
	list_destroy(&inject_handlers);
	mutex_destroy(&inject_delay_mtx);
	rw_destroy(&inject_lock);
}

#if defined(_KERNEL)
EXPORT_SYMBOL(zio_injection_enabled);
EXPORT_SYMBOL(zio_inject_fault);
EXPORT_SYMBOL(zio_inject_list_next);
EXPORT_SYMBOL(zio_clear_fault);
EXPORT_SYMBOL(zio_handle_fault_injection);
EXPORT_SYMBOL(zio_handle_device_injection);
EXPORT_SYMBOL(zio_handle_label_injection);
#endif
