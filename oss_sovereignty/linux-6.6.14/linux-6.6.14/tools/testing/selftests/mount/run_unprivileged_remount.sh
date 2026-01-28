ksft_skip=4
if [ -f /proc/self/uid_map ] ; then
	./unprivileged-remount-test ;
else
	echo "WARN: No /proc/self/uid_map exist, test skipped." ;
	exit $ksft_skip
fi
