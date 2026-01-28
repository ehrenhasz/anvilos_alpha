ksft_skip=4
ret=0
ebtables -V > /dev/null 2>&1
if [ $? -ne 0 ];then
	echo "SKIP: Could not run test without ebtables"
	exit $ksft_skip
fi
ip -Version > /dev/null 2>&1
if [ $? -ne 0 ];then
	echo "SKIP: Could not run test without ip tool"
	exit $ksft_skip
fi
ip netns add ns0
ip netns add ns1
ip netns add ns2
ip link add veth0 netns ns0 type veth peer name eth0 netns ns1
if [ $? -ne 0 ]; then
	echo "SKIP: Can't create veth device"
	exit $ksft_skip
fi
ip link add veth1 netns ns0 type veth peer name eth0 netns ns2
ip -net ns0 link set lo up
ip -net ns0 link set veth0 up
ip -net ns0 link set veth1 up
ip -net ns0 link add br0 type bridge
if [ $? -ne 0 ]; then
	echo "SKIP: Can't create bridge br0"
	exit $ksft_skip
fi
ip -net ns0 link set veth0 master br0
ip -net ns0 link set veth1 master br0
ip -net ns0 link set br0 up
ip -net ns0 addr add 10.0.0.1/24 dev br0
for i in 1 2; do
  ip -net ns$i link set lo up
  ip -net ns$i link set eth0 up
  ip -net ns$i addr add 10.0.0.1$i/24 dev eth0
done
test_ebtables_broute()
{
	local cipt
	ip netns exec ns0 ebtables -t broute -A BROUTING -p ipv4 --ip-protocol icmp -j redirect --redirect-target=DROP
	if [ $? -ne 0 ]; then
		echo "SKIP: Could not add ebtables broute redirect rule"
		return $ksft_skip
	fi
	ip netns exec ns1 ping -q -c 1 10.0.0.12 > /dev/null 2>&1
	if [ $? -eq 0 ]; then
		echo "ERROR: ping works, should have failed" 1>&2
		return 1
	fi
	ip netns exec ns0 sysctl -q net.ipv4.conf.veth0.forwarding=1
	ip netns exec ns0 sysctl -q net.ipv4.conf.veth1.forwarding=1
	sleep 1
	ip netns exec ns1 ping -q -c 1 10.0.0.12 > /dev/null
	if [ $? -ne 0 ]; then
		echo "ERROR: ping did not work, but it should (broute+forward)" 1>&2
		return 1
	fi
	echo "PASS: ns1/ns2 connectivity with active broute rule"
	ip netns exec ns0 ebtables -t broute -F
	ip netns exec ns1 ping -q -c 1 10.0.0.12 > /dev/null
	if [ $? -ne 0 ]; then
		echo "ERROR: ping did not work, but it should (bridged)" 1>&2
		return 1
	fi
	ip netns exec ns0 ebtables -t filter -A FORWARD -p ipv4 --ip-protocol icmp -j DROP
	ip netns exec ns1 ping -q -c 1 10.0.0.12 > /dev/null 2>&1
	if [ $? -eq 0 ]; then
		echo "ERROR: ping works, should have failed (icmp forward drop)" 1>&2
		return 1
	fi
	ip netns exec ns0 ebtables -t broute -A BROUTING -p ipv4 --ip-protocol icmp -j redirect --redirect-target=DROP
	ip netns exec ns2 ping -q -c 1 10.0.0.11 > /dev/null
	if [ $? -ne 0 ]; then
		echo "ERROR: ping did not work, but it should (broute+forward 2)" 1>&2
		return 1
	fi
	echo "PASS: ns1/ns2 connectivity with active broute rule and bridge forward drop"
	return 0
}
ip netns exec ns1 ping -c 1 -q 10.0.0.12 > /dev/null
if [ $? -ne 0 ]; then
    echo "ERROR: Could not reach ns2 from ns1" 1>&2
    ret=1
fi
ip netns exec ns2 ping -c 1 -q 10.0.0.11 > /dev/null
if [ $? -ne 0 ]; then
    echo "ERROR: Could not reach ns1 from ns2" 1>&2
    ret=1
fi
if [ $ret -eq 0 ];then
    echo "PASS: netns connectivity: ns1 and ns2 can reach each other"
fi
test_ebtables_broute
ret=$?
for i in 0 1 2; do ip netns del ns$i;done
exit $ret
