

 

#ifndef NCURSES_CURSESW_H_incl
#define NCURSES_CURSESW_H_incl 1



extern "C" {
#  include   <curses.h>
}

#if defined(BUILDING_NCURSES_CXX)
# define NCURSES_CXX_IMPEXP NCURSES_EXPORT_GENERAL_EXPORT
#else
# define NCURSES_CXX_IMPEXP NCURSES_EXPORT_GENERAL_IMPORT
#endif

#define NCURSES_CXX_WRAPPED_VAR(type,name) extern NCURSES_CXX_IMPEXP type NCURSES_PUBLIC_VAR(name)(void)

#define NCURSES_CXX_EXPORT(type) NCURSES_CXX_IMPEXP type NCURSES_API
#define NCURSES_CXX_EXPORT_VAR(type) NCURSES_CXX_IMPEXP type

#include <etip.h>

 
#undef lines

 
#undef UNDEF
#define UNDEF(name) CUR_ ##name

#ifdef addch
inline int UNDEF(addch)(chtype ch)  { return addch(ch); }
#undef addch
#define addch UNDEF(addch)
#endif

#ifdef addchstr
inline int UNDEF(addchstr)(chtype *at) { return addchstr(at); }
#undef addchstr
#define addchstr UNDEF(addchstr)
#endif

#ifdef addnstr
inline int UNDEF(addnstr)(const char *str, int n)
{ return addnstr(str, n); }
#undef addnstr
#define addnstr UNDEF(addnstr)
#endif

#ifdef addstr
inline int UNDEF(addstr)(const char * str)  { return addstr(str); }
#undef addstr
#define addstr UNDEF(addstr)
#endif

#ifdef attroff
inline int UNDEF(attroff)(chtype at) { return attroff(at); }
#undef attroff
#define attroff UNDEF(attroff)
#endif

#ifdef attron
inline int UNDEF(attron)(chtype at) { return attron(at); }
#undef attron
#define attron UNDEF(attron)
#endif

#ifdef attrset
inline chtype UNDEF(attrset)(chtype at) { return attrset(at); }
#undef attrset
#define attrset UNDEF(attrset)
#endif

#ifdef bkgd
inline int UNDEF(bkgd)(chtype ch) { return bkgd(ch); }
#undef bkgd
#define bkgd UNDEF(bkgd)
#endif

#ifdef bkgdset
inline void UNDEF(bkgdset)(chtype ch) { bkgdset(ch); }
#undef bkgdset
#define bkgdset UNDEF(bkgdset)
#endif

#ifdef border
inline int UNDEF(border)(chtype ls, chtype rs, chtype ts, chtype bs, chtype tl, chtype tr, chtype bl, chtype br)
{ return border(ls, rs, ts, bs, tl, tr, bl, br); }
#undef border
#define border UNDEF(border)
#endif

#ifdef box
inline int UNDEF(box)(WINDOW *win, int v, int h) { return box(win, v, h); }
#undef box
#define box UNDEF(box)
#endif

#ifdef chgat
inline int UNDEF(chgat)(int n, attr_t attr, NCURSES_PAIRS_T color, const void *opts) {
  return chgat(n, attr, color, opts); }
#undef chgat
#define chgat UNDEF(chgat)
#endif

#ifdef clear
inline int UNDEF(clear)()  { return clear(); }
#undef clear
#define clear UNDEF(clear)
#endif

#ifdef clearok
inline int UNDEF(clearok)(WINDOW* win, bool bf)  { return clearok(win, bf); }
#undef clearok
#define clearok UNDEF(clearok)
#else
extern "C" NCURSES_IMPEXP int NCURSES_API clearok(WINDOW*, bool);
#endif

#ifdef clrtobot
inline int UNDEF(clrtobot)()  { return clrtobot(); }
#undef clrtobot
#define clrtobot UNDEF(clrtobot)
#endif

#ifdef clrtoeol
inline int UNDEF(clrtoeol)()  { return clrtoeol(); }
#undef clrtoeol
#define clrtoeol UNDEF(clrtoeol)
#endif

#ifdef color_set
inline chtype UNDEF(color_set)(NCURSES_PAIRS_T p, void* opts) { return color_set(p, opts); }
#undef color_set
#define color_set UNDEF(color_set)
#endif

#ifdef crmode
inline int UNDEF(crmode)(void) { return crmode(); }
#undef crmode
#define crmode UNDEF(crmode)
#endif

#ifdef delch
inline int UNDEF(delch)()  { return delch(); }
#undef delch
#define delch UNDEF(delch)
#endif

#ifdef deleteln
inline int UNDEF(deleteln)()  { return deleteln(); }
#undef deleteln
#define deleteln UNDEF(deleteln)
#endif

#ifdef echochar
inline int UNDEF(echochar)(chtype ch)  { return echochar(ch); }
#undef echochar
#define echochar UNDEF(echochar)
#endif

#ifdef erase
inline int UNDEF(erase)()  { return erase(); }
#undef erase
#define erase UNDEF(erase)
#endif

#ifdef fixterm
inline int UNDEF(fixterm)(void) { return fixterm(); }
#undef fixterm
#define fixterm UNDEF(fixterm)
#endif

#ifdef flushok
inline int UNDEF(flushok)(WINDOW* _win, bool _bf)  {
  return flushok(_win, _bf); }
