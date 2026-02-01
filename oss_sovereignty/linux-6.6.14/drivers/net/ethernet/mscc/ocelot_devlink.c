
 
#include <net/devlink.h>
#include "ocelot.h"

 
#define OCELOT_RESOURCE_SZ		256
#define OCELOT_NUM_RESOURCES		4

#define BUF_xxxx_I			(0 * OCELOT_RESOURCE_SZ)
#define REF_xxxx_I			(1 * OCELOT_RESOURCE_SZ)
#define BUF_xxxx_E			(2 * OCELOT_RESOURCE_SZ)
#define REF_xxxx_E			(3 * OCELOT_RESOURCE_SZ)

 
#define xxx_Q_RSRV_x			0
#define xxx_PRIO_SHR_x			216
#define xxx_P_RSRV_x			224
#define xxx_COL_SHR_x			254

 

 
#define BUF_Q_RSRV_E(port, prio) \
	(BUF_xxxx_E + xxx_Q_RSRV_x + OCELOT_NUM_TC * (port) + (prio))

 
#define BUF_P_RSRV_E(port) \
	(BUF_xxxx_E + xxx_P_RSRV_x + (port))

 
#define BUF_Q_RSRV_I(port, prio) \
	(BUF_xxxx_I + xxx_Q_RSRV_x + OCELOT_NUM_TC * (port) + (prio))

 
#define BUF_P_RSRV_I(port) \
	(BUF_xxxx_I + xxx_P_RSRV_x + (port))

 
#define REF_Q_RSRV_E(port, prio) \
	(REF_xxxx_E + xxx_Q_RSRV_x + OCELOT_NUM_TC * (port) + (prio))

 
#define REF_P_RSRV_E(port) \
	(REF_xxxx_E + xxx_P_RSRV_x + (port))

 
#define REF_Q_RSRV_I(port, prio) \
	(REF_xxxx_I + xxx_Q_RSRV_x + OCELOT_NUM_TC * (port) + (prio))

 
#define REF_P_RSRV_I(port) \
	(REF_xxxx_I + xxx_P_RSRV_x + (port))

 

 
#define BUF_PRIO_SHR_E(prio) \
	(BUF_xxxx_E + xxx_PRIO_SHR_x + (prio))

 
#define BUF_COL_SHR_E(dp) \
	(BUF_xxxx_E + xxx_COL_SHR_x + (1 - (dp)))

 
#define BUF_PRIO_SHR_I(prio) \
	(BUF_xxxx_I + xxx_PRIO_SHR_x + (prio))

 
#define BUF_COL_SHR_I(dp) \
	(BUF_xxxx_I + xxx_COL_SHR_x + (1 - (dp)))

 
#define REF_PRIO_SHR_E(prio) \
	(REF_xxxx_E + xxx_PRIO_SHR_x + (prio))

 
#define REF_COL_SHR_E(dp) \
	(REF_xxxx_E + xxx_COL_SHR_x + (1 - (dp)))

 
#define REF_PRIO_SHR_I(prio) \
	(REF_xxxx_I + xxx_PRIO_SHR_x + (prio))

 
#define REF_COL_SHR_I(dp) \
	(REF_xxxx_I + xxx_COL_SHR_x + (1 - (dp)))

static u32 ocelot_wm_read(struct ocelot *ocelot, int index)
{
	int wm = ocelot_read_gix(ocelot, QSYS_RES_CFG, index);

	return ocelot->ops->wm_dec(wm);
}

static void ocelot_wm_write(struct ocelot *ocelot, int index, u32 val)
{
	u32 wm = ocelot->ops->wm_enc(val);

	ocelot_write_gix(ocelot, wm, QSYS_RES_CFG, index);
}

