lib_dir=$(dirname $0)/../../../net/forwarding
source $lib_dir/sch_ets_core.sh
source $lib_dir/devlink_lib.sh
ALL_TESTS="
	ping_ipv4
	priomap_mode
	ets_test_strict
	ets_test_mixed
	ets_test_dwrr
"
PARENT="parent 3:3"
switch_create()
{
	tc qdisc replace dev $swp2 root handle 3: tbf rate 1gbit \
		burst 128K limit 1G
	ets_switch_create
	devlink_port_pool_th_save $swp1 0
	devlink_port_pool_th_set $swp1 0 12
	devlink_tc_bind_pool_th_save $swp1 0 ingress
	devlink_tc_bind_pool_th_set $swp1 0 ingress 0 12
	devlink_port_pool_th_save $swp2 4
	devlink_port_pool_th_set $swp2 4 12
	devlink_tc_bind_pool_th_save $swp2 7 egress
	devlink_tc_bind_pool_th_set $swp2 7 egress 4 5
	devlink_tc_bind_pool_th_save $swp2 6 egress
	devlink_tc_bind_pool_th_set $swp2 6 egress 4 5
	devlink_tc_bind_pool_th_save $swp2 5 egress
	devlink_tc_bind_pool_th_set $swp2 5 egress 4 5
}
switch_destroy()
{
	devlink_tc_bind_pool_th_restore $swp2 5 egress
	devlink_tc_bind_pool_th_restore $swp2 6 egress
	devlink_tc_bind_pool_th_restore $swp2 7 egress
	devlink_port_pool_th_restore $swp2 4
	devlink_tc_bind_pool_th_restore $swp1 0 ingress
	devlink_port_pool_th_restore $swp1 0
	ets_switch_destroy
	tc qdisc del dev $swp2 root handle 3:
}
collect_stats()
{
	local -a streams=("$@")
	local stream
	busywait_for_counter 1000 +1 \
		qdisc_parent_stats_get $swp2 10:$((${streams[0]} + 1)) .bytes \
		> /dev/null
	for stream in ${streams[@]}; do
		qdisc_parent_stats_get $swp2 10:$((stream + 1)) .bytes
	done
}
bail_on_lldpad "configure DCB" "configure Qdiscs"
ets_run
