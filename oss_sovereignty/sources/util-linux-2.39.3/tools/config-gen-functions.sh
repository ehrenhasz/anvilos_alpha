ul_get_configuration() {
	local conf="$1"
	local dir=$(dirname $1)
	local opts=$(cat $conf)
	local old_opts=
	while [ "$opts" != "$old_opts" ]; do
		local new_opts=
		old_opts="$opts"
		for citem in $opts; do
			case $citem in
			include:*) new_opts="$new_opts $(cat $dir/${citem##*:})" ;;
			*) new_opts="$new_opts $citem" ;;
			esac
		done
		opts="$new_opts"
	done
	echo $opts | tr " " "\n" | sort -u
}
