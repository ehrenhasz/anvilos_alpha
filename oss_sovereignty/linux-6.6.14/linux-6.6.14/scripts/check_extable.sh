obj=$1
file ${obj} | grep -q ELF || (echo "${obj} is not an ELF file." 1>&2 ; exit 0)
objdump -hj __ex_table ${obj} 2> /dev/null > /dev/null
[ $? -ne 0 ] && exit 0
white_list=.text,.fixup
suspicious_relocs=$(objdump -rj __ex_table ${obj}  | tail -n +6 |
			grep -v $(eval echo -e{${white_list}}) | awk '{print $3}')
[ -z "${suspicious_relocs}" ] && exit 0
function find_section_offset_from_symbol()
{
    eval $(objdump -t ${obj} | grep ${1} | sed 's/\([0-9a-f]\+\) .\{7\} \([^ \t]\+\).*/section="\2"; section_offset="0x\1" /')
    section_offset=$(printf "0x%016x" $(( ${section_offset} + $2 )) )
}
function find_symbol_and_offset_from_reloc()
{
    eval $(echo $reloc | sed 's/\([^+]\+\)+\?\(0x[0-9a-f]\+\)\?/symbol="\1"; symbol_offset="\2"/')
    if [ -z "${symbol_offset}" ]; then
	symbol_offset=0x0
    fi
}
function find_alt_replacement_target()
{
    eval $(objdump -rj .altinstructions ${obj} | grep -B1 "${section}+${section_offset}" | head -n1 | awk '{print $3}' |
	   sed 's/\([^+]\+\)+\(0x[0-9a-f]\+\)/alt_target_section="\1"; alt_target_offset="\2"/')
}
function handle_alt_replacement_reloc()
{
    find_alt_replacement_target ${section} ${section_offset}
    echo "Error: found a reference to .altinstr_replacement in __ex_table:"
    addr2line -fip -j ${alt_target_section} -e ${obj} ${alt_target_offset} | awk '{print "\t" $0}'
    error=true
}
function is_executable_section()
{
    objdump -hwj ${section} ${obj} | grep -q CODE
    return $?
}
function handle_suspicious_generic_reloc()
{
    if is_executable_section ${section}; then
	echo "Warning: found a reference to section \"${section}\" in __ex_table:"
	addr2line -fip -j ${section} -e ${obj} ${section_offset} | awk '{print "\t" $0}'
    else
	echo "Error: found a reference to non-executable section \"${section}\" in __ex_table at offset ${section_offset}"
	error=true
    fi
}
function handle_suspicious_reloc()
{
    case "${section}" in
	".altinstr_replacement")
	    handle_alt_replacement_reloc ${section} ${section_offset}
	    ;;
	*)
	    handle_suspicious_generic_reloc ${section} ${section_offset}
	    ;;
    esac
}
function diagnose()
{
    for reloc in ${suspicious_relocs}; do
	find_symbol_and_offset_from_reloc ${reloc}
	find_section_offset_from_symbol ${symbol} ${symbol_offset}
	if [ -z "$( echo $section | grep -v $(eval echo -e{${white_list}}))" ]; then
	    continue;
	fi
	handle_suspicious_reloc
    done
}
function check_debug_info() {
    objdump -hj .debug_info ${obj} 2> /dev/null > /dev/null ||
	echo -e "${obj} does not contain debug information, the addr2line output will be limited.\n" \
	     "Recompile ${obj} with CONFIG_DEBUG_INFO to get a more useful output."
}
check_debug_info
diagnose
if [ "${error}" ]; then
    exit 1
fi
exit 0
