/* AnvilOS Fix: Defer to Ncurses but declare globals */
#include <ncurses/term.h>
#undef lines
#undef columns
#undef newline
extern char PC;
extern char *BC;
extern char *UP;
extern short ospeed;
