usage() {
	echo "Usage:"
	echo "	$0 -r <release> | <vmlinux> [<base path>|auto] [<modules path>]"
}
if type llvm-cxxfilt >/dev/null 2>&1 ; then
	cppfilt=llvm-cxxfilt
elif type c++filt >/dev/null 2>&1 ; then
	cppfilt=c++filt
	cppfilt_opts=-i
fi
UTIL_SUFFIX=
if [[ -z ${LLVM:-} ]]; then
	UTIL_PREFIX=${CROSS_COMPILE:-}
else
	UTIL_PREFIX=llvm-
	if [[ ${LLVM} == */ ]]; then
		UTIL_PREFIX=${LLVM}${UTIL_PREFIX}
	elif [[ ${LLVM} == -* ]]; then
		UTIL_SUFFIX=${LLVM}
	fi
fi
READELF=${UTIL_PREFIX}readelf${UTIL_SUFFIX}
ADDR2LINE=${UTIL_PREFIX}addr2line${UTIL_SUFFIX}
if [[ $1 == "-r" ]] ; then
	vmlinux=""
	basepath="auto"
	modpath=""
	release=$2
	for fn in {,/usr/lib/debug}/boot/vmlinux-$release{,.debug} /lib/modules/$release{,/build}/vmlinux ; do
		if [ -e "$fn" ] ; then
			vmlinux=$fn
			break
		fi
	done
	if [[ $vmlinux == "" ]] ; then
		echo "ERROR! vmlinux image for release $release is not found" >&2
		usage
		exit 2
	fi
else
	vmlinux=$1
	basepath=${2-auto}
	modpath=$3
	release=""
	debuginfod=
	if type debuginfod-find >/dev/null 2>&1 ; then
		debuginfod=${1-only}
	fi
	if [[ $vmlinux == "" && -z $debuginfod ]] ; then
		echo "ERROR! vmlinux image must be specified" >&2
		usage
		exit 1
	fi
fi
declare aarray_support=true
declare -A cache 2>/dev/null
if [[ $? != 0 ]]; then
	aarray_support=false
else
	declare -A modcache
