ksft_skip=4
IP0=172.30.30.1
IP1=172.30.30.2
PFXL=30
ret=0
sfx=$(mktemp -u "XXXXXXXX")
ns0="ns0-$sfx"
ns1="ns1-$sfx"
cleanup()
{
	ip netns pids $ns0 | xargs kill 2>/dev/null
	ip netns pids $ns1 | xargs kill 2>/dev/null
	ip netns del $ns0 $ns1
}
nft --version > /dev/null 2>&1
if [ $? -ne 0 ];then
	echo "SKIP: Could not run test without nft tool"
	exit $ksft_skip
fi
ip -Version > /dev/null 2>&1
if [ $? -ne 0 ];then
	echo "SKIP: Could not run test without ip tool"
	exit $ksft_skip
fi
ip netns add "$ns0"
if [ $? -ne 0 ];then
	echo "SKIP: Could not create net namespace $ns0"
	exit $ksft_skip
fi
ip netns add "$ns1"
trap cleanup EXIT
ip netns exec $ns0 sysctl -q -w net.ipv4.conf.default.rp_filter=0
ip netns exec $ns0 sysctl -q -w net.ipv4.conf.all.rp_filter=0
ip netns exec $ns0 sysctl -q -w net.ipv4.conf.all.rp_filter=0
ip link add veth0 netns "$ns0" type veth peer name veth0 netns "$ns1" > /dev/null 2>&1
if [ $? -ne 0 ];then
	echo "SKIP: Could not add veth device"
	exit $ksft_skip
fi
ip -net $ns0 li add tvrf type vrf table 9876
if [ $? -ne 0 ];then
	echo "SKIP: Could not add vrf device"
	exit $ksft_skip
fi
ip -net $ns0 li set lo up
ip -net $ns0 li set veth0 master tvrf
ip -net $ns0 li set tvrf up
ip -net $ns0 li set veth0 up
ip -net $ns1 li set veth0 up
ip -net $ns0 addr add $IP0/$PFXL dev veth0
ip -net $ns1 addr add $IP1/$PFXL dev veth0
ip netns exec $ns1 iperf3 -s > /dev/null 2>&1&
if [ $? -ne 0 ];then
	echo "SKIP: Could not start iperf3"
	exit $ksft_skip
fi
test_ct_zone_in()
{
ip netns exec $ns0 nft -f - <<EOF
table testct {
	chain rawpre {
		type filter hook prerouting priority raw;
		iif { veth0, tvrf } counter meta nftrace set 1
		iif veth0 counter ct zone set 1 counter return
		iif tvrf counter ct zone set 2 counter return
		ip protocol icmp counter
		notrack counter
	}
	chain rawout {
		type filter hook output priority raw;
		oif veth0 counter ct zone set 1 counter return
		oif tvrf counter ct zone set 2 counter return
		notrack counter
	}
}
EOF
	ip netns exec $ns1 ping -W 1 -c 1 -I veth0 $IP0 > /dev/null
	count=$(ip netns exec $ns0 conntrack -L -s $IP1 -d $IP0 -p icmp --zone 1 2>/dev/null | wc -l)
	if [ $count -eq 1 ]; then
		echo "PASS: entry found in conntrack zone 1"
	else
		echo "FAIL: entry not found in conntrack zone 1"
		count=$(ip netns exec $ns0 conntrack -L -s $IP1 -d $IP0 -p icmp --zone 2 2> /dev/null | wc -l)
		if [ $count -eq 1 ]; then
			echo "FAIL: entry found in zone 2 instead"
		else
			echo "FAIL: entry not in zone 1 or 2, dumping table"
			ip netns exec $ns0 conntrack -L
			ip netns exec $ns0 nft list ruleset
		fi
	fi
}
test_masquerade_vrf()
{
	local qdisc=$1
	if [ "$qdisc" != "default" ]; then
		tc -net $ns0 qdisc add dev tvrf root $qdisc
	fi
	ip netns exec $ns0 conntrack -F 2>/dev/null
ip netns exec $ns0 nft -f - <<EOF
flush ruleset
table ip nat {
	chain rawout {
		type filter hook output priority raw;
		oif tvrf ct state untracked counter
	}
	chain postrouting2 {
		type filter hook postrouting priority mangle;
		oif tvrf ct state untracked counter
	}
	chain postrouting {
		type nat hook postrouting priority 0;
		ip saddr 172.30.30.0/30 counter masquerade random
	}
}
EOF
	ip netns exec $ns0 ip vrf exec tvrf iperf3 -t 1 -c $IP1 >/dev/null
	if [ $? -ne 0 ]; then
		echo "FAIL: iperf3 connect failure with masquerade + sport rewrite on vrf device"
		ret=1
		return
	fi
	ip netns exec $ns0 nft list table ip nat |grep -q 'counter packets 2' &&
	ip netns exec $ns0 nft list table ip nat |grep -q 'untracked counter packets [1-9]'
	if [ $? -eq 0 ]; then
		echo "PASS: iperf3 connect with masquerade + sport rewrite on vrf device ($qdisc qdisc)"
	else
		echo "FAIL: vrf rules have unexpected counter value"
		ret=1
	fi
	if [ "$qdisc" != "default" ]; then
		tc -net $ns0 qdisc del dev tvrf root
	fi
}
test_masquerade_veth()
{
	ip netns exec $ns0 conntrack -F 2>/dev/null
ip netns exec $ns0 nft -f - <<EOF
flush ruleset
table ip nat {
	chain postrouting {
		type nat hook postrouting priority 0;
		meta oif veth0 ip saddr 172.30.30.0/30 counter masquerade random
	}
}
EOF
	ip netns exec $ns0 ip vrf exec tvrf iperf3 -t 1 -c $IP1 > /dev/null
	if [ $? -ne 0 ]; then
		echo "FAIL: iperf3 connect failure with masquerade + sport rewrite on veth device"
		ret=1
		return
	fi
	ip netns exec $ns0 nft list table ip nat |grep -q 'counter packets 2'
	if [ $? -eq 0 ]; then
		echo "PASS: iperf3 connect with masquerade + sport rewrite on veth device"
	else
		echo "FAIL: vrf masq rule has unexpected counter value"
		ret=1
	fi
}
test_ct_zone_in
test_masquerade_vrf "default"
test_masquerade_vrf "pfifo"
test_masquerade_veth
exit $ret
