#ifndef __KVM_S390_GACCESS_H
#define __KVM_S390_GACCESS_H
#include <linux/compiler.h>
#include <linux/kvm_host.h>
#include <linux/uaccess.h>
#include <linux/ptrace.h>
#include "kvm-s390.h"
static inline unsigned long _kvm_s390_real_to_abs(u32 prefix, unsigned long gra)
{
	if (gra < 2 * PAGE_SIZE)
		gra += prefix;
	else if (gra >= prefix && gra < prefix + 2 * PAGE_SIZE)
		gra -= prefix;
	return gra;
}
static inline unsigned long kvm_s390_real_to_abs(struct kvm_vcpu *vcpu,
						 unsigned long gra)
{
	return _kvm_s390_real_to_abs(kvm_s390_get_prefix(vcpu), gra);
}
static inline unsigned long _kvm_s390_logical_to_effective(psw_t *psw,
							   unsigned long ga)
{
	if (psw_bits(*psw).eaba == PSW_BITS_AMODE_64BIT)
		return ga;
	if (psw_bits(*psw).eaba == PSW_BITS_AMODE_31BIT)
		return ga & ((1UL << 31) - 1);
	return ga & ((1UL << 24) - 1);
}
static inline unsigned long kvm_s390_logical_to_effective(struct kvm_vcpu *vcpu,
							  unsigned long ga)
{
	return _kvm_s390_logical_to_effective(&vcpu->arch.sie_block->gpsw, ga);
}
#define put_guest_lc(vcpu, x, gra)				\
({								\
	struct kvm_vcpu *__vcpu = (vcpu);			\
	__typeof__(*(gra)) __x = (x);				\
	unsigned long __gpa;					\
								\
	__gpa = (unsigned long)(gra);				\
	__gpa += kvm_s390_get_prefix(__vcpu);			\
	kvm_write_guest(__vcpu->kvm, __gpa, &__x, sizeof(__x));	\
})
static inline __must_check
int write_guest_lc(struct kvm_vcpu *vcpu, unsigned long gra, void *data,
		   unsigned long len)
{
	unsigned long gpa = gra + kvm_s390_get_prefix(vcpu);
	return kvm_write_guest(vcpu->kvm, gpa, data, len);
}
static inline __must_check
int read_guest_lc(struct kvm_vcpu *vcpu, unsigned long gra, void *data,
		  unsigned long len)
{
	unsigned long gpa = gra + kvm_s390_get_prefix(vcpu);
	return kvm_read_guest(vcpu->kvm, gpa, data, len);
}
enum gacc_mode {
	GACC_FETCH,
	GACC_STORE,
	GACC_IFETCH,
};
int guest_translate_address_with_key(struct kvm_vcpu *vcpu, unsigned long gva, u8 ar,
				     unsigned long *gpa, enum gacc_mode mode,
				     u8 access_key);
int check_gva_range(struct kvm_vcpu *vcpu, unsigned long gva, u8 ar,
		    unsigned long length, enum gacc_mode mode, u8 access_key);
int check_gpa_range(struct kvm *kvm, unsigned long gpa, unsigned long length,
		    enum gacc_mode mode, u8 access_key);
int access_guest_abs_with_key(struct kvm *kvm, gpa_t gpa, void *data,
			      unsigned long len, enum gacc_mode mode, u8 access_key);
int access_guest_with_key(struct kvm_vcpu *vcpu, unsigned long ga, u8 ar,
			  void *data, unsigned long len, enum gacc_mode mode,
			  u8 access_key);
int access_guest_real(struct kvm_vcpu *vcpu, unsigned long gra,
		      void *data, unsigned long len, enum gacc_mode mode);
int cmpxchg_guest_abs_with_key(struct kvm *kvm, gpa_t gpa, int len, __uint128_t *old,
			       __uint128_t new, u8 access_key, bool *success);
static inline __must_check
int write_guest_with_key(struct kvm_vcpu *vcpu, unsigned long ga, u8 ar,
			 void *data, unsigned long len, u8 access_key)
{
	return access_guest_with_key(vcpu, ga, ar, data, len, GACC_STORE,
				     access_key);
}
static inline __must_check
int write_guest(struct kvm_vcpu *vcpu, unsigned long ga, u8 ar, void *data,
		unsigned long len)
{
	u8 access_key = psw_bits(vcpu->arch.sie_block->gpsw).key;
	return write_guest_with_key(vcpu, ga, ar, data, len, access_key);
}
static inline __must_check
int read_guest_with_key(struct kvm_vcpu *vcpu, unsigned long ga, u8 ar,
			void *data, unsigned long len, u8 access_key)
{
	return access_guest_with_key(vcpu, ga, ar, data, len, GACC_FETCH,
				     access_key);
}
static inline __must_check
int read_guest(struct kvm_vcpu *vcpu, unsigned long ga, u8 ar, void *data,
	       unsigned long len)
{
	u8 access_key = psw_bits(vcpu->arch.sie_block->gpsw).key;
	return read_guest_with_key(vcpu, ga, ar, data, len, access_key);
}
static inline __must_check
int read_guest_instr(struct kvm_vcpu *vcpu, unsigned long ga, void *data,
		     unsigned long len)
{
	u8 access_key = psw_bits(vcpu->arch.sie_block->gpsw).key;
	return access_guest_with_key(vcpu, ga, 0, data, len, GACC_IFETCH,
				     access_key);
}
static inline __must_check
int write_guest_abs(struct kvm_vcpu *vcpu, unsigned long gpa, void *data,
		    unsigned long len)
{
	return kvm_write_guest(vcpu->kvm, gpa, data, len);
}
static inline __must_check
int read_guest_abs(struct kvm_vcpu *vcpu, unsigned long gpa, void *data,
		   unsigned long len)
{
	return kvm_read_guest(vcpu->kvm, gpa, data, len);
}
static inline __must_check
int write_guest_real(struct kvm_vcpu *vcpu, unsigned long gra, void *data,
		     unsigned long len)
{
	return access_guest_real(vcpu, gra, data, len, 1);
}
static inline __must_check
int read_guest_real(struct kvm_vcpu *vcpu, unsigned long gra, void *data,
		    unsigned long len)
{
	return access_guest_real(vcpu, gra, data, len, 0);
}
void ipte_lock(struct kvm *kvm);
void ipte_unlock(struct kvm *kvm);
int ipte_lock_held(struct kvm *kvm);
int kvm_s390_check_low_addr_prot_real(struct kvm_vcpu *vcpu, unsigned long gra);
#define PEI_DAT_PROT 2
#define PEI_NOT_PTE 4
int kvm_s390_shadow_fault(struct kvm_vcpu *vcpu, struct gmap *shadow,
			  unsigned long saddr, unsigned long *datptr);
#endif  
