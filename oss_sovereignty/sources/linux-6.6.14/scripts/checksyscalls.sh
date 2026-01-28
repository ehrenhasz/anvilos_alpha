ignore_list() {
cat << EOF
/* *at */
/* Missing flags argument */
/* CLOEXEC flag */
/* MMU */
/* System calls for 32-bit kernels only */
/* i386-specific or historical system calls */
/* ... including the "new" 32-bit uid syscalls */
/* these can be expressed using other calls */
/* sync_file_range had a stupid ABI. Allow sync_file_range2 instead */
/* Unmerged syscalls for AFS, STREAMS, etc. */
/* 64-bit ports never needed these, and new 32-bit ports can use statx */
/* Newer ports are not required to provide fstat in favor of statx */
EOF
}
syscall_list() {
    grep '^[0-9]' "$1" | sort -n |
	while read nr abi name entry ; do
		echo "
		echo "
		echo "
	done
}
(ignore_list && syscall_list $(dirname $0)/../arch/x86/entry/syscalls/syscall_32.tbl) | \
$* -Wno-error -Wno-unused-macros -E -x c - > /dev/null
