


#if   defined ( __ICCARM__ )
  #pragma system_include         
#elif defined (__clang__)
  #pragma clang system_header    
#endif

#ifndef PAC_ARMV81_H
#define PAC_ARMV81_H





#if (defined (__ARM_FEATURE_PAUTH) && (__ARM_FEATURE_PAUTH == 1))


__STATIC_FORCEINLINE void __get_PAC_KEY_P (uint32_t* pPacKey) {
  __ASM volatile (
  "mrs   r1, pac_key_p_0\n"
  "str   r1,[%0,#0]\n"
  "mrs   r1, pac_key_p_1\n"
  "str   r1,[%0,#4]\n"
  "mrs   r1, pac_key_p_2\n"
  "str   r1,[%0,#8]\n"
  "mrs   r1, pac_key_p_3\n"
  "str   r1,[%0,#12]\n"
  : : "r" (pPacKey) : "memory", "r1"
  );
}


__STATIC_FORCEINLINE void __set_PAC_KEY_P (uint32_t* pPacKey) {
  __ASM volatile (
  "ldr   r1,[%0,#0]\n"
  "msr   pac_key_p_0, r1\n"
  "ldr   r1,[%0,#4]\n"
  "msr   pac_key_p_1, r1\n"
  "ldr   r1,[%0,#8]\n"
  "msr   pac_key_p_2, r1\n"
  "ldr   r1,[%0,#12]\n"
  "msr   pac_key_p_3, r1\n"
  : : "r" (pPacKey) : "memory", "r1"
  );
}


__STATIC_FORCEINLINE void __get_PAC_KEY_U (uint32_t* pPacKey) {
  __ASM volatile (
  "mrs   r1, pac_key_u_0\n"
  "str   r1,[%0,#0]\n"
  "mrs   r1, pac_key_u_1\n"
  "str   r1,[%0,#4]\n"
  "mrs   r1, pac_key_u_2\n"
  "str   r1,[%0,#8]\n"
  "mrs   r1, pac_key_u_3\n"
  "str   r1,[%0,#12]\n"
  : : "r" (pPacKey) : "memory", "r1"
  );
}


__STATIC_FORCEINLINE void __set_PAC_KEY_U (uint32_t* pPacKey) {
  __ASM volatile (
  "ldr   r1,[%0,#0]\n"
  "msr   pac_key_u_0, r1\n"
  "ldr   r1,[%0,#4]\n"
  "msr   pac_key_u_1, r1\n"
  "ldr   r1,[%0,#8]\n"
  "msr   pac_key_u_2, r1\n"
  "ldr   r1,[%0,#12]\n"
  "msr   pac_key_u_3, r1\n"
  : : "r" (pPacKey) : "memory", "r1"
  );
}

#if (defined (__ARM_FEATURE_CMSE ) && (__ARM_FEATURE_CMSE == 3))


__STATIC_FORCEINLINE void __TZ_get_PAC_KEY_P_NS (uint32_t* pPacKey) {
  __ASM volatile (
  "mrs   r1, pac_key_p_0_ns\n"
  "str   r1,[%0,#0]\n"
  "mrs   r1, pac_key_p_1_ns\n"
  "str   r1,[%0,#4]\n"
  "mrs   r1, pac_key_p_2_ns\n"
  "str   r1,[%0,#8]\n"
  "mrs   r1, pac_key_p_3_ns\n"
  "str   r1,[%0,#12]\n"
  : : "r" (pPacKey) : "memory", "r1"
  );
}


__STATIC_FORCEINLINE void __TZ_set_PAC_KEY_P_NS (uint32_t* pPacKey) {
  __ASM volatile (
  "ldr   r1,[%0,#0]\n"
  "msr   pac_key_p_0_ns, r1\n"
  "ldr   r1,[%0,#4]\n"
  "msr   pac_key_p_1_ns, r1\n"
  "ldr   r1,[%0,#8]\n"
  "msr   pac_key_p_2_ns, r1\n"
  "ldr   r1,[%0,#12]\n"
  "msr   pac_key_p_3_ns, r1\n"
  : : "r" (pPacKey) : "memory", "r1"
  );
}


__STATIC_FORCEINLINE void __TZ_get_PAC_KEY_U_NS (uint32_t* pPacKey) {
  __ASM volatile (
  "mrs   r1, pac_key_u_0_ns\n"
  "str   r1,[%0,#0]\n"
  "mrs   r1, pac_key_u_1_ns\n"
  "str   r1,[%0,#4]\n"
  "mrs   r1, pac_key_u_2_ns\n"
  "str   r1,[%0,#8]\n"
  "mrs   r1, pac_key_u_3_ns\n"
  "str   r1,[%0,#12]\n"
  : : "r" (pPacKey) : "memory", "r1"
  );
}


__STATIC_FORCEINLINE void __TZ_set_PAC_KEY_U_NS (uint32_t* pPacKey) {
  __ASM volatile (
  "ldr   r1,[%0,#0]\n"
  "msr   pac_key_u_0_ns, r1\n"
  "ldr   r1,[%0,#4]\n"
  "msr   pac_key_u_1_ns, r1\n"
  "ldr   r1,[%0,#8]\n"
  "msr   pac_key_u_2_ns, r1\n"
  "ldr   r1,[%0,#12]\n"
  "msr   pac_key_u_3_ns, r1\n"
  : : "r" (pPacKey) : "memory", "r1"
  );
}

#endif 

#endif 




#endif 
