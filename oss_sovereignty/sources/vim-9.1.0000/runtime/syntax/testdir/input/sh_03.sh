Variable='value'
Variable="value"
VariableA="pat1xxpat2"
VariableB="pat2xxpat1"
echo ${
echo ${
echo ${VariableA
echo ${VariableA
echo ${VariableB%pat1}
echo ${VariableB%%pat1}
Variable=${VariableB:+${VariableC:=eng}}	
Variable=${VariableB:-${VariableC:-eng}}	
Variable='${VariableB:+${VariableC:=eng}}'
Variable='${VariableB:-${VariableC:-eng}}'
Variable="${VariableB:+${VariableC:=eng}}"	
Variable="${VariableB:-${VariableC:-eng}}"  
: ${VariableB:-${VariableC:-eng}}
: "${VariableB:-${VariableC:-eng}}"
: '${VariableB:-${VariableC:-eng}}'
Variable=${VariableB:-${VariableC:-${VariableD:-${VariableE:=eng}}}}
       :        ${VariableB:=${VariableC:-${VariableD:-${VariableE:=eng}}}}