static void ocelot_wm_status(struct ocelot *ocelot, int index, u32 *inuse,
			     u32 *maxuse)
{
	int res_stat = ocelot_read_gix(ocelot, QSYS_RES_STAT, index);

	return ocelot->ops->wm_stat(res_stat, inuse, maxuse);
}

 
static void ocelot_disable_reservation_watermarks(struct ocelot *ocelot,
						  int port)
{
	int prio;

	for (prio = 0; prio < OCELOT_NUM_TC; prio++) {
		ocelot_wm_write(ocelot, BUF_Q_RSRV_I(port, prio), 0);
		ocelot_wm_write(ocelot, BUF_Q_RSRV_E(port, prio), 0);
		ocelot_wm_write(ocelot, REF_Q_RSRV_I(port, prio), 0);
		ocelot_wm_write(ocelot, REF_Q_RSRV_E(port, prio), 0);
	}

	ocelot_wm_write(ocelot, BUF_P_RSRV_I(port), 0);
	ocelot_wm_write(ocelot, BUF_P_RSRV_E(port), 0);
	ocelot_wm_write(ocelot, REF_P_RSRV_I(port), 0);
	ocelot_wm_write(ocelot, REF_P_RSRV_E(port), 0);
}

 
static void ocelot_disable_tc_sharing_watermarks(struct ocelot *ocelot)
{
	int prio;

	for (prio = 0; prio < OCELOT_NUM_TC; prio++) {
		ocelot_wm_write(ocelot, BUF_PRIO_SHR_I(prio), 0);
		ocelot_wm_write(ocelot, BUF_PRIO_SHR_E(prio), 0);
		ocelot_wm_write(ocelot, REF_PRIO_SHR_I(prio), 0);
		ocelot_wm_write(ocelot, REF_PRIO_SHR_E(prio), 0);
	}
}

static void ocelot_get_buf_rsrv(struct ocelot *ocelot, u32 *buf_rsrv_i,
				u32 *buf_rsrv_e)
{
	int port, prio;

	*buf_rsrv_i = 0;
	*buf_rsrv_e = 0;

	for (port = 0; port <= ocelot->num_phys_ports; port++) {
		for (prio = 0; prio < OCELOT_NUM_TC; prio++) {
			*buf_rsrv_i += ocelot_wm_read(ocelot,
						      BUF_Q_RSRV_I(port, prio));
			*buf_rsrv_e += ocelot_wm_read(ocelot,
						      BUF_Q_RSRV_E(port, prio));
		}

		*buf_rsrv_i += ocelot_wm_read(ocelot, BUF_P_RSRV_I(port));
		*buf_rsrv_e += ocelot_wm_read(ocelot, BUF_P_RSRV_E(port));
	}

	*buf_rsrv_i *= OCELOT_BUFFER_CELL_SZ;
	*buf_rsrv_e *= OCELOT_BUFFER_CELL_SZ;
}

static void ocelot_get_ref_rsrv(struct ocelot *ocelot, u32 *ref_rsrv_i,
				u32 *ref_rsrv_e)
{
	int port, prio;

	*ref_rsrv_i = 0;
	*ref_rsrv_e = 0;

	for (port = 0; port <= ocelot->num_phys_ports; port++) {
		for (prio = 0; prio < OCELOT_NUM_TC; prio++) {
			*ref_rsrv_i += ocelot_wm_read(ocelot,
						      REF_Q_RSRV_I(port, prio));
			*ref_rsrv_e += ocelot_wm_read(ocelot,
						      REF_Q_RSRV_E(port, prio));
		}

		*ref_rsrv_i += ocelot_wm_read(ocelot, REF_P_RSRV_I(port));
		*ref_rsrv_e += ocelot_wm_read(ocelot, REF_P_RSRV_E(port));
	}
}

 
static void ocelot_setup_sharing_watermarks(struct ocelot *ocelot)
{
	u32 buf_rsrv_i, buf_rsrv_e;
	u32 ref_rsrv_i, ref_rsrv_e;
	u32 buf_shr_i, buf_shr_e;
	u32 ref_shr_i, ref_shr_e;

	ocelot_get_buf_rsrv(ocelot, &buf_rsrv_i, &buf_rsrv_e);
	ocelot_get_ref_rsrv(ocelot, &ref_rsrv_i, &ref_rsrv_e);

	buf_shr_i = ocelot->pool_size[OCELOT_SB_BUF][OCELOT_SB_POOL_ING] -
		    buf_rsrv_i;
	buf_shr_e = ocelot->pool_size[OCELOT_SB_BUF][OCELOT_SB_POOL_EGR] -
		    buf_rsrv_e;
	ref_shr_i = ocelot->pool_size[OCELOT_SB_REF][OCELOT_SB_POOL_ING] -
		    ref_rsrv_i;
	ref_shr_e = ocelot->pool_size[OCELOT_SB_REF][OCELOT_SB_POOL_EGR] -
		    ref_rsrv_e;

	buf_shr_i /= OCELOT_BUFFER_CELL_SZ;
	buf_shr_e /= OCELOT_BUFFER_CELL_SZ;

	ocelot_wm_write(ocelot, BUF_COL_SHR_I(0), buf_shr_i);
	ocelot_wm_write(ocelot, BUF_COL_SHR_E(0), buf_shr_e);
	ocelot_wm_write(ocelot, REF_COL_SHR_E(0), ref_shr_e);
	ocelot_wm_write(ocelot, REF_COL_SHR_I(0), ref_shr_i);
	ocelot_wm_write(ocelot, BUF_COL_SHR_I(1), 0);
	ocelot_wm_write(ocelot, BUF_COL_SHR_E(1), 0);
	ocelot_wm_write(ocelot, REF_COL_SHR_E(1), 0);
	ocelot_wm_write(ocelot, REF_COL_SHR_I(1), 0);
}

 
static int ocelot_watermark_validate(struct ocelot *ocelot,
				     struct netlink_ext_ack *extack)
{
	u32 buf_rsrv_i, buf_rsrv_e;
	u32 ref_rsrv_i, ref_rsrv_e;

