ksft_skip=4
if ! /sbin/modprobe -q -n test-ww_mutex; then
	echo "ww_mutex: module test-ww_mutex is not found [SKIP]"
	exit $ksft_skip
fi
if /sbin/modprobe -q test-ww_mutex; then
       /sbin/modprobe -q -r test-ww_mutex
       echo "locking/ww_mutex: ok"
else
       echo "locking/ww_mutex: [FAIL]"
       exit 1
fi
