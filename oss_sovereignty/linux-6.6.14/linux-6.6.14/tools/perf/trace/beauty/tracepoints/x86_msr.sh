if [ $
	arch_x86_header_dir=tools/arch/x86/include/asm/
else
	arch_x86_header_dir=$1
fi
x86_msr_index=${arch_x86_header_dir}/msr-index.h
printf "static const char * const x86_MSRs[] = {\n"
regex='^[[:space:]]*
grep -E $regex ${x86_msr_index} | grep -E -v 'MSR_(ATOM|P[46]|IA32_(TSC_DEADLINE|UCODE_REV)|IDT_FCR4)' | \
	sed -r "s/$regex/\2 \1/g" | sort -n | \
	xargs printf "\t[%s] = \"%s\",\n"
printf "};\n\n"
regex='^[[:space:]]*
printf "#define x86_64_specific_MSRs_offset "
grep -E $regex ${x86_msr_index} | sed -r "s/$regex/\2/g" | sort -n | head -1
printf "static const char * const x86_64_specific_MSRs[] = {\n"
grep -E $regex ${x86_msr_index} | \
	sed -r "s/$regex/\2 \1/g" | grep -E -vw 'K6_WHCR' | sort -n | \
	xargs printf "\t[%s - x86_64_specific_MSRs_offset] = \"%s\",\n"
printf "};\n\n"
regex='^[[:space:]]*
printf "#define x86_AMD_V_KVM_MSRs_offset "
grep -E $regex ${x86_msr_index} | sed -r "s/$regex/\2/g" | sort -n | head -1
printf "static const char * const x86_AMD_V_KVM_MSRs[] = {\n"
grep -E $regex ${x86_msr_index} | \
	sed -r "s/$regex/\2 \1/g" | sort -n | \
	xargs printf "\t[%s - x86_AMD_V_KVM_MSRs_offset] = \"%s\",\n"
printf "};\n"
