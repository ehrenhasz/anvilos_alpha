PERFRECMEM="-m ,16M"
PERFRECOPT="$PERFRECMEM -e cs_etm//u"
TOOLS=$(dirname $0)
DIR="$TOOLS/$TEST"
BIN="$DIR/$TEST"
if ! test -x "$BIN"; then exit 2; fi
DATD="."
if test -n "$PERF_TEST_CORESIGHT_DATADIR"; then
	DATD="$PERF_TEST_CORESIGHT_DATADIR";
fi
STATD="."
if test -n "$PERF_TEST_CORESIGHT_STATDIR"; then
	STATD="$PERF_TEST_CORESIGHT_STATDIR";
fi
err() {
	echo "$1"
	exit 1
}
check_val_min() {
	STATF="$4"
	if test "$2" -lt "$3"; then
		echo ", FAILED" >> "$STATF"
		err "Sanity check number of $1 is too low ($2 < $3)"
	fi
}
perf_dump_aux_verify() {
	DUMP="$DATD/perf-tmp-aux-dump.txt"
	perf report --stdio --dump -i "$1" | \
		grep -o -e I_ATOM_F -e I_ASYNC -e I_TRACE_INFO > "$DUMP"
	ATOM_FX_NUM=$(grep -c I_ATOM_F "$DUMP")
	ASYNC_NUM=$(grep -c I_ASYNC "$DUMP")
	TRACE_INFO_NUM=$(grep -c I_TRACE_INFO "$DUMP")
	rm -f "$DUMP"
	CHECK_FX_MIN="$2"
	CHECK_ASYNC_MIN="$3"
	CHECK_TRACE_INFO_MIN="$4"
	STATF="$STATD/stats-$TEST-$DATV.csv"
	if ! test -f "$STATF"; then
		echo "ATOM Fx Count, Minimum, ASYNC Count, Minimum, TRACE INFO Count, Minimum" > "$STATF"
	fi
	echo -n "$ATOM_FX_NUM, $CHECK_FX_MIN, $ASYNC_NUM, $CHECK_ASYNC_MIN, $TRACE_INFO_NUM, $CHECK_TRACE_INFO_MIN" >> "$STATF"
	check_val_min "ATOM_FX" "$ATOM_FX_NUM" "$CHECK_FX_MIN" "$STATF"
	check_val_min "ASYNC" "$ASYNC_NUM" "$CHECK_ASYNC_MIN" "$STATF"
	check_val_min "TRACE_INFO" "$TRACE_INFO_NUM" "$CHECK_TRACE_INFO_MIN" "$STATF"
	echo ", Ok" >> "$STATF"
}
perf_dump_aux_tid_verify() {
	TIDS=$(cat "$2")
	FOUND_TIDS=$(perf report --stdio --dump -i "$1" | \
			grep -o "CID=0x[0-9a-z]\+" | sed 's/CID=//g' | \
			uniq | sort | uniq)
	if test -z "$FOUND_TIDS"; then
		FOUND_TIDS=$(perf report --stdio --dump -i "$1" | \
				grep -o "VMID=0x[0-9a-z]\+" | sed 's/VMID=//g' | \
				uniq | sort | uniq)
	fi
	MISSING=""
	for TID2 in $TIDS; do
		FOUND=""
		for TIDHEX in $FOUND_TIDS; do
			TID=$(printf "%i" $TIDHEX)
			if test "$TID" -eq "$TID2"; then
				FOUND="y"
				break
			fi
		done
		if test -z "$FOUND"; then
			MISSING="$MISSING $TID"
		fi
	done
	if test -n "$MISSING"; then
		err "Thread IDs $MISSING not found in perf AUX data"
	fi
}
