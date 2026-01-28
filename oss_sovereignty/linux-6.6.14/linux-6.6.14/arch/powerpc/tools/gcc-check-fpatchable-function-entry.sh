set -e
set -o pipefail
echo "int func() { return 0; }" | \
    $* -m64 -mabi=elfv2 -S -x c -O2 -fpatchable-function-entry=2 - -o - 2> /dev/null | \
    grep -q "__patchable_function_entries"
echo "int x; int func() { return x; }" | \
    $* -m64 -mabi=elfv2 -S -x c -O2 -fpatchable-function-entry=2 - -o - 2> /dev/null | \
    awk 'BEGIN { RS = ";" } /\.localentry.*nop.*\n[[:space:]]*nop/ { print $0 }' | \
    grep -q "func:"
exit 0
