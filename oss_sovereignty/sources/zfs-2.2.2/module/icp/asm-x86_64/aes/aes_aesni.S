






#if defined(lint) || defined(__lint)

#include <sys/types.h>

void
aes_encrypt_intel(const uint32_t rk[], int Nr, const uint32_t pt[4],
    uint32_t ct[4]) {
	(void) rk, (void) Nr, (void) pt, (void) ct;
}
void
aes_decrypt_intel(const uint32_t rk[], int Nr, const uint32_t ct[4],
    uint32_t pt[4]) {
	(void) rk, (void) Nr, (void) ct, (void) pt;
}
int
rijndael_key_setup_enc_intel(uint32_t rk[], const uint32_t cipherKey[],
    uint64_t keyBits) {
	(void) rk, (void) cipherKey, (void) keyBits;
	return (0);
}
int
rijndael_key_setup_dec_intel(uint32_t rk[], const uint32_t cipherKey[],
   uint64_t keyBits) {
	(void) rk, (void) cipherKey, (void) keyBits;
	return (0);
}


#elif defined(HAVE_AES)	

#define _ASM
#include <sys/asm_linkage.h>



ENTRY_NP2(_key_expansion_128, _key_expansion_256a)
_key_expansion_128_local:
_key_expansion_256a_local:
	pshufd	$0b11111111, %xmm1, %xmm1
	shufps	$0b00010000, %xmm0, %xmm4
	pxor	%xmm4, %xmm0
	shufps	$0b10001100, %xmm0, %xmm4
	pxor	%xmm4, %xmm0
	pxor	%xmm1, %xmm0
	movups	%xmm0, (%rcx)
	add	$0x10, %rcx
	RET
	nop
SET_SIZE(_key_expansion_128)
SET_SIZE(_key_expansion_256a)


ENTRY_NP(_key_expansion_192a)
_key_expansion_192a_local:
	pshufd	$0b01010101, %xmm1, %xmm1
	shufps	$0b00010000, %xmm0, %xmm4
	pxor	%xmm4, %xmm0
	shufps	$0b10001100, %xmm0, %xmm4
	pxor	%xmm4, %xmm0
	pxor	%xmm1, %xmm0

	movups	%xmm2, %xmm5
	movups	%xmm2, %xmm6
	pslldq	$4, %xmm5
	pshufd	$0b11111111, %xmm0, %xmm3
	pxor	%xmm3, %xmm2
	pxor	%xmm5, %xmm2

	movups	%xmm0, %xmm1
	shufps	$0b01000100, %xmm0, %xmm6
	movups	%xmm6, (%rcx)
	shufps	$0b01001110, %xmm2, %xmm1
	movups	%xmm1, 0x10(%rcx)
	add	$0x20, %rcx
	RET
SET_SIZE(_key_expansion_192a)


ENTRY_NP(_key_expansion_192b)
_key_expansion_192b_local:
	pshufd	$0b01010101, %xmm1, %xmm1
	shufps	$0b00010000, %xmm0, %xmm4
	pxor	%xmm4, %xmm0
	shufps	$0b10001100, %xmm0, %xmm4
	pxor	%xmm4, %xmm0
	pxor	%xmm1, %xmm0

	movups	%xmm2, %xmm5
	pslldq	$4, %xmm5
	pshufd	$0b11111111, %xmm0, %xmm3
	pxor	%xmm3, %xmm2
	pxor	%xmm5, %xmm2

	movups	%xmm0, (%rcx)
	add	$0x10, %rcx
	RET
SET_SIZE(_key_expansion_192b)


ENTRY_NP(_key_expansion_256b)
_key_expansion_256b_local:
	pshufd	$0b10101010, %xmm1, %xmm1
	shufps	$0b00010000, %xmm2, %xmm4
	pxor	%xmm4, %xmm2
	shufps	$0b10001100, %xmm2, %xmm4
	pxor	%xmm4, %xmm2
	pxor	%xmm1, %xmm2
	movups	%xmm2, (%rcx)
	add	$0x10, %rcx
	RET
SET_SIZE(_key_expansion_256b)




#ifdef	OPENSSL_INTERFACE
#define	rijndael_key_setup_enc_intel	intel_AES_set_encrypt_key
#define	rijndael_key_setup_dec_intel	intel_AES_set_decrypt_key

