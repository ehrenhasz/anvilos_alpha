echo "$LINK " >link_$PROG.cmd
exit_value=0
if test "$LINK_AS_NEEDED" = yes; then
  echo "link.sh: \$LINK_AS_NEEDED set to 'yes': invoking linker directly."
  cat link_$PROG.cmd
  if sh link_$PROG.cmd; then
    exit_value=0
    echo "link.sh: Linked fine"
  else
    exit_value=$?
    echo "link.sh: Linking failed"
  fi
else
  if test -f auto/link.sed; then
  echo "link.sh: The file 'auto/link.sed' exists, which is going to be used now."
  echo "link.sh: If linking fails, try deleting the auto/link.sed file."
  echo "link.sh: If this fails too, try creating an empty auto/link.sed file."
else
  cat link_$PROG.cmd
  if sh link_$PROG.cmd; then
    touch auto/link.sed
    cp link_$PROG.cmd linkit_$PROG.sh
    for libname in SM ICE nsl dnet dnet_stub inet socket dir elf iconv Xt Xmu Xp Xpm X11 Xdmcp x w perl dl pthread thread readline m crypt attr; do
      cont=yes
      while test -n "$cont"; do
        if grep "l$libname " linkit_$PROG.sh >/dev/null; then
          if test ! -f link1_$PROG.sed; then
            echo "link.sh: OK, linking works, let's try omitting a few libraries."
            echo "link.sh: See auto/link.log for details."
            rm -f auto/link.log
          fi
          echo "s/-l$libname  *//" >link1_$PROG.sed
          sed -f auto/link.sed <link_$PROG.cmd >linkit2_$PROG.sh
          sed -f link1_$PROG.sed <linkit2_$PROG.sh >linkit_$PROG.sh
          if test $libname != "m" || grep "lm " linkit_$PROG.sh >/dev/null; then
            echo "link.sh: Trying to omit the $libname library..."
            cat linkit_$PROG.sh >>auto/link.log
            if sh linkit_$PROG.sh >>auto/link.log 2>&1; then
              echo "link.sh: Vim doesn't need the $libname library!"
              cat link1_$PROG.sed >>auto/link.sed
              rm -f auto/pathdef.c
            else
              echo "link.sh: Vim DOES need the $libname library."
              cont=
              cp link_$PROG.cmd linkit_$PROG.sh
            fi
          else
            cont=
            cp link_$PROG.cmd linkit_$PROG.sh
          fi
        else
          cont=
          cp link_$PROG.cmd linkit_$PROG.sh
        fi
      done
    done
    if test ! -f auto/pathdef.c; then
      $MAKE objects/pathdef.o
    fi
    if test ! -f link1_$PROG.sed; then
      echo "link.sh: Linked fine, no libraries can be omitted"
      touch link3_$PROG.sed
    fi
  else
    exit_value=$?
  fi
fi
if test -s auto/link.sed; then
  echo "link.sh: Using auto/link.sed file to omit a few libraries"
  sed -f auto/link.sed <link_$PROG.cmd >linkit_$PROG.sh
  cat linkit_$PROG.sh
  if sh linkit_$PROG.sh; then
    exit_value=0
    echo "link.sh: Linked fine with a few libraries omitted"
  else
    exit_value=$?
    echo "link.sh: Linking failed, making auto/link.sed empty and trying again"
    mv -f auto/link.sed link2_$PROG.sed
    touch auto/link.sed
    rm -f auto/pathdef.c
    $MAKE objects/pathdef.o
  fi
fi
if test -f auto/link.sed -a ! -s auto/link.sed -a ! -f link3_$PROG.sed; then
  echo "link.sh: Using unmodified link command"
  cat link_$PROG.cmd
  if sh link_$PROG.cmd; then
    exit_value=0
    echo "link.sh: Linked OK"
  else
    exit_value=$?
    if test -f link2_$PROG.sed; then
      echo "link.sh: Linking doesn't work at all, removing auto/link.sed"
      rm -f auto/link.sed
    fi
  fi
fi
fi
rm -f link_$PROG.cmd linkit_$PROG.sh link1_$PROG.sed link2_$PROG.sed \
  link3_$PROG.sed linkit2_$PROG.sh
exit $exit_value
