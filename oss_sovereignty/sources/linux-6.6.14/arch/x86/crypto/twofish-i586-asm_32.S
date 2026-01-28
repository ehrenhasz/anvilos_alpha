


.file "twofish-i586-asm.S"
.text

#include <linux/linkage.h>
#include <asm/asm-offsets.h>



#define in_blk    12  
#define out_blk   8  
#define ctx       4  

#define a_offset	0
#define b_offset	4
#define c_offset	8
#define d_offset	12



#define s0	0	
#define s1	1024	
#define s2	2048	
#define s3	3072	
#define w	4096	
#define k	4128	



#define R0D    %eax
#define R0B    %al
#define R0H    %ah

#define R1D    %ebx
#define R1B    %bl
#define R1H    %bh

#define R2D    %ecx
#define R2B    %cl
#define R2H    %ch

#define R3D    %edx
#define R3B    %dl
#define R3H    %dh



#define input_whitening(src,context,offset)\
	xor	w+offset(context),	src;


#define output_whitening(src,context,offset)\
	xor	w+16+offset(context),	src;


#define encrypt_round(a,b,c,d,round)\
	push	d ## D;\
	movzx	b ## B,		%edi;\
	mov	s1(%ebp,%edi,4),d ## D;\
	movzx	a ## B,		%edi;\
	mov	s2(%ebp,%edi,4),%esi;\
	movzx	b ## H,		%edi;\
	ror	$16,		b ## D;\
	xor	s2(%ebp,%edi,4),d ## D;\
	movzx	a ## H,		%edi;\
	ror	$16,		a ## D;\
	xor	s3(%ebp,%edi,4),%esi;\
	movzx	b ## B,		%edi;\
	xor	s3(%ebp,%edi,4),d ## D;\
	movzx	a ## B,		%edi;\
	xor	(%ebp,%edi,4),	%esi;\
	movzx	b ## H,		%edi;\
	ror	$15,		b ## D;\
	xor	(%ebp,%edi,4),	d ## D;\
	movzx	a ## H,		%edi;\
	xor	s1(%ebp,%edi,4),%esi;\
	pop	%edi;\
	add	d ## D,		%esi;\
	add	%esi,		d ## D;\
	add	k+round(%ebp),	%esi;\
	xor	%esi,		c ## D;\
	rol	$15,		c ## D;\
	add	k+4+round(%ebp),d ## D;\
	xor	%edi,		d ## D;


#define encrypt_last_round(a,b,c,d,round)\
	push	d ## D;\
	movzx	b ## B,		%edi;\
	mov	s1(%ebp,%edi,4),d ## D;\
	movzx	a ## B,		%edi;\
	mov	s2(%ebp,%edi,4),%esi;\
	movzx	b ## H,		%edi;\
	ror	$16,		b ## D;\
	xor	s2(%ebp,%edi,4),d ## D;\
	movzx	a ## H,		%edi;\
	ror	$16,		a ## D;\
	xor	s3(%ebp,%edi,4),%esi;\
	movzx	b ## B,		%edi;\
	xor	s3(%ebp,%edi,4),d ## D;\
	movzx	a ## B,		%edi;\
	xor	(%ebp,%edi,4),	%esi;\
	movzx	b ## H,		%edi;\
	ror	$16,		b ## D;\
	xor	(%ebp,%edi,4),	d ## D;\
	movzx	a ## H,		%edi;\
	xor	s1(%ebp,%edi,4),%esi;\
	pop	%edi;\
	add	d ## D,		%esi;\
	add	%esi,		d ## D;\
	add	k+round(%ebp),	%esi;\
	xor	%esi,		c ## D;\
	ror	$1,		c ## D;\
	add	k+4+round(%ebp),d ## D;\
	xor	%edi,		d ## D;


#define decrypt_round(a,b,c,d,round)\
	push	c ## D;\
	movzx	a ## B,		%edi;\
	mov	(%ebp,%edi,4),	c ## D;\
	movzx	b ## B,		%edi;\
	mov	s3(%ebp,%edi,4),%esi;\
	movzx	a ## H,		%edi;\
	ror	$16,		a ## D;\
	xor	s1(%ebp,%edi,4),c ## D;\
	movzx	b ## H,		%edi;\
	ror	$16,		b ## D;\
	xor	(%ebp,%edi,4),	%esi;\
	movzx	a ## B,		%edi;\
	xor	s2(%ebp,%edi,4),c ## D;\
	movzx	b ## B,		%edi;\
	xor	s1(%ebp,%edi,4),%esi;\
	movzx	a ## H,		%edi;\
	ror	$15,		a ## D;\
	xor	s3(%ebp,%edi,4),c ## D;\
	movzx	b ## H,		%edi;\
	xor	s2(%ebp,%edi,4),%esi;\
	pop	%edi;\
	add	%esi,		c ## D;\
	add	c ## D,		%esi;\
	add	k+round(%ebp),	c ## D;\
	xor	%edi,		c ## D;\
	add	k+4+round(%ebp),%esi;\
	xor	%esi,		d ## D;\
	rol	$15,		d ## D;


