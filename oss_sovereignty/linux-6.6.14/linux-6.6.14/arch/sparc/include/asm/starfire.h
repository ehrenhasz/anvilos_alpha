#ifndef _SPARC64_STARFIRE_H
#define _SPARC64_STARFIRE_H
#ifndef __ASSEMBLY__
extern int this_is_starfire;
void check_if_starfire(void);
void starfire_hookup(int);
unsigned int starfire_translate(unsigned long imap, unsigned int upaid);
#endif
#endif
