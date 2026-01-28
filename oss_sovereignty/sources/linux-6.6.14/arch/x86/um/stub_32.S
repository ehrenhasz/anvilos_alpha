
#include <as-layout.h>

.section .__syscall_stub, "ax"

	.globl batch_syscall_stub
batch_syscall_stub:
	
	mov %esp, %ecx
	
	add $8, %esp
again:
	
	mov	0x0(%esp), %eax

	
	
	mov	%eax, 0x4(%ecx)
	cmpl	$0, %eax
	jz	done

	
	mov	%esp, 0x4(%ecx)

	
	add	%eax, %esp

	
	pop	%eax

	
	pop	%ebx
	pop	%ecx
	pop	%edx
	pop	%esi
 	pop	%edi
	pop	%ebp

	
	int	$0x80

	
	mov	%esp, %ecx
	andl	$(~UM_KERN_PAGE_SIZE) + 1, %ecx

	
	pop	%ebx
	cmp	%ebx, %eax
	je	again

done:
	
	mov	%eax, (%ecx)

	
	int3
