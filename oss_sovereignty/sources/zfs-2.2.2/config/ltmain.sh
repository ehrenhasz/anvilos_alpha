PROGRAM=libtool
PACKAGE=libtool
VERSION=2.4.7
package_revision=2.4.7
: ${AUTOCONF="autoconf"}
: ${AUTOMAKE="automake"}
scriptversion=2019-02-19.15; # UTC
DUALCASE=1; export DUALCASE # for MKS sh
if test -n "${ZSH_VERSION+set}" && (emulate sh) >/dev/null 2>&1; then :
  emulate sh
  NULLCMD=:
  alias -g '${1+"$@"}'='"$@"'
  setopt NO_GLOB_SUBST
else
  case `(set -o) 2>/dev/null` in *posix*) set -o posix ;; esac
fi
_G_user_locale=
_G_safe_locale=
for _G_var in LANG LANGUAGE LC_ALL LC_CTYPE LC_COLLATE LC_MESSAGES
do
  eval "if test set = \"\${$_G_var+set}\"; then
          save_$_G_var=\$$_G_var
          $_G_var=C
	  export $_G_var
	  _G_user_locale=\"$_G_var=\\\$save_\$_G_var; \$_G_user_locale\"
	  _G_safe_locale=\"$_G_var=C; \$_G_safe_locale\"
	fi"
done
LC_ALL=C
LANGUAGE=C
export LANGUAGE LC_ALL
sp=' '
nl='
'
IFS="$sp	$nl"
if test "${PATH_SEPARATOR+set}" != set; then
  PATH_SEPARATOR=:
  (PATH='/bin;/bin'; FPATH=$PATH; sh -c :) >/dev/null 2>&1 && {
    (PATH='/bin:/bin'; FPATH=$PATH; sh -c :) >/dev/null 2>&1 ||
      PATH_SEPARATOR=';'
  }
fi
func_unset ()
{
    { eval $1=; (eval unset $1) >/dev/null 2>&1 && eval unset $1 || : ; }
}
func_unset CDPATH
func_unset GREP_OPTIONS
func_executable_p ()
{
    test -f "$1" && test -x "$1"
}
func_path_progs ()
{
    _G_progs_list=$1
    _G_check_func=$2
    _G_PATH=${3-"$PATH"}
    _G_path_prog_max=0
    _G_path_prog_found=false
    _G_save_IFS=$IFS; IFS=${PATH_SEPARATOR-:}
    for _G_dir in $_G_PATH; do
      IFS=$_G_save_IFS
      test -z "$_G_dir" && _G_dir=.
      for _G_prog_name in $_G_progs_list; do
        for _exeext in '' .EXE; do
          _G_path_prog=$_G_dir/$_G_prog_name$_exeext
          func_executable_p "$_G_path_prog" || continue
          case `"$_G_path_prog" --version 2>&1` in
            *GNU*) func_path_progs_result=$_G_path_prog _G_path_prog_found=: ;;
            *)     $_G_check_func $_G_path_prog
		   func_path_progs_result=$func_check_prog_result
		   ;;
          esac
          $_G_path_prog_found && break 3
        done
      done
    done
    IFS=$_G_save_IFS
    test -z "$func_path_progs_result" && {
      echo "no acceptable sed could be found in \$PATH" >&2
      exit 1
    }
}
test -z "$SED" && {
  _G_sed_script=s/aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa/bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb/
  for _G_i in 1 2 3 4 5 6 7; do
    _G_sed_script=$_G_sed_script$nl$_G_sed_script
  done
  echo "$_G_sed_script" 2>/dev/null | sed 99q >conftest.sed
  _G_sed_script=
  func_check_prog_sed ()
  {
    _G_path_prog=$1
    _G_count=0
    printf 0123456789 >conftest.in
    while :
    do
      cat conftest.in conftest.in >conftest.tmp
      mv conftest.tmp conftest.in
      cp conftest.in conftest.nl
      echo '' >> conftest.nl
      "$_G_path_prog" -f conftest.sed <conftest.nl >conftest.out 2>/dev/null || break
      diff conftest.out conftest.nl >/dev/null 2>&1 || break
      _G_count=`expr $_G_count + 1`
      if test "$_G_count" -gt "$_G_path_prog_max"; then
        func_check_prog_result=$_G_path_prog
        _G_path_prog_max=$_G_count
      fi
      test 10 -lt "$_G_count" && break
    done
    rm -f conftest.in conftest.tmp conftest.nl conftest.out
  }
  func_path_progs "sed gsed" func_check_prog_sed "$PATH:/usr/xpg4/bin"
  rm -f conftest.sed
  SED=$func_path_progs_result
}
test -z "$GREP" && {
  func_check_prog_grep ()
  {
    _G_path_prog=$1
    _G_count=0
    _G_path_prog_max=0
    printf 0123456789 >conftest.in
    while :
    do
      cat conftest.in conftest.in >conftest.tmp
      mv conftest.tmp conftest.in
      cp conftest.in conftest.nl
      echo 'GREP' >> conftest.nl
      "$_G_path_prog" -e 'GREP$' -e '-(cannot match)-' <conftest.nl >conftest.out 2>/dev/null || break
      diff conftest.out conftest.nl >/dev/null 2>&1 || break
      _G_count=`expr $_G_count + 1`
      if test "$_G_count" -gt "$_G_path_prog_max"; then
        func_check_prog_result=$_G_path_prog
        _G_path_prog_max=$_G_count
      fi
      test 10 -lt "$_G_count" && break
    done
    rm -f conftest.in conftest.tmp conftest.nl conftest.out
  }
  func_path_progs "grep ggrep" func_check_prog_grep "$PATH:/usr/xpg4/bin"
  GREP=$func_path_progs_result
}
: ${CP="cp -f"}
: ${ECHO="printf %s\n"}
: ${EGREP="$GREP -E"}
: ${FGREP="$GREP -F"}
: ${LN_S="ln -s"}
: ${MAKE="make"}
: ${MKDIR="mkdir"}
: ${MV="mv -f"}
: ${RM="rm -f"}
: ${SHELL="${CONFIG_SHELL-/bin/sh}"}
sed_dirname='s|/[^/]*$||'
sed_basename='s|^.*/||'
sed_quote_subst='s|\([`"$\\]\)|\\\1|g'
sed_double_quote_subst='s/\(["`\\]\)/\\\1/g'
sed_make_literal_regex='s|[].[^$\\*\/]|\\&|g'
sed_naive_backslashify='s|\\\\*|\\|g;s|/|\\|g;s|\\|\\\\|g'
_G_bs='\\'
_G_bs2='\\\\'
_G_bs4='\\\\\\\\'
_G_dollar='\$'
sed_double_backslash="\
  s/$_G_bs4/&\\
/g
  s/^$_G_bs2$_G_dollar/$_G_bs&/
  s/\\([^$_G_bs]\\)$_G_bs2$_G_dollar/\\1$_G_bs2$_G_bs$_G_dollar/g
  s/\n//g"
require_check_ifs_backslash=func_require_check_ifs_backslash
func_require_check_ifs_backslash ()
{
  _G_save_IFS=$IFS
  IFS='\'
  _G_check_ifs_backshlash='a\\b'
  for _G_i in $_G_check_ifs_backshlash
  do
  case $_G_i in
  a)
    check_ifs_backshlash_broken=false
    ;;
  '')
    break
    ;;
  *)
    check_ifs_backshlash_broken=:
    break
    ;;
  esac
  done
  IFS=$_G_save_IFS
  require_check_ifs_backslash=:
}
EXIT_SUCCESS=0
EXIT_FAILURE=1
EXIT_MISMATCH=63  # $? = 63 is used to indicate version mismatch to missing.
EXIT_SKIP=77	  # $? = 77 is used to indicate a skipped test to automake.
debug_cmd=${debug_cmd-":"}
exit_cmd=:
exit_status=$EXIT_SUCCESS
progpath=$0
progname=`$ECHO "$progpath" |$SED "$sed_basename"`
case $progpath in
  [\\/]*|[A-Za-z]:\\*) ;;
  *[\\/]*)
     progdir=`$ECHO "$progpath" |$SED "$sed_dirname"`
     progdir=`cd "$progdir" && pwd`
     progpath=$progdir/$progname
     ;;
  *)
     _G_IFS=$IFS
     IFS=${PATH_SEPARATOR-:}
     for progdir in $PATH; do
       IFS=$_G_IFS
       test -x "$progdir/$progname" && break
     done
     IFS=$_G_IFS
     test -n "$progdir" || progdir=`pwd`
     progpath=$progdir/$progname
     ;;
esac
opt_dry_run=false
opt_quiet=false
opt_verbose=false
warning_categories=
warning_func=func_warn_and_continue
opt_warning_types=all
require_term_colors=func_require_term_colors
func_require_term_colors ()
{
    $debug_cmd
    test -t 1 && {
      test -n "${COLORTERM+set}" && : ${USE_ANSI_COLORS="1"}
      if test 1 = "$USE_ANSI_COLORS"; then
        tc_reset='[0m'
        tc_bold='[1m';   tc_standout='[7m'
        tc_red='[31m';   tc_green='[32m'
        tc_blue='[34m';  tc_cyan='[36m'
      else
        test -n "`tput sgr0 2>/dev/null`" && {
          tc_reset=`tput sgr0`
          test -n "`tput bold 2>/dev/null`" && tc_bold=`tput bold`
          tc_standout=$tc_bold
          test -n "`tput smso 2>/dev/null`" && tc_standout=`tput smso`
          test -n "`tput setaf 1 2>/dev/null`" && tc_red=`tput setaf 1`
          test -n "`tput setaf 2 2>/dev/null`" && tc_green=`tput setaf 2`
          test -n "`tput setaf 4 2>/dev/null`" && tc_blue=`tput setaf 4`
          test -n "`tput setaf 5 2>/dev/null`" && tc_cyan=`tput setaf 5`
        }
      fi
    }
    require_term_colors=:
}
  if test set = "${BASH_VERSION+set}${ZSH_VERSION+set}"; then
    : ${_G_HAVE_ARITH_OP="yes"}
    : ${_G_HAVE_XSI_OPS="yes"}
    case $BASH_VERSION in
      [12].* | 3.0 | 3.0*) ;;
      *)
        : ${_G_HAVE_PLUSEQ_OP="yes"}
        ;;
    esac
  fi
  test -z "$_G_HAVE_PLUSEQ_OP" \
    && (eval 'x=a; x+=" b"; test "a b" = "$x"') 2>/dev/null \
    && _G_HAVE_PLUSEQ_OP=yes
if test yes = "$_G_HAVE_PLUSEQ_OP"
then
  eval 'func_append ()
  {
    $debug_cmd
    eval "$1+=\$2"
  }'
else
  func_append ()
  {
    $debug_cmd
    eval "$1=\$$1\$2"
  }
fi
if test yes = "$_G_HAVE_PLUSEQ_OP"; then
  eval 'func_append_quoted ()
  {
    $debug_cmd
    func_quote_arg pretty "$2"
    eval "$1+=\\ \$func_quote_arg_result"
  }'
else
  func_append_quoted ()
  {
    $debug_cmd
    func_quote_arg pretty "$2"
    eval "$1=\$$1\\ \$func_quote_arg_result"
  }
fi
func_append_uniq ()
{
    $debug_cmd
    eval _G_current_value='`$ECHO $'$1'`'
    _G_delim=`expr "$2" : '\(.\)'`
    case $_G_delim$_G_current_value$_G_delim in
      *"$2$_G_delim"*) ;;
      *) func_append "$@" ;;
    esac
}
  test -z "$_G_HAVE_ARITH_OP" \
    && (eval 'test 2 = $(( 1 + 1 ))') 2>/dev/null \
    && _G_HAVE_ARITH_OP=yes
if test yes = "$_G_HAVE_ARITH_OP"; then
  eval 'func_arith ()
  {
    $debug_cmd
    func_arith_result=$(( $* ))
  }'
else
  func_arith ()
  {
    $debug_cmd
    func_arith_result=`expr "$@"`
  }
