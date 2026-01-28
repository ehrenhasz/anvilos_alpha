__my_reassemble_comp_words_by_ref()
{
	local exclude i j first
	exclude="${1//[^$COMP_WORDBREAKS]}"
	cword_=$COMP_CWORD
	if [ -z "$exclude" ]; then
		words_=("${COMP_WORDS[@]}")
		return
	fi
	for ((i=0, j=0; i < ${
		first=t
		while
			[ $i -gt 0 ] &&
			[ -n "${COMP_WORDS[$i]}" ] &&
			[ "${COMP_WORDS[$i]//[^$exclude]}" = "${COMP_WORDS[$i]}" ]
		do
			if [ $j -ge 2 ] && [ -n "$first" ]; then
				((j--))
			fi
			first=
			words_[$j]=${words_[j]}${COMP_WORDS[i]}
			if [ $i = $COMP_CWORD ]; then
				cword_=$j
			fi
			if (($i < ${
				((i++))
			else
				return
			fi
		done
		words_[$j]=${words_[j]}${COMP_WORDS[i]}
		if [ $i = $COMP_CWORD ]; then
			cword_=$j
		fi
	done
}
preload_get_comp_words_by_ref="true"
if [ $preload_get_comp_words_by_ref = "true" ]; then
	type _get_comp_words_by_ref &>/dev/null ||
	preload_get_comp_words_by_ref="false"
fi
[ $preload_get_comp_words_by_ref = "true" ] ||
__perf_get_comp_words_by_ref()
{
	local exclude cur_ words_ cword_
	if [ "$1" = "-n" ]; then
		exclude=$2
		shift 2
	fi
	__my_reassemble_comp_words_by_ref "$exclude"
	cur_=${words_[cword_]}
	while [ $
		case "$1" in
		cur)
			cur=$cur_
			;;
		prev)
			prev=${words_[$cword_-1]}
			;;
		words)
			words=("${words_[@]}")
			;;
		cword)
			cword=$cword_
			;;
		esac
		shift
	done
}
preload__ltrim_colon_completions="true"
if [ $preload__ltrim_colon_completions = "true" ]; then
	type __ltrim_colon_completions &>/dev/null ||
	preload__ltrim_colon_completions="false"
fi
[ $preload__ltrim_colon_completions = "true" ] ||
__perf__ltrim_colon_completions()
{
	if [[ "$1" == *:* && "$COMP_WORDBREAKS" == *:* ]]; then
		local colon_word=${1%"${1##*:}"}
		local i=${
		while [[ $((--i)) -ge 0 ]]; do
			COMPREPLY[$i]=${COMPREPLY[$i]
		done
	fi
}
__perfcomp ()
{
	COMPREPLY=( $( compgen -W "$1" -- "$2" ) )
}
__perfcomp_colon ()
{
	__perfcomp "$1" "$2"
	if [ $preload__ltrim_colon_completions = "true" ]; then
		__ltrim_colon_completions $cur
	else
		__perf__ltrim_colon_completions $cur
	fi
}
__perf_prev_skip_opts ()
{
	local i cmd_ cmds_
	let i=cword-1
	cmds_=$($cmd $1 --list-cmds)
	prev_skip_opts=()
	while [ $i -ge 0 ]; do
		if [[ ${words[i]} == $1 ]]; then
			return
		fi
		for cmd_ in $cmds_; do
			if [[ ${words[i]} == $cmd_ ]]; then
				prev_skip_opts=${words[i]}
				return
			fi
		done
		((i--))
	done
}
__perf_main ()
{
	local cmd
	cmd=${words[0]}
	COMPREPLY=()
	__perf_prev_skip_opts
	if [ -z $prev_skip_opts ]; then
		if [[ $cur == --* ]]; then
			cmds=$($cmd --list-opts)
		else
			cmds=$($cmd --list-cmds)
		fi
		__perfcomp "$cmds" "$cur"
	elif [[ $prev == @("-e"|"--event") &&
		$prev_skip_opts == @(record|stat|top) ]]; then
		local cur1=${COMP_WORDS[COMP_CWORD]}
		local raw_evts=$($cmd list --raw-dump)
		local arr s tmp result cpu_evts
		if [[ `uname -m` != aarch64 ]]; then
			cpu_evts=$(ls /sys/bus/event_source/devices/cpu/events)
		fi
		if [[ "$cur1" == */* && ${cur1
			OLD_IFS="$IFS"
			IFS=" "
			arr=($raw_evts)
			IFS="$OLD_IFS"
			for s in ${arr[@]}
			do
				if [[ "$s" == *cpu/* ]]; then
					tmp=${s
					result=$result" ""cpu/"${tmp^^}
				else
					result=$result" "$s
				fi
			done
			evts=${result}" "${cpu_evts}
		else
			evts=${raw_evts}" "${cpu_evts}
		fi
		if [[ "$cur1" == , ]]; then
			__perfcomp_colon "$evts" ""
		else
			__perfcomp_colon "$evts" "$cur1"
		fi
	else
		if [[ $prev_skip_opts == @(kvm|kmem|mem|lock|sched|
			|data|help|script|test|timechart|trace) ]]; then
			subcmds=$($cmd $prev_skip_opts --list-cmds)
			__perfcomp_colon "$subcmds" "$cur"
		fi
		if [[ $cur == --* ]];  then
			subcmd=$prev_skip_opts
			__perf_prev_skip_opts $subcmd
			subcmd=$subcmd" "$prev_skip_opts
			opts=$($cmd $subcmd --list-opts)
			__perfcomp "$opts" "$cur"
		fi
	fi
}
if [[ -n ${ZSH_VERSION-} ]]; then
	autoload -U +X compinit && compinit
	__perfcomp ()
	{
		emulate -L zsh
		local c IFS=$' \t\n'
		local -a array
		for c in ${=1}; do
			case $c in
			--*=*|*.) ;;
			*) c="$c " ;;
			esac
			array[${
		done
		compset -P '*[=:]'
		compadd -Q -S '' -a -- array && _ret=0
	}
	__perfcomp_colon ()
	{
		emulate -L zsh
		local cur_="${2-$cur}"
		local c IFS=$' \t\n'
		local -a array
		if [[ "$cur_" == *:* ]]; then
			local colon_word=${cur_%"${cur_##*:}"}
		fi
		for c in ${=1}; do
			case $c in
			--*=*|*.) ;;
			*) c="$c " ;;
			esac
			array[$
		done
		compset -P '*[=:]'
		compadd -Q -S '' -a -- array && _ret=0
	}
	_perf ()
	{
		local _ret=1 cur cword prev
		cur=${words[CURRENT]}
		prev=${words[CURRENT-1]}
		let cword=CURRENT-1
		emulate ksh -c __perf_main
		let _ret && _default && _ret=0
		return _ret
	}
	compdef _perf perf
	return
fi
type perf &>/dev/null &&
_perf()
{
	if [[ "$COMP_WORDBREAKS" != *,* ]]; then
		COMP_WORDBREAKS="${COMP_WORDBREAKS},"
		export COMP_WORDBREAKS
	fi
	if [[ "$COMP_WORDBREAKS" == *:* ]]; then
		COMP_WORDBREAKS="${COMP_WORDBREAKS/:/}"
		export COMP_WORDBREAKS
	fi
	local cur words cword prev
	if [ $preload_get_comp_words_by_ref = "true" ]; then
		_get_comp_words_by_ref -n =:, cur words cword prev
	else
		__perf_get_comp_words_by_ref -n =:, cur words cword prev
	fi
	__perf_main
} &&
complete -o bashdefault -o default -o nospace -F _perf perf 2>/dev/null \
	|| complete -o default -o nospace -F _perf perf
