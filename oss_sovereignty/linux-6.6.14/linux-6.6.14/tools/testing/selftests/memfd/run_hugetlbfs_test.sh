ksft_skip=4
hpages_test=8
while read name size unit; do
        if [ "$name" = "HugePages_Free:" ]; then
                freepgs=$size
        fi
done < /proc/meminfo
if [ -n "$freepgs" ] && [ $freepgs -lt $hpages_test ]; then
	nr_hugepgs=`cat /proc/sys/vm/nr_hugepages`
	hpages_needed=`expr $hpages_test - $freepgs`
	if [ $UID != 0 ]; then
		echo "Please run memfd with hugetlbfs test as root"
		exit $ksft_skip
	fi
	echo 3 > /proc/sys/vm/drop_caches
	echo $(( $hpages_needed + $nr_hugepgs )) > /proc/sys/vm/nr_hugepages
	while read name size unit; do
		if [ "$name" = "HugePages_Free:" ]; then
			freepgs=$size
		fi
	done < /proc/meminfo
fi
if [ $freepgs -lt $hpages_test ]; then
	if [ -n "$nr_hugepgs" ]; then
		echo $nr_hugepgs > /proc/sys/vm/nr_hugepages
	fi
	printf "Not enough huge pages available (%d < %d)\n" \
		$freepgs $needpgs
	exit $ksft_skip
fi
./memfd_test hugetlbfs
./run_fuse_test.sh hugetlbfs
if [ -n "$nr_hugepgs" ]; then
	echo $nr_hugepgs > /proc/sys/vm/nr_hugepages
fi