#define	USERCIPHERKEY		rdi	
#define	KEYSIZE32		esi	
#define	KEYSIZE64		rsi	
#define	AESKEY			rdx	

#else	
#define	AESKEY			rdi	
#define	USERCIPHERKEY		rsi	
#define	KEYSIZE32		edx	
#define	KEYSIZE64		rdx	
#endif	

#define	ROUNDS32		KEYSIZE32	
#define	ROUNDS64		KEYSIZE64	
#define	ENDAESKEY		USERCIPHERKEY	

ENTRY_NP(rijndael_key_setup_enc_intel)
rijndael_key_setup_enc_intel_local:
	FRAME_BEGIN
	
	test	%USERCIPHERKEY, %USERCIPHERKEY
	jz	.Lenc_key_invalid_param
	test	%AESKEY, %AESKEY
	jz	.Lenc_key_invalid_param

	movups	(%USERCIPHERKEY), %xmm0	
	movups	%xmm0, (%AESKEY)
	lea	0x10(%AESKEY), %rcx	
	pxor	%xmm4, %xmm4		

	cmp	$256, %KEYSIZE32
	jnz	.Lenc_key192

	
#ifdef OPENSSL_INTERFACE
	mov	$14, %ROUNDS32
	movl	%ROUNDS32, 240(%AESKEY)		
#endif	

	movups	0x10(%USERCIPHERKEY), %xmm2	
	movups	%xmm2, (%rcx)
	add	$0x10, %rcx

	aeskeygenassist $0x1, %xmm2, %xmm1	
	call	_key_expansion_256a_local
	aeskeygenassist $0x1, %xmm0, %xmm1
	call	_key_expansion_256b_local
	aeskeygenassist $0x2, %xmm2, %xmm1	
	call	_key_expansion_256a_local
	aeskeygenassist $0x2, %xmm0, %xmm1
	call	_key_expansion_256b_local
	aeskeygenassist $0x4, %xmm2, %xmm1	
	call	_key_expansion_256a_local
	aeskeygenassist $0x4, %xmm0, %xmm1
	call	_key_expansion_256b_local
	aeskeygenassist $0x8, %xmm2, %xmm1	
	call	_key_expansion_256a_local
	aeskeygenassist $0x8, %xmm0, %xmm1
	call	_key_expansion_256b_local
	aeskeygenassist $0x10, %xmm2, %xmm1	
	call	_key_expansion_256a_local
	aeskeygenassist $0x10, %xmm0, %xmm1
	call	_key_expansion_256b_local
	aeskeygenassist $0x20, %xmm2, %xmm1	
	call	_key_expansion_256a_local
	aeskeygenassist $0x20, %xmm0, %xmm1
	call	_key_expansion_256b_local
	aeskeygenassist $0x40, %xmm2, %xmm1	
	call	_key_expansion_256a_local

#ifdef	OPENSSL_INTERFACE
	xor	%rax, %rax			
#else	
	mov	$14, %rax			
#endif
	FRAME_END
	RET

.balign 4
.Lenc_key192:
	cmp	$192, %KEYSIZE32
	jnz	.Lenc_key128

	
#ifdef OPENSSL_INTERFACE
	mov	$12, %ROUNDS32
	movl	%ROUNDS32, 240(%AESKEY)	
#endif	

	movq	0x10(%USERCIPHERKEY), %xmm2	
	aeskeygenassist $0x1, %xmm2, %xmm1	
	call	_key_expansion_192a_local
	aeskeygenassist $0x2, %xmm2, %xmm1	
	call	_key_expansion_192b_local
	aeskeygenassist $0x4, %xmm2, %xmm1	
	call	_key_expansion_192a_local
	aeskeygenassist $0x8, %xmm2, %xmm1	
	call	_key_expansion_192b_local
	aeskeygenassist $0x10, %xmm2, %xmm1	
	call	_key_expansion_192a_local
	aeskeygenassist $0x20, %xmm2, %xmm1	
	call	_key_expansion_192b_local
	aeskeygenassist $0x40, %xmm2, %xmm1	
	call	_key_expansion_192a_local
	aeskeygenassist $0x80, %xmm2, %xmm1	
	call	_key_expansion_192b_local