#undef flushok
#define flushok UNDEF(flushok)
#else
#define _no_flushok
#endif

#ifdef getattrs
inline int UNDEF(getattrs)(WINDOW *win) { return getattrs(win); }
#undef getattrs
#define getattrs UNDEF(getattrs)
#endif

#ifdef getbegyx
inline void UNDEF(getbegyx)(WINDOW* win, int& y, int& x) { getbegyx(win, y, x); }
#undef getbegyx
#define getbegyx UNDEF(getbegyx)
#endif

#ifdef getbkgd
inline chtype UNDEF(getbkgd)(const WINDOW *win) { return getbkgd(win); }
#undef getbkgd
#define getbkgd UNDEF(getbkgd)
#endif

#ifdef getch
inline int UNDEF(getch)()  { return getch(); }
#undef getch
#define getch UNDEF(getch)
#endif

#ifdef getmaxyx
inline void UNDEF(getmaxyx)(WINDOW* win, int& y, int& x) { getmaxyx(win, y, x); }
#undef getmaxyx
#define getmaxyx UNDEF(getmaxyx)
#endif

#ifdef getnstr
inline int UNDEF(getnstr)(char *_str, int n)  { return getnstr(_str, n); }
#undef getnstr
#define getnstr UNDEF(getnstr)
#endif

#ifdef getparyx
inline void UNDEF(getparyx)(WINDOW* win, int& y, int& x) { getparyx(win, y, x); }
#undef getparyx
#define getparyx UNDEF(getparyx)
#endif

#ifdef getstr
inline int UNDEF(getstr)(char *_str)  { return getstr(_str); }
#undef getstr
#define getstr UNDEF(getstr)
#endif

#ifdef getyx
inline void UNDEF(getyx)(const WINDOW* win, int& y, int& x) {
  getyx(win, y, x); }
#undef getyx
#define getyx UNDEF(getyx)
#endif

#ifdef hline
inline int UNDEF(hline)(chtype ch, int n) { return hline(ch, n); }
#undef hline
#define hline UNDEF(hline)
#endif

#ifdef inch
inline chtype UNDEF(inch)()  { return inch(); }
#undef inch
#define inch UNDEF(inch)
#endif

#ifdef inchstr
inline int UNDEF(inchstr)(chtype *str)  { return inchstr(str); }
#undef inchstr
#define inchstr UNDEF(inchstr)
#endif

#ifdef innstr
inline int UNDEF(innstr)(char *_str, int n)  { return innstr(_str, n); }
#undef innstr
#define innstr UNDEF(innstr)
#endif

#ifdef insch
inline int UNDEF(insch)(chtype c)  { return insch(c); }
#undef insch
#define insch UNDEF(insch)
#endif

#ifdef insdelln
inline int UNDEF(insdelln)(int n)  { return insdelln(n); }
#undef insdelln
#define insdelln UNDEF(insdelln)
#endif

#ifdef insertln
inline int UNDEF(insertln)()  { return insertln(); }
#undef insertln
#define insertln UNDEF(insertln)
#endif

#ifdef insnstr
inline int UNDEF(insnstr)(const char *_str, int n)  {
  return insnstr(_str, n); }
#undef insnstr
#define insnstr UNDEF(insnstr)
#endif

#ifdef insstr
inline int UNDEF(insstr)(const char *_str)  {
  return insstr(_str); }
#undef insstr
#define insstr UNDEF(insstr)
#endif

#ifdef instr
inline int UNDEF(instr)(char *_str)  { return instr(_str); }
#undef instr
#define instr UNDEF(instr)
#endif

#ifdef intrflush
inline void UNDEF(intrflush)(WINDOW *win, bool bf) { intrflush(); }
#undef intrflush
#define intrflush UNDEF(intrflush)
#endif

#ifdef is_linetouched
inline int UNDEF(is_linetouched)(WINDOW *w, int l)  { return is_linetouched(w,l); }
#undef is_linetouched
#define is_linetouched UNDEF(is_linetouched)
#endif

#ifdef leaveok
inline int UNDEF(leaveok)(WINDOW* win, bool bf)  { return leaveok(win, bf); }
#undef leaveok
#define leaveok UNDEF(leaveok)
#else
extern "C" NCURSES_IMPEXP int NCURSES_API leaveok(WINDOW* win, bool bf);
#endif

#ifdef move
inline int UNDEF(move)(int x, int y)  { return move(x, y); }
#undef move
#define move UNDEF(move)
#endif

#ifdef mvaddch
inline int UNDEF(mvaddch)(int y, int x, chtype ch)
{ return mvaddch(y, x, ch); }
#undef mvaddch
#define mvaddch UNDEF(mvaddch)
#endif

#ifdef mvaddnstr
inline int UNDEF(mvaddnstr)(int y, int x, const char *str, int n)
{ return mvaddnstr(y, x, str, n); }
#undef mvaddnstr
#define mvaddnstr UNDEF(mvaddnstr)
#endif

#ifdef mvaddstr
inline int UNDEF(mvaddstr)(int y, int x, const char * str)
{ return mvaddstr(y, x, str); }
#undef mvaddstr
#define mvaddstr UNDEF(mvaddstr)
#endif

#ifdef mvchgat
inline int UNDEF(mvchgat)(int y, int x, int n,
			  attr_t attr, NCURSES_PAIRS_T color, const void *opts) {
  return mvchgat(y, x, n, attr, color, opts); }
