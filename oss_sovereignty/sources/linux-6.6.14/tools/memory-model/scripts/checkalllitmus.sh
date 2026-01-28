. scripts/parseargs.sh
litmusdir=litmus-tests
if test -d "$litmusdir" -a -r "$litmusdir" -a -x "$litmusdir"
then
	:
else
	echo ' --- ' error: $litmusdir is not an accessible directory
	exit 255
fi
if test "$LKMM_DESTDIR" != "."
then
	find $litmusdir -type d -print |
	( cd "$LKMM_DESTDIR"; sed -e 's/^/mkdir -p /' | sh )
fi
ret=0
for i in $litmusdir/*.litmus
do
	if test -n "$LKMM_HW_MAP_FILE" && ! scripts/simpletest.sh $i
	then
		continue
	fi
	if ! scripts/checklitmus.sh $i
	then
		ret=1
	fi
done
if test "$ret" -ne 0
then
	echo " ^^^ VERIFICATION MISMATCHES" 1>&2
else
	echo All litmus tests verified as was expected. 1>&2
fi
exit $ret
