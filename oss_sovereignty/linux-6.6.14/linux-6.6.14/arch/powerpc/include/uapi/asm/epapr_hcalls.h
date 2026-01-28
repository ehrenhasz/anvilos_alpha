#ifndef _UAPI_ASM_POWERPC_EPAPR_HCALLS_H
#define _UAPI_ASM_POWERPC_EPAPR_HCALLS_H
#define EV_BYTE_CHANNEL_SEND		1
#define EV_BYTE_CHANNEL_RECEIVE		2
#define EV_BYTE_CHANNEL_POLL		3
#define EV_INT_SET_CONFIG		4
#define EV_INT_GET_CONFIG		5
#define EV_INT_SET_MASK			6
#define EV_INT_GET_MASK			7
#define EV_INT_IACK			9
#define EV_INT_EOI			10
#define EV_INT_SEND_IPI			11
#define EV_INT_SET_TASK_PRIORITY	12
#define EV_INT_GET_TASK_PRIORITY	13
#define EV_DOORBELL_SEND		14
#define EV_MSGSND			15
#define EV_IDLE				16
#define EV_LOCAL_VENDOR_ID		0	 
#define EV_EPAPR_VENDOR_ID		1
#define EV_FSL_VENDOR_ID		2	 
#define EV_IBM_VENDOR_ID		3	 
#define EV_GHS_VENDOR_ID		4	 
#define EV_ENEA_VENDOR_ID		5	 
#define EV_WR_VENDOR_ID			6	 
#define EV_AMCC_VENDOR_ID		7	 
#define EV_KVM_VENDOR_ID		42	 
#define EV_BYTE_CHANNEL_MAX_BYTES	16
#define _EV_HCALL_TOKEN(id, num) (((id) << 16) | (num))
#define EV_HCALL_TOKEN(hcall_num) _EV_HCALL_TOKEN(EV_EPAPR_VENDOR_ID, hcall_num)
#define EV_SUCCESS		0
#define EV_EPERM		1	 
#define EV_ENOENT		2	 
#define EV_EIO			3	 
#define EV_EAGAIN		4	 
#define EV_ENOMEM		5	 
#define EV_EFAULT		6	 
#define EV_ENODEV		7	 
#define EV_EINVAL		8	 
#define EV_INTERNAL		9	 
#define EV_CONFIG		10	 
#define EV_INVALID_STATE	11	 
#define EV_UNIMPLEMENTED	12	 
#define EV_BUFFER_OVERFLOW	13	 
#endif  
