. scripts/parseargs.sh
. scripts/hwfnseg.sh
T=/tmp/checkghlitmus.sh.$$
trap 'rm -rf $T' 0
mkdir $T
if test -d litmus
then
	:
else
	git clone https://github.com/paulmckrcu/litmus
	( cd litmus; git checkout origin/master )
fi
if test "$LKMM_DESTDIR" != "."
then
	find litmus -type d -print |
	( cd "$LKMM_DESTDIR"; sed -e 's/^/mkdir -p /' | sh )
fi
( cd $LKMM_DESTDIR; find litmus -name "*.litmus${hwfnseg}.out" -print ) |
	sed -e "s/${hwfnseg}"'\.out$//' |
	xargs -r grep -E -l '^ \* Result: (Never|Sometimes|Always|DEADLOCK)' |
	xargs -r grep -L "^P${LKMM_PROCS}"> $T/list-C-already
find litmus -name '*.litmus' -print | mselect7 -arch C > $T/list-C
xargs < $T/list-C -r grep -E -l '^ \* Result: (Never|Sometimes|Always|DEADLOCK)' > $T/list-C-result
xargs < $T/list-C-result -r grep -L "^P${LKMM_PROCS}" > $T/list-C-result-short
sort $T/list-C-already $T/list-C-result-short | uniq -u > $T/list-C-needed
if scripts/runlitmushist.sh < $T/list-C-needed > $T/run.stdout 2> $T/run.stderr
then
	errs=
else
	errs=1
fi
sed < $T/list-C-result-short -e 's,^,scripts/judgelitmus.sh ,' |
	sh > $T/judge.stdout 2> $T/judge.stderr
if test -n "$errs"
then
	cat $T/run.stderr 1>&2
fi
grep '!!!' $T/judge.stdout