#undef mvchgat
#define mvchgat UNDEF(mvchgat)
#endif

#ifdef mvdelch
inline int UNDEF(mvdelch)(int y, int x) { return mvdelch(y, x);}
#undef mvdelch
#define mvdelch UNDEF(mvdelch)
#endif

#ifdef mvgetch
inline int UNDEF(mvgetch)(int y, int x) { return mvgetch(y, x);}
#undef mvgetch
#define mvgetch UNDEF(mvgetch)
#endif

#ifdef mvgetnstr
inline int UNDEF(mvgetnstr)(int y, int x, char *str, int n) {
  return mvgetnstr(y, x, str, n);}
#undef mvgetnstr
#define mvgetnstr UNDEF(mvgetnstr)
#endif

#ifdef mvgetstr
inline int UNDEF(mvgetstr)(int y, int x, char *str) {return mvgetstr(y, x, str);}
#undef mvgetstr
#define mvgetstr UNDEF(mvgetstr)
#endif

#ifdef mvinch
inline chtype UNDEF(mvinch)(int y, int x) { return mvinch(y, x);}
#undef mvinch
#define mvinch UNDEF(mvinch)
#endif

#ifdef mvinnstr
inline int UNDEF(mvinnstr)(int y, int x, char *_str, int n) {
  return mvinnstr(y, x, _str, n); }
#undef mvinnstr
#define mvinnstr UNDEF(mvinnstr)
#endif

#ifdef mvinsch
inline int UNDEF(mvinsch)(int y, int x, chtype c)
{ return mvinsch(y, x, c); }
#undef mvinsch
#define mvinsch UNDEF(mvinsch)
#endif

#ifdef mvinsnstr
inline int UNDEF(mvinsnstr)(int y, int x, const char *_str, int n) {
  return mvinsnstr(y, x, _str, n); }
#undef mvinsnstr
#define mvinsnstr UNDEF(mvinsnstr)
#endif

#ifdef mvinsstr
inline int UNDEF(mvinsstr)(int y, int x, const char *_str)  {
  return mvinsstr(y, x, _str); }
#undef mvinsstr
#define mvinsstr UNDEF(mvinsstr)
#endif

#ifdef mvwaddch
inline int UNDEF(mvwaddch)(WINDOW *win, int y, int x, const chtype ch)
{ return mvwaddch(win, y, x, ch); }
#undef mvwaddch
#define mvwaddch UNDEF(mvwaddch)
#endif

#ifdef mvwaddchnstr
inline int UNDEF(mvwaddchnstr)(WINDOW *win, int y, int x, const chtype *str, int n)
{ return mvwaddchnstr(win, y, x, str, n); }
#undef mvwaddchnstr
#define mvwaddchnstr UNDEF(mvwaddchnstr)
#endif

#ifdef mvwaddchstr
inline int UNDEF(mvwaddchstr)(WINDOW *win, int y, int x, const chtype *str)
{ return mvwaddchstr(win, y, x, str); }
#undef mvwaddchstr
#define mvwaddchstr UNDEF(mvwaddchstr)
#endif

#ifdef mvwaddnstr
inline int UNDEF(mvwaddnstr)(WINDOW *win, int y, int x, const char *str, int n)
{ return mvwaddnstr(win, y, x, str, n); }
#undef mvwaddnstr
#define mvwaddnstr UNDEF(mvwaddnstr)
#endif

#ifdef mvwaddstr
inline int UNDEF(mvwaddstr)(WINDOW *win, int y, int x, const char * str)
{ return mvwaddstr(win, y, x, str); }
#undef mvwaddstr
#define mvwaddstr UNDEF(mvwaddstr)
#endif

#ifdef mvwchgat
inline int UNDEF(mvwchgat)(WINDOW *win, int y, int x, int n,
			   attr_t attr, NCURSES_PAIRS_T color, const void *opts) {
  return mvwchgat(win, y, x, n, attr, color, opts); }
#undef mvwchgat
#define mvwchgat UNDEF(mvwchgat)
#endif

#ifdef mvwdelch
inline int UNDEF(mvwdelch)(WINDOW *win, int y, int x)
{ return mvwdelch(win, y, x); }
#undef mvwdelch
#define mvwdelch UNDEF(mvwdelch)
#endif

#ifdef mvwgetch
inline int UNDEF(mvwgetch)(WINDOW *win, int y, int x) { return mvwgetch(win, y, x);}
#undef mvwgetch
#define mvwgetch UNDEF(mvwgetch)
#endif

#ifdef mvwgetnstr
inline int UNDEF(mvwgetnstr)(WINDOW *win, int y, int x, char *str, int n)
{return mvwgetnstr(win, y, x, str, n);}
#undef mvwgetnstr
#define mvwgetnstr UNDEF(mvwgetnstr)
#endif

#ifdef mvwgetstr
inline int UNDEF(mvwgetstr)(WINDOW *win, int y, int x, char *str)
{return mvwgetstr(win, y, x, str);}
#undef mvwgetstr
#define mvwgetstr UNDEF(mvwgetstr)
#endif

#ifdef mvwhline
inline int UNDEF(mvwhline)(WINDOW *win, int y, int x, chtype c, int n) {
  return mvwhline(win, y, x, c, n); }
