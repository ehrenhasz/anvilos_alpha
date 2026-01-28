RM="rm -f"
MP_PY=micropython
numtests=0
numtestcases=0
numpassed=0
numskipped=0
numfailed=0
nameskipped=
namefailed=
if [ $
then
    tests="basics/*.py micropython/*.py float/*.py import/*.py io/*.py misc/*.py unicode/*.py extmod/*.py unix/*.py"
else
    tests="$@"
fi
for infile in $tests
do
    basename=`basename $infile .py`
    outfile=${basename}.py.out
    expfile=$infile.exp
    $MP_PY $infile > $outfile
    numtestcases=$(expr $numtestcases + $(cat $expfile | wc -l))
    if grep -q "SKIP\|SyntaxError: invalid micropython decorator" $outfile
    then
        echo "skip  $infile"
        $RM $outfile
        numskipped=$(expr $numskipped + 1)
        nameskipped="$nameskipped $basename"
    else
        diff --brief $expfile $outfile > /dev/null
        if [ $? -eq 0 ]
        then
            echo "pass  $infile"
            $RM $outfile
            numpassed=$(expr $numpassed + 1)
        else
            echo "FAIL  $infile"
            numfailed=$(expr $numfailed + 1)
            namefailed="$namefailed $basename"
        fi
    fi
    numtests=$(expr $numtests + 1)
done
echo "$numtests tests performed ($numtestcases individual testcases)"
echo "$numpassed tests passed"
if [ $numskipped != 0 ]
then
    echo "$numskipped tests skipped -$nameskipped"
fi
if [ $numfailed != 0 ]
then
    echo "$numfailed tests failed -$namefailed"
    exit 1
else
    exit 0
fi
