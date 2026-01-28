. scripts/parseargs.sh
T=/tmp/checklitmushist.sh.$$
trap 'rm -rf $T' 0
mkdir $T
if test -d litmus
then
	:
else
	echo Run scripts/initlitmushist.sh first, need litmus repo.
	exit 1
fi
mkdir $T/results
find litmus -type d -print | ( cd $T/results; sed -e 's/^/mkdir -p /' | sh )
( cd $LKMM_DESTDIR; find litmus -name '*.litmus.out' -print ) |
	sed -e 's/\.out$//' |
	xargs -r grep -L "^P${LKMM_PROCS}"> $T/list-C-already
xargs < $T/list-C-already -r grep -L "^P${LKMM_PROCS}" > $T/list-C-short
destdir="$LKMM_DESTDIR"
LKMM_DESTDIR=$T/results; export LKMM_DESTDIR
scripts/runlitmushist.sh < $T/list-C-short > $T/runlitmushist.sh.out 2>&1
LKMM_DESTDIR="$destdir"; export LKMM_DESTDIR
cdir=`pwd`
ddir=`awk -v c="$cdir" -v d="$LKMM_DESTDIR" \
	'END { if (d ~ /^\//) print d; else print c "/" d; }' < /dev/null`
( cd $T/results; find litmus -type f -name '*.litmus.out' -print |
  sed -e 's,^.*$,cp & '"$ddir"'/&.new,' | sh )
sed < $T/list-C-short -e 's,^,'"$LKMM_DESTDIR/"',' |
	sh scripts/cmplitmushist.sh
exit $?
