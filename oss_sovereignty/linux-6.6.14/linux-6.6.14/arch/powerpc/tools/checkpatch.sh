script_base=$(realpath $(dirname $0))
exec $script_base/../../../scripts/checkpatch.pl \
	--subjective \
	--no-summary \
	--show-types \
	--ignore ARCH_INCLUDE_LINUX \
	--ignore BIT_MACRO \
	--ignore COMPARISON_TO_NULL \
	--ignore EMAIL_SUBJECT \
	--ignore FILE_PATH_CHANGES \
	--ignore GLOBAL_INITIALISERS \
	--ignore LINE_SPACING \
	--ignore MULTIPLE_ASSIGNMENTS \
	--ignore DT_SPLIT_BINDING_PATCH \
	$@
