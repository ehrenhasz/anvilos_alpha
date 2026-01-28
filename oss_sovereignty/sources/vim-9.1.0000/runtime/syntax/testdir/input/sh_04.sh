Variable=${VariableB:-{VariableC}}
Variable=${VariableB:-${VariableC:-{Var3:=eng}}}
Variable=${VariableB:-${VariableC:-{Var3:=eng}}}
Variable=${VariableB:=${VariableC:={Var3:=${Var4:-eng}}}}
Variable=${VariableB:=${VariableC:={Var3:=${Var4:-${Var5:-eng}}}}}
Variable=${VariableB:=${VariableC:={Var3:=${Var4:-${Var5:-$Var6}}}}}
Variable="${VariableB:-${VariableC:-{Var3:=eng}}}"
Variable="${VariableB:=${VariableC:={Var3:=${Var4:-eng}}}}"
: ${VariableB:-${VariableC:-{Var3:=eng}}}
: ${VariableB:=${VariableC:={Var3:=${Var4:-eng}}}}
: ${VariableB:-${VariableC:-eng}}
: "${VariableB:-${VariableC:-eng}}"
Variable=${VariableB:=${VariableC:={Var3:=${Var4:-eng}}}
Variable=${VariableB:-${VariableC:-{Var3:=eng}}
