#ifndef _HV_UTILS_TRANSPORT_H
#define _HV_UTILS_TRANSPORT_H
#include <linux/connector.h>
#include <linux/miscdevice.h>
enum hvutil_transport_mode {
	HVUTIL_TRANSPORT_INIT = 0,
	HVUTIL_TRANSPORT_NETLINK,
	HVUTIL_TRANSPORT_CHARDEV,
	HVUTIL_TRANSPORT_DESTROY,
};
struct hvutil_transport {
	int mode;                            
	struct file_operations fops;         
	struct miscdevice mdev;              
	struct cb_id cn_id;                  
	struct list_head list;               
	int (*on_msg)(void *, int);          
	void (*on_reset)(void);              
	void (*on_read)(void);               
	u8 *outmsg;                          
	int outmsg_len;                      
	wait_queue_head_t outmsg_q;          
	struct mutex lock;                   
	struct completion release;           
};
struct hvutil_transport *hvutil_transport_init(const char *name,
					       u32 cn_idx, u32 cn_val,
					       int (*on_msg)(void *, int),
					       void (*on_reset)(void));
int hvutil_transport_send(struct hvutil_transport *hvt, void *msg, int len,
			  void (*on_read_cb)(void));
void hvutil_transport_destroy(struct hvutil_transport *hvt);
#endif  
