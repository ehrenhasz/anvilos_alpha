usage()
{
echo -e "Usage: $0 -[p] <compiler> [test_name]\n"
echo -e "\tkselftest_deps.sh [-p] gcc"
echo -e "\tkselftest_deps.sh [-p] gcc mm"
echo -e "\tkselftest_deps.sh [-p] aarch64-linux-gnu-gcc"
echo -e "\tkselftest_deps.sh [-p] aarch64-linux-gnu-gcc mm\n"
echo "- Should be run in selftests directory in the kernel repo."
echo "- Checks if Kselftests can be built/cross-built on a system."
echo "- Parses all test/sub-test Makefile to find library dependencies."
echo "- Runs compile test on a trivial C file with LDLIBS specified"
echo "  in the test Makefiles to identify missing library dependencies."
echo "- Prints suggested target list for a system filtering out tests"
echo "  failed the build dependency check from the TARGETS in Selftests"
echo "  main Makefile when optional -p is specified."
echo "- Prints pass/fail dependency check for each tests/sub-test."
echo "- Prints pass/fail targets and libraries."
echo "- Default: runs dependency checks on all tests."
echo "- Optional: test name can be specified to check dependencies for it."
exit 1
}
main()
{
base_dir=`pwd`
if [ $(basename "$base_dir") !=  "selftests" ]; then
	echo -e "\tPlease run $0 in"
	echo -e "\ttools/testing/selftests directory ..."
	exit 1
fi
print_targets=0
while getopts "p" arg; do
	case $arg in
		p)
		print_targets=1
	shift;;
	esac
done
if [ $# -eq 0 ]
then
	usage
fi
CC=$1
tmp_file=$(mktemp).c
trap "rm -f $tmp_file.o $tmp_file $tmp_file.bin" EXIT
pass=$(mktemp).out
trap "rm -f $pass" EXIT
fail=$(mktemp).out
trap "rm -f $fail" EXIT
cat << "EOF" > $tmp_file
int main()
{
}
EOF
total_cnt=0
fail_trgts=()
fail_libs=()
fail_cnt=0
pass_trgts=()
pass_libs=()
pass_cnt=0
targets=$(grep -E "^TARGETS +|^TARGETS =" Makefile | cut -d "=" -f2)
filter="\$(VAR_LDLIBS)\|pkg-config\|PKG_CONFIG\|IOURING_EXTRA_LIBS"
if [ $# -eq 2 ]
then
	test=$2/Makefile
	l1_test $test
	l2_test $test
	l3_test $test
	l4_test $test
	l5_test $test
	print_results $1 $2
	exit $?
fi
l1_tests=$(grep -r --include=Makefile "^LDLIBS" | \
		grep -v "$filter" | awk -F: '{print $1}' | uniq)
l2_tests=$(grep -r --include=Makefile ": LDLIBS" | \
		grep -v "$filter" | awk -F: '{print $1}' | uniq)
l3_tests=$(grep -r --include=Makefile "^VAR_LDLIBS" | \
		grep -v "pkg-config\|PKG_CONFIG" | awk -F: '{print $1}' | uniq)
l4_tests=$(grep -r --include=Makefile "^LDLIBS" | \
		grep "pkg-config\|PKG_CONFIG" | awk -F: '{print $1}' | uniq)
l5_tests=$(grep -r --include=Makefile "LDLIBS +=.*\$(IOURING_EXTRA_LIBS)" | \
	awk -F: '{print $1}' | uniq)
all_tests
print_results $1 $2
exit $?
}
all_tests()
{
	for test in $l1_tests; do
		l1_test $test
	done
	for test in $l2_tests; do
		l2_test $test
	done
	for test in $l3_tests; do
		l3_test $test
	done
	for test in $l4_tests; do
		l4_test $test
	done
	for test in $l5_tests; do
		l5_test $test
	done
}
l1_test()
{
	test_libs=$(grep --include=Makefile "^LDLIBS" $test | \
			grep -v "$filter" | \
			sed -e 's/\:/ /' | \
			sed -e 's/+/ /' | cut -d "=" -f 2)
	check_libs $test $test_libs
}
l2_test()
{
	test_libs=$(grep --include=Makefile ": LDLIBS" $test | \
			grep -v "$filter" | \
			sed -e 's/\:/ /' | sed -e 's/+/ /' | \
			cut -d "=" -f 2)
	check_libs $test $test_libs
}
l3_test()
{
	test_libs=$(grep --include=Makefile "^VAR_LDLIBS" $test | \
			grep -v "pkg-config" | sed -e 's/\:/ /' |
			sed -e 's/+/ /' | cut -d "=" -f 2)
	check_libs $test $test_libs
}
l4_test()
{
	test_libs=$(grep --include=Makefile "^VAR_LDLIBS\|^LDLIBS" $test | \
			grep "\(pkg-config\|PKG_CONFIG\).*|| echo " | \
			sed -e 's/.*|| echo //' | sed -e 's/)$//')
	check_libs $test $test_libs
}
l5_test()
{
	tests=$(find $(dirname "$test") -type f -name "*.mk")
	test_libs=$(grep "^IOURING_EXTRA_LIBS +\?=" $tests | \
			cut -d "=" -f 2)
	check_libs $test $test_libs
}
check_libs()
{
if [[ ! -z "${test_libs// }" ]]
then
	for lib in $test_libs; do
	let total_cnt+=1
	$CC -o $tmp_file.bin $lib $tmp_file > /dev/null 2>&1
	if [ $? -ne 0 ]; then
		echo "FAIL: $test dependency check: $lib" >> $fail
		let fail_cnt+=1
		fail_libs+="$lib "
		fail_target=$(echo "$test" | cut -d "/" -f1)
		fail_trgts+="$fail_target "
		targets=$(echo "$targets" | grep -v "$fail_target")
	else
		echo "PASS: $test dependency check passed $lib" >> $pass
		let pass_cnt+=1
		pass_libs+="$lib "
		pass_trgts+="$(echo "$test" | cut -d "/" -f1) "
	fi
	done
fi
}
print_results()
{
	echo -e "========================================================";
	echo -e "Kselftest Dependency Check for [$0 $1 $2] results..."
	if [ $print_targets -ne 0 ]
	then
	echo -e "Suggested Selftest Targets for your configuration:"
	echo -e "$targets";
	fi
	echo -e "========================================================";
	echo -e "Checked tests defining LDLIBS dependencies"
	echo -e "--------------------------------------------------------";
	echo -e "Total tests with Dependencies:"
	echo -e "$total_cnt Pass: $pass_cnt Fail: $fail_cnt";
	if [ $pass_cnt -ne 0 ]; then
	echo -e "--------------------------------------------------------";
	cat $pass
	echo -e "--------------------------------------------------------";
	echo -e "Targets passed build dependency check on system:"
	echo -e "$(echo "$pass_trgts" | xargs -n1 | sort -u | xargs)"
	fi
	if [ $fail_cnt -ne 0 ]; then
	echo -e "--------------------------------------------------------";
	cat $fail
	echo -e "--------------------------------------------------------";
	echo -e "Targets failed build dependency check on system:"
	echo -e "$(echo "$fail_trgts" | xargs -n1 | sort -u | xargs)"
	echo -e "--------------------------------------------------------";
	echo -e "Missing libraries system"
	echo -e "$(echo "$fail_libs" | xargs -n1 | sort -u | xargs)"
	fi
	echo -e "--------------------------------------------------------";
	echo -e "========================================================";
}
main "$@"
