 

 
#include <string.h>
#include <linux/bpf.h>
#include <linux/ipv6.h>
#include <linux/version.h>
#include <sys/socket.h>
#include <bpf/bpf_helpers.h>

#define _(P) ({typeof(P) val = 0; bpf_probe_read_kernel(&val, sizeof(val), &P); val;})
#define TCP_ESTATS_MAGIC 0xBAADBEEF

 
typedef __u32 __bitwise __portpair;
typedef __u64 __bitwise __addrpair;

struct sock_common {
	unsigned short		skc_family;
	union {
		__addrpair	skc_addrpair;
		struct {
			__be32	skc_daddr;
			__be32	skc_rcv_saddr;
		};
	};
	union {
		__portpair	skc_portpair;
		struct {
			__be16	skc_dport;
			__u16	skc_num;
		};
	};
	struct in6_addr		skc_v6_daddr;
	struct in6_addr		skc_v6_rcv_saddr;
};

struct sock {
	struct sock_common	__sk_common;
#define sk_family		__sk_common.skc_family
#define sk_v6_daddr		__sk_common.skc_v6_daddr
#define sk_v6_rcv_saddr		__sk_common.skc_v6_rcv_saddr
};

struct inet_sock {
	struct sock		sk;
#define inet_daddr		sk.__sk_common.skc_daddr
#define inet_dport		sk.__sk_common.skc_dport
	__be32			inet_saddr;
	__be16			inet_sport;
};

struct pt_regs {
	long di;
};

static inline struct inet_sock *inet_sk(const struct sock *sk)
{
	return (struct inet_sock *)sk;
}

 
enum tcp_estats_addrtype {
	TCP_ESTATS_ADDRTYPE_IPV4 = 1,
	TCP_ESTATS_ADDRTYPE_IPV6 = 2
};

enum tcp_estats_event_type {
	TCP_ESTATS_ESTABLISH,
	TCP_ESTATS_PERIODIC,
	TCP_ESTATS_TIMEOUT,
	TCP_ESTATS_RETRANSMIT_TIMEOUT,
	TCP_ESTATS_RETRANSMIT_OTHER,
	TCP_ESTATS_SYN_RETRANSMIT,
	TCP_ESTATS_SYNACK_RETRANSMIT,
	TCP_ESTATS_TERM,
	TCP_ESTATS_TX_RESET,
	TCP_ESTATS_RX_RESET,
	TCP_ESTATS_WRITE_TIMEOUT,
	TCP_ESTATS_CONN_TIMEOUT,
	TCP_ESTATS_ACK_LATENCY,
	TCP_ESTATS_NEVENTS,
};

struct tcp_estats_event {
	int pid;
	int cpu;
	unsigned long ts;
	unsigned int magic;
	enum tcp_estats_event_type event_type;
};

 
struct tcp_estats_conn_id {
	unsigned int localaddressType;
	struct {
		unsigned char data[16];
	} localaddress;
	struct {
		unsigned char data[16];
	} remaddress;
	unsigned short    localport;
	unsigned short    remport;
} __attribute__((__packed__));

struct tcp_estats_basic_event {
	struct tcp_estats_event event;
	struct tcp_estats_conn_id conn_id;
};

struct {
	__uint(type, BPF_MAP_TYPE_HASH);
	__uint(max_entries, 1024);
	__type(key, __u32);
	__type(value, struct tcp_estats_basic_event);
} ev_record_map SEC(".maps");

struct dummy_tracepoint_args {
	unsigned long long pad;
	struct sock *sock;
};

static __always_inline void tcp_estats_ev_init(struct tcp_estats_event *event,
					       enum tcp_estats_event_type type)
{
	event->magic = TCP_ESTATS_MAGIC;
	event->ts = bpf_ktime_get_ns();
	event->event_type = type;
}

static __always_inline void unaligned_u32_set(unsigned char *to, __u8 *from)
{
	to[0] = _(from[0]);
	to[1] = _(from[1]);
	to[2] = _(from[2]);
	to[3] = _(from[3]);
}

static __always_inline void conn_id_ipv4_init(struct tcp_estats_conn_id *conn_id,
					      __be32 *saddr, __be32 *daddr)
{
	conn_id->localaddressType = TCP_ESTATS_ADDRTYPE_IPV4;

	unaligned_u32_set(conn_id->localaddress.data, (__u8 *)saddr);
	unaligned_u32_set(conn_id->remaddress.data, (__u8 *)daddr);
}

static __always_inline void conn_id_ipv6_init(struct tcp_estats_conn_id *conn_id,
					      __be32 *saddr, __be32 *daddr)
{
	conn_id->localaddressType = TCP_ESTATS_ADDRTYPE_IPV6;

	unaligned_u32_set(conn_id->localaddress.data, (__u8 *)saddr);
	unaligned_u32_set(conn_id->localaddress.data + sizeof(__u32),
			  (__u8 *)(saddr + 1));
	unaligned_u32_set(conn_id->localaddress.data + sizeof(__u32) * 2,
			  (__u8 *)(saddr + 2));
	unaligned_u32_set(conn_id->localaddress.data + sizeof(__u32) * 3,
			  (__u8 *)(saddr + 3));

	unaligned_u32_set(conn_id->remaddress.data,
			  (__u8 *)(daddr));
	unaligned_u32_set(conn_id->remaddress.data + sizeof(__u32),
			  (__u8 *)(daddr + 1));
	unaligned_u32_set(conn_id->remaddress.data + sizeof(__u32) * 2,
			  (__u8 *)(daddr + 2));
	unaligned_u32_set(conn_id->remaddress.data + sizeof(__u32) * 3,
			  (__u8 *)(daddr + 3));
}

static __always_inline void tcp_estats_conn_id_init(struct tcp_estats_conn_id *conn_id,
						    struct sock *sk)
{
	conn_id->localport = _(inet_sk(sk)->inet_sport);
	conn_id->remport = _(inet_sk(sk)->inet_dport);

	if (_(sk->sk_family) == AF_INET6)
		conn_id_ipv6_init(conn_id,
				  sk->sk_v6_rcv_saddr.s6_addr32,
				  sk->sk_v6_daddr.s6_addr32);
	else
		conn_id_ipv4_init(conn_id,
				  &inet_sk(sk)->inet_saddr,
				  &inet_sk(sk)->inet_daddr);
}

static __always_inline void tcp_estats_init(struct sock *sk,
					    struct tcp_estats_event *event,
					    struct tcp_estats_conn_id *conn_id,
					    enum tcp_estats_event_type type)
{
	tcp_estats_ev_init(event, type);
	tcp_estats_conn_id_init(conn_id, sk);
}

static __always_inline void send_basic_event(struct sock *sk,
					     enum tcp_estats_event_type type)
{
	struct tcp_estats_basic_event ev;
	__u32 key = bpf_get_prandom_u32();

	memset(&ev, 0, sizeof(ev));
	tcp_estats_init(sk, &ev.event, &ev.conn_id, type);
	bpf_map_update_elem(&ev_record_map, &key, &ev, BPF_ANY);
}

SEC("tp/dummy/tracepoint")
int _dummy_tracepoint(struct dummy_tracepoint_args *arg)
{
	if (!arg->sock)
		return 0;

	send_basic_event(arg->sock, TCP_ESTATS_TX_RESET);
	return 0;
}

char _license[] SEC("license") = "GPL";
