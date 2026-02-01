

#include <asm/machvec.h>
#include "8250.h"

bool alpha_jensen(void)
{
	return !strcmp(alpha_mv.vector_name, "Jensen");
}

void alpha_jensen_set_mctrl(struct uart_port *port, unsigned int mctrl)
{
	 
	mctrl |= TIOCM_OUT1 | TIOCM_OUT2;

	serial8250_do_set_mctrl(port, mctrl);
}
