ALL_TESTS="
	ping_ipv4
	test_qos_pfc
"
lib_dir=$(dirname $0)/../../../net/forwarding
NUM_NETIFS=6
source $lib_dir/lib.sh
source $lib_dir/devlink_lib.sh
_1KB=1000
_100KB=$((100 * _1KB))
_500KB=$((500 * _1KB))
_1MB=$((1000 * _1KB))
_10MB=$((10 * _1MB))
h1_create()
{
	simple_if_init $h1
	mtu_set $h1 10000
	vlan_create $h1 111 v$h1 192.0.2.33/28
}
h1_destroy()
{
	vlan_destroy $h1 111
	mtu_restore $h1
	simple_if_fini $h1
}
h2_create()
{
	simple_if_init $h2
	mtu_set $h2 10000
	vlan_create $h2 111 v$h2 192.0.2.34/28
}
h2_destroy()
{
	vlan_destroy $h2 111
	mtu_restore $h2
	simple_if_fini $h2
}
switch_create()
{
	local lanes_swp4
	local pg1_size
	devlink_pool_size_thtype_save 0
	devlink_pool_size_thtype_save 4
	devlink_pool_size_thtype_save 1
	devlink_pool_size_thtype_save 5
	devlink_pool_size_thtype_save 2
	devlink_pool_size_thtype_save 6
	devlink_port_pool_th_save $swp1 1
	devlink_port_pool_th_save $swp2 6
	devlink_port_pool_th_save $swp3 5
	devlink_port_pool_th_save $swp4 2
	devlink_tc_bind_pool_th_save $swp1 1 ingress
	devlink_tc_bind_pool_th_save $swp2 1 egress
	devlink_tc_bind_pool_th_save $swp3 1 egress
	devlink_tc_bind_pool_th_save $swp4 1 ingress
	devlink_pool_size_thtype_set 0 dynamic $_500KB
	devlink_pool_size_thtype_set 4 dynamic $_500KB
	devlink_pool_size_thtype_set 1 static $_10MB
	devlink_pool_size_thtype_set 5 static $_10MB
	devlink_pool_size_thtype_set 2 static $_1MB
	devlink_pool_size_thtype_set 6 static $_10MB
	ip link set dev $swp1 up
	mtu_set $swp1 10000
	vlan_create $swp1 111
	ip link set dev $swp1.111 type vlan ingress-qos-map 0:0 1:1
	devlink_port_pool_th_set $swp1 1 $_10MB
	devlink_tc_bind_pool_th_set $swp1 1 ingress 1 $_10MB
	tc qdisc replace dev $swp1 root handle 1: \
	   ets bands 8 strict 8 priomap 7 6
	dcb buffer set dev $swp1 prio-buffer all:0 1:1
	ip link set dev $swp2 up
	mtu_set $swp2 10000
	vlan_create $swp2 111
	ip link set dev $swp2.111 type vlan egress-qos-map 0:0 1:1
	devlink_port_pool_th_set $swp2 6 $_10MB
	devlink_tc_bind_pool_th_set $swp2 1 egress 6 $_10MB
	tc qdisc replace dev $swp2 root handle 1: \
	   ets bands 8 strict 8 priomap 7 6
	tc qdisc replace dev $swp2 parent 1:7 handle 17: \
	   tbf rate 200Mbit burst 131072 limit 1M
	ip link set dev $swp3 up
	mtu_set $swp3 10000
	vlan_create $swp3 111
	ip link set dev $swp3.111 type vlan egress-qos-map 0:0 1:1
	devlink_port_pool_th_set $swp3 5 $_10MB
	devlink_tc_bind_pool_th_set $swp3 1 egress 5 $_10MB
	tc qdisc replace dev $swp3 root handle 1: \
	   ets bands 8 strict 8 priomap 7 6
	dcb buffer set dev $swp3 prio-buffer all:0 1:1
	dcb pfc set dev $swp3 prio-pfc all:off 1:on
	ip link set dev $swp4 up
	mtu_set $swp4 10000
	vlan_create $swp4 111
	ip link set dev $swp4.111 type vlan ingress-qos-map 0:0 1:1
	devlink_port_pool_th_set $swp4 2 $_1MB
	devlink_tc_bind_pool_th_set $swp4 1 ingress 2 $_1MB
	tc qdisc replace dev $swp4 root handle 1: \
	   ets bands 8 strict 8 priomap 7 6
	dcb buffer set dev $swp4 prio-buffer all:0 1:1
	dcb pfc set dev $swp4 prio-pfc all:off 1:on
	pg1_size=$_100KB
	setup_wait_dev_with_timeout $swp4
	lanes_swp4=$(ethtool $swp4 | grep 'Lanes:')
	lanes_swp4=${lanes_swp4
	if [[ $lanes_swp4 -eq 8 ]]; then
		pg1_size=$((pg1_size * 2))
	fi
	dcb buffer set dev $swp4 buffer-size all:0 1:$pg1_size
	ip link add name br1 type bridge vlan_filtering 0
	ip link set dev $swp1.111 master br1
	ip link set dev $swp3.111 master br1
	ip link set dev br1 up
	ip link add name br2 type bridge vlan_filtering 0
	ip link set dev $swp2.111 master br2
	ip link set dev $swp4.111 master br2
	ip link set dev br2 up
}
switch_destroy()
{
	devlink_pool_size_thtype_restore 6
	devlink_pool_size_thtype_restore 5
	devlink_pool_size_thtype_restore 4
	devlink_pool_size_thtype_restore 2
	devlink_pool_size_thtype_restore 1
	devlink_pool_size_thtype_restore 0
	ip link set dev br2 down
	ip link set dev $swp4.111 nomaster
	ip link set dev $swp2.111 nomaster
	ip link del dev br2
	ip link set dev br1 down
	ip link set dev $swp3.111 nomaster
	ip link set dev $swp1.111 nomaster
	ip link del dev br1
	dcb buffer set dev $swp4 buffer-size all:0
	dcb pfc set dev $swp4 prio-pfc all:off
	dcb buffer set dev $swp4 prio-buffer all:0
	tc qdisc del dev $swp4 root
	devlink_tc_bind_pool_th_restore $swp4 1 ingress
	devlink_port_pool_th_restore $swp4 2
	vlan_destroy $swp4 111
	mtu_restore $swp4
	ip link set dev $swp4 down
	dcb pfc set dev $swp3 prio-pfc all:off
	dcb buffer set dev $swp3 prio-buffer all:0
	tc qdisc del dev $swp3 root
	devlink_tc_bind_pool_th_restore $swp3 1 egress
	devlink_port_pool_th_restore $swp3 5
	vlan_destroy $swp3 111
	mtu_restore $swp3
	ip link set dev $swp3 down
	tc qdisc del dev $swp2 parent 1:7
	tc qdisc del dev $swp2 root
	devlink_tc_bind_pool_th_restore $swp2 1 egress
	devlink_port_pool_th_restore $swp2 6
	vlan_destroy $swp2 111
	mtu_restore $swp2
	ip link set dev $swp2 down
	dcb buffer set dev $swp1 prio-buffer all:0
	tc qdisc del dev $swp1 root
	devlink_tc_bind_pool_th_restore $swp1 1 ingress
	devlink_port_pool_th_restore $swp1 1
	vlan_destroy $swp1 111
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
	swp4=${NETIFS[p6]}
	h2mac=$(mac_get $h2)
	vrf_prepare
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
	vrf_cleanup
}
ping_ipv4()
{
	ping_test $h1 192.0.2.34
}
test_qos_pfc()
{
	RET=0
	local pkts=$((_10MB / 8050))
	local size=$((pkts * 8050))
	local in0=$(ethtool_stats_get $swp1 rx_octets_prio_1)
	local out0=$(ethtool_stats_get $swp2 tx_octets_prio_1)
	$MZ $h1 -p 8000 -Q 1:111 -A 192.0.2.33 -B 192.0.2.34 \
		-a own -b $h2mac -c $pkts -t udp -q
	sleep 2
	local in1=$(ethtool_stats_get $swp1 rx_octets_prio_1)
	local out1=$(ethtool_stats_get $swp2 tx_octets_prio_1)
	local din=$((in1 - in0))
	local dout=$((out1 - out0))
	local pct_in=$((din * 100 / size))
	((pct_in > 95 && pct_in < 105))
	check_err $? "Relative ingress out of expected bounds, $pct_in% should be 100%"
	((dout == din))
	check_err $? "$((din - dout)) bytes out of $din ingressed got lost"
	log_test "PFC"
}
bail_on_lldpad "configure DCB" "configure Qdiscs"
trap cleanup EXIT
setup_prepare
setup_wait
tests_run
exit $EXIT_STATUS
