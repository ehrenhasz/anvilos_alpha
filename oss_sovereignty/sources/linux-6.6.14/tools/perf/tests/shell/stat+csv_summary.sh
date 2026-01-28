set -e
perf stat -e cycles  -x' ' -I1000 --interval-count 1 --summary 2>&1 | \
grep -e summary | \
while read summary _num _event _run _pct
do
	if [ $summary != "summary" ]; then
		exit 1
	fi
done
perf stat -e cycles  -x' ' -I1000 --interval-count 1 --summary --no-csv-summary 2>&1 | \
grep -e summary | \
while read _num _event _run _pct
do
	exit 1
done
exit 0