	ocelot_get_buf_rsrv(ocelot, &buf_rsrv_i, &buf_rsrv_e);
	ocelot_get_ref_rsrv(ocelot, &ref_rsrv_i, &ref_rsrv_e);

	if (buf_rsrv_i > ocelot->pool_size[OCELOT_SB_BUF][OCELOT_SB_POOL_ING]) {
		NL_SET_ERR_MSG_MOD(extack,
				   "Ingress frame reservations exceed pool size");
		return -ERANGE;
	}
	if (buf_rsrv_e > ocelot->pool_size[OCELOT_SB_BUF][OCELOT_SB_POOL_EGR]) {
		NL_SET_ERR_MSG_MOD(extack,
				   "Egress frame reservations exceed pool size");
		return -ERANGE;
	}
	if (ref_rsrv_i > ocelot->pool_size[OCELOT_SB_REF][OCELOT_SB_POOL_ING]) {
		NL_SET_ERR_MSG_MOD(extack,
				   "Ingress reference reservations exceed pool size");
		return -ERANGE;
	}
	if (ref_rsrv_e > ocelot->pool_size[OCELOT_SB_REF][OCELOT_SB_POOL_EGR]) {
		NL_SET_ERR_MSG_MOD(extack,
				   "Egress reference reservations exceed pool size");
		return -ERANGE;
	}

	return 0;
}

 
static void ocelot_watermark_init(struct ocelot *ocelot)
{
	int all_tcs = GENMASK(OCELOT_NUM_TC - 1, 0);
	int port;

	ocelot_write(ocelot, all_tcs, QSYS_RES_QOS_MODE);

	for (port = 0; port <= ocelot->num_phys_ports; port++)
		ocelot_disable_reservation_watermarks(ocelot, port);

	ocelot_disable_tc_sharing_watermarks(ocelot);
	ocelot_setup_sharing_watermarks(ocelot);
}

 
u16 ocelot_wm_enc(u16 value)
{
	WARN_ON(value >= 16 * BIT(8));

	if (value >= BIT(8))
		return BIT(8) | (value / 16);

	return value;
}
EXPORT_SYMBOL(ocelot_wm_enc);

u16 ocelot_wm_dec(u16 wm)
{
	if (wm & BIT(8))
		return (wm & GENMASK(7, 0)) * 16;

	return wm;
}
EXPORT_SYMBOL(ocelot_wm_dec);

void ocelot_wm_stat(u32 val, u32 *inuse, u32 *maxuse)
{
	*inuse = (val & GENMASK(23, 12)) >> 12;
	*maxuse = val & GENMASK(11, 0);
}
EXPORT_SYMBOL(ocelot_wm_stat);

 
static const struct devlink_sb_pool_info ocelot_sb_pool[] = {
	[OCELOT_SB_BUF] = {
		.cell_size = OCELOT_BUFFER_CELL_SZ,
		.threshold_type = DEVLINK_SB_THRESHOLD_TYPE_STATIC,
	},
	[OCELOT_SB_REF] = {
		.cell_size = 1,
		.threshold_type = DEVLINK_SB_THRESHOLD_TYPE_STATIC,
	},
};

 
int ocelot_sb_pool_get(struct ocelot *ocelot, unsigned int sb_index,
		       u16 pool_index,
		       struct devlink_sb_pool_info *pool_info)
{
	if (sb_index >= OCELOT_SB_NUM)
		return -ENODEV;
	if (pool_index >= OCELOT_SB_POOL_NUM)
		return -ENODEV;

