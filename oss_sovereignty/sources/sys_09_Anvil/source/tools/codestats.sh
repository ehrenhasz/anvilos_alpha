if [ ! -d py -o ! -d ports/unix -o ! -d ports/stm32 ]; then
    echo "script must be run from root of the repository"
    exit 1
fi
output=codestats.dat
RM=/bin/rm
AWK=awk
MAKE="make -j2"
bin_unix=ports/unix/build-standard/micropython
bin_stm32=ports/stm32/build-PYBV10/firmware.elf
bin_barearm_1=ports/bare-arm/build/flash.elf
bin_barearm_2=ports/bare-arm/build/firmware.elf
bin_minimal=ports/minimal/build/firmware.elf
bin_cc3200_1=ports/cc3200/build/LAUNCHXL/application.axf
bin_cc3200_2=ports/cc3200/build/LAUNCHXL/release/application.axf
bin_cc3200_3=ports/cc3200/build/WIPY/release/application.axf
size_unix="0"
size_stm32="0"
size_barearm="0"
size_minimal="0"
size_cc3200="0"
pystones="0"
pystoneavg=/tmp/pystoneavg.py
cat > $pystoneavg << EOF
import pystone
samples = [pystone.pystones(300000)[1] for i in range(5)]
samples.sort()
stones = sum(samples[1:-1]) / (len(samples) - 2) # exclude smallest and largest
print("stones %g" % stones)
EOF
function get_size() {
    if [ -r $2 ]; then
        size $2 | tail -n1 | $AWK '{print $1}'
    else
        echo $1
    fi
}
function get_size2() {
    if [ -r $2 ]; then
        size $2 | tail -n1 | $AWK '{print $1}'
    elif [ -r $3 ]; then
        size $3 | tail -n1 | $AWK '{print $1}'
    else
        echo $1
    fi
}
function get_size3() {
    if [ -r $2 ]; then
        size $2 | tail -n1 | $AWK '{print $1}'
    elif [ -r $3 ]; then
        size $3 | tail -n1 | $AWK '{print $1}'
    elif [ -r $4 ]; then
        size $4 | tail -n1 | $AWK '{print $1}'
    else
        echo $1
    fi
}
if [ -r $output ]; then
    last_rev=$(tail -n1 $output | $AWK '{print $1}')
else
    echo "# hash size_unix size_stm32 size_barearm size_minimal size_cc3200 pystones" > $output
    last_rev="v1.0"
fi
hashes=$(git log --format=format:"%H" --reverse ${last_rev}..master)
for hash in $hashes; do
    git checkout $hash
    if [ $? -ne 0 ]; then
        echo "aborting"
        exit 1
    fi
    if grep -q '#if defined(MP_CLOCKS_PER_SEC) && (MP_CLOCKS_PER_SEC == 1000000) // POSIX' unix/modtime.c; then
        echo apply patch
        git apply - << EOF
diff --git a/unix/modtime.c b/unix/modtime.c
index 77d2945..dae0644 100644
--- a/unix/modtime.c
+++ b/unix/modtime.c
@@ -55,10 +55,8 @@ void msec_sleep_tv(struct timeval *tv) {
-#if defined(MP_CLOCKS_PER_SEC) && (MP_CLOCKS_PER_SEC == 1000000) // POSIX
-#define CLOCK_DIV 1000.0
-#elif defined(MP_CLOCKS_PER_SEC) && (MP_CLOCKS_PER_SEC == 1000) // WIN32
-#define CLOCK_DIV 1.0
+#if defined(MP_CLOCKS_PER_SEC)
+#define CLOCK_DIV (MP_CLOCKS_PER_SEC / 1000.0F)
EOF
    fi
    $RM $bin_unix
    $MAKE -C ports/unix CFLAGS_EXTRA=-DNDEBUG
    size_unix=$(get_size $size_unix $bin_unix)
    git checkout unix/modtime.c
    $RM $bin_stm32
    $MAKE -C ports/stm32 board=PYBV10
    size_stm32=$(get_size $size_stm32 $bin_stm32)
    $RM $bin_barearm_1 $bin_barearm_2
    $MAKE -C ports/bare-arm
    size_barearm=$(get_size2 $size_barearm $bin_barearm_1 $bin_barearm_2)
    if [ -r ports/minimal/Makefile ]; then
        $RM $bin_minimal
        $MAKE -C ports/minimal CROSS=1
        size_minimal=$(get_size $size_minimal $bin_minimal)
    fi
    if [ -r ports/cc3200/Makefile ]; then
        $RM $bin_cc3200_1 $bin_cc3200_2 $bin_cc3200_3
        $MAKE -C ports/cc3200 BTARGET=application
        size_cc3200=$(get_size3 $size_cc3200 $bin_cc3200_1 $bin_cc3200_2 $bin_cc3200_3)
    fi
    if [ -x $bin_unix ]; then
        new_pystones=$($bin_unix $pystoneavg)
        if echo $new_pystones | grep -q "^stones"; then
            pystones=$(echo $new_pystones | $AWK '{print $2}')
        fi
    fi
    echo "$hash $size_unix $size_stm32 $size_barearm $size_minimal $size_cc3200 $pystones" >> $output
done
git checkout master
$RM $pystoneavg