#ifdef	OPENSSL_INTERFACE
	xor	%rax, %rax			
#else	
	mov	$12, %rax			
#endif
	FRAME_END
	RET

.balign 4
.Lenc_key128:
	cmp $128, %KEYSIZE32
	jnz .Lenc_key_invalid_key_bits

	
#ifdef OPENSSL_INTERFACE
	mov	$10, %ROUNDS32
	movl	%ROUNDS32, 240(%AESKEY)		
#endif	

	aeskeygenassist $0x1, %xmm0, %xmm1	
	call	_key_expansion_128_local
	aeskeygenassist $0x2, %xmm0, %xmm1	
	call	_key_expansion_128_local
	aeskeygenassist $0x4, %xmm0, %xmm1	
	call	_key_expansion_128_local
	aeskeygenassist $0x8, %xmm0, %xmm1	
	call	_key_expansion_128_local
	aeskeygenassist $0x10, %xmm0, %xmm1	
	call	_key_expansion_128_local
	aeskeygenassist $0x20, %xmm0, %xmm1	
	call	_key_expansion_128_local
	aeskeygenassist $0x40, %xmm0, %xmm1	
	call	_key_expansion_128_local
	aeskeygenassist $0x80, %xmm0, %xmm1	
	call	_key_expansion_128_local
	aeskeygenassist $0x1b, %xmm0, %xmm1	
	call	_key_expansion_128_local
	aeskeygenassist $0x36, %xmm0, %xmm1	
	call	_key_expansion_128_local

#ifdef	OPENSSL_INTERFACE
	xor	%rax, %rax			
#else	
	mov	$10, %rax			
#endif
	FRAME_END
	RET

.Lenc_key_invalid_param:
#ifdef	OPENSSL_INTERFACE
	mov	$-1, %rax	
	FRAME_END
	RET
#else
	
#endif	

.Lenc_key_invalid_key_bits:
#ifdef	OPENSSL_INTERFACE
	mov	$-2, %rax	
#else	
	xor	%rax, %rax	
#endif	
	FRAME_END
	RET
	SET_SIZE(rijndael_key_setup_enc_intel)




ENTRY_NP(rijndael_key_setup_dec_intel)
FRAME_BEGIN
	
	call	rijndael_key_setup_enc_intel_local
	test	%rax, %rax
#ifdef	OPENSSL_INTERFACE
	jnz	.Ldec_key_exit	
#else	
	jz	.Ldec_key_exit	
#endif	

	
#ifndef	OPENSSL_INTERFACE		
	mov	%rax, %ROUNDS64		
					
#endif

	lea	0x10(%AESKEY), %rcx	
	shl	$4, %ROUNDS32
	add	%AESKEY, %ROUNDS64
	mov	%ROUNDS64, %ENDAESKEY

.balign 4
.Ldec_key_reorder_loop:
	movups	(%AESKEY), %xmm0
	movups	(%ROUNDS64), %xmm1
	movups	%xmm0, (%ROUNDS64)
	movups	%xmm1, (%AESKEY)
	lea	0x10(%AESKEY), %AESKEY
	lea	-0x10(%ROUNDS64), %ROUNDS64
	cmp	%AESKEY, %ROUNDS64
	ja	.Ldec_key_reorder_loop

.balign 4
.Ldec_key_inv_loop:
	movups	(%rcx), %xmm0
	
	
	aesimc	%xmm0, %xmm1
	movups	%xmm1, (%rcx)
	lea	0x10(%rcx), %rcx
	cmp	%ENDAESKEY, %rcx
	jnz	.Ldec_key_inv_loop

.Ldec_key_exit:
	
	
	FRAME_END
	RET
	SET_SIZE(rijndael_key_setup_dec_intel)




#ifdef	OPENSSL_INTERFACE
#define	aes_encrypt_intel	intel_AES_encrypt
#define	aes_decrypt_intel	intel_AES_decrypt

#define	INP		rdi	
#define	OUTP		rsi	
#define	KEYP		rdx	


#define	NROUNDS32	ecx	
#define	NROUNDS		cl	

#else	
#define	KEYP		rdi	
#define	NROUNDS		esi	
#define	INP		rdx	
#define	OUTP		rcx	
#endif	