	*pool_info = ocelot_sb_pool[sb_index];
	pool_info->size = ocelot->pool_size[sb_index][pool_index];
	if (pool_index)
		pool_info->pool_type = DEVLINK_SB_POOL_TYPE_INGRESS;
	else
		pool_info->pool_type = DEVLINK_SB_POOL_TYPE_EGRESS;

	return 0;
}
EXPORT_SYMBOL(ocelot_sb_pool_get);

 
int ocelot_sb_pool_set(struct ocelot *ocelot, unsigned int sb_index,
		       u16 pool_index, u32 size,
		       enum devlink_sb_threshold_type threshold_type,
		       struct netlink_ext_ack *extack)
{
	u32 old_pool_size;
	int err;

	if (sb_index >= OCELOT_SB_NUM) {
		NL_SET_ERR_MSG_MOD(extack,
				   "Invalid sb, use 0 for buffers and 1 for frame references");
		return -ENODEV;
	}
	if (pool_index >= OCELOT_SB_POOL_NUM) {
		NL_SET_ERR_MSG_MOD(extack,
				   "Invalid pool, use 0 for ingress and 1 for egress");
		return -ENODEV;
	}
	if (threshold_type != DEVLINK_SB_THRESHOLD_TYPE_STATIC) {
		NL_SET_ERR_MSG_MOD(extack,
				   "Only static threshold supported");
		return -EOPNOTSUPP;
	}

	old_pool_size = ocelot->pool_size[sb_index][pool_index];
	ocelot->pool_size[sb_index][pool_index] = size;

	err = ocelot_watermark_validate(ocelot, extack);
	if (err) {
		ocelot->pool_size[sb_index][pool_index] = old_pool_size;
		return err;
	}

	ocelot_setup_sharing_watermarks(ocelot);

	return 0;
}
EXPORT_SYMBOL(ocelot_sb_pool_set);

 
int ocelot_sb_port_pool_get(struct ocelot *ocelot, int port,
			    unsigned int sb_index, u16 pool_index,
			    u32 *p_threshold)
{
	int wm_index;

	switch (sb_index) {
	case OCELOT_SB_BUF:
		if (pool_index == OCELOT_SB_POOL_ING)
			wm_index = BUF_P_RSRV_I(port);
		else
			wm_index = BUF_P_RSRV_E(port);
		break;
	case OCELOT_SB_REF:
		if (pool_index == OCELOT_SB_POOL_ING)
			wm_index = REF_P_RSRV_I(port);
		else
			wm_index = REF_P_RSRV_E(port);
		break;
	default:
		return -ENODEV;
	}

	*p_threshold = ocelot_wm_read(ocelot, wm_index);
	*p_threshold *= ocelot_sb_pool[sb_index].cell_size;

	return 0;
}
EXPORT_SYMBOL(ocelot_sb_port_pool_get);

 
int ocelot_sb_port_pool_set(struct ocelot *ocelot, int port,
			    unsigned int sb_index, u16 pool_index,
			    u32 threshold, struct netlink_ext_ack *extack)
{
	int wm_index, err;
	u32 old_thr;

	switch (sb_index) {
	case OCELOT_SB_BUF:
		if (pool_index == OCELOT_SB_POOL_ING)
			wm_index = BUF_P_RSRV_I(port);
		else
			wm_index = BUF_P_RSRV_E(port);
		break;
	case OCELOT_SB_REF:
		if (pool_index == OCELOT_SB_POOL_ING)
			wm_index = REF_P_RSRV_I(port);
		else
			wm_index = REF_P_RSRV_E(port);
		break;
	default:
		NL_SET_ERR_MSG_MOD(extack, "Invalid shared buffer");
		return -ENODEV;
	}

	threshold /= ocelot_sb_pool[sb_index].cell_size;

	old_thr = ocelot_wm_read(ocelot, wm_index);
	ocelot_wm_write(ocelot, wm_index, threshold);

	err = ocelot_watermark_validate(ocelot, extack);
	if (err) {
		ocelot_wm_write(ocelot, wm_index, old_thr);
		return err;
	}

	ocelot_setup_sharing_watermarks(ocelot);

	return 0;
}
EXPORT_SYMBOL(ocelot_sb_port_pool_set);

 
int ocelot_sb_tc_pool_bind_get(struct ocelot *ocelot, int port,
			       unsigned int sb_index, u16 tc_index,
			       enum devlink_sb_pool_type pool_type,
			       u16 *p_pool_index, u32 *p_threshold)
{
	int wm_index;

	switch (sb_index) {
	case OCELOT_SB_BUF:
		if (pool_type == DEVLINK_SB_POOL_TYPE_INGRESS)
			wm_index = BUF_Q_RSRV_I(port, tc_index);
		else
			wm_index = BUF_Q_RSRV_E(port, tc_index);
		break;
	case OCELOT_SB_REF:
		if (pool_type == DEVLINK_SB_POOL_TYPE_INGRESS)
			wm_index = REF_Q_RSRV_I(port, tc_index);
		else
			wm_index = REF_Q_RSRV_E(port, tc_index);
		break;
	default:
		return -ENODEV;
	}

	*p_threshold = ocelot_wm_read(ocelot, wm_index);
	*p_threshold *= ocelot_sb_pool[sb_index].cell_size;

	if (pool_type == DEVLINK_SB_POOL_TYPE_INGRESS)
		*p_pool_index = 0;
	else
		*p_pool_index = 1;

	return 0;
}
EXPORT_SYMBOL(ocelot_sb_tc_pool_bind_get);

 
int ocelot_sb_tc_pool_bind_set(struct ocelot *ocelot, int port,
			       unsigned int sb_index, u16 tc_index,
			       enum devlink_sb_pool_type pool_type,
			       u16 pool_index, u32 threshold,
			       struct netlink_ext_ack *extack)
{
	int wm_index, err;
	u32 old_thr;

	 
	if (pool_index == OCELOT_SB_POOL_ING &&
	    pool_type != DEVLINK_SB_POOL_TYPE_INGRESS)
		return -EINVAL;
	if (pool_index == OCELOT_SB_POOL_EGR &&
	    pool_type != DEVLINK_SB_POOL_TYPE_EGRESS)
		return -EINVAL;

