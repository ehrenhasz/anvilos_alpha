#ifndef _IAVF_ADMINQ_H_
#define _IAVF_ADMINQ_H_
#include "iavf_osdep.h"
#include "iavf_status.h"
#include "iavf_adminq_cmd.h"
#define IAVF_ADMINQ_DESC(R, i)   \
	(&(((struct iavf_aq_desc *)((R).desc_buf.va))[i]))
#define IAVF_ADMINQ_DESC_ALIGNMENT 4096
struct iavf_adminq_ring {
	struct iavf_virt_mem dma_head;	 
	struct iavf_dma_mem desc_buf;	 
	struct iavf_virt_mem cmd_buf;	 
	union {
		struct iavf_dma_mem *asq_bi;
		struct iavf_dma_mem *arq_bi;
	} r;
	u16 count;		 
	u16 rx_buf_len;		 
	u16 next_to_use;
	u16 next_to_clean;
	u32 head;
	u32 tail;
	u32 len;
	u32 bah;
	u32 bal;
};
struct iavf_asq_cmd_details {
	void *callback;  
	u64 cookie;
	u16 flags_ena;
	u16 flags_dis;
	bool async;
	bool postpone;
	struct iavf_aq_desc *wb_desc;
};
#define IAVF_ADMINQ_DETAILS(R, i)   \
	(&(((struct iavf_asq_cmd_details *)((R).cmd_buf.va))[i]))
struct iavf_arq_event_info {
	struct iavf_aq_desc desc;
	u16 msg_len;
	u16 buf_len;
	u8 *msg_buf;
};
struct iavf_adminq_info {
	struct iavf_adminq_ring arq;     
	struct iavf_adminq_ring asq;     
	u32 asq_cmd_timeout;             
	u16 num_arq_entries;             
	u16 num_asq_entries;             
	u16 arq_buf_size;                
	u16 asq_buf_size;                
	u16 fw_maj_ver;                  
	u16 fw_min_ver;                  
	u32 fw_build;                    
	u16 api_maj_ver;                 
	u16 api_min_ver;                 
	struct mutex asq_mutex;  
	struct mutex arq_mutex;  
	enum iavf_admin_queue_err asq_last_status;
	enum iavf_admin_queue_err arq_last_status;
};
static inline int iavf_aq_rc_to_posix(int aq_ret, int aq_rc)
{
	int aq_to_posix[] = {
		0,            
		-EPERM,       
		-ENOENT,      
		-ESRCH,       
		-EINTR,       
		-EIO,         
		-ENXIO,       
		-E2BIG,       
		-EAGAIN,      
		-ENOMEM,      
		-EACCES,      
		-EFAULT,      
		-EBUSY,       
		-EEXIST,      
		-EINVAL,      
		-ENOTTY,      
		-ENOSPC,      
		-ENOSYS,      
		-ERANGE,      
		-EPIPE,       
		-ESPIPE,      
		-EROFS,       
		-EFBIG,       
	};
	if (aq_ret == IAVF_ERR_ADMIN_QUEUE_TIMEOUT)
		return -EAGAIN;
	if (!((u32)aq_rc < (sizeof(aq_to_posix) / sizeof((aq_to_posix)[0]))))
		return -ERANGE;
	return aq_to_posix[aq_rc];
}
#define IAVF_AQ_LARGE_BUF	512
#define IAVF_ASQ_CMD_TIMEOUT	250000   
void iavf_fill_default_direct_cmd_desc(struct iavf_aq_desc *desc, u16 opcode);
#endif  
