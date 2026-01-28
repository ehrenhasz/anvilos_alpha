Variable='value'
Variable="value"
VariableA="pat1xxpat2"
VariableB="pat2xxpat1"
echo ${#}
echo ${#VariableA}
echo ${VariableA#pat1}
echo ${VariableA##pat1}
echo ${VariableB%pat1}
echo ${VariableB%%pat1}
Variable=${VariableB:+${VariableC:=eng}}	# :+ seems to work for ksh as well as bash
Variable=${VariableB:-${VariableC:-eng}}	# :- is ksh and bash
Variable='${VariableB:+${VariableC:=eng}}'
Variable='${VariableB:-${VariableC:-eng}}'
Variable="${VariableB:+${VariableC:=eng}}"	# :+ seems to work for ksh as well as bash
Variable="${VariableB:-${VariableC:-eng}}"  # :- is ksh and bash
: ${VariableB:-${VariableC:-eng}}
: "${VariableB:-${VariableC:-eng}}"
: '${VariableB:-${VariableC:-eng}}'
Variable=${VariableB:-${VariableC:-${VariableD:-${VariableE:=eng}}}}
       :        ${VariableB:=${VariableC:-${VariableD:-${VariableE:=eng}}}}
