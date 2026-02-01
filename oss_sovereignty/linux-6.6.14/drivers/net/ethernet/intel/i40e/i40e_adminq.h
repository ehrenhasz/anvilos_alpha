 
 

#ifndef _I40E_ADMINQ_H_
#define _I40E_ADMINQ_H_

#include "i40e_osdep.h"
#include "i40e_adminq_cmd.h"

#define I40E_ADMINQ_DESC(R, i)   \
	(&(((struct i40e_aq_desc *)((R).desc_buf.va))[i]))

#define I40E_ADMINQ_DESC_ALIGNMENT 4096

struct i40e_adminq_ring {
	struct i40e_virt_mem dma_head;	 
	struct i40e_dma_mem desc_buf;	 
	struct i40e_virt_mem cmd_buf;	 

	union {
		struct i40e_dma_mem *asq_bi;
		struct i40e_dma_mem *arq_bi;
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

 
struct i40e_asq_cmd_details {
	void *callback;  
	u64 cookie;
	u16 flags_ena;
	u16 flags_dis;
	bool async;
	bool postpone;
	struct i40e_aq_desc *wb_desc;
};

#define I40E_ADMINQ_DETAILS(R, i)   \
	(&(((struct i40e_asq_cmd_details *)((R).cmd_buf.va))[i]))

 
struct i40e_arq_event_info {
	struct i40e_aq_desc desc;
	u16 msg_len;
	u16 buf_len;
	u8 *msg_buf;
};

 
struct i40e_adminq_info {
	struct i40e_adminq_ring arq;     
	struct i40e_adminq_ring asq;     
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

	 
	enum i40e_admin_queue_err asq_last_status;
	enum i40e_admin_queue_err arq_last_status;
};

 
static inline int i40e_aq_rc_to_posix(int aq_ret, int aq_rc)
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

	 
	if (aq_ret == -EIO)
		return -EAGAIN;

	if (!((u32)aq_rc < (sizeof(aq_to_posix) / sizeof((aq_to_posix)[0]))))
		return -ERANGE;

	return aq_to_posix[aq_rc];
}

 
#define I40E_AQ_LARGE_BUF	512
#define I40E_ASQ_CMD_TIMEOUT	250000   

void i40e_fill_default_direct_cmd_desc(struct i40e_aq_desc *desc,
				       u16 opcode);

#endif  