	switch (sb_index) {
	case OCELOT_SB_BUF:
		if (pool_type == DEVLINK_SB_POOL_TYPE_INGRESS)
			wm_index = BUF_Q_RSRV_I(port, tc_index);
		else
			wm_index = BUF_Q_RSRV_E(port, tc_index);
		break;
	case OCELOT_SB_REF:
		if (pool_type == DEVLINK_SB_POOL_TYPE_INGRESS)
			wm_index = REF_Q_RSRV_I(port, tc_index);
		else
			wm_index = REF_Q_RSRV_E(port, tc_index);
		break;
	default:
		NL_SET_ERR_MSG_MOD(extack, "Invalid shared buffer");
		return -ENODEV;
	}

	threshold /= ocelot_sb_pool[sb_index].cell_size;

	old_thr = ocelot_wm_read(ocelot, wm_index);
	ocelot_wm_write(ocelot, wm_index, threshold);
	err = ocelot_watermark_validate(ocelot, extack);
	if (err) {
		ocelot_wm_write(ocelot, wm_index, old_thr);
		return err;
	}

	ocelot_setup_sharing_watermarks(ocelot);

	return 0;
}
EXPORT_SYMBOL(ocelot_sb_tc_pool_bind_set);

 
int ocelot_sb_occ_snapshot(struct ocelot *ocelot, unsigned int sb_index)
{
	return 0;
}
EXPORT_SYMBOL(ocelot_sb_occ_snapshot);

 
int ocelot_sb_occ_max_clear(struct ocelot *ocelot, unsigned int sb_index)
{
	u32 inuse, maxuse;
	int port, prio;

	switch (sb_index) {
	case OCELOT_SB_BUF:
		for (port = 0; port <= ocelot->num_phys_ports; port++) {
			for (prio = 0; prio < OCELOT_NUM_TC; prio++) {
				ocelot_wm_status(ocelot, BUF_Q_RSRV_I(port, prio),
						 &inuse, &maxuse);
				ocelot_wm_status(ocelot, BUF_Q_RSRV_E(port, prio),
						 &inuse, &maxuse);
			}
			ocelot_wm_status(ocelot, BUF_P_RSRV_I(port),
					 &inuse, &maxuse);
			ocelot_wm_status(ocelot, BUF_P_RSRV_E(port),
					 &inuse, &maxuse);
		}
		break;
	case OCELOT_SB_REF:
		for (port = 0; port <= ocelot->num_phys_ports; port++) {
			for (prio = 0; prio < OCELOT_NUM_TC; prio++) {
				ocelot_wm_status(ocelot, REF_Q_RSRV_I(port, prio),
						 &inuse, &maxuse);
				ocelot_wm_status(ocelot, REF_Q_RSRV_E(port, prio),
						 &inuse, &maxuse);
			}
			ocelot_wm_status(ocelot, REF_P_RSRV_I(port),
					 &inuse, &maxuse);
			ocelot_wm_status(ocelot, REF_P_RSRV_E(port),
					 &inuse, &maxuse);
		}
		break;
	default:
		return -ENODEV;
	}

	return 0;
}
EXPORT_SYMBOL(ocelot_sb_occ_max_clear);

 
int ocelot_sb_occ_port_pool_get(struct ocelot *ocelot, int port,
				unsigned int sb_index, u16 pool_index,
				u32 *p_cur, u32 *p_max)
{
	int wm_index;

	switch (sb_index) {
	case OCELOT_SB_BUF:
		if (pool_index == OCELOT_SB_POOL_ING)
			wm_index = BUF_P_RSRV_I(port);
		else
			wm_index = BUF_P_RSRV_E(port);
		break;
	case OCELOT_SB_REF:
		if (pool_index == OCELOT_SB_POOL_ING)
			wm_index = REF_P_RSRV_I(port);
		else
			wm_index = REF_P_RSRV_E(port);
		break;
	default:
		return -ENODEV;
	}

	ocelot_wm_status(ocelot, wm_index, p_cur, p_max);
	*p_cur *= ocelot_sb_pool[sb_index].cell_size;
	*p_max *= ocelot_sb_pool[sb_index].cell_size;

	return 0;
}
EXPORT_SYMBOL(ocelot_sb_occ_port_pool_get);

 
int ocelot_sb_occ_tc_port_bind_get(struct ocelot *ocelot, int port,
				   unsigned int sb_index, u16 tc_index,
				   enum devlink_sb_pool_type pool_type,
				   u32 *p_cur, u32 *p_max)
{
	int wm_index;

	switch (sb_index) {
	case OCELOT_SB_BUF:
		if (pool_type == DEVLINK_SB_POOL_TYPE_INGRESS)
			wm_index = BUF_Q_RSRV_I(port, tc_index);
		else
			wm_index = BUF_Q_RSRV_E(port, tc_index);
		break;
	case OCELOT_SB_REF:
		if (pool_type == DEVLINK_SB_POOL_TYPE_INGRESS)
			wm_index = REF_Q_RSRV_I(port, tc_index);
		else
			wm_index = REF_Q_RSRV_E(port, tc_index);
		break;
	default:
		return -ENODEV;
	}

	ocelot_wm_status(ocelot, wm_index, p_cur, p_max);
	*p_cur *= ocelot_sb_pool[sb_index].cell_size;
	*p_max *= ocelot_sb_pool[sb_index].cell_size;

	return 0;
}
EXPORT_SYMBOL(ocelot_sb_occ_tc_port_bind_get);

