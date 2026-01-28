set -e
min_tool_version=$(dirname $0)/min-tool-version.sh
get_canonical_version()
{
	IFS=.
	set -- $1
	echo $((100000 * $1 + 100 * $2 + $3))
}
print_docs_reference()
{
	echo >&2 "***"
	echo >&2 "*** Please see Documentation/rust/quick-start.rst for details"
	echo >&2 "*** on how to set up the Rust support."
	echo >&2 "***"
}
print_kbuild_explanation()
{
	echo >&2 "***"
	echo >&2 "*** This script is intended to be called from Kbuild."
	echo >&2 "*** Please use the 'rustavailable' target to call it instead."
	echo >&2 "*** Otherwise, the results may not be meaningful."
	exit 1
}
warning=0
trap 'if [ $? -ne 0 ] || [ $warning -ne 0 ]; then print_docs_reference; fi' EXIT
if [ -z "${RUSTC+x}" ]; then
	echo >&2 "***"
	echo >&2 "*** Environment variable 'RUSTC' is not set."
	print_kbuild_explanation
fi
if [ -z "${BINDGEN+x}" ]; then
	echo >&2 "***"
	echo >&2 "*** Environment variable 'BINDGEN' is not set."
	print_kbuild_explanation
fi
if [ -z "${CC+x}" ]; then
	echo >&2 "***"
	echo >&2 "*** Environment variable 'CC' is not set."
	print_kbuild_explanation
fi
if ! command -v "$RUSTC" >/dev/null; then
	echo >&2 "***"
	echo >&2 "*** Rust compiler '$RUSTC' could not be found."
	echo >&2 "***"
	exit 1
fi
if ! command -v "$BINDGEN" >/dev/null; then
	echo >&2 "***"
	echo >&2 "*** Rust bindings generator '$BINDGEN' could not be found."
	echo >&2 "***"
	exit 1
fi
rust_compiler_output=$( \
	LC_ALL=C "$RUSTC" --version 2>/dev/null
) || rust_compiler_code=$?
if [ -n "$rust_compiler_code" ]; then
	echo >&2 "***"
	echo >&2 "*** Running '$RUSTC' to check the Rust compiler version failed with"
	echo >&2 "*** code $rust_compiler_code. See output and docs below for details:"
	echo >&2 "***"
	echo >&2 "$rust_compiler_output"
	echo >&2 "***"
	exit 1
fi
rust_compiler_version=$( \
	echo "$rust_compiler_output" \
		| sed -nE '1s:.*rustc ([0-9]+\.[0-9]+\.[0-9]+).*:\1:p'
)
if [ -z "$rust_compiler_version" ]; then
	echo >&2 "***"
	echo >&2 "*** Running '$RUSTC' to check the Rust compiler version did not return"
	echo >&2 "*** an expected output. See output and docs below for details:"
	echo >&2 "***"
	echo >&2 "$rust_compiler_output"
	echo >&2 "***"
	exit 1
fi
rust_compiler_min_version=$($min_tool_version rustc)
rust_compiler_cversion=$(get_canonical_version $rust_compiler_version)
rust_compiler_min_cversion=$(get_canonical_version $rust_compiler_min_version)
if [ "$rust_compiler_cversion" -lt "$rust_compiler_min_cversion" ]; then
	echo >&2 "***"
	echo >&2 "*** Rust compiler '$RUSTC' is too old."
	echo >&2 "***   Your version:    $rust_compiler_version"
	echo >&2 "***   Minimum version: $rust_compiler_min_version"
	echo >&2 "***"
	exit 1
fi
if [ "$rust_compiler_cversion" -gt "$rust_compiler_min_cversion" ]; then
	echo >&2 "***"
	echo >&2 "*** Rust compiler '$RUSTC' is too new. This may or may not work."
	echo >&2 "***   Your version:     $rust_compiler_version"
	echo >&2 "***   Expected version: $rust_compiler_min_version"
	echo >&2 "***"
	warning=1
fi
rust_bindings_generator_output=$( \
	LC_ALL=C "$BINDGEN" --version 2>/dev/null
) || rust_bindings_generator_code=$?
if [ -n "$rust_bindings_generator_code" ]; then
	echo >&2 "***"
	echo >&2 "*** Running '$BINDGEN' to check the Rust bindings generator version failed with"
	echo >&2 "*** code $rust_bindings_generator_code. See output and docs below for details:"
	echo >&2 "***"
	echo >&2 "$rust_bindings_generator_output"
	echo >&2 "***"
	exit 1
