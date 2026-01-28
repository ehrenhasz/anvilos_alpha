lscpu | grep -q "aarch64" || exit 2
PERF_DATA=$(mktemp /tmp/__perf_test.perf.data.XXXXX)
TEST_PROGRAM="perf test -w leafloop"
cleanup_files()
{
	rm -f "$PERF_DATA"
}
trap cleanup_files EXIT TERM INT
perf record -o "$PERF_DATA" --call-graph fp -e cycles//u -D 1000 --user-callchains -- $TEST_PROGRAM 2> /dev/null &
PID=$!
echo " + Recording (PID=$PID)..."
sleep 2
echo " + Stopping perf-record..."
kill $PID
wait $PID
perf script -i "$PERF_DATA" -F comm,ip,sym | head -n4
perf script -i "$PERF_DATA" -F comm,ip,sym | head -n4 | \
	awk '{ if ($2 != "") sym[i++] = $2 } END { if (sym[0] != "leaf" ||
						       sym[1] != "parent" ||
						       sym[2] != "leafloop") exit 1 }'