#undef mvwhline
#define mvwhline UNDEF(mvwhline)
#endif

#ifdef mvwinch
inline chtype UNDEF(mvwinch)(WINDOW *win, int y, int x) {
  return mvwinch(win, y, x);}
#undef mvwinch
#define mvwinch UNDEF(mvwinch)
#endif

#ifdef mvwinchnstr
inline int UNDEF(mvwinchnstr)(WINDOW *win, int y, int x, chtype *str, int n)  { return mvwinchnstr(win, y, x, str, n); }
#undef mvwinchnstr
#define mvwinchnstr UNDEF(mvwinchnstr)
#endif

#ifdef mvwinchstr
inline int UNDEF(mvwinchstr)(WINDOW *win, int y, int x, chtype *str)  { return mvwinchstr(win, y, x, str); }
#undef mvwinchstr
#define mvwinchstr UNDEF(mvwinchstr)
#endif

#ifdef mvwinnstr
inline int UNDEF(mvwinnstr)(WINDOW *win, int y, int x, char *_str, int n) {
  return mvwinnstr(win, y, x, _str, n); }
#undef mvwinnstr
#define mvwinnstr UNDEF(mvwinnstr)
#endif

#ifdef mvwinsch
inline int UNDEF(mvwinsch)(WINDOW *win, int y, int x, chtype c)
{ return mvwinsch(win, y, x, c); }
#undef mvwinsch
#define mvwinsch UNDEF(mvwinsch)
#endif

#ifdef mvwinsnstr
inline int UNDEF(mvwinsnstr)(WINDOW *w, int y, int x, const char *_str, int n) {
  return mvwinsnstr(w, y, x, _str, n); }
#undef mvwinsnstr
#define mvwinsnstr UNDEF(mvwinsnstr)
#endif

#ifdef mvwinsstr
inline int UNDEF(mvwinsstr)(WINDOW *w, int y, int x,  const char *_str)  {
  return mvwinsstr(w, y, x, _str); }
#undef mvwinsstr
#define mvwinsstr UNDEF(mvwinsstr)
#endif

#ifdef mvwvline
inline int UNDEF(mvwvline)(WINDOW *win, int y, int x, chtype c, int n) {
  return mvwvline(win, y, x, c, n); }
#undef mvwvline
#define mvwvline UNDEF(mvwvline)
#endif

#ifdef napms
inline void UNDEF(napms)(unsigned long x) { napms(x); }
#undef napms
#define napms UNDEF(napms)
#endif

#ifdef nocrmode
inline int UNDEF(nocrmode)(void) { return nocrmode(); }
#undef nocrmode
#define nocrmode UNDEF(nocrmode)
#endif

#ifdef nodelay
inline void UNDEF(nodelay)() { nodelay(); }
#undef nodelay
#define nodelay UNDEF(nodelay)
#endif

#ifdef redrawwin
inline int UNDEF(redrawwin)(WINDOW *win)  { return redrawwin(win); }
#undef redrawwin
#define redrawwin UNDEF(redrawwin)
#endif

#ifdef refresh
inline int UNDEF(refresh)()  { return refresh(); }
#undef refresh
#define refresh UNDEF(refresh)
#endif

#ifdef resetterm
inline int UNDEF(resetterm)(void) { return resetterm(); }
#undef resetterm
#define resetterm UNDEF(resetterm)
#endif

#ifdef saveterm
inline int UNDEF(saveterm)(void) { return saveterm(); }
#undef saveterm
#define saveterm UNDEF(saveterm)
#endif

#ifdef scrl
inline int UNDEF(scrl)(int l) { return scrl(l); }
#undef scrl
#define scrl UNDEF(scrl)
#endif

#ifdef scroll
inline int UNDEF(scroll)(WINDOW *win) { return scroll(win); }
#undef scroll
#define scroll UNDEF(scroll)
#endif

#ifdef scrollok
inline int UNDEF(scrollok)(WINDOW* win, bool bf)  { return scrollok(win, bf); }
#undef scrollok
#define scrollok UNDEF(scrollok)
#else
#if	defined(__NCURSES_H)
extern "C" NCURSES_IMPEXP int NCURSES_API scrollok(WINDOW*, bool);
#else
extern "C" NCURSES_IMPEXP int NCURSES_API scrollok(WINDOW*, char);
#endif
#endif

#ifdef setscrreg
inline int UNDEF(setscrreg)(int t, int b) { return setscrreg(t, b); }
#undef setscrreg
#define setscrreg UNDEF(setscrreg)
#endif

#ifdef standend
inline int UNDEF(standend)()  { return standend(); }
#undef standend
#define standend UNDEF(standend)
#endif

#ifdef standout
inline int UNDEF(standout)()  { return standout(); }
#undef standout
#define standout UNDEF(standout)
#endif

#ifdef subpad
inline WINDOW *UNDEF(subpad)(WINDOW *p, int l, int c, int y, int x)
{ return derwin(p, l, c, y, x); }
#undef subpad
#define subpad UNDEF(subpad)
#endif

#ifdef timeout
inline void UNDEF(timeout)(int delay) { timeout(delay); }
#undef timeout
#define timeout UNDEF(timeout)
#endif

#ifdef touchline
inline int UNDEF(touchline)(WINDOW *win, int s, int c)
{ return touchline(win, s, c); }
#undef touchline
#define touchline UNDEF(touchline)
#endif

