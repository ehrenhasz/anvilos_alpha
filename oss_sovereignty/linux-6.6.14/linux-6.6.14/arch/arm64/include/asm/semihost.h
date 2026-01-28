#ifndef _ARM64_SEMIHOST_H_
#define _ARM64_SEMIHOST_H_
struct uart_port;
static inline void smh_putc(struct uart_port *port, unsigned char c)
{
	asm volatile("mov  x1, %0\n"
		     "mov  x0, #3\n"
		     "hlt  0xf000\n"
		     : : "r" (&c) : "x0", "x1", "memory");
}
#endif  
