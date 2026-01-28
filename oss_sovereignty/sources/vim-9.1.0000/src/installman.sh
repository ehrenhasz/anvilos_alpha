errstatus=0
what=$1
destdir=$2
langadd=$3
vimloc=$4
scriptloc=$5
vimrcloc=$6
helpsource=$7
manmod=$8
exename=$9
shift
vimdiffname=$9
shift
evimname=$9
helpsubloc=$scriptloc/doc
printsubloc=$scriptloc/print
synsubloc=$scriptloc/syntax
tutorsubloc=$scriptloc/tutor
if test $what = "install" -o $what = "xxd"; then
   if test ! -d $destdir; then
      echo creating $destdir
      /bin/sh install-sh -c -d $destdir
      chmod 755 $destdir
   fi
fi
if test $what = "install"; then
   if test -r $helpsource/vim$langadd.1; then
      echo installing $destdir/$exename.1
      LC_ALL=C sed -e s+/usr/local/lib/vim+$vimloc+ \
	      -e s+$vimloc/doc+$helpsubloc+ \
	      -e s+$vimloc/print+$printsubloc+ \
	      -e s+$vimloc/syntax+$synsubloc+ \
	      -e s+$vimloc/tutor+$tutorsubloc+ \
	      -e s+$vimloc/vimrc+$vimrcloc/vimrc+ \
	      -e s+$vimloc/gvimrc+$vimrcloc/gvimrc+ \
	      -e s+$vimloc/menu.vim+$scriptloc/menu.vim+ \
	      -e s+$vimloc/bugreport.vim+$scriptloc/bugreport.vim+ \
	      -e s+$vimloc/filetype.vim+$scriptloc/filetype.vim+ \
	      -e s+$vimloc/scripts.vim+$scriptloc/scripts.vim+ \
	      -e s+$vimloc/optwin.vim+$scriptloc/optwin.vim+ \
	      -e 's+$vimloc/\*.ps+$scriptloc/\*.ps+' \
	      $helpsource/vim$langadd.1 > $destdir/$exename.1
      chmod $manmod $destdir/$exename.1
   fi
   if test -r $helpsource/vimtutor$langadd.1; then
      echo installing $destdir/$exename""tutor.1
      LC_ALL=C sed -e s+/usr/local/lib/vim+$vimloc+ \
	      -e s+$vimloc/tutor+$tutorsubloc+ \
	      $helpsource/vimtutor$langadd.1 > $destdir/$exename""tutor.1
      chmod $manmod $destdir/$exename""tutor.1
   fi
   if test -r $helpsource/vimdiff$langadd.1; then
      echo installing $destdir/$vimdiffname.1
      cp $helpsource/vimdiff$langadd.1 $destdir/$vimdiffname.1
      chmod $manmod $destdir/$vimdiffname.1
   fi
   if test -r $helpsource/evim$langadd.1; then
      echo installing $destdir/$evimname.1
      LC_ALL=C sed -e s+/usr/local/lib/vim+$vimloc+ \
	      -e s+$vimloc/evim.vim+$scriptloc/evim.vim+ \
	      $helpsource/evim$langadd.1 > $destdir/$evimname.1
      chmod $manmod $destdir/$evimname.1
   fi
fi
if test $what = "uninstall"; then
   echo Checking for Vim manual pages in $destdir...
   if test -r $destdir/$exename.1; then
      echo deleting $destdir/$exename.1
      rm -f $destdir/$exename.1
   fi
   if test -r $destdir/$exename""tutor.1; then
      echo deleting $destdir/$exename""tutor.1
      rm -f $destdir/$exename""tutor.1
   fi
   if test -r $destdir/$vimdiffname.1; then
      echo deleting $destdir/$vimdiffname.1
      rm -f $destdir/$vimdiffname.1
   fi
   if test -r $destdir/$evimname.1; then
      echo deleting $destdir/$evimname.1
      rm -f $destdir/$evimname.1
   fi
fi
if test $what = "xxd" -a -r "$helpsource/xxd${langadd}.1"; then
   echo installing $destdir/xxd.1
   cp $helpsource/xxd$langadd.1 $destdir/xxd.1
   chmod $manmod $destdir/xxd.1
fi
exit $errstatus
