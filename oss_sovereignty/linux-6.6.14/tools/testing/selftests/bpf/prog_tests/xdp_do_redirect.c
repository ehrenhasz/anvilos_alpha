
#include <test_progs.h>
#include <network_helpers.h>
#include <net/if.h>
#include <linux/if_ether.h>
#include <linux/if_packet.h>
#include <linux/if_link.h>
#include <linux/ipv6.h>
#include <linux/in6.h>
#include <linux/udp.h>
#include <bpf/bpf_endian.h>
#include <uapi/linux/netdev.h>
#include "test_xdp_do_redirect.skel.h"

struct udp_packet {
	struct ethhdr eth;
	struct ipv6hdr iph;
	struct udphdr udp;
	__u8 payload[64 - sizeof(struct udphdr)
		     - sizeof(struct ethhdr) - sizeof(struct ipv6hdr)];
} __packed;

static struct udp_packet pkt_udp = {
	.eth.h_proto = __bpf_constant_htons(ETH_P_IPV6),
	.eth.h_dest = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55},
	.eth.h_source = {0x66, 0x77, 0x88, 0x99, 0xaa, 0xbb},
	.iph.version = 6,
	.iph.nexthdr = IPPROTO_UDP,
	.iph.payload_len = bpf_htons(sizeof(struct udp_packet)
				     - offsetof(struct udp_packet, udp)),
	.iph.hop_limit = 2,
	.iph.saddr.s6_addr16 = {bpf_htons(0xfc00), 0, 0, 0, 0, 0, 0, bpf_htons(1)},
	.iph.daddr.s6_addr16 = {bpf_htons(0xfc00), 0, 0, 0, 0, 0, 0, bpf_htons(2)},
	.udp.source = bpf_htons(1),
	.udp.dest = bpf_htons(1),
	.udp.len = bpf_htons(sizeof(struct udp_packet)
			     - offsetof(struct udp_packet, udp)),
	.payload = {0x42},  
};

static int attach_tc_prog(struct bpf_tc_hook *hook, int fd)
{
	DECLARE_LIBBPF_OPTS(bpf_tc_opts, opts, .handle = 1, .priority = 1, .prog_fd = fd);
	int ret;

	ret = bpf_tc_hook_create(hook);
	if (!ASSERT_OK(ret, "create tc hook"))
		return ret;

	ret = bpf_tc_attach(hook, &opts);
	if (!ASSERT_OK(ret, "bpf_tc_attach")) {
		bpf_tc_hook_destroy(hook);
		return ret;
	}

	return 0;
}

 
#if defined(__s390x__)
#define MAX_PKT_SIZE 3216
#else
#define MAX_PKT_SIZE 3408
#endif
static void test_max_pkt_size(int fd)
{
	char data[MAX_PKT_SIZE + 1] = {};
	int err;
	DECLARE_LIBBPF_OPTS(bpf_test_run_opts, opts,
			    .data_in = &data,
			    .data_size_in = MAX_PKT_SIZE,
			    .flags = BPF_F_TEST_XDP_LIVE_FRAMES,
			    .repeat = 1,
		);
	err = bpf_prog_test_run_opts(fd, &opts);
	ASSERT_OK(err, "prog_run_max_size");

	opts.data_size_in += 1;
	err = bpf_prog_test_run_opts(fd, &opts);
	ASSERT_EQ(err, -EINVAL, "prog_run_too_big");
}

