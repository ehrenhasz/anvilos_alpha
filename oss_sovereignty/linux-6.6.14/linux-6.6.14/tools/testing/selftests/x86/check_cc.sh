CC="$1"
TESTPROG="$2"
shift 2
if [ -n "$CC" ] && $CC -o /dev/null "$TESTPROG" -O0 "$@" 2>/dev/null; then
    echo 1
else
    echo 0
fi
exit 0
