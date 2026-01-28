. ./.config || exit 1
debug=false
postcompile=false
test x"$1" = x"--post" && { postcompile=true; shift; }
common_bufsiz_h=$1
test x"$NM" = x"" && NM="${CONFIG_CROSS_COMPILER_PREFIX}nm"
test x"$CC" = x"" && CC="${CONFIG_CROSS_COMPILER_PREFIX}gcc"
exitcmd="exit 0"
regenerate() {
	cat >"$1.$$"
	test -f "$1" && diff "$1.$$" "$1" >/dev/null && rm "$1.$$" && return
	mv "$1.$$" "$1"
}
generate_std_and_exit() {
	$debug && echo "Configuring: bb_common_bufsiz1[] in bss"
	{
	echo "enum { COMMON_BUFSIZE = 1024 };"
	echo "extern char bb_common_bufsiz1[];"
	echo "
	} | regenerate "$common_bufsiz_h"
	echo "std" >"$common_bufsiz_h.method"
	$exitcmd
}
generate_big_and_exit() {
	$debug && echo "Configuring: bb_common_bufsiz1[] in bss, COMMON_BUFSIZE = $1"
	{
	echo "enum { COMMON_BUFSIZE = $1 };"
	echo "extern char bb_common_bufsiz1[];"
	echo "
	} | regenerate "$common_bufsiz_h"
	echo "$2" >"$common_bufsiz_h.method"
	$exitcmd
}
generate_1k_and_exit() {
	generate_big_and_exit 1024 "1k"
}
round_down_COMMON_BUFSIZE() {
	COMMON_BUFSIZE=1024
	test "$1" -le 32 && return
	COMMON_BUFSIZE=$(( ($1-32) & 0x0ffffff0 ))
	COMMON_BUFSIZE=$(( COMMON_BUFSIZE < 1024 ? 1024 : COMMON_BUFSIZE ))
}
test x"$CONFIG_FEATURE_USE_BSS_TAIL" = x"y" || generate_std_and_exit
if $postcompile; then
	test -f busybox_unstripped || exit 1
	test -f "$common_bufsiz_h.method" || exit 1
	method=`cat -- "$common_bufsiz_h.method"`
	END=`$NM busybox_unstripped | grep ' . _end$'| cut -d' ' -f1`
	test x"$END" = x"" && generate_std_and_exit
	$debug && echo "END:0x$END $((0x$END))"
	END=$((0x$END))
	{
	echo "
	echo "
	echo "char page_size[PAGE_SIZE];"
	echo "
	} >page_size_$$.c
	$CC -c "page_size_$$.c" || exit 1
	PAGE_SIZE=`$NM --size-sort "page_size_$$.o" | cut -d' ' -f1`
	rm "page_size_$$.c" "page_size_$$.o"
	test x"$PAGE_SIZE" = x"" && exit 1
	$debug && echo "PAGE_SIZE:0x$PAGE_SIZE $((0x$PAGE_SIZE))"
	PAGE_SIZE=$((0x$PAGE_SIZE))
	test $PAGE_SIZE -lt 1024 && exit 1
	PAGE_MASK=$((PAGE_SIZE-1))
	TAIL_SIZE=$(( (-END) & PAGE_MASK ))
	$debug && echo "TAIL_SIZE:$TAIL_SIZE bytes"
	if test x"$method" = x"1k"; then
		{
		echo $TAIL_SIZE
		md5sum <.config | cut -d' ' -f1
		stat -c "%Y" .config
		} >"$common_bufsiz_h.1k.OK"
		round_down_COMMON_BUFSIZE $((1024 + TAIL_SIZE))
		test $COMMON_BUFSIZE -gt 1024 \
			&& echo "Rerun make to use larger COMMON_BUFSIZE ($COMMON_BUFSIZE)"
		test $COMMON_BUFSIZE = 1024 && generate_1k_and_exit
		generate_big_and_exit $COMMON_BUFSIZE "big"
	fi
fi
if test -f "$common_bufsiz_h.1k.OK"; then
	oldcfg=`tail -n2 -- "$common_bufsiz_h.1k.OK"`
	curcfg=`md5sum <.config | cut -d' ' -f1; stat -c "%Y" .config`
	if test x"$oldcfg" = x"$curcfg"; then
		TAIL_SIZE=`head -n1 -- "$common_bufsiz_h.1k.OK"`
		round_down_COMMON_BUFSIZE $((1024 + TAIL_SIZE))
		test $COMMON_BUFSIZE = 1024 && generate_1k_and_exit
		generate_big_and_exit $COMMON_BUFSIZE "big"
	fi
	rm -rf -- "$common_bufsiz_h.1k.OK"
fi
generate_1k_and_exit