#ifdef touchwin
inline int UNDEF(touchwin)(WINDOW *win) { return touchwin(win); }
#undef touchwin
#define touchwin UNDEF(touchwin)
#endif

#ifdef untouchwin
inline int UNDEF(untouchwin)(WINDOW *win) { return untouchwin(win); }
#undef untouchwin
#define untouchwin UNDEF(untouchwin)
#endif

#ifdef vline
inline int UNDEF(vline)(chtype ch, int n) { return vline(ch, n); }
#undef vline
#define vline UNDEF(vline)
#endif

#ifdef waddchstr
inline int UNDEF(waddchstr)(WINDOW *win, chtype *at) { return waddchstr(win, at); }
#undef waddchstr
#define waddchstr UNDEF(waddchstr)
#endif

#ifdef waddstr
inline int UNDEF(waddstr)(WINDOW *win, char *str) { return waddstr(win, str); }
#undef waddstr
#define waddstr UNDEF(waddstr)
#endif

#ifdef wattroff
inline int UNDEF(wattroff)(WINDOW *win, int att) { return wattroff(win, att); }
#undef wattroff
#define wattroff UNDEF(wattroff)
#endif

#ifdef wattrset
inline int UNDEF(wattrset)(WINDOW *win, int att) { return wattrset(win, att); }
#undef wattrset
#define wattrset UNDEF(wattrset)
#endif

#ifdef winch
inline chtype UNDEF(winch)(const WINDOW* win) { return winch(win); }
#undef winch
#define winch UNDEF(winch)
#endif

#ifdef winchnstr
inline int UNDEF(winchnstr)(WINDOW *win, chtype *str, int n)  { return winchnstr(win, str, n); }
#undef winchnstr
#define winchnstr UNDEF(winchnstr)
#endif

#ifdef winchstr
inline int UNDEF(winchstr)(WINDOW *win, chtype *str)  { return winchstr(win, str); }
#undef winchstr
#define winchstr UNDEF(winchstr)
#endif

#ifdef winsstr
inline int UNDEF(winsstr)(WINDOW *w, const char *_str)  {
  return winsstr(w, _str); }
#undef winsstr
#define winsstr UNDEF(winsstr)
#endif

#ifdef wstandend
inline int UNDEF(wstandend)(WINDOW *win)  { return wstandend(win); }
#undef wstandend
#define wstandend UNDEF(wstandend)
#endif

#ifdef wstandout
inline int UNDEF(wstandout)(WINDOW *win)  { return wstandout(win); }
#undef wstandout
#define wstandout UNDEF(wstandout)
#endif

 

extern "C" int     _nc_ripoffline(int, int (*init)(WINDOW*, int));
extern "C" int     _nc_xx_ripoff_init(WINDOW *, int);
extern "C" int     _nc_has_mouse(void);

class NCURSES_CXX_IMPEXP NCursesWindow
{
  friend class NCursesMenu;
  friend class NCursesForm;

private:
  static bool    b_initialized;
  static void    initialize();
  void           constructing();
  friend int     _nc_xx_ripoff_init(WINDOW *, int);

  void           set_keyboard();

  NCURSES_COLOR_T getcolor(int getback) const;
  NCURSES_PAIRS_T getPair() const;

  static int     setpalette(NCURSES_COLOR_T fore, NCURSES_COLOR_T back, NCURSES_PAIRS_T pair);
  static int     colorInitialized;

   
   
  NCursesWindow(WINDOW* win, int ncols);

protected:
  virtual void   err_handler(const char *) const THROWS(NCursesException);
   

  static long count;         
   
   
   

  WINDOW*        w;                 

  bool           alloced;           

  NCursesWindow* par;               
  NCursesWindow* subwins;           
  NCursesWindow* sib;               

  void           kill_subwindows();  
   

   
  NCursesWindow();

public:
  explicit NCursesWindow(WINDOW* window);    

  NCursesWindow(int nlines,         
		int ncols,          
		int begin_y,        
		int begin_x);       

  NCursesWindow(NCursesWindow& par, 
		int nlines,         
		int ncols,          
		int begin_y,        
		int begin_x,        
		char absrel = 'a'); 
  

  NCursesWindow(NCursesWindow& par,
		bool do_box = TRUE);
  
  
  

  NCursesWindow& operator=(const NCursesWindow& rhs)
  {
    if (this != &rhs)
      *this = rhs;
    return *this;
  }

  NCursesWindow(const NCursesWindow& rhs)
    : w(rhs.w), alloced(rhs.alloced), par(rhs.par), subwins(rhs.subwins), sib(rhs.sib)
  {
  }

  virtual ~NCursesWindow() THROWS(NCursesException);

  NCursesWindow Clone();
  

  
  static void    useColors(void);
  

  static int ripoffline(int ripoff_lines,
			int (*init)(NCursesWindow& win));
  
  
  
  
  
  
  
  

  
  
  
  int            lines() const { initialize(); return LINES; }
  

  int            cols() const { initialize(); return COLS; }
  

  int            tabsize() const { initialize(); return TABSIZE; }
  

  static int     NumberOfColors();
  

  int            colors() const { return NumberOfColors(); }
  

  
  
  
  int            height() const { return maxy() + 1; }
  

  int            width() const { return maxx() + 1; }
  

