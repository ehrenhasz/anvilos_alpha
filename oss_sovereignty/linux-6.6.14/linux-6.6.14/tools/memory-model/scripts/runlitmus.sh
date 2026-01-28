litmus=$1
if test -f "$litmus" -a -r "$litmus"
then
	:
else
	echo ' !!! ' error: \"$litmus\" is not a readable file
	exit 255
fi
if test -z "$LKMM_HW_MAP_FILE" -o ! -e $LKMM_DESTDIR/$litmus.out
then
	herdoptions=${LKMM_HERD_OPTIONS--conf linux-kernel.cfg}
	echo Herd options: $herdoptions > $LKMM_DESTDIR/$litmus.out
	/usr/bin/time $LKMM_TIMEOUT_CMD herd7 $herdoptions $litmus >> $LKMM_DESTDIR/$litmus.out 2>&1
	ret=$?
	if test -z "$LKMM_HW_MAP_FILE"
	then
		exit $ret
	fi
	echo " --- " Automatically generated LKMM output for '"'--hw $LKMM_HW_MAP_FILE'"' run
fi
T=/tmp/checklitmushw.sh.$$
trap 'rm -rf $T' 0 2
mkdir $T
mapfile="Linux2${LKMM_HW_MAP_FILE}.map"
themefile="$T/${LKMM_HW_MAP_FILE}.theme"
herdoptions="-model $LKMM_HW_CAT_FILE"
hwlitmus=`echo $litmus | sed -e 's/\.litmus$/.litmus.'${LKMM_HW_MAP_FILE}'/'`
hwlitmusfile=`echo $hwlitmus | sed -e 's,^.*/,,'`
if ! scripts/simpletest.sh $litmus
then
	echo ' --- ' error: \"$litmus\" contains locking, RCU, or SRCU
	exit 254
fi
gen_theme7 -n 10 -map $mapfile -call Linux.call > $themefile
jingle7 -v -theme $themefile $litmus > $LKMM_DESTDIR/$hwlitmus 2> $T/$hwlitmusfile.jingle7.out
if grep -q "Generated 0 tests" $T/$hwlitmusfile.jingle7.out
then
	echo ' !!! ' jingle7 failed, errors in $hwlitmus.err
	cp $T/$hwlitmusfile.jingle7.out $LKMM_DESTDIR/$hwlitmus.err
	exit 253
fi
/usr/bin/time $LKMM_TIMEOUT_CMD herd7 -unroll 0 $LKMM_DESTDIR/$hwlitmus > $LKMM_DESTDIR/$hwlitmus.out 2>&1
exit $?
