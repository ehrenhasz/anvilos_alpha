. scripts/parseargs.sh
T=/tmp/newlitmushist.sh.$$
trap 'rm -rf $T' 0
mkdir $T
if test -d litmus
then
	:
else
	echo Run scripts/initlitmushist.sh first, need litmus repo.
	exit 1
fi
if test "$LKMM_DESTDIR" != "."
then
	find litmus -type d -print |
	( cd "$LKMM_DESTDIR"; sed -e 's/^/mkdir -p /' | sh )
fi
( cd $LKMM_DESTDIR; find litmus -name '*.litmus.out' -print ) |
	sed -e 's/\.out$//' |
	xargs -r grep -L "^P${LKMM_PROCS}"> $T/list-C-already
find litmus -name '*.litmus' -print | mselect7 -arch C > $T/list-C-all
xargs < $T/list-C-all -r grep -L "^P${LKMM_PROCS}" > $T/list-C-short
sort $T/list-C-already $T/list-C-short | uniq -u > $T/list-C-new
sed < $T/list-C-short -e 's,^.*$,if test & -nt '"$LKMM_DESTDIR"'/&.out; then echo &; fi,' > $T/list-C-script
sh $T/list-C-script > $T/list-C-newer
sort -u $T/list-C-new $T/list-C-newer > $T/list-C-needed
scripts/runlitmushist.sh < $T/list-C-needed
exit 0
