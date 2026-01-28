case $1 in
	-h|--help)
		echo -e "$0 [-j <n>]"
		echo -e "\tTest the different ways of building bpftool."
		echo -e ""
		echo -e "\tOptions:"
		echo -e "\t\t-j <n>:\tPass -j flag to 'make'."
		exit 0
		;;
esac
J=$*
SCRIPT_REL_PATH=$(realpath --relative-to=$PWD $0)
SCRIPT_REL_DIR=$(dirname $SCRIPT_REL_PATH)
KDIR_ROOT_DIR=$(realpath $PWD/$SCRIPT_REL_DIR/../../../../)
cd $KDIR_ROOT_DIR
if [ ! -e tools/bpf/bpftool/Makefile ]; then
	echo -e "skip:    bpftool files not found!\n"
	exit 4 
fi
ERROR=0
TMPDIR=
return_value() {
	if [ -d "$TMPDIR" ] ; then
		rm -rf -- $TMPDIR
	fi
	exit $ERROR
}
trap return_value EXIT
check() {
	local dir=$(realpath $1)
	echo -n "binary:  "
	find $dir -type f -executable -name bpftool -print -exec false {} + && \
		ERROR=1 && printf "FAILURE: Did not find bpftool\n"
}
make_and_clean() {
	echo -e "\$PWD:    $PWD"
	echo -e "command: make -s $* >/dev/null"
	make $J -s $* >/dev/null
	if [ $? -ne 0 ] ; then
		ERROR=1
	fi
	if [ $
		check ${@: -1}
	else
		check .
	fi
	(
		if [ $
			cd ${@: -1}
		fi
		make -s clean
	)
	echo
}
make_with_tmpdir() {
	local ARGS
	TMPDIR=$(mktemp -d)
	if [ $
		ARGS=${@:1:(($
	fi
	echo -e "\$PWD:    $PWD"
	echo -e "command: make -s $ARGS ${@: -1}=$TMPDIR/ >/dev/null"
	make $J -s $ARGS ${@: -1}=$TMPDIR/ >/dev/null
	if [ $? -ne 0 ] ; then
		ERROR=1
	fi
	check $TMPDIR
	rm -rf -- $TMPDIR
	echo
}
echo "Trying to build bpftool"
echo -e "... through kbuild\n"
if [ -f ".config" ] ; then
	make_and_clean tools/bpf
	make -C tools/bpf/runqslower OUTPUT=${KDIR_ROOT_DIR}/tools/bpf/runqslower/ clean
	echo -e "skip:    make tools/bpf OUTPUT=<dir> (not supported)\n"
	make_with_tmpdir tools/bpf O
else
	echo -e "skip:    make tools/bpf (no .config found)\n"
	echo -e "skip:    make tools/bpf OUTPUT=<dir> (not supported)\n"
	echo -e "skip:    make tools/bpf O=<dir> (no .config found)\n"
fi
echo -e "... from kernel source tree\n"
make_and_clean -C tools/bpf/bpftool
make_with_tmpdir -C tools/bpf/bpftool OUTPUT
make_with_tmpdir -C tools/bpf/bpftool O
echo -e "... from tools/\n"
cd tools/
make_and_clean bpf
echo -e "skip:    make bpf OUTPUT=<dir> (not supported)\n"
make_with_tmpdir bpf O
echo -e "... from bpftool's dir\n"
cd bpf/bpftool
make_and_clean
make_with_tmpdir OUTPUT
make_with_tmpdir O
