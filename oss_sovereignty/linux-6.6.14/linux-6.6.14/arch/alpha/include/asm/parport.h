#ifndef _ASM_AXP_PARPORT_H
#define _ASM_AXP_PARPORT_H 1
static int parport_pc_find_isa_ports (int autoirq, int autodma);
static int parport_pc_find_nonpci_ports (int autoirq, int autodma)
{
	return parport_pc_find_isa_ports (autoirq, autodma);
}
#endif  
