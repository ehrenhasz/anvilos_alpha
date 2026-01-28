ATOMICDIR=$(dirname $0)
. ${ATOMICDIR}/atomic-tbl.sh
gen_template_fallback()
{
	local template="$1"; shift
	local meta="$1"; shift
	local pfx="$1"; shift
	local name="$1"; shift
	local sfx="$1"; shift
	local order="$1"; shift
	local atomic="$1"; shift
	local int="$1"; shift
	local ret="$(gen_ret_type "${meta}" "${int}")"
	local retstmt="$(gen_ret_stmt "${meta}")"
	local params="$(gen_params "${int}" "${atomic}" "$@")"
	local args="$(gen_args "$@")"
	. ${template}
}
gen_order_fallback()
{
	local meta="$1"; shift
	local pfx="$1"; shift
	local name="$1"; shift
	local sfx="$1"; shift
	local order="$1"; shift
	local tmpl_order=${order#_}
	local tmpl="${ATOMICDIR}/fallbacks/${tmpl_order:-fence}"
	gen_template_fallback "${tmpl}" "${meta}" "${pfx}" "${name}" "${sfx}" "${order}" "$@"
}
gen_proto_fallback()
{
	local meta="$1"; shift
	local pfx="$1"; shift
	local name="$1"; shift
	local sfx="$1"; shift
	local order="$1"; shift
	local tmpl="$(find_fallback_template "${pfx}" "${name}" "${sfx}" "${order}")"
	gen_template_fallback "${tmpl}" "${meta}" "${pfx}" "${name}" "${sfx}" "${order}" "$@"
}
gen_proto_order_variant()
{
	local meta="$1"; shift
	local pfx="$1"; shift
	local name="$1"; shift
	local sfx="$1"; shift
	local order="$1"; shift
	local atomic="$1"; shift
	local int="$1"; shift
	local atomicname="${atomic}_${pfx}${name}${sfx}${order}"
	local basename="${atomic}_${pfx}${name}${sfx}"
	local template="$(find_fallback_template "${pfx}" "${name}" "${sfx}" "${order}")"
	local ret="$(gen_ret_type "${meta}" "${int}")"
	local retstmt="$(gen_ret_stmt "${meta}")"
	local params="$(gen_params "${int}" "${atomic}" "$@")"
	local args="$(gen_args "$@")"
	gen_kerneldoc "raw_" "${meta}" "${pfx}" "${name}" "${sfx}" "${order}" "${atomic}" "${int}" "$@"
	printf "static __always_inline ${ret}\n"
	printf "raw_${atomicname}(${params})\n"
	printf "{\n"
	if [ -z ${template} ] && [ -z "${order}" ] && ! meta_has_relaxed "${meta}"; then
		printf "\t${retstmt}arch_${atomicname}(${args});\n"
		printf "}\n\n"
		return
	fi
	printf "#if defined(arch_${atomicname})\n"
	printf "\t${retstmt}arch_${atomicname}(${args});\n"
	if [ "${order}" != "_relaxed" ] && meta_has_relaxed "${meta}"; then
		printf "#elif defined(arch_${basename}_relaxed)\n"
		gen_order_fallback "${meta}" "${pfx}" "${name}" "${sfx}" "${order}" "${atomic}" "${int}" "$@"
	fi
	if [ ! -z "${order}" ] && ! meta_is_implicitly_relaxed "${meta}"; then
		printf "#elif defined(arch_${basename})\n"
		printf "\t${retstmt}arch_${basename}(${args});\n"
	fi
	printf "#else\n"
	if [ ! -z "${template}" ]; then
		gen_proto_fallback "${meta}" "${pfx}" "${name}" "${sfx}" "${order}" "${atomic}" "${int}" "$@"
	else
		printf "#error \"Unable to define raw_${atomicname}\"\n"
	fi
	printf "#endif\n"
	printf "}\n\n"
}
gen_proto_order_variants()
{
	local meta="$1"; shift
	local pfx="$1"; shift
	local name="$1"; shift
	local sfx="$1"; shift
	local atomic="$1"
	gen_proto_order_variant "${meta}" "${pfx}" "${name}" "${sfx}" "" "$@"
	if meta_has_acquire "${meta}"; then
		gen_proto_order_variant "${meta}" "${pfx}" "${name}" "${sfx}" "_acquire" "$@"
	fi
	if meta_has_release "${meta}"; then
		gen_proto_order_variant "${meta}" "${pfx}" "${name}" "${sfx}" "_release" "$@"
	fi
	if meta_has_relaxed "${meta}"; then
		gen_proto_order_variant "${meta}" "${pfx}" "${name}" "${sfx}" "_relaxed" "$@"
	fi
}
gen_basic_fallbacks()
{
	local basename="$1"; shift
cat << EOF
EOF
}
gen_order_fallbacks()
{
	local xchg="$1"; shift
cat <<EOF
	__atomic_op_acquire(arch_${xchg}, __VA_ARGS__)
	__atomic_op_release(arch_${xchg}, __VA_ARGS__)
	__atomic_op_fence(arch_${xchg}, __VA_ARGS__)
EOF
}
gen_xchg_order_fallback()
{
	local xchg="$1"; shift
	local order="$1"; shift
	local forder="${order:-_fence}"
	printf "#if defined(arch_${xchg}${order})\n"
	printf "#define raw_${xchg}${order} arch_${xchg}${order}\n"
	if [ "${order}" != "_relaxed" ]; then
		printf "#elif defined(arch_${xchg}_relaxed)\n"
		printf "#define raw_${xchg}${order}(...) \\\\\n"
		printf "	__atomic_op${forder}(arch_${xchg}, __VA_ARGS__)\n"
	fi
	if [ ! -z "${order}" ]; then
		printf "#elif defined(arch_${xchg})\n"
		printf "#define raw_${xchg}${order} arch_${xchg}\n"
	fi
	printf "#else\n"
	printf "extern void raw_${xchg}${order}_not_implemented(void);\n"
	printf "#define raw_${xchg}${order}(...) raw_${xchg}${order}_not_implemented()\n"
	printf "#endif\n\n"
}
gen_xchg_fallbacks()
{
	local xchg="$1"; shift
	for order in "" "_acquire" "_release" "_relaxed"; do
		gen_xchg_order_fallback "${xchg}" "${order}"
	done
}
gen_try_cmpxchg_fallback()
{
	local cmpxchg="$1"; shift;
	local order="$1"; shift;
cat <<EOF
({ \\
	typeof(*(_ptr)) *___op = (_oldp), ___o = *___op, ___r; \\
	___r = raw_${cmpxchg}${order}((_ptr), ___o, (_new)); \\
	if (unlikely(___r != ___o)) \\
		*___op = ___r; \\
	likely(___r == ___o); \\
})
EOF
}
gen_try_cmpxchg_order_fallback()
{
	local cmpxchg="$1"; shift
	local order="$1"; shift
	local forder="${order:-_fence}"
	printf "#if defined(arch_try_${cmpxchg}${order})\n"
	printf "#define raw_try_${cmpxchg}${order} arch_try_${cmpxchg}${order}\n"
	if [ "${order}" != "_relaxed" ]; then
		printf "#elif defined(arch_try_${cmpxchg}_relaxed)\n"
		printf "#define raw_try_${cmpxchg}${order}(...) \\\\\n"
		printf "	__atomic_op${forder}(arch_try_${cmpxchg}, __VA_ARGS__)\n"
	fi
	if [ ! -z "${order}" ]; then
		printf "#elif defined(arch_try_${cmpxchg})\n"
		printf "#define raw_try_${cmpxchg}${order} arch_try_${cmpxchg}\n"
	fi
	printf "#else\n"
	gen_try_cmpxchg_fallback "${cmpxchg}" "${order}"
	printf "#endif\n\n"
}
gen_try_cmpxchg_fallbacks()
{
	local cmpxchg="$1"; shift;
	for order in "" "_acquire" "_release" "_relaxed"; do
		gen_try_cmpxchg_order_fallback "${cmpxchg}" "${order}"
	done
}
gen_cmpxchg_local_fallbacks()
{
	local cmpxchg="$1"; shift
	printf "#define raw_${cmpxchg} arch_${cmpxchg}\n\n"
	printf "#ifdef arch_try_${cmpxchg}\n"
	printf "#define raw_try_${cmpxchg} arch_try_${cmpxchg}\n"
	printf "#else\n"
	gen_try_cmpxchg_fallback "${cmpxchg}" ""
	printf "#endif\n\n"
}
cat << EOF
// SPDX-License-Identifier: GPL-2.0
// Generated by $0
// DO NOT MODIFY THIS FILE DIRECTLY
EOF
for xchg in "xchg" "cmpxchg" "cmpxchg64" "cmpxchg128"; do
	gen_xchg_fallbacks "${xchg}"
done
for cmpxchg in "cmpxchg" "cmpxchg64" "cmpxchg128"; do
	gen_try_cmpxchg_fallbacks "${cmpxchg}"
done
for cmpxchg in "cmpxchg_local" "cmpxchg64_local" "cmpxchg128_local"; do
	gen_cmpxchg_local_fallbacks "${cmpxchg}" ""
done
for cmpxchg in "sync_cmpxchg"; do
	printf "#define raw_${cmpxchg} arch_${cmpxchg}\n\n"
done
grep '^[a-z]' "$1" | while read name meta args; do
	gen_proto "${meta}" "${name}" "atomic" "int" ${args}
done
cat <<EOF
EOF
grep '^[a-z]' "$1" | while read name meta args; do
	gen_proto "${meta}" "${name}" "atomic64" "s64" ${args}
done
cat <<EOF
EOF
