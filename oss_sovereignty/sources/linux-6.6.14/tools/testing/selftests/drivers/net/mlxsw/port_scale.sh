PORT_NUM_NETIFS=0
declare -a unsplit
port_setup_prepare()
{
	:
}
port_cleanup()
{
	pre_cleanup
	for port in "${unsplit[@]}"; do
		devlink port unsplit $port
		check_err $? "Did not unsplit $netdev"
	done
	unsplit=()
}
split_all_ports()
{
	local should_fail=$1; shift
	while read netdev count <<<$(
		devlink -j port show |
		jq -r '.[][] | select(.splittable==true) | "\(.netdev) \(.lanes)"'
		)
		[[ ! -z $netdev ]]
	do
		devlink port split $netdev count $count
		check_err $? "Did not split $netdev into $count"
		unsplit+=( "${netdev}s0" )
	done
}
port_test()
{
	local max_ports=$1; shift
	local should_fail=$1; shift
	split_all_ports $should_fail
	occ=$(devlink -j resource show $DEVLINK_DEV \
	      | jq '.[][][] | select(.name=="physical_ports") |.["occ"]')
	[[ $occ -eq $max_ports ]]
	check_err_fail $should_fail $? "Attempt to create $max_ports ports (actual result $occ)"
}