  int            begx() const { return getbegx(w); }
  

  int            begy() const { return getbegy(w); }
  

  int            curx() const { return getcurx(w); }
  

  int            cury() const { return getcury(w); }
  

  int            maxx() const { return getmaxx(w) == ERR ? ERR : getmaxx(w)-1; }
  

  int            maxy() const { return getmaxy(w) == ERR ? ERR : getmaxy(w)-1; }
  

  NCURSES_PAIRS_T getcolor() const;
  

  NCURSES_COLOR_T foreground() const { return getcolor(0); }
  

  NCURSES_COLOR_T background() const { return getcolor(1); }
  

  int            setpalette(NCURSES_COLOR_T fore, NCURSES_COLOR_T back);
  

  int            setcolor(NCURSES_PAIRS_T pair);
  

  
  
  
  virtual int    mvwin(int begin_y, int begin_x) {
    return ::mvwin(w, begin_y, begin_x); }
  
  

  
  
  
  int            move(int y, int x) { return ::wmove(w, y, x); }
  

  void           getyx(int& y, int& x) const { ::getyx(w, y, x); }
  

  void           getbegyx(int& y, int& x) const { ::getbegyx(w, y, x); }
  

  void           getmaxyx(int& y, int& x) const { ::getmaxyx(w, y, x); }
  

  void           getparyx(int& y, int& x) const { ::getparyx(w, y, x); }
  

  int            mvcur(int oldrow, int oldcol, int newrow, int newcol) const {
    return ::mvcur(oldrow, oldcol, newrow, newcol); }
  

  
  
  
  int            getch() { return ::wgetch(w); }
  

  int            getch(int y, int x) { return ::mvwgetch(w, y, x); }
  

  int            getstr(char* str, int n=-1) {
    return ::wgetnstr(w, str, n); }
  
  
  

  int            getstr(int y, int x, char* str, int n=-1) {
    return ::mvwgetnstr(w, y, x, str, n); }
  
  

  int            instr(char *s, int n=-1) { return ::winnstr(w, s, n); }
  
  
  

  int            instr(int y, int x, char *s, int n=-1) {
    return ::mvwinnstr(w, y, x, s, n); }
  
  

  int            scanw(const char* fmt, ...)
    
#if __GNUG__ >= 2
    __attribute__ ((format (scanf, 2, 3)));
#else
  ;
#endif

  int            scanw(const char*, va_list);
    

  int            scanw(int y, int x, const char* fmt, ...)
    
    
#if __GNUG__ >= 2
    __attribute__ ((format (scanf, 4, 5)));
#else
  ;
#endif

  int            scanw(int y, int x, const char* fmt, va_list);
    
    

  
  
  
  int            addch(const chtype ch) { return ::waddch(w, ch); }
  

  int            addch(int y, int x, const chtype ch) {
    return ::mvwaddch(w, y, x, ch); }
  
  

  int            echochar(const chtype ch) { return ::wechochar(w, ch); }
  

  int            addstr(const char* str, int n=-1) {
    return ::waddnstr(w, str, n); }
  
  

  int            addstr(int y, int x, const char * str, int n=-1) {
    return ::mvwaddnstr(w, y, x, str, n); }
  
  

  int            addchstr(const chtype* str, int n=-1) {
    return ::waddchnstr(w, str, n); }
  
  

  int            addchstr(int y, int x, const chtype * str, int n=-1) {
    return ::mvwaddchnstr(w, y, x, str, n); }
  
  

  int            printw(const char* fmt, ...)
    
#if (__GNUG__ >= 2) && !defined(printf)
    __attribute__ ((format (printf, 2, 3)));
#else
  ;
#endif

  int            printw(int y, int x, const char * fmt, ...)
    
#if (__GNUG__ >= 2) && !defined(printf)
    __attribute__ ((format (printf, 4, 5)));
#else
  ;
#endif

  int            printw(const char* fmt, va_list args);
    

  int            printw(int y, int x, const char * fmt, va_list args);
    

  chtype         inch() const { return ::winch(w); }
  

  chtype         inch(int y, int x) { return ::mvwinch(w, y, x); }
  
  

  int            inchstr(chtype* str, int n=-1) {
    return ::winchnstr(w, str, n); }
  
  

  int            inchstr(int y, int x, chtype * str, int n=-1) {
    return ::mvwinchnstr(w, y, x, str, n); }
  
  

  int            insch(chtype ch) { return ::winsch(w, ch); }
  
  

  int            insch(int y, int x, chtype ch) {
    return ::mvwinsch(w, y, x, ch); }
  
  

  int            insertln() { return ::winsdelln(w, 1); }
  

  int            insdelln(int n=1) { return ::winsdelln(w, n); }
  
  

  int            insstr(const char *s, int n=-1) {
    return ::winsnstr(w, s, n); }
  
  
  

  int            insstr(int y, int x, const char *s, int n=-1) {
    return ::mvwinsnstr(w, y, x, s, n); }
  
  

  int            attron (chtype at) { return ::wattron (w, at); }
  

  int            attroff(chtype at) { return ::wattroff(w, static_cast<int>(at)); }
  

  int            attrset(chtype at) { return ::wattrset(w, static_cast<int>(at)); }
  

  chtype         attrget() { return ::getattrs(w); }
  

