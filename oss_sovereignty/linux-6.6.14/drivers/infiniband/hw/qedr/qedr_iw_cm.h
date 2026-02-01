 
#include <rdma/iw_cm.h>

int qedr_iw_connect(struct iw_cm_id *cm_id,
		    struct iw_cm_conn_param *conn_param);

int qedr_iw_create_listen(struct iw_cm_id *cm_id, int backlog);

int qedr_iw_destroy_listen(struct iw_cm_id *cm_id);

int qedr_iw_accept(struct iw_cm_id *cm_id, struct iw_cm_conn_param *conn_param);

int qedr_iw_reject(struct iw_cm_id *cm_id, const void *pdata, u8 pdata_len);

void qedr_iw_qp_add_ref(struct ib_qp *qp);

void qedr_iw_qp_rem_ref(struct ib_qp *qp);

struct ib_qp *qedr_iw_get_qp(struct ib_device *dev, int qpn);