fi
find_module() {
	if [[ -n $debuginfod ]] ; then
		if [[ -n $modbuildid ]] ; then
			debuginfod-find debuginfo $modbuildid && return
		fi
		if [[ $debuginfod == "only" ]] ; then
			return
		fi
	fi
	if [[ "$modpath" != "" ]] ; then
		for fn in $(find "$modpath" -name "${module//_/[-_]}.ko*") ; do
			if ${READELF} -WS "$fn" | grep -qwF .debug_line ; then
				echo $fn
				return
			fi
		done
		return 1
	fi
	modpath=$(dirname "$vmlinux")
	find_module && return
	if [[ $release == "" ]] ; then
		release=$(gdb -ex 'print init_uts_ns.name.release' -ex 'quit' -quiet -batch "$vmlinux" 2>/dev/null | sed -n 's/\$1 = "\(.*\)".*/\1/p')
	fi
	for dn in {/usr/lib/debug,}/lib/modules/$release ; do
		if [ -e "$dn" ] ; then
			modpath="$dn"
			find_module && return
		fi
	done
	modpath=""
	return 1
}
parse_symbol() {
	if [[ $module == "" ]] ; then
		local objfile=$vmlinux
	elif [[ $aarray_support == true && "${modcache[$module]+isset}" == "isset" ]]; then
		local objfile=${modcache[$module]}
	else
		local objfile=$(find_module)
		if [[ $objfile == "" ]] ; then
			echo "WARNING! Modules path isn't set, but is needed to parse this symbol" >&2
			return
		fi
		if [[ $aarray_support == true ]]; then
			modcache[$module]=$objfile
		fi
	fi
	symbol=${symbol
	symbol=${symbol%\)}
	local segment
	if [[ $symbol == *:* ]] ; then
		segment=${symbol%%:*}:
		symbol=${symbol
	fi
	local name=${symbol%+*}
	if [[ $aarray_support == true && "${cache[$module,$name]+isset}" == "isset" ]]; then
		local base_addr=${cache[$module,$name]}
	else
		local base_addr=$(nm "$objfile" 2>/dev/null | awk '$3 == "'$name'" && ($2 == "t" || $2 == "T") {print $1; exit}')
		if [[ $base_addr == "" ]] ; then
			return
		fi
		if [[ $aarray_support == true ]]; then
			cache[$module,$name]="$base_addr"
		fi
	fi
	local expr=${symbol%/*}
	expr=${expr/$name/0x$base_addr}
	expr=$((expr))
	local address=$(printf "%x\n" "$expr")
	if [[ $aarray_support == true && "${cache[$module,$address]+isset}" == "isset" ]]; then
		local code=${cache[$module,$address]}
	else
		local code=$(${ADDR2LINE} -i -e "$objfile" "$address" 2>/dev/null)
		if [[ $aarray_support == true ]]; then
			cache[$module,$address]=$code
		fi
	fi
	if [[ $code == "??:0" ]]; then
		return
	fi
	code=$(while read -r line; do echo "${line#$basepath/}"; done <<< "$code")
	code=${code//$'\n'/' '}
	if [[ $name =~ ^_R && $cppfilt != "" ]] ; then
		name=$("$cppfilt" "$cppfilt_opts" "$name")
	fi
	symbol="$segment$name ($code)"
}
debuginfod_get_vmlinux() {
	local vmlinux_buildid=${1
	if [[ $vmlinux != "" ]]; then
		return
	fi
	if [[ $vmlinux_buildid =~ ^[0-9a-f]+ ]]; then
		vmlinux=$(debuginfod-find debuginfo $vmlinux_buildid)
		if [[ $? -ne 0 ]] ; then
			echo "ERROR! vmlinux image not found via debuginfod-find" >&2
			usage
			exit 2
		fi
		return
	fi
	echo "ERROR! Build ID for vmlinux not found. Try passing -r or specifying vmlinux" >&2
	usage
	exit 2
}
decode_code() {
	local scripts=`dirname "${BASH_SOURCE[0]}"`
	echo "$1" | $scripts/decodecode
}
handle_line() {
	if [[ $basepath == "auto" && $vmlinux != "" ]] ; then
		module=""
		symbol="kernel_init+0x0/0x0"
		parse_symbol
		basepath=${symbol
		basepath=${basepath%/init/main.c:*)}
	fi
	local words
	read -a words <<<"$1"
	local last=$(( ${
	for i in "${!words[@]}"; do
		if [[ ${words[$i]} =~ \[\<([^]]+)\>\] ]]; then
			unset words[$i]
		fi
		if [[ ${words[$i]} == \[ && ${words[$i+1]} == *\] ]]; then
			unset words[$i]
			words[$i+1]=$(printf "[%13s\n" "${words[$i+1]}")
		fi
	done
	if [[ ${words[$last]} =~ ^[0-9a-f]+\] ]]; then
		words[$last-1]="${words[$last-1]} ${words[$last]}"
		unset words[$last]
		last=$(( $last - 1 ))
	fi
	if [[ ${words[$last]} =~ \[([^]]+)\] ]]; then
		module=${words[$last]}
		module=${module
		module=${module%\]}
		modbuildid=${module
		module=${module% *}
		if [[ $modbuildid == $module ]]; then
			modbuildid=
		fi
		symbol=${words[$last-1]}
		unset words[$last-1]
	else
		symbol=${words[$last]}
		module=
		modbuildid=
	fi
	unset words[$last]
	parse_symbol 
	echo "${words[@]}" "$symbol $module"
}
while read line; do
	if [[ $line =~ \[\<([^]]+)\>\] ]] ||
	   [[ $line =~ [^+\ ]+\+0x[0-9a-f]+/0x[0-9a-f]+ ]]; then
		handle_line "$line"
	elif [[ $line == *Code:* ]]; then
		decode_code "$line"
	elif [[ -n $debuginfod && $line =~ PID:\ [0-9]+\ Comm: ]]; then
		debuginfod_get_vmlinux "$line"
	else
		echo "$line"
	fi
done
