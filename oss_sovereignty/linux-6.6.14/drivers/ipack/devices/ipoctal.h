#ifndef _IPOCTAL_H_
#define _IPOCTAL_H_
#define NR_CHANNELS		8
#define IPOCTAL_MAX_BOARDS	16
#define MAX_DEVICES		(NR_CHANNELS * IPOCTAL_MAX_BOARDS)
struct ipoctal_stats {
	unsigned long tx;
	unsigned long rx;
	unsigned long overrun_err;
	unsigned long parity_err;
	unsigned long framing_err;
	unsigned long rcv_break;
};
#endif  
