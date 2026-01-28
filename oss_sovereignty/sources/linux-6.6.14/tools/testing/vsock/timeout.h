
#ifndef TIMEOUT_H
#define TIMEOUT_H

enum {
	
	TIMEOUT = 10 
};

void sigalrm(int signo);
void timeout_begin(unsigned int seconds);
void timeout_check(const char *operation);
void timeout_end(void);

#endif 
