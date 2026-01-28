export skip_rc=4
export timeout_rc=124
export logfile=/dev/stdout
export per_test_logging=
export kselftest_default_timeout=45
if [ -z "$BASE_DIR" ]; then
	echo "Error: BASE_DIR must be set before sourcing." >&2
	exit 1
fi
TR_CMD=$(command -v tr)
tap_prefix()
{
	if [ ! -x /usr/bin/perl ]; then
		sed -e 's/^/# /'
	else
		"$BASE_DIR"/kselftest/prefix.pl
	fi
}
tap_timeout()
{
	if [ -x /usr/bin/timeout ] ; then
		/usr/bin/timeout --foreground "$kselftest_timeout" \
			/usr/bin/timeout "$kselftest_timeout" $1
	else
		$1
	fi
}
run_one()
{
	DIR="$1"
	TEST="$2"
	NUM="$3"
	BASENAME_TEST=$(basename $TEST)
	export kselftest_timeout="$kselftest_default_timeout"
	kselftest_cmd_args_ref="KSELFTEST_ARGS"
	if [ -n "$TR_CMD" ]; then
		BASENAME_SANITIZED=$(echo "$BASENAME_TEST" | \
					$TR_CMD -d "[:blank:][:cntrl:]" | \
					$TR_CMD -c "[:alnum:]_" "_" | \
					$TR_CMD [:lower:] [:upper:])
		kselftest_cmd_args_ref="KSELFTEST_${BASENAME_SANITIZED}_ARGS"
	fi
	settings="$BASE_DIR/$DIR/settings"
	if [ -r "$settings" ] ; then
		while read line ; do
			if echo "$line" | grep -q '^#'; then
				continue
			fi
			field=$(echo "$line" | cut -d= -f1)
			value=$(echo "$line" | cut -d= -f2-)
			eval "kselftest_$field"="$value"
		done < "$settings"
	fi
	if [ -n "$kselftest_override_timeout" ]; then
		kselftest_timeout="$kselftest_override_timeout"
		echo "# overriding timeout to $kselftest_timeout" >> "$logfile"
	else
		echo "# timeout set to $kselftest_timeout" >> "$logfile"
	fi
	TEST_HDR_MSG="selftests: $DIR: $BASENAME_TEST"
	echo "# $TEST_HDR_MSG"
	if [ ! -e "$TEST" ]; then
		echo "# Warning: file $TEST is missing!"
		echo "not ok $test_num $TEST_HDR_MSG"
	else
		if [ -x /usr/bin/stdbuf ]; then
			stdbuf="/usr/bin/stdbuf --output=L "
		fi
		eval kselftest_cmd_args="\$${kselftest_cmd_args_ref:-}"
		cmd="$stdbuf ./$BASENAME_TEST $kselftest_cmd_args"
		if [ ! -x "$TEST" ]; then
			echo "# Warning: file $TEST is not executable"
			if [ $(head -n 1 "$TEST" | cut -c -2) = "#!" ]
			then
				interpreter=$(head -n 1 "$TEST" | cut -c 3-)
				cmd="$stdbuf $interpreter ./$BASENAME_TEST"
			else
				echo "not ok $test_num $TEST_HDR_MSG"
				return
			fi
		fi
		cd `dirname $TEST` > /dev/null
		((((( tap_timeout "$cmd" 2>&1; echo $? >&3) |
			tap_prefix >&4) 3>&1) |
			(read xs; exit $xs)) 4>>"$logfile" &&
		echo "ok $test_num $TEST_HDR_MSG") ||
		(rc=$?;	\
		if [ $rc -eq $skip_rc ]; then	\
			echo "ok $test_num $TEST_HDR_MSG # SKIP"
		elif [ $rc -eq $timeout_rc ]; then \
			echo "#"
			echo "not ok $test_num $TEST_HDR_MSG # TIMEOUT $kselftest_timeout seconds"
		else
			echo "not ok $test_num $TEST_HDR_MSG # exit=$rc"
		fi)
		cd - >/dev/null
	fi
}
run_many()
{
	echo "TAP version 13"
	DIR="${PWD#${BASE_DIR}/}"
	test_num=0
	total=$(echo "$@" | wc -w)
	echo "1..$total"
	for TEST in "$@"; do
		BASENAME_TEST=$(basename $TEST)
		test_num=$(( test_num + 1 ))
		if [ -n "$per_test_logging" ]; then
			logfile="/tmp/$BASENAME_TEST"
			cat /dev/null > "$logfile"
		fi
		run_one "$DIR" "$TEST" "$test_num"
	done
}
