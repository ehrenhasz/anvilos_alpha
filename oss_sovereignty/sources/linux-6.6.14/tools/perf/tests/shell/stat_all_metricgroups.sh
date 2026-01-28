set -e
for m in $(perf list --raw-dump metricgroups); do
  echo "Testing $m"
  perf stat -M "$m" -a true
done
exit 0
