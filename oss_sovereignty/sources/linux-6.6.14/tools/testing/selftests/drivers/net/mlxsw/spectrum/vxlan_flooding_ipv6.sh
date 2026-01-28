lib_dir=$(dirname $0)/../../../../net/forwarding
ALL_TESTS="flooding_test"
NUM_NETIFS=4
source $lib_dir/tc_common.sh
source $lib_dir/lib.sh
h1_create()
{
	simple_if_init $h1 2001:db8:1::1/64
}
h1_destroy()
{
	simple_if_fini $h1 2001:db8:1::1/64
}
switch_create()
{
	ip link add dev br0 type bridge mcast_snooping 0
	ip link set dev br0 address $(mac_get $swp1)
	ip link add name vxlan0 type vxlan id 10 nolearning \
		udp6zerocsumrx udp6zerocsumtx ttl 20 tos inherit \
		local 2001:db8:2::1 dstport 4789
	ip address add 2001:db8:2::1/128 dev lo
	ip link set dev $swp1 master br0
	ip link set dev vxlan0 master br0
	ip link set dev br0 up
	ip link set dev $swp1 up
	ip link set dev vxlan0 up
}
switch_destroy()
{
	ip link set dev vxlan0 down
	ip link set dev $swp1 down
	ip link set dev br0 down
	ip link set dev vxlan0 nomaster
	ip link set dev $swp1 nomaster
	ip address del 2001:db8:2::1/128 dev lo
	ip link del dev vxlan0
	ip link del dev br0
}
router1_create()
{
	ip link set dev $rp1 up
	ip address add 2001:db8:3::1/64 dev $rp1
	ip route add 2001:db8:2::0/64 via 2001:db8:3::2
}
router1_destroy()
{
	ip route del 2001:db8:2::0/64 via 2001:db8:3::2
	ip address del 2001:db8:3::1/64 dev $rp1
	ip link set dev $rp1 down
}
router2_create()
{
	simple_if_init $rp2 2001:db8:3::2/64
}
router2_destroy()
{
	simple_if_fini $rp2 2001:db8:3::2/64
}
setup_prepare()
{
	h1=${NETIFS[p1]}
	swp1=${NETIFS[p2]}
	rp1=${NETIFS[p3]}
	rp2=${NETIFS[p4]}
	vrf_prepare
	h1_create
	switch_create
	router1_create
	router2_create
	forwarding_enable
}
cleanup()
{
	pre_cleanup
	forwarding_restore
	router2_destroy
	router1_destroy
	switch_destroy
	h1_destroy
	vrf_cleanup
}
flooding_remotes_add()
{
	local num_remotes=$1
	local lsb
	local i
	for i in $(eval echo {1..$num_remotes}); do
		lsb=$((i + 1))
		bridge fdb append 00:00:00:00:00:00 dev vxlan0 self \
			dst 2001:db8:2::$lsb
	done
}
flooding_filters_add()
{
	local num_remotes=$1
	local lsb
	local i
	tc qdisc add dev $rp2 clsact
	for i in $(eval echo {1..$num_remotes}); do
		lsb=$((i + 1))
		tc filter add dev $rp2 ingress protocol ipv6 pref $i handle $i \
			flower ip_proto udp dst_ip 2001:db8:2::$lsb \
			dst_port 4789 skip_sw action drop
	done
}
flooding_filters_del()
{
	local num_remotes=$1
	local i
	for i in $(eval echo {1..$num_remotes}); do
		tc filter del dev $rp2 ingress protocol ipv6 pref $i \
			handle $i flower
	done
	tc qdisc del dev $rp2 clsact
}
flooding_check_packets()
{
	local packets=("$@")
	local num_remotes=${#packets[@]}
	local i
	for i in $(eval echo {1..$num_remotes}); do
		tc_check_packets "dev $rp2 ingress" $i ${packets[i - 1]}
		check_err $? "remote $i - did not get expected number of packets"
	done
}
flooding_test()
{
	declare -a packets=(1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1)
	local num_remotes=20
	RET=0
	flooding_remotes_add $num_remotes
	flooding_filters_add $num_remotes
	$MZ -6 $h1 -q -p 64 -b de:ad:be:ef:13:37 -t ip -c 1
	flooding_check_packets "${packets[@]}"
	log_test "flood after 1 packet"
	RET=0
	packets=(2 2 2 2 2 2 2 2 2 2 1 1 1 1 1 2 2 2 2 2)
	bridge fdb del 00:00:00:00:00:00 dev vxlan0 self dst 2001:db8:2::12
	bridge fdb del 00:00:00:00:00:00 dev vxlan0 self dst 2001:db8:2::13
	bridge fdb del 00:00:00:00:00:00 dev vxlan0 self dst 2001:db8:2::14
	bridge fdb del 00:00:00:00:00:00 dev vxlan0 self dst 2001:db8:2::15
	bridge fdb del 00:00:00:00:00:00 dev vxlan0 self dst 2001:db8:2::16
	$MZ -6 $h1 -q -p 64 -b de:ad:be:ef:13:37 -t ip -c 1
	flooding_check_packets "${packets[@]}"
	log_test "flood after 2 packets"
	RET=0
	packets=(2 2 2 2 2 3 3 3 3 3 1 1 1 1 1 3 3 3 3 3)
	bridge fdb del 00:00:00:00:00:00 dev vxlan0 self dst 2001:db8:2::2
	bridge fdb del 00:00:00:00:00:00 dev vxlan0 self dst 2001:db8:2::3
	bridge fdb del 00:00:00:00:00:00 dev vxlan0 self dst 2001:db8:2::4
	bridge fdb del 00:00:00:00:00:00 dev vxlan0 self dst 2001:db8:2::5
	bridge fdb del 00:00:00:00:00:00 dev vxlan0 self dst 2001:db8:2::6
	$MZ -6 $h1 -q -p 64 -b de:ad:be:ef:13:37 -t ip -c 1
	flooding_check_packets "${packets[@]}"
	log_test "flood after 3 packets"
	RET=0
	packets=(2 2 2 2 2 4 4 4 4 4 1 1 1 1 1 3 3 3 3 3)
	bridge fdb del 00:00:00:00:00:00 dev vxlan0 self dst 2001:db8:2::17
	bridge fdb del 00:00:00:00:00:00 dev vxlan0 self dst 2001:db8:2::18
	bridge fdb del 00:00:00:00:00:00 dev vxlan0 self dst 2001:db8:2::19
	bridge fdb del 00:00:00:00:00:00 dev vxlan0 self dst 2001:db8:2::20
	bridge fdb del 00:00:00:00:00:00 dev vxlan0 self dst 2001:db8:2::21
	$MZ -6 $h1 -q -p 64 -b de:ad:be:ef:13:37 -t ip -c 1
	flooding_check_packets "${packets[@]}"
	log_test "flood after 4 packets"
	RET=0
	packets=(2 2 2 2 2 4 5 5 5 5 1 1 1 1 1 3 3 3 3 3)
	bridge fdb del 00:00:00:00:00:00 dev vxlan0 self dst 2001:db8:2::7
	$MZ -6 $h1 -q -p 64 -b de:ad:be:ef:13:37 -t ip -c 1
	flooding_check_packets "${packets[@]}"
	log_test "flood after 5 packets"
	RET=0
	packets=(2 2 2 2 2 4 5 6 6 6 1 1 1 1 1 3 3 3 3 3)
	bridge fdb del 00:00:00:00:00:00 dev vxlan0 self dst 2001:db8:2::8
	$MZ -6 $h1 -q -p 64 -b de:ad:be:ef:13:37 -t ip -c 1
	flooding_check_packets "${packets[@]}"
	log_test "flood after 6 packets"
	RET=0
	packets=(2 2 2 2 2 4 5 6 7 7 1 1 1 1 1 3 3 3 3 3)
	bridge fdb del 00:00:00:00:00:00 dev vxlan0 self dst 2001:db8:2::9
	$MZ -6 $h1 -q -p 64 -b de:ad:be:ef:13:37 -t ip -c 1
	flooding_check_packets "${packets[@]}"
	log_test "flood after 7 packets"
	RET=0
	packets=(2 2 2 2 2 4 5 6 7 8 1 1 1 1 1 3 3 3 3 3)
	bridge fdb del 00:00:00:00:00:00 dev vxlan0 self dst 2001:db8:2::10
	$MZ -6 $h1 -q -p 64 -b de:ad:be:ef:13:37 -t ip -c 1
	flooding_check_packets "${packets[@]}"
	log_test "flood after 8 packets"
	RET=0
	packets=(2 2 2 2 2 4 5 6 7 8 1 1 1 1 1 3 3 3 3 3)
	bridge fdb del 00:00:00:00:00:00 dev vxlan0 self dst 2001:db8:2::11
	$MZ -6 $h1 -q -p 64 -b de:ad:be:ef:13:37 -t ip -c 1
	flooding_check_packets "${packets[@]}"
	log_test "flood after 9 packets"
	flooding_filters_del $num_remotes
}
trap cleanup EXIT
setup_prepare
setup_wait
tests_run
exit $EXIT_STATUS
