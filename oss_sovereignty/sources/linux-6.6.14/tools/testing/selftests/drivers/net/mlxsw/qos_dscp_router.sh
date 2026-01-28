ALL_TESTS="
	ping_ipv4
	test_update
	test_no_update
	test_pedit_norewrite
	test_dscp_leftover
"
lib_dir=$(dirname $0)/../../../net/forwarding
NUM_NETIFS=4
source $lib_dir/lib.sh
reprioritize()
{
	local in=$1; shift
	local -a reprio=(0 0 2 2 6 6 4 4)
	echo ${reprio[$in]}
}
zero()
{
    echo 0
}
three()
{
    echo 3
}
h1_create()
{
	simple_if_init $h1 192.0.2.1/28
	tc qdisc add dev $h1 clsact
	dscp_capture_install $h1 0
	ip route add vrf v$h1 192.0.2.16/28 via 192.0.2.2
}
h1_destroy()
{
	ip route del vrf v$h1 192.0.2.16/28 via 192.0.2.2
	dscp_capture_uninstall $h1 0
	tc qdisc del dev $h1 clsact
	simple_if_fini $h1 192.0.2.1/28
}
h2_create()
{
	simple_if_init $h2 192.0.2.18/28
	tc qdisc add dev $h2 clsact
	dscp_capture_install $h2 0
	ip route add vrf v$h2 192.0.2.0/28 via 192.0.2.17
}
h2_destroy()
{
	ip route del vrf v$h2 192.0.2.0/28 via 192.0.2.17
	dscp_capture_uninstall $h2 0
	tc qdisc del dev $h2 clsact
	simple_if_fini $h2 192.0.2.18/28
}
switch_create()
{
	simple_if_init $swp1 192.0.2.2/28
	__simple_if_init $swp2 v$swp1 192.0.2.17/28
	tc qdisc add dev $swp1 clsact
	tc qdisc add dev $swp2 clsact
	dcb app add dev $swp1 dscp-prio 0:0 1:1 2:2 3:3 4:4 5:5 6:6 7:7
	dcb app add dev $swp2 dscp-prio 0:0 1:1 2:2 3:3 4:4 5:5 6:6 7:7
}
switch_destroy()
{
	dcb app del dev $swp2 dscp-prio 0:0 1:1 2:2 3:3 4:4 5:5 6:6 7:7
	dcb app del dev $swp1 dscp-prio 0:0 1:1 2:2 3:3 4:4 5:5 6:6 7:7
	tc qdisc del dev $swp2 clsact
	tc qdisc del dev $swp1 clsact
	__simple_if_fini $swp2 192.0.2.17/28
	simple_if_fini $swp1 192.0.2.2/28
}
setup_prepare()
{
	h1=${NETIFS[p1]}
	swp1=${NETIFS[p2]}
	swp2=${NETIFS[p3]}
	h2=${NETIFS[p4]}
	vrf_prepare
	sysctl_set net.ipv4.ip_forward_update_priority 1
	h1_create
	h2_create
	switch_create
}
cleanup()
{
	pre_cleanup
	switch_destroy
	h2_destroy
	h1_destroy
	sysctl_restore net.ipv4.ip_forward_update_priority
	vrf_cleanup
}
ping_ipv4()
{
	ping_test $h1 192.0.2.18
}
dscp_ping_test()
{
	local vrf_name=$1; shift
	local sip=$1; shift
	local dip=$1; shift
	local prio=$1; shift
	local reprio=$1; shift
	local dev1=$1; shift
	local dev2=$1; shift
	local i
	local prio2=$($reprio $prio)   # ICMP Request egress prio
	local prio3=$($reprio $prio2)  # ICMP Response egress prio
	local dscp=$((prio << 2))     # ICMP Request ingress DSCP
	local dscp2=$((prio2 << 2))   # ICMP Request egress DSCP
	local dscp3=$((prio3 << 2))   # ICMP Response egress DSCP
	RET=0
	eval "local -A dev1_t0s=($(dscp_fetch_stats $dev1 0))"
	eval "local -A dev2_t0s=($(dscp_fetch_stats $dev2 0))"
	local ping_timeout=$((PING_TIMEOUT * 5))
	ip vrf exec $vrf_name \
	   ${PING} -Q $dscp ${sip:+-I $sip} $dip \
		   -c 10 -i 0.5 -w $ping_timeout &> /dev/null
	eval "local -A dev1_t1s=($(dscp_fetch_stats $dev1 0))"
	eval "local -A dev2_t1s=($(dscp_fetch_stats $dev2 0))"
	for i in {0..7}; do
		local dscpi=$((i << 2))
		local expect2=0
		local expect3=0
		if ((i == prio2)); then
			expect2=10
		fi
		if ((i == prio3)); then
			expect3=10
		fi
		local delta=$((dev2_t1s[$i] - dev2_t0s[$i]))
		((expect2 == delta))
		check_err $? "DSCP $dscpi@$dev2: Expected to capture $expect2 packets, got $delta."
		delta=$((dev1_t1s[$i] - dev1_t0s[$i]))
		((expect3 == delta))
		check_err $? "DSCP $dscpi@$dev1: Expected to capture $expect3 packets, got $delta."
	done
	log_test "DSCP rewrite: $dscp-(prio $prio2)-$dscp2-(prio $prio3)-$dscp3"
}
__test_update()
{
	local update=$1; shift
	local reprio=$1; shift
	local prio
	sysctl_restore net.ipv4.ip_forward_update_priority
	sysctl_set net.ipv4.ip_forward_update_priority $update
	for prio in {0..7}; do
		dscp_ping_test v$h1 192.0.2.1 192.0.2.18 $prio $reprio $h1 $h2
	done
}
test_update()
{
	echo "Test net.ipv4.ip_forward_update_priority=1"
	__test_update 1 reprioritize
}
test_no_update()
{
	echo "Test net.ipv4.ip_forward_update_priority=0"
	__test_update 0 echo
}
test_pedit_norewrite()
{
	echo "Test no DSCP rewrite after DSCP is updated by pedit"
	tc filter add dev $swp1 ingress handle 101 pref 1 prot ip flower \
	    action pedit ex munge ip dsfield set $((3 << 2)) retain 0xfc \
	    action skbedit priority 3
	__test_update 0 three
	tc filter del dev $swp1 ingress pref 1
}
test_dscp_leftover()
{
	echo "Test that last removed DSCP rule is deconfigured correctly"
	dcb app del dev $swp2 dscp-prio 0:0 1:1 2:2 3:3 4:4 5:5 6:6 7:7
	__test_update 0 zero
	dcb app add dev $swp2 dscp-prio 0:0 1:1 2:2 3:3 4:4 5:5 6:6 7:7
}
trap cleanup EXIT
setup_prepare
setup_wait
tests_run
exit $EXIT_STATUS