fi
if test yes = "$_G_HAVE_XSI_OPS"; then
  _b='func_basename_result=${1##*/}'
  _d='case $1 in
        */*) func_dirname_result=${1%/*}$2 ;;
        *  ) func_dirname_result=$3        ;;
      esac'
else
  _b='func_basename_result=`$ECHO "$1" |$SED "$sed_basename"`'
  _d='func_dirname_result=`$ECHO "$1"  |$SED "$sed_dirname"`
      if test "X$func_dirname_result" = "X$1"; then
        func_dirname_result=$3
      else
        func_append func_dirname_result "$2"
      fi'
fi
eval 'func_basename ()
{
    $debug_cmd
    '"$_b"'
}'
eval 'func_dirname ()
{
    $debug_cmd
    '"$_d"'
}'
eval 'func_dirname_and_basename ()
{
    $debug_cmd
    '"$_b"'
    '"$_d"'
}'
func_echo ()
{
    $debug_cmd
    _G_message=$*
    func_echo_IFS=$IFS
    IFS=$nl
    for _G_line in $_G_message; do
      IFS=$func_echo_IFS
      $ECHO "$progname: $_G_line"
    done
    IFS=$func_echo_IFS
}
func_echo_all ()
{
    $ECHO "$*"
}
func_echo_infix_1 ()
{
    $debug_cmd
    $require_term_colors
    _G_infix=$1; shift
    _G_indent=$_G_infix
    _G_prefix="$progname: $_G_infix: "
    _G_message=$*
    for _G_tc in "$tc_reset" "$tc_bold" "$tc_standout" "$tc_red" "$tc_green" "$tc_blue" "$tc_cyan"
    do
      test -n "$_G_tc" && {
        _G_esc_tc=`$ECHO "$_G_tc" | $SED "$sed_make_literal_regex"`
        _G_indent=`$ECHO "$_G_indent" | $SED "s|$_G_esc_tc||g"`
      }
    done
    _G_indent="$progname: "`echo "$_G_indent" | $SED 's|.| |g'`"  " ## exclude from sc_prohibit_nested_quotes
    func_echo_infix_1_IFS=$IFS
    IFS=$nl
    for _G_line in $_G_message; do
      IFS=$func_echo_infix_1_IFS
      $ECHO "$_G_prefix$tc_bold$_G_line$tc_reset" >&2
      _G_prefix=$_G_indent
    done
    IFS=$func_echo_infix_1_IFS
}
func_error ()
{
    $debug_cmd
    $require_term_colors
    func_echo_infix_1 "  $tc_standout${tc_red}error$tc_reset" "$*" >&2
}
func_fatal_error ()
{
    $debug_cmd
    func_error "$*"
    exit $EXIT_FAILURE
}
func_grep ()
{
    $debug_cmd
    $GREP "$1" "$2" >/dev/null 2>&1
}
  test -z "$_G_HAVE_XSI_OPS" \
    && (eval 'x=a/b/c;
      test 5aa/bb/cc = "${#x}${x%%/*}${x%/*}${x#*/}${x##*/}"') 2>/dev/null \
    && _G_HAVE_XSI_OPS=yes
if test yes = "$_G_HAVE_XSI_OPS"; then
  eval 'func_len ()
  {
    $debug_cmd
    func_len_result=${#1}
  }'
else
  func_len ()
  {
    $debug_cmd
    func_len_result=`expr "$1" : ".*" 2>/dev/null || echo $max_cmd_len`
  }
fi
func_mkdir_p ()
{
    $debug_cmd
    _G_directory_path=$1
    _G_dir_list=
    if test -n "$_G_directory_path" && test : != "$opt_dry_run"; then
      case $_G_directory_path in
        -*) _G_directory_path=./$_G_directory_path ;;
      esac
      while test ! -d "$_G_directory_path"; do
        _G_dir_list=$_G_directory_path:$_G_dir_list
        case $_G_directory_path in */*) ;; *) break ;; esac
        _G_directory_path=`$ECHO "$_G_directory_path" | $SED -e "$sed_dirname"`
      done
      _G_dir_list=`$ECHO "$_G_dir_list" | $SED 's|:*$||'`
      func_mkdir_p_IFS=$IFS; IFS=:
      for _G_dir in $_G_dir_list; do
	IFS=$func_mkdir_p_IFS
        $MKDIR "$_G_dir" 2>/dev/null || :
      done
      IFS=$func_mkdir_p_IFS
      test -d "$_G_directory_path" || \
        func_fatal_error "Failed to create '$1'"
    fi
}
func_mktempdir ()
{
    $debug_cmd
    _G_template=${TMPDIR-/tmp}/${1-$progname}
    if test : = "$opt_dry_run"; then
      _G_tmpdir=$_G_template-$$
    else
      _G_tmpdir=`mktemp -d "$_G_template-XXXXXXXX" 2>/dev/null`
      if test ! -d "$_G_tmpdir"; then
        _G_tmpdir=$_G_template-${RANDOM-0}$$
        func_mktempdir_umask=`umask`
        umask 0077
        $MKDIR "$_G_tmpdir"
        umask $func_mktempdir_umask
      fi
      test -d "$_G_tmpdir" || \
        func_fatal_error "cannot create temporary directory '$_G_tmpdir'"
    fi
    $ECHO "$_G_tmpdir"
}
func_normal_abspath ()
{
    $debug_cmd
    _G_pathcar='s|^/\([^/]*\).*$|\1|'
    _G_pathcdr='s|^/[^/]*||'
    _G_removedotparts=':dotsl
		s|/\./|/|g
		t dotsl
		s|/\.$|/|'
    _G_collapseslashes='s|/\{1,\}|/|g'
    _G_finalslash='s|/*$|/|'
    func_normal_abspath_result=
    func_normal_abspath_tpath=$1
    func_normal_abspath_altnamespace=
    case $func_normal_abspath_tpath in
      "")
        func_stripname '' '/' "`pwd`"
        func_normal_abspath_result=$func_stripname_result
        return
        ;;
      ///*)
        ;;
      //*)
        func_normal_abspath_altnamespace=/
        ;;
      /*)
        ;;
      *)
        func_normal_abspath_tpath=`pwd`/$func_normal_abspath_tpath
        ;;
    esac
    func_normal_abspath_tpath=`$ECHO "$func_normal_abspath_tpath" | $SED \
          -e "$_G_removedotparts" -e "$_G_collapseslashes" -e "$_G_finalslash"`
    while :; do
      if test / = "$func_normal_abspath_tpath"; then
        if test -z "$func_normal_abspath_result"; then
          func_normal_abspath_result=/
        fi
        break
      fi
      func_normal_abspath_tcomponent=`$ECHO "$func_normal_abspath_tpath" | $SED \
          -e "$_G_pathcar"`
      func_normal_abspath_tpath=`$ECHO "$func_normal_abspath_tpath" | $SED \
          -e "$_G_pathcdr"`
      case $func_normal_abspath_tcomponent in
        "")
          ;;
        ..)
          func_dirname "$func_normal_abspath_result"
          func_normal_abspath_result=$func_dirname_result
          ;;
        *)
          func_append func_normal_abspath_result "/$func_normal_abspath_tcomponent"
          ;;
      esac
    done
    func_normal_abspath_result=$func_normal_abspath_altnamespace$func_normal_abspath_result
}
func_notquiet ()
{
    $debug_cmd
    $opt_quiet || func_echo ${1+"$@"}
    :
}
func_relative_path ()
{
    $debug_cmd
    func_relative_path_result=
    func_normal_abspath "$1"
    func_relative_path_tlibdir=$func_normal_abspath_result
    func_normal_abspath "$2"
    func_relative_path_tbindir=$func_normal_abspath_result
    while :; do
      case $func_relative_path_tbindir in
        $func_relative_path_tlibdir)
          func_relative_path_tcancelled=
          break
          ;;
        $func_relative_path_tlibdir*)
          func_stripname "$func_relative_path_tlibdir" '' "$func_relative_path_tbindir"
          func_relative_path_tcancelled=$func_stripname_result
          if test -z "$func_relative_path_result"; then
            func_relative_path_result=.
          fi
          break
          ;;
        *)
          func_dirname $func_relative_path_tlibdir
          func_relative_path_tlibdir=$func_dirname_result
          if test -z "$func_relative_path_tlibdir"; then
            func_relative_path_result=../$func_relative_path_result
            func_relative_path_tcancelled=$func_relative_path_tbindir
            break
          fi
          func_relative_path_result=../$func_relative_path_result
          ;;
      esac
    done
    func_stripname '' '/' "$func_relative_path_result"
    func_relative_path_result=$func_stripname_result
    func_stripname '/' '/' "$func_relative_path_tcancelled"
    if test -n "$func_stripname_result"; then
      func_append func_relative_path_result "/$func_stripname_result"
    fi
    if test -n "$func_relative_path_result"; then
      func_stripname './' '' "$func_relative_path_result"
      func_relative_path_result=$func_stripname_result
    fi
    test -n "$func_relative_path_result" || func_relative_path_result=.
    :
}
func_quote_portable ()
{
    $debug_cmd
    $require_check_ifs_backslash
    func_quote_portable_result=$2
    while true
    do
      if $1; then
        func_quote_portable_result=`$ECHO "$2" | $SED \
          -e "$sed_double_quote_subst" -e "$sed_double_backslash"`
        break
      fi
      case $func_quote_portable_result in
        *[\\\`\"\$]*)
          case $check_ifs_backshlash_broken$func_quote_portable_result in
            :*|*[\[\*\?]*)
              func_quote_portable_result=`$ECHO "$func_quote_portable_result" \
                  | $SED "$sed_quote_subst"`
              break
              ;;
          esac
          func_quote_portable_old_IFS=$IFS
          for _G_char in '\' '`' '"' '$'
          do
            set start "" ""
            func_quote_portable_result=dummy"$_G_char$func_quote_portable_result$_G_char"dummy
            IFS=$_G_char
            for _G_part in $func_quote_portable_result
            do
              case $1 in
              quote)
                func_append func_quote_portable_result "$3$2"
                set quote "$_G_part" "\\$_G_char"
                ;;
              start)
                set first "" ""
                func_quote_portable_result=
                ;;
              first)
                set quote "$_G_part" ""
                ;;
              esac
            done
          done
          IFS=$func_quote_portable_old_IFS
          ;;
        *) ;;
      esac
      break
    done
    func_quote_portable_unquoted_result=$func_quote_portable_result
    case $func_quote_portable_result in
      *[\[\~\#\^\&\*\(\)\{\}\|\;\<\>\?\'\ \	]*|*]*|"")
        func_quote_portable_result=\"$func_quote_portable_result\"
        ;;
    esac
}
if test xyes = `(x=; printf -v x %q yes; echo x"$x") 2>/dev/null`; then
  printf -v _GL_test_printf_tilde %q '~'
  if test '\~' = "$_GL_test_printf_tilde"; then
    func_quotefast_eval ()
    {
      printf -v func_quotefast_eval_result %q "$1"
    }
  else
    func_quotefast_eval ()
    {
      case $1 in
        '~'*)
          func_quote_portable false "$1"
          func_quotefast_eval_result=$func_quote_portable_result
          ;;
        *)
          printf -v func_quotefast_eval_result %q "$1"
          ;;
      esac
    }
  fi
else
  func_quotefast_eval ()
  {
    func_quote_portable false "$1"
    func_quotefast_eval_result=$func_quote_portable_result
  }
fi
func_quote_arg ()
{
    _G_quote_expand=false
    case ,$1, in
      *,expand,*)
        _G_quote_expand=:
        ;;
    esac
    case ,$1, in
      *,pretty,*|*,expand,*|*,unquoted,*)
        func_quote_portable $_G_quote_expand "$2"
        func_quote_arg_result=$func_quote_portable_result
        func_quote_arg_unquoted_result=$func_quote_portable_unquoted_result
        ;;
      *)
        func_quotefast_eval "$2"
        func_quote_arg_result=$func_quotefast_eval_result
        ;;
    esac
}
func_quote ()
{
    $debug_cmd
    _G_func_quote_mode=$1 ; shift
    func_quote_result=
    while test 0 -lt $#; do
      func_quote_arg "$_G_func_quote_mode" "$1"
      if test -n "$func_quote_result"; then
        func_append func_quote_result " $func_quote_arg_result"
      else
        func_append func_quote_result "$func_quote_arg_result"
      fi
      shift
    done
}
if test yes = "$_G_HAVE_XSI_OPS"; then
  eval 'func_stripname ()
  {
    $debug_cmd
    func_stripname_result=$3
    func_stripname_result=${func_stripname_result#"$1"}
    func_stripname_result=${func_stripname_result%"$2"}
  }'
else
  func_stripname ()
  {
    $debug_cmd
    case $2 in
      .*) func_stripname_result=`$ECHO "$3" | $SED -e "s%^$1%%" -e "s%\\\\$2\$%%"`;;
      *)  func_stripname_result=`$ECHO "$3" | $SED -e "s%^$1%%" -e "s%$2\$%%"`;;
    esac
  }
fi
func_show_eval ()
{
    $debug_cmd
    _G_cmd=$1
    _G_fail_exp=${2-':'}
    func_quote_arg pretty,expand "$_G_cmd"
    eval "func_notquiet $func_quote_arg_result"
    $opt_dry_run || {
      eval "$_G_cmd"
      _G_status=$?
      if test 0 -ne "$_G_status"; then
	eval "(exit $_G_status); $_G_fail_exp"
      fi
    }
}
func_show_eval_locale ()
{
    $debug_cmd
    _G_cmd=$1
    _G_fail_exp=${2-':'}
    $opt_quiet || {
      func_quote_arg expand,pretty "$_G_cmd"
      eval "func_echo $func_quote_arg_result"
    }
    $opt_dry_run || {
      eval "$_G_user_locale
	    $_G_cmd"
      _G_status=$?
      eval "$_G_safe_locale"
      if test 0 -ne "$_G_status"; then
	eval "(exit $_G_status); $_G_fail_exp"
      fi
    }
}
func_tr_sh ()
{
    $debug_cmd
    case $1 in
    [0-9]* | *[!a-zA-Z0-9_]*)
      func_tr_sh_result=`$ECHO "$1" | $SED -e 's/^\([0-9]\)/_\1/' -e 's/[^a-zA-Z0-9_]/_/g'`
      ;;
    * )
      func_tr_sh_result=$1
      ;;
    esac
}
func_verbose ()
{
    $debug_cmd
    $opt_verbose && func_echo "$*"
    :
}
func_warn_and_continue ()
{
    $debug_cmd
    $require_term_colors
    func_echo_infix_1 "${tc_red}warning$tc_reset" "$*" >&2
}
func_warning ()
{
    $debug_cmd
    case " $warning_categories " in
      *" $1 "*) ;;
      *) func_internal_error "invalid warning category '$1'" ;;
    esac
    _G_category=$1
    shift
    case " $opt_warning_types " in
      *" $_G_category "*) $warning_func ${1+"$@"} ;;
    esac
}
func_sort_ver ()
{
    $debug_cmd
    printf '%s\n%s\n' "$1" "$2" \
      | sort -t. -k 1,1n -k 2,2n -k 3,3n -k 4,4n -k 5,5n -k 6,6n -k 7,7n -k 8,8n -k 9,9n
}
func_lt_ver ()
{
    $debug_cmd
    test "x$1" = x`func_sort_ver "$1" "$2" | $SED 1q`
}
scriptversion=2019-02-19.15; # UTC
usage='$progpath [OPTION]...'
usage_message="\
       --debug        enable verbose shell tracing
   -W, --warnings=CATEGORY
                      report the warnings falling in CATEGORY [all]
   -v, --verbose      verbosely report processing
       --version      print version information and exit
   -h, --help         print short or long help message and exit
"
long_help_message="
Warning categories include:
       'all'          show all warnings
       'none'         turn off all the warnings
       'error'        warnings are treated as fatal errors"
fatal_help="Try '\$progname --help' for more information."
func_hookable ()
{
    $debug_cmd
    func_append hookable_fns " $1"
}
func_add_hook ()
{
    $debug_cmd
    case " $hookable_fns " in
      *" $1 "*) ;;
      *) func_fatal_error "'$1' does not accept hook functions." ;;
    esac
    eval func_append ${1}_hooks '" $2"'
}
func_remove_hook ()
{
    $debug_cmd
    eval ${1}_hooks='`$ECHO "\$'$1'_hooks" |$SED "s| '$2'||"`'
}
func_propagate_result ()
{
    $debug_cmd
    func_propagate_result_result=:
    if eval "test \"\${${1}_result+set}\" = set"
    then
      eval "${2}_result=\$${1}_result"
    else
      func_propagate_result_result=false
    fi
}
func_run_hooks ()
{
    $debug_cmd
    case " $hookable_fns " in
      *" $1 "*) ;;
      *) func_fatal_error "'$1' does not support hook functions." ;;
    esac
    eval _G_hook_fns=\$$1_hooks; shift
    for _G_hook in $_G_hook_fns; do
      func_unset "${_G_hook}_result"
      eval $_G_hook '${1+"$@"}'
      func_propagate_result $_G_hook func_run_hooks
      if $func_propagate_result_result; then
        eval set dummy "$func_run_hooks_result"; shift
      fi
    done
}
func_options_finish ()
{
    $debug_cmd
    func_run_hooks func_options ${1+"$@"}
    func_propagate_result func_run_hooks func_options_finish
}
func_hookable func_options
func_options ()
{
    $debug_cmd
    _G_options_quoted=false
    for my_func in options_prep parse_options validate_options options_finish
    do
      func_unset func_${my_func}_result
      func_unset func_run_hooks_result
      eval func_$my_func '${1+"$@"}'
      func_propagate_result func_$my_func func_options
      if $func_propagate_result_result; then
        eval set dummy "$func_options_result"; shift
        _G_options_quoted=:
      fi
    done
    $_G_options_quoted || {
      func_quote eval ${1+"$@"}
      func_options_result=$func_quote_result
    }
}
func_hookable func_options_prep
func_options_prep ()
{
    $debug_cmd
    opt_verbose=false
    opt_warning_types=
    func_run_hooks func_options_prep ${1+"$@"}
    func_propagate_result func_run_hooks func_options_prep
}
func_hookable func_parse_options
func_parse_options ()
{
    $debug_cmd
    _G_parse_options_requote=false
    while test $# -gt 0; do
      func_run_hooks func_parse_options ${1+"$@"}
      func_propagate_result func_run_hooks func_parse_options
      if $func_propagate_result_result; then
        eval set dummy "$func_parse_options_result"; shift
        _G_parse_options_requote=false
      fi
      test $# -gt 0 || break
      _G_match_parse_options=:
      _G_opt=$1
      shift
      case $_G_opt in
        --debug|-x)   debug_cmd='set -x'
                      func_echo "enabling shell trace mode" >&2
                      $debug_cmd
                      ;;
        --no-warnings|--no-warning|--no-warn)
                      set dummy --warnings none ${1+"$@"}
                      shift
		      ;;
        --warnings|--warning|-W)
                      if test $# = 0 && func_missing_arg $_G_opt; then
                        _G_parse_options_requote=:
                        break
                      fi
                      case " $warning_categories $1" in
                        *" $1 "*)
                          func_append_uniq opt_warning_types " $1"
                          ;;
                        *all)
                          opt_warning_types=$warning_categories
                          ;;
                        *none)
                          opt_warning_types=none
                          warning_func=:
                          ;;
                        *error)
                          opt_warning_types=$warning_categories
                          warning_func=func_fatal_error
                          ;;
                        *)
                          func_fatal_error \
                             "unsupported warning category: '$1'"
                          ;;
                      esac
                      shift
                      ;;
        --verbose|-v) opt_verbose=: ;;
        --version)    func_version ;;
        -\?|-h)       func_usage ;;
        --help)       func_help ;;
	--*=*)        func_split_equals "$_G_opt"
	              set dummy "$func_split_equals_lhs" \
                          "$func_split_equals_rhs" ${1+"$@"}
                      shift
                      ;;
        -W*)
                      func_split_short_opt "$_G_opt"
                      set dummy "$func_split_short_opt_name" \
                          "$func_split_short_opt_arg" ${1+"$@"}
                      shift
                      ;;
        -\?*|-h*|-v*|-x*)
                      func_split_short_opt "$_G_opt"
                      set dummy "$func_split_short_opt_name" \
                          "-$func_split_short_opt_arg" ${1+"$@"}
                      shift
                      ;;
        --)           _G_parse_options_requote=: ; break ;;
        -*)           func_fatal_help "unrecognised option: '$_G_opt'" ;;
        *)            set dummy "$_G_opt" ${1+"$@"}; shift
                      _G_match_parse_options=false
                      break
                      ;;
      esac
      if $_G_match_parse_options; then
        _G_parse_options_requote=:
      fi
    done
    if $_G_parse_options_requote; then
      func_quote eval ${1+"$@"}
      func_parse_options_result=$func_quote_result
    fi
}
func_hookable func_validate_options
func_validate_options ()
{
    $debug_cmd
    test -n "$opt_warning_types" || opt_warning_types=" $warning_categories"
    func_run_hooks func_validate_options ${1+"$@"}
    func_propagate_result func_run_hooks func_validate_options
    $exit_cmd $EXIT_FAILURE
}
func_fatal_help ()
{
    $debug_cmd
    eval \$ECHO \""Usage: $usage"\"
    eval \$ECHO \""$fatal_help"\"
    func_error ${1+"$@"}
    exit $EXIT_FAILURE
}
func_help ()
{
    $debug_cmd
    func_usage_message
    $ECHO "$long_help_message"
    exit 0
}
func_missing_arg ()
{
    $debug_cmd
    func_error "Missing argument for '$1'."
    exit_cmd=exit
}
test -z "$_G_HAVE_XSI_OPS" \
    && (eval 'x=a/b/c;
      test 5aa/bb/cc = "${#x}${x%%/*}${x%/*}${x#*/}${x##*/}"') 2>/dev/null \
    && _G_HAVE_XSI_OPS=yes
if test yes = "$_G_HAVE_XSI_OPS"
then
  eval 'func_split_equals ()
  {
      $debug_cmd
      func_split_equals_lhs=${1%%=*}
      func_split_equals_rhs=${1#*=}
      if test "x$func_split_equals_lhs" = "x$1"; then
        func_split_equals_rhs=
      fi
  }'
else
  func_split_equals ()
  {
      $debug_cmd
      func_split_equals_lhs=`expr "x$1" : 'x\([^=]*\)'`
      func_split_equals_rhs=
      test "x$func_split_equals_lhs=" = "x$1" \
        || func_split_equals_rhs=`expr "x$1" : 'x[^=]*=\(.*\)$'`
  }
fi #func_split_equals
if test yes = "$_G_HAVE_XSI_OPS"
then
  eval 'func_split_short_opt ()
  {
      $debug_cmd
      func_split_short_opt_arg=${1#??}
      func_split_short_opt_name=${1%"$func_split_short_opt_arg"}
  }'
else
  func_split_short_opt ()
  {
      $debug_cmd
      func_split_short_opt_name=`expr "x$1" : 'x\(-.\)'`
      func_split_short_opt_arg=`expr "x$1" : 'x-.\(.*\)$'`
  }
fi #func_split_short_opt
func_usage ()
{
    $debug_cmd
    func_usage_message
    $ECHO "Run '$progname --help |${PAGER-more}' for full usage"
    exit 0
}
func_usage_message ()
{
    $debug_cmd
    eval \$ECHO \""Usage: $usage"\"
    echo
    $SED -n 's|^# ||
        /^Written by/{
          x;p;x
        }
	h
	/^Written by/q' < "$progpath"
    echo
    eval \$ECHO \""$usage_message"\"
}
func_version ()
{
    $debug_cmd
    printf '%s\n' "$progname $scriptversion"
    $SED -n '
        /^# Written by /!b
        s|^# ||; p; n
        :fwd2blnk
        /./ {
          n
          b fwd2blnk
        }
        p; n
        :holdwrnt
        s|^# ||
        s|^# *$||
        /^Copyright /!{
          /./H
          n
          b holdwrnt
        }
        s|\((C)\)[ 0-9,-]*[ ,-]\([1-9][0-9]* \)|\1 \2|
        G
        s|\(\n\)\n*|\1|g
        p; q' < "$progpath"
    exit $?
}
scriptversion='(GNU libtool) 2.4.7'
func_echo ()
{
    $debug_cmd
    _G_message=$*
    func_echo_IFS=$IFS
    IFS=$nl
    for _G_line in $_G_message; do
      IFS=$func_echo_IFS
      $ECHO "$progname${opt_mode+: $opt_mode}: $_G_line"
    done
    IFS=$func_echo_IFS
}
func_warning ()
{
    $debug_cmd
    $warning_func ${1+"$@"}
}
usage='$progpath [OPTION]... [MODE-ARG]...'
usage_message="Options:
       --config             show all configuration variables
       --debug              enable verbose shell tracing
   -n, --dry-run            display commands without modifying any files
       --features           display basic configuration information and exit
       --mode=MODE          use operation mode MODE
       --no-warnings        equivalent to '-Wnone'
       --preserve-dup-deps  don't remove duplicate dependency libraries
       --quiet, --silent    don't print informational messages
       --tag=TAG            use configuration variables from tag TAG
   -v, --verbose            print more informational messages than default
       --version            print version information
   -W, --warnings=CATEGORY  report the warnings falling in CATEGORY [all]
   -h, --help, --help-all   print short, long, or detailed help message
"
func_help ()
{
    $debug_cmd
    func_usage_message
    $ECHO "$long_help_message
MODE must be one of the following:
       clean           remove files from the build directory
       compile         compile a source file into a libtool object
       execute         automatically set library path, then run a program
       finish          complete the installation of libtool libraries
       install         install libraries or executables
       link            create a library or an executable
       uninstall       remove libraries from an installed directory
MODE-ARGS vary depending on the MODE.  When passed as first option,
'--mode=MODE' may be abbreviated as 'MODE' or a unique abbreviation of that.
Try '$progname --help --mode=MODE' for a more detailed description of MODE.
When reporting a bug, please describe a test case to reproduce it and
include the following information:
       host-triplet:   $host
       shell:          $SHELL
       compiler:       $LTCC
       compiler flags: $LTCFLAGS
       linker:         $LD (gnu? $with_gnu_ld)
       version:        $progname (GNU libtool) 2.4.7
       automake:       `($AUTOMAKE --version) 2>/dev/null |$SED 1q`
       autoconf:       `($AUTOCONF --version) 2>/dev/null |$SED 1q`
Report bugs to <bug-libtool@gnu.org>.
GNU libtool home page: <http://www.gnu.org/software/libtool/>.
General help using GNU software: <http://www.gnu.org/gethelp/>."
    exit 0
}
lo2o=s/\\.lo\$/.$objext/
o2lo=s/\\.$objext\$/.lo/
if test yes = "$_G_HAVE_XSI_OPS"; then
  eval 'func_lo2o ()
  {
    case $1 in
      *.lo) func_lo2o_result=${1%.lo}.$objext ;;
      *   ) func_lo2o_result=$1               ;;
    esac
  }'
  eval 'func_xform ()
  {
    func_xform_result=${1%.*}.lo
  }'
else
  func_lo2o ()
  {
    func_lo2o_result=`$ECHO "$1" | $SED "$lo2o"`
  }
  func_xform ()
  {
    func_xform_result=`$ECHO "$1" | $SED 's|\.[^.]*$|.lo|'`
  }
fi
func_fatal_configuration ()
{
    func_fatal_error ${1+"$@"} \
      "See the $PACKAGE documentation for more information." \
      "Fatal configuration error."
}
func_config ()
{
    re_begincf='^# ### BEGIN LIBTOOL'
    re_endcf='^# ### END LIBTOOL'
    $SED "1,/$re_begincf CONFIG/d;/$re_endcf CONFIG/,\$d" < "$progpath"
    for tagname in $taglist; do
      $SED -n "/$re_begincf TAG CONFIG: $tagname\$/,/$re_endcf TAG CONFIG: $tagname\$/p" < "$progpath"
    done
    exit $?
}
func_features ()
{
    echo "host: $host"
    if test yes = "$build_libtool_libs"; then
      echo "enable shared libraries"
    else
      echo "disable shared libraries"
    fi
    if test yes = "$build_old_libs"; then
      echo "enable static libraries"
    else
      echo "disable static libraries"
    fi
    exit $?
}
func_enable_tag ()
{
    tagname=$1
    re_begincf="^# ### BEGIN LIBTOOL TAG CONFIG: $tagname\$"
    re_endcf="^# ### END LIBTOOL TAG CONFIG: $tagname\$"
    sed_extractcf=/$re_begincf/,/$re_endcf/p
    case $tagname in
      *[!-_A-Za-z0-9,/]*)
        func_fatal_error "invalid tag name: $tagname"
        ;;
    esac
    case $tagname in
        CC) ;;
    *)
        if $GREP "$re_begincf" "$progpath" >/dev/null 2>&1; then
	  taglist="$taglist $tagname"
	  extractedcf=`$SED -n -e "$sed_extractcf" < "$progpath"`
	  eval "$extractedcf"
        else
	  func_error "ignoring unknown tag $tagname"
        fi
        ;;
    esac
}
func_check_version_match ()
{
    if test "$package_revision" != "$macro_revision"; then
      if test "$VERSION" != "$macro_version"; then
        if test -z "$macro_version"; then
          cat >&2 <<_LT_EOF
$progname: Version mismatch error.  This is $PACKAGE $VERSION, but the
$progname: definition of this LT_INIT comes from an older release.
$progname: You should recreate aclocal.m4 with macros from $PACKAGE $VERSION
$progname: and run autoconf again.
_LT_EOF
        else
          cat >&2 <<_LT_EOF
$progname: Version mismatch error.  This is $PACKAGE $VERSION, but the
$progname: definition of this LT_INIT comes from $PACKAGE $macro_version.
$progname: You should recreate aclocal.m4 with macros from $PACKAGE $VERSION
$progname: and run autoconf again.
_LT_EOF
        fi
      else
        cat >&2 <<_LT_EOF
$progname: Version mismatch error.  This is $PACKAGE $VERSION, revision $package_revision,
$progname: but the definition of this LT_INIT comes from revision $macro_revision.
$progname: You should recreate aclocal.m4 with macros from revision $package_revision
$progname: of $PACKAGE $VERSION and run autoconf again.
_LT_EOF
      fi
      exit $EXIT_MISMATCH
    fi
}
libtool_options_prep ()
{
    $debug_mode
    opt_config=false
    opt_dlopen=
    opt_dry_run=false
    opt_help=false
    opt_mode=
    opt_preserve_dup_deps=false
    opt_quiet=false
    nonopt=
    preserve_args=
    _G_rc_lt_options_prep=:
    case $1 in
    clean|clea|cle|cl)
      shift; set dummy --mode clean ${1+"$@"}; shift
      ;;
    compile|compil|compi|comp|com|co|c)
      shift; set dummy --mode compile ${1+"$@"}; shift
      ;;
    execute|execut|execu|exec|exe|ex|e)
      shift; set dummy --mode execute ${1+"$@"}; shift
      ;;
    finish|finis|fini|fin|fi|f)
      shift; set dummy --mode finish ${1+"$@"}; shift
      ;;
    install|instal|insta|inst|ins|in|i)
      shift; set dummy --mode install ${1+"$@"}; shift
      ;;
    link|lin|li|l)
      shift; set dummy --mode link ${1+"$@"}; shift
      ;;
    uninstall|uninstal|uninsta|uninst|unins|unin|uni|un|u)
      shift; set dummy --mode uninstall ${1+"$@"}; shift
      ;;
    *)
      _G_rc_lt_options_prep=false
      ;;
    esac
    if $_G_rc_lt_options_prep; then
      func_quote eval ${1+"$@"}
      libtool_options_prep_result=$func_quote_result
    fi
}
func_add_hook func_options_prep libtool_options_prep
libtool_parse_options ()
{
    $debug_cmd
    _G_rc_lt_parse_options=false
    while test $# -gt 0; do
      _G_match_lt_parse_options=:
      _G_opt=$1
      shift
      case $_G_opt in
        --dry-run|--dryrun|-n)
                        opt_dry_run=:
                        ;;
        --config)       func_config ;;
        --dlopen|-dlopen)
                        opt_dlopen="${opt_dlopen+$opt_dlopen
}$1"
                        shift
                        ;;
        --preserve-dup-deps)
                        opt_preserve_dup_deps=: ;;
        --features)     func_features ;;
        --finish)       set dummy --mode finish ${1+"$@"}; shift ;;
        --help)         opt_help=: ;;
        --help-all)     opt_help=': help-all' ;;
        --mode)         test $# = 0 && func_missing_arg $_G_opt && break
                        opt_mode=$1
                        case $1 in
                          clean|compile|execute|finish|install|link|relink|uninstall) ;;
                          *) func_error "invalid argument for $_G_opt"
                             exit_cmd=exit
                             break
                             ;;
                        esac
                        shift
                        ;;
        --no-silent|--no-quiet)
                        opt_quiet=false
                        func_append preserve_args " $_G_opt"
                        ;;
        --no-warnings|--no-warning|--no-warn)
                        opt_warning=false
                        func_append preserve_args " $_G_opt"
                        ;;
        --no-verbose)
                        opt_verbose=false
                        func_append preserve_args " $_G_opt"
                        ;;
        --silent|--quiet)
                        opt_quiet=:
                        opt_verbose=false
                        func_append preserve_args " $_G_opt"
                        ;;
        --tag)          test $# = 0 && func_missing_arg $_G_opt && break
                        opt_tag=$1
                        func_append preserve_args " $_G_opt $1"
                        func_enable_tag "$1"
                        shift
                        ;;
        --verbose|-v)   opt_quiet=false
                        opt_verbose=:
                        func_append preserve_args " $_G_opt"
                        ;;
        *)              set dummy "$_G_opt" ${1+"$@"} ; shift
                        _G_match_lt_parse_options=false
                        break
                        ;;
      esac
      $_G_match_lt_parse_options && _G_rc_lt_parse_options=:
    done
    if $_G_rc_lt_parse_options; then
      func_quote eval ${1+"$@"}
      libtool_parse_options_result=$func_quote_result
    fi
}
func_add_hook func_parse_options libtool_parse_options
libtool_validate_options ()
{
    if test 0 -lt $#; then
      nonopt=$1
      shift
    fi
    test : = "$debug_cmd" || func_append preserve_args " --debug"
    opt_duplicate_compiler_generated_deps=:
    $opt_help || {
      func_check_version_match
      test yes != "$build_libtool_libs" \
        && test yes != "$build_old_libs" \
        && func_fatal_configuration "not configured to build any kind of library"
      eval std_shrext=\"$shrext_cmds\"
      if test -n "$opt_dlopen" && test execute != "$opt_mode"; then
        func_error "unrecognized option '-dlopen'"
        $ECHO "$help" 1>&2
        exit $EXIT_FAILURE
      fi
      generic_help=$help
      help="Try '$progname --help --mode=$opt_mode' for more information."
    }
    func_quote eval ${1+"$@"}
    libtool_validate_options_result=$func_quote_result
}
func_add_hook func_validate_options libtool_validate_options
func_options ${1+"$@"}
eval set dummy "$func_options_result"; shift
magic='%%%MAGIC variable%%%'
magic_exe='%%%MAGIC EXE variable%%%'
extracted_archives=
extracted_serial=0
exec_cmd=
func_fallback_echo ()
{
  eval 'cat <<_LTECHO_EOF
$1
_LTECHO_EOF'
}
func_generated_by_libtool_p ()
{
  $GREP "^# Generated by .*$PACKAGE" > /dev/null 2>&1
}
func_lalib_p ()
{
    test -f "$1" &&
      $SED -e 4q "$1" 2>/dev/null | func_generated_by_libtool_p
}
func_lalib_unsafe_p ()
{
    lalib_p=no
    if test -f "$1" && test -r "$1" && exec 5<&0 <"$1"; then
	for lalib_p_l in 1 2 3 4
	do
	    read lalib_p_line
	    case $lalib_p_line in
		\#\ Generated\ by\ *$PACKAGE* ) lalib_p=yes; break;;
	    esac
	done
	exec 0<&5 5<&-
    fi
    test yes = "$lalib_p"
}
func_ltwrapper_script_p ()
{
    test -f "$1" &&
      $lt_truncate_bin < "$1" 2>/dev/null | func_generated_by_libtool_p
}
func_ltwrapper_executable_p ()
{
    func_ltwrapper_exec_suffix=
    case $1 in
    *.exe) ;;
    *) func_ltwrapper_exec_suffix=.exe ;;
    esac
    $GREP "$magic_exe" "$1$func_ltwrapper_exec_suffix" >/dev/null 2>&1
}
func_ltwrapper_scriptname ()
{
    func_dirname_and_basename "$1" "" "."
    func_stripname '' '.exe' "$func_basename_result"
    func_ltwrapper_scriptname_result=$func_dirname_result/$objdir/${func_stripname_result}_ltshwrapper
}
func_ltwrapper_p ()
{
    func_ltwrapper_script_p "$1" || func_ltwrapper_executable_p "$1"
}
func_execute_cmds ()
{
    $debug_cmd
    save_ifs=$IFS; IFS='~'
    for cmd in $1; do
      IFS=$sp$nl
      eval cmd=\"$cmd\"
      IFS=$save_ifs
      func_show_eval "$cmd" "${2-:}"
    done
    IFS=$save_ifs
}
func_source ()
{
    $debug_cmd
    case $1 in
    */* | *\\*)	. "$1" ;;
    *)		. "./$1" ;;
    esac
}
func_resolve_sysroot ()
{
  func_resolve_sysroot_result=$1
  case $func_resolve_sysroot_result in
  =*)
    func_stripname '=' '' "$func_resolve_sysroot_result"
    func_resolve_sysroot_result=$lt_sysroot$func_stripname_result
    ;;
  esac
}
func_replace_sysroot ()
{
  case $lt_sysroot:$1 in
  ?*:"$lt_sysroot"*)
    func_stripname "$lt_sysroot" '' "$1"
    func_replace_sysroot_result='='$func_stripname_result
    ;;
  *)
    func_replace_sysroot_result=$1
    ;;
  esac
}
func_infer_tag ()
{
    $debug_cmd
    if test -n "$available_tags" && test -z "$tagname"; then
      CC_quoted=
      for arg in $CC; do
	func_append_quoted CC_quoted "$arg"
      done
      CC_expanded=`func_echo_all $CC`
      CC_quoted_expanded=`func_echo_all $CC_quoted`
      case $@ in
      " $CC "* | "$CC "* | " $CC_expanded "* | "$CC_expanded "* | \
      " $CC_quoted"* | "$CC_quoted "* | " $CC_quoted_expanded "* | "$CC_quoted_expanded "*) ;;
      *)
	for z in $available_tags; do
	  if $GREP "^# ### BEGIN LIBTOOL TAG CONFIG: $z$" < "$progpath" > /dev/null; then
	    eval "`$SED -n -e '/^# ### BEGIN LIBTOOL TAG CONFIG: '$z'$/,/^# ### END LIBTOOL TAG CONFIG: '$z'$/p' < $progpath`"
	    CC_quoted=
	    for arg in $CC; do
	      func_append_quoted CC_quoted "$arg"
	    done
	    CC_expanded=`func_echo_all $CC`
	    CC_quoted_expanded=`func_echo_all $CC_quoted`
	    case "$@ " in
	    " $CC "* | "$CC "* | " $CC_expanded "* | "$CC_expanded "* | \
	    " $CC_quoted"* | "$CC_quoted "* | " $CC_quoted_expanded "* | "$CC_quoted_expanded "*)
	      tagname=$z
	      break
	      ;;
	    esac
	  fi
	done
	if test -z "$tagname"; then
	  func_echo "unable to infer tagged configuration"
	  func_fatal_error "specify a tag with '--tag'"
	fi
	;;
      esac
    fi
}
func_write_libtool_object ()
{
    write_libobj=$1
    if test yes = "$build_libtool_libs"; then
      write_lobj=\'$2\'
    else
      write_lobj=none
    fi
    if test yes = "$build_old_libs"; then
      write_oldobj=\'$3\'
    else
      write_oldobj=none
    fi
    $opt_dry_run || {
      cat >${write_libobj}T <<EOF
pic_object=$write_lobj
non_pic_object=$write_oldobj
EOF
      $MV "${write_libobj}T" "$write_libobj"
    }
}
func_convert_core_file_wine_to_w32 ()
{
  $debug_cmd
  func_convert_core_file_wine_to_w32_result=$1
  if test -n "$1"; then
    func_convert_core_file_wine_to_w32_tmp=`winepath -w "$1" 2>/dev/null`
    if test "$?" -eq 0 && test -n "$func_convert_core_file_wine_to_w32_tmp"; then
      func_convert_core_file_wine_to_w32_result=`$ECHO "$func_convert_core_file_wine_to_w32_tmp" |
        $SED -e "$sed_naive_backslashify"`
    else
      func_convert_core_file_wine_to_w32_result=
    fi
  fi
}
func_convert_core_path_wine_to_w32 ()
{
  $debug_cmd
  func_convert_core_path_wine_to_w32_result=
  if test -n "$1"; then
    oldIFS=$IFS
    IFS=:
    for func_convert_core_path_wine_to_w32_f in $1; do
      IFS=$oldIFS
      func_convert_core_file_wine_to_w32 "$func_convert_core_path_wine_to_w32_f"
      if test -n "$func_convert_core_file_wine_to_w32_result"; then
        if test -z "$func_convert_core_path_wine_to_w32_result"; then
          func_convert_core_path_wine_to_w32_result=$func_convert_core_file_wine_to_w32_result
        else
          func_append func_convert_core_path_wine_to_w32_result ";$func_convert_core_file_wine_to_w32_result"
        fi
      fi
    done
    IFS=$oldIFS
  fi
}
func_cygpath ()
{
  $debug_cmd
  if test -n "$LT_CYGPATH" && test -f "$LT_CYGPATH"; then
    func_cygpath_result=`$LT_CYGPATH "$@" 2>/dev/null`
    if test "$?" -ne 0; then
      func_cygpath_result=
    fi
  else
    func_cygpath_result=
    func_error "LT_CYGPATH is empty or specifies non-existent file: '$LT_CYGPATH'"
  fi
}
func_convert_core_msys_to_w32 ()
{
  $debug_cmd
  func_convert_core_msys_to_w32_result=`( cmd //c echo "$1" ) 2>/dev/null |
    $SED -e 's/[ ]*$//' -e "$sed_naive_backslashify"`
}
func_convert_file_check ()
{
  $debug_cmd
  if test -z "$2" && test -n "$1"; then
    func_error "Could not determine host file name corresponding to"
    func_error "  '$1'"
    func_error "Continuing, but uninstalled executables may not work."
    func_to_host_file_result=$1
  fi
}
func_convert_path_check ()
{
  $debug_cmd
  if test -z "$4" && test -n "$3"; then
    func_error "Could not determine the host path corresponding to"
    func_error "  '$3'"
    func_error "Continuing, but uninstalled executables may not work."
    if test "x$1" != "x$2"; then
      lt_replace_pathsep_chars="s|$1|$2|g"
      func_to_host_path_result=`echo "$3" |
        $SED -e "$lt_replace_pathsep_chars"`
    else
      func_to_host_path_result=$3
    fi
  fi
}
func_convert_path_front_back_pathsep ()
{
  $debug_cmd
  case $4 in
  $1 ) func_to_host_path_result=$3$func_to_host_path_result
    ;;
  esac
  case $4 in
  $2 ) func_append func_to_host_path_result "$3"
    ;;
  esac
}
func_to_host_file ()
{
  $debug_cmd
  $to_host_file_cmd "$1"
}
func_to_tool_file ()
{
  $debug_cmd
  case ,$2, in
    *,"$to_tool_file_cmd",*)
      func_to_tool_file_result=$1
      ;;
    *)
      $to_tool_file_cmd "$1"
      func_to_tool_file_result=$func_to_host_file_result
      ;;
  esac
}
func_convert_file_noop ()
{
  func_to_host_file_result=$1
}
func_convert_file_msys_to_w32 ()
{
  $debug_cmd
  func_to_host_file_result=$1
  if test -n "$1"; then
    func_convert_core_msys_to_w32 "$1"
    func_to_host_file_result=$func_convert_core_msys_to_w32_result
  fi
  func_convert_file_check "$1" "$func_to_host_file_result"
}
func_convert_file_cygwin_to_w32 ()
{
  $debug_cmd
  func_to_host_file_result=$1
  if test -n "$1"; then
    func_to_host_file_result=`cygpath -m "$1"`
  fi
  func_convert_file_check "$1" "$func_to_host_file_result"
}
func_convert_file_nix_to_w32 ()
{
  $debug_cmd
  func_to_host_file_result=$1
  if test -n "$1"; then
    func_convert_core_file_wine_to_w32 "$1"
    func_to_host_file_result=$func_convert_core_file_wine_to_w32_result
  fi
  func_convert_file_check "$1" "$func_to_host_file_result"
}
func_convert_file_msys_to_cygwin ()
{
  $debug_cmd
  func_to_host_file_result=$1
  if test -n "$1"; then
    func_convert_core_msys_to_w32 "$1"
    func_cygpath -u "$func_convert_core_msys_to_w32_result"
    func_to_host_file_result=$func_cygpath_result
  fi
  func_convert_file_check "$1" "$func_to_host_file_result"
}
func_convert_file_nix_to_cygwin ()
{
  $debug_cmd
  func_to_host_file_result=$1
  if test -n "$1"; then
    func_convert_core_file_wine_to_w32 "$1"
    func_cygpath -u "$func_convert_core_file_wine_to_w32_result"
    func_to_host_file_result=$func_cygpath_result
  fi
  func_convert_file_check "$1" "$func_to_host_file_result"
}
to_host_path_cmd=
func_init_to_host_path_cmd ()
{
  $debug_cmd
  if test -z "$to_host_path_cmd"; then
    func_stripname 'func_convert_file_' '' "$to_host_file_cmd"
    to_host_path_cmd=func_convert_path_$func_stripname_result
  fi
}
func_to_host_path ()
{
  $debug_cmd
  func_init_to_host_path_cmd
  $to_host_path_cmd "$1"
}
func_convert_path_noop ()
{
  func_to_host_path_result=$1
}
func_convert_path_msys_to_w32 ()
{
  $debug_cmd
  func_to_host_path_result=$1
  if test -n "$1"; then
    func_stripname : : "$1"
    func_to_host_path_tmp1=$func_stripname_result
    func_convert_core_msys_to_w32 "$func_to_host_path_tmp1"
    func_to_host_path_result=$func_convert_core_msys_to_w32_result
    func_convert_path_check : ";" \
      "$func_to_host_path_tmp1" "$func_to_host_path_result"
    func_convert_path_front_back_pathsep ":*" "*:" ";" "$1"
  fi
}
func_convert_path_cygwin_to_w32 ()
{
  $debug_cmd
  func_to_host_path_result=$1
  if test -n "$1"; then
    func_stripname : : "$1"
    func_to_host_path_tmp1=$func_stripname_result
    func_to_host_path_result=`cygpath -m -p "$func_to_host_path_tmp1"`
    func_convert_path_check : ";" \
      "$func_to_host_path_tmp1" "$func_to_host_path_result"
    func_convert_path_front_back_pathsep ":*" "*:" ";" "$1"
  fi
}
func_convert_path_nix_to_w32 ()
{
  $debug_cmd
  func_to_host_path_result=$1
  if test -n "$1"; then
    func_stripname : : "$1"
    func_to_host_path_tmp1=$func_stripname_result
    func_convert_core_path_wine_to_w32 "$func_to_host_path_tmp1"
    func_to_host_path_result=$func_convert_core_path_wine_to_w32_result
    func_convert_path_check : ";" \
      "$func_to_host_path_tmp1" "$func_to_host_path_result"
    func_convert_path_front_back_pathsep ":*" "*:" ";" "$1"
  fi
}
func_convert_path_msys_to_cygwin ()
{
  $debug_cmd
  func_to_host_path_result=$1
  if test -n "$1"; then
    func_stripname : : "$1"
    func_to_host_path_tmp1=$func_stripname_result
    func_convert_core_msys_to_w32 "$func_to_host_path_tmp1"
    func_cygpath -u -p "$func_convert_core_msys_to_w32_result"
    func_to_host_path_result=$func_cygpath_result
    func_convert_path_check : : \
      "$func_to_host_path_tmp1" "$func_to_host_path_result"
    func_convert_path_front_back_pathsep ":*" "*:" : "$1"
  fi
}
func_convert_path_nix_to_cygwin ()
{
  $debug_cmd
  func_to_host_path_result=$1
  if test -n "$1"; then
    func_stripname : : "$1"
    func_to_host_path_tmp1=$func_stripname_result
    func_convert_core_path_wine_to_w32 "$func_to_host_path_tmp1"
    func_cygpath -u -p "$func_convert_core_path_wine_to_w32_result"
    func_to_host_path_result=$func_cygpath_result
    func_convert_path_check : : \
      "$func_to_host_path_tmp1" "$func_to_host_path_result"
    func_convert_path_front_back_pathsep ":*" "*:" : "$1"
  fi
}
func_dll_def_p ()
{
  $debug_cmd
  func_dll_def_p_tmp=`$SED -n \
    -e 's/^[	 ]*//' \
    -e '/^\(;.*\)*$/d' \
    -e 's/^\(EXPORTS\|LIBRARY\)\([	 ].*\)*$/DEF/p' \
    -e q \
    "$1"`
  test DEF = "$func_dll_def_p_tmp"
}
func_mode_compile ()
{
    $debug_cmd
    base_compile=
    srcfile=$nonopt  #  always keep a non-empty value in "srcfile"
    suppress_opt=yes
    suppress_output=
    arg_mode=normal
    libobj=
    later=
    pie_flag=
    for arg
    do
      case $arg_mode in
      arg  )
	lastarg=$arg
	arg_mode=normal
	;;
      target )
	libobj=$arg
	arg_mode=normal
	continue
	;;
      normal )
	case $arg in
	-o)
	  test -n "$libobj" && \
	    func_fatal_error "you cannot specify '-o' more than once"
	  arg_mode=target
	  continue
	  ;;
	-pie | -fpie | -fPIE)
          func_append pie_flag " $arg"
	  continue
	  ;;
	-shared | -static | -prefer-pic | -prefer-non-pic)
	  func_append later " $arg"
	  continue
	  ;;
	-no-suppress)
	  suppress_opt=no
	  continue
	  ;;
	-Xcompiler)
	  arg_mode=arg  #  the next one goes into the "base_compile" arg list
	  continue      #  The current "srcfile" will either be retained or
	  ;;            #  replaced later.  I would guess that would be a bug.
	-Wc,*)
	  func_stripname '-Wc,' '' "$arg"
	  args=$func_stripname_result
	  lastarg=
	  save_ifs=$IFS; IFS=,
	  for arg in $args; do
	    IFS=$save_ifs
	    func_append_quoted lastarg "$arg"
	  done
	  IFS=$save_ifs
	  func_stripname ' ' '' "$lastarg"
	  lastarg=$func_stripname_result
	  func_append base_compile " $lastarg"
	  continue
	  ;;
	*)
	  lastarg=$srcfile
	  srcfile=$arg
	  ;;
	esac  #  case $arg
	;;
      esac    #  case $arg_mode
      func_append_quoted base_compile "$lastarg"
    done # for arg
    case $arg_mode in
    arg)
      func_fatal_error "you must specify an argument for -Xcompile"
      ;;
    target)
      func_fatal_error "you must specify a target with '-o'"
      ;;
    *)
      test -z "$libobj" && {
	func_basename "$srcfile"
	libobj=$func_basename_result
      }
      ;;
    esac
    case $libobj in
    *.[cCFSifmso] | \
    *.ada | *.adb | *.ads | *.asm | \
    *.c++ | *.cc | *.ii | *.class | *.cpp | *.cxx | \
    *.[fF][09]? | *.for | *.java | *.go | *.obj | *.sx | *.cu | *.cup)
      func_xform "$libobj"
      libobj=$func_xform_result
      ;;
    esac
    case $libobj in
    *.lo) func_lo2o "$libobj"; obj=$func_lo2o_result ;;
    *)
      func_fatal_error "cannot determine name of library object from '$libobj'"
      ;;
    esac
    func_infer_tag $base_compile
    for arg in $later; do
      case $arg in
      -shared)
	test yes = "$build_libtool_libs" \
	  || func_fatal_configuration "cannot build a shared library"
	build_old_libs=no
	continue
	;;
      -static)
	build_libtool_libs=no
	build_old_libs=yes
	continue
	;;
      -prefer-pic)
	pic_mode=yes
	continue
	;;
      -prefer-non-pic)
	pic_mode=no
	continue
	;;
      esac
    done
    func_quote_arg pretty "$libobj"
    test "X$libobj" != "X$func_quote_arg_result" \
      && $ECHO "X$libobj" | $GREP '[]~#^*{};<>?"'"'"'	 &()|`$[]' \
      && func_warning "libobj name '$libobj' may not contain shell special characters."
    func_dirname_and_basename "$obj" "/" ""
    objname=$func_basename_result
    xdir=$func_dirname_result
    lobj=$xdir$objdir/$objname
    test -z "$base_compile" && \
      func_fatal_help "you must specify a compilation command"
    if test yes = "$build_old_libs"; then
      removelist="$obj $lobj $libobj ${libobj}T"
    else
      removelist="$lobj $libobj ${libobj}T"
    fi
    case $host_os in
    cygwin* | mingw* | pw32* | os2* | cegcc*)
      pic_mode=default
      ;;
    esac
    if test no = "$pic_mode" && test pass_all != "$deplibs_check_method"; then
      pic_mode=default
    fi
    if test no = "$compiler_c_o"; then
      output_obj=`$ECHO "$srcfile" | $SED 's%^.*/%%; s%\.[^.]*$%%'`.$objext
      lockfile=$output_obj.lock
    else
      output_obj=
      need_locks=no
      lockfile=
    fi
    if test yes = "$need_locks"; then
      until $opt_dry_run || ln "$progpath" "$lockfile" 2>/dev/null; do
	func_echo "Waiting for $lockfile to be removed"
	sleep 2
      done
    elif test warn = "$need_locks"; then
      if test -f "$lockfile"; then
	$ECHO "\
*** ERROR, $lockfile exists and contains:
`cat $lockfile 2>/dev/null`
This indicates that another process is trying to use the same
temporary object file, and libtool could not work around it because
your compiler does not support '-c' and '-o' together.  If you
repeat this compilation, it may succeed, by chance, but you had better
avoid parallel builds (make -j) in this platform, or get a better
compiler."
	$opt_dry_run || $RM $removelist
	exit $EXIT_FAILURE
      fi
      func_append removelist " $output_obj"
      $ECHO "$srcfile" > "$lockfile"
    fi
    $opt_dry_run || $RM $removelist
    func_append removelist " $lockfile"
    trap '$opt_dry_run || $RM $removelist; exit $EXIT_FAILURE' 1 2 15
    func_to_tool_file "$srcfile" func_convert_file_msys_to_w32
    srcfile=$func_to_tool_file_result
    func_quote_arg pretty "$srcfile"
    qsrcfile=$func_quote_arg_result
    if test yes = "$build_libtool_libs"; then
      fbsd_hideous_sh_bug=$base_compile
      if test no != "$pic_mode"; then
	command="$base_compile $qsrcfile $pic_flag"
      else
	command="$base_compile $qsrcfile"
      fi
      func_mkdir_p "$xdir$objdir"
      if test -z "$output_obj"; then
	func_append command " -o $lobj"
      fi
      func_show_eval_locale "$command"	\
          'test -n "$output_obj" && $RM $removelist; exit $EXIT_FAILURE'
      if test warn = "$need_locks" &&
	 test "X`cat $lockfile 2>/dev/null`" != "X$srcfile"; then
	$ECHO "\
*** ERROR, $lockfile contains:
`cat $lockfile 2>/dev/null`
but it should contain:
$srcfile
This indicates that another process is trying to use the same
temporary object file, and libtool could not work around it because
your compiler does not support '-c' and '-o' together.  If you
repeat this compilation, it may succeed, by chance, but you had better
avoid parallel builds (make -j) in this platform, or get a better
compiler."
	$opt_dry_run || $RM $removelist
	exit $EXIT_FAILURE
      fi
      if test -n "$output_obj" && test "X$output_obj" != "X$lobj"; then
	func_show_eval '$MV "$output_obj" "$lobj"' \
	  'error=$?; $opt_dry_run || $RM $removelist; exit $error'
      fi
      if test yes = "$suppress_opt"; then
	suppress_output=' >/dev/null 2>&1'
      fi
    fi
    if test yes = "$build_old_libs"; then
      if test yes != "$pic_mode"; then
	command="$base_compile $qsrcfile$pie_flag"
      else
	command="$base_compile $qsrcfile $pic_flag"
      fi
      if test yes = "$compiler_c_o"; then
	func_append command " -o $obj"
      fi
      func_append command "$suppress_output"
      func_show_eval_locale "$command" \
        '$opt_dry_run || $RM $removelist; exit $EXIT_FAILURE'
      if test warn = "$need_locks" &&
	 test "X`cat $lockfile 2>/dev/null`" != "X$srcfile"; then
	$ECHO "\
*** ERROR, $lockfile contains:
`cat $lockfile 2>/dev/null`
but it should contain:
$srcfile
This indicates that another process is trying to use the same
temporary object file, and libtool could not work around it because
your compiler does not support '-c' and '-o' together.  If you
repeat this compilation, it may succeed, by chance, but you had better
avoid parallel builds (make -j) in this platform, or get a better
compiler."
	$opt_dry_run || $RM $removelist
	exit $EXIT_FAILURE
      fi
      if test -n "$output_obj" && test "X$output_obj" != "X$obj"; then
	func_show_eval '$MV "$output_obj" "$obj"' \
	  'error=$?; $opt_dry_run || $RM $removelist; exit $error'
      fi
    fi
    $opt_dry_run || {
      func_write_libtool_object "$libobj" "$objdir/$objname" "$objname"
      if test no != "$need_locks"; then
	removelist=$lockfile
        $RM "$lockfile"
      fi
    }
    exit $EXIT_SUCCESS
}
$opt_help || {
  test compile = "$opt_mode" && func_mode_compile ${1+"$@"}
}
func_mode_help ()
{
    case $opt_mode in
      "")
        func_help
        ;;
      clean)
        $ECHO \
