if [ $FILE_BASIC ]; then
	return 0
else
	FILE_BASIC=DONE
fi
amd_pstate_basic()
{
	printf "\n---------------------------------------------\n"
	printf "*** Running AMD P-state ut                ***"
	printf "\n---------------------------------------------\n"
	if ! /sbin/modprobe -q -n amd-pstate-ut; then
		echo "amd-pstate-ut: module amd-pstate-ut is not found [SKIP]"
		exit $ksft_skip
	fi
	if /sbin/modprobe -q amd-pstate-ut; then
		/sbin/modprobe -q -r amd-pstate-ut
		echo "amd-pstate-basic: ok"
	else
		echo "amd-pstate-basic: [FAIL]"
		exit 1
	fi
}
