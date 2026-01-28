if test -z "$CC"; then
  CC=cc
fi
if test -z "$srcdir"; then
  srcdir=.
fi
export LC_COLLATE=C
export LC_ALL=
rm -f core* *.core
cat << EOF > osdef0.c
EOF
$CC -I. -I$srcdir -E osdef0.c >osdef0.cc
sed < osdef0.cc -e '/\(..*\)/s// \1/' > osdef0.ccc
sed < $srcdir/osdef1.h.in -n -e '/^extern/s@.*[)* 	][)* 	]*\([a-zA-Z_][a-zA-Z0-9_]*\)(.*@/[)*, 	][(]*\1[)]*[ 	(]/i\\\
\\/\\[^a-zA-Z_\\]\1(\\/d@p' > osdef11.sed
sed < $srcdir/osdef2.h.in -n -e '/^extern/s@.*[)* 	][)* 	]*\([a-zA-Z_][a-zA-Z0-9_]*\)(.*@/[)*, 	][(]*\1[)]*[ 	(]/i\\\
\\/\\[^a-zA-Z_\\]\1(\\/d@p' > osdef21.sed
cat << EOF > osdef2.sed
1i\\
/*
1i\\
 * osdef.h is automagically created from osdef?.h.in by osdef.sh -- DO NOT EDIT
1i\\
 */
EOF
cat osdef0.ccc | sed -n -f osdef11.sed >> osdef2.sed
sed -f osdef2.sed < $srcdir/osdef1.h.in > auto/osdef.h
cat osdef0.ccc | sed -n -f osdef21.sed > osdef2.sed
sed -f osdef2.sed < $srcdir/osdef2.h.in >> auto/osdef.h
rm osdef0.c osdef0.cc osdef0.ccc osdef11.sed osdef21.sed osdef2.sed
if test -f core*; then
  file core*
  echo "  Sorry, your sed is broken. Call the system administrator."
  echo "  Meanwhile, you may try to compile Vim with an empty osdef.h file."
  echo "  If you compiler complains about missing prototypes, move the needed"
  echo "  ones from osdef1.h.in and osdef2.h.in to osdef.h."
  exit 1
fi
cat $srcdir/osdef1.h.in $srcdir/osdef2.h.in >osdefX.h.in
if eval test "`diff auto/osdef.h osdefX.h.in | wc -l`" -eq 4; then
  echo "  Hmm, sed is very pessimistic about your system header files."
  echo "  But it did not dump core -- strange! Let's continue carefully..."
  echo "  If this fails, you may want to remove offending lines from osdef.h"
  echo "  or try with an empty osdef.h file, if your compiler can do without"
  echo "  function declarations."
fi
rm osdefX.h.in
