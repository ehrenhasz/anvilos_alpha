set -e
set -o pipefail
echo "int func() { return 0; }" | \
    $* -m64 -S -x c -O2 -p -mprofile-kernel - -o - \
    2> /dev/null | grep -q "_mcount"
echo -e "#include <linux/compiler.h>\nnotrace int func() { return 0; }" | \
    $* -m64 -S -x c -O2 -p -mprofile-kernel - -o - \
    2> /dev/null | grep -q "_mcount" && \
    exit 1
exit 0
