
 

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/init.h>
#include <linux/module.h>
#include <linux/pci.h>

#include "cx25821.h"

 

struct cx25821_board cx25821_boards[] = {
	[UNKNOWN_BOARD] = {
		.name = "UNKNOWN/GENERIC",
		 
		.clk_freq = 0,
	},

	[CX25821_BOARD] = {
		.name = "CX25821",
		.portb = CX25821_RAW,
		.portc = CX25821_264,
	},

};
