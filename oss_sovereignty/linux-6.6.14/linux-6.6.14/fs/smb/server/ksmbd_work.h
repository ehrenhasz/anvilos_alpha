#ifndef __KSMBD_WORK_H__
#define __KSMBD_WORK_H__
#include <linux/ctype.h>
#include <linux/workqueue.h>
struct ksmbd_conn;
struct ksmbd_session;
struct ksmbd_tree_connect;
enum {
	KSMBD_WORK_ACTIVE = 0,
	KSMBD_WORK_CANCELLED,
	KSMBD_WORK_CLOSED,
};
struct aux_read {
	void *buf;
	struct list_head entry;
};
struct ksmbd_work {
	struct ksmbd_conn               *conn;
	struct ksmbd_session            *sess;
	struct ksmbd_tree_connect       *tcon;
	void                            *request_buf;
	void                            *response_buf;
	struct list_head		aux_read_list;
	struct kvec			*iov;
	int				iov_alloc_cnt;
	int				iov_cnt;
	int				iov_idx;
	int                             next_smb2_rcv_hdr_off;
	int                             next_smb2_rsp_hdr_off;
	int                             curr_smb2_rsp_hdr_off;
	u64				compound_fid;
	u64				compound_pfid;
	u64				compound_sid;
	const struct cred		*saved_cred;
	unsigned int			credits_granted;
	unsigned int                    response_sz;
	void				*tr_buf;
	unsigned char			state;
	bool                            send_no_response:1;
	bool                            encrypted:1;
	bool                            asynchronous:1;
	bool                            need_invalidate_rkey:1;
	unsigned int                    remote_key;
	int                             async_id;
	void                            **cancel_argv;
	void                            (*cancel_fn)(void **argv);
	struct work_struct              work;
	struct list_head                request_entry;
	struct list_head                async_request_entry;
	struct list_head                fp_entry;
	struct list_head                interim_entry;
};
static inline void *ksmbd_resp_buf_next(struct ksmbd_work *work)
{
	return work->response_buf + work->next_smb2_rsp_hdr_off + 4;
}
static inline void *ksmbd_resp_buf_curr(struct ksmbd_work *work)
{
	return work->response_buf + work->curr_smb2_rsp_hdr_off + 4;
}
static inline void *ksmbd_req_buf_next(struct ksmbd_work *work)
{
	return work->request_buf + work->next_smb2_rcv_hdr_off + 4;
}
struct ksmbd_work *ksmbd_alloc_work_struct(void);
void ksmbd_free_work_struct(struct ksmbd_work *work);
void ksmbd_work_pool_destroy(void);
int ksmbd_work_pool_init(void);
int ksmbd_workqueue_init(void);
void ksmbd_workqueue_destroy(void);
bool ksmbd_queue_work(struct ksmbd_work *work);
int ksmbd_iov_pin_rsp_read(struct ksmbd_work *work, void *ib, int len,
			   void *aux_buf, unsigned int aux_size);
int ksmbd_iov_pin_rsp(struct ksmbd_work *work, void *ib, int len);
int allocate_interim_rsp_buf(struct ksmbd_work *work);
#endif  
