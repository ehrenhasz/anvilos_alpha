 

 

#ifndef _TERMCAP_H
#define _TERMCAP_H 1

#if __STDC__

extern int tgetent (char *buffer, const char *termtype);

extern int tgetnum (const char *name);
extern int tgetflag (const char *name);
extern char *tgetstr (const char *name, char **area);

extern char PC;
extern short ospeed;
extern int tputs (const char *string, int nlines, int (*outfun) (int));

extern char *tparam (const char *ctlstring, char *buffer, int size, ...);

extern char *UP;
extern char *BC;

extern char *tgoto (const char *cstring, int hpos, int vpos);

#else  

extern int tgetent ();

extern int tgetnum ();
extern int tgetflag ();
extern char *tgetstr ();

extern char PC;
extern short ospeed;

extern void tputs ();

extern char *tparam ();

extern char *UP;
extern char *BC;

extern char *tgoto ();

#endif  

#endif  
