sphinx="$1"
shift || true
parallel="$PARALLELISM"
if [ -z "$parallel" ] ; then
	auto=$(perl -e 'open IN,"'"$sphinx"' --version 2>&1 |";
			while (<IN>) {
				if (m/([\d\.]+)/) {
					print "auto" if ($1 >= "1.7")
				}
			}
			close IN')
	if [ -n "$auto" ] ; then
		parallel="$auto"
	fi
fi
if [ -n "$parallel" ] ; then
	parallel="-j$parallel"
fi
exec "$sphinx" $parallel "$@"
