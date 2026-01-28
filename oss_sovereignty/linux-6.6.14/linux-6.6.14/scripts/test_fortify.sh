set -e
IN="$1"
shift
FILE="${IN##*/}"
FUNC="${FILE#*-}"
FUNC="${FUNC%%-*}"
FUNC="${FUNC%%.*}"
WANT="__${FILE%%-*}"
OUT="$1"
shift
TMP="${OUT}.tmp"
NM="$1"
shift
__cleanup() {
	rm -f "$TMP"
}
trap __cleanup EXIT
export LANG=C
status=
if "$@" -Werror -c "$IN" -o "$OUT".o 2> "$TMP" ; then
	if ! $NM -A "$OUT".o | grep -m1 "\bU ${WANT}$" >>"$TMP" ; then
		status="warning: unsafe ${FUNC}() usage lacked '$WANT' symbol in $IN"
	fi
else
	if ! grep -Eq -m1 "error: call to .?\b${WANT}\b.?" "$TMP" ; then
		status="warning: unsafe ${FUNC}() usage lacked '$WANT' warning in $IN"
	fi
fi
if [ -n "$status" ]; then
	echo "$status" | tee "$OUT" >&2
else
	echo "ok: unsafe ${FUNC}() usage correctly detected with '$WANT' in $IN" >"$OUT"
fi
cat "$TMP" >>"$OUT"
