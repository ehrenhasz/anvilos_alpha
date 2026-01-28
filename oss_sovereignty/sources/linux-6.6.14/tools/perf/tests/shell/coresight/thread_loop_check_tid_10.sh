TEST="thread_loop"
. "$(dirname $0)"/../lib/coresight.sh
ARGS="10 1"
DATV="check-tid-10th"
DATA="$DATD/perf-$TEST-$DATV.data"
STDO="$DATD/perf-$TEST-$DATV.stdout"
SHOW_TID=1 perf record -s $PERFRECOPT -o "$DATA" "$BIN" $ARGS > $STDO
perf_dump_aux_tid_verify "$DATA" "$STDO"
err=$?
exit $err