fi
rust_bindings_generator_version=$( \
	echo "$rust_bindings_generator_output" \
		| sed -nE '1s:.*bindgen ([0-9]+\.[0-9]+\.[0-9]+).*:\1:p'
)
if [ -z "$rust_bindings_generator_version" ]; then
	echo >&2 "***"
	echo >&2 "*** Running '$BINDGEN' to check the bindings generator version did not return"
	echo >&2 "*** an expected output. See output and docs below for details:"
	echo >&2 "***"
	echo >&2 "$rust_bindings_generator_output"
	echo >&2 "***"
	exit 1
fi
rust_bindings_generator_min_version=$($min_tool_version bindgen)
rust_bindings_generator_cversion=$(get_canonical_version $rust_bindings_generator_version)
rust_bindings_generator_min_cversion=$(get_canonical_version $rust_bindings_generator_min_version)
if [ "$rust_bindings_generator_cversion" -lt "$rust_bindings_generator_min_cversion" ]; then
	echo >&2 "***"
	echo >&2 "*** Rust bindings generator '$BINDGEN' is too old."
	echo >&2 "***   Your version:    $rust_bindings_generator_version"
	echo >&2 "***   Minimum version: $rust_bindings_generator_min_version"
	echo >&2 "***"
	exit 1
fi
if [ "$rust_bindings_generator_cversion" -gt "$rust_bindings_generator_min_cversion" ]; then
	echo >&2 "***"
	echo >&2 "*** Rust bindings generator '$BINDGEN' is too new. This may or may not work."
	echo >&2 "***   Your version:     $rust_bindings_generator_version"
	echo >&2 "***   Expected version: $rust_bindings_generator_min_version"
	echo >&2 "***"
	warning=1
fi
bindgen_libclang_output=$( \
	LC_ALL=C "$BINDGEN" $(dirname $0)/rust_is_available_bindgen_libclang.h 2>&1 >/dev/null
) || bindgen_libclang_code=$?
if [ -n "$bindgen_libclang_code" ]; then
	echo >&2 "***"
	echo >&2 "*** Running '$BINDGEN' to check the libclang version (used by the Rust"
	echo >&2 "*** bindings generator) failed with code $bindgen_libclang_code. This may be caused by"
	echo >&2 "*** a failure to locate libclang. See output and docs below for details:"
	echo >&2 "***"
	echo >&2 "$bindgen_libclang_output"
	echo >&2 "***"
	exit 1
fi
bindgen_libclang_version=$( \
	echo "$bindgen_libclang_output" \
		| sed -nE 's:.*clang version ([0-9]+\.[0-9]+\.[0-9]+).*:\1:p'
)
if [ -z "$bindgen_libclang_version" ]; then
	echo >&2 "***"
	echo >&2 "*** Running '$BINDGEN' to check the libclang version (used by the Rust"
	echo >&2 "*** bindings generator) did not return an expected output. See output"
	echo >&2 "*** and docs below for details:"
	echo >&2 "***"
	echo >&2 "$bindgen_libclang_output"
	echo >&2 "***"
	exit 1
fi
bindgen_libclang_min_version=$($min_tool_version llvm)
bindgen_libclang_cversion=$(get_canonical_version $bindgen_libclang_version)
bindgen_libclang_min_cversion=$(get_canonical_version $bindgen_libclang_min_version)
if [ "$bindgen_libclang_cversion" -lt "$bindgen_libclang_min_cversion" ]; then
	echo >&2 "***"
	echo >&2 "*** libclang (used by the Rust bindings generator '$BINDGEN') is too old."
	echo >&2 "***   Your version:    $bindgen_libclang_version"
	echo >&2 "***   Minimum version: $bindgen_libclang_min_version"
	echo >&2 "***"
	exit 1
fi
cc_name=$($(dirname $0)/cc-version.sh $CC | cut -f1 -d' ')
if [ "$cc_name" = Clang ]; then
	clang_version=$( \
		LC_ALL=C $CC --version 2>/dev/null \
			| sed -nE '1s:.*version ([0-9]+\.[0-9]+\.[0-9]+).*:\1:p'
	)
	if [ "$clang_version" != "$bindgen_libclang_version" ]; then
		echo >&2 "***"
		echo >&2 "*** libclang (used by the Rust bindings generator '$BINDGEN')"
		echo >&2 "*** version does not match Clang's. This may be a problem."
		echo >&2 "***   libclang version: $bindgen_libclang_version"
		echo >&2 "***   Clang version:    $clang_version"
		echo >&2 "***"
		warning=1
	fi
fi
rustc_sysroot=$("$RUSTC" $KRUSTFLAGS --print sysroot)
rustc_src=${RUST_LIB_SRC:-"$rustc_sysroot/lib/rustlib/src/rust/library"}
rustc_src_core="$rustc_src/core/src/lib.rs"
if [ ! -e "$rustc_src_core" ]; then
	echo >&2 "***"
	echo >&2 "*** Source code for the 'core' standard library could not be found"
	echo >&2 "*** at '$rustc_src_core'."
	echo >&2 "***"
	exit 1
fi
