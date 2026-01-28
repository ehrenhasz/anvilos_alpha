#ifndef __ARM64_HYP_IMAGE_H__
#define __ARM64_HYP_IMAGE_H__
#define __HYP_CONCAT(a, b)	a ## b
#define HYP_CONCAT(a, b)	__HYP_CONCAT(a, b)
#ifndef __KVM_NVHE_HYPERVISOR__
#define kvm_nvhe_sym(sym)	__kvm_nvhe_##sym
#else
#define kvm_nvhe_sym(sym)	sym
#endif
#ifdef LINKER_SCRIPT
#define HYP_SECTION_NAME(NAME)	.hyp##NAME
#define HYP_SECTION_SYMBOL_NAME(NAME) \
	HYP_CONCAT(__hyp_section_, HYP_SECTION_NAME(NAME))
#define BEGIN_HYP_SECTION(NAME)				\
	HYP_SECTION_NAME(NAME) : {			\
		HYP_SECTION_SYMBOL_NAME(NAME) = .;
#define END_HYP_SECTION					\
	}
#define HYP_SECTION(NAME)			\
	BEGIN_HYP_SECTION(NAME)			\
		*(NAME NAME##.*)		\
	END_HYP_SECTION
#define KVM_NVHE_ALIAS(sym)	kvm_nvhe_sym(sym) = sym;
#define KVM_NVHE_ALIAS_HYP(first, sec)	kvm_nvhe_sym(first) = kvm_nvhe_sym(sec);
#endif  
#endif  
