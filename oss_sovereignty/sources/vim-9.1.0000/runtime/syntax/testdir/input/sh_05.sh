[ -t 0 ] && date
Variable1=value1
Variable2='value2'
Variable3="value3"
echo "$Variable1" "$Variable2" "$Variable3"
[ -t 0 ] && echo "\ndate"
Variable1=$HOME
Variable2='$HOME'
Variable3="$HOME"
echo "$Variable1" "$Variable2" "$Variable3"
[ -t 0 ] && echo "\ndate"
Variable1=$HOME$SHELL
Variable2=$HOME.$SHELL
Variable3=$HOME.$SHELL+$HOME-$SHELL/$HOME
echo "$Variable1" "$Variable2" "$Variable3"
[ -t 0 ] && echo "\ndate"
Variable1=`date`
Variable2=`id -ng`
Variable3=`id -ng | wc -c`
echo "$Variable1" "$Variable2" "$Variable3"
[ -t 0 ] && echo "\ndate"
Variable1=${VariableA:=eng1}
Variable2=${VariableA:-eng2}
Variable3=${VariableA:?eng3}
Variable3=${VariableA:+eng3}
echo "$Variable1" "$Variable2" "$Variable3"
[ -t 0 ] && echo "\ndate"
Variable1=${VariableA:='eng1'}
Variable2=${VariableA:-'eng2'}
Variable3=${VariableA:?'eng3'}
Variable3=${VariableA:+'eng3'}
echo "$Variable1" "$Variable2" "$Variable3"
[ -t 0 ] && echo "\ndate"
Variable1=${VariableA:="eng1"}
Variable2=${VariableA:-"eng2"}
Variable3=${VariableA:?"eng3"}
Variable3=${VariableA:+"eng3"}
echo "$Variable1" "$Variable2" "$Variable3"
Variable1=${VariableA=eng1}
Variable2=${VariableA-eng2}
Variable3=${VariableA?eng3}
Variable3=${VariableA+eng3}
Variable1=${VariableA='eng1'}
Variable2=${VariableA-'eng2'}
Variable3=${VariableA?'eng3'}
Variable3=${VariableA+'eng3'}
Variable1=${VariableA="eng1"}
Variable2=${VariableA-"eng2"}
Variable3=${VariableA?"eng3"}
Variable3=${VariableA+"eng3"}
[ -t 0 ] && echo "\ndate"
Variable1=${VariableA:=$HOME}
Variable2=${VariableA:-$HOME}
Variable3=${VariableA:?$HOME}
Variable3=${VariableA:+$HOME}
echo "$Variable1" "$Variable2" "$Variable3"
[ -t 0 ] && echo "\ndate"
Variable1=${VariableA:='$HOME'}
Variable2=${VariableA:-'$HOME'}
Variable3=${VariableA:?'$HOME'}
Variable3=${VariableA:+'$HOME'}
echo "$Variable1" "$Variable2" "$Variable3"
[ -t 0 ] && echo "\ndate"
Variable1=${VariableA:="$HOME"}
Variable2=${VariableA:-"$HOME"}
Variable3=${VariableA:?"$HOME"}
Variable3=${VariableA:+"$HOME"}
echo "$Variable1" "$Variable2" "$Variable3"
[ -t 0 ] && echo "\ndate"
Variable1=${VariableA:=`date`}
Variable2=${VariableA:-`date`}
Variable3=${VariableA:?`date`}
Variable3=${VariableA:+`date`}
echo "$Variable1" "$Variable2" "$Variable3"
[ -t 0 ] && echo "\ndate"
Variable1=${VariableA:=`id -ng`}
Variable2=${VariableA:-`id -ng`}
Variable3=${VariableA:?`id -ng`}
Variable3=${VariableA:+`id -ng`}
echo "$Variable1" "$Variable2" "$Variable3"
[ -t 0 ] && echo "\ndate"
Variable1=${VariableA:=`id -ng | wc -c`}
Variable2=${VariableA:-`id -ng | wc -c`}
Variable3=${VariableA:?`id -ng | wc -c`}
Variable3=${VariableA:+`id -ng | wc -c`}
echo "$Variable1" "$Variable2" "$Variable3"
[ -t 0 ] && echo "\ndate"
Variable1=${VariableA:-${VarB:=eng1}}
Variable2=${VariableA:-${VarB:-eng2}}
Variable3=${VariableA:-${VarB:?eng3}}
Variable3=${VariableA:-${VarB:+eng3}}
echo "$Variable1" "$Variable2" "$Variable3"
[ -t 0 ] && echo "\ndate"
Variable1=${VariableA:-${VarB:='eng1'}}
Variable2=${VariableA:-${VarB:-'eng2'}}
Variable3=${VariableA:-${VarB:?'eng3'}}
Variable3=${VariableA:-${VarB:+'eng3'}}
echo "$Variable1" "$Variable2" "$Variable3"
[ -t 0 ] && echo "\ndate"
Variable1=${VariableA:-${VarB:="eng1"}}
Variable2=${VariableA:-${VarB:-"eng2"}}
Variable3=${VariableA:-${VarB:?"eng3"}}
Variable3=${VariableA:-${VarB:+"eng3"}}
echo "$Variable1" "$Variable2" "$Variable3"
[ -t 0 ] && echo "\ndate"
Variable1=${VariableA:-${VarB:=$HOME}}
Variable2=${VariableA:-${VarB:-$HOME}}
Variable3=${VariableA:-${VarB:?$HOME}}
Variable3=${VariableA:-${VarB:+$HOME}}
echo "$Variable1" "$Variable2" "$Variable3"
[ -t 0 ] && echo "\ndate"
Variable1=${VariableA:-${VarB:='$HOME'}}
Variable2=${VariableA:-${VarB:-'$HOME'}}
Variable3=${VariableA:-${VarB:?'$HOME'}}
Variable3=${VariableA:-${VarB:+'$HOME'}}
echo "$Variable1" "$Variable2" "$Variable3"
[ -t 0 ] && echo "\ndate"
Variable1=${VariableA:-${VarB:="$HOME"}}
Variable2=${VariableA:-${VarB:-"$HOME"}}
Variable3=${VariableA:-${VarB:?"$HOME"}}
Variable3=${VariableA:-${VarB:+"$HOME"}}
echo "$Variable1" "$Variable2" "$Variable3"
[ -t 0 ] && echo "\ndate"
Variable1=${VariableA:-${VarB:=`date`}}
Variable2=${VariableA:-${VarB:-`date`}}
Variable3=${VariableA:-${VarB:?`date`}}
Variable3=${VariableA:-${VarB:+`date`}}
echo "$Variable1" "$Variable2" "$Variable3"
[ -t 0 ] && echo "\ndate"
Variable1=${VariableA:-${VarB:=`id -ng`}}
Variable2=${VariableA:-${VarB:-`id -ng`}}
Variable3=${VariableA:-${VarB:?`id -ng`}}
Variable3=${VariableA:-${VarB:+`id -ng`}}
echo "$Variable1" "$Variable2" "$Variable3"
[ -t 0 ] && echo "\ndate"
Variable1=${VariableA:-${VarB:=`id -ng | wc -c`}}
Variable2=${VariableA:-${VarB:-`id -ng | wc -c`}}
Variable3=${VariableA:-${VarB:?`id -ng | wc -c`}}
Variable3=${VariableA:-${VarB:+`id -ng | wc -c`}}
echo "$Variable1" "$Variable2" "$Variable3"
[ -t 0 ] && echo "\ndate"
Variable1=${VariableA:-${VarB:-${VarC:=eng1}}}
Variable2=${VariableA:-${VarB:-${VarC:-eng2}}}
Variable3=${VariableA:-${VarB:-${VarC:?eng3}}}
Variable3=${VariableA:-${VarB:-${VarC:+eng3}}}
echo "$Variable1" "$Variable2" "$Variable3"
[ -t 0 ] && echo "\ndate"
Variable1=${VariableA:-${VarB:-${VarC:='eng1'}}}
Variable2=${VariableA:-${VarB:-${VarC:-'eng2'}}}
Variable3=${VariableA:-${VarB:-${VarC:?'eng3'}}}
Variable3=${VariableA:-${VarB:-${VarC:+'eng3'}}}
echo "$Variable1" "$Variable2" "$Variable3"
[ -t 0 ] && echo "\ndate"
Variable1=${VariableA:-${VarB:-${VarC:="eng1"}}}
Variable2=${VariableA:-${VarB:-${VarC:-"eng2"}}}
Variable3=${VariableA:-${VarB:-${VarC:?"eng3"}}}
Variable3=${VariableA:-${VarB:-${VarC:+"eng3"}}}
echo "$Variable1" "$Variable2" "$Variable3"
[ -t 0 ] && echo "\ndate"
Variable1=${VariableA:-${VarB:-${VarC:=$HOME}}}
Variable2=${VariableA:-${VarB:-${VarC:-$HOME}}}
Variable3=${VariableA:-${VarB:-${VarC:?$HOME}}}
Variable3=${VariableA:-${VarB:-${VarC:+$HOME}}}
echo "$Variable1" "$Variable2" "$Variable3"
[ -t 0 ] && echo "\ndate"
Variable1=${VariableA:-${VarB:-${VarC:='$HOME'}}}
Variable2=${VariableA:-${VarB:-${VarC:-'$HOME'}}}
Variable3=${VariableA:-${VarB:-${VarC:?'$HOME'}}}
Variable3=${VariableA:-${VarB:-${VarC:+'$HOME'}}}
echo "$Variable1" "$Variable2" "$Variable3"
[ -t 0 ] && echo "\ndate"
Variable1=${VariableA:-${VarB:-${VarC:="$HOME"}}}
Variable2=${VariableA:-${VarB:-${VarC:-"$HOME"}}}
Variable3=${VariableA:-${VarB:-${VarC:?"$HOME"}}}
Variable3=${VariableA:-${VarB:-${VarC:?"$HOME"}}}
Variable3=${VariableA:-${VarB:-${VarC:+"$HOME"}}}
echo "$Variable1" "$Variable2" "$Variable3"
[ -t 0 ] && echo "\ndate"
Variable1=${VariableA:-${VarB:-${VarC:=`date`}}}
Variable2=${VariableA:-${VarB:-${VarC:-`date`}}}
Variable3=${VariableA:-${VarB:-${VarC:?`date`}}}
Variable3=${VariableA:-${VarB:-${VarC:+`date`}}}
echo "$Variable1" "$Variable2" "$Variable3"
[ -t 0 ] && echo "\ndate"
Variable1=${VariableA:-${VarB:-${VarC:=`id -ng`}}}
Variable2=${VariableA:-${VarB:-${VarC:-`id -ng`}}}
Variable3=${VariableA:-${VarB:-${VarC:?`id -ng`}}}
Variable3=${VariableA:-${VarB:-${VarC:+`id -ng`}}}
echo "$Variable1" "$Variable2" "$Variable3"
[ -t 0 ] && echo "\ndate"
Variable1=${VariableA:-${VarB:-${VarC:=`id -ng | wc -c`}}}
Variable2=${VariableA:-${VarB:-${VarC:-`id -ng | wc -c`}}}
Variable3=${VariableA:-${VarB:-${VarC:?`id -ng | wc -c`}}}
Variable3=${VariableA:-${VarB:-${VarC:+`id -ng | wc -c`}}}
echo "$Variable1" "$Variable2" "$Variable3"
[ -t 0 ] && echo "\ndate"
Variable1=${VariableA:-${VarB:-${VarC:-${VarD:=eng1}}}}
Variable2=${VariableA:-${VarB:-${VarC:-${VarD:-eng2}}}}
Variable3=${VariableA:-${VarB:-${VarC:-${VarD:?eng3}}}}
Variable3=${VariableA:-${VarB:-${VarC:-${VarD:+eng3}}}}
echo "$Variable1" "$Variable2" "$Variable3"
[ -t 0 ] && echo "\ndate"
Variable1=${VariableA:-${VarB:-${VarC:-${VarD:='eng1'}}}}
Variable2=${VariableA:-${VarB:-${VarC:-${VarD:-'eng2'}}}}
Variable3=${VariableA:-${VarB:-${VarC:-${VarD:?'eng3'}}}}
Variable3=${VariableA:-${VarB:-${VarC:-${VarD:+'eng3'}}}}
echo "$Variable1" "$Variable2" "$Variable3"
[ -t 0 ] && echo "\ndate"
Variable1=${VariableA:-${VarB:-${VarC:-${VarD:="eng1"}}}}
Variable2=${VariableA:-${VarB:-${VarC:-${VarD:-"eng2"}}}}
Variable3=${VariableA:-${VarB:-${VarC:-${VarD:?"eng3"}}}}
Variable3=${VariableA:-${VarB:-${VarC:-${VarD:+"eng3"}}}}
echo "$Variable1" "$Variable2" "$Variable3"
[ -t 0 ] && echo "\ndate"
Variable1=${VariableA:-${VarB:-${VarC:-${VarD:=$HOME}}}}
Variable2=${VariableA:-${VarB:-${VarC:-${VarD:-$HOME}}}}
Variable3=${VariableA:-${VarB:-${VarC:-${VarD:?$HOME}}}}
Variable3=${VariableA:-${VarB:-${VarC:-${VarD:+$HOME}}}}
echo "$Variable1" "$Variable2" "$Variable3"
[ -t 0 ] && echo "\ndate"
Variable1=${VariableA:-${VarB:-${VarC:-${VarD:='$HOME'}}}}
Variable2=${VariableA:-${VarB:-${VarC:-${VarD:-'$HOME'}}}}
Variable3=${VariableA:-${VarB:-${VarC:-${VarD:?'$HOME'}}}}
Variable3=${VariableA:-${VarB:-${VarC:-${VarD:+'$HOME'}}}}
echo "$Variable1" "$Variable2" "$Variable3"
[ -t 0 ] && echo "\ndate"
Variable1=${VariableA:-${VarB:-${VarC:-${VarD:="$HOME"}}}}
Variable2=${VariableA:-${VarB:-${VarC:-${VarD:-"$HOME"}}}}
Variable3=${VariableA:-${VarB:-${VarC:-${VarD:?"$HOME"}}}}
Variable3=${VariableA:-${VarB:-${VarC:-${VarD:+"$HOME"}}}}
echo "$Variable1" "$Variable2" "$Variable3"
[ -t 0 ] && echo "\ndate"
Variable1=${VariableA:-${VarB:-${VarC:-${VarD:=`date`}}}}
Variable2=${VariableA:-${VarB:-${VarC:-${VarD:-`date`}}}}
Variable3=${VariableA:-${VarB:-${VarC:-${VarD:?`date`}}}}
Variable3=${VariableA:-${VarB:-${VarC:-${VarD:+`date`}}}}
echo "$Variable1" "$Variable2" "$Variable3"
[ -t 0 ] && echo "\ndate"
Variable1=${VariableA:-${VarB:-${VarC:-${VarD:=`id -ng`}}}}
Variable2=${VariableA:-${VarB:-${VarC:-${VarD:-`id -ng`}}}}
Variable3=${VariableA:-${VarB:-${VarC:-${VarD:?`id -ng`}}}}
Variable3=${VariableA:-${VarB:-${VarC:-${VarD:+`id -ng`}}}}
echo "$Variable1" "$Variable2" "$Variable3"
[ -t 0 ] && echo "\ndate"
Variable1=${VariableA:-${VarB:-${VarC:-${VarD:=`id -ng | wc -c`}}}}
Variable2=${VariableA:-${VarB:-${VarC:-${VarD:-`id -ng | wc -c`}}}}
Variable3=${VariableA:-${VarB:-${VarC:-${VarD:?`id -ng | wc -c`}}}}
Variable3=${VariableA:-${VarB:-${VarC:-${VarD:+`id -ng | wc -c`}}}}
echo "$Variable1" "$Variable2" "$Variable3"
Variable1=${VariableA-${VarB-${VarC-${VarD=`id -ng | wc -c`}}}}
Variable4=${Variable4:?}
Variable4=${Variable4:?OK}
Variable4=${Variable4:?`date`}
Variable4=${Variable4:?'an OK string'}
Variable4=${Variable4:?"an OK string"}
Variable4=${Variable4:?$HOME$SHELL}
Variable4=${Variable4:?$HOME:$SHELL}
Variable4=${Variable4:?This is OK}
Variable4=${Variable4:?This is OK, too: `date`}
Variable5=${
