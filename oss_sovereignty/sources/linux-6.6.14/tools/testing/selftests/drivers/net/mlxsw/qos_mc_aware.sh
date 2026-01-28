ALL_TESTS="
	ping_ipv4
	test_mc_aware
	test_uc_aware
"
lib_dir=$(dirname $0)/../../../net/forwarding
NUM_NETIFS=6
source $lib_dir/lib.sh
source $lib_dir/devlink_lib.sh
source qos_lib.sh
h1_create()
{
	simple_if_init $h1 192.0.2.65/28
	mtu_set $h1 10000
}
h1_destroy()
{
	mtu_restore $h1
	simple_if_fini $h1 192.0.2.65/28
}
h2_create()
{
	simple_if_init $h2
	mtu_set $h2 10000
	vlan_create $h2 111 v$h2 192.0.2.129/28
	ip link set dev $h2.111 type vlan egress-qos-map 0:1
}
h2_destroy()
{
	vlan_destroy $h2 111
	mtu_restore $h2
	simple_if_fini $h2
}
h3_create()
{
	simple_if_init $h3 192.0.2.66/28
	mtu_set $h3 10000
	vlan_create $h3 111 v$h3 192.0.2.130/28
}
h3_destroy()
{
	vlan_destroy $h3 111
	mtu_restore $h3
	simple_if_fini $h3 192.0.2.66/28
}
switch_create()
{
	ip link set dev $swp1 up
	mtu_set $swp1 10000
	ip link set dev $swp2 up
	mtu_set $swp2 10000
	ip link set dev $swp3 up
	mtu_set $swp3 10000
	vlan_create $swp2 111
	vlan_create $swp3 111
	tc qdisc replace dev $swp3 root handle 3: tbf rate 1gbit \
		burst 128K limit 1G
	tc qdisc replace dev $swp3 parent 3:3 handle 33: \
		prio bands 8 priomap 7 7 7 7 7 7 7 7
	ip link add name br1 type bridge vlan_filtering 0
	ip link set dev br1 addrgenmode none
	ip link set dev br1 up
	ip link set dev $swp1 master br1
	ip link set dev $swp3 master br1
	ip link add name br111 type bridge vlan_filtering 0
	ip link set dev br111 addrgenmode none
	ip link set dev br111 up
	ip link set dev $swp2.111 master br111
	ip link set dev $swp3.111 master br111
	devlink_port_pool_th_save $swp1 0
	devlink_port_pool_th_set $swp1 0 5
	devlink_tc_bind_pool_th_save $swp1 0 ingress
	devlink_tc_bind_pool_th_set $swp1 0 ingress 0 5
	devlink_port_pool_th_save $swp2 0
	devlink_port_pool_th_set $swp2 0 5
	devlink_tc_bind_pool_th_save $swp2 1 ingress
	devlink_tc_bind_pool_th_set $swp2 1 ingress 0 5
	devlink_port_pool_th_save $swp3 4
	devlink_port_pool_th_set $swp3 4 12
}
switch_destroy()
{
	devlink_port_pool_th_restore $swp3 4
	devlink_tc_bind_pool_th_restore $swp2 1 ingress
	devlink_port_pool_th_restore $swp2 0
	devlink_tc_bind_pool_th_restore $swp1 0 ingress
	devlink_port_pool_th_restore $swp1 0
	ip link del dev br111
	ip link del dev br1
	tc qdisc del dev $swp3 parent 3:3 handle 33:
	tc qdisc del dev $swp3 root handle 3:
	vlan_destroy $swp3 111
	vlan_destroy $swp2 111
	mtu_restore $swp3
	ip link set dev $swp3 down
	mtu_restore $swp2
	ip link set dev $swp2 down
	mtu_restore $swp1
	ip link set dev $swp1 down
}
setup_prepare()
{
	h1=${NETIFS[p1]}
	swp1=${NETIFS[p2]}
	swp2=${NETIFS[p3]}
	h2=${NETIFS[p4]}
	swp3=${NETIFS[p5]}
	h3=${NETIFS[p6]}
	h3mac=$(mac_get $h3)
	vrf_prepare
	h1_create
	h2_create
	h3_create
	switch_create
}
cleanup()
{
	pre_cleanup
	switch_destroy
	h3_destroy
	h2_destroy
	h1_destroy
	vrf_cleanup
}
ping_ipv4()
{
	ping_test $h2 192.0.2.130
}
test_mc_aware()
{
	RET=0
	local -a uc_rate
	start_traffic $h2.111 192.0.2.129 192.0.2.130 $h3mac
	uc_rate=($(measure_rate $swp2 $h3 rx_octets_prio_1 "UC-only"))
	check_err $? "Could not get high enough UC-only ingress rate"
	stop_traffic
	local ucth1=${uc_rate[1]}
	start_traffic $h1 192.0.2.65 bc bc
	local d0=$(date +%s)
	local t0=$(ethtool_stats_get $h3 rx_octets_prio_0)
	local u0=$(ethtool_stats_get $swp1 rx_octets_prio_0)
	local -a uc_rate_2
	start_traffic $h2.111 192.0.2.129 192.0.2.130 $h3mac
	uc_rate_2=($(measure_rate $swp2 $h3 rx_octets_prio_1 "UC+MC"))
	check_err $? "Could not get high enough UC+MC ingress rate"
	stop_traffic
	local ucth2=${uc_rate_2[1]}
	local d1=$(date +%s)
	local t1=$(ethtool_stats_get $h3 rx_octets_prio_0)
	local u1=$(ethtool_stats_get $swp1 rx_octets_prio_0)
	local deg=$(bc <<< "
			scale=2
			ret = 100 * ($ucth1 - $ucth2) / $ucth1
			if (ret > 0) { ret } else { 0 }
		    ")
	check_err $(bc <<< "$deg < 15") "Minimum shaper not in effect"
	check_err $(bc <<< "$deg > 25") "MC traffic degrades UC performance too much"
	local interval=$((d1 - d0))
	local mc_ir=$(rate $u0 $u1 $interval)
	local mc_er=$(rate $t0 $t1 $interval)
	stop_traffic
	log_test "UC performance under MC overload"
	echo "UC-only throughput  $(humanize $ucth1)"
	echo "UC+MC throughput    $(humanize $ucth2)"
	echo "Degradation         $deg %"
	echo
	echo "Full report:"
	echo "  UC only:"
	echo "    ingress UC throughput $(humanize ${uc_rate[0]})"
	echo "    egress UC throughput  $(humanize ${uc_rate[1]})"
	echo "  UC+MC:"
	echo "    ingress UC throughput $(humanize ${uc_rate_2[0]})"
	echo "    egress UC throughput  $(humanize ${uc_rate_2[1]})"
	echo "    ingress MC throughput $(humanize $mc_ir)"
	echo "    egress MC throughput  $(humanize $mc_er)"
	echo
}
test_uc_aware()
{
	RET=0
	start_traffic $h2.111 192.0.2.129 192.0.2.130 $h3mac
	local d0=$(date +%s)
	local t0=$(ethtool_stats_get $h3 rx_octets_prio_1)
	local u0=$(ethtool_stats_get $swp2 rx_octets_prio_1)
	sleep 1
	local attempts=50
	local passes=0
	local i
	for ((i = 0; i < attempts; ++i)); do
		if $ARPING -c 1 -I $h1 -b 192.0.2.66 -q -w 1; then
			((passes++))
		fi
		sleep 0.1
	done
	local d1=$(date +%s)
	local t1=$(ethtool_stats_get $h3 rx_octets_prio_1)
	local u1=$(ethtool_stats_get $swp2 rx_octets_prio_1)
	local interval=$((d1 - d0))
	local uc_ir=$(rate $u0 $u1 $interval)
	local uc_er=$(rate $t0 $t1 $interval)
	((attempts == passes))
	check_err $?
	stop_traffic
	log_test "MC performance under UC overload"
	echo "    ingress UC throughput $(humanize ${uc_ir})"
	echo "    egress UC throughput  $(humanize ${uc_er})"
	echo "    sent $attempts BC ARPs, got $passes responses"
}
trap cleanup EXIT
setup_prepare
setup_wait
tests_run
exit $EXIT_STATUS
