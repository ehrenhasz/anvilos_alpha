set -e
LD="$1"
KBUILD_LDFLAGS="$2"
LDFLAGS_vmlinux="$3"
is_enabled() {
	grep -q "^$1=y" include/config/auto.conf
}
info()
{
	printf "  %-7s %s\n" "${1}" "${2}"
}
vmlinux_link()
{
	local output=${1}
	local objs
	local libs
	local ld
	local ldflags
	local ldlibs
	info LD ${output}
	shift
	if is_enabled CONFIG_LTO_CLANG || is_enabled CONFIG_X86_KERNEL_IBT; then
		objs=vmlinux.o
		libs=
	else
		objs=vmlinux.a
		libs="${KBUILD_VMLINUX_LIBS}"
	fi
	if is_enabled CONFIG_MODULES; then
		objs="${objs} .vmlinux.export.o"
	fi
	objs="${objs} init/version-timestamp.o"
	if [ "${SRCARCH}" = "um" ]; then
		wl=-Wl,
		ld="${CC}"
		ldflags="${CFLAGS_vmlinux}"
		ldlibs="-lutil -lrt -lpthread"
	else
		wl=
		ld="${LD}"
		ldflags="${KBUILD_LDFLAGS} ${LDFLAGS_vmlinux}"
		ldlibs=
	fi
	ldflags="${ldflags} ${wl}--script=${objtree}/${KBUILD_LDS}"
	if [ "$output" != "${output#.tmp_vmlinux.kallsyms}" ] ; then
		ldflags="${ldflags} ${wl}--strip-debug"
	fi
	if is_enabled CONFIG_VMLINUX_MAP; then
		ldflags="${ldflags} ${wl}-Map=${output}.map"
	fi
	${ld} ${ldflags} -o ${output}					\
		${wl}--whole-archive ${objs} ${wl}--no-whole-archive	\
		${wl}--start-group ${libs} ${wl}--end-group		\
		$@ ${ldlibs}
}
gen_btf()
{
	local pahole_ver
	if ! [ -x "$(command -v ${PAHOLE})" ]; then
		echo >&2 "BTF: ${1}: pahole (${PAHOLE}) is not available"
		return 1
	fi
	pahole_ver=$(${PAHOLE} --version | sed -E 's/v([0-9]+)\.([0-9]+)/\1\2/')
	if [ "${pahole_ver}" -lt "116" ]; then
		echo >&2 "BTF: ${1}: pahole version $(${PAHOLE} --version) is too old, need at least v1.16"
		return 1
	fi
	vmlinux_link ${1}
	info "BTF" ${2}
	LLVM_OBJCOPY="${OBJCOPY}" ${PAHOLE} -J ${PAHOLE_FLAGS} ${1}
	${OBJCOPY} --only-section=.BTF --set-section-flags .BTF=alloc,readonly \
		--strip-all ${1} ${2} 2>/dev/null
	printf '\1' | dd of=${2} conv=notrunc bs=1 seek=16 status=none
}
kallsyms()
{
	local kallsymopt;
	if is_enabled CONFIG_KALLSYMS_ALL; then
		kallsymopt="${kallsymopt} --all-symbols"
	fi
	if is_enabled CONFIG_KALLSYMS_ABSOLUTE_PERCPU; then
		kallsymopt="${kallsymopt} --absolute-percpu"
	fi
	if is_enabled CONFIG_KALLSYMS_BASE_RELATIVE; then
		kallsymopt="${kallsymopt} --base-relative"
	fi
	if is_enabled CONFIG_LTO_CLANG; then
		kallsymopt="${kallsymopt} --lto-clang"
	fi
	info KSYMS ${2}
	scripts/kallsyms ${kallsymopt} ${1} > ${2}
}
kallsyms_step()
{
	kallsymso_prev=${kallsymso}
	kallsyms_vmlinux=.tmp_vmlinux.kallsyms${1}
	kallsymso=${kallsyms_vmlinux}.o
	kallsyms_S=${kallsyms_vmlinux}.S
	vmlinux_link ${kallsyms_vmlinux} "${kallsymso_prev}" ${btf_vmlinux_bin_o}
	mksysmap ${kallsyms_vmlinux} ${kallsyms_vmlinux}.syms ${kallsymso_prev}
	kallsyms ${kallsyms_vmlinux}.syms ${kallsyms_S}
	info AS ${kallsyms_S}
	${CC} ${NOSTDINC_FLAGS} ${LINUXINCLUDE} ${KBUILD_CPPFLAGS} \
	      ${KBUILD_AFLAGS} ${KBUILD_AFLAGS_KERNEL} \
	      -c -o ${kallsymso} ${kallsyms_S}
}
mksysmap()
{
	info NM ${2}
	${CONFIG_SHELL} "${srctree}/scripts/mksysmap" ${1} ${2} ${3}
}
sorttable()
{
	${objtree}/scripts/sorttable ${1}
}
cleanup()
{
	rm -f .btf.*
	rm -f System.map
	rm -f vmlinux
	rm -f vmlinux.map
}
case "${KBUILD_VERBOSE}" in
*1*)
	set -x
	;;
esac
if [ "$1" = "clean" ]; then
	cleanup
	exit 0
fi
${MAKE} -f "${srctree}/scripts/Makefile.build" obj=init init/version-timestamp.o
btf_vmlinux_bin_o=""
if is_enabled CONFIG_DEBUG_INFO_BTF; then
	btf_vmlinux_bin_o=.btf.vmlinux.bin.o
	if ! gen_btf .tmp_vmlinux.btf $btf_vmlinux_bin_o ; then
		echo >&2 "Failed to generate BTF for vmlinux"
		echo >&2 "Try to disable CONFIG_DEBUG_INFO_BTF"
		exit 1
	fi
fi
kallsymso=""
kallsymso_prev=""
kallsyms_vmlinux=""
if is_enabled CONFIG_KALLSYMS; then
	kallsyms_step 1
	kallsyms_step 2
	size1=$(${CONFIG_SHELL} "${srctree}/scripts/file-size.sh" ${kallsymso_prev})
	size2=$(${CONFIG_SHELL} "${srctree}/scripts/file-size.sh" ${kallsymso})
	if [ $size1 -ne $size2 ] || [ -n "${KALLSYMS_EXTRA_PASS}" ]; then
		kallsyms_step 3
	fi
fi
vmlinux_link vmlinux "${kallsymso}" ${btf_vmlinux_bin_o}
if is_enabled CONFIG_DEBUG_INFO_BTF && is_enabled CONFIG_BPF; then
	info BTFIDS vmlinux
	${RESOLVE_BTFIDS} vmlinux
fi
mksysmap vmlinux System.map ${kallsymso}
if is_enabled CONFIG_BUILDTIME_TABLE_SORT; then
	info SORTTAB vmlinux
	if ! sorttable vmlinux; then
		echo >&2 Failed to sort kernel tables
		exit 1
	fi
fi
if is_enabled CONFIG_KALLSYMS; then
	if ! cmp -s System.map ${kallsyms_vmlinux}.syms; then
		echo >&2 Inconsistent kallsyms data
		echo >&2 'Try "make KALLSYMS_EXTRA_PASS=1" as a workaround'
		exit 1
	fi
fi
echo "vmlinux: $0" > .vmlinux.d
