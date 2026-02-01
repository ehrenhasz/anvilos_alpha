 
#include "vmlinux.h"
#include "net_shared.h"
#include <linux/version.h>
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_core_read.h>
#include <bpf/bpf_tracing.h>

 
SEC("kprobe.multi/__netif_receive_skb_core*")
int bpf_prog1(struct pt_regs *ctx)
{
	 
	char devname[IFNAMSIZ];
	struct net_device *dev;
	struct sk_buff *skb;
	int len;

	bpf_core_read(&skb, sizeof(skb), (void *)PT_REGS_PARM1(ctx));
	dev = BPF_CORE_READ(skb, dev);
	len = BPF_CORE_READ(skb, len);

	BPF_CORE_READ_STR_INTO(&devname, dev, name);

	if (devname[0] == 'l' && devname[1] == 'o') {
		char fmt[] = "skb %p len %d\n";
		 
		bpf_trace_printk(fmt, sizeof(fmt), skb, len);
	}

	return 0;
}

char _license[] SEC("license") = "GPL";
u32 _version SEC("version") = LINUX_VERSION_CODE;
