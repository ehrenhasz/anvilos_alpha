. scripts/parseargs.sh
ret=0
for i in "$@"
do
	if scripts/checklitmus.sh $i
	then
		:
	else
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
