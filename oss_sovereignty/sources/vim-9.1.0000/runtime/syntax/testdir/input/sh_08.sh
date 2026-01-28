[ -t 0 ] && date
Variable1=${VariableA:-This is a Text}
Variable2=${VariableA:=This is a Text}
Variable3=${VariableA:?This is a Text}
echo "$Variable1" ; echo "$Variable2" ; echo "$Variable3"
[ -t 0 ] && echo "\n`date`" && unset VariableA
Variable1=${VariableA:-$HOME This is a Text}
Variable2=${VariableA:=$HOME This is a Text}
Variable3=${VariableA:?$HOME This is a Text}
echo "$Variable1" ; echo "$Variable2" ; echo "$Variable3"
[ -t 0 ] && echo "\n`date`" && unset VariableA
Variable1=${VariableA:-This is a Text in $HOME}
Variable2=${VariableA:=This is a Text in $HOME}
Variable3=${VariableA:+This is a Text in $HOME}       
Variable1=${VariableA:-This is a Text in $HOME too}
Variable2=${VariableA:=This is a Text in $HOME too}
Variable3=${VariableA:+This is a Text in $HOME too}
echo "$Variable1" ; echo "$Variable2" ; echo "$Variable3"
[ -t 0 ] && echo "\n`date`" && unset VariableA
Variable1=${VariableA:-$SHELL}
Variable1=${VariableA:-$SHELL This is a Text in $HOME}
Variable2=${VariableA:=$SHELL This is a Text in $HOME}
Variable3=${VariableA:+$SHELL This is a Text in $HOME}
echo "$Variable1" ; echo "$Variable2" ; echo "$Variable3"
[ -t 0 ] && echo "\n`date`" && unset VariableA
Variable1=${VariableA:-"This is a Text in $HOME $SHELL"}
Variable1=${VariableA:-This is a Text in $HOME $SHELL}
Variable2=${VariableA:=This is a Text in $HOME $SHELL}
Variable3=${VariableA:+This is a Text in $HOME $SHELL}
echo "$Variable1" ; echo "$Variable2" ; echo "$Variable3"
[ -t 0 ] && echo "\n`date`" && unset VariableA
: ${VariableA:-This is a Text}
: ${VariableA:-$HOME This is a Text}
: ${VariableA:-This is a Text in $HOME}
: ${VariableA:-$SHELL This is a Text in $HOME}
: ${VariableA:-This is a Text in $HOME $SHELL}
[ -t 0 ] && echo "\n`date`" && unset VariableA
: ${VariableA-This is a Text}
: ${VariableA-$HOME This is a Text}
: ${VariableA-This is a Text in $HOME}
: ${VariableA-$SHELL This is a Text in $HOME}
: ${VariableA-This is a Text in $HOME $SHELL}
Variable4=${Variable4:?This is an Error Message}
Variable4=${Variable4:?This is an Error Message from `date`}
: ${Variable4:?This is an Error Message}
: ${Variable4:?This is an Error Message from `date`}
exit $?
if [ $
	echo whatever
	exit 1
fi
