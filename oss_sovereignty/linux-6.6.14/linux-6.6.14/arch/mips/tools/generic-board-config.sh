srctree="$1"
objtree="$2"
ref_cfg="$3"
cfg="$4"
boards_origin="$5"
shift 5
case ${boards_origin} in
"command line")
	print_skipped=1
	;;
environment*)
	print_skipped=1
	;;
*)
	print_skipped=0
	;;
esac
for board in $@; do
	board_cfg="${srctree}/arch/mips/configs/generic/board-${board}.config"
	if [ ! -f "${board_cfg}" ]; then
		echo "WARNING: Board config '${board_cfg}' not found"
		continue
	fi
	grep -E '^
	    cut -d' ' -f 3- | \
	    while read req; do
		case ${req} in
		*=y)
			grep -Eq "^${req}\$" "${ref_cfg}" && continue
			;;
		*=n)
			grep -Eq "^${req/%=n/=y}\$" "${ref_cfg}" || continue
			;;
		*)
			echo "WARNING: Unhandled requirement '${req}'"
			;;
		esac
		[ ${print_skipped} -eq 1 ] && echo "Skipping ${board_cfg}"
		exit 1
	done || continue
	${srctree}/scripts/kconfig/merge_config.sh \
		-m -O ${objtree} ${cfg} ${board_cfg} \
		| grep -Ev '^(
done