  int            color_set(NCURSES_PAIRS_T color_pair_number, void* opts=NULL) {
    return ::wcolor_set(w, color_pair_number, opts); }
  

  int            chgat(int n, attr_t attr, NCURSES_PAIRS_T color, const void *opts=NULL) {
    return ::wchgat(w, n, attr, color, opts); }
  
  
  

  int            chgat(int y, int x,
		       int n, attr_t attr, NCURSES_PAIRS_T color, const void *opts=NULL) {
    return ::mvwchgat(w, y, x, n, attr, color, opts); }
  
  

  
  
  
  chtype         getbkgd() const { return ::getbkgd(w); }
  

  int            bkgd(const chtype ch) { return ::wbkgd(w, ch); }
  

  void           bkgdset(chtype ch) { ::wbkgdset(w, ch); }
  

  
  
  
  int            box(chtype vert=0, chtype  hor=0) {
    return ::wborder(w, vert, vert, hor, hor, 0, 0, 0, 0); }
  
  
  

  int            border(chtype left=0, chtype right=0,
			chtype top =0, chtype bottom=0,
			chtype top_left =0, chtype top_right=0,
			chtype bottom_left =0, chtype bottom_right=0) {
    return ::wborder(w, left, right, top, bottom, top_left, top_right,
		     bottom_left, bottom_right); }
  
  
  

  
  
  
  int            hline(int len, chtype ch=0) { return ::whline(w, ch, len); }
  
  

  int            hline(int y, int x, int len, chtype ch=0) {
    return ::mvwhline(w, y, x, ch, len); }
  

  int            vline(int len, chtype ch=0) { return ::wvline(w, ch, len); }
  
  

  int            vline(int y, int x, int len, chtype ch=0) {
    return ::mvwvline(w, y, x, ch, len); }
  

  
  
  
  int            erase() { return ::werase(w); }
  

  int            clear() { return ::wclear(w); }
  

  int            clearok(bool bf) { return ::clearok(w, bf); }
  
  

  int            clrtobot() { return ::wclrtobot(w); }
  

  int            clrtoeol() { return ::wclrtoeol(w); }
  

  int            delch() { return ::wdelch(w); }
  

  int            delch(int y, int x) { return ::mvwdelch(w, y, x); }
  
  

  int            deleteln() { return ::winsdelln(w, -1); }
  

  
  
  
  int            scroll(int amount=1) { return ::wscrl(w, amount); }
  
  

  int            scrollok(bool bf) { return ::scrollok(w, bf); }
  
  
  

  int            setscrreg(int from, int to) {
    return ::wsetscrreg(w, from, to); }
  

  int            idlok(bool bf) { return ::idlok(w, bf); }
  
  

  void           idcok(bool bf) { ::idcok(w, bf); }
  
  

  int            touchline(int s, int c) { return ::touchline(w, s, c); }
  

  int            touchwin()   { return ::wtouchln(w, 0, height(), 1); }
  

  int            untouchwin() { return ::wtouchln(w, 0, height(), 0); }
  

  int            touchln(int s, int cnt, bool changed=TRUE) {
    return ::wtouchln(w, s, cnt, static_cast<int>(changed ? 1 : 0)); }
  
  

  bool           is_linetouched(int line) const {
    return (::is_linetouched(w, line) == TRUE ? TRUE:FALSE); }
  

  bool           is_wintouched() const {
    return (::is_wintouched(w) ? TRUE:FALSE); }
  

  int            leaveok(bool bf) { return ::leaveok(w, bf); }
  
  

  int            redrawln(int from, int n) { return ::wredrawln(w, from, n); }
  

  int            redrawwin() { return ::wredrawln(w, 0, height()); }
  

  int            doupdate()  { return ::doupdate(); }
  

  void           syncdown()  { ::wsyncdown(w); }
  

  void           syncup()    { ::wsyncup(w); }
  

  void           cursyncup() { ::wcursyncup(w); }
  

  int            syncok(bool bf) { return ::syncok(w, bf); }
  

#ifndef _no_flushok
  int            flushok(bool bf) { return ::flushok(w, bf); }
#endif

  void           immedok(bool bf) { ::immedok(w, bf); }
  
  

  int            intrflush(bool bf) { return ::intrflush(w, bf); }

  int            keypad(bool bf) { return ::keypad(w, bf); }
  

  int            nodelay(bool bf) { return ::nodelay(w, bf); }

  int            meta(bool bf) { return ::meta(w, bf); }
  
  

  int            standout() { return ::wstandout(w); }
  

  int            standend() { return ::wstandend(w); }
  

  
  
  
  
  virtual int    refresh() { return ::wrefresh(w); }
  
  

  virtual int    noutrefresh() { return ::wnoutrefresh(w); }
  
  

  
  
  
  int            overlay(NCursesWindow& win) {
    return ::overlay(w, win.w); }
  

  int            overwrite(NCursesWindow& win) {
    return ::overwrite(w, win.w); }
  

  int            copywin(NCursesWindow& win,
			 int sminrow, int smincol,
			 int dminrow, int dmincol,
			 int dmaxrow, int dmaxcol, bool overlaywin=TRUE) {
    return ::copywin(w, win.w, sminrow, smincol, dminrow, dmincol,
		     dmaxrow, dmaxcol, static_cast<int>(overlaywin ? 1 : 0)); }
  
  
  

  
  
  
#if defined(NCURSES_EXT_FUNCS) && (NCURSES_EXT_FUNCS != 0)
  int            wresize(int newLines, int newColumns) {
    return ::wresize(w, newLines, newColumns); }
#endif

  
  
  
  bool has_mouse() const;
  

  
  
  
  NCursesWindow*  child() { return subwins; }
  

