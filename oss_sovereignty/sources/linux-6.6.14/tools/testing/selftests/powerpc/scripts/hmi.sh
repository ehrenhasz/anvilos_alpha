if [ -x ./getscom ] && [ -x ./putscom ]; then
	GETSCOM=./getscom
	PUTSCOM=./putscom
elif which getscom > /dev/null; then
	GETSCOM=$(which getscom)
	PUTSCOM=$(which putscom)
else
	cat <<EOF
Can't find getscom/putscom in . or \$PATH.
See https://github.com/open-power/skiboot.
The tool is in external/xscom-utils
EOF
	exit 1
fi
expected_hmis=8
COUNT_HMIS() {
    dmesg | grep -c 'Harmless Hypervisor Maintenance interrupt'
}
ppc64_cpu --smt-snooze-delay=1000000000
trap "ppc64_cpu --smt-snooze-delay=100" 0 1
grep -E -o 'OCC: Chip [0-9a-f]+ Core [0-9a-f]' < /sys/firmware/opal/msglog |
while read chipcore; do
	chip=$(echo "$chipcore"|awk '{print $3}')
	core=$(echo "$chipcore"|awk '{print $5}')
	fir="0x1${core}013100"
	if [ "$($GETSCOM -c 0x${chip} $fir)" != 0 ]; then
		echo "FIR was not zero before injection for chip $chip, core $core. Aborting!"
		echo "Result of $GETSCOM -c 0x${chip} $fir:"
		$GETSCOM -c 0x${chip} $fir
		echo "If you get a -5 error, the core may be in idle state. Try stress-ng."
		echo "Otherwise, try $PUTSCOM -c 0x${chip} $fir 0"
		exit 1
	fi
	old_hmis=$(COUNT_HMIS)
	echo "Injecting HMI on core $core, chip $chip" | tee /dev/kmsg
	if ! $PUTSCOM -c 0x${chip} $fir 2000000000000000 > /dev/null; then
		echo "Error injecting. Aborting!"
		exit 1
	fi
	i=0;
	new_hmis=$(COUNT_HMIS)
	while [ $new_hmis -lt $((old_hmis + expected_hmis)) ] && [ $i -lt 12 ]; do
	    echo "Seen $((new_hmis - old_hmis)) HMI(s) out of $expected_hmis expected, sleeping"
	    sleep 5;
	    i=$((i + 1))
	    new_hmis=$(COUNT_HMIS)
	done
	if [ $i = 12 ]; then
	    echo "Haven't seen expected $expected_hmis recoveries after 1 min. Aborting."
	    exit 1
	fi
	echo "Processed $expected_hmis events; presumed success. Check dmesg."
	echo ""
done
