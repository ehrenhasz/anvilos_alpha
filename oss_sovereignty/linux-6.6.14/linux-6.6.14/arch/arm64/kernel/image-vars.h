#ifndef __ARM64_KERNEL_IMAGE_VARS_H
#define __ARM64_KERNEL_IMAGE_VARS_H
#ifndef LINKER_SCRIPT
#error This file should only be included in vmlinux.lds.S
#endif
PROVIDE(__efistub_primary_entry		= primary_entry);
PROVIDE(__efistub_caches_clean_inval_pou = __pi_caches_clean_inval_pou);
PROVIDE(__efistub__text			= _text);
PROVIDE(__efistub__end			= _end);
PROVIDE(__efistub___inittext_end       	= __inittext_end);
PROVIDE(__efistub__edata		= _edata);
PROVIDE(__efistub_screen_info		= screen_info);
PROVIDE(__efistub__ctype		= _ctype);
PROVIDE(__pi___memcpy			= __pi_memcpy);
PROVIDE(__pi___memmove			= __pi_memmove);
PROVIDE(__pi___memset			= __pi_memset);
#ifdef CONFIG_KVM
KVM_NVHE_ALIAS(kvm_patch_vector_branch);
KVM_NVHE_ALIAS(kvm_update_va_mask);
KVM_NVHE_ALIAS(kvm_get_kimage_voffset);
KVM_NVHE_ALIAS(kvm_compute_final_ctr_el0);
KVM_NVHE_ALIAS(spectre_bhb_patch_loop_iter);
KVM_NVHE_ALIAS(spectre_bhb_patch_loop_mitigation_enable);
KVM_NVHE_ALIAS(spectre_bhb_patch_wa3);
KVM_NVHE_ALIAS(spectre_bhb_patch_clearbhb);
KVM_NVHE_ALIAS(alt_cb_patch_nops);
KVM_NVHE_ALIAS(kvm_vgic_global_state);
KVM_NVHE_ALIAS(nvhe_hyp_panic_handler);
KVM_NVHE_ALIAS(__hyp_stub_vectors);
KVM_NVHE_ALIAS(vgic_v2_cpuif_trap);
KVM_NVHE_ALIAS(vgic_v3_cpuif_trap);
#ifdef CONFIG_ARM64_PSEUDO_NMI
KVM_NVHE_ALIAS(gic_nonsecure_priorities);
#endif
KVM_NVHE_ALIAS(__start___kvm_ex_table);
KVM_NVHE_ALIAS(__stop___kvm_ex_table);
#ifdef CONFIG_HW_PERF_EVENTS
KVM_NVHE_ALIAS(kvm_arm_pmu_available);
#endif
KVM_NVHE_ALIAS_HYP(clear_page, __pi_clear_page);
KVM_NVHE_ALIAS_HYP(copy_page, __pi_copy_page);
KVM_NVHE_ALIAS_HYP(memcpy, __pi_memcpy);
KVM_NVHE_ALIAS_HYP(memset, __pi_memset);
#ifdef CONFIG_KASAN
KVM_NVHE_ALIAS_HYP(__memcpy, __pi_memcpy);
KVM_NVHE_ALIAS_HYP(__memset, __pi_memset);
#endif
KVM_NVHE_ALIAS(__hyp_idmap_text_start);
KVM_NVHE_ALIAS(__hyp_idmap_text_end);
KVM_NVHE_ALIAS(__hyp_text_start);
KVM_NVHE_ALIAS(__hyp_text_end);
KVM_NVHE_ALIAS(__hyp_bss_start);
KVM_NVHE_ALIAS(__hyp_bss_end);
KVM_NVHE_ALIAS(__hyp_rodata_start);
KVM_NVHE_ALIAS(__hyp_rodata_end);
KVM_NVHE_ALIAS(kvm_protected_mode_initialized);
#endif  
#ifdef CONFIG_EFI_ZBOOT
_kernel_codesize = ABSOLUTE(__inittext_end - _text);
#endif
#endif  
