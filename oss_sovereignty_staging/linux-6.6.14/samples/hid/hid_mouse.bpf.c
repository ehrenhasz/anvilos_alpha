

#include "vmlinux.h"
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>
#include "hid_bpf_helpers.h"

SEC("fmod_ret/hid_bpf_device_event")
int BPF_PROG(hid_y_event, struct hid_bpf_ctx *hctx)
{
	s16 y;
	__u8 *data = hid_bpf_get_data(hctx, 0  , 9  );

	if (!data)
		return 0;  

	bpf_printk("event: size: %d", hctx->size);
	bpf_printk("incoming event: %02x %02x %02x",
		   data[0],
		   data[1],
		   data[2]);
	bpf_printk("                %02x %02x %02x",
		   data[3],
		   data[4],
		   data[5]);
	bpf_printk("                %02x %02x %02x",
		   data[6],
		   data[7],
		   data[8]);

	y = data[3] | (data[4] << 8);

	y = -y;

	data[3] = y & 0xFF;
	data[4] = (y >> 8) & 0xFF;

	bpf_printk("modified event: %02x %02x %02x",
		   data[0],
		   data[1],
		   data[2]);
	bpf_printk("                %02x %02x %02x",
		   data[3],
		   data[4],
		   data[5]);
	bpf_printk("                %02x %02x %02x",
		   data[6],
		   data[7],
		   data[8]);

	return 0;
}

SEC("fmod_ret/hid_bpf_device_event")
int BPF_PROG(hid_x_event, struct hid_bpf_ctx *hctx)
{
	s16 x;
	__u8 *data = hid_bpf_get_data(hctx, 0  , 9  );

	if (!data)
		return 0;  

	x = data[1] | (data[2] << 8);

	x = -x;

	data[1] = x & 0xFF;
	data[2] = (x >> 8) & 0xFF;
	return 0;
}

SEC("fmod_ret/hid_bpf_rdesc_fixup")
int BPF_PROG(hid_rdesc_fixup, struct hid_bpf_ctx *hctx)
{
	__u8 *data = hid_bpf_get_data(hctx, 0  , 4096  );

	if (!data)
		return 0;  

	bpf_printk("rdesc: %02x %02x %02x",
		   data[0],
		   data[1],
		   data[2]);
	bpf_printk("       %02x %02x %02x",
		   data[3],
		   data[4],
		   data[5]);
	bpf_printk("       %02x %02x %02x ...",
		   data[6],
		   data[7],
		   data[8]);

	 
	data[39] = 0x31;
	data[41] = 0x30;

	return 0;
}

char _license[] SEC("license") = "GPL";
