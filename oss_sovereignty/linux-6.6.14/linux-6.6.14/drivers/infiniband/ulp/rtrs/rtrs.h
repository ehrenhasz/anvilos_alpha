#ifndef RTRS_H
#define RTRS_H
#include <linux/socket.h>
#include <linux/scatterlist.h>
struct rtrs_permit;
struct rtrs_clt_sess;
struct rtrs_srv_ctx;
struct rtrs_srv_sess;
struct rtrs_srv_op;
enum rtrs_clt_link_ev {
	RTRS_CLT_LINK_EV_RECONNECTED,
	RTRS_CLT_LINK_EV_DISCONNECTED,
};
struct rtrs_addr {
	struct sockaddr_storage *src;
	struct sockaddr_storage *dst;
};
struct rtrs_clt_ops {
	void	*priv;
	void	(*link_ev)(void *priv, enum rtrs_clt_link_ev ev);
};
struct rtrs_clt_sess *rtrs_clt_open(struct rtrs_clt_ops *ops,
				 const char *pathname,
				 const struct rtrs_addr *paths,
				 size_t path_cnt, u16 port,
				 size_t pdu_sz, u8 reconnect_delay_sec,
				 s16 max_reconnect_attempts, u32 nr_poll_queues);
void rtrs_clt_close(struct rtrs_clt_sess *clt);
enum wait_type {
	RTRS_PERMIT_NOWAIT = 0,
	RTRS_PERMIT_WAIT   = 1
};
enum rtrs_clt_con_type {
	RTRS_ADMIN_CON,
	RTRS_IO_CON
};
struct rtrs_permit *rtrs_clt_get_permit(struct rtrs_clt_sess *sess,
					enum rtrs_clt_con_type con_type,
					enum wait_type wait);
void rtrs_clt_put_permit(struct rtrs_clt_sess *sess,
			 struct rtrs_permit *permit);
struct rtrs_clt_req_ops {
	void	*priv;
	void	(*conf_fn)(void *priv, int errno);
};
int rtrs_clt_request(int dir, struct rtrs_clt_req_ops *ops,
		     struct rtrs_clt_sess *sess, struct rtrs_permit *permit,
		     const struct kvec *vec, size_t nr, size_t len,
		     struct scatterlist *sg, unsigned int sg_cnt);
int rtrs_clt_rdma_cq_direct(struct rtrs_clt_sess *clt, unsigned int index);
struct rtrs_attrs {
	u32		queue_depth;
	u32		max_io_size;
	u32		max_segments;
};
int rtrs_clt_query(struct rtrs_clt_sess *sess, struct rtrs_attrs *attr);
enum rtrs_srv_link_ev {
	RTRS_SRV_LINK_EV_CONNECTED,
	RTRS_SRV_LINK_EV_DISCONNECTED,
};
struct rtrs_srv_ops {
	int (*rdma_ev)(void *priv,
		       struct rtrs_srv_op *id,
		       void *data, size_t datalen, const void *usr,
		       size_t usrlen);
	int (*link_ev)(struct rtrs_srv_sess *sess, enum rtrs_srv_link_ev ev,
		       void *priv);
};
struct rtrs_srv_ctx *rtrs_srv_open(struct rtrs_srv_ops *ops, u16 port);
void rtrs_srv_close(struct rtrs_srv_ctx *ctx);
bool rtrs_srv_resp_rdma(struct rtrs_srv_op *id, int errno);
void rtrs_srv_set_sess_priv(struct rtrs_srv_sess *sess, void *priv);
int rtrs_srv_get_path_name(struct rtrs_srv_sess *sess, char *pathname,
			   size_t len);
int rtrs_srv_get_queue_depth(struct rtrs_srv_sess *sess);
int rtrs_addr_to_sockaddr(const char *str, size_t len, u16 port,
			  struct rtrs_addr *addr);
int sockaddr_to_str(const struct sockaddr *addr, char *buf, size_t len);
int rtrs_addr_to_str(const struct rtrs_addr *addr, char *buf, size_t len);
#endif
