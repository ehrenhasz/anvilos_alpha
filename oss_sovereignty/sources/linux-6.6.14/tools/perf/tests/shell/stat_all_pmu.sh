set -e
for p in $(perf list --raw-dump pmu | sed 's/[[:graph:]]\+?[[:graph:]]\+[[:space:]]//g'); do
  echo "Testing $p"
  result=$(perf stat -e "$p" true 2>&1)
  if ! echo "$result" | grep -q "$p" && ! echo "$result" | grep -q "<not supported>" ; then
    result=$(perf stat -e "$p" perf bench internals synthesize 2>&1)
    if ! echo "$result" | grep -q "$p" ; then
      echo "Event '$p' not printed in:"
      echo "$result"
      exit 1
    fi
  fi
done
exit 0
