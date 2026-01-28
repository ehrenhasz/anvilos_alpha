if test -t 1; then
  if test $
    if test -t 0; then
      echo "Missing filename" 1>&2
      exit
    fi
    vim --cmd 'let no_plugin_maps = 1' -c 'runtime! macros/less.vim' -
  else
    vim --cmd 'let no_plugin_maps = 1' -c 'runtime! macros/less.vim' "$@"
  fi
else
  if test $
    if test -t 0; then
      echo "Missing filename" 1>&2
      exit
    fi
    cat
  else
    cat "$@"
  fi
fi
