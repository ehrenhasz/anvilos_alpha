TEST_NAME="test_hmm"
DRIVER="test_hmm"
exitcode=1
ksft_skip=4
check_test_requirements()
{
	uid=$(id -u)
	if [ $uid -ne 0 ]; then
		echo "$0: Must be run as root"
		exit $ksft_skip
	fi
	if ! which modprobe > /dev/null 2>&1; then
		echo "$0: You need modprobe installed"
		exit $ksft_skip
	fi
	if ! modinfo $DRIVER > /dev/null 2>&1; then
		echo "$0: You must have the following enabled in your kernel:"
		echo "CONFIG_TEST_HMM=m"
		exit $ksft_skip
	fi
}
load_driver()
{
	if [ $
		modprobe $DRIVER > /dev/null 2>&1
	else
		if [ $
			modprobe $DRIVER spm_addr_dev0=$1 spm_addr_dev1=$2
				> /dev/null 2>&1
		else
			echo "Missing module parameters. Make sure pass"\
			"spm_addr_dev0 and spm_addr_dev1"
			usage
		fi
	fi
}
unload_driver()
{
	modprobe -r $DRIVER > /dev/null 2>&1
}
run_smoke()
{
	echo "Running smoke test. Note, this test provides basic coverage."
	load_driver $1 $2
	$(dirname "${BASH_SOURCE[0]}")/hmm-tests
	unload_driver
}
usage()
{
	echo -n "Usage: $0"
	echo
	echo "Example usage:"
	echo
	echo "
	echo "./${TEST_NAME}.sh"
	echo
	echo "
	echo "./${TEST_NAME}.sh smoke"
	echo
	echo "
	echo "./${TEST_NAME}.sh smoke <spm_addr_dev0> <spm_addr_dev1>"
	echo
	exit 0
}
function run_test()
{
	if [ $
		usage
	else
		if [ "$1" = "smoke" ]; then
			run_smoke $2 $3
		else
			usage
		fi
	fi
}
check_test_requirements
run_test $@
exit 0