#define NUM_PKTS 10000
void test_xdp_do_redirect(void)
{
	int err, xdp_prog_fd, tc_prog_fd, ifindex_src, ifindex_dst;
	char data[sizeof(pkt_udp) + sizeof(__u64)];
	struct test_xdp_do_redirect *skel = NULL;
	struct nstoken *nstoken = NULL;
	struct bpf_link *link;
	LIBBPF_OPTS(bpf_xdp_query_opts, query_opts);
	struct xdp_md ctx_in = { .data = sizeof(__u64),
				 .data_end = sizeof(data) };
	DECLARE_LIBBPF_OPTS(bpf_test_run_opts, opts,
			    .data_in = &data,
			    .data_size_in = sizeof(data),
			    .ctx_in = &ctx_in,
			    .ctx_size_in = sizeof(ctx_in),
			    .flags = BPF_F_TEST_XDP_LIVE_FRAMES,
			    .repeat = NUM_PKTS,
			    .batch_size = 64,
		);
	DECLARE_LIBBPF_OPTS(bpf_tc_hook, tc_hook,
			    .attach_point = BPF_TC_INGRESS);

	memcpy(&data[sizeof(__u64)], &pkt_udp, sizeof(pkt_udp));
	*((__u32 *)data) = 0x42;  
	*((__u32 *)data + 4) = 0;

	skel = test_xdp_do_redirect__open();
	if (!ASSERT_OK_PTR(skel, "skel"))
		return;

	 
	SYS(out, "ip netns add testns");
	nstoken = open_netns("testns");
	if (!ASSERT_OK_PTR(nstoken, "setns"))
		goto out;

	SYS(out, "ip link add veth_src type veth peer name veth_dst");
	SYS(out, "ip link set dev veth_src address 00:11:22:33:44:55");
	SYS(out, "ip link set dev veth_dst address 66:77:88:99:aa:bb");
	SYS(out, "ip link set dev veth_src up");
	SYS(out, "ip link set dev veth_dst up");
	SYS(out, "ip addr add dev veth_src fc00::1/64");
	SYS(out, "ip addr add dev veth_dst fc00::2/64");
	SYS(out, "ip neigh add fc00::2 dev veth_src lladdr 66:77:88:99:aa:bb");

	 
	SYS(out, "sysctl -qw net.ipv6.conf.all.forwarding=1");

	ifindex_src = if_nametoindex("veth_src");
	ifindex_dst = if_nametoindex("veth_dst");
	if (!ASSERT_NEQ(ifindex_src, 0, "ifindex_src") ||
	    !ASSERT_NEQ(ifindex_dst, 0, "ifindex_dst"))
		goto out;

	 
	err = bpf_xdp_query(ifindex_src, XDP_FLAGS_DRV_MODE, &query_opts);
	if (!ASSERT_OK(err, "veth_src bpf_xdp_query"))
		goto out;

	if (!ASSERT_EQ(query_opts.feature_flags,
		       NETDEV_XDP_ACT_BASIC | NETDEV_XDP_ACT_REDIRECT |
		       NETDEV_XDP_ACT_RX_SG,
		       "veth_src query_opts.feature_flags"))
		goto out;

	err = bpf_xdp_query(ifindex_dst, XDP_FLAGS_DRV_MODE, &query_opts);
	if (!ASSERT_OK(err, "veth_dst bpf_xdp_query"))
		goto out;

	if (!ASSERT_EQ(query_opts.feature_flags,
		       NETDEV_XDP_ACT_BASIC | NETDEV_XDP_ACT_REDIRECT |
		       NETDEV_XDP_ACT_RX_SG,
		       "veth_dst query_opts.feature_flags"))
		goto out;

	 
	SYS(out, "ethtool -K veth_src gro on");
	SYS(out, "ethtool -K veth_dst gro on");

	err = bpf_xdp_query(ifindex_src, XDP_FLAGS_DRV_MODE, &query_opts);
	if (!ASSERT_OK(err, "veth_src bpf_xdp_query gro on"))
		goto out;

	if (!ASSERT_EQ(query_opts.feature_flags,
		       NETDEV_XDP_ACT_BASIC | NETDEV_XDP_ACT_REDIRECT |
		       NETDEV_XDP_ACT_NDO_XMIT | NETDEV_XDP_ACT_RX_SG |
		       NETDEV_XDP_ACT_NDO_XMIT_SG,
		       "veth_src query_opts.feature_flags gro on"))
		goto out;

	err = bpf_xdp_query(ifindex_dst, XDP_FLAGS_DRV_MODE, &query_opts);
	if (!ASSERT_OK(err, "veth_dst bpf_xdp_query gro on"))
		goto out;

	if (!ASSERT_EQ(query_opts.feature_flags,
		       NETDEV_XDP_ACT_BASIC | NETDEV_XDP_ACT_REDIRECT |
		       NETDEV_XDP_ACT_NDO_XMIT | NETDEV_XDP_ACT_RX_SG |
		       NETDEV_XDP_ACT_NDO_XMIT_SG,
		       "veth_dst query_opts.feature_flags gro on"))
		goto out;

	memcpy(skel->rodata->expect_dst, &pkt_udp.eth.h_dest, ETH_ALEN);
	skel->rodata->ifindex_out = ifindex_src;  
	skel->rodata->ifindex_in = ifindex_src;
	ctx_in.ingress_ifindex = ifindex_src;
	tc_hook.ifindex = ifindex_src;

	if (!ASSERT_OK(test_xdp_do_redirect__load(skel), "load"))
		goto out;

	link = bpf_program__attach_xdp(skel->progs.xdp_count_pkts, ifindex_dst);
	if (!ASSERT_OK_PTR(link, "prog_attach"))
		goto out;
	skel->links.xdp_count_pkts = link;

	tc_prog_fd = bpf_program__fd(skel->progs.tc_count_pkts);
	if (attach_tc_prog(&tc_hook, tc_prog_fd))
		goto out;

	xdp_prog_fd = bpf_program__fd(skel->progs.xdp_redirect);
	err = bpf_prog_test_run_opts(xdp_prog_fd, &opts);
	if (!ASSERT_OK(err, "prog_run"))
		goto out_tc;

	 
	kern_sync_rcu();

	 
	ASSERT_EQ(skel->bss->pkts_seen_xdp, 2, "pkt_count_xdp");
	ASSERT_EQ(skel->bss->pkts_seen_zero, 2, "pkt_count_zero");
	ASSERT_EQ(skel->bss->pkts_seen_tc, NUM_PKTS - 2, "pkt_count_tc");

	test_max_pkt_size(bpf_program__fd(skel->progs.xdp_count_pkts));

out_tc:
	bpf_tc_hook_destroy(&tc_hook);
out:
	if (nstoken)
		close_netns(nstoken);
	SYS_NOFAIL("ip netns del testns");
	test_xdp_do_redirect__destroy(skel);
}