"Usage: $progname [OPTION]... --mode=clean RM [RM-OPTION]... FILE...
Remove files from the build directory.
RM is the name of the program to use to delete files associated with each FILE
(typically '/bin/rm').  RM-OPTIONS are options (such as '-f') to be passed
to RM.
If FILE is a libtool library, object or program, all the files associated
with it are deleted. Otherwise, only FILE itself is deleted using RM."
        ;;
      compile)
      $ECHO \
"Usage: $progname [OPTION]... --mode=compile COMPILE-COMMAND... SOURCEFILE
Compile a source file into a libtool library object.
This mode accepts the following additional options:
  -o OUTPUT-FILE    set the output file name to OUTPUT-FILE
  -no-suppress      do not suppress compiler output for multiple passes
  -prefer-pic       try to build PIC objects only
  -prefer-non-pic   try to build non-PIC objects only
  -shared           do not build a '.o' file suitable for static linking
  -static           only build a '.o' file suitable for static linking
  -Wc,FLAG
  -Xcompiler FLAG   pass FLAG directly to the compiler
COMPILE-COMMAND is a command to be used in creating a 'standard' object file
from the given SOURCEFILE.
The output file name is determined by removing the directory component from
SOURCEFILE, then substituting the C source code suffix '.c' with the
library object suffix, '.lo'."
        ;;
      execute)
        $ECHO \
"Usage: $progname [OPTION]... --mode=execute COMMAND [ARGS]...
Automatically set library path, then run a program.
This mode accepts the following additional options:
  -dlopen FILE      add the directory containing FILE to the library path
This mode sets the library path environment variable according to '-dlopen'
flags.
If any of the ARGS are libtool executable wrappers, then they are translated
into their corresponding uninstalled binary, and any of their required library
directories are added to the library path.
Then, COMMAND is executed, with ARGS as arguments."
        ;;
      finish)
        $ECHO \
"Usage: $progname [OPTION]... --mode=finish [LIBDIR]...
Complete the installation of libtool libraries.
Each LIBDIR is a directory that contains libtool libraries.
The commands that this mode executes may require superuser privileges.  Use
the '--dry-run' option if you just want to see what would be executed."
        ;;
      install)
        $ECHO \
"Usage: $progname [OPTION]... --mode=install INSTALL-COMMAND...
Install executables or libraries.
INSTALL-COMMAND is the installation command.  The first component should be
either the 'install' or 'cp' program.
The following components of INSTALL-COMMAND are treated specially:
  -inst-prefix-dir PREFIX-DIR  Use PREFIX-DIR as a staging area for installation
The rest of the components are interpreted as arguments to that command (only
BSD-compatible install options are recognized)."
        ;;
      link)
        $ECHO \
"Usage: $progname [OPTION]... --mode=link LINK-COMMAND...
Link object files or libraries together to form another library, or to
create an executable program.
LINK-COMMAND is a command using the C compiler that you would use to create
a program from several object files.
The following components of LINK-COMMAND are treated specially:
  -all-static       do not do any dynamic linking at all
  -avoid-version    do not add a version suffix if possible
  -bindir BINDIR    specify path to binaries directory (for systems where
                    libraries must be found in the PATH setting at runtime)
  -dlopen FILE      '-dlpreopen' FILE if it cannot be dlopened at runtime
  -dlpreopen FILE   link in FILE and add its symbols to lt_preloaded_symbols
  -export-dynamic   allow symbols from OUTPUT-FILE to be resolved with dlsym(3)
  -export-symbols SYMFILE
                    try to export only the symbols listed in SYMFILE
  -export-symbols-regex REGEX
                    try to export only the symbols matching REGEX
  -LLIBDIR          search LIBDIR for required installed libraries
  -lNAME            OUTPUT-FILE requires the installed library libNAME
  -module           build a library that can dlopened
  -no-fast-install  disable the fast-install mode
  -no-install       link a not-installable executable
  -no-undefined     declare that a library does not refer to external symbols
  -o OUTPUT-FILE    create OUTPUT-FILE from the specified objects
  -objectlist FILE  use a list of object files found in FILE to specify objects
  -os2dllname NAME  force a short DLL name on OS/2 (no effect on other OSes)
  -precious-files-regex REGEX
                    don't remove output files matching REGEX
  -release RELEASE  specify package release information
  -rpath LIBDIR     the created library will eventually be installed in LIBDIR
  -R[ ]LIBDIR       add LIBDIR to the runtime path of programs and libraries
  -shared           only do dynamic linking of libtool libraries
  -shrext SUFFIX    override the standard shared library file extension
  -static           do not do any dynamic linking of uninstalled libtool libraries
  -static-libtool-libs
                    do not do any dynamic linking of libtool libraries
  -version-info CURRENT[:REVISION[:AGE]]
                    specify library version info [each variable defaults to 0]
  -weak LIBNAME     declare that the target provides the LIBNAME interface
  -Wc,FLAG
  -Xcompiler FLAG   pass linker-specific FLAG directly to the compiler
  -Wa,FLAG
  -Xassembler FLAG  pass linker-specific FLAG directly to the assembler
  -Wl,FLAG
  -Xlinker FLAG     pass linker-specific FLAG directly to the linker
  -XCClinker FLAG   pass link-specific FLAG to the compiler driver (CC)
All other options (arguments beginning with '-') are ignored.
Every other argument is treated as a filename.  Files ending in '.la' are
treated as uninstalled libtool libraries, other files are standard or library
object files.
If the OUTPUT-FILE ends in '.la', then a libtool library is created,
only library objects ('.lo' files) may be specified, and '-rpath' is
required, except when creating a convenience library.
If OUTPUT-FILE ends in '.a' or '.lib', then a standard library is created
using 'ar' and 'ranlib', or on Windows using 'lib'.
If OUTPUT-FILE ends in '.lo' or '.$objext', then a reloadable object file
is created, otherwise an executable program is created."
        ;;
      uninstall)
        $ECHO \
"Usage: $progname [OPTION]... --mode=uninstall RM [RM-OPTION]... FILE...
Remove libraries from an installation directory.
RM is the name of the program to use to delete files associated with each FILE
(typically '/bin/rm').  RM-OPTIONS are options (such as '-f') to be passed
to RM.
If FILE is a libtool library, all the files associated with it are deleted.
Otherwise, only FILE itself is deleted using RM."
        ;;
      *)
        func_fatal_help "invalid operation mode '$opt_mode'"
        ;;
    esac
    echo
    $ECHO "Try '$progname --help' for more information about other modes."
}
if $opt_help; then
  if test : = "$opt_help"; then
    func_mode_help
  else
    {
      func_help noexit
      for opt_mode in compile link execute install finish uninstall clean; do
	func_mode_help
      done
    } | $SED -n '1p; 2,$s/^Usage:/  or: /p'
    {
      func_help noexit
      for opt_mode in compile link execute install finish uninstall clean; do
	echo
	func_mode_help
      done
    } |
    $SED '1d
      /^When reporting/,/^Report/{
	H
	d
      }
      $x
      /information about other modes/d
      /more detailed .*MODE/d
      s/^Usage:.*--mode=\([^ ]*\) .*/Description of \1 mode:/'
  fi
  exit $?
fi
func_mode_execute ()
{
    $debug_cmd
    cmd=$nonopt
    test -z "$cmd" && \
      func_fatal_help "you must specify a COMMAND"
    for file in $opt_dlopen; do
      test -f "$file" \
	|| func_fatal_help "'$file' is not a file"
      dir=
      case $file in
      *.la)
	func_resolve_sysroot "$file"
	file=$func_resolve_sysroot_result
	func_lalib_unsafe_p "$file" \
	  || func_fatal_help "'$lib' is not a valid libtool archive"
	dlname=
	library_names=
	func_source "$file"
	if test -z "$dlname"; then
	  test -n "$library_names" && \
	    func_warning "'$file' was not linked with '-export-dynamic'"
	  continue
	fi
	func_dirname "$file" "" "."
	dir=$func_dirname_result
	if test -f "$dir/$objdir/$dlname"; then
	  func_append dir "/$objdir"
	else
	  if test ! -f "$dir/$dlname"; then
	    func_fatal_error "cannot find '$dlname' in '$dir' or '$dir/$objdir'"
	  fi
	fi
	;;
      *.lo)
	func_dirname "$file" "" "."
	dir=$func_dirname_result
	;;
      *)
	func_warning "'-dlopen' is ignored for non-libtool libraries and objects"
	continue
	;;
      esac
      absdir=`cd "$dir" && pwd`
      test -n "$absdir" && dir=$absdir
      if eval "test -z \"\$$shlibpath_var\""; then
	eval "$shlibpath_var=\"\$dir\""
      else
	eval "$shlibpath_var=\"\$dir:\$$shlibpath_var\""
      fi
    done
    libtool_execute_magic=$magic
    args=
    for file
    do
      case $file in
      -* | *.la | *.lo ) ;;
      *)
	if func_ltwrapper_script_p "$file"; then
	  func_source "$file"
	  file=$progdir/$program
	elif func_ltwrapper_executable_p "$file"; then
	  func_ltwrapper_scriptname "$file"
	  func_source "$func_ltwrapper_scriptname_result"
	  file=$progdir/$program
	fi
	;;
      esac
      func_append_quoted args "$file"
    done
    if $opt_dry_run; then
      if test -n "$shlibpath_var"; then
	eval "\$ECHO \"\$shlibpath_var=\$$shlibpath_var\""
	echo "export $shlibpath_var"
      fi
      $ECHO "$cmd$args"
      exit $EXIT_SUCCESS
    else
      if test -n "$shlibpath_var"; then
	eval "export $shlibpath_var"
      fi
      for lt_var in LANG LANGUAGE LC_ALL LC_CTYPE LC_COLLATE LC_MESSAGES
      do
	eval "if test \"\${save_$lt_var+set}\" = set; then
                $lt_var=\$save_$lt_var; export $lt_var
	      else
		$lt_unset $lt_var
	      fi"
      done
      exec_cmd=\$cmd$args
    fi
}
test execute = "$opt_mode" && func_mode_execute ${1+"$@"}
func_mode_finish ()
{
    $debug_cmd
    libs=
    libdirs=
    admincmds=
    for opt in "$nonopt" ${1+"$@"}
    do
      if test -d "$opt"; then
	func_append libdirs " $opt"
      elif test -f "$opt"; then
	if func_lalib_unsafe_p "$opt"; then
	  func_append libs " $opt"
	else
	  func_warning "'$opt' is not a valid libtool archive"
	fi
      else
	func_fatal_error "invalid argument '$opt'"
      fi
    done
    if test -n "$libs"; then
      if test -n "$lt_sysroot"; then
        sysroot_regex=`$ECHO "$lt_sysroot" | $SED "$sed_make_literal_regex"`
        sysroot_cmd="s/\([ ']\)$sysroot_regex/\1/g;"
      else
        sysroot_cmd=
      fi
      if $opt_dry_run; then
        for lib in $libs; do
          echo "removing references to $lt_sysroot and '=' prefixes from $lib"
        done
      else
        tmpdir=`func_mktempdir`
        for lib in $libs; do
	  $SED -e "$sysroot_cmd s/\([ ']-[LR]\)=/\1/g; s/\([ ']\)=/\1/g" $lib \
	    > $tmpdir/tmp-la
	  mv -f $tmpdir/tmp-la $lib
	done
        ${RM}r "$tmpdir"
      fi
    fi
    if test -n "$finish_cmds$finish_eval" && test -n "$libdirs"; then
      for libdir in $libdirs; do
	if test -n "$finish_cmds"; then
	  func_execute_cmds "$finish_cmds" 'admincmds="$admincmds
'"$cmd"'"'
	fi
	if test -n "$finish_eval"; then
	  eval cmds=\"$finish_eval\"
	  $opt_dry_run || eval "$cmds" || func_append admincmds "
       $cmds"
	fi
      done
    fi
    $opt_quiet && exit $EXIT_SUCCESS
    if test -n "$finish_cmds$finish_eval" && test -n "$libdirs"; then
      echo "----------------------------------------------------------------------"
      echo "Libraries have been installed in:"
      for libdir in $libdirs; do
	$ECHO "   $libdir"
      done
      echo
      echo "If you ever happen to want to link against installed libraries"
      echo "in a given directory, LIBDIR, you must either use libtool, and"
      echo "specify the full pathname of the library, or use the '-LLIBDIR'"
      echo "flag during linking and do at least one of the following:"
      if test -n "$shlibpath_var"; then
	echo "   - add LIBDIR to the '$shlibpath_var' environment variable"
	echo "     during execution"
      fi
      if test -n "$runpath_var"; then
	echo "   - add LIBDIR to the '$runpath_var' environment variable"
	echo "     during linking"
      fi
      if test -n "$hardcode_libdir_flag_spec"; then
	libdir=LIBDIR
	eval flag=\"$hardcode_libdir_flag_spec\"
	$ECHO "   - use the '$flag' linker flag"
      fi
      if test -n "$admincmds"; then
	$ECHO "   - have your system administrator run these commands:$admincmds"
      fi
      if test -f /etc/ld.so.conf; then
	echo "   - have your system administrator add LIBDIR to '/etc/ld.so.conf'"
      fi
      echo
      echo "See any operating system documentation about shared libraries for"
      case $host in
	solaris2.[6789]|solaris2.1[0-9])
	  echo "more information, such as the ld(1), crle(1) and ld.so(8) manual"
	  echo "pages."
	  ;;
	*)
	  echo "more information, such as the ld(1) and ld.so(8) manual pages."
	  ;;
      esac
      echo "----------------------------------------------------------------------"
    fi
    exit $EXIT_SUCCESS
}
test finish = "$opt_mode" && func_mode_finish ${1+"$@"}
func_mode_install ()
{
    $debug_cmd
    if test "$SHELL" = "$nonopt" || test /bin/sh = "$nonopt" ||
       case $nonopt in *shtool*) :;; *) false;; esac
    then
      func_quote_arg pretty "$nonopt"
      install_prog="$func_quote_arg_result "
      arg=$1
      shift
    else
      install_prog=
      arg=$nonopt
    fi
    func_quote_arg pretty "$arg"
    func_append install_prog "$func_quote_arg_result"
    install_shared_prog=$install_prog
    case " $install_prog " in
      *[\\\ /]cp\ *) install_cp=: ;;
      *) install_cp=false ;;
    esac
    dest=
    files=
    opts=
    prev=
    install_type=
    isdir=false
    stripme=
    no_mode=:
    for arg
    do
      arg2=
      if test -n "$dest"; then
	func_append files " $dest"
	dest=$arg
	continue
      fi
      case $arg in
      -d) isdir=: ;;
      -f)
	if $install_cp; then :; else
	  prev=$arg
	fi
	;;
      -g | -m | -o)
	prev=$arg
	;;
      -s)
	stripme=" -s"
	continue
	;;
      -*)
	;;
      *)
	if test -n "$prev"; then
	  if test X-m = "X$prev" && test -n "$install_override_mode"; then
	    arg2=$install_override_mode
	    no_mode=false
	  fi
	  prev=
	else
	  dest=$arg
	  continue
	fi
	;;
      esac
      func_quote_arg pretty "$arg"
      func_append install_prog " $func_quote_arg_result"
      if test -n "$arg2"; then
	func_quote_arg pretty "$arg2"
      fi
      func_append install_shared_prog " $func_quote_arg_result"
    done
    test -z "$install_prog" && \
      func_fatal_help "you must specify an install program"
    test -n "$prev" && \
      func_fatal_help "the '$prev' option requires an argument"
    if test -n "$install_override_mode" && $no_mode; then
      if $install_cp; then :; else
	func_quote_arg pretty "$install_override_mode"
	func_append install_shared_prog " -m $func_quote_arg_result"
      fi
    fi
    if test -z "$files"; then
      if test -z "$dest"; then
	func_fatal_help "no file or destination specified"
      else
	func_fatal_help "you must specify a destination"
      fi
    fi
    func_stripname '' '/' "$dest"
    dest=$func_stripname_result
    test -d "$dest" && isdir=:
    if $isdir; then
      destdir=$dest
      destname=
    else
      func_dirname_and_basename "$dest" "" "."
      destdir=$func_dirname_result
      destname=$func_basename_result
      set dummy $files; shift
      test "$#" -gt 1 && \
	func_fatal_help "'$dest' is not a directory"
    fi
    case $destdir in
    [\\/]* | [A-Za-z]:[\\/]*) ;;
    *)
      for file in $files; do
	case $file in
	*.lo) ;;
	*)
	  func_fatal_help "'$destdir' must be an absolute directory name"
	  ;;
	esac
      done
      ;;
    esac
    libtool_install_magic=$magic
    staticlibs=
    future_libdirs=
    current_libdirs=
    for file in $files; do
      case $file in
      *.$libext)
	func_append staticlibs " $file"
	;;
      *.la)
	func_resolve_sysroot "$file"
	file=$func_resolve_sysroot_result
	func_lalib_unsafe_p "$file" \
	  || func_fatal_help "'$file' is not a valid libtool archive"
	library_names=
	old_library=
	relink_command=
	func_source "$file"
	if test "X$destdir" = "X$libdir"; then
	  case "$current_libdirs " in
	  *" $libdir "*) ;;
	  *) func_append current_libdirs " $libdir" ;;
	  esac
	else
	  case "$future_libdirs " in
	  *" $libdir "*) ;;
	  *) func_append future_libdirs " $libdir" ;;
	  esac
	fi
	func_dirname "$file" "/" ""
	dir=$func_dirname_result
	func_append dir "$objdir"
	if test -n "$relink_command"; then
	  inst_prefix_dir=`$ECHO "$destdir" | $SED -e "s%$libdir\$%%"`
	  test "$inst_prefix_dir" = "$destdir" && \
	    func_fatal_error "error: cannot install '$file' to a directory not ending in $libdir"
	  if test -n "$inst_prefix_dir"; then
	    relink_command=`$ECHO "$relink_command" | $SED "s%@inst_prefix_dir@%-inst-prefix-dir $inst_prefix_dir%"`
	  else
	    relink_command=`$ECHO "$relink_command" | $SED "s%@inst_prefix_dir@%%"`
	  fi
	  func_warning "relinking '$file'"
	  func_show_eval "$relink_command" \
	    'func_fatal_error "error: relink '\''$file'\'' with the above command before installing it"'
	fi
	set dummy $library_names; shift
	if test -n "$1"; then
	  realname=$1
	  shift
	  srcname=$realname
	  test -n "$relink_command" && srcname=${realname}T
	  func_show_eval "$install_shared_prog $dir/$srcname $destdir/$realname" \
	      'exit $?'
	  tstripme=$stripme
	  case $host_os in
	  cygwin* | mingw* | pw32* | cegcc*)
	    case $realname in
	    *.dll.a)
	      tstripme=
	      ;;
	    esac
	    ;;
	  os2*)
	    case $realname in
	    *_dll.a)
	      tstripme=
	      ;;
	    esac
	    ;;
	  esac
	  if test -n "$tstripme" && test -n "$striplib"; then
	    func_show_eval "$striplib $destdir/$realname" 'exit $?'
	  fi
	  if test "$#" -gt 0; then
	    for linkname
	    do
	      test "$linkname" != "$realname" \
		&& func_show_eval "(cd $destdir && { $LN_S -f $realname $linkname || { $RM $linkname && $LN_S $realname $linkname; }; })"
	    done
	  fi
	  lib=$destdir/$realname
	  func_execute_cmds "$postinstall_cmds" 'exit $?'
	fi
	func_basename "$file"
	name=$func_basename_result
	instname=$dir/${name}i
	func_show_eval "$install_prog $instname $destdir/$name" 'exit $?'
	test -n "$old_library" && func_append staticlibs " $dir/$old_library"
	;;
      *.lo)
	if test -n "$destname"; then
	  destfile=$destdir/$destname
	else
	  func_basename "$file"
	  destfile=$func_basename_result
	  destfile=$destdir/$destfile
	fi
	case $destfile in
	*.lo)
	  func_lo2o "$destfile"
	  staticdest=$func_lo2o_result
	  ;;
	*.$objext)
	  staticdest=$destfile
	  destfile=
	  ;;
	*)
	  func_fatal_help "cannot copy a libtool object to '$destfile'"
	  ;;
	esac
	test -n "$destfile" && \
	  func_show_eval "$install_prog $file $destfile" 'exit $?'
	if test yes = "$build_old_libs"; then
	  func_lo2o "$file"
	  staticobj=$func_lo2o_result
	  func_show_eval "$install_prog \$staticobj \$staticdest" 'exit $?'
	fi
	exit $EXIT_SUCCESS
	;;
      *)
	if test -n "$destname"; then
	  destfile=$destdir/$destname
	else
	  func_basename "$file"
	  destfile=$func_basename_result
	  destfile=$destdir/$destfile
	fi
	stripped_ext=
	case $file in
	  *.exe)
	    if test ! -f "$file"; then
	      func_stripname '' '.exe' "$file"
	      file=$func_stripname_result
	      stripped_ext=.exe
	    fi
	    ;;
	esac
	case $host in
	*cygwin* | *mingw*)
	    if func_ltwrapper_executable_p "$file"; then
	      func_ltwrapper_scriptname "$file"
	      wrapper=$func_ltwrapper_scriptname_result
	    else
	      func_stripname '' '.exe' "$file"
	      wrapper=$func_stripname_result
	    fi
	    ;;
	*)
	    wrapper=$file
	    ;;
	esac
	if func_ltwrapper_script_p "$wrapper"; then
	  notinst_deplibs=
	  relink_command=
	  func_source "$wrapper"
	  test -z "$generated_by_libtool_version" && \
	    func_fatal_error "invalid libtool wrapper script '$wrapper'"
	  finalize=:
	  for lib in $notinst_deplibs; do
	    libdir=
	    if test -f "$lib"; then
	      func_source "$lib"
	    fi
	    libfile=$libdir/`$ECHO "$lib" | $SED 's%^.*/%%g'`
	    if test -n "$libdir" && test ! -f "$libfile"; then
	      func_warning "'$lib' has not been installed in '$libdir'"
	      finalize=false
	    fi
	  done
	  relink_command=
	  func_source "$wrapper"
	  outputname=
	  if test no = "$fast_install" && test -n "$relink_command"; then
	    $opt_dry_run || {
	      if $finalize; then
	        tmpdir=`func_mktempdir`
		func_basename "$file$stripped_ext"
		file=$func_basename_result
	        outputname=$tmpdir/$file
	        relink_command=`$ECHO "$relink_command" | $SED 's%@OUTPUT@%'"$outputname"'%g'`
	        $opt_quiet || {
	          func_quote_arg expand,pretty "$relink_command"
		  eval "func_echo $func_quote_arg_result"
	        }
	        if eval "$relink_command"; then :
	          else
		  func_error "error: relink '$file' with the above command before installing it"
		  $opt_dry_run || ${RM}r "$tmpdir"
		  continue
	        fi
	        file=$outputname
	      else
	        func_warning "cannot relink '$file'"
	      fi
	    }
	  else
	    file=`$ECHO "$file$stripped_ext" | $SED "s%\([^/]*\)$%$objdir/\1%"`
	  fi
	fi
	case $install_prog,$host in
	*/usr/bin/install*,*cygwin*)
	  case $file:$destfile in
	  *.exe:*.exe)
	    ;;
	  *.exe:*)
	    destfile=$destfile.exe
	    ;;
	  *:*.exe)
	    func_stripname '' '.exe' "$destfile"
	    destfile=$func_stripname_result
	    ;;
	  esac
	  ;;
	esac
	func_show_eval "$install_prog\$stripme \$file \$destfile" 'exit $?'
	$opt_dry_run || if test -n "$outputname"; then
	  ${RM}r "$tmpdir"
	fi
	;;
      esac
    done
    for file in $staticlibs; do
      func_basename "$file"
      name=$func_basename_result
      oldlib=$destdir/$name
      func_to_tool_file "$oldlib" func_convert_file_msys_to_w32
      tool_oldlib=$func_to_tool_file_result
      func_show_eval "$install_prog \$file \$oldlib" 'exit $?'
      if test -n "$stripme" && test -n "$old_striplib"; then
	func_show_eval "$old_striplib $tool_oldlib" 'exit $?'
      fi
      func_execute_cmds "$old_postinstall_cmds" 'exit $?'
    done
    test -n "$future_libdirs" && \
      func_warning "remember to run '$progname --finish$future_libdirs'"
    if test -n "$current_libdirs"; then
      $opt_dry_run && current_libdirs=" -n$current_libdirs"
      exec_cmd='$SHELL "$progpath" $preserve_args --finish$current_libdirs'
    else
      exit $EXIT_SUCCESS
    fi
}
test install = "$opt_mode" && func_mode_install ${1+"$@"}
func_generate_dlsyms ()
{
    $debug_cmd
    my_outputname=$1
    my_originator=$2
    my_pic_p=${3-false}
    my_prefix=`$ECHO "$my_originator" | $SED 's%[^a-zA-Z0-9]%_%g'`
    my_dlsyms=
    if test -n "$dlfiles$dlprefiles" || test no != "$dlself"; then
      if test -n "$NM" && test -n "$global_symbol_pipe"; then
	my_dlsyms=${my_outputname}S.c
      else
	func_error "not configured to extract global symbols from dlpreopened files"
      fi
    fi
    if test -n "$my_dlsyms"; then
      case $my_dlsyms in
      "") ;;
      *.c)
	nlist=$output_objdir/$my_outputname.nm
	func_show_eval "$RM $nlist ${nlist}S ${nlist}T"
	func_verbose "creating $output_objdir/$my_dlsyms"
	$opt_dry_run || $ECHO > "$output_objdir/$my_dlsyms" "\
/* $my_dlsyms - symbol resolution table for '$my_outputname' dlsym emulation. */
/* Generated by $PROGRAM (GNU $PACKAGE) $VERSION */
extern \"C\" {
/* Keep this code in sync between libtool.m4, ltmain, lt_system.h, and tests.  */
/* DATA imports from DLLs on WIN32 can't be const, because runtime
   relocations are performed -- see ld's documentation on pseudo-relocs.  */
