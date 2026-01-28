source ../tc_flower_scale.sh
tc_flower_get_target()
{
	local should_fail=$1; shift
	local target=5631
	if ((! should_fail)); then
		echo $target
	else
		echo $((target + 1))
	fi
}
