Function1 () {
echo "Function1: for loop inside a function:\t\c"
[ "$*" ] || echo "none\c"
for Var
do
    [ 1 $Var 2 ] && echo "OK \c" || echo "no \c"
done
echo
} # End of Function1
Function2 () {
echo "Function2: for loop inside a function:\t\c"
for Var in $*
do
    [ 1 $Var 2 ] && echo "OK \c" || echo "no \c"
done ; echo
} # End of Function2
Function3 () {
echo "Function3: for loop inside a function:\t\c"
for Var in $@
do
    [ 1 $Var 2 ] && echo "OK \c" || echo "no \c"
done ; echo
} # End of Function3
Function4 () {
echo "Function4: for loop inside a function:\t\c"
for Var in "$@"
do
    [ 1 $Var 2 ] && echo "OK \c" || echo "no \c"
done ; echo
} # End of Function4
echo "Processing the following command line arguments: ${*:-none}"
echo "Script:    for loop outside a function:\t\c"
for Var
do
    [ 1 $Var 2 ] && echo "OK \c" || echo "no \c"
done ; echo
Function1 -eq -ne -gt -ge -le -lt
Function2 -eq -ne -gt -ge -le -lt
Function3 -eq -ne -gt -ge -le -lt
Function4 -eq -ne -gt -ge -le -lt '-ge 1 -a 2 -ge'
Function1 -eq -ne -gt -ge -le -lt '-ge 1 -a 2 -ge'
Function1
exit $?
