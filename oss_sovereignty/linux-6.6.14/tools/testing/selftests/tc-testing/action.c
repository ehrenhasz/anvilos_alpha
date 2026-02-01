 

#include <linux/bpf.h>
#include <linux/pkt_cls.h>

__attribute__((section("action-ok"),used)) int action_ok(struct __sk_buff *s)
{
	return TC_ACT_OK;
}

__attribute__((section("action-ko"),used)) int action_ko(struct __sk_buff *s)
{
	s->data = 0x0;
	return TC_ACT_OK;
}

char _license[] __attribute__((section("license"),used)) = "GPL";
