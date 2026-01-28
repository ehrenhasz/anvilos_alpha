. scripts/parseargs.sh
T=/tmp/initlitmushist.sh.$$
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
find litmus -name '*.litmus' -print | mselect7 -arch C > $T/list-C
xargs < $T/list-C -r grep -L "^P${LKMM_PROCS}" > $T/list-C-short
scripts/runlitmushist.sh < $T/list-C-short
exit 0