#define decrypt_last_round(a,b,c,d,round)\
	push	c ## D;\
	movzx	a ## B,		%edi;\
	mov	(%ebp,%edi,4),	c ## D;\
	movzx	b ## B,		%edi;\
	mov	s3(%ebp,%edi,4),%esi;\
	movzx	a ## H,		%edi;\
	ror	$16,		a ## D;\
	xor	s1(%ebp,%edi,4),c ## D;\
	movzx	b ## H,		%edi;\
	ror	$16,		b ## D;\
	xor	(%ebp,%edi,4),	%esi;\
	movzx	a ## B,		%edi;\
	xor	s2(%ebp,%edi,4),c ## D;\
	movzx	b ## B,		%edi;\
	xor	s1(%ebp,%edi,4),%esi;\
	movzx	a ## H,		%edi;\
	ror	$16,		a ## D;\
	xor	s3(%ebp,%edi,4),c ## D;\
	movzx	b ## H,		%edi;\
	xor	s2(%ebp,%edi,4),%esi;\
	pop	%edi;\
	add	%esi,		c ## D;\
	add	c ## D,		%esi;\
	add	k+round(%ebp),	c ## D;\
	xor	%edi,		c ## D;\
	add	k+4+round(%ebp),%esi;\
	xor	%esi,		d ## D;\
	ror	$1,		d ## D;

SYM_FUNC_START(twofish_enc_blk)
	push	%ebp			
	push    %ebx
	push    %esi
	push    %edi

	mov	ctx + 16(%esp),	%ebp	
	mov     in_blk+16(%esp),%edi	

	mov	(%edi),		%eax
	mov	b_offset(%edi),	%ebx
	mov	c_offset(%edi),	%ecx
	mov	d_offset(%edi),	%edx
	input_whitening(%eax,%ebp,a_offset)
	ror	$16,	%eax
	input_whitening(%ebx,%ebp,b_offset)
	input_whitening(%ecx,%ebp,c_offset)
	input_whitening(%edx,%ebp,d_offset)
	rol	$1,	%edx

	encrypt_round(R0,R1,R2,R3,0);
	encrypt_round(R2,R3,R0,R1,8);
	encrypt_round(R0,R1,R2,R3,2*8);
	encrypt_round(R2,R3,R0,R1,3*8);
	encrypt_round(R0,R1,R2,R3,4*8);
	encrypt_round(R2,R3,R0,R1,5*8);
	encrypt_round(R0,R1,R2,R3,6*8);
	encrypt_round(R2,R3,R0,R1,7*8);
	encrypt_round(R0,R1,R2,R3,8*8);
	encrypt_round(R2,R3,R0,R1,9*8);
	encrypt_round(R0,R1,R2,R3,10*8);
	encrypt_round(R2,R3,R0,R1,11*8);
	encrypt_round(R0,R1,R2,R3,12*8);
	encrypt_round(R2,R3,R0,R1,13*8);
	encrypt_round(R0,R1,R2,R3,14*8);
	encrypt_last_round(R2,R3,R0,R1,15*8);

	output_whitening(%eax,%ebp,c_offset)
	output_whitening(%ebx,%ebp,d_offset)
	output_whitening(%ecx,%ebp,a_offset)
	output_whitening(%edx,%ebp,b_offset)
	mov	out_blk+16(%esp),%edi;
	mov	%eax,		c_offset(%edi)
	mov	%ebx,		d_offset(%edi)
	mov	%ecx,		(%edi)
	mov	%edx,		b_offset(%edi)

	pop	%edi
	pop	%esi
	pop	%ebx
	pop	%ebp
	mov	$1,	%eax
	RET
SYM_FUNC_END(twofish_enc_blk)

SYM_FUNC_START(twofish_dec_blk)
	push	%ebp			
	push    %ebx
	push    %esi
	push    %edi


	mov	ctx + 16(%esp),	%ebp	
	mov     in_blk+16(%esp),%edi	

	mov	(%edi),		%eax
	mov	b_offset(%edi),	%ebx
	mov	c_offset(%edi),	%ecx
	mov	d_offset(%edi),	%edx
	output_whitening(%eax,%ebp,a_offset)
	output_whitening(%ebx,%ebp,b_offset)
	ror	$16,	%ebx
	output_whitening(%ecx,%ebp,c_offset)
	output_whitening(%edx,%ebp,d_offset)
	rol	$1,	%ecx

	decrypt_round(R0,R1,R2,R3,15*8);
	decrypt_round(R2,R3,R0,R1,14*8);
	decrypt_round(R0,R1,R2,R3,13*8);
	decrypt_round(R2,R3,R0,R1,12*8);
	decrypt_round(R0,R1,R2,R3,11*8);
	decrypt_round(R2,R3,R0,R1,10*8);
	decrypt_round(R0,R1,R2,R3,9*8);
	decrypt_round(R2,R3,R0,R1,8*8);
	decrypt_round(R0,R1,R2,R3,7*8);
	decrypt_round(R2,R3,R0,R1,6*8);
	decrypt_round(R0,R1,R2,R3,5*8);
	decrypt_round(R2,R3,R0,R1,4*8);
	decrypt_round(R0,R1,R2,R3,3*8);
	decrypt_round(R2,R3,R0,R1,2*8);
	decrypt_round(R0,R1,R2,R3,1*8);
	decrypt_last_round(R2,R3,R0,R1,0);

	input_whitening(%eax,%ebp,c_offset)
	input_whitening(%ebx,%ebp,d_offset)
	input_whitening(%ecx,%ebp,a_offset)
	input_whitening(%edx,%ebp,b_offset)
	mov	out_blk+16(%esp),%edi;
	mov	%eax,		c_offset(%edi)
	mov	%ebx,		d_offset(%edi)
	mov	%ecx,		(%edi)
	mov	%edx,		b_offset(%edi)

	pop	%edi
	pop	%esi
	pop	%ebx
	pop	%ebp
	mov	$1,	%eax
	RET
SYM_FUNC_END(twofish_dec_blk)
