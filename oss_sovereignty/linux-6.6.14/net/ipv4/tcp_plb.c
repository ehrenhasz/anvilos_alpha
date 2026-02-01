 

#include <net/tcp.h>

 
void tcp_plb_update_state(const struct sock *sk, struct tcp_plb_state *plb,
			  const int cong_ratio)
{
	struct net *net = sock_net(sk);

	if (!READ_ONCE(net->ipv4.sysctl_tcp_plb_enabled))
		return;

	if (cong_ratio >= 0) {
		if (cong_ratio < READ_ONCE(net->ipv4.sysctl_tcp_plb_cong_thresh))
			plb->consec_cong_rounds = 0;
		else if (plb->consec_cong_rounds <
			 READ_ONCE(net->ipv4.sysctl_tcp_plb_rehash_rounds))
			plb->consec_cong_rounds++;
	}
}
EXPORT_SYMBOL_GPL(tcp_plb_update_state);

 
void tcp_plb_check_rehash(struct sock *sk, struct tcp_plb_state *plb)
{
	struct net *net = sock_net(sk);
	u32 max_suspend;
	bool forced_rehash = false, idle_rehash = false;

	if (!READ_ONCE(net->ipv4.sysctl_tcp_plb_enabled))
		return;

	forced_rehash = plb->consec_cong_rounds >=
			READ_ONCE(net->ipv4.sysctl_tcp_plb_rehash_rounds);
	 
	idle_rehash = READ_ONCE(net->ipv4.sysctl_tcp_plb_idle_rehash_rounds) &&
		      !tcp_sk(sk)->packets_out &&
		      plb->consec_cong_rounds >=
		      READ_ONCE(net->ipv4.sysctl_tcp_plb_idle_rehash_rounds);

	if (!forced_rehash && !idle_rehash)
		return;

	 
	max_suspend = 2 * READ_ONCE(net->ipv4.sysctl_tcp_plb_suspend_rto_sec) * HZ;
	if (plb->pause_until &&
	    (!before(tcp_jiffies32, plb->pause_until) ||
	     before(tcp_jiffies32 + max_suspend, plb->pause_until)))
		plb->pause_until = 0;

	if (plb->pause_until)
		return;

	sk_rethink_txhash(sk);
	plb->consec_cong_rounds = 0;
	tcp_sk(sk)->plb_rehash++;
	NET_INC_STATS(sock_net(sk), LINUX_MIB_TCPPLBREHASH);
}
EXPORT_SYMBOL_GPL(tcp_plb_check_rehash);

 
void tcp_plb_update_state_upon_rto(struct sock *sk, struct tcp_plb_state *plb)
{
	struct net *net = sock_net(sk);
	u32 pause;

	if (!READ_ONCE(net->ipv4.sysctl_tcp_plb_enabled))
		return;

	pause = READ_ONCE(net->ipv4.sysctl_tcp_plb_suspend_rto_sec) * HZ;
	pause += get_random_u32_below(pause);
	plb->pause_until = tcp_jiffies32 + pause;

	 
	plb->consec_cong_rounds = 0;
}
EXPORT_SYMBOL_GPL(tcp_plb_update_state_upon_rto);
