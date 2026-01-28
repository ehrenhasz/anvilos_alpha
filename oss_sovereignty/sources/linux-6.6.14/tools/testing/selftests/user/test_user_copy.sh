ksft_skip=4
if ! /sbin/modprobe -q -n test_user_copy; then
	echo "user: module test_user_copy is not found [SKIP]"
	exit $ksft_skip
fi
if /sbin/modprobe -q test_user_copy; then
	/sbin/modprobe -q -r test_user_copy
	echo "user_copy: ok"
else
	echo "user_copy: [FAIL]"
	exit 1
fi
