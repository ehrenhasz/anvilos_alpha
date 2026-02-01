 
 

#ifndef __KSMBD_IDA_MANAGEMENT_H__
#define __KSMBD_IDA_MANAGEMENT_H__

#include <linux/slab.h>
#include <linux/idr.h>

 
int ksmbd_acquire_smb2_tid(struct ida *ida);

 
int ksmbd_acquire_smb2_uid(struct ida *ida);
int ksmbd_acquire_async_msg_id(struct ida *ida);

int ksmbd_acquire_id(struct ida *ida);

void ksmbd_release_id(struct ida *ida, int id);
#endif  
