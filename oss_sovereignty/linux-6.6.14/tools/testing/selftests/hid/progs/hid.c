
 
#include "vmlinux.h"
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>
#include "hid_bpf_helpers.h"

char _license[] SEC("license") = "GPL";

struct attach_prog_args {
	int prog_fd;
	unsigned int hid;
	int retval;
	int insert_head;
};

__u64 callback_check = 52;
__u64 callback2_check = 52;

SEC("?fmod_ret/hid_bpf_device_event")
int BPF_PROG(hid_first_event, struct hid_bpf_ctx *hid_ctx)
{
	__u8 *rw_data = hid_bpf_get_data(hid_ctx, 0  , 3  );

	if (!rw_data)
		return 0;  

	callback_check = rw_data[1];

	rw_data[2] = rw_data[1] + 5;

	return hid_ctx->size;
}

SEC("?fmod_ret/hid_bpf_device_event")
int BPF_PROG(hid_second_event, struct hid_bpf_ctx *hid_ctx)
{
	__u8 *rw_data = hid_bpf_get_data(hid_ctx, 0  , 4  );

	if (!rw_data)
		return 0;  

	rw_data[3] = rw_data[2] + 5;

	return hid_ctx->size;
}

SEC("?fmod_ret/hid_bpf_device_event")
int BPF_PROG(hid_change_report_id, struct hid_bpf_ctx *hid_ctx)
{
	__u8 *rw_data = hid_bpf_get_data(hid_ctx, 0  , 3  );

	if (!rw_data)
		return 0;  

	rw_data[0] = 2;

	return 9;
}

SEC("syscall")
int attach_prog(struct attach_prog_args *ctx)
{
	ctx->retval = hid_bpf_attach_prog(ctx->hid,
					  ctx->prog_fd,
					  ctx->insert_head ? HID_BPF_FLAG_INSERT_HEAD :
							     HID_BPF_FLAG_NONE);
	return 0;
}

struct hid_hw_request_syscall_args {
	 
	__u8 data[10];
	unsigned int hid;
	int retval;
	size_t size;
	enum hid_report_type type;
	__u8 request_type;
};

SEC("syscall")
int hid_user_raw_request(struct hid_hw_request_syscall_args *args)
{
	struct hid_bpf_ctx *ctx;
	const size_t size = args->size;
	int i, ret = 0;

	if (size > sizeof(args->data))
		return -7;  

	ctx = hid_bpf_allocate_context(args->hid);
	if (!ctx)
		return -1;  

	ret = hid_bpf_hw_request(ctx,
				 args->data,
				 size,
				 args->type,
				 args->request_type);
	args->retval = ret;

	hid_bpf_release_context(ctx);

	return 0;
}

static const __u8 rdesc[] = {
	0x05, 0x01,				 
	0x09, 0x32,				 
	0x95, 0x01,				 
	0x81, 0x06,				 

	0x06, 0x00, 0xff,			 
	0x19, 0x01,				 
	0x29, 0x03,				 
	0x15, 0x00,				 
	0x25, 0x01,				 
	0x95, 0x03,				 
	0x75, 0x01,				 
	0x91, 0x02,				 
	0x95, 0x01,				 
	0x75, 0x05,				 
	0x91, 0x01,				 

	0x06, 0x00, 0xff,			 
	0x19, 0x06,				 
	0x29, 0x08,				 
	0x15, 0x00,				 
	0x25, 0x01,				 
	0x95, 0x03,				 
	0x75, 0x01,				 
	0xb1, 0x02,				 
	0x95, 0x01,				 
	0x75, 0x05,				 
	0x91, 0x01,				 

	0xc0,				 
	0xc0,			 
};

SEC("?fmod_ret/hid_bpf_rdesc_fixup")
int BPF_PROG(hid_rdesc_fixup, struct hid_bpf_ctx *hid_ctx)
{
	__u8 *data = hid_bpf_get_data(hid_ctx, 0  , 4096  );

	if (!data)
		return 0;  

	callback2_check = data[4];

	 
	__builtin_memcpy(&data[73], rdesc, sizeof(rdesc));

	 
	data[4] = 0x42;

	return sizeof(rdesc) + 73;
}

SEC("?fmod_ret/hid_bpf_device_event")
int BPF_PROG(hid_test_insert1, struct hid_bpf_ctx *hid_ctx)
{
	__u8 *data = hid_bpf_get_data(hid_ctx, 0  , 4  );

	if (!data)
		return 0;  

	 
	if (data[2] || data[3])
		return -1;

	data[1] = 1;

	return 0;
}

SEC("?fmod_ret/hid_bpf_device_event")
int BPF_PROG(hid_test_insert2, struct hid_bpf_ctx *hid_ctx)
{
	__u8 *data = hid_bpf_get_data(hid_ctx, 0  , 4  );

	if (!data)
		return 0;  

	 
	if (!data[1] || data[3])
		return -1;

	data[2] = 2;

	return 0;
}

SEC("?fmod_ret/hid_bpf_device_event")
int BPF_PROG(hid_test_insert3, struct hid_bpf_ctx *hid_ctx)
{
	__u8 *data = hid_bpf_get_data(hid_ctx, 0  , 4  );

	if (!data)
		return 0;  

	 
	if (!data[1] || !data[2])
		return -1;

	data[3] = 3;

	return 0;
}
