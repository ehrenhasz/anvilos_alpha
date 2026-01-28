#ifndef LINUX_QEDR_CM_H_
#define LINUX_QEDR_CM_H_
#define QEDR_GSI_MAX_RECV_WR	(4096)
#define QEDR_GSI_MAX_SEND_WR	(4096)
#define QEDR_GSI_MAX_RECV_SGE	(1)	 
#define QEDR_ROCE_V2_UDP_SPORT	(0000)
static inline u32 qedr_get_ipv4_from_gid(const u8 *gid)
{
	return *(u32 *)(void *)&gid[12];
}
int qedr_gsi_poll_cq(struct ib_cq *ibcq, int num_entries, struct ib_wc *wc);
int qedr_gsi_post_recv(struct ib_qp *ibqp, const struct ib_recv_wr *wr,
		       const struct ib_recv_wr **bad_wr);
int qedr_gsi_post_send(struct ib_qp *ibqp, const struct ib_send_wr *wr,
		       const struct ib_send_wr **bad_wr);
int qedr_create_gsi_qp(struct qedr_dev *dev, struct ib_qp_init_attr *attrs,
		       struct qedr_qp *qp);
void qedr_store_gsi_qp_cq(struct qedr_dev *dev,
			  struct qedr_qp *qp, struct ib_qp_init_attr *attrs);
int qedr_destroy_gsi_qp(struct qedr_dev *dev);
void qedr_inc_sw_gsi_cons(struct qedr_qp_hwq_info *info);
#endif
