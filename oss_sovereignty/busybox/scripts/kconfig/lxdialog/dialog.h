#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#ifdef __sun__
#define CURS_MACROS
#endif
#include CURSES_LOC
#if defined(NCURSES_VERSION) && defined(_NEED_WRAP) && !defined(GCC_PRINTFLIKE)
#define OLD_NCURSES 1
#undef  wbkgdset
#define wbkgdset(w,p)		 
#else
#define OLD_NCURSES 0
#endif
#define TR(params) _tracef params
#define ESC 27
#define TAB 9
#define MAX_LEN 2048
#define BUF_SIZE (10*1024)
#define MIN(x,y) (x < y ? x : y)
#define MAX(x,y) (x > y ? x : y)
#ifndef ACS_ULCORNER
#define ACS_ULCORNER '+'
#endif
#ifndef ACS_LLCORNER
#define ACS_LLCORNER '+'
#endif
#ifndef ACS_URCORNER
#define ACS_URCORNER '+'
#endif
#ifndef ACS_LRCORNER
#define ACS_LRCORNER '+'
#endif
#ifndef ACS_HLINE
#define ACS_HLINE '-'
#endif
#ifndef ACS_VLINE
#define ACS_VLINE '|'
#endif
#ifndef ACS_LTEE
#define ACS_LTEE '+'
#endif
#ifndef ACS_RTEE
#define ACS_RTEE '+'
#endif
#ifndef ACS_UARROW
#define ACS_UARROW '^'
#endif
#ifndef ACS_DARROW
#define ACS_DARROW 'v'
#endif
#define screen_attr                   attributes[0]
#define shadow_attr                   attributes[1]
#define dialog_attr                   attributes[2]
#define title_attr                    attributes[3]
#define border_attr                   attributes[4]
#define button_active_attr            attributes[5]
#define button_inactive_attr          attributes[6]
#define button_key_active_attr        attributes[7]
#define button_key_inactive_attr      attributes[8]
#define button_label_active_attr      attributes[9]
#define button_label_inactive_attr    attributes[10]
#define inputbox_attr                 attributes[11]
#define inputbox_border_attr          attributes[12]
#define searchbox_attr                attributes[13]
#define searchbox_title_attr          attributes[14]
#define searchbox_border_attr         attributes[15]
#define position_indicator_attr       attributes[16]
#define menubox_attr                  attributes[17]
#define menubox_border_attr           attributes[18]
#define item_attr                     attributes[19]
#define item_selected_attr            attributes[20]
#define tag_attr                      attributes[21]
#define tag_selected_attr             attributes[22]
#define tag_key_attr                  attributes[23]
#define tag_key_selected_attr         attributes[24]
#define check_attr                    attributes[25]
#define check_selected_attr           attributes[26]
#define uarrow_attr                   attributes[27]
#define darrow_attr                   attributes[28]
#define ATTRIBUTE_COUNT               29
extern bool use_colors;
extern bool use_shadow;
extern chtype attributes[];
extern const char *backtitle;
extern void create_rc(const char *filename);
extern int parse_rc(void);
void init_dialog(void);
void end_dialog(void);
void attr_clear(WINDOW * win, int height, int width, chtype attr);
void dialog_clear(void);
void color_setup(void);
void print_autowrap(WINDOW * win, const char *prompt, int width, int y, int x);
void print_button(WINDOW * win, const char *label, int y, int x, int selected);
void print_title(WINDOW *dialog, const char *title, int width);
void draw_box(WINDOW * win, int y, int x, int height, int width, chtype box,
	      chtype border);
void draw_shadow(WINDOW * win, int y, int x, int height, int width);
int first_alpha(const char *string, const char *exempt);
int dialog_yesno(const char *title, const char *prompt, int height, int width);
int dialog_msgbox(const char *title, const char *prompt, int height,
		  int width, int pause);
int dialog_textbox(const char *title, const char *file, int height, int width);
int dialog_menu(const char *title, const char *prompt, int height, int width,
		int menu_height, const char *choice, int item_no,
		const char *const *items);
int dialog_checklist(const char *title, const char *prompt, int height,
		     int width, int list_height, int item_no,
		     const char *const *items);
extern char dialog_input_result[];
int dialog_inputbox(const char *title, const char *prompt, int height,
		    int width, const char *init);
#define M_EVENT (KEY_MAX+1)
