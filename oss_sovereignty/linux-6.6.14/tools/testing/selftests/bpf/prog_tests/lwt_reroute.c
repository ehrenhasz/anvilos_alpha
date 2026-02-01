

 
#include "lwt_helpers.h"
#include "network_helpers.h"
#include <linux/net_tstamp.h>

#define BPF_OBJECT            "test_lwt_reroute.bpf.o"
#define LOCAL_SRC             "10.0.0.1"
#define TEST_CIDR             "10.0.0.0/24"
#define XMIT_HOOK             "xmit"
#define XMIT_SECTION          "lwt_xmit"
#define NSEC_PER_SEC          1000000000ULL

 
static void ping_once(const char *ip)
{
	 
	SYS_NOFAIL("ping %s -c1 -W1 -s %d >/dev/null 2>&1",
		   ip, ICMP_PAYLOAD_SIZE);
}

 
static int overflow_fq(int snd_target, const char *target_ip)
{
	struct sockaddr_in addr = {
		.sin_family = AF_INET,
		.sin_port = htons(1234),
	};

	char data_buf[8];  
	char control_buf[CMSG_SPACE(sizeof(uint64_t))];
	struct iovec iov = {
		.iov_base = data_buf,
		.iov_len = sizeof(data_buf),
	};
	int err = -1;
	int s = -1;
	struct sock_txtime txtime_on = {
		.clockid = CLOCK_MONOTONIC,
		.flags = 0,
	};
	struct msghdr msg = {
		.msg_name = &addr,
		.msg_namelen = sizeof(addr),
		.msg_control = control_buf,
		.msg_controllen = sizeof(control_buf),
		.msg_iovlen = 1,
		.msg_iov = &iov,
	};
	struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msg);

	memset(data_buf, 0, sizeof(data_buf));

	s = socket(AF_INET, SOCK_DGRAM, 0);
	if (!ASSERT_GE(s, 0, "socket"))
		goto out;

	err = setsockopt(s, SOL_SOCKET, SO_TXTIME, &txtime_on, sizeof(txtime_on));
	if (!ASSERT_OK(err, "setsockopt(SO_TXTIME)"))
		goto out;

	err = inet_pton(AF_INET, target_ip, &addr.sin_addr);
	if (!ASSERT_EQ(err, 1, "inet_pton"))
		goto out;

	while (snd_target > 0) {
		struct timespec now;

		memset(control_buf, 0, sizeof(control_buf));
		cmsg->cmsg_type = SCM_TXTIME;
		cmsg->cmsg_level = SOL_SOCKET;
		cmsg->cmsg_len = CMSG_LEN(sizeof(uint64_t));

		err = clock_gettime(CLOCK_MONOTONIC, &now);
		if (!ASSERT_OK(err, "clock_gettime(CLOCK_MONOTONIC)")) {
			err = -1;
			goto out;
		}

		*(uint64_t *)CMSG_DATA(cmsg) = (now.tv_nsec + 1) * NSEC_PER_SEC +
					       now.tv_nsec;

		 
		sendmsg(s, &msg, MSG_NOSIGNAL);
		snd_target--;
	}

	 
	err = 0;

out:
	if (s >= 0)
		close(s);

	return err;
}

static int setup(const char *tun_dev)
{
	int target_index = -1;
	int tap_fd = -1;

	tap_fd = open_tuntap(tun_dev, false);
	if (!ASSERT_GE(tap_fd, 0, "open_tun"))
		return -1;

	target_index = if_nametoindex(tun_dev);
	if (!ASSERT_GE(target_index, 0, "if_nametoindex"))
		return -1;

	SYS(fail, "ip link add link_err type dummy");
	SYS(fail, "ip link set lo up");
	SYS(fail, "ip addr add dev lo " LOCAL_SRC "/32");
	SYS(fail, "ip link set link_err up");
	SYS(fail, "ip link set %s up", tun_dev);

	SYS(fail, "ip route add %s dev link_err encap bpf xmit obj %s sec lwt_xmit",
	    TEST_CIDR, BPF_OBJECT);

	SYS(fail, "ip rule add pref 100 from all fwmark %d lookup 100",
	    target_index);
	SYS(fail, "ip route add t 100 default dev %s", tun_dev);

	return tap_fd;

fail:
	if (tap_fd >= 0)
		close(tap_fd);
	return -1;
}

static void test_lwt_reroute_normal_xmit(void)
{
	const char *tun_dev = "tun0";
	int tun_fd = -1;
	int ifindex = -1;
	char ip[256];
	struct timeval timeo = {
		.tv_sec = 0,
		.tv_usec = 250000,
	};

	tun_fd = setup(tun_dev);
	if (!ASSERT_GE(tun_fd, 0, "setup_reroute"))
		return;

	ifindex = if_nametoindex(tun_dev);
	if (!ASSERT_GE(ifindex, 0, "if_nametoindex"))
		return;

	snprintf(ip, 256, "10.0.0.%d", ifindex);

	 
	ping_once(ip);

	if (!ASSERT_EQ(wait_for_packet(tun_fd, __expect_icmp_ipv4, &timeo), 1,
		       "wait_for_packet"))
		log_err("%s xmit", __func__);
}

 
static void test_lwt_reroute_qdisc_dropped(void)
{
	const char *tun_dev = "tun0";
	int tun_fd = -1;
	int ifindex = -1;
	char ip[256];

	tun_fd = setup(tun_dev);
	if (!ASSERT_GE(tun_fd, 0, "setup_reroute"))
		goto fail;

	SYS(fail, "tc qdisc replace dev %s root fq limit 5 flow_limit 5", tun_dev);

	ifindex = if_nametoindex(tun_dev);
	if (!ASSERT_GE(ifindex, 0, "if_nametoindex"))
		return;

	snprintf(ip, 256, "10.0.0.%d", ifindex);
	ASSERT_EQ(overflow_fq(10, ip), 0, "overflow_fq");

fail:
	if (tun_fd >= 0)
		close(tun_fd);
}

static void *test_lwt_reroute_run(void *arg)
{
	netns_delete();
	RUN_TEST(lwt_reroute_normal_xmit);
	RUN_TEST(lwt_reroute_qdisc_dropped);
	return NULL;
}

void test_lwt_reroute(void)
{
	pthread_t test_thread;
	int err;

	 
	err = pthread_create(&test_thread, NULL, &test_lwt_reroute_run, NULL);
	if (ASSERT_OK(err, "pthread_create"))
		ASSERT_OK(pthread_join(test_thread, NULL), "pthread_join");
}