#define	STATE		xmm0	
#define	KEY		xmm1	


ENTRY_NP(aes_encrypt_intel)

	movups	(%INP), %STATE			
	movups	(%KEYP), %KEY			
#ifdef	OPENSSL_INTERFACE
	mov	240(%KEYP), %NROUNDS32		
#else	
	
#endif	

	pxor	%KEY, %STATE			
	lea	0x30(%KEYP), %KEYP
	cmp	$12, %NROUNDS
	jb	.Lenc128
	lea	0x20(%KEYP), %KEYP
	je	.Lenc192

	
	lea	0x20(%KEYP), %KEYP
	movups	-0x60(%KEYP), %KEY
	aesenc	%KEY, %STATE
	movups	-0x50(%KEYP), %KEY
	aesenc	%KEY, %STATE

.balign 4
.Lenc192:
	
	movups	-0x40(%KEYP), %KEY
	aesenc	%KEY, %STATE
	movups	-0x30(%KEYP), %KEY
	aesenc	%KEY, %STATE

.balign 4
.Lenc128:
	
	movups	-0x20(%KEYP), %KEY
	aesenc	%KEY, %STATE
	movups	-0x10(%KEYP), %KEY
	aesenc	%KEY, %STATE
	movups	(%KEYP), %KEY
	aesenc	%KEY, %STATE
	movups	0x10(%KEYP), %KEY
	aesenc	%KEY, %STATE
	movups	0x20(%KEYP), %KEY
	aesenc	%KEY, %STATE
	movups	0x30(%KEYP), %KEY
	aesenc	%KEY, %STATE
	movups	0x40(%KEYP), %KEY
	aesenc	%KEY, %STATE
	movups	0x50(%KEYP), %KEY
	aesenc	%KEY, %STATE
	movups	0x60(%KEYP), %KEY
	aesenc	%KEY, %STATE
	movups	0x70(%KEYP), %KEY
	aesenclast	 %KEY, %STATE		
	movups	%STATE, (%OUTP)			

	RET
	SET_SIZE(aes_encrypt_intel)



ENTRY_NP(aes_decrypt_intel)

	movups	(%INP), %STATE			
	movups	(%KEYP), %KEY			
#ifdef	OPENSSL_INTERFACE
	mov	240(%KEYP), %NROUNDS32		
#else	
	
#endif	

	pxor	%KEY, %STATE			
	lea	0x30(%KEYP), %KEYP
	cmp	$12, %NROUNDS
	jb	.Ldec128
	lea	0x20(%KEYP), %KEYP
	je	.Ldec192

	
	lea	0x20(%KEYP), %KEYP
	movups	-0x60(%KEYP), %KEY
	aesdec	%KEY, %STATE
	movups	-0x50(%KEYP), %KEY
	aesdec	%KEY, %STATE

.balign 4
.Ldec192:
	
	movups	-0x40(%KEYP), %KEY
	aesdec	%KEY, %STATE
	movups	-0x30(%KEYP), %KEY
	aesdec	%KEY, %STATE

.balign 4
.Ldec128:
	
	movups	-0x20(%KEYP), %KEY
	aesdec	%KEY, %STATE
	movups	-0x10(%KEYP), %KEY
	aesdec	%KEY, %STATE
	movups	(%KEYP), %KEY
	aesdec	%KEY, %STATE
	movups	0x10(%KEYP), %KEY
	aesdec	%KEY, %STATE
	movups	0x20(%KEYP), %KEY
	aesdec	%KEY, %STATE
	movups	0x30(%KEYP), %KEY
	aesdec	%KEY, %STATE
	movups	0x40(%KEYP), %KEY
	aesdec	%KEY, %STATE
	movups	0x50(%KEYP), %KEY
	aesdec	%KEY, %STATE
	movups	0x60(%KEYP), %KEY
	aesdec	%KEY, %STATE
	movups	0x70(%KEYP), %KEY
	aesdeclast	%KEY, %STATE		
	movups	%STATE, (%OUTP)			

	RET
	SET_SIZE(aes_decrypt_intel)

#endif	

#ifdef __ELF__
.section .note.GNU-stack,"",%progbits
#endif
