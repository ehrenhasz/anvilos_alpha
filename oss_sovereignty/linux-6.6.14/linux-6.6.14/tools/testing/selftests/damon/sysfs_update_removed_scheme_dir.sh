ksft_skip=4
if [ $EUID -ne 0 ]
then
	echo "Run as root"
	exit $ksft_skip
fi
damon_sysfs="/sys/kernel/mm/damon/admin"
if [ ! -d "$damon_sysfs" ]
then
	echo "damon sysfs not found"
	exit $ksft_skip
fi
dmesg -C
echo 1 > "$damon_sysfs/kdamonds/nr_kdamonds"
echo 1 > "$damon_sysfs/kdamonds/0/contexts/nr_contexts"
echo "vaddr" > "$damon_sysfs/kdamonds/0/contexts/0/operations"
echo 1 > "$damon_sysfs/kdamonds/0/contexts/0/targets/nr_targets"
echo $$ > "$damon_sysfs/kdamonds/0/contexts/0/targets/0/pid_target"
echo 1 > "$damon_sysfs/kdamonds/0/contexts/0/schemes/nr_schemes"
scheme_dir="$damon_sysfs/kdamonds/0/contexts/0/schemes/0"
echo 4096000 > "$scheme_dir/access_pattern/sz/max"
echo 20 > "$scheme_dir/access_pattern/nr_accesses/max"
echo 1024 > "$scheme_dir/access_pattern/age/max"
echo "on" > "$damon_sysfs/kdamonds/0/state"
sleep 0.3
echo 0 > "$damon_sysfs/kdamonds/0/contexts/0/schemes/nr_schemes"
echo "update_schemes_stats" > "$damon_sysfs/kdamonds/0/state"
if dmesg | grep -q BUG
then
	echo "update_schemes_stats triggers a kernel bug"
	dmesg
	exit 1
fi
echo "update_schemes_tried_regions" > "$damon_sysfs/kdamonds/0/state"
if dmesg | grep -q BUG
then
	echo "update_schemes_tried_regions triggers a kernel bug"
	dmesg
	exit 1
fi
echo "off" > "$damon_sysfs/kdamonds/0/state"
