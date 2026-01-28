ksft_skip=4
rnd=$(mktemp -u XXXXXXXX)
rx=$(mktemp)
r_a="ns-ra-$rnd"
r_b="ns-rb-$rnd"
r_w="ns-rw-$rnd"
c_a="ns-ca-$rnd"
c_b="ns-cb-$rnd"
checktool (){
	if ! $1 > /dev/null 2>&1; then
		echo "SKIP: Could not $2"
		exit $ksft_skip
	fi
}
checktool "iptables --version" "run test without iptables"
checktool "ip -Version" "run test without ip tool"
checktool "which socat" "run test without socat"
checktool "ip netns add ${r_a}" "create net namespace"
for n in ${r_b} ${r_w} ${c_a} ${c_b};do
	ip netns add ${n}
done
cleanup() {
	for n in ${r_a} ${r_b} ${r_w} ${c_a} ${c_b};do
		ip netns del ${n}
	done
	rm -f ${rx}
}
trap cleanup EXIT
test_path() {
	msg="$1"
	ip netns exec ${c_b} socat -t 3 - udp4-listen:5000,reuseaddr > ${rx} < /dev/null &
	sleep 1
	for i in 1 2 3; do
		head -c1400 /dev/zero | tr "\000" "a" | \
			ip netns exec ${c_a} socat -t 1 -u STDIN UDP:192.168.20.2:5000
	done
	wait
	bytes=$(wc -c < ${rx})
	if [ $bytes -eq 1400 ];then
		echo "OK: PMTU $msg connection tracking"
	else
		echo "FAIL: PMTU $msg connection tracking: got $bytes, expected 1400"
		exit 1
	fi
}
ip link add veth0 netns ${r_a} type veth peer name veth0 netns ${r_w}
ip link add veth1 netns ${r_a} type veth peer name veth0 netns ${c_a}
l_addr="10.2.2.1"
r_addr="10.4.4.1"
ip netns exec ${r_a} ip link add ipip0 type ipip local ${l_addr} remote ${r_addr} mode ipip || exit $ksft_skip
for dev in lo veth0 veth1 ipip0; do
    ip -net ${r_a} link set $dev up
done
ip -net ${r_a} addr add 10.2.2.1/24 dev veth0
ip -net ${r_a} addr add 192.168.10.1/24 dev veth1
ip -net ${r_a} route add 192.168.20.0/24 dev ipip0
ip -net ${r_a} route add 10.4.4.0/24 via 10.2.2.254
ip netns exec ${r_a} sysctl -q net.ipv4.conf.all.forwarding=1 > /dev/null
ip link add veth0 netns ${r_b} type veth peer name veth1 netns ${r_w}
ip link add veth1 netns ${r_b} type veth peer name veth0 netns ${c_b}
l_addr="10.4.4.1"
r_addr="10.2.2.1"
ip netns exec ${r_b} ip link add ipip0 type ipip local ${l_addr} remote ${r_addr} mode ipip || exit $ksft_skip
for dev in lo veth0 veth1 ipip0; do
	ip -net ${r_b} link set $dev up
done
ip -net ${r_b} addr add 10.4.4.1/24 dev veth0
ip -net ${r_b} addr add 192.168.20.1/24 dev veth1
ip -net ${r_b} route add 192.168.10.0/24 dev ipip0
ip -net ${r_b} route add 10.2.2.0/24 via 10.4.4.254
ip netns exec ${r_b} sysctl -q net.ipv4.conf.all.forwarding=1 > /dev/null
ip -net ${c_a} addr add 192.168.10.2/24 dev veth0
ip -net ${c_a} link set dev lo up
ip -net ${c_a} link set dev veth0 up
ip -net ${c_a} route add default via 192.168.10.1
ip -net ${c_b} addr add 192.168.20.2/24 dev veth0
ip -net ${c_b} link set dev veth0 up
ip -net ${c_b} link set dev lo up
ip -net ${c_b} route add default via 192.168.20.1
ip -net ${r_w} addr add 10.2.2.254/24 dev veth0
ip -net ${r_w} addr add 10.4.4.254/24 dev veth1
ip -net ${r_w} link set dev lo up
ip -net ${r_w} link set dev veth0 up mtu 1400
ip -net ${r_w} link set dev veth1 up mtu 1400
ip -net ${r_a} link set dev veth0 mtu 1400
ip -net ${r_b} link set dev veth0 mtu 1400
ip netns exec ${r_w} sysctl -q net.ipv4.conf.all.forwarding=1 > /dev/null
test_path "without"
ip netns exec ${r_a} iptables -A FORWARD -m conntrack --ctstate NEW
test_path "with"
