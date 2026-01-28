if [ $
	arch_x86_header_dir=tools/arch/x86/include/asm/
else
	arch_x86_header_dir=$1
fi
x86_irq_vectors=${arch_x86_header_dir}/irq_vectors.h
first_external_regex='^
first_external_vector=$(grep -E ${first_external_regex} ${x86_irq_vectors} | sed -r "s/${first_external_regex}/\1/g")
printf "static const char *x86_irq_vectors[] = {\n"
regex='^
sed -r "s/FIRST_EXTERNAL_VECTOR/${first_external_vector}/g" ${x86_irq_vectors} | \
grep -E ${regex} | \
	sed -r "s/${regex}/\2 \1/g" | sort -n | \
	xargs printf "\t[%s] = \"%s\",\n"
printf "};\n\n"