  NCursesWindow*  sibling() { return sib; }
  

  NCursesWindow*  parent() { return par; }
  

  bool isDescendant(NCursesWindow& win);
  
};




class NCURSES_CXX_IMPEXP NCursesColorWindow : public NCursesWindow
{
public:
  explicit NCursesColorWindow(WINDOW* &window)   
    : NCursesWindow(window) {
      useColors(); }

  NCursesColorWindow(int nlines,        
		     int ncols,         
		     int begin_y,       
		     int begin_x)       
    : NCursesWindow(nlines, ncols, begin_y, begin_x) {
      useColors(); }

  NCursesColorWindow(NCursesWindow& parentWin,
		     int nlines,        
		     int ncols,         
		     int begin_y,       
		     int begin_x,       
		     char absrel = 'a') 
    : NCursesWindow(parentWin,
		    nlines, ncols,	
		    begin_y, begin_x,   
		    absrel ) {          
      useColors(); }
};




  typedef enum {
    REQ_PAD_REFRESH = KEY_MAX + 1,
    REQ_PAD_UP,
    REQ_PAD_DOWN,
    REQ_PAD_LEFT,
    REQ_PAD_RIGHT,
    REQ_PAD_EXIT
  } Pad_Request;

  const Pad_Request PAD_LOW  = REQ_PAD_REFRESH;   
  const Pad_Request PAD_HIGH = REQ_PAD_EXIT;      





class NCURSES_CXX_IMPEXP NCursesPad : public NCursesWindow
{
private:
  NCursesWindow* viewWin;       
  NCursesWindow* viewSub;       

  int h_gridsize, v_gridsize;

protected:
  int min_row, min_col;         

  NCursesWindow* Win(void) const {
    
    return (viewSub?viewSub:(viewWin?viewWin:0));
  }

  NCursesWindow* getWindow(void) const {
    return viewWin;
  }

  NCursesWindow* getSubWindow(void) const {
    return viewSub;
  }

  virtual int driver (int key);      
  

  virtual void OnUnknownOperation(int pad_req) {
    (void) pad_req;
    ::beep();
  }
  

  virtual void OnNavigationError(int pad_req) {
    (void) pad_req;
    ::beep();
  }
  

  virtual void OnOperation(int pad_req) {
    (void) pad_req;
  };
  
  

public:
  NCursesPad(int nlines, int ncols);
  

  NCursesPad& operator=(const NCursesPad& rhs)
  {
    if (this != &rhs) {
      *this = rhs;
      NCursesWindow::operator=(rhs);
    }
    return *this;
  }

  NCursesPad(const NCursesPad& rhs)
    : NCursesWindow(rhs),
      viewWin(rhs.viewWin),
      viewSub(rhs.viewSub),
      h_gridsize(rhs.h_gridsize),
      v_gridsize(rhs.v_gridsize),
      min_row(rhs.min_row),
      min_col(rhs.min_col)
  {
  }

  virtual ~NCursesPad() THROWS(NCursesException) {}

  int echochar(const chtype ch) { return ::pechochar(w, ch); }
  
  

  int refresh() NCURSES_OVERRIDE;
  
  

  int refresh(int pminrow, int pmincol,
	      int sminrow, int smincol,
	      int smaxrow, int smaxcol) {
    return ::prefresh(w, pminrow, pmincol,
		      sminrow, smincol, smaxrow, smaxcol);
  }
  
  
  

  int noutrefresh() NCURSES_OVERRIDE;
  
  

  int noutrefresh(int pminrow, int pmincol,
		  int sminrow, int smincol,
		  int smaxrow, int smaxcol) {
    return ::pnoutrefresh(w, pminrow, pmincol,
			  sminrow, smincol, smaxrow, smaxcol);
  }
  

  virtual void setWindow(NCursesWindow& view, int v_grid = 1, int h_grid = 1);
  

  virtual void setSubWindow(NCursesWindow& sub);
  
  
  

  virtual void operator() (void);
  
};




class NCURSES_CXX_IMPEXP NCursesFramedPad : public NCursesPad
{
protected:
  virtual void OnOperation(int pad_req) NCURSES_OVERRIDE;

public:
  NCursesFramedPad(NCursesWindow& win, int nlines, int ncols,
		   int v_grid = 1, int h_grid = 1)
    : NCursesPad(nlines, ncols) {
    NCursesPad::setWindow(win, v_grid, h_grid);
    NCursesPad::setSubWindow(*(new NCursesWindow(win)));
  }
  

  virtual ~NCursesFramedPad() THROWS(NCursesException) {
    delete getSubWindow();
  }

  void setWindow(NCursesWindow& view, int v_grid = 1, int h_grid = 1) NCURSES_OVERRIDE {
    (void) view;
    (void) v_grid;
    (void) h_grid;
    err_handler("Operation not allowed");
  }
  

  void setSubWindow(NCursesWindow& sub) NCURSES_OVERRIDE {
    (void) sub;
    err_handler("Operation not allowed");
  }
  

};

#endif  
