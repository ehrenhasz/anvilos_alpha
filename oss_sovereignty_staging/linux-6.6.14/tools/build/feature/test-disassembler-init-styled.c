
#include <stdio.h>
#include <dis-asm.h>

int main(void)
{
	struct disassemble_info info;

	init_disassemble_info(&info, stdout,
			      NULL, NULL);

	return 0;
}