/* This system does not cope well with relocations in const data.  */
/* External symbol declarations for the compiler. */\
"
	if test yes = "$dlself"; then
	  func_verbose "generating symbol list for '$output'"
	  $opt_dry_run || echo ': @PROGRAM@ ' > "$nlist"
	  progfiles=`$ECHO "$objs$old_deplibs" | $SP2NL | $SED "$lo2o" | $NL2SP`
	  for progfile in $progfiles; do
	    func_to_tool_file "$progfile" func_convert_file_msys_to_w32
	    func_verbose "extracting global C symbols from '$func_to_tool_file_result'"
	    $opt_dry_run || eval "$NM $func_to_tool_file_result | $global_symbol_pipe >> '$nlist'"
	  done
	  if test -n "$exclude_expsyms"; then
	    $opt_dry_run || {
	      eval '$EGREP -v " ($exclude_expsyms)$" "$nlist" > "$nlist"T'
	      eval '$MV "$nlist"T "$nlist"'
	    }
	  fi
	  if test -n "$export_symbols_regex"; then
	    $opt_dry_run || {
	      eval '$EGREP -e "$export_symbols_regex" "$nlist" > "$nlist"T'
	      eval '$MV "$nlist"T "$nlist"'
	    }
	  fi
	  if test -z "$export_symbols"; then
	    export_symbols=$output_objdir/$outputname.exp
	    $opt_dry_run || {
	      $RM $export_symbols
	      eval "$SED -n -e '/^: @PROGRAM@ $/d' -e 's/^.* \(.*\)$/\1/p' "'< "$nlist" > "$export_symbols"'
	      case $host in
	      *cygwin* | *mingw* | *cegcc* )
                eval "echo EXPORTS "'> "$output_objdir/$outputname.def"'
                eval 'cat "$export_symbols" >> "$output_objdir/$outputname.def"'
	        ;;
	      esac
	    }
	  else
	    $opt_dry_run || {
	      eval "$SED -e 's/\([].[*^$]\)/\\\\\1/g' -e 's/^/ /' -e 's/$/$/'"' < "$export_symbols" > "$output_objdir/$outputname.exp"'
	      eval '$GREP -f "$output_objdir/$outputname.exp" < "$nlist" > "$nlist"T'
	      eval '$MV "$nlist"T "$nlist"'
	      case $host in
	        *cygwin* | *mingw* | *cegcc* )
	          eval "echo EXPORTS "'> "$output_objdir/$outputname.def"'
	          eval 'cat "$nlist" >> "$output_objdir/$outputname.def"'
	          ;;
	      esac
	    }
	  fi
	fi
	for dlprefile in $dlprefiles; do
	  func_verbose "extracting global C symbols from '$dlprefile'"
	  func_basename "$dlprefile"
	  name=$func_basename_result
          case $host in
	    *cygwin* | *mingw* | *cegcc* )
	      if func_win32_import_lib_p "$dlprefile"; then
	        func_tr_sh "$dlprefile"
	        eval "curr_lafile=\$libfile_$func_tr_sh_result"
	        dlprefile_dlbasename=
	        if test -n "$curr_lafile" && func_lalib_p "$curr_lafile"; then
	          dlprefile_dlname=`source "$curr_lafile" && echo "$dlname"`
	          if test -n "$dlprefile_dlname"; then
	            func_basename "$dlprefile_dlname"
	            dlprefile_dlbasename=$func_basename_result
	          else
	            $sharedlib_from_linklib_cmd "$dlprefile"
	            dlprefile_dlbasename=$sharedlib_from_linklib_result
	          fi
	        fi
	        $opt_dry_run || {
	          if test -n "$dlprefile_dlbasename"; then
	            eval '$ECHO ": $dlprefile_dlbasename" >> "$nlist"'
	          else
	            func_warning "Could not compute DLL name from $name"
	            eval '$ECHO ": $name " >> "$nlist"'
	          fi
	          func_to_tool_file "$dlprefile" func_convert_file_msys_to_w32
	          eval "$NM \"$func_to_tool_file_result\" 2>/dev/null | $global_symbol_pipe |
	            $SED -e '/I __imp/d' -e 's/I __nm_/D /;s/_nm__//' >> '$nlist'"
	        }
	      else # not an import lib
	        $opt_dry_run || {
	          eval '$ECHO ": $name " >> "$nlist"'
	          func_to_tool_file "$dlprefile" func_convert_file_msys_to_w32
	          eval "$NM \"$func_to_tool_file_result\" 2>/dev/null | $global_symbol_pipe >> '$nlist'"
	        }
	      fi
	    ;;
	    *)
	      $opt_dry_run || {
	        eval '$ECHO ": $name " >> "$nlist"'
	        func_to_tool_file "$dlprefile" func_convert_file_msys_to_w32
	        eval "$NM \"$func_to_tool_file_result\" 2>/dev/null | $global_symbol_pipe >> '$nlist'"
	      }
	    ;;
          esac
	done
	$opt_dry_run || {
	  test -f "$nlist" || : > "$nlist"
	  if test -n "$exclude_expsyms"; then
	    $EGREP -v " ($exclude_expsyms)$" "$nlist" > "$nlist"T
	    $MV "$nlist"T "$nlist"
	  fi
	  if $GREP -v "^: " < "$nlist" |
	      if sort -k 3 </dev/null >/dev/null 2>&1; then
		sort -k 3
	      else
		sort +2
	      fi |
	      uniq > "$nlist"S; then
	    :
	  else
	    $GREP -v "^: " < "$nlist" > "$nlist"S
	  fi
	  if test -f "$nlist"S; then
	    eval "$global_symbol_to_cdecl"' < "$nlist"S >> "$output_objdir/$my_dlsyms"'
	  else
	    echo '/* NONE */' >> "$output_objdir/$my_dlsyms"
	  fi
	  func_show_eval '$RM "${nlist}I"'
	  if test -n "$global_symbol_to_import"; then
	    eval "$global_symbol_to_import"' < "$nlist"S > "$nlist"I'
	  fi
	  echo >> "$output_objdir/$my_dlsyms" "\
/* The mapping between symbol names and symbols.  */
typedef struct {
  const char *name;
  void *address;
} lt_dlsymlist;
extern LT_DLSYM_CONST lt_dlsymlist
lt_${my_prefix}_LTX_preloaded_symbols[];\
"
	  if test -s "$nlist"I; then
	    echo >> "$output_objdir/$my_dlsyms" "\
static void lt_syminit(void)
{
  LT_DLSYM_CONST lt_dlsymlist *symbol = lt_${my_prefix}_LTX_preloaded_symbols;
  for (; symbol->name; ++symbol)
    {"
	    $SED 's/.*/      if (STREQ (symbol->name, \"&\")) symbol->address = (void *) \&&;/' < "$nlist"I >> "$output_objdir/$my_dlsyms"
	    echo >> "$output_objdir/$my_dlsyms" "\
    }
}"
	  fi
	  echo >> "$output_objdir/$my_dlsyms" "\
LT_DLSYM_CONST lt_dlsymlist
lt_${my_prefix}_LTX_preloaded_symbols[] =
{ {\"$my_originator\", (void *) 0},"
	  if test -s "$nlist"I; then
	    echo >> "$output_objdir/$my_dlsyms" "\
  {\"@INIT@\", (void *) &lt_syminit},"
	  fi
	  case $need_lib_prefix in
	  no)
	    eval "$global_symbol_to_c_name_address" < "$nlist" >> "$output_objdir/$my_dlsyms"
	    ;;
	  *)
	    eval "$global_symbol_to_c_name_address_lib_prefix" < "$nlist" >> "$output_objdir/$my_dlsyms"
	    ;;
	  esac
	  echo >> "$output_objdir/$my_dlsyms" "\
  {0, (void *) 0}
};
/* This works around a problem in FreeBSD linker */
static const void *lt_preloaded_setup() {
  return lt_${my_prefix}_LTX_preloaded_symbols;
}
}
"
	} # !$opt_dry_run
	pic_flag_for_symtable=
	case "$compile_command " in
	*" -static "*) ;;
	*)
	  case $host in
	  *-*-freebsd2.*|*-*-freebsd3.0*|*-*-freebsdelf3.0*)
	    pic_flag_for_symtable=" $pic_flag -DFREEBSD_WORKAROUND" ;;
	  *-*-hpux*)
	    pic_flag_for_symtable=" $pic_flag"  ;;
	  *)
	    $my_pic_p && pic_flag_for_symtable=" $pic_flag"
	    ;;
	  esac
	  ;;
	esac
	symtab_cflags=
	for arg in $LTCFLAGS; do
	  case $arg in
	  -pie | -fpie | -fPIE) ;;
	  *) func_append symtab_cflags " $arg" ;;
	  esac
	done
	func_show_eval '(cd $output_objdir && $LTCC$symtab_cflags -c$no_builtin_flag$pic_flag_for_symtable "$my_dlsyms")' 'exit $?'
	func_show_eval '$RM "$output_objdir/$my_dlsyms" "$nlist" "${nlist}S" "${nlist}T" "${nlist}I"'
	symfileobj=$output_objdir/${my_outputname}S.$objext
	case $host in
	*cygwin* | *mingw* | *cegcc* )
	  if test -f "$output_objdir/$my_outputname.def"; then
	    compile_command=`$ECHO "$compile_command" | $SED "s%@SYMFILE@%$output_objdir/$my_outputname.def $symfileobj%"`
	    finalize_command=`$ECHO "$finalize_command" | $SED "s%@SYMFILE@%$output_objdir/$my_outputname.def $symfileobj%"`
	  else
	    compile_command=`$ECHO "$compile_command" | $SED "s%@SYMFILE@%$symfileobj%"`
	    finalize_command=`$ECHO "$finalize_command" | $SED "s%@SYMFILE@%$symfileobj%"`
	  fi
	  ;;
	*)
	  compile_command=`$ECHO "$compile_command" | $SED "s%@SYMFILE@%$symfileobj%"`
	  finalize_command=`$ECHO "$finalize_command" | $SED "s%@SYMFILE@%$symfileobj%"`
	  ;;
	esac
	;;
      *)
	func_fatal_error "unknown suffix for '$my_dlsyms'"
	;;
      esac
    else
      compile_command=`$ECHO "$compile_command" | $SED "s% @SYMFILE@%%"`
      finalize_command=`$ECHO "$finalize_command" | $SED "s% @SYMFILE@%%"`
    fi
}
func_cygming_gnu_implib_p ()
{
  $debug_cmd
  func_to_tool_file "$1" func_convert_file_msys_to_w32
  func_cygming_gnu_implib_tmp=`$NM "$func_to_tool_file_result" | eval "$global_symbol_pipe" | $EGREP ' (_head_[A-Za-z0-9_]+_[ad]l*|[A-Za-z0-9_]+_[ad]l*_iname)$'`
  test -n "$func_cygming_gnu_implib_tmp"
}
func_cygming_ms_implib_p ()
{
  $debug_cmd
  func_to_tool_file "$1" func_convert_file_msys_to_w32
  func_cygming_ms_implib_tmp=`$NM "$func_to_tool_file_result" | eval "$global_symbol_pipe" | $GREP '_NULL_IMPORT_DESCRIPTOR'`
  test -n "$func_cygming_ms_implib_tmp"
}
func_win32_libid ()
{
  $debug_cmd
  win32_libid_type=unknown
  win32_fileres=`file -L $1 2>/dev/null`
  case $win32_fileres in
  *ar\ archive\ import\ library*) # definitely import
    win32_libid_type="x86 archive import"
    ;;
  *ar\ archive*) # could be an import, or static
    if eval $OBJDUMP -f $1 | $SED -e '10q' 2>/dev/null |
       $EGREP 'file format (pei*-i386(.*architecture: i386)?|pe-arm-wince|pe-x86-64)' >/dev/null; then
      case $nm_interface in
      "MS dumpbin")
	if func_cygming_ms_implib_p "$1" ||
	   func_cygming_gnu_implib_p "$1"
	then
	  win32_nmres=import
	else
	  win32_nmres=
	fi
	;;
      *)
	func_to_tool_file "$1" func_convert_file_msys_to_w32
	win32_nmres=`eval $NM -f posix -A \"$func_to_tool_file_result\" |
	  $SED -n -e '
	    1,100{
		/ I /{
		    s|.*|import|
		    p
		    q
		}
	    }'`
	;;
      esac
      case $win32_nmres in
      import*)  win32_libid_type="x86 archive import";;
      *)        win32_libid_type="x86 archive static";;
      esac
    fi
    ;;
  *DLL*)
    win32_libid_type="x86 DLL"
    ;;
  *executable*) # but shell scripts are "executable" too...
    case $win32_fileres in
    *MS\ Windows\ PE\ Intel*)
      win32_libid_type="x86 DLL"
      ;;
    esac
    ;;
  esac
  $ECHO "$win32_libid_type"
}
func_cygming_dll_for_implib ()
{
  $debug_cmd
  sharedlib_from_linklib_result=`$DLLTOOL --identify-strict --identify "$1"`
}
func_cygming_dll_for_implib_fallback_core ()
{
  $debug_cmd
  match_literal=`$ECHO "$1" | $SED "$sed_make_literal_regex"`
  $OBJDUMP -s --section "$1" "$2" 2>/dev/null |
    $SED '/^Contents of section '"$match_literal"':/{
      s/.*/====MARK====/
      p
      d
    }
    /:[	 ]*file format pe[i]\{,1\}-/d
    /^In archive [^:]*:/d
    /^====MARK====/p
    /^.\{43\}/!d
    s/^.\{43\}//' |
    $SED -n '
      /^====MARK====/ b para
      H
      $ b para
      b
      :para
      x
      s/\n//g
      s/^====MARK====//
      s/[\. \t]*$//
      /./p' |
    $SED -e '/^\./d;/^.\./d;q'
}
func_cygming_dll_for_implib_fallback ()
{
  $debug_cmd
  if func_cygming_gnu_implib_p "$1"; then
    sharedlib_from_linklib_result=`func_cygming_dll_for_implib_fallback_core '.idata$7' "$1"`
  elif func_cygming_ms_implib_p "$1"; then
    sharedlib_from_linklib_result=`func_cygming_dll_for_implib_fallback_core '.idata$6' "$1"`
  else
    sharedlib_from_linklib_result=
  fi
}
func_extract_an_archive ()
{
    $debug_cmd
    f_ex_an_ar_dir=$1; shift
    f_ex_an_ar_oldlib=$1
    if test yes = "$lock_old_archive_extraction"; then
      lockfile=$f_ex_an_ar_oldlib.lock
      until $opt_dry_run || ln "$progpath" "$lockfile" 2>/dev/null; do
	func_echo "Waiting for $lockfile to be removed"
	sleep 2
      done
    fi
    func_show_eval "(cd \$f_ex_an_ar_dir && $AR x \"\$f_ex_an_ar_oldlib\")" \
		   'stat=$?; rm -f "$lockfile"; exit $stat'
    if test yes = "$lock_old_archive_extraction"; then
      $opt_dry_run || rm -f "$lockfile"
    fi
    if ($AR t "$f_ex_an_ar_oldlib" | sort | sort -uc >/dev/null 2>&1); then
     :
    else
      func_fatal_error "object name conflicts in archive: $f_ex_an_ar_dir/$f_ex_an_ar_oldlib"
    fi
}
func_extract_archives ()
{
    $debug_cmd
    my_gentop=$1; shift
    my_oldlibs=${1+"$@"}
    my_oldobjs=
    my_xlib=
    my_xabs=
    my_xdir=
    for my_xlib in $my_oldlibs; do
      case $my_xlib in
	[\\/]* | [A-Za-z]:[\\/]*) my_xabs=$my_xlib ;;
	*) my_xabs=`pwd`"/$my_xlib" ;;
      esac
      func_basename "$my_xlib"
      my_xlib=$func_basename_result
      my_xlib_u=$my_xlib
      while :; do
        case " $extracted_archives " in
	*" $my_xlib_u "*)
	  func_arith $extracted_serial + 1
	  extracted_serial=$func_arith_result
	  my_xlib_u=lt$extracted_serial-$my_xlib ;;
	*) break ;;
	esac
      done
      extracted_archives="$extracted_archives $my_xlib_u"
      my_xdir=$my_gentop/$my_xlib_u
      func_mkdir_p "$my_xdir"
      case $host in
      *-darwin*)
	func_verbose "Extracting $my_xabs"
	$opt_dry_run || {
	  darwin_orig_dir=`pwd`
	  cd $my_xdir || exit $?
	  darwin_archive=$my_xabs
	  darwin_curdir=`pwd`
	  func_basename "$darwin_archive"
	  darwin_base_archive=$func_basename_result
	  darwin_arches=`$LIPO -info "$darwin_archive" 2>/dev/null | $GREP Architectures 2>/dev/null || true`
	  if test -n "$darwin_arches"; then
	    darwin_arches=`$ECHO "$darwin_arches" | $SED -e 's/.*are://'`
	    darwin_arch=
	    func_verbose "$darwin_base_archive has multiple architectures $darwin_arches"
	    for darwin_arch in  $darwin_arches; do
	      func_mkdir_p "unfat-$$/$darwin_base_archive-$darwin_arch"
	      $LIPO -thin $darwin_arch -output "unfat-$$/$darwin_base_archive-$darwin_arch/$darwin_base_archive" "$darwin_archive"
	      cd "unfat-$$/$darwin_base_archive-$darwin_arch"
	      func_extract_an_archive "`pwd`" "$darwin_base_archive"
	      cd "$darwin_curdir"
	      $RM "unfat-$$/$darwin_base_archive-$darwin_arch/$darwin_base_archive"
	    done # $darwin_arches
	    darwin_filelist=`find unfat-$$ -type f -name \*.o -print -o -name \*.lo -print | $SED -e "$sed_basename" | sort -u`
	    darwin_file=
	    darwin_files=
	    for darwin_file in $darwin_filelist; do
	      darwin_files=`find unfat-$$ -name $darwin_file -print | sort | $NL2SP`
	      $LIPO -create -output "$darwin_file" $darwin_files
	    done # $darwin_filelist
	    $RM -rf unfat-$$
	    cd "$darwin_orig_dir"
	  else
	    cd $darwin_orig_dir
	    func_extract_an_archive "$my_xdir" "$my_xabs"
	  fi # $darwin_arches
	} # !$opt_dry_run
	;;
      *)
        func_extract_an_archive "$my_xdir" "$my_xabs"
	;;
      esac
      my_oldobjs="$my_oldobjs "`find $my_xdir -name \*.$objext -print -o -name \*.lo -print | sort | $NL2SP`
    done
    func_extract_archives_result=$my_oldobjs
}
func_emit_wrapper ()
{
	func_emit_wrapper_arg1=${1-no}
	$ECHO "\
sed_quote_subst='$sed_quote_subst'
if test -n \"\${ZSH_VERSION+set}\" && (emulate sh) >/dev/null 2>&1; then
  emulate sh
  NULLCMD=:
  alias -g '\${1+\"\$@\"}'='\"\$@\"'
  setopt NO_GLOB_SUBST
else
  case \`(set -o) 2>/dev/null\` in *posix*) set -o posix;; esac
fi
BIN_SH=xpg4; export BIN_SH # for Tru64
DUALCASE=1; export DUALCASE # for MKS sh
(unset CDPATH) >/dev/null 2>&1 && unset CDPATH
relink_command=\"$relink_command\"
if test \"\$libtool_install_magic\" = \"$magic\"; then
  generated_by_libtool_version='$macro_version'
  notinst_deplibs='$notinst_deplibs'
else
  if test \"\$libtool_execute_magic\" != \"$magic\"; then
    file=\"\$0\""
    func_quote_arg pretty "$ECHO"
    qECHO=$func_quote_arg_result
    $ECHO "\
func_fallback_echo ()
{
  eval 'cat <<_LTECHO_EOF
\$1
_LTECHO_EOF'
}
    ECHO=$qECHO
  fi
lt_option_debug=
func_parse_lt_options ()
{
  lt_script_arg0=\$0
  shift
  for lt_opt
  do
    case \"\$lt_opt\" in
    --lt-debug) lt_option_debug=1 ;;
    --lt-dump-script)
        lt_dump_D=\`\$ECHO \"X\$lt_script_arg0\" | $SED -e 's/^X//' -e 's%/[^/]*$%%'\`
        test \"X\$lt_dump_D\" = \"X\$lt_script_arg0\" && lt_dump_D=.
        lt_dump_F=\`\$ECHO \"X\$lt_script_arg0\" | $SED -e 's/^X//' -e 's%^.*/%%'\`
        cat \"\$lt_dump_D/\$lt_dump_F\"
        exit 0
      ;;
    --lt-*)
        \$ECHO \"Unrecognized --lt- option: '\$lt_opt'\" 1>&2
        exit 1
      ;;
    esac
  done
  if test -n \"\$lt_option_debug\"; then
    echo \"$outputname:$output:\$LINENO: libtool wrapper (GNU $PACKAGE) $VERSION\" 1>&2
  fi
}
func_lt_dump_args ()
{
  lt_dump_args_N=1;
  for lt_arg
  do
    \$ECHO \"$outputname:$output:\$LINENO: newargv[\$lt_dump_args_N]: \$lt_arg\"
    lt_dump_args_N=\`expr \$lt_dump_args_N + 1\`
  done
}
func_exec_program_core ()
{
"
  case $host in
  *-*-mingw | *-*-os2* | *-cegcc*)
    $ECHO "\
      if test -n \"\$lt_option_debug\"; then
        \$ECHO \"$outputname:$output:\$LINENO: newargv[0]: \$progdir\\\\\$program\" 1>&2
        func_lt_dump_args \${1+\"\$@\"} 1>&2
      fi
      exec \"\$progdir\\\\\$program\" \${1+\"\$@\"}
"
    ;;
  *)
    $ECHO "\
      if test -n \"\$lt_option_debug\"; then
        \$ECHO \"$outputname:$output:\$LINENO: newargv[0]: \$progdir/\$program\" 1>&2
        func_lt_dump_args \${1+\"\$@\"} 1>&2
      fi
      exec \"\$progdir/\$program\" \${1+\"\$@\"}
"
    ;;
  esac
  $ECHO "\
      \$ECHO \"\$0: cannot exec \$program \$*\" 1>&2
      exit 1
}
func_exec_program ()
{
  case \" \$* \" in
  *\\ --lt-*)
    for lt_wr_arg
    do
      case \$lt_wr_arg in
      --lt-*) ;;
      *) set x \"\$@\" \"\$lt_wr_arg\"; shift;;
      esac
      shift
    done ;;
  esac
  func_exec_program_core \${1+\"\$@\"}
}
  func_parse_lt_options \"\$0\" \${1+\"\$@\"}
  thisdir=\`\$ECHO \"\$file\" | $SED 's%/[^/]*$%%'\`
  test \"x\$thisdir\" = \"x\$file\" && thisdir=.
  file=\`ls -ld \"\$file\" | $SED -n 's/.*-> //p'\`
  while test -n \"\$file\"; do
    destdir=\`\$ECHO \"\$file\" | $SED 's%/[^/]*\$%%'\`
    if test \"x\$destdir\" != \"x\$file\"; then
      case \"\$destdir\" in
      [\\\\/]* | [A-Za-z]:[\\\\/]*) thisdir=\"\$destdir\" ;;
      *) thisdir=\"\$thisdir/\$destdir\" ;;
      esac
    fi
    file=\`\$ECHO \"\$file\" | $SED 's%^.*/%%'\`
    file=\`ls -ld \"\$thisdir/\$file\" | $SED -n 's/.*-> //p'\`
  done
  WRAPPER_SCRIPT_BELONGS_IN_OBJDIR=$func_emit_wrapper_arg1
  if test \"\$WRAPPER_SCRIPT_BELONGS_IN_OBJDIR\" = \"yes\"; then
    if test \"\$thisdir\" = \".\"; then
      thisdir=\`pwd\`
    fi
    case \"\$thisdir\" in
    *[\\\\/]$objdir ) thisdir=\`\$ECHO \"\$thisdir\" | $SED 's%[\\\\/][^\\\\/]*$%%'\` ;;
    $objdir )   thisdir=. ;;
    esac
  fi
  absdir=\`cd \"\$thisdir\" && pwd\`
  test -n \"\$absdir\" && thisdir=\"\$absdir\"
"
	if test yes = "$fast_install"; then
	  $ECHO "\
  program=lt-'$outputname'$exeext
  progdir=\"\$thisdir/$objdir\"
  if test ! -f \"\$progdir/\$program\" ||
     { file=\`ls -1dt \"\$progdir/\$program\" \"\$progdir/../\$program\" 2>/dev/null | $SED 1q\`; \\
       test \"X\$file\" != \"X\$progdir/\$program\"; }; then
    file=\"\$\$-\$program\"
    if test ! -d \"\$progdir\"; then
      $MKDIR \"\$progdir\"
    else
      $RM \"\$progdir/\$file\"
    fi"
	  $ECHO "\
    if test -n \"\$relink_command\"; then
      if relink_command_output=\`eval \$relink_command 2>&1\`; then :
      else
	\$ECHO \"\$relink_command_output\" >&2
	$RM \"\$progdir/\$file\"
	exit 1
      fi
    fi
    $MV \"\$progdir/\$file\" \"\$progdir/\$program\" 2>/dev/null ||
    { $RM \"\$progdir/\$program\";
      $MV \"\$progdir/\$file\" \"\$progdir/\$program\"; }
    $RM \"\$progdir/\$file\"
  fi"
	else
	  $ECHO "\
  program='$outputname'
  progdir=\"\$thisdir/$objdir\"
"
	fi
	$ECHO "\
  if test -f \"\$progdir/\$program\"; then"
	if test -n "$dllsearchpath"; then
	  $ECHO "\
    PATH=$dllsearchpath:\$PATH
"
	fi
	if test yes = "$shlibpath_overrides_runpath" && test -n "$shlibpath_var" && test -n "$temp_rpath"; then
	  $ECHO "\
    $shlibpath_var=\"$temp_rpath\$$shlibpath_var\"
    $shlibpath_var=\`\$ECHO \"\$$shlibpath_var\" | $SED 's/::*\$//'\`
    export $shlibpath_var
"
	fi
	$ECHO "\
    if test \"\$libtool_execute_magic\" != \"$magic\"; then
      func_exec_program \${1+\"\$@\"}
    fi
  else
    \$ECHO \"\$0: error: '\$progdir/\$program' does not exist\" 1>&2
    \$ECHO \"This script is just a wrapper for \$program.\" 1>&2
    \$ECHO \"See the $PACKAGE documentation for more information.\" 1>&2
    exit 1
  fi
fi\
"
}
func_emit_cwrapperexe_src ()
{
	cat <<EOF
/* $cwrappersource - temporary wrapper executable for $objdir/$outputname
   Generated by $PROGRAM (GNU $PACKAGE) $VERSION
   The $output program cannot be directly executed until all the libtool
   libraries that it depends on are installed.
   This wrapper executable should never be moved out of the build directory.
   If it is, it will not operate correctly.
*/
EOF
	    cat <<"EOF"
/* declarations of non-ANSI functions */
int _putenv (const char *);
char *realpath (const char *, char *);
int putenv (char *);
int setenv (const char *, const char *, int);
/* #elif defined other_platform || defined ... */
/* portability defines, excluding path handling macros */
/* #elif defined other platforms ... */
/* path handling portability macros */
  defined __OS2__
	(((ch) == DIR_SEPARATOR) || ((ch) == DIR_SEPARATOR_2))
  if (stale) { free (stale); stale = 0; } \
} while (0)
static int lt_debug = 1;
static int lt_debug = 0;
const char *program_name = "libtool-wrapper"; /* in case xstrdup fails */
void *xmalloc (size_t num);
char *xstrdup (const char *string);
const char *base_name (const char *name);
char *find_executable (const char *wrapper);
char *chase_symlinks (const char *pathspec);
int make_executable (const char *path);
int check_executable (const char *path);
char *strendzap (char *str, const char *pat);
void lt_debugprintf (const char *file, int line, const char *fmt, ...);
void lt_fatal (const char *file, int line, const char *message, ...);
static const char *nonnull (const char *s);
static const char *nonempty (const char *s);
void lt_setenv (const char *name, const char *value);
char *lt_extend_str (const char *orig_value, const char *add, int to_end);
void lt_update_exe_path (const char *name, const char *value);
void lt_update_lib_path (const char *name, const char *value);
char **prepare_spawn (char **argv);
void lt_dump_script (FILE *f);
EOF
	    cat <<EOF
externally_visible const char * MAGIC_EXE = "$magic_exe";
const char * LIB_PATH_VARNAME = "$shlibpath_var";
EOF
	    if test yes = "$shlibpath_overrides_runpath" && test -n "$shlibpath_var" && test -n "$temp_rpath"; then
              func_to_host_path "$temp_rpath"
	      cat <<EOF
const char * LIB_PATH_VALUE   = "$func_to_host_path_result";
EOF
	    else
	      cat <<"EOF"
const char * LIB_PATH_VALUE   = "";
EOF
	    fi
	    if test -n "$dllsearchpath"; then
              func_to_host_path "$dllsearchpath:"
	      cat <<EOF
const char * EXE_PATH_VARNAME = "PATH";
const char * EXE_PATH_VALUE   = "$func_to_host_path_result";
EOF
	    else
	      cat <<"EOF"
const char * EXE_PATH_VARNAME = "";
const char * EXE_PATH_VALUE   = "";
EOF
	    fi
	    if test yes = "$fast_install"; then
	      cat <<EOF
const char * TARGET_PROGRAM_NAME = "lt-$outputname"; /* hopefully, no .exe */
EOF
	    else
	      cat <<EOF
const char * TARGET_PROGRAM_NAME = "$outputname"; /* hopefully, no .exe */
EOF
	    fi
	    cat <<"EOF"
static const char *ltwrapper_option_prefix = LTWRAPPER_OPTION_PREFIX;
static const char *dumpscript_opt       = LTWRAPPER_OPTION_PREFIX "dump-script";
static const char *debug_opt            = LTWRAPPER_OPTION_PREFIX "debug";
int
main (int argc, char *argv[])
{
  char **newargz;
  int  newargc;
  char *tmp_pathspec;
  char *actual_cwrapper_path;
  char *actual_cwrapper_name;
  char *target_name;
  char *lt_argv_zero;
  int rval = 127;
  int i;
  program_name = (char *) xstrdup (base_name (argv[0]));
  newargz = XMALLOC (char *, (size_t) argc + 1);
  /* very simple arg parsing; don't want to rely on getopt
   * also, copy all non cwrapper options to newargz, except
   * argz[0], which is handled differently
   */
  newargc=0;
  for (i = 1; i < argc; i++)
    {
      if (STREQ (argv[i], dumpscript_opt))
	{
EOF
	    case $host in
	      *mingw* | *cygwin* )
		echo "          setmode(1,_O_BINARY);"
		;;
	      esac
	    cat <<"EOF"
	  lt_dump_script (stdout);
	  return 0;
	}
      if (STREQ (argv[i], debug_opt))
	{
          lt_debug = 1;
          continue;
	}
      if (STREQ (argv[i], ltwrapper_option_prefix))
        {
          /* however, if there is an option in the LTWRAPPER_OPTION_PREFIX
             namespace, but it is not one of the ones we know about and
             have already dealt with, above (inluding dump-script), then
             report an error. Otherwise, targets might begin to believe
             they are allowed to use options in the LTWRAPPER_OPTION_PREFIX
             namespace. The first time any user complains about this, we'll
             need to make LTWRAPPER_OPTION_PREFIX a configure-time option
             or a configure.ac-settable value.
           */
          lt_fatal (__FILE__, __LINE__,
		    "unrecognized %s option: '%s'",
                    ltwrapper_option_prefix, argv[i]);
        }
      /* otherwise ... */
      newargz[++newargc] = xstrdup (argv[i]);
    }
  newargz[++newargc] = NULL;
EOF
	    cat <<EOF
  /* The GNU banner must be the first non-error debug message */
  lt_debugprintf (__FILE__, __LINE__, "libtool wrapper (GNU $PACKAGE) $VERSION\n");
EOF
	    cat <<"EOF"
  lt_debugprintf (__FILE__, __LINE__, "(main) argv[0]: %s\n", argv[0]);
  lt_debugprintf (__FILE__, __LINE__, "(main) program_name: %s\n", program_name);
  tmp_pathspec = find_executable (argv[0]);
  if (tmp_pathspec == NULL)
    lt_fatal (__FILE__, __LINE__, "couldn't find %s", argv[0]);
  lt_debugprintf (__FILE__, __LINE__,
                  "(main) found exe (before symlink chase) at: %s\n",
		  tmp_pathspec);
  actual_cwrapper_path = chase_symlinks (tmp_pathspec);
  lt_debugprintf (__FILE__, __LINE__,
                  "(main) found exe (after symlink chase) at: %s\n",
		  actual_cwrapper_path);
  XFREE (tmp_pathspec);
  actual_cwrapper_name = xstrdup (base_name (actual_cwrapper_path));
  strendzap (actual_cwrapper_path, actual_cwrapper_name);
  /* wrapper name transforms */
  strendzap (actual_cwrapper_name, ".exe");
  tmp_pathspec = lt_extend_str (actual_cwrapper_name, ".exe", 1);
  XFREE (actual_cwrapper_name);
  actual_cwrapper_name = tmp_pathspec;
  tmp_pathspec = 0;
  /* target_name transforms -- use actual target program name; might have lt- prefix */
  target_name = xstrdup (base_name (TARGET_PROGRAM_NAME));
  strendzap (target_name, ".exe");
  tmp_pathspec = lt_extend_str (target_name, ".exe", 1);
  XFREE (target_name);
  target_name = tmp_pathspec;
  tmp_pathspec = 0;
  lt_debugprintf (__FILE__, __LINE__,
		  "(main) libtool target name: %s\n",
		  target_name);
EOF
	    cat <<EOF
  newargz[0] =
    XMALLOC (char, (strlen (actual_cwrapper_path) +
		    strlen ("$objdir") + 1 + strlen (actual_cwrapper_name) + 1));
  strcpy (newargz[0], actual_cwrapper_path);
  strcat (newargz[0], "$objdir");
  strcat (newargz[0], "/");
EOF
	    cat <<"EOF"
  /* stop here, and copy so we don't have to do this twice */
  tmp_pathspec = xstrdup (newargz[0]);
  /* do NOT want the lt- prefix here, so use actual_cwrapper_name */
  strcat (newargz[0], actual_cwrapper_name);
  /* DO want the lt- prefix here if it exists, so use target_name */
  lt_argv_zero = lt_extend_str (tmp_pathspec, target_name, 1);
  XFREE (tmp_pathspec);
  tmp_pathspec = NULL;
EOF
	    case $host_os in
	      mingw*)
	    cat <<"EOF"
  {
    char* p;
    while ((p = strchr (newargz[0], '\\')) != NULL)
      {
	*p = '/';
      }
    while ((p = strchr (lt_argv_zero, '\\')) != NULL)
      {
	*p = '/';
      }
  }
EOF
	    ;;
	    esac
	    cat <<"EOF"
  XFREE (target_name);
  XFREE (actual_cwrapper_path);
  XFREE (actual_cwrapper_name);
  lt_setenv ("BIN_SH", "xpg4"); /* for Tru64 */
  lt_setenv ("DUALCASE", "1");  /* for MSK sh */
  /* Update the DLL searchpath.  EXE_PATH_VALUE ($dllsearchpath) must
     be prepended before (that is, appear after) LIB_PATH_VALUE ($temp_rpath)
     because on Windows, both *_VARNAMEs are PATH but uninstalled
     libraries must come first. */
  lt_update_exe_path (EXE_PATH_VARNAME, EXE_PATH_VALUE);
  lt_update_lib_path (LIB_PATH_VARNAME, LIB_PATH_VALUE);
  lt_debugprintf (__FILE__, __LINE__, "(main) lt_argv_zero: %s\n",
		  nonnull (lt_argv_zero));
  for (i = 0; i < newargc; i++)
    {
      lt_debugprintf (__FILE__, __LINE__, "(main) newargz[%d]: %s\n",
		      i, nonnull (newargz[i]));
    }
EOF
	    case $host_os in
	      mingw*)
		cat <<"EOF"
  /* execv doesn't actually work on mingw as expected on unix */
  newargz = prepare_spawn (newargz);
  rval = (int) _spawnv (_P_WAIT, lt_argv_zero, (const char * const *) newargz);
  if (rval == -1)
    {
      /* failed to start process */
      lt_debugprintf (__FILE__, __LINE__,
		      "(main) failed to launch target \"%s\": %s\n",
		      lt_argv_zero, nonnull (strerror (errno)));
      return 127;
    }
  return rval;
EOF
		;;
	      *)
		cat <<"EOF"
  execv (lt_argv_zero, newargz);
  return rval; /* =127, but avoids unused variable warning */
EOF
		;;
	    esac
	    cat <<"EOF"
}
void *
xmalloc (size_t num)
{
  void *p = (void *) malloc (num);
  if (!p)
    lt_fatal (__FILE__, __LINE__, "memory exhausted");
  return p;
}
char *
xstrdup (const char *string)
{
  return string ? strcpy ((char *) xmalloc (strlen (string) + 1),
			  string) : NULL;
}
const char *
base_name (const char *name)
{
  const char *base;
  /* Skip over the disk name in MSDOS pathnames. */
  if (isalpha ((unsigned char) name[0]) && name[1] == ':')
    name += 2;
  for (base = name; *name; name++)
    if (IS_DIR_SEPARATOR (*name))
      base = name + 1;
  return base;
}
int
check_executable (const char *path)
{
  struct stat st;
  lt_debugprintf (__FILE__, __LINE__, "(check_executable): %s\n",
                  nonempty (path));
  if ((!path) || (!*path))
    return 0;
  if ((stat (path, &st) >= 0)
      && (st.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH)))
    return 1;
  else
    return 0;
}
int
make_executable (const char *path)
{
  int rval = 0;
  struct stat st;
  lt_debugprintf (__FILE__, __LINE__, "(make_executable): %s\n",
                  nonempty (path));
  if ((!path) || (!*path))
    return 0;
  if (stat (path, &st) >= 0)
    {
      rval = chmod (path, st.st_mode | S_IXOTH | S_IXGRP | S_IXUSR);
    }
  return rval;
}
/* Searches for the full path of the wrapper.  Returns
   newly allocated full path name if found, NULL otherwise
   Does not chase symlinks, even on platforms that support them.
*/
char *
find_executable (const char *wrapper)
{
  int has_slash = 0;
  const char *p;
  const char *p_next;
  /* static buffer for getcwd */
  char tmp[LT_PATHMAX + 1];
  size_t tmp_len;
  char *concat_name;
  lt_debugprintf (__FILE__, __LINE__, "(find_executable): %s\n",
                  nonempty (wrapper));
  if ((wrapper == NULL) || (*wrapper == '\0'))
    return NULL;
  /* Absolute path? */
  if (isalpha ((unsigned char) wrapper[0]) && wrapper[1] == ':')
    {
      concat_name = xstrdup (wrapper);
      if (check_executable (concat_name))
	return concat_name;
      XFREE (concat_name);
    }
  else
    {
      if (IS_DIR_SEPARATOR (wrapper[0]))
	{
	  concat_name = xstrdup (wrapper);
	  if (check_executable (concat_name))
	    return concat_name;
	  XFREE (concat_name);
	}
    }
  for (p = wrapper; *p; p++)
    if (*p == '/')
      {
	has_slash = 1;
	break;
      }
  if (!has_slash)
    {
      /* no slashes; search PATH */
      const char *path = getenv ("PATH");
      if (path != NULL)
	{
	  for (p = path; *p; p = p_next)
	    {
	      const char *q;
	      size_t p_len;
	      for (q = p; *q; q++)
		if (IS_PATH_SEPARATOR (*q))
		  break;
	      p_len = (size_t) (q - p);
	      p_next = (*q == '\0' ? q : q + 1);
	      if (p_len == 0)
		{
		  /* empty path: current directory */
		  if (getcwd (tmp, LT_PATHMAX) == NULL)
		    lt_fatal (__FILE__, __LINE__, "getcwd failed: %s",
                              nonnull (strerror (errno)));
		  tmp_len = strlen (tmp);
		  concat_name =
		    XMALLOC (char, tmp_len + 1 + strlen (wrapper) + 1);
		  memcpy (concat_name, tmp, tmp_len);
		  concat_name[tmp_len] = '/';
		  strcpy (concat_name + tmp_len + 1, wrapper);
		}
	      else
		{
		  concat_name =
		    XMALLOC (char, p_len + 1 + strlen (wrapper) + 1);
		  memcpy (concat_name, p, p_len);
		  concat_name[p_len] = '/';
		  strcpy (concat_name + p_len + 1, wrapper);
		}
	      if (check_executable (concat_name))
		return concat_name;
	      XFREE (concat_name);
	    }
	}
      /* not found in PATH; assume curdir */
    }
  /* Relative path | not found in path: prepend cwd */
  if (getcwd (tmp, LT_PATHMAX) == NULL)
    lt_fatal (__FILE__, __LINE__, "getcwd failed: %s",
              nonnull (strerror (errno)));
  tmp_len = strlen (tmp);
  concat_name = XMALLOC (char, tmp_len + 1 + strlen (wrapper) + 1);
  memcpy (concat_name, tmp, tmp_len);
  concat_name[tmp_len] = '/';
  strcpy (concat_name + tmp_len + 1, wrapper);
  if (check_executable (concat_name))
    return concat_name;
  XFREE (concat_name);
  return NULL;
}
char *
chase_symlinks (const char *pathspec)
{
  return xstrdup (pathspec);
  char buf[LT_PATHMAX];
  struct stat s;
  char *tmp_pathspec = xstrdup (pathspec);
  char *p;
  int has_symlinks = 0;
  while (strlen (tmp_pathspec) && !has_symlinks)
    {
      lt_debugprintf (__FILE__, __LINE__,
		      "checking path component for symlinks: %s\n",
		      tmp_pathspec);
      if (lstat (tmp_pathspec, &s) == 0)
	{
	  if (S_ISLNK (s.st_mode) != 0)
	    {
	      has_symlinks = 1;
	      break;
	    }
	  /* search backwards for last DIR_SEPARATOR */
	  p = tmp_pathspec + strlen (tmp_pathspec) - 1;
	  while ((p > tmp_pathspec) && (!IS_DIR_SEPARATOR (*p)))
	    p--;
	  if ((p == tmp_pathspec) && (!IS_DIR_SEPARATOR (*p)))
	    {
	      /* no more DIR_SEPARATORS left */
	      break;
	    }
	  *p = '\0';
	}
      else
	{
	  lt_fatal (__FILE__, __LINE__,
		    "error accessing file \"%s\": %s",
		    tmp_pathspec, nonnull (strerror (errno)));
	}
    }
  XFREE (tmp_pathspec);
  if (!has_symlinks)
    {
      return xstrdup (pathspec);
    }
  tmp_pathspec = realpath (pathspec, buf);
  if (tmp_pathspec == 0)
    {
      lt_fatal (__FILE__, __LINE__,
		"could not follow symlinks for %s", pathspec);
    }
  return xstrdup (tmp_pathspec);
}
char *
strendzap (char *str, const char *pat)
{
  size_t len, patlen;
  assert (str != NULL);
  assert (pat != NULL);
  len = strlen (str);
  patlen = strlen (pat);
  if (patlen <= len)
    {
      str += len - patlen;
      if (STREQ (str, pat))
	*str = '\0';
    }
  return str;
}
void
lt_debugprintf (const char *file, int line, const char *fmt, ...)
{
  va_list args;
  if (lt_debug)
    {
      (void) fprintf (stderr, "%s:%s:%d: ", program_name, file, line);
      va_start (args, fmt);
      (void) vfprintf (stderr, fmt, args);
      va_end (args);
    }
}
static void
lt_error_core (int exit_status, const char *file,
	       int line, const char *mode,
	       const char *message, va_list ap)
{
  fprintf (stderr, "%s:%s:%d: %s: ", program_name, file, line, mode);
  vfprintf (stderr, message, ap);
  fprintf (stderr, ".\n");
  if (exit_status >= 0)
    exit (exit_status);
}
void
lt_fatal (const char *file, int line, const char *message, ...)
{
  va_list ap;
  va_start (ap, message);
  lt_error_core (EXIT_FAILURE, file, line, "FATAL", message, ap);
  va_end (ap);
}
static const char *
nonnull (const char *s)
{
  return s ? s : "(null)";
}
static const char *
nonempty (const char *s)
{
  return (s && !*s) ? "(empty)" : nonnull (s);
}
void
lt_setenv (const char *name, const char *value)
{
  lt_debugprintf (__FILE__, __LINE__,
		  "(lt_setenv) setting '%s' to '%s'\n",
                  nonnull (name), nonnull (value));
  {
    /* always make a copy, for consistency with !HAVE_SETENV */
    char *str = xstrdup (value);
    setenv (name, str, 1);
    size_t len = strlen (name) + 1 + strlen (value) + 1;
    char *str = XMALLOC (char, len);
    sprintf (str, "%s=%s", name, value);
    if (putenv (str) != EXIT_SUCCESS)
      {
        XFREE (str);
      }
  }
}
char *
lt_extend_str (const char *orig_value, const char *add, int to_end)
{
  char *new_value;
  if (orig_value && *orig_value)
    {
      size_t orig_value_len = strlen (orig_value);
      size_t add_len = strlen (add);
      new_value = XMALLOC (char, add_len + orig_value_len + 1);
      if (to_end)
        {
          strcpy (new_value, orig_value);
          strcpy (new_value + orig_value_len, add);
        }
      else
        {
          strcpy (new_value, add);
          strcpy (new_value + add_len, orig_value);
        }
    }
  else
    {
      new_value = xstrdup (add);
    }
  return new_value;
}
void
lt_update_exe_path (const char *name, const char *value)
{
  lt_debugprintf (__FILE__, __LINE__,
		  "(lt_update_exe_path) modifying '%s' by prepending '%s'\n",
                  nonnull (name), nonnull (value));
  if (name && *name && value && *value)
    {
      char *new_value = lt_extend_str (getenv (name), value, 0);
      /* some systems can't cope with a ':'-terminated path #' */
      size_t len = strlen (new_value);
      while ((len > 0) && IS_PATH_SEPARATOR (new_value[len-1]))
        {
          new_value[--len] = '\0';
        }
      lt_setenv (name, new_value);
      XFREE (new_value);
    }
}
void
lt_update_lib_path (const char *name, const char *value)
{
  lt_debugprintf (__FILE__, __LINE__,
		  "(lt_update_lib_path) modifying '%s' by prepending '%s'\n",
                  nonnull (name), nonnull (value));
  if (name && *name && value && *value)
    {
      char *new_value = lt_extend_str (getenv (name), value, 0);
      lt_setenv (name, new_value);
      XFREE (new_value);
    }
}
EOF
	    case $host_os in
	      mingw*)
		cat <<"EOF"
/* Prepares an argument vector before calling spawn().
   Note that spawn() does not by itself call the command interpreter
     (getenv ("COMSPEC") != NULL ? getenv ("COMSPEC") :
      ({ OSVERSIONINFO v; v.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
         GetVersionEx(&v);
         v.dwPlatformId == VER_PLATFORM_WIN32_NT;
      }) ? "cmd.exe" : "command.com").
   Instead it simply concatenates the arguments, separated by ' ', and calls
   CreateProcess().  We must quote the arguments since Win32 CreateProcess()
   interprets characters like ' ', '\t', '\\', '"' (but not '<' and '>') in a
   special way:
   - Space and tab are interpreted as delimiters. They are not treated as
     delimiters if they are surrounded by double quotes: "...".
   - Unescaped double quotes are removed from the input. Their only effect is
     that within double quotes, space and tab are treated like normal
     characters.
   - Backslashes not followed by double quotes are not special.
   - But 2*n+1 backslashes followed by a double quote become
     n backslashes followed by a double quote (n >= 0):
       \" -> "
       \\\" -> \"
       \\\\\" -> \\"
 */
char **
prepare_spawn (char **argv)
{
  size_t argc;
  char **new_argv;
  size_t i;
  /* Count number of arguments.  */
  for (argc = 0; argv[argc] != NULL; argc++)
    ;
  /* Allocate new argument vector.  */
  new_argv = XMALLOC (char *, argc + 1);
  /* Put quoted arguments into the new argument vector.  */
  for (i = 0; i < argc; i++)
    {
      const char *string = argv[i];
      if (string[0] == '\0')
	new_argv[i] = xstrdup ("\"\"");
      else if (strpbrk (string, SHELL_SPECIAL_CHARS) != NULL)
	{
	  int quote_around = (strpbrk (string, SHELL_SPACE_CHARS) != NULL);
	  size_t length;
	  unsigned int backslashes;
	  const char *s;
	  char *quoted_string;
	  char *p;
	  length = 0;
	  backslashes = 0;
	  if (quote_around)
	    length++;
	  for (s = string; *s != '\0'; s++)
	    {
	      char c = *s;
	      if (c == '"')
		length += backslashes + 1;
	      length++;
	      if (c == '\\')
		backslashes++;
	      else
		backslashes = 0;
	    }
	  if (quote_around)
	    length += backslashes + 1;
	  quoted_string = XMALLOC (char, length + 1);
	  p = quoted_string;
	  backslashes = 0;
	  if (quote_around)
	    *p++ = '"';
	  for (s = string; *s != '\0'; s++)
	    {
	      char c = *s;
	      if (c == '"')
		{
		  unsigned int j;
		  for (j = backslashes + 1; j > 0; j--)
		    *p++ = '\\';
		}
	      *p++ = c;
	      if (c == '\\')
		backslashes++;
	      else
		backslashes = 0;
	    }
	  if (quote_around)
	    {
	      unsigned int j;
	      for (j = backslashes; j > 0; j--)
		*p++ = '\\';
	      *p++ = '"';
	    }
	  *p = '\0';
	  new_argv[i] = quoted_string;
	}
      else
	new_argv[i] = (char *) string;
    }
  new_argv[argc] = NULL;
  return new_argv;
}
EOF
		;;
	    esac
            cat <<"EOF"
void lt_dump_script (FILE* f)
{
EOF
	    func_emit_wrapper yes |
	      $SED -n -e '
s/^\(.\{79\}\)\(..*\)/\1\
\2/
h
s/\([\\"]\)/\\\1/g
s/$/\\n/
s/\([^\n]*\).*/  fputs ("\1", f);/p
g
D'
            cat <<"EOF"
}
EOF
}
func_win32_import_lib_p ()
{
    $debug_cmd
    case `eval $file_magic_cmd \"\$1\" 2>/dev/null | $SED -e 10q` in
    *import*) : ;;
    *) false ;;
    esac
}
func_suncc_cstd_abi ()
{
    $debug_cmd
    case " $compile_command " in
    *" -compat=g "*|*\ -std=c++[0-9][0-9]\ *|*" -library=stdcxx4 "*|*" -library=stlport4 "*)
      suncc_use_cstd_abi=no
      ;;
    *)
      suncc_use_cstd_abi=yes
      ;;
    esac
}
func_mode_link ()
{
    $debug_cmd
    case $host in
    *-*-cygwin* | *-*-mingw* | *-*-pw32* | *-*-os2* | *-cegcc*)
      allow_undefined=yes
      ;;
    *)
      allow_undefined=yes
      ;;
    esac
    libtool_args=$nonopt
    base_compile="$nonopt $@"
    compile_command=$nonopt
    finalize_command=$nonopt
    compile_rpath=
    finalize_rpath=
    compile_shlibpath=
    finalize_shlibpath=
    convenience=
    old_convenience=
    deplibs=
    old_deplibs=
    compiler_flags=
    linker_flags=
    dllsearchpath=
    lib_search_path=`pwd`
    inst_prefix_dir=
    new_inherited_linker_flags=
    avoid_version=no
    bindir=
    dlfiles=
    dlprefiles=
    dlself=no
    export_dynamic=no
    export_symbols=
    export_symbols_regex=
    generated=
    libobjs=
    ltlibs=
    module=no
    no_install=no
    objs=
    os2dllname=
    non_pic_objects=
    precious_files_regex=
    prefer_static_libs=no
    preload=false
    prev=
    prevarg=
    release=
    rpath=
    xrpath=
    perm_rpath=
    temp_rpath=
    thread_safe=no
    vinfo=
    vinfo_number=no
    weak_libs=
    single_module=$wl-single_module
    func_infer_tag $base_compile
    for arg
    do
      case $arg in
      -shared)
	test yes != "$build_libtool_libs" \
	  && func_fatal_configuration "cannot build a shared library"
	build_old_libs=no
	break
	;;
      -all-static | -static | -static-libtool-libs)
	case $arg in
	-all-static)
	  if test yes = "$build_libtool_libs" && test -z "$link_static_flag"; then
	    func_warning "complete static linking is impossible in this configuration"
	  fi
	  if test -n "$link_static_flag"; then
	    dlopen_self=$dlopen_self_static
	  fi
	  prefer_static_libs=yes
	  ;;
	-static)
	  if test -z "$pic_flag" && test -n "$link_static_flag"; then
	    dlopen_self=$dlopen_self_static
	  fi
	  prefer_static_libs=built
	  ;;
	-static-libtool-libs)
	  if test -z "$pic_flag" && test -n "$link_static_flag"; then
	    dlopen_self=$dlopen_self_static
	  fi
	  prefer_static_libs=yes
	  ;;
	esac
	build_libtool_libs=no
	build_old_libs=yes
	break
	;;
      esac
    done
    test -n "$old_archive_from_new_cmds" && build_old_libs=yes
    while test "$#" -gt 0; do
      arg=$1
      shift
      func_quote_arg pretty,unquoted "$arg"
      qarg=$func_quote_arg_unquoted_result
      func_append libtool_args " $func_quote_arg_result"
      if test -n "$prev"; then
	case $prev in
	output)
	  func_append compile_command " @OUTPUT@"
	  func_append finalize_command " @OUTPUT@"
	  ;;
	esac
	case $prev in
	bindir)
	  bindir=$arg
	  prev=
	  continue
	  ;;
	dlfiles|dlprefiles)
	  $preload || {
	    func_append compile_command " @SYMFILE@"
	    func_append finalize_command " @SYMFILE@"
	    preload=:
	  }
	  case $arg in
	  *.la | *.lo) ;;  # We handle these cases below.
	  force)
	    if test no = "$dlself"; then
	      dlself=needless
	      export_dynamic=yes
	    fi
	    prev=
	    continue
	    ;;
	  self)
	    if test dlprefiles = "$prev"; then
	      dlself=yes
	    elif test dlfiles = "$prev" && test yes != "$dlopen_self"; then
	      dlself=yes
	    else
	      dlself=needless
	      export_dynamic=yes
	    fi
	    prev=
	    continue
	    ;;
	  *)
	    if test dlfiles = "$prev"; then
	      func_append dlfiles " $arg"
	    else
	      func_append dlprefiles " $arg"
	    fi
	    prev=
	    continue
	    ;;
	  esac
	  ;;
	expsyms)
	  export_symbols=$arg
	  test -f "$arg" \
	    || func_fatal_error "symbol file '$arg' does not exist"
	  prev=
	  continue
	  ;;
	expsyms_regex)
	  export_symbols_regex=$arg
	  prev=
	  continue
	  ;;
	framework)
	  case $host in
	    *-*-darwin*)
	      case "$deplibs " in
		*" $qarg.ltframework "*) ;;
		*) func_append deplibs " $qarg.ltframework" # this is fixed later
		   ;;
	      esac
	      ;;
	  esac
	  prev=
	  continue
	  ;;
	inst_prefix)
	  inst_prefix_dir=$arg
	  prev=
	  continue
	  ;;
	mllvm)
	  prev=
	  continue
	  ;;
	objectlist)
	  if test -f "$arg"; then
	    save_arg=$arg
	    moreargs=
	    for fil in `cat "$save_arg"`
	    do
	      arg=$fil
	      if func_lalib_unsafe_p "$arg"; then
		pic_object=
		non_pic_object=
		func_source "$arg"
		if test -z "$pic_object" ||
		   test -z "$non_pic_object" ||
		   test none = "$pic_object" &&
		   test none = "$non_pic_object"; then
		  func_fatal_error "cannot find name of object for '$arg'"
		fi
		func_dirname "$arg" "/" ""
		xdir=$func_dirname_result
		if test none != "$pic_object"; then
		  pic_object=$xdir$pic_object
		  if test dlfiles = "$prev"; then
		    if test yes = "$build_libtool_libs" && test yes = "$dlopen_support"; then
		      func_append dlfiles " $pic_object"
		      prev=
		      continue
		    else
		      prev=dlprefiles
		    fi
		  fi
		  if test dlprefiles = "$prev"; then
		    func_append dlprefiles " $pic_object"
		    prev=
		  fi
		  func_append libobjs " $pic_object"
		  arg=$pic_object
		fi
		if test none != "$non_pic_object"; then
		  non_pic_object=$xdir$non_pic_object
		  func_append non_pic_objects " $non_pic_object"
		  if test -z "$pic_object" || test none = "$pic_object"; then
		    arg=$non_pic_object
		  fi
		else
		  non_pic_object=$pic_object
		  func_append non_pic_objects " $non_pic_object"
		fi
	      else
		if $opt_dry_run; then
		  func_dirname "$arg" "/" ""
		  xdir=$func_dirname_result
		  func_lo2o "$arg"
		  pic_object=$xdir$objdir/$func_lo2o_result
		  non_pic_object=$xdir$func_lo2o_result
		  func_append libobjs " $pic_object"
		  func_append non_pic_objects " $non_pic_object"
	        else
		  func_fatal_error "'$arg' is not a valid libtool object"
		fi
	      fi
	    done
	  else
	    func_fatal_error "link input file '$arg' does not exist"
	  fi
	  arg=$save_arg
	  prev=
	  continue
	  ;;
	os2dllname)
	  os2dllname=$arg
	  prev=
	  continue
	  ;;
	precious_regex)
	  precious_files_regex=$arg
	  prev=
	  continue
	  ;;
	release)
	  release=-$arg
	  prev=
	  continue
	  ;;
	rpath | xrpath)
	  case $arg in
	  [\\/]* | [A-Za-z]:[\\/]*) ;;
	  *)
	    func_fatal_error "only absolute run-paths are allowed"
	    ;;
	  esac
	  if test rpath = "$prev"; then
	    case "$rpath " in
	    *" $arg "*) ;;
	    *) func_append rpath " $arg" ;;
	    esac
	  else
	    case "$xrpath " in
	    *" $arg "*) ;;
	    *) func_append xrpath " $arg" ;;
	    esac
	  fi
	  prev=
	  continue
	  ;;
	shrext)
	  shrext_cmds=$arg
	  prev=
	  continue
	  ;;
	weak)
	  func_append weak_libs " $arg"
	  prev=
	  continue
	  ;;
	xassembler)
	  func_append compiler_flags " -Xassembler $qarg"
	  prev=
	  func_append compile_command " -Xassembler $qarg"
	  func_append finalize_command " -Xassembler $qarg"
	  continue
	  ;;
	xcclinker)
	  func_append linker_flags " $qarg"
	  func_append compiler_flags " $qarg"
	  prev=
	  func_append compile_command " $qarg"
	  func_append finalize_command " $qarg"
	  continue
	  ;;
	xcompiler)
	  func_append compiler_flags " $qarg"
	  prev=
	  func_append compile_command " $qarg"
	  func_append finalize_command " $qarg"
	  continue
	  ;;
	xlinker)
	  func_append linker_flags " $qarg"
	  func_append compiler_flags " $wl$qarg"
	  prev=
	  func_append compile_command " $wl$qarg"
	  func_append finalize_command " $wl$qarg"
	  continue
	  ;;
	*)
	  eval "$prev=\"\$arg\""
	  prev=
	  continue
	  ;;
	esac
      fi # test -n "$prev"
      prevarg=$arg
      case $arg in
      -all-static)
	if test -n "$link_static_flag"; then
	  func_append compile_command " $link_static_flag"
	  func_append finalize_command " $link_static_flag"
	fi
	continue
	;;
      -allow-undefined)
	func_fatal_error "'-allow-undefined' must not be used because it is the default"
	;;
      -avoid-version)
	avoid_version=yes
	continue
	;;
      -bindir)
	prev=bindir
	continue
	;;
      -dlopen)
	prev=dlfiles
	continue
	;;
      -dlpreopen)
	prev=dlprefiles
	continue
	;;
      -export-dynamic)
	export_dynamic=yes
	continue
	;;
      -export-symbols | -export-symbols-regex)
	if test -n "$export_symbols" || test -n "$export_symbols_regex"; then
	  func_fatal_error "more than one -exported-symbols argument is not allowed"
	fi
	if test X-export-symbols = "X$arg"; then
	  prev=expsyms
	else
	  prev=expsyms_regex
	fi
	continue
	;;
      -framework)
	prev=framework
	continue
	;;
      -inst-prefix-dir)
	prev=inst_prefix
	continue
	;;
      -L[A-Z][A-Z]*:*)
	case $with_gcc/$host in
	no/*-*-irix* | /*-*-irix*)
	  func_append compile_command " $arg"
	  func_append finalize_command " $arg"
	  ;;
	esac
	continue
	;;
      -L*)
	func_stripname "-L" '' "$arg"
	if test -z "$func_stripname_result"; then
	  if test "$#" -gt 0; then
	    func_fatal_error "require no space between '-L' and '$1'"
	  else
	    func_fatal_error "need path for '-L' option"
	  fi
	fi
	func_resolve_sysroot "$func_stripname_result"
	dir=$func_resolve_sysroot_result
	case $dir in
	[\\/]* | [A-Za-z]:[\\/]*) ;;
	*)
	  absdir=`cd "$dir" && pwd`
	  test -z "$absdir" && \
	    func_fatal_error "cannot determine absolute directory name of '$dir'"
	  dir=$absdir
	  ;;
	esac
	case "$deplibs " in
	*" -L$dir "* | *" $arg "*)
	  ;;
	*)
	  case $dir in
	    [\\/]* | [A-Za-z]:[\\/]* | =*) func_append deplibs " $arg" ;;
	    *) func_append deplibs " -L$dir" ;;
	  esac
	  func_append lib_search_path " $dir"
	  ;;
	esac
	case $host in
	*-*-cygwin* | *-*-mingw* | *-*-pw32* | *-*-os2* | *-cegcc*)
	  testbindir=`$ECHO "$dir" | $SED 's*/lib$*/bin*'`
	  case :$dllsearchpath: in
	  *":$dir:"*) ;;
	  ::) dllsearchpath=$dir;;
	  *) func_append dllsearchpath ":$dir";;
	  esac
	  case :$dllsearchpath: in
	  *":$testbindir:"*) ;;
	  ::) dllsearchpath=$testbindir;;
	  *) func_append dllsearchpath ":$testbindir";;
	  esac
	  ;;
	esac
	continue
	;;
      -l*)
	if test X-lc = "X$arg" || test X-lm = "X$arg"; then
	  case $host in
	  *-*-cygwin* | *-*-mingw* | *-*-pw32* | *-*-beos* | *-cegcc* | *-*-haiku*)
	    continue
	    ;;
	  *-*-os2*)
	    test X-lc = "X$arg" && continue
	    ;;
	  *-*-openbsd* | *-*-freebsd* | *-*-dragonfly* | *-*-bitrig* | *-*-midnightbsd*)
	    test X-lc = "X$arg" && continue
	    ;;
	  *-*-rhapsody* | *-*-darwin1.[012])
	    func_append deplibs " System.ltframework"
	    continue
	    ;;
	  *-*-sco3.2v5* | *-*-sco5v6*)
	    test X-lc = "X$arg" && continue
	    ;;
	  *-*-sysv4.2uw2* | *-*-sysv5* | *-*-unixware* | *-*-OpenUNIX*)
	    test X-lc = "X$arg" && continue
	    ;;
	  esac
	elif test X-lc_r = "X$arg"; then
	 case $host in
	 *-*-openbsd* | *-*-freebsd* | *-*-dragonfly* | *-*-bitrig* | *-*-midnightbsd*)
	   continue
	   ;;
	 esac
	fi
	func_append deplibs " $arg"
	continue
	;;
      -mllvm)
	prev=mllvm
	continue
	;;
      -module)
	module=yes
	continue
	;;
      -model|-arch|-isysroot|--sysroot)
	func_append compiler_flags " $arg"
	func_append compile_command " $arg"
	func_append finalize_command " $arg"
	prev=xcompiler
	continue
	;;
     -pthread)
	case $host in
	  *solaris2*) ;;
	  *)
	    case "$new_inherited_linker_flags " in
	        *" $arg "*) ;;
	        * ) func_append new_inherited_linker_flags " $arg" ;;
	    esac
	  ;;
	esac
	continue
	;;
      -mt|-mthreads|-kthread|-Kthread|-pthreads|--thread-safe \
      |-threads|-fopenmp|-openmp|-mp|-xopenmp|-omp|-qsmp=*)
	func_append compiler_flags " $arg"
	func_append compile_command " $arg"
	func_append finalize_command " $arg"
	case "$new_inherited_linker_flags " in
	    *" $arg "*) ;;
	    * ) func_append new_inherited_linker_flags " $arg" ;;
	esac
	continue
	;;
      -multi_module)
	single_module=$wl-multi_module
	continue
	;;
      -no-fast-install)
	fast_install=no
	continue
	;;
      -no-install)
	case $host in
	*-*-cygwin* | *-*-mingw* | *-*-pw32* | *-*-os2* | *-*-darwin* | *-cegcc*)
	  func_warning "'-no-install' is ignored for $host"
	  func_warning "assuming '-no-fast-install' instead"
	  fast_install=no
	  ;;
	*) no_install=yes ;;
	esac
	continue
	;;
      -no-undefined)
	allow_undefined=no
	continue
	;;
      -objectlist)
	prev=objectlist
	continue
	;;
      -os2dllname)
	prev=os2dllname
	continue
	;;
      -o) prev=output ;;
      -precious-files-regex)
	prev=precious_regex
	continue
	;;
      -release)
	prev=release
	continue
	;;
      -rpath)
	prev=rpath
	continue
	;;
      -R)
	prev=xrpath
	continue
	;;
      -R*)
	func_stripname '-R' '' "$arg"
	dir=$func_stripname_result
	case $dir in
	[\\/]* | [A-Za-z]:[\\/]*) ;;
	=*)
	  func_stripname '=' '' "$dir"
	  dir=$lt_sysroot$func_stripname_result
	  ;;
	*)
	  func_fatal_error "only absolute run-paths are allowed"
	  ;;
	esac
	case "$xrpath " in
	*" $dir "*) ;;
	*) func_append xrpath " $dir" ;;
	esac
	continue
	;;
      -shared)
	continue
	;;
      -shrext)
	prev=shrext
	continue
	;;
      -static | -static-libtool-libs)
	continue
	;;
      -thread-safe)
	thread_safe=yes
	continue
	;;
      -version-info)
	prev=vinfo
	continue
	;;
      -version-number)
	prev=vinfo
	vinfo_number=yes
	continue
	;;
      -weak)
        prev=weak
	continue
	;;
      -Wc,*)
	func_stripname '-Wc,' '' "$arg"
	args=$func_stripname_result
	arg=
	save_ifs=$IFS; IFS=,
	for flag in $args; do
	  IFS=$save_ifs
          func_quote_arg pretty "$flag"
	  func_append arg " $func_quote_arg_result"
	  func_append compiler_flags " $func_quote_arg_result"
	done
	IFS=$save_ifs
	func_stripname ' ' '' "$arg"
	arg=$func_stripname_result
	;;
      -Wl,*)
	func_stripname '-Wl,' '' "$arg"
	args=$func_stripname_result
	arg=
	save_ifs=$IFS; IFS=,
	for flag in $args; do
	  IFS=$save_ifs
          func_quote_arg pretty "$flag"
	  func_append arg " $wl$func_quote_arg_result"
	  func_append compiler_flags " $wl$func_quote_arg_result"
	  func_append linker_flags " $func_quote_arg_result"
	done
	IFS=$save_ifs
	func_stripname ' ' '' "$arg"
	arg=$func_stripname_result
	;;
      -Xassembler)
        prev=xassembler
        continue
        ;;
      -Xcompiler)
	prev=xcompiler
	continue
	;;
      -Xlinker)
	prev=xlinker
	continue
	;;
      -XCClinker)
	prev=xcclinker
	continue
	;;
      -msg_*)
	func_quote_arg pretty "$arg"
	arg=$func_quote_arg_result
	;;
      -64|-mips[0-9]|-r[0-9][0-9]*|-xarch=*|-xtarget=*|+DA*|+DD*|-q*|-m*| \
      -t[45]*|-txscale*|-p|-pg|--coverage|-fprofile-*|-F*|@*|-tp=*|--sysroot=*| \
      -O*|-g*|-flto*|-fwhopr*|-fuse-linker-plugin|-fstack-protector*|-stdlib=*| \
      -specs=*|-fsanitize=*|-fuse-ld=*|-Wa,*)
        func_quote_arg pretty "$arg"
	arg=$func_quote_arg_result
        func_append compile_command " $arg"
        func_append finalize_command " $arg"
        func_append compiler_flags " $arg"
        continue
        ;;
      -Z*)
        if test os2 = "`expr $host : '.*\(os2\)'`"; then
	  compiler_flags="$compiler_flags $arg"
	  func_append compile_command " $arg"
	  func_append finalize_command " $arg"
	  case $arg in
	  -Zlinker | -Zstack)
	    prev=xcompiler
	    ;;
	  esac
	  continue
        else
	  func_quote_arg pretty "$arg"
	  arg=$func_quote_arg_result
        fi
	;;
      -* | +*)
        func_quote_arg pretty "$arg"
	arg=$func_quote_arg_result
	;;
      *.$objext)
	func_append objs " $arg"
	;;
      *.lo)
	if func_lalib_unsafe_p "$arg"; then
	  pic_object=
	  non_pic_object=
	  func_source "$arg"
	  if test -z "$pic_object" ||
	     test -z "$non_pic_object" ||
	     test none = "$pic_object" &&
	     test none = "$non_pic_object"; then
	    func_fatal_error "cannot find name of object for '$arg'"
	  fi
	  func_dirname "$arg" "/" ""
	  xdir=$func_dirname_result
	  test none = "$pic_object" || {
	    pic_object=$xdir$pic_object
	    if test dlfiles = "$prev"; then
	      if test yes = "$build_libtool_libs" && test yes = "$dlopen_support"; then
		func_append dlfiles " $pic_object"
		prev=
		continue
	      else
		prev=dlprefiles
	      fi
	    fi
	    if test dlprefiles = "$prev"; then
	      func_append dlprefiles " $pic_object"
	      prev=
	    fi
	    func_append libobjs " $pic_object"
	    arg=$pic_object
	  }
	  if test none != "$non_pic_object"; then
	    non_pic_object=$xdir$non_pic_object
	    func_append non_pic_objects " $non_pic_object"
	    if test -z "$pic_object" || test none = "$pic_object"; then
	      arg=$non_pic_object
	    fi
	  else
	    non_pic_object=$pic_object
	    func_append non_pic_objects " $non_pic_object"
	  fi
	else
	  if $opt_dry_run; then
	    func_dirname "$arg" "/" ""
	    xdir=$func_dirname_result
	    func_lo2o "$arg"
	    pic_object=$xdir$objdir/$func_lo2o_result
	    non_pic_object=$xdir$func_lo2o_result
	    func_append libobjs " $pic_object"
	    func_append non_pic_objects " $non_pic_object"
	  else
	    func_fatal_error "'$arg' is not a valid libtool object"
	  fi
	fi
	;;
      *.$libext)
	func_append deplibs " $arg"
	func_append old_deplibs " $arg"
	continue
	;;
      *.la)
	func_resolve_sysroot "$arg"
	if test dlfiles = "$prev"; then
	  func_append dlfiles " $func_resolve_sysroot_result"
	  prev=
	elif test dlprefiles = "$prev"; then
	  func_append dlprefiles " $func_resolve_sysroot_result"
	  prev=
	else
	  func_append deplibs " $func_resolve_sysroot_result"
	fi
	continue
	;;
      *)
	func_quote_arg pretty "$arg"
	arg=$func_quote_arg_result
	;;
      esac # arg
      if test -n "$arg"; then
	func_append compile_command " $arg"
	func_append finalize_command " $arg"
      fi
    done # argument parsing loop
    test -n "$prev" && \
      func_fatal_help "the '$prevarg' option requires an argument"
    if test yes = "$export_dynamic" && test -n "$export_dynamic_flag_spec"; then
      eval arg=\"$export_dynamic_flag_spec\"
      func_append compile_command " $arg"
      func_append finalize_command " $arg"
    fi
    oldlibs=
    func_basename "$output"
    outputname=$func_basename_result
    libobjs_save=$libobjs
    if test -n "$shlibpath_var"; then
      eval shlib_search_path=\`\$ECHO \"\$$shlibpath_var\" \| \$SED \'s/:/ /g\'\`
    else
      shlib_search_path=
    fi
    eval sys_lib_search_path=\"$sys_lib_search_path_spec\"
    eval sys_lib_dlsearch_path=\"$sys_lib_dlsearch_path_spec\"
    func_munge_path_list sys_lib_dlsearch_path "$LT_SYS_LIBRARY_PATH"
    func_dirname "$output" "/" ""
    output_objdir=$func_dirname_result$objdir
    func_to_tool_file "$output_objdir/"
    tool_output_objdir=$func_to_tool_file_result
    func_mkdir_p "$output_objdir"
    case $output in
    "")
      func_fatal_help "you must specify an output file"
      ;;
    *.$libext) linkmode=oldlib ;;
    *.lo | *.$objext) linkmode=obj ;;
    *.la) linkmode=lib ;;
    *) linkmode=prog ;; # Anything else should be a program.
    esac
    specialdeplibs=
    libs=
    for deplib in $deplibs; do
      if $opt_preserve_dup_deps; then
	case "$libs " in
	*" $deplib "*) func_append specialdeplibs " $deplib" ;;
	esac
      fi
      func_append libs " $deplib"
    done
    if test lib = "$linkmode"; then
      libs="$predeps $libs $compiler_lib_search_path $postdeps"
      pre_post_deps=
      if $opt_duplicate_compiler_generated_deps; then
	for pre_post_dep in $predeps $postdeps; do
	  case "$pre_post_deps " in
	  *" $pre_post_dep "*) func_append specialdeplibs " $pre_post_deps" ;;
	  esac
	  func_append pre_post_deps " $pre_post_dep"
	done
      fi
      pre_post_deps=
    fi
    deplibs=
    newdependency_libs=
    newlib_search_path=
    need_relink=no # whether we're linking any uninstalled libtool libraries
    notinst_deplibs= # not-installed libtool libraries
    notinst_path= # paths that contain not-installed libtool libraries
    case $linkmode in
    lib)
	passes="conv dlpreopen link"
	for file in $dlfiles $dlprefiles; do
	  case $file in
	  *.la) ;;
	  *)
	    func_fatal_help "libraries can '-dlopen' only libtool libraries: $file"
	    ;;
	  esac
	done
	;;
    prog)
	compile_deplibs=
	finalize_deplibs=
	alldeplibs=false
	newdlfiles=
	newdlprefiles=
	passes="conv scan dlopen dlpreopen link"
	;;
    *)  passes="conv"
	;;
    esac
    for pass in $passes; do
      if test lib,link = "$linkmode,$pass"; then
        tmp_deplibs=
	for deplib in $deplibs; do
	  tmp_deplibs="$deplib $tmp_deplibs"
	done
	deplibs=$tmp_deplibs
      fi
      if test lib,link = "$linkmode,$pass" ||
	 test prog,scan = "$linkmode,$pass"; then
	libs=$deplibs
	deplibs=
      fi
      if test prog = "$linkmode"; then
	case $pass in
	dlopen) libs=$dlfiles ;;
	dlpreopen) libs=$dlprefiles ;;
	link) libs="$deplibs %DEPLIBS% $dependency_libs" ;;
	esac
      fi
      if test lib,dlpreopen = "$linkmode,$pass"; then
	for lib in $dlprefiles; do
	  dependency_libs=
	  func_resolve_sysroot "$lib"
	  case $lib in
	  *.la)	func_source "$func_resolve_sysroot_result" ;;
	  esac
	  for deplib in $dependency_libs; do
	    func_basename "$deplib"
            deplib_base=$func_basename_result
	    case " $weak_libs " in
	    *" $deplib_base "*) ;;
	    *) func_append deplibs " $deplib" ;;
	    esac
	  done
	done
	libs=$dlprefiles
      fi
      if test dlopen = "$pass"; then
	save_deplibs=$deplibs
	deplibs=
      fi
      for deplib in $libs; do
	lib=
	found=false
	case $deplib in
	-mt|-mthreads|-kthread|-Kthread|-pthread|-pthreads|--thread-safe \
        |-threads|-fopenmp|-openmp|-mp|-xopenmp|-omp|-qsmp=*)
	  if test prog,link = "$linkmode,$pass"; then
	    compile_deplibs="$deplib $compile_deplibs"
	    finalize_deplibs="$deplib $finalize_deplibs"
	  else
	    func_append compiler_flags " $deplib"
	    if test lib = "$linkmode"; then
		case "$new_inherited_linker_flags " in
		    *" $deplib "*) ;;
		    * ) func_append new_inherited_linker_flags " $deplib" ;;
		esac
	    fi
	  fi
	  continue
	  ;;
	-l*)
	  if test lib != "$linkmode" && test prog != "$linkmode"; then
	    func_warning "'-l' is ignored for archives/objects"
	    continue
	  fi
	  func_stripname '-l' '' "$deplib"
	  name=$func_stripname_result
	  if test lib = "$linkmode"; then
	    searchdirs="$newlib_search_path $lib_search_path $compiler_lib_search_dirs $sys_lib_search_path $shlib_search_path"
	  else
	    searchdirs="$newlib_search_path $lib_search_path $sys_lib_search_path $shlib_search_path"
	  fi
	  for searchdir in $searchdirs; do
	    for search_ext in .la $std_shrext .so .a; do
	      lib=$searchdir/lib$name$search_ext
	      if test -f "$lib"; then
		if test .la = "$search_ext"; then
		  found=:
		else
		  found=false
		fi
		break 2
	      fi
	    done
	  done
	  if $found; then
	    if test yes = "$allow_libtool_libs_with_static_runtimes"; then
	      case " $predeps $postdeps " in
	      *" $deplib "*)
		if func_lalib_p "$lib"; then
		  library_names=
		  old_library=
		  func_source "$lib"
		  for l in $old_library $library_names; do
		    ll=$l
		  done
		  if test "X$ll" = "X$old_library"; then # only static version available
		    found=false
		    func_dirname "$lib" "" "."
		    ladir=$func_dirname_result
		    lib=$ladir/$old_library
		    if test prog,link = "$linkmode,$pass"; then
		      compile_deplibs="$deplib $compile_deplibs"
		      finalize_deplibs="$deplib $finalize_deplibs"
		    else
		      deplibs="$deplib $deplibs"
		      test lib = "$linkmode" && newdependency_libs="$deplib $newdependency_libs"
		    fi
		    continue
		  fi
		fi
		;;
	      *) ;;
	      esac
	    fi
	  else
	    if test prog,link = "$linkmode,$pass"; then
	      compile_deplibs="$deplib $compile_deplibs"
	      finalize_deplibs="$deplib $finalize_deplibs"
	    else
	      deplibs="$deplib $deplibs"
	      test lib = "$linkmode" && newdependency_libs="$deplib $newdependency_libs"
	    fi
	    continue
	  fi
	  ;; # -l
	*.ltframework)
	  if test prog,link = "$linkmode,$pass"; then
	    compile_deplibs="$deplib $compile_deplibs"
	    finalize_deplibs="$deplib $finalize_deplibs"
	  else
	    deplibs="$deplib $deplibs"
	    if test lib = "$linkmode"; then
		case "$new_inherited_linker_flags " in
		    *" $deplib "*) ;;
		    * ) func_append new_inherited_linker_flags " $deplib" ;;
		esac
	    fi
	  fi
	  continue
	  ;;
	-L*)
	  case $linkmode in
	  lib)
	    deplibs="$deplib $deplibs"
	    test conv = "$pass" && continue
	    newdependency_libs="$deplib $newdependency_libs"
	    func_stripname '-L' '' "$deplib"
	    func_resolve_sysroot "$func_stripname_result"
	    func_append newlib_search_path " $func_resolve_sysroot_result"
	    ;;
	  prog)
	    if test conv = "$pass"; then
	      deplibs="$deplib $deplibs"
	      continue
	    fi
	    if test scan = "$pass"; then
	      deplibs="$deplib $deplibs"
	    else
	      compile_deplibs="$deplib $compile_deplibs"
	      finalize_deplibs="$deplib $finalize_deplibs"
	    fi
	    func_stripname '-L' '' "$deplib"
	    func_resolve_sysroot "$func_stripname_result"
	    func_append newlib_search_path " $func_resolve_sysroot_result"
	    ;;
	  *)
	    func_warning "'-L' is ignored for archives/objects"
	    ;;
	  esac # linkmode
	  continue
	  ;; # -L
	-R*)
	  if test link = "$pass"; then
	    func_stripname '-R' '' "$deplib"
	    func_resolve_sysroot "$func_stripname_result"
	    dir=$func_resolve_sysroot_result
	    case "$xrpath " in
	    *" $dir "*) ;;
	    *) func_append xrpath " $dir" ;;
	    esac
	  fi
	  deplibs="$deplib $deplibs"
	  continue
	  ;;
	*.la)
	  func_resolve_sysroot "$deplib"
	  lib=$func_resolve_sysroot_result
	  ;;
	*.$libext)
	  if test conv = "$pass"; then
	    deplibs="$deplib $deplibs"
	    continue
	  fi
	  case $linkmode in
	  lib)
	    case " $dlpreconveniencelibs " in
	    *" $deplib "*) ;;
	    *)
	      valid_a_lib=false
	      case $deplibs_check_method in
		match_pattern*)
		  set dummy $deplibs_check_method; shift
		  match_pattern_regex=`expr "$deplibs_check_method" : "$1 \(.*\)"`
		  if eval "\$ECHO \"$deplib\"" 2>/dev/null | $SED 10q \
		    | $EGREP "$match_pattern_regex" > /dev/null; then
		    valid_a_lib=:
		  fi
		;;
		pass_all)
		  valid_a_lib=:
		;;
	      esac
	      if $valid_a_lib; then
		echo
		$ECHO "*** Warning: Linking the shared library $output against the"
		$ECHO "*** static library $deplib is not portable!"
		deplibs="$deplib $deplibs"
	      else
		echo
		$ECHO "*** Warning: Trying to link with static lib archive $deplib."
		echo "*** I have the capability to make that library automatically link in when"
		echo "*** you link to this library.  But I can only do this if you have a"
		echo "*** shared version of the library, which you do not appear to have"
		echo "*** because the file extensions .$libext of this argument makes me believe"
		echo "*** that it is just a static archive that I should not use here."
	      fi
	      ;;
	    esac
	    continue
	    ;;
	  prog)
	    if test link != "$pass"; then
	      deplibs="$deplib $deplibs"
	    else
	      compile_deplibs="$deplib $compile_deplibs"
	      finalize_deplibs="$deplib $finalize_deplibs"
	    fi
	    continue
	    ;;
	  esac # linkmode
	  ;; # *.$libext
	*.lo | *.$objext)
	  if test conv = "$pass"; then
	    deplibs="$deplib $deplibs"
	  elif test prog = "$linkmode"; then
	    if test dlpreopen = "$pass" || test yes != "$dlopen_support" || test no = "$build_libtool_libs"; then
	      func_append newdlprefiles " $deplib"
	      compile_deplibs="$deplib $compile_deplibs"
	      finalize_deplibs="$deplib $finalize_deplibs"
	    else
	      func_append newdlfiles " $deplib"
	    fi
	  fi
	  continue
	  ;;
	%DEPLIBS%)
	  alldeplibs=:
	  continue
	  ;;
	esac # case $deplib
	$found || test -f "$lib" \
	  || func_fatal_error "cannot find the library '$lib' or unhandled argument '$deplib'"
	func_lalib_unsafe_p "$lib" \
	  || func_fatal_error "'$lib' is not a valid libtool archive"
	func_dirname "$lib" "" "."
	ladir=$func_dirname_result
	dlname=
	dlopen=
	dlpreopen=
	libdir=
	library_names=
	old_library=
	inherited_linker_flags=
	installed=yes
	shouldnotlink=no
	avoidtemprpath=
	func_source "$lib"
	if test -n "$inherited_linker_flags"; then
	  tmp_inherited_linker_flags=`$ECHO "$inherited_linker_flags" | $SED 's/-framework \([^ $]*\)/\1.ltframework/g'`
	  for tmp_inherited_linker_flag in $tmp_inherited_linker_flags; do
	    case " $new_inherited_linker_flags " in
	      *" $tmp_inherited_linker_flag "*) ;;
	      *) func_append new_inherited_linker_flags " $tmp_inherited_linker_flag";;
	    esac
	  done
	fi
	dependency_libs=`$ECHO " $dependency_libs" | $SED 's% \([^ $]*\).ltframework% -framework \1%g'`
	if test lib,link = "$linkmode,$pass" ||
	   test prog,scan = "$linkmode,$pass" ||
	   { test prog != "$linkmode" && test lib != "$linkmode"; }; then
	  test -n "$dlopen" && func_append dlfiles " $dlopen"
	  test -n "$dlpreopen" && func_append dlprefiles " $dlpreopen"
	fi
	if test conv = "$pass"; then
	  deplibs="$lib $deplibs"
	  if test -z "$libdir"; then
	    if test -z "$old_library"; then
	      func_fatal_error "cannot find name of link library for '$lib'"
	    fi
	    func_append convenience " $ladir/$objdir/$old_library"
	    func_append old_convenience " $ladir/$objdir/$old_library"
	  elif test prog != "$linkmode" && test lib != "$linkmode"; then
	    func_fatal_error "'$lib' is not a convenience library"
	  fi
	  tmp_libs=
	  for deplib in $dependency_libs; do
	    deplibs="$deplib $deplibs"
	    if $opt_preserve_dup_deps; then
	      case "$tmp_libs " in
	      *" $deplib "*) func_append specialdeplibs " $deplib" ;;
	      esac
	    fi
	    func_append tmp_libs " $deplib"
	  done
	  continue
	fi # $pass = conv
	linklib=
	if test -n "$old_library" &&
	   { test yes = "$prefer_static_libs" ||
	     test built,no = "$prefer_static_libs,$installed"; }; then
	  linklib=$old_library
	else
	  for l in $old_library $library_names; do
	    linklib=$l
	  done
	fi
	if test -z "$linklib"; then
	  func_fatal_error "cannot find name of link library for '$lib'"
	fi
	if test dlopen = "$pass"; then
	  test -z "$libdir" \
	    && func_fatal_error "cannot -dlopen a convenience library: '$lib'"
	  if test -z "$dlname" ||
	     test yes != "$dlopen_support" ||
	     test no = "$build_libtool_libs"
	  then
	    func_append dlprefiles " $lib $dependency_libs"
	  else
	    func_append newdlfiles " $lib"
	  fi
	  continue
	fi # $pass = dlopen
	case $ladir in
	[\\/]* | [A-Za-z]:[\\/]*) abs_ladir=$ladir ;;
	*)
	  abs_ladir=`cd "$ladir" && pwd`
	  if test -z "$abs_ladir"; then
	    func_warning "cannot determine absolute directory name of '$ladir'"
	    func_warning "passing it literally to the linker, although it might fail"
	    abs_ladir=$ladir
	  fi
	  ;;
	esac
	func_basename "$lib"
	laname=$func_basename_result
	if test yes = "$installed"; then
	  if test ! -f "$lt_sysroot$libdir/$linklib" && test -f "$abs_ladir/$linklib"; then
	    func_warning "library '$lib' was moved."
	    dir=$ladir
	    absdir=$abs_ladir
	    libdir=$abs_ladir
	  else
	    dir=$lt_sysroot$libdir
	    absdir=$lt_sysroot$libdir
	  fi
	  test yes = "$hardcode_automatic" && avoidtemprpath=yes
	else
	  if test ! -f "$ladir/$objdir/$linklib" && test -f "$abs_ladir/$linklib"; then
	    dir=$ladir
	    absdir=$abs_ladir
	    func_append notinst_path " $abs_ladir"
	  else
	    dir=$ladir/$objdir
	    absdir=$abs_ladir/$objdir
	    func_append notinst_path " $abs_ladir"
	  fi
	fi # $installed = yes
	func_stripname 'lib' '.la' "$laname"
	name=$func_stripname_result
	if test dlpreopen = "$pass"; then
	  if test -z "$libdir" && test prog = "$linkmode"; then
	    func_fatal_error "only libraries may -dlpreopen a convenience library: '$lib'"
	  fi
	  case $host in
	    *cygwin* | *mingw* | *cegcc* )
	      if test -n "$dlname"; then
	        func_tr_sh "$dir/$linklib"
	        eval "libfile_$func_tr_sh_result=\$abs_ladir/\$laname"
	        func_append newdlprefiles " $dir/$linklib"
	      else
	        func_append newdlprefiles " $dir/$old_library"
	        test -z "$libdir" && \
	          func_append dlpreconveniencelibs " $dir/$old_library"
	      fi
	    ;;
	    * )
	      if test -n "$old_library"; then
	        func_append newdlprefiles " $dir/$old_library"
	        test -z "$libdir" && \
	          func_append dlpreconveniencelibs " $dir/$old_library"
	      elif test -n "$dlname"; then
	        func_append newdlprefiles " $dir/$dlname"
	      else
	        func_append newdlprefiles " $dir/$linklib"
	      fi
	    ;;
	  esac
	fi # $pass = dlpreopen
	if test -z "$libdir"; then
	  if test lib = "$linkmode"; then
	    deplibs="$dir/$old_library $deplibs"
	  elif test prog,link = "$linkmode,$pass"; then
	    compile_deplibs="$dir/$old_library $compile_deplibs"
	    finalize_deplibs="$dir/$old_library $finalize_deplibs"
	  else
	    deplibs="$lib $deplibs" # used for prog,scan pass
	  fi
	  continue
	fi
	if test prog = "$linkmode" && test link != "$pass"; then
	  func_append newlib_search_path " $ladir"
	  deplibs="$lib $deplibs"
	  linkalldeplibs=false
	  if test no != "$link_all_deplibs" || test -z "$library_names" ||
	     test no = "$build_libtool_libs"; then
	    linkalldeplibs=:
	  fi
	  tmp_libs=
	  for deplib in $dependency_libs; do
	    case $deplib in
	    -L*) func_stripname '-L' '' "$deplib"
	         func_resolve_sysroot "$func_stripname_result"
	         func_append newlib_search_path " $func_resolve_sysroot_result"
		 ;;
	    esac
	    if $linkalldeplibs; then
	      deplibs="$deplib $deplibs"
	    else
	      newdependency_libs="$deplib $newdependency_libs"
	    fi
	    if $opt_preserve_dup_deps; then
	      case "$tmp_libs " in
	      *" $deplib "*) func_append specialdeplibs " $deplib" ;;
	      esac
	    fi
	    func_append tmp_libs " $deplib"
	  done # for deplib
	  continue
	fi # $linkmode = prog...
	if test prog,link = "$linkmode,$pass"; then
	  if test -n "$library_names" &&
	     { { test no = "$prefer_static_libs" ||
	         test built,yes = "$prefer_static_libs,$installed"; } ||
	       test -z "$old_library"; }; then
	    if test -n "$shlibpath_var" && test -z "$avoidtemprpath"; then
	      case $temp_rpath: in
	      *"$absdir:"*) ;;
	      *) func_append temp_rpath "$absdir:" ;;
	      esac
	    fi
	    case " $sys_lib_dlsearch_path " in
	    *" $absdir "*) ;;
	    *)
	      case "$compile_rpath " in
	      *" $absdir "*) ;;
	      *) func_append compile_rpath " $absdir" ;;
	      esac
	      ;;
	    esac
	    case " $sys_lib_dlsearch_path " in
	    *" $libdir "*) ;;
	    *)
	      case "$finalize_rpath " in
	      *" $libdir "*) ;;
	      *) func_append finalize_rpath " $libdir" ;;
	      esac
	      ;;
	    esac
	  fi # $linkmode,$pass = prog,link...
	  if $alldeplibs &&
	     { test pass_all = "$deplibs_check_method" ||
	       { test yes = "$build_libtool_libs" &&
		 test -n "$library_names"; }; }; then
	    continue
	  fi
	fi
	link_static=no # Whether the deplib will be linked statically
	use_static_libs=$prefer_static_libs
	if test built = "$use_static_libs" && test yes = "$installed"; then
	  use_static_libs=no
	fi
	if test -n "$library_names" &&
	   { test no = "$use_static_libs" || test -z "$old_library"; }; then
	  case $host in
	  *cygwin* | *mingw* | *cegcc* | *os2*)
	      func_append notinst_deplibs " $lib"
	      need_relink=no
	    ;;
	  *)
	    if test no = "$installed"; then
	      func_append notinst_deplibs " $lib"
	      need_relink=yes
	    fi
	    ;;
	  esac
	  dlopenmodule=
	  for dlpremoduletest in $dlprefiles; do
	    if test "X$dlpremoduletest" = "X$lib"; then
	      dlopenmodule=$dlpremoduletest
	      break
	    fi
	  done
	  if test -z "$dlopenmodule" && test yes = "$shouldnotlink" && test link = "$pass"; then
	    echo
	    if test prog = "$linkmode"; then
	      $ECHO "*** Warning: Linking the executable $output against the loadable module"
	    else
	      $ECHO "*** Warning: Linking the shared library $output against the loadable module"
	    fi
	    $ECHO "*** $linklib is not portable!"
	  fi
	  if test lib = "$linkmode" &&
	     test yes = "$hardcode_into_libs"; then
	    case " $sys_lib_dlsearch_path " in
	    *" $absdir "*) ;;
	    *)
	      case "$compile_rpath " in
	      *" $absdir "*) ;;
	      *) func_append compile_rpath " $absdir" ;;
	      esac
	      ;;
	    esac
	    case " $sys_lib_dlsearch_path " in
	    *" $libdir "*) ;;
	    *)
	      case "$finalize_rpath " in
	      *" $libdir "*) ;;
	      *) func_append finalize_rpath " $libdir" ;;
	      esac
	      ;;
	    esac
	  fi
	  if test -n "$old_archive_from_expsyms_cmds"; then
	    set dummy $library_names
	    shift
	    realname=$1
	    shift
	    libname=`eval "\\$ECHO \"$libname_spec\""`
	    if test -n "$dlname"; then
	      soname=$dlname
	    elif test -n "$soname_spec"; then
	      case $host in
	      *cygwin* | mingw* | *cegcc* | *os2*)
	        func_arith $current - $age
		major=$func_arith_result
		versuffix=-$major
		;;
	      esac
	      eval soname=\"$soname_spec\"
	    else
	      soname=$realname
	    fi
	    soroot=$soname
	    func_basename "$soroot"
	    soname=$func_basename_result
	    func_stripname 'lib' '.dll' "$soname"
	    newlib=libimp-$func_stripname_result.a
	    if test -f "$output_objdir/$soname-def"; then :
	    else
	      func_verbose "extracting exported symbol list from '$soname'"
	      func_execute_cmds "$extract_expsyms_cmds" 'exit $?'
	    fi
	    if test -f "$output_objdir/$newlib"; then :; else
	      func_verbose "generating import library for '$soname'"
	      func_execute_cmds "$old_archive_from_expsyms_cmds" 'exit $?'
	    fi
	    dir=$output_objdir
	    linklib=$newlib
	  fi # test -n "$old_archive_from_expsyms_cmds"
	  if test prog = "$linkmode" || test relink != "$opt_mode"; then
	    add_shlibpath=
	    add_dir=
	    add=
	    lib_linked=yes
	    case $hardcode_action in
	    immediate | unsupported)
	      if test no = "$hardcode_direct"; then
		add=$dir/$linklib
		case $host in
		  *-*-sco3.2v5.0.[024]*) add_dir=-L$dir ;;
		  *-*-sysv4*uw2*) add_dir=-L$dir ;;
		  *-*-sysv5OpenUNIX* | *-*-sysv5UnixWare7.[01].[10]* | \
		    *-*-unixware7*) add_dir=-L$dir ;;
		  *-*-darwin* )
		    if /usr/bin/file -L $add 2> /dev/null |
			 $GREP ": [^:]* bundle" >/dev/null; then
		      if test "X$dlopenmodule" != "X$lib"; then
			$ECHO "*** Warning: lib $linklib is a module, not a shared library"
			if test -z "$old_library"; then
			  echo
			  echo "*** And there doesn't seem to be a static archive available"
			  echo "*** The link will probably fail, sorry"
			else
			  add=$dir/$old_library
			fi
		      elif test -n "$old_library"; then
			add=$dir/$old_library
		      fi
		    fi
		esac
	      elif test no = "$hardcode_minus_L"; then
		case $host in
		*-*-sunos*) add_shlibpath=$dir ;;
		esac
		add_dir=-L$dir
		add=-l$name
	      elif test no = "$hardcode_shlibpath_var"; then
		add_shlibpath=$dir
		add=-l$name
	      else
		lib_linked=no
	      fi
	      ;;
	    relink)
	      if test yes = "$hardcode_direct" &&
	         test no = "$hardcode_direct_absolute"; then
		add=$dir/$linklib
	      elif test yes = "$hardcode_minus_L"; then
		add_dir=-L$absdir
		if test -n "$inst_prefix_dir"; then
		  case $libdir in
		    [\\/]*)
		      func_append add_dir " -L$inst_prefix_dir$libdir"
		      ;;
		  esac
		fi
		add=-l$name
	      elif test yes = "$hardcode_shlibpath_var"; then
		add_shlibpath=$dir
		add=-l$name
	      else
		lib_linked=no
	      fi
	      ;;
	    *) lib_linked=no ;;
	    esac
	    if test yes != "$lib_linked"; then
	      func_fatal_configuration "unsupported hardcode properties"
	    fi
	    if test -n "$add_shlibpath"; then
	      case :$compile_shlibpath: in
	      *":$add_shlibpath:"*) ;;
	      *) func_append compile_shlibpath "$add_shlibpath:" ;;
	      esac
	    fi
	    if test prog = "$linkmode"; then
	      test -n "$add_dir" && compile_deplibs="$add_dir $compile_deplibs"
	      test -n "$add" && compile_deplibs="$add $compile_deplibs"
	    else
	      test -n "$add_dir" && deplibs="$add_dir $deplibs"
	      test -n "$add" && deplibs="$add $deplibs"
	      if test yes != "$hardcode_direct" &&
		 test yes != "$hardcode_minus_L" &&
		 test yes = "$hardcode_shlibpath_var"; then
		case :$finalize_shlibpath: in
		*":$libdir:"*) ;;
		*) func_append finalize_shlibpath "$libdir:" ;;
		esac
	      fi
	    fi
	  fi
	  if test prog = "$linkmode" || test relink = "$opt_mode"; then
	    add_shlibpath=
	    add_dir=
	    add=
	    if test yes = "$hardcode_direct" &&
	       test no = "$hardcode_direct_absolute"; then
	      add=$libdir/$linklib
	    elif test yes = "$hardcode_minus_L"; then
	      add_dir=-L$libdir
	      add=-l$name
	    elif test yes = "$hardcode_shlibpath_var"; then
	      case :$finalize_shlibpath: in
	      *":$libdir:"*) ;;
	      *) func_append finalize_shlibpath "$libdir:" ;;
	      esac
	      add=-l$name
	    elif test yes = "$hardcode_automatic"; then
	      if test -n "$inst_prefix_dir" &&
		 test -f "$inst_prefix_dir$libdir/$linklib"; then
		add=$inst_prefix_dir$libdir/$linklib
	      else
		add=$libdir/$linklib
	      fi
	    else
	      add_dir=-L$libdir
	      if test -n "$inst_prefix_dir"; then
		case $libdir in
		  [\\/]*)
		    func_append add_dir " -L$inst_prefix_dir$libdir"
		    ;;
		esac
	      fi
	      add=-l$name
	    fi
	    if test prog = "$linkmode"; then
	      test -n "$add_dir" && finalize_deplibs="$add_dir $finalize_deplibs"
	      test -n "$add" && finalize_deplibs="$add $finalize_deplibs"
	    else
	      test -n "$add_dir" && deplibs="$add_dir $deplibs"
	      test -n "$add" && deplibs="$add $deplibs"
	    fi
	  fi
	elif test prog = "$linkmode"; then
	  if test unsupported != "$hardcode_direct"; then
	    test -n "$old_library" && linklib=$old_library
	    compile_deplibs="$dir/$linklib $compile_deplibs"
	    finalize_deplibs="$dir/$linklib $finalize_deplibs"
	  else
	    compile_deplibs="-l$name -L$dir $compile_deplibs"
	    finalize_deplibs="-l$name -L$dir $finalize_deplibs"
	  fi
	elif test yes = "$build_libtool_libs"; then
	  if test pass_all != "$deplibs_check_method"; then
	    echo
	    $ECHO "*** Warning: This system cannot link to static lib archive $lib."
	    echo "*** I have the capability to make that library automatically link in when"
	    echo "*** you link to this library.  But I can only do this if you have a"
	    echo "*** shared version of the library, which you do not appear to have."
	    if test yes = "$module"; then
	      echo "*** But as you try to build a module library, libtool will still create "
	      echo "*** a static module, that should work as long as the dlopening application"
	      echo "*** is linked with the -dlopen flag to resolve symbols at runtime."
	      if test -z "$global_symbol_pipe"; then
		echo
		echo "*** However, this would only work if libtool was able to extract symbol"
		echo "*** lists from a program, using 'nm' or equivalent, but libtool could"
		echo "*** not find such a program.  So, this module is probably useless."
		echo "*** 'nm' from GNU binutils and a full rebuild may help."
	      fi
	      if test no = "$build_old_libs"; then
		build_libtool_libs=module
		build_old_libs=yes
	      else
		build_libtool_libs=no
	      fi
	    fi
	  else
	    deplibs="$dir/$old_library $deplibs"
	    link_static=yes
	  fi
	fi # link shared/static library?
	if test lib = "$linkmode"; then
	  if test -n "$dependency_libs" &&
	     { test yes != "$hardcode_into_libs" ||
	       test yes = "$build_old_libs" ||
	       test yes = "$link_static"; }; then
	    temp_deplibs=
	    for libdir in $dependency_libs; do
	      case $libdir in
	      -R*) func_stripname '-R' '' "$libdir"
	           temp_xrpath=$func_stripname_result
		   case " $xrpath " in
		   *" $temp_xrpath "*) ;;
		   *) func_append xrpath " $temp_xrpath";;
		   esac;;
	      *) func_append temp_deplibs " $libdir";;
	      esac
	    done
	    dependency_libs=$temp_deplibs
	  fi
	  func_append newlib_search_path " $absdir"
	  test no = "$link_static" && newdependency_libs="$abs_ladir/$laname $newdependency_libs"
	  tmp_libs=
	  for deplib in $dependency_libs; do
	    newdependency_libs="$deplib $newdependency_libs"
	    case $deplib in
              -L*) func_stripname '-L' '' "$deplib"
                   func_resolve_sysroot "$func_stripname_result";;
              *) func_resolve_sysroot "$deplib" ;;
            esac
	    if $opt_preserve_dup_deps; then
	      case "$tmp_libs " in
	      *" $func_resolve_sysroot_result "*)
                func_append specialdeplibs " $func_resolve_sysroot_result" ;;
	      esac
	    fi
	    func_append tmp_libs " $func_resolve_sysroot_result"
	  done
	  if test no != "$link_all_deplibs"; then
	    for deplib in $dependency_libs; do
	      path=
	      case $deplib in
	      -L*) path=$deplib ;;
	      *.la)
	        func_resolve_sysroot "$deplib"
	        deplib=$func_resolve_sysroot_result
	        func_dirname "$deplib" "" "."
		dir=$func_dirname_result
		case $dir in
		[\\/]* | [A-Za-z]:[\\/]*) absdir=$dir ;;
		*)
		  absdir=`cd "$dir" && pwd`
		  if test -z "$absdir"; then
		    func_warning "cannot determine absolute directory name of '$dir'"
		    absdir=$dir
		  fi
		  ;;
		esac
		if $GREP "^installed=no" $deplib > /dev/null; then
		case $host in
		*-*-darwin*)
		  depdepl=
		  eval deplibrary_names=`$SED -n -e 's/^library_names=\(.*\)$/\1/p' $deplib`
		  if test -n "$deplibrary_names"; then
		    for tmp in $deplibrary_names; do
		      depdepl=$tmp
		    done
		    if test -f "$absdir/$objdir/$depdepl"; then
		      depdepl=$absdir/$objdir/$depdepl
		      darwin_install_name=`$OTOOL -L $depdepl | awk '{if (NR == 2) {print $1;exit}}'`
                      if test -z "$darwin_install_name"; then
                          darwin_install_name=`$OTOOL64 -L $depdepl  | awk '{if (NR == 2) {print $1;exit}}'`
                      fi
		      func_append compiler_flags " $wl-dylib_file $wl$darwin_install_name:$depdepl"
		      func_append linker_flags " -dylib_file $darwin_install_name:$depdepl"
		      path=
		    fi
		  fi
		  ;;
		*)
		  path=-L$absdir/$objdir
		  ;;
		esac
		else
		  eval libdir=`$SED -n -e 's/^libdir=\(.*\)$/\1/p' $deplib`
		  test -z "$libdir" && \
		    func_fatal_error "'$deplib' is not a valid libtool archive"
		  test "$absdir" != "$libdir" && \
		    func_warning "'$deplib' seems to be moved"
		  path=-L$absdir
		fi
		;;
	      esac
	      case " $deplibs " in
	      *" $path "*) ;;
	      *) deplibs="$path $deplibs" ;;
	      esac
	    done
	  fi # link_all_deplibs != no
	fi # linkmode = lib
      done # for deplib in $libs
      if test link = "$pass"; then
	if test prog = "$linkmode"; then
	  compile_deplibs="$new_inherited_linker_flags $compile_deplibs"
	  finalize_deplibs="$new_inherited_linker_flags $finalize_deplibs"
	else
	  compiler_flags="$compiler_flags "`$ECHO " $new_inherited_linker_flags" | $SED 's% \([^ $]*\).ltframework% -framework \1%g'`
	fi
      fi
      dependency_libs=$newdependency_libs
      if test dlpreopen = "$pass"; then
	for deplib in $save_deplibs; do
	  deplibs="$deplib $deplibs"
	done
      fi
      if test dlopen != "$pass"; then
	test conv = "$pass" || {
	  lib_search_path=
	  for dir in $newlib_search_path; do
	    case "$lib_search_path " in
	    *" $dir "*) ;;
	    *) func_append lib_search_path " $dir" ;;
	    esac
	  done
	  newlib_search_path=
	}
	if test prog,link = "$linkmode,$pass"; then
	  vars="compile_deplibs finalize_deplibs"
	else
	  vars=deplibs
	fi
	for var in $vars dependency_libs; do
	  eval tmp_libs=\"\$$var\"
	  new_libs=
	  for deplib in $tmp_libs; do
	    case $deplib in
	    -L*) new_libs="$deplib $new_libs" ;;
	    -R*) ;;
	    *)
	      case " $specialdeplibs " in
	      *" $deplib "*) new_libs="$deplib $new_libs" ;;
	      *)
		case " $new_libs " in
		*" $deplib "*) ;;
		*) new_libs="$deplib $new_libs" ;;
		esac
		;;
	      esac
	      ;;
	    esac
	  done
	  tmp_libs=
	  for deplib in $new_libs; do
	    case $deplib in
	    -L*)
	      case " $tmp_libs " in
	      *" $deplib "*) ;;
	      *) func_append tmp_libs " $deplib" ;;
	      esac
	      ;;
	    *) func_append tmp_libs " $deplib" ;;
	    esac
	  done
	  eval $var=\"$tmp_libs\"
	done # for var
      fi
      test CXX = "$tagname" && {
        case $host_os in
        linux*)
          case `$CC -V 2>&1 | $SED 5q` in
          *Sun\ C*) # Sun C++ 5.9
            func_suncc_cstd_abi
            if test no != "$suncc_use_cstd_abi"; then
              func_append postdeps ' -library=Cstd -library=Crun'
            fi
            ;;
          esac
          ;;
        solaris*)
          func_cc_basename "$CC"
          case $func_cc_basename_result in
          CC* | sunCC*)
            func_suncc_cstd_abi
            if test no != "$suncc_use_cstd_abi"; then
              func_append postdeps ' -library=Cstd -library=Crun'
            fi
            ;;
          esac
          ;;
        esac
      }
      tmp_libs=
      for i in $dependency_libs; do
	case " $predeps $postdeps $compiler_lib_search_path " in
	*" $i "*)
	  i=
	  ;;
	esac
	if test -n "$i"; then
	  func_append tmp_libs " $i"
	fi
      done
      dependency_libs=$tmp_libs
    done # for pass
    if test prog = "$linkmode"; then
      dlfiles=$newdlfiles
    fi
    if test prog = "$linkmode" || test lib = "$linkmode"; then
      dlprefiles=$newdlprefiles
    fi
    case $linkmode in
    oldlib)
      if test -n "$dlfiles$dlprefiles" || test no != "$dlself"; then
	func_warning "'-dlopen' is ignored for archives"
      fi
      case " $deplibs" in
      *\ -l* | *\ -L*)
	func_warning "'-l' and '-L' are ignored for archives" ;;
      esac
      test -n "$rpath" && \
	func_warning "'-rpath' is ignored for archives"
      test -n "$xrpath" && \
	func_warning "'-R' is ignored for archives"
      test -n "$vinfo" && \
	func_warning "'-version-info/-version-number' is ignored for archives"
      test -n "$release" && \
	func_warning "'-release' is ignored for archives"
      test -n "$export_symbols$export_symbols_regex" && \
	func_warning "'-export-symbols' is ignored for archives"
      build_libtool_libs=no
      oldlibs=$output
      func_append objs "$old_deplibs"
      ;;
    lib)
      case $outputname in
      lib*)
	func_stripname 'lib' '.la' "$outputname"
	name=$func_stripname_result
	eval shared_ext=\"$shrext_cmds\"
	eval libname=\"$libname_spec\"
	;;
      *)
	test no = "$module" \
	  && func_fatal_help "libtool library '$output' must begin with 'lib'"
	if test no != "$need_lib_prefix"; then
	  func_stripname '' '.la' "$outputname"
	  name=$func_stripname_result
	  eval shared_ext=\"$shrext_cmds\"
	  eval libname=\"$libname_spec\"
	else
	  func_stripname '' '.la' "$outputname"
	  libname=$func_stripname_result
	fi
	;;
      esac
      if test -n "$objs"; then
	if test pass_all != "$deplibs_check_method"; then
	  func_fatal_error "cannot build libtool library '$output' from non-libtool objects on this host:$objs"
	else
	  echo
	  $ECHO "*** Warning: Linking the shared library $output against the non-libtool"
	  $ECHO "*** objects $objs is not portable!"
	  func_append libobjs " $objs"
	fi
      fi
      test no = "$dlself" \
	|| func_warning "'-dlopen self' is ignored for libtool libraries"
      set dummy $rpath
      shift
      test 1 -lt "$#" \
	&& func_warning "ignoring multiple '-rpath's for a libtool library"
      install_libdir=$1
      oldlibs=
      if test -z "$rpath"; then
	if test yes = "$build_libtool_libs"; then
	  oldlibs="$output_objdir/$libname.$libext $oldlibs"
	  build_libtool_libs=convenience
	  build_old_libs=yes
	fi
	test -n "$vinfo" && \
	  func_warning "'-version-info/-version-number' is ignored for convenience libraries"
	test -n "$release" && \
	  func_warning "'-release' is ignored for convenience libraries"
      else
	save_ifs=$IFS; IFS=:
	set dummy $vinfo 0 0 0
	shift
	IFS=$save_ifs
	test -n "$7" && \
	  func_fatal_help "too many parameters to '-version-info'"
	case $vinfo_number in
	yes)
	  number_major=$1
	  number_minor=$2
	  number_revision=$3
	  case $version_type in
	  darwin|freebsd-elf|linux|midnightbsd-elf|osf|windows|none)
	    func_arith $number_major + $number_minor
	    current=$func_arith_result
	    age=$number_minor
	    revision=$number_revision
	    ;;
	  freebsd-aout|qnx|sunos)
	    current=$number_major
	    revision=$number_minor
	    age=0
	    ;;
	  irix|nonstopux)
	    func_arith $number_major + $number_minor
	    current=$func_arith_result
	    age=$number_minor
	    revision=$number_minor
	    lt_irix_increment=no
	    ;;
	  esac
	  ;;
	no)
	  current=$1
	  revision=$2
	  age=$3
	  ;;
	esac
	case $current in
	0|[1-9]|[1-9][0-9]|[1-9][0-9][0-9]|[1-9][0-9][0-9][0-9]|[1-9][0-9][0-9][0-9][0-9]) ;;
	*)
	  func_error "CURRENT '$current' must be a nonnegative integer"
	  func_fatal_error "'$vinfo' is not valid version information"
	  ;;
	esac
	case $revision in
	0|[1-9]|[1-9][0-9]|[1-9][0-9][0-9]|[1-9][0-9][0-9][0-9]|[1-9][0-9][0-9][0-9][0-9]) ;;
	*)
	  func_error "REVISION '$revision' must be a nonnegative integer"
	  func_fatal_error "'$vinfo' is not valid version information"
	  ;;
	esac
	case $age in
	0|[1-9]|[1-9][0-9]|[1-9][0-9][0-9]|[1-9][0-9][0-9][0-9]|[1-9][0-9][0-9][0-9][0-9]) ;;
	*)
	  func_error "AGE '$age' must be a nonnegative integer"
	  func_fatal_error "'$vinfo' is not valid version information"
	  ;;
	esac
	if test "$age" -gt "$current"; then
	  func_error "AGE '$age' is greater than the current interface number '$current'"
	  func_fatal_error "'$vinfo' is not valid version information"
	fi
	major=
	versuffix=
	verstring=
	case $version_type in
	none) ;;
	darwin)
	  func_arith $current - $age
	  major=.$func_arith_result
	  versuffix=$major.$age.$revision
	  func_arith $current + 1
	  minor_current=$func_arith_result
	  xlcverstring="$wl-compatibility_version $wl$minor_current $wl-current_version $wl$minor_current.$revision"
	  verstring="-compatibility_version $minor_current -current_version $minor_current.$revision"
          case $CC in
              nagfor*)
                  verstring="$wl-compatibility_version $wl$minor_current $wl-current_version $wl$minor_current.$revision"
                  ;;
              *)
                  verstring="-compatibility_version $minor_current -current_version $minor_current.$revision"
                  ;;
          esac
	  ;;
	freebsd-aout)
	  major=.$current
	  versuffix=.$current.$revision
	  ;;
	freebsd-elf | midnightbsd-elf)
	  func_arith $current - $age
	  major=.$func_arith_result
	  versuffix=$major.$age.$revision
	  ;;
	irix | nonstopux)
	  if test no = "$lt_irix_increment"; then
	    func_arith $current - $age
	  else
	    func_arith $current - $age + 1
	  fi
	  major=$func_arith_result
	  case $version_type in
	    nonstopux) verstring_prefix=nonstopux ;;
	    *)         verstring_prefix=sgi ;;
	  esac
	  verstring=$verstring_prefix$major.$revision
	  loop=$revision
	  while test 0 -ne "$loop"; do
	    func_arith $revision - $loop
	    iface=$func_arith_result
	    func_arith $loop - 1
	    loop=$func_arith_result
	    verstring=$verstring_prefix$major.$iface:$verstring
	  done
	  major=.$major
	  versuffix=$major.$revision
	  ;;
	linux) # correct to gnu/linux during the next big refactor
	  func_arith $current - $age
	  major=.$func_arith_result
	  versuffix=$major.$age.$revision
	  ;;
	osf)
	  func_arith $current - $age
	  major=.$func_arith_result
	  versuffix=.$current.$age.$revision
	  verstring=$current.$age.$revision
	  loop=$age
	  while test 0 -ne "$loop"; do
	    func_arith $current - $loop
	    iface=$func_arith_result
	    func_arith $loop - 1
	    loop=$func_arith_result
	    verstring=$verstring:$iface.0
	  done
	  func_append verstring ":$current.0"
	  ;;
	qnx)
	  major=.$current
	  versuffix=.$current
	  ;;
	sco)
	  major=.$current
	  versuffix=.$current
	  ;;
	sunos)
	  major=.$current
	  versuffix=.$current.$revision
	  ;;
	windows)
	  func_arith $current - $age
	  major=$func_arith_result
	  versuffix=-$major
	  ;;
	*)
	  func_fatal_configuration "unknown library version type '$version_type'"
	  ;;
	esac
	if test -z "$vinfo" && test -n "$release"; then
	  major=
	  case $version_type in
	  darwin)
	    verstring=
	    ;;
	  *)
	    verstring=0.0
	    ;;
	  esac
	  if test no = "$need_version"; then
	    versuffix=
	  else
	    versuffix=.0.0
	  fi
	fi
	if test yes,no = "$avoid_version,$need_version"; then
	  major=
	  versuffix=
	  verstring=
	fi
	if test yes = "$allow_undefined"; then
	  if test unsupported = "$allow_undefined_flag"; then
	    if test yes = "$build_old_libs"; then
	      func_warning "undefined symbols not allowed in $host shared libraries; building static only"
	      build_libtool_libs=no
	    else
	      func_fatal_error "can't build $host shared library unless -no-undefined is specified"
	    fi
	  fi
	else
	  allow_undefined_flag=$no_undefined_flag
	fi
      fi
      func_generate_dlsyms "$libname" "$libname" :
      func_append libobjs " $symfileobj"
      test " " = "$libobjs" && libobjs=
      if test relink != "$opt_mode"; then
	removelist=
	tempremovelist=`$ECHO "$output_objdir/*"`
	for p in $tempremovelist; do
	  case $p in
	    *.$objext | *.gcno)
	       ;;
	    $output_objdir/$outputname | $output_objdir/$libname.* | $output_objdir/$libname$release.*)
	       if test -n "$precious_files_regex"; then
		 if $ECHO "$p" | $EGREP -e "$precious_files_regex" >/dev/null 2>&1
		 then
		   continue
		 fi
	       fi
	       func_append removelist " $p"
	       ;;
	    *) ;;
	  esac
	done
	test -n "$removelist" && \
	  func_show_eval "${RM}r \$removelist"
      fi
      if test yes = "$build_old_libs" && test convenience != "$build_libtool_libs"; then
	func_append oldlibs " $output_objdir/$libname.$libext"
	oldobjs="$objs "`$ECHO "$libobjs" | $SP2NL | $SED "/\.$libext$/d; $lo2o" | $NL2SP`
      fi
      if test -n "$xrpath"; then
	temp_xrpath=
	for libdir in $xrpath; do
	  func_replace_sysroot "$libdir"
	  func_append temp_xrpath " -R$func_replace_sysroot_result"
	  case "$finalize_rpath " in
	  *" $libdir "*) ;;
	  *) func_append finalize_rpath " $libdir" ;;
	  esac
	done
	if test yes != "$hardcode_into_libs" || test yes = "$build_old_libs"; then
	  dependency_libs="$temp_xrpath $dependency_libs"
	fi
      fi
      old_dlfiles=$dlfiles
      dlfiles=
      for lib in $old_dlfiles; do
	case " $dlprefiles $dlfiles " in
	*" $lib "*) ;;
	*) func_append dlfiles " $lib" ;;
	esac
      done
      old_dlprefiles=$dlprefiles
      dlprefiles=
      for lib in $old_dlprefiles; do
	case "$dlprefiles " in
	*" $lib "*) ;;
	*) func_append dlprefiles " $lib" ;;
	esac
      done
      if test yes = "$build_libtool_libs"; then
	if test -n "$rpath"; then
	  case $host in
	  *-*-cygwin* | *-*-mingw* | *-*-pw32* | *-*-os2* | *-*-beos* | *-cegcc* | *-*-haiku*)
	    ;;
	  *-*-rhapsody* | *-*-darwin1.[012])
	    func_append deplibs " System.ltframework"
	    ;;
	  *-*-netbsd*)
	    ;;
	  *-*-openbsd* | *-*-freebsd* | *-*-dragonfly* | *-*-midnightbsd*)
	    ;;
	  *-*-sco3.2v5* | *-*-sco5v6*)
	    ;;
	  *-*-sysv4.2uw2* | *-*-sysv5* | *-*-unixware* | *-*-OpenUNIX*)
	    ;;
	  *)
	    if test yes = "$build_libtool_need_lc"; then
	      func_append deplibs " -lc"
	    fi
	    ;;
	  esac
	fi
	name_save=$name
	libname_save=$libname
	release_save=$release
	versuffix_save=$versuffix
	major_save=$major
	release=
	versuffix=
	major=
	newdeplibs=
	droppeddeps=no
	case $deplibs_check_method in
	pass_all)
	  newdeplibs=$deplibs
	  ;;
	test_compile)
	  $opt_dry_run || $RM conftest.c
	  cat > conftest.c <<EOF
	  int main() { return 0; }
EOF
	  $opt_dry_run || $RM conftest
	  if $LTCC $LTCFLAGS -o conftest conftest.c $deplibs; then
	    ldd_output=`ldd conftest`
	    for i in $deplibs; do
	      case $i in
	      -l*)
		func_stripname -l '' "$i"
		name=$func_stripname_result
		if test yes = "$allow_libtool_libs_with_static_runtimes"; then
		  case " $predeps $postdeps " in
		  *" $i "*)
		    func_append newdeplibs " $i"
		    i=
		    ;;
		  esac
		fi
		if test -n "$i"; then
		  libname=`eval "\\$ECHO \"$libname_spec\""`
		  deplib_matches=`eval "\\$ECHO \"$library_names_spec\""`
		  set dummy $deplib_matches; shift
		  deplib_match=$1
		  if test `expr "$ldd_output" : ".*$deplib_match"` -ne 0; then
		    func_append newdeplibs " $i"
		  else
		    droppeddeps=yes
		    echo
		    $ECHO "*** Warning: dynamic linker does not accept needed library $i."
		    echo "*** I have the capability to make that library automatically link in when"
		    echo "*** you link to this library.  But I can only do this if you have a"
		    echo "*** shared version of the library, which I believe you do not have"
		    echo "*** because a test_compile did reveal that the linker did not use it for"
		    echo "*** its dynamic dependency list that programs get resolved with at runtime."
		  fi
		fi
		;;
	      *)
		func_append newdeplibs " $i"
		;;
	      esac
	    done
	  else
	    for i in $deplibs; do
	      case $i in
	      -l*)
		func_stripname -l '' "$i"
		name=$func_stripname_result
		$opt_dry_run || $RM conftest
		if $LTCC $LTCFLAGS -o conftest conftest.c $i; then
		  ldd_output=`ldd conftest`
		  if test yes = "$allow_libtool_libs_with_static_runtimes"; then
		    case " $predeps $postdeps " in
		    *" $i "*)
		      func_append newdeplibs " $i"
		      i=
		      ;;
		    esac
		  fi
		  if test -n "$i"; then
		    libname=`eval "\\$ECHO \"$libname_spec\""`
		    deplib_matches=`eval "\\$ECHO \"$library_names_spec\""`
		    set dummy $deplib_matches; shift
		    deplib_match=$1
		    if test `expr "$ldd_output" : ".*$deplib_match"` -ne 0; then
		      func_append newdeplibs " $i"
		    else
		      droppeddeps=yes
		      echo
		      $ECHO "*** Warning: dynamic linker does not accept needed library $i."
		      echo "*** I have the capability to make that library automatically link in when"
		      echo "*** you link to this library.  But I can only do this if you have a"
		      echo "*** shared version of the library, which you do not appear to have"
		      echo "*** because a test_compile did reveal that the linker did not use this one"
		      echo "*** as a dynamic dependency that programs can get resolved with at runtime."
		    fi
		  fi
		else
		  droppeddeps=yes
		  echo
		  $ECHO "*** Warning!  Library $i is needed by this library but I was not able to"
		  echo "*** make it link in!  You will probably need to install it or some"
		  echo "*** library that it depends on before this library will be fully"
		  echo "*** functional.  Installing it before continuing would be even better."
		fi
		;;
	      *)
		func_append newdeplibs " $i"
		;;
	      esac
	    done
	  fi
	  ;;
	file_magic*)
	  set dummy $deplibs_check_method; shift
	  file_magic_regex=`expr "$deplibs_check_method" : "$1 \(.*\)"`
	  for a_deplib in $deplibs; do
	    case $a_deplib in
	    -l*)
	      func_stripname -l '' "$a_deplib"
	      name=$func_stripname_result
	      if test yes = "$allow_libtool_libs_with_static_runtimes"; then
		case " $predeps $postdeps " in
		*" $a_deplib "*)
		  func_append newdeplibs " $a_deplib"
		  a_deplib=
		  ;;
		esac
	      fi
	      if test -n "$a_deplib"; then
		libname=`eval "\\$ECHO \"$libname_spec\""`
		if test -n "$file_magic_glob"; then
		  libnameglob=`func_echo_all "$libname" | $SED -e $file_magic_glob`
		else
		  libnameglob=$libname
		fi
		test yes = "$want_nocaseglob" && nocaseglob=`shopt -p nocaseglob`
		for i in $lib_search_path $sys_lib_search_path $shlib_search_path; do
		  if test yes = "$want_nocaseglob"; then
		    shopt -s nocaseglob
		    potential_libs=`ls $i/$libnameglob[.-]* 2>/dev/null`
		    $nocaseglob
		  else
		    potential_libs=`ls $i/$libnameglob[.-]* 2>/dev/null`
		  fi
		  for potent_lib in $potential_libs; do
		      if ls -lLd "$potent_lib" 2>/dev/null |
			 $GREP " -> " >/dev/null; then
			continue
		      fi
		      potlib=$potent_lib
		      while test -h "$potlib" 2>/dev/null; do
			potliblink=`ls -ld $potlib | $SED 's/.* -> //'`
			case $potliblink in
			[\\/]* | [A-Za-z]:[\\/]*) potlib=$potliblink;;
			*) potlib=`$ECHO "$potlib" | $SED 's|[^/]*$||'`"$potliblink";;
			esac
		      done
		      if eval $file_magic_cmd \"\$potlib\" 2>/dev/null |
			 $SED -e 10q |
			 $EGREP "$file_magic_regex" > /dev/null; then
			func_append newdeplibs " $a_deplib"
			a_deplib=
			break 2
		      fi
		  done
		done
	      fi
	      if test -n "$a_deplib"; then
		droppeddeps=yes
		echo
		$ECHO "*** Warning: linker path does not have real file for library $a_deplib."
		echo "*** I have the capability to make that library automatically link in when"
		echo "*** you link to this library.  But I can only do this if you have a"
		echo "*** shared version of the library, which you do not appear to have"
		echo "*** because I did check the linker path looking for a file starting"
		if test -z "$potlib"; then
		  $ECHO "*** with $libname but no candidates were found. (...for file magic test)"
		else
		  $ECHO "*** with $libname and none of the candidates passed a file format test"
		  $ECHO "*** using a file magic. Last file checked: $potlib"
		fi
	      fi
	      ;;
	    *)
	      func_append newdeplibs " $a_deplib"
	      ;;
	    esac
	  done # Gone through all deplibs.
	  ;;
	match_pattern*)
	  set dummy $deplibs_check_method; shift
	  match_pattern_regex=`expr "$deplibs_check_method" : "$1 \(.*\)"`
	  for a_deplib in $deplibs; do
	    case $a_deplib in
	    -l*)
	      func_stripname -l '' "$a_deplib"
	      name=$func_stripname_result
	      if test yes = "$allow_libtool_libs_with_static_runtimes"; then
		case " $predeps $postdeps " in
		*" $a_deplib "*)
		  func_append newdeplibs " $a_deplib"
		  a_deplib=
		  ;;
		esac
	      fi
	      if test -n "$a_deplib"; then
		libname=`eval "\\$ECHO \"$libname_spec\""`
		for i in $lib_search_path $sys_lib_search_path $shlib_search_path; do
		  potential_libs=`ls $i/$libname[.-]* 2>/dev/null`
		  for potent_lib in $potential_libs; do
		    potlib=$potent_lib # see symlink-check above in file_magic test
		    if eval "\$ECHO \"$potent_lib\"" 2>/dev/null | $SED 10q | \
		       $EGREP "$match_pattern_regex" > /dev/null; then
		      func_append newdeplibs " $a_deplib"
		      a_deplib=
		      break 2
		    fi
		  done
		done
	      fi
	      if test -n "$a_deplib"; then
		droppeddeps=yes
		echo
		$ECHO "*** Warning: linker path does not have real file for library $a_deplib."
		echo "*** I have the capability to make that library automatically link in when"
		echo "*** you link to this library.  But I can only do this if you have a"
		echo "*** shared version of the library, which you do not appear to have"
		echo "*** because I did check the linker path looking for a file starting"
		if test -z "$potlib"; then
		  $ECHO "*** with $libname but no candidates were found. (...for regex pattern test)"
		else
		  $ECHO "*** with $libname and none of the candidates passed a file format test"
		  $ECHO "*** using a regex pattern. Last file checked: $potlib"
		fi
	      fi
	      ;;
	    *)
	      func_append newdeplibs " $a_deplib"
	      ;;
	    esac
	  done # Gone through all deplibs.
	  ;;
	none | unknown | *)
	  newdeplibs=
	  tmp_deplibs=`$ECHO " $deplibs" | $SED 's/ -lc$//; s/ -[LR][^ ]*//g'`
	  if test yes = "$allow_libtool_libs_with_static_runtimes"; then
	    for i in $predeps $postdeps; do
	      tmp_deplibs=`$ECHO " $tmp_deplibs" | $SED "s|$i||"`
	    done
	  fi
	  case $tmp_deplibs in
	  *[!\	\ ]*)
	    echo
	    if test none = "$deplibs_check_method"; then
	      echo "*** Warning: inter-library dependencies are not supported in this platform."
	    else
	      echo "*** Warning: inter-library dependencies are not known to be supported."
	    fi
	    echo "*** All declared inter-library dependencies are being dropped."
	    droppeddeps=yes
	    ;;
	  esac
	  ;;
	esac
	versuffix=$versuffix_save
	major=$major_save
	release=$release_save
	libname=$libname_save
	name=$name_save
	case $host in
	*-*-rhapsody* | *-*-darwin1.[012])
	  newdeplibs=`$ECHO " $newdeplibs" | $SED 's/ -lc / System.ltframework /'`
	  ;;
	esac
	if test yes = "$droppeddeps"; then
	  if test yes = "$module"; then
	    echo
	    echo "*** Warning: libtool could not satisfy all declared inter-library"
	    $ECHO "*** dependencies of module $libname.  Therefore, libtool will create"
	    echo "*** a static module, that should work as long as the dlopening"
	    echo "*** application is linked with the -dlopen flag."
	    if test -z "$global_symbol_pipe"; then
	      echo
	      echo "*** However, this would only work if libtool was able to extract symbol"
	      echo "*** lists from a program, using 'nm' or equivalent, but libtool could"
	      echo "*** not find such a program.  So, this module is probably useless."
	      echo "*** 'nm' from GNU binutils and a full rebuild may help."
	    fi
	    if test no = "$build_old_libs"; then
	      oldlibs=$output_objdir/$libname.$libext
	      build_libtool_libs=module
	      build_old_libs=yes
	    else
	      build_libtool_libs=no
	    fi
	  else
	    echo "*** The inter-library dependencies that have been dropped here will be"
	    echo "*** automatically added whenever a program is linked with this library"
	    echo "*** or is declared to -dlopen it."
	    if test no = "$allow_undefined"; then
	      echo
	      echo "*** Since this library must not contain undefined symbols,"
	      echo "*** because either the platform does not support them or"
	      echo "*** it was explicitly requested with -no-undefined,"
	      echo "*** libtool will only create a static version of it."
	      if test no = "$build_old_libs"; then
		oldlibs=$output_objdir/$libname.$libext
		build_libtool_libs=module
		build_old_libs=yes
	      else
		build_libtool_libs=no
	      fi
	    fi
	  fi
	fi
	deplibs=$newdeplibs
      fi
      case $host in
	*-*-darwin*)
	  newdeplibs=`$ECHO " $newdeplibs" | $SED 's% \([^ $]*\).ltframework% -framework \1%g'`
	  new_inherited_linker_flags=`$ECHO " $new_inherited_linker_flags" | $SED 's% \([^ $]*\).ltframework% -framework \1%g'`
	  deplibs=`$ECHO " $deplibs" | $SED 's% \([^ $]*\).ltframework% -framework \1%g'`
	  ;;
      esac
      new_libs=
      for path in $notinst_path; do
	case " $new_libs " in
	*" -L$path/$objdir "*) ;;
	*)
	  case " $deplibs " in
	  *" -L$path/$objdir "*)
	    func_append new_libs " -L$path/$objdir" ;;
	  esac
	  ;;
	esac
      done
      for deplib in $deplibs; do
	case $deplib in
	-L*)
	  case " $new_libs " in
	  *" $deplib "*) ;;
	  *) func_append new_libs " $deplib" ;;
	  esac
	  ;;
	*) func_append new_libs " $deplib" ;;
	esac
      done
      deplibs=$new_libs
      library_names=
      old_library=
      dlname=
      if test yes = "$build_libtool_libs"; then
	case $archive_cmds in
	  *\$LD\ *) wl= ;;
        esac
	if test yes = "$hardcode_into_libs"; then
	  hardcode_libdirs=
	  dep_rpath=
	  rpath=$finalize_rpath
	  test relink = "$opt_mode" || rpath=$compile_rpath$rpath
	  for libdir in $rpath; do
	    if test -n "$hardcode_libdir_flag_spec"; then
	      if test -n "$hardcode_libdir_separator"; then
		func_replace_sysroot "$libdir"
		libdir=$func_replace_sysroot_result
		if test -z "$hardcode_libdirs"; then
		  hardcode_libdirs=$libdir
		else
		  case $hardcode_libdir_separator$hardcode_libdirs$hardcode_libdir_separator in
		  *"$hardcode_libdir_separator$libdir$hardcode_libdir_separator"*)
		    ;;
		  *)
		    func_append hardcode_libdirs "$hardcode_libdir_separator$libdir"
		    ;;
		  esac
		fi
	      else
		eval flag=\"$hardcode_libdir_flag_spec\"
		func_append dep_rpath " $flag"
	      fi
	    elif test -n "$runpath_var"; then
	      case "$perm_rpath " in
	      *" $libdir "*) ;;
	      *) func_append perm_rpath " $libdir" ;;
	      esac
	    fi
	  done
	  if test -n "$hardcode_libdir_separator" &&
	     test -n "$hardcode_libdirs"; then
	    libdir=$hardcode_libdirs
	    eval "dep_rpath=\"$hardcode_libdir_flag_spec\""
	  fi
	  if test -n "$runpath_var" && test -n "$perm_rpath"; then
	    rpath=
	    for dir in $perm_rpath; do
	      func_append rpath "$dir:"
	    done
	    eval "$runpath_var='$rpath\$$runpath_var'; export $runpath_var"
	  fi
	  test -n "$dep_rpath" && deplibs="$dep_rpath $deplibs"
	fi
	shlibpath=$finalize_shlibpath
	test relink = "$opt_mode" || shlibpath=$compile_shlibpath$shlibpath
	if test -n "$shlibpath"; then
	  eval "$shlibpath_var='$shlibpath\$$shlibpath_var'; export $shlibpath_var"
	fi
	eval shared_ext=\"$shrext_cmds\"
	eval library_names=\"$library_names_spec\"
	set dummy $library_names
	shift
	realname=$1
	shift
	if test -n "$soname_spec"; then
	  eval soname=\"$soname_spec\"
	else
	  soname=$realname
	fi
	if test -z "$dlname"; then
	  dlname=$soname
	fi
	lib=$output_objdir/$realname
	linknames=
	for link
	do
	  func_append linknames " $link"
	done
	test -z "$pic_flag" && libobjs=`$ECHO "$libobjs" | $SP2NL | $SED "$lo2o" | $NL2SP`
	test "X$libobjs" = "X " && libobjs=
	delfiles=
	if test -n "$export_symbols" && test -n "$include_expsyms"; then
	  $opt_dry_run || cp "$export_symbols" "$output_objdir/$libname.uexp"
	  export_symbols=$output_objdir/$libname.uexp
	  func_append delfiles " $export_symbols"
	fi
	orig_export_symbols=
	case $host_os in
	cygwin* | mingw* | cegcc*)
	  if test -n "$export_symbols" && test -z "$export_symbols_regex"; then
	    func_dll_def_p "$export_symbols" || {
	      orig_export_symbols=$export_symbols
	      export_symbols=
	      always_export_symbols=yes
	    }
	  fi
	  ;;
	esac
	if test -z "$export_symbols"; then
	  if test yes = "$always_export_symbols" || test -n "$export_symbols_regex"; then
	    func_verbose "generating symbol list for '$libname.la'"
	    export_symbols=$output_objdir/$libname.exp
	    $opt_dry_run || $RM $export_symbols
	    cmds=$export_symbols_cmds
	    save_ifs=$IFS; IFS='~'
	    for cmd1 in $cmds; do
	      IFS=$save_ifs
	      case $nm_file_list_spec~$to_tool_file_cmd in
		*~func_convert_file_noop | *~func_convert_file_msys_to_w32 | ~*)
		  try_normal_branch=yes
		  eval cmd=\"$cmd1\"
		  func_len " $cmd"
		  len=$func_len_result
		  ;;
		*)
		  try_normal_branch=no
		  ;;
	      esac
	      if test yes = "$try_normal_branch" \
		 && { test "$len" -lt "$max_cmd_len" \
		      || test "$max_cmd_len" -le -1; }
	      then
		func_show_eval "$cmd" 'exit $?'
		skipped_export=false
	      elif test -n "$nm_file_list_spec"; then
		func_basename "$output"
		output_la=$func_basename_result
		save_libobjs=$libobjs
		save_output=$output
		output=$output_objdir/$output_la.nm
		func_to_tool_file "$output"
		libobjs=$nm_file_list_spec$func_to_tool_file_result
		func_append delfiles " $output"
		func_verbose "creating $NM input file list: $output"
		for obj in $save_libobjs; do
		  func_to_tool_file "$obj"
		  $ECHO "$func_to_tool_file_result"
		done > "$output"
		eval cmd=\"$cmd1\"
		func_show_eval "$cmd" 'exit $?'
		output=$save_output
		libobjs=$save_libobjs
		skipped_export=false
	      else
		func_verbose "using reloadable object file for export list..."
		skipped_export=:
		break
	      fi
	    done
	    IFS=$save_ifs
	    if test -n "$export_symbols_regex" && test : != "$skipped_export"; then
	      func_show_eval '$EGREP -e "$export_symbols_regex" "$export_symbols" > "${export_symbols}T"'
	      func_show_eval '$MV "${export_symbols}T" "$export_symbols"'
	    fi
	  fi
	fi
	if test -n "$export_symbols" && test -n "$include_expsyms"; then
	  tmp_export_symbols=$export_symbols
	  test -n "$orig_export_symbols" && tmp_export_symbols=$orig_export_symbols
	  $opt_dry_run || eval '$ECHO "$include_expsyms" | $SP2NL >> "$tmp_export_symbols"'
	fi
	if test : != "$skipped_export" && test -n "$orig_export_symbols"; then
	  func_verbose "filter symbol list for '$libname.la' to tag DATA exports"
	  $opt_dry_run || $SED -e '/[ ,]DATA/!d;s,\(.*\)\([ \,].*\),s|^\1$|\1\2|,' < $export_symbols > $output_objdir/$libname.filter
	  func_append delfiles " $export_symbols $output_objdir/$libname.filter"
	  export_symbols=$output_objdir/$libname.def
	  $opt_dry_run || $SED -f $output_objdir/$libname.filter < $orig_export_symbols > $export_symbols
	fi
	tmp_deplibs=
	for test_deplib in $deplibs; do
	  case " $convenience " in
	  *" $test_deplib "*) ;;
	  *)
	    func_append tmp_deplibs " $test_deplib"
	    ;;
	  esac
	done
	deplibs=$tmp_deplibs
	if test -n "$convenience"; then
	  if test -n "$whole_archive_flag_spec" &&
	    test yes = "$compiler_needs_object" &&
	    test -z "$libobjs"; then
	    whole_archive_flag_spec=
	  fi
	  if test -n "$whole_archive_flag_spec"; then
	    save_libobjs=$libobjs
	    eval libobjs=\"\$libobjs $whole_archive_flag_spec\"
	    test "X$libobjs" = "X " && libobjs=
	  else
	    gentop=$output_objdir/${outputname}x
	    func_append generated " $gentop"
	    func_extract_archives $gentop $convenience
	    func_append libobjs " $func_extract_archives_result"
	    test "X$libobjs" = "X " && libobjs=
	  fi
	fi
	if test yes = "$thread_safe" && test -n "$thread_safe_flag_spec"; then
	  eval flag=\"$thread_safe_flag_spec\"
	  func_append linker_flags " $flag"
	fi
	if test relink = "$opt_mode"; then
	  $opt_dry_run || eval '(cd $output_objdir && $RM ${realname}U && $MV $realname ${realname}U)' || exit $?
	fi
	if test yes = "$module" && test -n "$module_cmds"; then
	  if test -n "$export_symbols" && test -n "$module_expsym_cmds"; then
	    eval test_cmds=\"$module_expsym_cmds\"
	    cmds=$module_expsym_cmds
	  else
	    eval test_cmds=\"$module_cmds\"
	    cmds=$module_cmds
	  fi
	else
	  if test -n "$export_symbols" && test -n "$archive_expsym_cmds"; then
	    eval test_cmds=\"$archive_expsym_cmds\"
	    cmds=$archive_expsym_cmds
	  else
	    eval test_cmds=\"$archive_cmds\"
	    cmds=$archive_cmds
	  fi
	fi
	if test : != "$skipped_export" &&
	   func_len " $test_cmds" &&
	   len=$func_len_result &&
	   test "$len" -lt "$max_cmd_len" || test "$max_cmd_len" -le -1; then
	  :
	else
	  if test -z "$convenience" || test -z "$whole_archive_flag_spec"; then
	    save_libobjs=$libobjs
	  fi
	  save_output=$output
	  func_basename "$output"
	  output_la=$func_basename_result
	  test_cmds=
	  concat_cmds=
	  objlist=
	  last_robj=
	  k=1
	  if test -n "$save_libobjs" && test : != "$skipped_export" && test yes = "$with_gnu_ld"; then
	    output=$output_objdir/$output_la.lnkscript
	    func_verbose "creating GNU ld script: $output"
	    echo 'INPUT (' > $output
	    for obj in $save_libobjs
	    do
	      func_to_tool_file "$obj"
	      $ECHO "$func_to_tool_file_result" >> $output
	    done
	    echo ')' >> $output
	    func_append delfiles " $output"
	    func_to_tool_file "$output"
	    output=$func_to_tool_file_result
	  elif test -n "$save_libobjs" && test : != "$skipped_export" && test -n "$file_list_spec"; then
	    output=$output_objdir/$output_la.lnk
	    func_verbose "creating linker input file list: $output"
	    : > $output
	    set x $save_libobjs
	    shift
	    firstobj=
	    if test yes = "$compiler_needs_object"; then
	      firstobj="$1 "
	      shift
	    fi
	    for obj
	    do
	      func_to_tool_file "$obj"
	      $ECHO "$func_to_tool_file_result" >> $output
	    done
	    func_append delfiles " $output"
	    func_to_tool_file "$output"
	    output=$firstobj\"$file_list_spec$func_to_tool_file_result\"
	  else
	    if test -n "$save_libobjs"; then
	      func_verbose "creating reloadable object files..."
	      output=$output_objdir/$output_la-$k.$objext
	      eval test_cmds=\"$reload_cmds\"
	      func_len " $test_cmds"
	      len0=$func_len_result
	      len=$len0
	      for obj in $save_libobjs
	      do
		func_len " $obj"
		func_arith $len + $func_len_result
		len=$func_arith_result
		if test -z "$objlist" ||
		   test "$len" -lt "$max_cmd_len"; then
		  func_append objlist " $obj"
		else
		  if test 1 -eq "$k"; then
		    reload_objs=$objlist
		    eval concat_cmds=\"$reload_cmds\"
		  else
		    reload_objs="$objlist $last_robj"
		    eval concat_cmds=\"\$concat_cmds~$reload_cmds~\$RM $last_robj\"
		  fi
		  last_robj=$output_objdir/$output_la-$k.$objext
		  func_arith $k + 1
		  k=$func_arith_result
		  output=$output_objdir/$output_la-$k.$objext
		  objlist=" $obj"
		  func_len " $last_robj"
		  func_arith $len0 + $func_len_result
		  len=$func_arith_result
		fi
	      done
	      test -z "$concat_cmds" || concat_cmds=$concat_cmds~
	      reload_objs="$objlist $last_robj"
	      eval concat_cmds=\"\$concat_cmds$reload_cmds\"
	      if test -n "$last_robj"; then
	        eval concat_cmds=\"\$concat_cmds~\$RM $last_robj\"
	      fi
	      func_append delfiles " $output"
	    else
	      output=
	    fi
	    ${skipped_export-false} && {
	      func_verbose "generating symbol list for '$libname.la'"
	      export_symbols=$output_objdir/$libname.exp
	      $opt_dry_run || $RM $export_symbols
	      libobjs=$output
	      test -z "$concat_cmds" || concat_cmds=$concat_cmds~
	      eval concat_cmds=\"\$concat_cmds$export_symbols_cmds\"
	      if test -n "$last_robj"; then
		eval concat_cmds=\"\$concat_cmds~\$RM $last_robj\"
	      fi
	    }
	    test -n "$save_libobjs" &&
	      func_verbose "creating a temporary reloadable object file: $output"
	    save_ifs=$IFS; IFS='~'
	    for cmd in $concat_cmds; do
	      IFS=$save_ifs
	      $opt_quiet || {
		  func_quote_arg expand,pretty "$cmd"
		  eval "func_echo $func_quote_arg_result"
	      }
	      $opt_dry_run || eval "$cmd" || {
		lt_exit=$?
		if test relink = "$opt_mode"; then
		  ( cd "$output_objdir" && \
		    $RM "${realname}T" && \
		    $MV "${realname}U" "$realname" )
		fi
		exit $lt_exit
	      }
	    done
	    IFS=$save_ifs
	    if test -n "$export_symbols_regex" && ${skipped_export-false}; then
	      func_show_eval '$EGREP -e "$export_symbols_regex" "$export_symbols" > "${export_symbols}T"'
	      func_show_eval '$MV "${export_symbols}T" "$export_symbols"'
	    fi
	  fi
          ${skipped_export-false} && {
	    if test -n "$export_symbols" && test -n "$include_expsyms"; then
	      tmp_export_symbols=$export_symbols
	      test -n "$orig_export_symbols" && tmp_export_symbols=$orig_export_symbols
	      $opt_dry_run || eval '$ECHO "$include_expsyms" | $SP2NL >> "$tmp_export_symbols"'
	    fi
	    if test -n "$orig_export_symbols"; then
	      func_verbose "filter symbol list for '$libname.la' to tag DATA exports"
	      $opt_dry_run || $SED -e '/[ ,]DATA/!d;s,\(.*\)\([ \,].*\),s|^\1$|\1\2|,' < $export_symbols > $output_objdir/$libname.filter
	      func_append delfiles " $export_symbols $output_objdir/$libname.filter"
	      export_symbols=$output_objdir/$libname.def
	      $opt_dry_run || $SED -f $output_objdir/$libname.filter < $orig_export_symbols > $export_symbols
	    fi
	  }
	  libobjs=$output
	  output=$save_output
	  if test -n "$convenience" && test -n "$whole_archive_flag_spec"; then
	    eval libobjs=\"\$libobjs $whole_archive_flag_spec\"
	    test "X$libobjs" = "X " && libobjs=
	  fi
	  if test yes = "$module" && test -n "$module_cmds"; then
	    if test -n "$export_symbols" && test -n "$module_expsym_cmds"; then
	      cmds=$module_expsym_cmds
	    else
	      cmds=$module_cmds
	    fi
	  else
	    if test -n "$export_symbols" && test -n "$archive_expsym_cmds"; then
	      cmds=$archive_expsym_cmds
	    else
	      cmds=$archive_cmds
	    fi
	  fi
	fi
	if test -n "$delfiles"; then
	  eval cmds=\"\$cmds~\$RM $delfiles\"
	fi
	if test -n "$dlprefiles"; then
	  gentop=$output_objdir/${outputname}x
	  func_append generated " $gentop"
	  func_extract_archives $gentop $dlprefiles
	  func_append libobjs " $func_extract_archives_result"
	  test "X$libobjs" = "X " && libobjs=
	fi
	save_ifs=$IFS; IFS='~'
	for cmd in $cmds; do
	  IFS=$sp$nl
	  eval cmd=\"$cmd\"
	  IFS=$save_ifs
	  $opt_quiet || {
	    func_quote_arg expand,pretty "$cmd"
	    eval "func_echo $func_quote_arg_result"
	  }
	  $opt_dry_run || eval "$cmd" || {
	    lt_exit=$?
	    if test relink = "$opt_mode"; then
	      ( cd "$output_objdir" && \
	        $RM "${realname}T" && \
		$MV "${realname}U" "$realname" )
	    fi
	    exit $lt_exit
	  }
	done
	IFS=$save_ifs
	if test relink = "$opt_mode"; then
	  $opt_dry_run || eval '(cd $output_objdir && $RM ${realname}T && $MV $realname ${realname}T && $MV ${realname}U $realname)' || exit $?
	  if test -n "$convenience"; then
	    if test -z "$whole_archive_flag_spec"; then
	      func_show_eval '${RM}r "$gentop"'
	    fi
	  fi
	  exit $EXIT_SUCCESS
	fi
	for linkname in $linknames; do
	  if test "$realname" != "$linkname"; then
	    func_show_eval '(cd "$output_objdir" && $RM "$linkname" && $LN_S "$realname" "$linkname")' 'exit $?'
	  fi
	done
	if test yes = "$module" || test yes = "$export_dynamic"; then
	  dlname=$soname
	fi
      fi
      ;;
    obj)
      if test -n "$dlfiles$dlprefiles" || test no != "$dlself"; then
	func_warning "'-dlopen' is ignored for objects"
      fi
      case " $deplibs" in
      *\ -l* | *\ -L*)
	func_warning "'-l' and '-L' are ignored for objects" ;;
      esac
      test -n "$rpath" && \
	func_warning "'-rpath' is ignored for objects"
      test -n "$xrpath" && \
	func_warning "'-R' is ignored for objects"
      test -n "$vinfo" && \
	func_warning "'-version-info' is ignored for objects"
      test -n "$release" && \
	func_warning "'-release' is ignored for objects"
      case $output in
      *.lo)
	test -n "$objs$old_deplibs" && \
	  func_fatal_error "cannot build library object '$output' from non-libtool objects"
	libobj=$output
	func_lo2o "$libobj"
	obj=$func_lo2o_result
	;;
      *)
	libobj=
	obj=$output
	;;
      esac
      $opt_dry_run || $RM $obj $libobj
      reload_conv_objs=
      gentop=
      case $reload_cmds in
        *\$LD[\ \$]*) wl= ;;
      esac
      if test -n "$convenience"; then
	if test -n "$whole_archive_flag_spec"; then
	  eval tmp_whole_archive_flags=\"$whole_archive_flag_spec\"
	  test -n "$wl" || tmp_whole_archive_flags=`$ECHO "$tmp_whole_archive_flags" | $SED 's|,| |g'`
	  reload_conv_objs=$reload_objs\ $tmp_whole_archive_flags
	else
	  gentop=$output_objdir/${obj}x
	  func_append generated " $gentop"
	  func_extract_archives $gentop $convenience
	  reload_conv_objs="$reload_objs $func_extract_archives_result"
	fi
      fi
      test yes = "$build_libtool_libs" || libobjs=$non_pic_objects
      reload_objs=$objs$old_deplibs' '`$ECHO "$libobjs" | $SP2NL | $SED "/\.$libext$/d; /\.lib$/d; $lo2o" | $NL2SP`' '$reload_conv_objs
      output=$obj
      func_execute_cmds "$reload_cmds" 'exit $?'
      if test -z "$libobj"; then
	if test -n "$gentop"; then
	  func_show_eval '${RM}r "$gentop"'
	fi
	exit $EXIT_SUCCESS
      fi
      test yes = "$build_libtool_libs" || {
	if test -n "$gentop"; then
	  func_show_eval '${RM}r "$gentop"'
	fi
	exit $EXIT_SUCCESS
      }
      if test -n "$pic_flag" || test default != "$pic_mode"; then
	reload_objs="$libobjs $reload_conv_objs"
	output=$libobj
	func_execute_cmds "$reload_cmds" 'exit $?'
      fi
      if test -n "$gentop"; then
	func_show_eval '${RM}r "$gentop"'
      fi
      exit $EXIT_SUCCESS
      ;;
    prog)
      case $host in
	*cygwin*) func_stripname '' '.exe' "$output"
	          output=$func_stripname_result.exe;;
      esac
      test -n "$vinfo" && \
	func_warning "'-version-info' is ignored for programs"
      test -n "$release" && \
	func_warning "'-release' is ignored for programs"
      $preload \
	&& test unknown,unknown,unknown = "$dlopen_support,$dlopen_self,$dlopen_self_static" \
	&& func_warning "'LT_INIT([dlopen])' not used. Assuming no dlopen support."
      case $host in
      *-*-rhapsody* | *-*-darwin1.[012])
	compile_deplibs=`$ECHO " $compile_deplibs" | $SED 's/ -lc / System.ltframework /'`
	finalize_deplibs=`$ECHO " $finalize_deplibs" | $SED 's/ -lc / System.ltframework /'`
	;;
      esac
      case $host in
      *-*-darwin*)
	if test CXX = "$tagname"; then
	  case ${MACOSX_DEPLOYMENT_TARGET-10.0} in
	    10.[0123])
	      func_append compile_command " $wl-bind_at_load"
	      func_append finalize_command " $wl-bind_at_load"
	    ;;
	  esac
	fi
	compile_deplibs=`$ECHO " $compile_deplibs" | $SED 's% \([^ $]*\).ltframework% -framework \1%g'`
	finalize_deplibs=`$ECHO " $finalize_deplibs" | $SED 's% \([^ $]*\).ltframework% -framework \1%g'`
	;;
      esac
      new_libs=
      for path in $notinst_path; do
	case " $new_libs " in
	*" -L$path/$objdir "*) ;;
	*)
	  case " $compile_deplibs " in
	  *" -L$path/$objdir "*)
	    func_append new_libs " -L$path/$objdir" ;;
	  esac
	  ;;
	esac
      done
      for deplib in $compile_deplibs; do
	case $deplib in
	-L*)
	  case " $new_libs " in
	  *" $deplib "*) ;;
	  *) func_append new_libs " $deplib" ;;
	  esac
	  ;;
	*) func_append new_libs " $deplib" ;;
	esac
      done
      compile_deplibs=$new_libs
      func_append compile_command " $compile_deplibs"
      func_append finalize_command " $finalize_deplibs"
      if test -n "$rpath$xrpath"; then
	for libdir in $rpath $xrpath; do
	  case "$finalize_rpath " in
	  *" $libdir "*) ;;
	  *) func_append finalize_rpath " $libdir" ;;
	  esac
	done
      fi
      rpath=
      hardcode_libdirs=
      for libdir in $compile_rpath $finalize_rpath; do
	if test -n "$hardcode_libdir_flag_spec"; then
	  if test -n "$hardcode_libdir_separator"; then
	    if test -z "$hardcode_libdirs"; then
	      hardcode_libdirs=$libdir
	    else
	      case $hardcode_libdir_separator$hardcode_libdirs$hardcode_libdir_separator in
	      *"$hardcode_libdir_separator$libdir$hardcode_libdir_separator"*)
		;;
	      *)
		func_append hardcode_libdirs "$hardcode_libdir_separator$libdir"
		;;
	      esac
	    fi
	  else
	    eval flag=\"$hardcode_libdir_flag_spec\"
	    func_append rpath " $flag"
	  fi
	elif test -n "$runpath_var"; then
	  case "$perm_rpath " in
	  *" $libdir "*) ;;
	  *) func_append perm_rpath " $libdir" ;;
	  esac
	fi
	case $host in
	*-*-cygwin* | *-*-mingw* | *-*-pw32* | *-*-os2* | *-cegcc*)
	  testbindir=`$ECHO "$libdir" | $SED -e 's*/lib$*/bin*'`
	  case :$dllsearchpath: in
	  *":$libdir:"*) ;;
	  ::) dllsearchpath=$libdir;;
	  *) func_append dllsearchpath ":$libdir";;
	  esac
	  case :$dllsearchpath: in
	  *":$testbindir:"*) ;;
	  ::) dllsearchpath=$testbindir;;
	  *) func_append dllsearchpath ":$testbindir";;
	  esac
	  ;;
	esac
      done
      if test -n "$hardcode_libdir_separator" &&
	 test -n "$hardcode_libdirs"; then
	libdir=$hardcode_libdirs
	eval rpath=\" $hardcode_libdir_flag_spec\"
      fi
      compile_rpath=$rpath
      rpath=
      hardcode_libdirs=
      for libdir in $finalize_rpath; do
	if test -n "$hardcode_libdir_flag_spec"; then
	  if test -n "$hardcode_libdir_separator"; then
	    if test -z "$hardcode_libdirs"; then
	      hardcode_libdirs=$libdir
	    else
	      case $hardcode_libdir_separator$hardcode_libdirs$hardcode_libdir_separator in
	      *"$hardcode_libdir_separator$libdir$hardcode_libdir_separator"*)
		;;
	      *)
		func_append hardcode_libdirs "$hardcode_libdir_separator$libdir"
		;;
	      esac
	    fi
	  else
	    eval flag=\"$hardcode_libdir_flag_spec\"
	    func_append rpath " $flag"
	  fi
	elif test -n "$runpath_var"; then
	  case "$finalize_perm_rpath " in
	  *" $libdir "*) ;;
	  *) func_append finalize_perm_rpath " $libdir" ;;
	  esac
	fi
      done
      if test -n "$hardcode_libdir_separator" &&
	 test -n "$hardcode_libdirs"; then
	libdir=$hardcode_libdirs
	eval rpath=\" $hardcode_libdir_flag_spec\"
      fi
      finalize_rpath=$rpath
      if test -n "$libobjs" && test yes = "$build_old_libs"; then
	compile_command=`$ECHO "$compile_command" | $SP2NL | $SED "$lo2o" | $NL2SP`
	finalize_command=`$ECHO "$finalize_command" | $SP2NL | $SED "$lo2o" | $NL2SP`
      fi
      func_generate_dlsyms "$outputname" "@PROGRAM@" false
      if test -n "$prelink_cmds"; then
	func_execute_cmds "$prelink_cmds" 'exit $?'
      fi
      wrappers_required=:
      case $host in
      *cegcc* | *mingw32ce*)
        wrappers_required=false
        ;;
      *cygwin* | *mingw* )
        test yes = "$build_libtool_libs" || wrappers_required=false
        ;;
      *)
        if test no = "$need_relink" || test yes != "$build_libtool_libs"; then
          wrappers_required=false
        fi
        ;;
      esac
      $wrappers_required || {
	compile_command=`$ECHO "$compile_command" | $SED 's%@OUTPUT@%'"$output"'%g'`
	link_command=$compile_command$compile_rpath
	exit_status=0
	func_show_eval "$link_command" 'exit_status=$?'
	if test -n "$postlink_cmds"; then
	  func_to_tool_file "$output"
	  postlink_cmds=`func_echo_all "$postlink_cmds" | $SED -e 's%@OUTPUT@%'"$output"'%g' -e 's%@TOOL_OUTPUT@%'"$func_to_tool_file_result"'%g'`
	  func_execute_cmds "$postlink_cmds" 'exit $?'
	fi
	if test -f "$output_objdir/${outputname}S.$objext"; then
	  func_show_eval '$RM "$output_objdir/${outputname}S.$objext"'
	fi
	exit $exit_status
      }
      if test -n "$compile_shlibpath$finalize_shlibpath"; then
	compile_command="$shlibpath_var=\"$compile_shlibpath$finalize_shlibpath\$$shlibpath_var\" $compile_command"
      fi
      if test -n "$finalize_shlibpath"; then
	finalize_command="$shlibpath_var=\"$finalize_shlibpath\$$shlibpath_var\" $finalize_command"
      fi
      compile_var=
      finalize_var=
      if test -n "$runpath_var"; then
	if test -n "$perm_rpath"; then
	  rpath=
	  for dir in $perm_rpath; do
	    func_append rpath "$dir:"
	  done
	  compile_var="$runpath_var=\"$rpath\$$runpath_var\" "
	fi
	if test -n "$finalize_perm_rpath"; then
	  rpath=
	  for dir in $finalize_perm_rpath; do
	    func_append rpath "$dir:"
	  done
	  finalize_var="$runpath_var=\"$rpath\$$runpath_var\" "
	fi
      fi
      if test yes = "$no_install"; then
	link_command=$compile_var$compile_command$compile_rpath
	link_command=`$ECHO "$link_command" | $SED 's%@OUTPUT@%'"$output"'%g'`
	$opt_dry_run || $RM $output
	func_show_eval "$link_command" 'exit $?'
	if test -n "$postlink_cmds"; then
	  func_to_tool_file "$output"
	  postlink_cmds=`func_echo_all "$postlink_cmds" | $SED -e 's%@OUTPUT@%'"$output"'%g' -e 's%@TOOL_OUTPUT@%'"$func_to_tool_file_result"'%g'`
	  func_execute_cmds "$postlink_cmds" 'exit $?'
	fi
	exit $EXIT_SUCCESS
      fi
      case $hardcode_action,$fast_install in
        relink,*)
	  link_command=$compile_var$compile_command$compile_rpath
	  relink_command=$finalize_var$finalize_command$finalize_rpath
	  func_warning "this platform does not like uninstalled shared libraries"
	  func_warning "'$output' will be relinked during installation"
	  ;;
        *,yes)
	  link_command=$finalize_var$compile_command$finalize_rpath
	  relink_command=`$ECHO "$compile_var$compile_command$compile_rpath" | $SED 's%@OUTPUT@%\$progdir/\$file%g'`
          ;;
	*,no)
	  link_command=$compile_var$compile_command$compile_rpath
	  relink_command=$finalize_var$finalize_command$finalize_rpath
          ;;
	*,needless)
	  link_command=$finalize_var$compile_command$finalize_rpath
	  relink_command=
          ;;
      esac
      link_command=`$ECHO "$link_command" | $SED 's%@OUTPUT@%'"$output_objdir/$outputname"'%g'`
      $opt_dry_run || $RM $output $output_objdir/$outputname $output_objdir/lt-$outputname
      func_show_eval "$link_command" 'exit $?'
      if test -n "$postlink_cmds"; then
	func_to_tool_file "$output_objdir/$outputname"
	postlink_cmds=`func_echo_all "$postlink_cmds" | $SED -e 's%@OUTPUT@%'"$output_objdir/$outputname"'%g' -e 's%@TOOL_OUTPUT@%'"$func_to_tool_file_result"'%g'`
	func_execute_cmds "$postlink_cmds" 'exit $?'
      fi
      func_verbose "creating $output"
      if test -n "$relink_command"; then
	for var in $variables_saved_for_relink; do
	  if eval test -z \"\${$var+set}\"; then
	    relink_command="{ test -z \"\${$var+set}\" || $lt_unset $var || { $var=; export $var; }; }; $relink_command"
	  elif eval var_value=\$$var; test -z "$var_value"; then
	    relink_command="$var=; export $var; $relink_command"
	  else
	    func_quote_arg pretty "$var_value"
	    relink_command="$var=$func_quote_arg_result; export $var; $relink_command"
	  fi
	done
	func_quote eval cd "`pwd`"
	func_quote_arg pretty,unquoted "($func_quote_result; $relink_command)"
	relink_command=$func_quote_arg_unquoted_result
      fi
      $opt_dry_run || {
	case $output in
	  *.exe) func_stripname '' '.exe' "$output"
	         output=$func_stripname_result ;;
	esac
	case $host in
	  *cygwin*)
	    exeext=.exe
	    func_stripname '' '.exe' "$outputname"
	    outputname=$func_stripname_result ;;
	  *) exeext= ;;
	esac
	case $host in
	  *cygwin* | *mingw* )
	    func_dirname_and_basename "$output" "" "."
	    output_name=$func_basename_result
	    output_path=$func_dirname_result
	    cwrappersource=$output_path/$objdir/lt-$output_name.c
	    cwrapper=$output_path/$output_name.exe
	    $RM $cwrappersource $cwrapper
	    trap "$RM $cwrappersource $cwrapper; exit $EXIT_FAILURE" 1 2 15
	    func_emit_cwrapperexe_src > $cwrappersource
	    $opt_dry_run || {
	      $LTCC $LTCFLAGS -o $cwrapper $cwrappersource
	      $STRIP $cwrapper
	    }
	    func_ltwrapper_scriptname $cwrapper
	    $RM $func_ltwrapper_scriptname_result
	    trap "$RM $func_ltwrapper_scriptname_result; exit $EXIT_FAILURE" 1 2 15
	    $opt_dry_run || {
	      if test "x$build" = "x$host"; then
		$cwrapper --lt-dump-script > $func_ltwrapper_scriptname_result
	      else
		func_emit_wrapper no > $func_ltwrapper_scriptname_result
	      fi
	    }
	  ;;
	  * )
	    $RM $output
	    trap "$RM $output; exit $EXIT_FAILURE" 1 2 15
	    func_emit_wrapper no > $output
	    chmod +x $output
	  ;;
	esac
      }
      exit $EXIT_SUCCESS
      ;;
    esac
    for oldlib in $oldlibs; do
      case $build_libtool_libs in
        convenience)
	  oldobjs="$libobjs_save $symfileobj"
	  addlibs=$convenience
	  build_libtool_libs=no
	  ;;
	module)
	  oldobjs=$libobjs_save
	  addlibs=$old_convenience
	  build_libtool_libs=no
          ;;
	*)
	  oldobjs="$old_deplibs $non_pic_objects"
	  $preload && test -f "$symfileobj" \
	    && func_append oldobjs " $symfileobj"
	  addlibs=$old_convenience
	  ;;
      esac
      if test -n "$addlibs"; then
	gentop=$output_objdir/${outputname}x
	func_append generated " $gentop"
	func_extract_archives $gentop $addlibs
	func_append oldobjs " $func_extract_archives_result"
      fi
      if test -n "$old_archive_from_new_cmds" && test yes = "$build_libtool_libs"; then
	cmds=$old_archive_from_new_cmds
      else
	if test -n "$dlprefiles"; then
	  gentop=$output_objdir/${outputname}x
	  func_append generated " $gentop"
	  func_extract_archives $gentop $dlprefiles
	  func_append oldobjs " $func_extract_archives_result"
	fi
	if (for obj in $oldobjs
	    do
	      func_basename "$obj"
	      $ECHO "$func_basename_result"
	    done | sort | sort -uc >/dev/null 2>&1); then
	  :
	else
	  echo "copying selected object files to avoid basename conflicts..."
	  gentop=$output_objdir/${outputname}x
	  func_append generated " $gentop"
	  func_mkdir_p "$gentop"
	  save_oldobjs=$oldobjs
	  oldobjs=
	  counter=1
	  for obj in $save_oldobjs
	  do
	    func_basename "$obj"
	    objbase=$func_basename_result
	    case " $oldobjs " in
	    " ") oldobjs=$obj ;;
	    *[\ /]"$objbase "*)
	      while :; do
		newobj=lt$counter-$objbase
		func_arith $counter + 1
		counter=$func_arith_result
		case " $oldobjs " in
		*[\ /]"$newobj "*) ;;
		*) if test ! -f "$gentop/$newobj"; then break; fi ;;
		esac
	      done
	      func_show_eval "ln $obj $gentop/$newobj || cp $obj $gentop/$newobj"
	      func_append oldobjs " $gentop/$newobj"
	      ;;
	    *) func_append oldobjs " $obj" ;;
	    esac
	  done
	fi
	func_to_tool_file "$oldlib" func_convert_file_msys_to_w32
	tool_oldlib=$func_to_tool_file_result
	eval cmds=\"$old_archive_cmds\"
	func_len " $cmds"
	len=$func_len_result
	if test "$len" -lt "$max_cmd_len" || test "$max_cmd_len" -le -1; then
	  cmds=$old_archive_cmds
	elif test -n "$archiver_list_spec"; then
	  func_verbose "using command file archive linking..."
	  for obj in $oldobjs
	  do
	    func_to_tool_file "$obj"
	    $ECHO "$func_to_tool_file_result"
	  done > $output_objdir/$libname.libcmd
	  func_to_tool_file "$output_objdir/$libname.libcmd"
	  oldobjs=" $archiver_list_spec$func_to_tool_file_result"
	  cmds=$old_archive_cmds
	else
	  func_verbose "using piecewise archive linking..."
	  save_RANLIB=$RANLIB
	  RANLIB=:
	  objlist=
	  concat_cmds=
	  save_oldobjs=$oldobjs
	  oldobjs=
	  for obj in $save_oldobjs
	  do
	    last_oldobj=$obj
	  done
	  eval test_cmds=\"$old_archive_cmds\"
	  func_len " $test_cmds"
	  len0=$func_len_result
	  len=$len0
	  for obj in $save_oldobjs
	  do
	    func_len " $obj"
	    func_arith $len + $func_len_result
	    len=$func_arith_result
	    func_append objlist " $obj"
	    if test "$len" -lt "$max_cmd_len"; then
	      :
	    else
	      oldobjs=$objlist
	      if test "$obj" = "$last_oldobj"; then
		RANLIB=$save_RANLIB
	      fi
	      test -z "$concat_cmds" || concat_cmds=$concat_cmds~
	      eval concat_cmds=\"\$concat_cmds$old_archive_cmds\"
	      objlist=
	      len=$len0
	    fi
	  done
	  RANLIB=$save_RANLIB
	  oldobjs=$objlist
	  if test -z "$oldobjs"; then
	    eval cmds=\"\$concat_cmds\"
	  else
	    eval cmds=\"\$concat_cmds~\$old_archive_cmds\"
	  fi
	fi
      fi
      func_execute_cmds "$cmds" 'exit $?'
    done
    test -n "$generated" && \
      func_show_eval "${RM}r$generated"
    case $output in
    *.la)
      old_library=
      test yes = "$build_old_libs" && old_library=$libname.$libext
      func_verbose "creating $output"
      for var in $variables_saved_for_relink; do
	if eval test -z \"\${$var+set}\"; then
	  relink_command="{ test -z \"\${$var+set}\" || $lt_unset $var || { $var=; export $var; }; }; $relink_command"
	elif eval var_value=\$$var; test -z "$var_value"; then
	  relink_command="$var=; export $var; $relink_command"
	else
	  func_quote_arg pretty,unquoted "$var_value"
	  relink_command="$var=$func_quote_arg_unquoted_result; export $var; $relink_command"
	fi
      done
      func_quote eval cd "`pwd`"
      relink_command="($func_quote_result; $SHELL \"$progpath\" $preserve_args --mode=relink $libtool_args @inst_prefix_dir@)"
      func_quote_arg pretty,unquoted "$relink_command"
      relink_command=$func_quote_arg_unquoted_result
      if test yes = "$hardcode_automatic"; then
	relink_command=
      fi
      $opt_dry_run || {
	for installed in no yes; do
	  if test yes = "$installed"; then
	    if test -z "$install_libdir"; then
	      break
	    fi
	    output=$output_objdir/${outputname}i
	    newdependency_libs=
	    for deplib in $dependency_libs; do
	      case $deplib in
	      *.la)
		func_basename "$deplib"
		name=$func_basename_result
		func_resolve_sysroot "$deplib"
		eval libdir=`$SED -n -e 's/^libdir=\(.*\)$/\1/p' $func_resolve_sysroot_result`
		test -z "$libdir" && \
		  func_fatal_error "'$deplib' is not a valid libtool archive"
		func_append newdependency_libs " ${lt_sysroot:+=}$libdir/$name"
		;;
	      -L*)
		func_stripname -L '' "$deplib"
		func_replace_sysroot "$func_stripname_result"
		func_append newdependency_libs " -L$func_replace_sysroot_result"
		;;
	      -R*)
		func_stripname -R '' "$deplib"
		func_replace_sysroot "$func_stripname_result"
		func_append newdependency_libs " -R$func_replace_sysroot_result"
		;;
	      *) func_append newdependency_libs " $deplib" ;;
	      esac
	    done
	    dependency_libs=$newdependency_libs
	    newdlfiles=
	    for lib in $dlfiles; do
	      case $lib in
	      *.la)
	        func_basename "$lib"
		name=$func_basename_result
		eval libdir=`$SED -n -e 's/^libdir=\(.*\)$/\1/p' $lib`
		test -z "$libdir" && \
		  func_fatal_error "'$lib' is not a valid libtool archive"
		func_append newdlfiles " ${lt_sysroot:+=}$libdir/$name"
		;;
	      *) func_append newdlfiles " $lib" ;;
	      esac
	    done
	    dlfiles=$newdlfiles
	    newdlprefiles=
	    for lib in $dlprefiles; do
	      case $lib in
	      *.la)
		func_basename "$lib"
		name=$func_basename_result
		eval libdir=`$SED -n -e 's/^libdir=\(.*\)$/\1/p' $lib`
		test -z "$libdir" && \
		  func_fatal_error "'$lib' is not a valid libtool archive"
		func_append newdlprefiles " ${lt_sysroot:+=}$libdir/$name"
		;;
	      esac
	    done
	    dlprefiles=$newdlprefiles
	  else
	    newdlfiles=
	    for lib in $dlfiles; do
	      case $lib in
		[\\/]* | [A-Za-z]:[\\/]*) abs=$lib ;;
		*) abs=`pwd`"/$lib" ;;
	      esac
	      func_append newdlfiles " $abs"
	    done
	    dlfiles=$newdlfiles
	    newdlprefiles=
	    for lib in $dlprefiles; do
	      case $lib in
		[\\/]* | [A-Za-z]:[\\/]*) abs=$lib ;;
		*) abs=`pwd`"/$lib" ;;
	      esac
	      func_append newdlprefiles " $abs"
	    done
	    dlprefiles=$newdlprefiles
	  fi
	  $RM $output
	  tdlname=$dlname
	  case $host,$output,$installed,$module,$dlname in
	    *cygwin*,*lai,yes,no,*.dll | *mingw*,*lai,yes,no,*.dll | *cegcc*,*lai,yes,no,*.dll)
	      if test -n "$bindir"; then
		func_relative_path "$install_libdir" "$bindir"
		tdlname=$func_relative_path_result/$dlname
	      else
		tdlname=../bin/$dlname
	      fi
	      ;;
	  esac
	  $ECHO > $output "\
dlname='$tdlname'
library_names='$library_names'
old_library='$old_library'
inherited_linker_flags='$new_inherited_linker_flags'
dependency_libs='$dependency_libs'
weak_library_names='$weak_libs'
current=$current
age=$age
revision=$revision
installed=$installed
shouldnotlink=$module
dlopen='$dlfiles'
dlpreopen='$dlprefiles'
libdir='$install_libdir'"
	  if test no,yes = "$installed,$need_relink"; then
	    $ECHO >> $output "\
relink_command=\"$relink_command\""
	  fi
	done
      }
      func_show_eval '( cd "$output_objdir" && $RM "$outputname" && $LN_S "../$outputname" "$outputname" )' 'exit $?'
      ;;
    esac
    exit $EXIT_SUCCESS
}
if test link = "$opt_mode" || test relink = "$opt_mode"; then
  func_mode_link ${1+"$@"}
fi
func_mode_uninstall ()
{
    $debug_cmd
    RM=$nonopt
    files=
    rmforce=false
    exit_status=0
    libtool_install_magic=$magic
    for arg
    do
      case $arg in
      -f) func_append RM " $arg"; rmforce=: ;;
      -*) func_append RM " $arg" ;;
      *) func_append files " $arg" ;;
      esac
    done
    test -z "$RM" && \
      func_fatal_help "you must specify an RM program"
    rmdirs=
    for file in $files; do
      func_dirname "$file" "" "."
      dir=$func_dirname_result
      if test . = "$dir"; then
	odir=$objdir
      else
	odir=$dir/$objdir
      fi
      func_basename "$file"
      name=$func_basename_result
      test uninstall = "$opt_mode" && odir=$dir
      if test clean = "$opt_mode"; then
	case " $rmdirs " in
	  *" $odir "*) ;;
	  *) func_append rmdirs " $odir" ;;
	esac
      fi
      if { test -L "$file"; } >/dev/null 2>&1 ||
	 { test -h "$file"; } >/dev/null 2>&1 ||
	 test -f "$file"; then
	:
      elif test -d "$file"; then
	exit_status=1
	continue
      elif $rmforce; then
	continue
      fi
      rmfiles=$file
      case $name in
      *.la)
	if func_lalib_p "$file"; then
	  func_source $dir/$name
	  for n in $library_names; do
	    func_append rmfiles " $odir/$n"
	  done
	  test -n "$old_library" && func_append rmfiles " $odir/$old_library"
	  case $opt_mode in
	  clean)
	    case " $library_names " in
	    *" $dlname "*) ;;
	    *) test -n "$dlname" && func_append rmfiles " $odir/$dlname" ;;
	    esac
	    test -n "$libdir" && func_append rmfiles " $odir/$name $odir/${name}i"
	    ;;
	  uninstall)
	    if test -n "$library_names"; then
	      func_execute_cmds "$postuninstall_cmds" '$rmforce || exit_status=1'
	    fi
	    if test -n "$old_library"; then
	      func_execute_cmds "$old_postuninstall_cmds" '$rmforce || exit_status=1'
	    fi
	    ;;
	  esac
	fi
	;;
      *.lo)
	if func_lalib_p "$file"; then
	  func_source $dir/$name
	  if test -n "$pic_object" && test none != "$pic_object"; then
	    func_append rmfiles " $dir/$pic_object"
	  fi
	  if test -n "$non_pic_object" && test none != "$non_pic_object"; then
	    func_append rmfiles " $dir/$non_pic_object"
	  fi
	fi
	;;
      *)
	if test clean = "$opt_mode"; then
	  noexename=$name
	  case $file in
	  *.exe)
	    func_stripname '' '.exe' "$file"
	    file=$func_stripname_result
	    func_stripname '' '.exe' "$name"
	    noexename=$func_stripname_result
	    func_append rmfiles " $file"
	    ;;
	  esac
	  if func_ltwrapper_p "$file"; then
	    if func_ltwrapper_executable_p "$file"; then
	      func_ltwrapper_scriptname "$file"
	      relink_command=
	      func_source $func_ltwrapper_scriptname_result
	      func_append rmfiles " $func_ltwrapper_scriptname_result"
	    else
	      relink_command=
	      func_source $dir/$noexename
	    fi
	    func_append rmfiles " $odir/$name $odir/${name}S.$objext"
	    if test yes = "$fast_install" && test -n "$relink_command"; then
	      func_append rmfiles " $odir/lt-$name"
	    fi
	    if test "X$noexename" != "X$name"; then
	      func_append rmfiles " $odir/lt-$noexename.c"
	    fi
	  fi
	fi
	;;
      esac
      func_show_eval "$RM $rmfiles" 'exit_status=1'
    done
    for dir in $rmdirs; do
      if test -d "$dir"; then
	func_show_eval "rmdir $dir >/dev/null 2>&1"
      fi
    done
    exit $exit_status
}
if test uninstall = "$opt_mode" || test clean = "$opt_mode"; then
  func_mode_uninstall ${1+"$@"}
fi
test -z "$opt_mode" && {
  help=$generic_help
  func_fatal_help "you must specify a MODE"
}
test -z "$exec_cmd" && \
  func_fatal_help "invalid operation mode '$opt_mode'"
if test -n "$exec_cmd"; then
  eval exec "$exec_cmd"
  exit $EXIT_FAILURE
fi
exit $exit_status
build_libtool_libs=no
build_old_libs=yes
build_old_libs=`case $build_libtool_libs in yes) echo no;; *) echo yes;; esac`