int ocelot_devlink_sb_register(struct ocelot *ocelot)
{
	int err;

	err = devlink_sb_register(ocelot->devlink, OCELOT_SB_BUF,
				  ocelot->packet_buffer_size, 1, 1,
				  OCELOT_NUM_TC, OCELOT_NUM_TC);
	if (err)
		return err;

	err = devlink_sb_register(ocelot->devlink, OCELOT_SB_REF,
				  ocelot->num_frame_refs, 1, 1,
				  OCELOT_NUM_TC, OCELOT_NUM_TC);
	if (err) {
		devlink_sb_unregister(ocelot->devlink, OCELOT_SB_BUF);
		return err;
	}

	ocelot->pool_size[OCELOT_SB_BUF][OCELOT_SB_POOL_ING] = ocelot->packet_buffer_size;
	ocelot->pool_size[OCELOT_SB_BUF][OCELOT_SB_POOL_EGR] = ocelot->packet_buffer_size;
	ocelot->pool_size[OCELOT_SB_REF][OCELOT_SB_POOL_ING] = ocelot->num_frame_refs;
	ocelot->pool_size[OCELOT_SB_REF][OCELOT_SB_POOL_EGR] = ocelot->num_frame_refs;

	ocelot_watermark_init(ocelot);

	return 0;
}
EXPORT_SYMBOL(ocelot_devlink_sb_register);

void ocelot_devlink_sb_unregister(struct ocelot *ocelot)
{
	devlink_sb_unregister(ocelot->devlink, OCELOT_SB_BUF);
	devlink_sb_unregister(ocelot->devlink, OCELOT_SB_REF);
}
EXPORT_SYMBOL(ocelot_devlink_sb_unregister);
