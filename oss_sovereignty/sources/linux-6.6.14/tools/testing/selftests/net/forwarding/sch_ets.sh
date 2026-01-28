lib_dir=.
source sch_ets_core.sh
ALL_TESTS="
	ping_ipv4
	priomap_mode
	ets_test_strict
	ets_test_mixed
	ets_test_dwrr
	classifier_mode
	ets_test_strict
	ets_test_mixed
	ets_test_dwrr
"
switch_create()
{
	ets_switch_create
	tc qdisc add dev $swp2 root handle 1: tbf \
	   rate 1Gbit burst 1Mbit latency 100ms
	PARENT="parent 1:"
}
switch_destroy()
{
	ets_switch_destroy
	tc qdisc del dev $swp2 root
}
collect_stats()
{
	local -a streams=("$@")
	local stream
	for stream in ${streams[@]}; do
		qdisc_parent_stats_get $swp2 10:$((stream + 1)) .bytes
	done
}
ets_run
