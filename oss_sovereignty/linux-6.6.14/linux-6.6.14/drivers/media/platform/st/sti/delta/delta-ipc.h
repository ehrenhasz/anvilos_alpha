#ifndef DELTA_IPC_H
#define DELTA_IPC_H
int delta_ipc_init(struct delta_dev *delta);
void delta_ipc_exit(struct delta_dev *delta);
int delta_ipc_open(struct delta_ctx *ctx, const char *name,
		   struct delta_ipc_param *param, u32 ipc_buf_size,
		   struct delta_buf **ipc_buf, void **hdl);
int delta_ipc_set_stream(void *hdl, struct delta_ipc_param *param);
int delta_ipc_decode(void *hdl, struct delta_ipc_param *param,
		     struct delta_ipc_param *status);
void delta_ipc_close(void *hdl);
#endif  
