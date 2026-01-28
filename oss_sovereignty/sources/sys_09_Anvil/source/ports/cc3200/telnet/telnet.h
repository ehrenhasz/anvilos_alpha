
#ifndef MICROPY_INCLUDED_CC3200_TELNET_TELNET_H
#define MICROPY_INCLUDED_CC3200_TELNET_TELNET_H


extern void telnet_init (void);
extern void telnet_run (void);
extern void telnet_tx_strn (const char *str, int len);
extern bool telnet_rx_any (void);
extern int  telnet_rx_char (void);
extern void telnet_enable (void);
extern void telnet_disable (void);
extern void telnet_reset (void);

#endif 
