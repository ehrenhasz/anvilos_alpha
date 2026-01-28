
#ifndef __VTERM_H__
#define __VTERM_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>

#include "vterm_keycodes.h"


#define TRUE 1
#define FALSE 0


typedef unsigned char		uint8_t;
typedef unsigned short		uint16_t;
typedef unsigned int		uint32_t;


#define VTERM_MAX_COLS 1000
#define VTERM_MAX_ROWS 1000

#define VTERM_VERSION_MAJOR 0
#define VTERM_VERSION_MINOR 3
#define VTERM_VERSION_PATCH 3

#define VTERM_CHECK_VERSION \
        vterm_check_version(VTERM_VERSION_MAJOR, VTERM_VERSION_MINOR)


#define VTERM_MAX_CHARS_PER_CELL 6

typedef struct VTerm VTerm;
typedef struct VTermState VTermState;
typedef struct VTermScreen VTermScreen;


typedef struct {
  int row;
  int col;
} VTermPos;




int vterm_pos_cmp(VTermPos a, VTermPos b);

#if defined(DEFINE_INLINES) || USE_INLINE
INLINE int vterm_pos_cmp(VTermPos a, VTermPos b)
{
  return (a.row == b.row) ? a.col - b.col : a.row - b.row;
}
#endif


typedef struct {
  int start_row;
  int end_row;
  int start_col;
  int end_col;
} VTermRect;


int vterm_rect_contains(VTermRect r, VTermPos p);

#if defined(DEFINE_INLINES) || USE_INLINE
INLINE int vterm_rect_contains(VTermRect r, VTermPos p)
{
  return p.row >= r.start_row && p.row < r.end_row &&
         p.col >= r.start_col && p.col < r.end_col;
}
#endif




void vterm_rect_move(VTermRect *rect, int row_delta, int col_delta);

#if defined(DEFINE_INLINES) || USE_INLINE
INLINE void vterm_rect_move(VTermRect *rect, int row_delta, int col_delta)
{
  rect->start_row += row_delta; rect->end_row += row_delta;
  rect->start_col += col_delta; rect->end_col += col_delta;
}
#endif


typedef enum {
  
  VTERM_COLOR_RGB = 0x00,

  
  VTERM_COLOR_INDEXED = 0x01,

  
  VTERM_COLOR_TYPE_MASK = 0x01,

  
  VTERM_COLOR_DEFAULT_FG = 0x02,

  
  VTERM_COLOR_DEFAULT_BG = 0x04,

  
  VTERM_COLOR_DEFAULT_MASK = 0x06,

  
  VTERM_COLOR_INVALID = 0x08
} VTermColorType;


#define VTERM_COLOR_IS_INDEXED(col) \
  (((col)->type & VTERM_COLOR_TYPE_MASK) == VTERM_COLOR_INDEXED)


#define VTERM_COLOR_IS_RGB(col) \
  (((col)->type & VTERM_COLOR_TYPE_MASK) == VTERM_COLOR_RGB)


#define VTERM_COLOR_IS_DEFAULT_FG(col) \
  (!!((col)->type & VTERM_COLOR_DEFAULT_FG))


#define VTERM_COLOR_IS_DEFAULT_BG(col) \
  (!!((col)->type & VTERM_COLOR_DEFAULT_BG))


#define VTERM_COLOR_IS_INVALID(col) (!!((col)->type & VTERM_COLOR_INVALID))


typedef struct {
  
  uint8_t type;

  uint8_t red, green, blue;

  uint8_t index;
} VTermColor;


void vterm_color_rgb(VTermColor *col, uint8_t red, uint8_t green, uint8_t blue);


void vterm_color_indexed(VTermColor *col, uint8_t idx);


int vterm_color_is_equal(const VTermColor *a, const VTermColor *b);

typedef enum {
  
  VTERM_VALUETYPE_BOOL = 1,
  VTERM_VALUETYPE_INT,
  VTERM_VALUETYPE_STRING,
  VTERM_VALUETYPE_COLOR,

  VTERM_N_VALUETYPES
} VTermValueType;

typedef struct {
  const char *str;
  size_t      len : 30;
  unsigned int  initial : 1;
  unsigned int  final : 1;
} VTermStringFragment;

typedef union {
  int boolean;
  int number;
  VTermStringFragment string;
  VTermColor color;
} VTermValue;

typedef enum {
  
  VTERM_ATTR_BOLD = 1,   
  VTERM_ATTR_UNDERLINE,  
  VTERM_ATTR_ITALIC,     
  VTERM_ATTR_BLINK,      
  VTERM_ATTR_REVERSE,    
  VTERM_ATTR_CONCEAL,    
  VTERM_ATTR_STRIKE,     
  VTERM_ATTR_FONT,       
  VTERM_ATTR_FOREGROUND, 
  VTERM_ATTR_BACKGROUND, 
  VTERM_ATTR_SMALL,      
  VTERM_ATTR_BASELINE,   

  VTERM_N_ATTRS
} VTermAttr;

typedef enum {
  
  VTERM_PROP_CURSORVISIBLE = 1, 
  VTERM_PROP_CURSORBLINK,       
  VTERM_PROP_ALTSCREEN,         
  VTERM_PROP_TITLE,             
  VTERM_PROP_ICONNAME,          
  VTERM_PROP_REVERSE,           
  VTERM_PROP_CURSORSHAPE,       
  VTERM_PROP_MOUSE,             
  VTERM_PROP_FOCUSREPORT,       
  VTERM_PROP_CURSORCOLOR,       

  VTERM_N_PROPS
} VTermProp;

enum {
  VTERM_PROP_CURSORSHAPE_BLOCK = 1,
  VTERM_PROP_CURSORSHAPE_UNDERLINE,
  VTERM_PROP_CURSORSHAPE_BAR_LEFT,

  VTERM_N_PROP_CURSORSHAPES
};

enum {
  VTERM_PROP_MOUSE_NONE = 0,
  VTERM_PROP_MOUSE_CLICK,
  VTERM_PROP_MOUSE_DRAG,
  VTERM_PROP_MOUSE_MOVE,

  VTERM_N_PROP_MOUSES
};

typedef enum {
  VTERM_SELECTION_CLIPBOARD = (1<<0),
  VTERM_SELECTION_PRIMARY   = (1<<1),
  VTERM_SELECTION_SECONDARY = (1<<2),
  VTERM_SELECTION_SELECT    = (1<<3),
  VTERM_SELECTION_CUT0      = (1<<4), 
} VTermSelectionMask;

typedef struct {
  const uint32_t *chars;
  int             width;
  unsigned int    protected_cell:1;  
  unsigned int    dwl:1;             
  unsigned int    dhl:2;             
} VTermGlyphInfo;

typedef struct {
  unsigned int    doublewidth:1;     
  unsigned int    doubleheight:2;    
  unsigned int    continuation:1;    
} VTermLineInfo;


typedef struct {
  VTermPos pos;                
  VTermLineInfo *lineinfos[2]; 
} VTermStateFields;

typedef struct {
  
  void *(*malloc)(size_t size, void *allocdata);
  void  (*free)(void *ptr, void *allocdata);
} VTermAllocatorFunctions;

void vterm_check_version(int major, int minor);

struct VTermBuilder {
  int ver; 

  int rows, cols;

  const VTermAllocatorFunctions *allocator;
  void *allocdata;

  
  size_t outbuffer_len;  
  size_t tmpbuffer_len;  
};

VTerm *vterm_build(const struct VTermBuilder *builder);



VTerm *vterm_new(int rows, int cols);



VTerm *vterm_new_with_allocator(int rows, int cols, VTermAllocatorFunctions *funcs, void *allocdata);


void   vterm_free(VTerm* vt);


void vterm_get_size(const VTerm *vt, int *rowsp, int *colsp);

void vterm_set_size(VTerm *vt, int rows, int cols);

int  vterm_get_utf8(const VTerm *vt);
void vterm_set_utf8(VTerm *vt, int is_utf8);

size_t vterm_input_write(VTerm *vt, const char *bytes, size_t len);


typedef void VTermOutputCallback(const char *s, size_t len, void *user);
void vterm_output_set_callback(VTerm *vt, VTermOutputCallback *func, void *user);


size_t vterm_output_get_buffer_size(const VTerm *vt);
size_t vterm_output_get_buffer_current(const VTerm *vt);
size_t vterm_output_get_buffer_remaining(const VTerm *vt);


size_t vterm_output_read(VTerm *vt, char *buffer, size_t len);

int vterm_is_modify_other_keys(VTerm *vt);
int vterm_is_kitty_keyboard(VTerm *vt);
void vterm_keyboard_unichar(VTerm *vt, uint32_t c, VTermModifier mod);
void vterm_keyboard_key(VTerm *vt, VTermKey key, VTermModifier mod);

void vterm_keyboard_start_paste(VTerm *vt);
void vterm_keyboard_end_paste(VTerm *vt);

void vterm_mouse_move(VTerm *vt, int row, int col, VTermModifier mod);


void vterm_mouse_button(VTerm *vt, int button, int pressed, VTermModifier mod);






#define CSI_ARG_FLAG_MORE (1U<<31)
#define CSI_ARG_MASK      (~(1U<<31))

#define CSI_ARG_HAS_MORE(a) ((a) & CSI_ARG_FLAG_MORE)
#define CSI_ARG(a)          ((a) & CSI_ARG_MASK)



#define CSI_ARG_MISSING ((1<<30)-1)

#define CSI_ARG_IS_MISSING(a) (CSI_ARG(a) == CSI_ARG_MISSING)
#define CSI_ARG_OR(a,def)     (CSI_ARG(a) == CSI_ARG_MISSING ? (def) : CSI_ARG(a))
#define CSI_ARG_COUNT(a)      (CSI_ARG(a) == CSI_ARG_MISSING || CSI_ARG(a) == 0 ? 1 : CSI_ARG(a))

typedef struct {
  int (*text)(const char *bytes, size_t len, void *user);
  int (*control)(unsigned char control, void *user);
  int (*escape)(const char *bytes, size_t len, void *user);
  int (*csi)(const char *leader, const long args[], int argcount, const char *intermed, char command, void *user);
  int (*osc)(int command, VTermStringFragment frag, void *user);
  int (*dcs)(const char *command, size_t commandlen, VTermStringFragment frag, void *user);
  int (*apc)(VTermStringFragment frag, void *user);
  int (*pm)(VTermStringFragment frag, void *user);
  int (*sos)(VTermStringFragment frag, void *user);
  int (*resize)(int rows, int cols, void *user);
} VTermParserCallbacks;

void  vterm_parser_set_callbacks(VTerm *vt, const VTermParserCallbacks *callbacks, void *user);
void *vterm_parser_get_cbdata(VTerm *vt);


void vterm_parser_set_emit_nul(VTerm *vt, int emit);





typedef struct {
  int (*putglyph)(VTermGlyphInfo *info, VTermPos pos, void *user);
  int (*movecursor)(VTermPos pos, VTermPos oldpos, int visible, void *user);
  int (*scrollrect)(VTermRect rect, int downward, int rightward, void *user);
  int (*moverect)(VTermRect dest, VTermRect src, void *user);
  int (*erase)(VTermRect rect, int selective, void *user);
  int (*initpen)(void *user);
  int (*setpenattr)(VTermAttr attr, VTermValue *val, void *user);
  
  
  int (*settermprop)(VTermProp prop, VTermValue *val, void *user);
  int (*bell)(void *user);
  int (*resize)(int rows, int cols, VTermStateFields *fields, void *user);
  int (*setlineinfo)(int row, const VTermLineInfo *newinfo, const VTermLineInfo *oldinfo, void *user);
  int (*sb_clear)(void *user);
} VTermStateCallbacks;


typedef struct {
  VTermPos pos;
  int	   buttons;
#define MOUSE_BUTTON_LEFT 0x01
#define MOUSE_BUTTON_MIDDLE 0x02
#define MOUSE_BUTTON_RIGHT 0x04
  int      flags;
#define MOUSE_WANT_CLICK 0x01
#define MOUSE_WANT_DRAG  0x02
#define MOUSE_WANT_MOVE  0x04
  
} VTermMouseState;

typedef struct {
  int (*control)(unsigned char control, void *user);
  int (*csi)(const char *leader, const long args[], int argcount, const char *intermed, char command, void *user);
  int (*osc)(int command, VTermStringFragment frag, void *user);
  int (*dcs)(const char *command, size_t commandlen, VTermStringFragment frag, void *user);
  int (*apc)(VTermStringFragment frag, void *user);
  int (*pm)(VTermStringFragment frag, void *user);
  int (*sos)(VTermStringFragment frag, void *user);
} VTermStateFallbacks;

typedef struct {
  int (*set)(VTermSelectionMask mask, VTermStringFragment frag, void *user);
  int (*query)(VTermSelectionMask mask, void *user);
} VTermSelectionCallbacks;

VTermState *vterm_obtain_state(VTerm *vt);

void  vterm_state_set_callbacks(VTermState *state, const VTermStateCallbacks *callbacks, void *user);
void *vterm_state_get_cbdata(VTermState *state);

void  vterm_state_set_unrecognised_fallbacks(VTermState *state, const VTermStateFallbacks *fallbacks, void *user);
void *vterm_state_get_unrecognised_fbdata(VTermState *state);


void vterm_state_reset(VTermState *state, int hard);

void vterm_state_get_cursorpos(const VTermState *state, VTermPos *cursorpos);

void vterm_state_get_mousestate(const VTermState *state, VTermMouseState *mousestate);
void vterm_state_get_default_colors(const VTermState *state, VTermColor *default_fg, VTermColor *default_bg);
void vterm_state_get_palette_color(const VTermState *state, int index, VTermColor *col);
void vterm_state_set_default_colors(VTermState *state, const VTermColor *default_fg, const VTermColor *default_bg);
void vterm_state_set_palette_color(VTermState *state, int index, const VTermColor *col);
void vterm_state_set_bold_highbright(VTermState *state, int bold_is_highbright);
int  vterm_state_get_penattr(const VTermState *state, VTermAttr attr, VTermValue *val);
int  vterm_state_set_termprop(VTermState *state, VTermProp prop, VTermValue *val);
void vterm_state_focus_in(VTermState *state);
void vterm_state_focus_out(VTermState *state);
const VTermLineInfo *vterm_state_get_lineinfo(const VTermState *state, int row);


void vterm_state_convert_color_to_rgb(const VTermState *state, VTermColor *col);

void vterm_state_set_selection_callbacks(VTermState *state, const VTermSelectionCallbacks *callbacks, void *user,
    char *buffer, size_t buflen);

void vterm_state_send_selection(VTermState *state, VTermSelectionMask mask, VTermStringFragment frag);





typedef struct {
    unsigned int bold      : 1;
    unsigned int underline : 2;
    unsigned int italic    : 1;
    unsigned int blink     : 1;
    unsigned int reverse   : 1;
    unsigned int conceal   : 1;
    unsigned int strike    : 1;
    unsigned int font      : 4; 
    unsigned int dwl       : 1; 
    unsigned int dhl       : 2; 
    unsigned int small     : 1;
    unsigned int baseline  : 2;
} VTermScreenCellAttrs;

enum {
  VTERM_UNDERLINE_OFF,
  VTERM_UNDERLINE_SINGLE,
  VTERM_UNDERLINE_DOUBLE,
  VTERM_UNDERLINE_CURLY,
};

enum {
  VTERM_BASELINE_NORMAL,
  VTERM_BASELINE_RAISE,
  VTERM_BASELINE_LOWER,
};

typedef struct {
  uint32_t chars[VTERM_MAX_CHARS_PER_CELL];
  char     width;
  VTermScreenCellAttrs attrs;
  VTermColor fg, bg;
} VTermScreenCell;


typedef struct {
  int (*damage)(VTermRect rect, void *user);
  int (*moverect)(VTermRect dest, VTermRect src, void *user);
  int (*movecursor)(VTermPos pos, VTermPos oldpos, int visible, void *user);
  int (*settermprop)(VTermProp prop, VTermValue *val, void *user);
  int (*bell)(void *user);
  int (*resize)(int rows, int cols, void *user);
  
  
  
  int (*sb_pushline)(int cols, const VTermScreenCell *cells, void *user);
  int (*sb_popline)(int cols, VTermScreenCell *cells, void *user);
  int (*sb_clear)(void* user);
} VTermScreenCallbacks;


VTermScreen *vterm_obtain_screen(VTerm *vt);


void  vterm_screen_set_callbacks(VTermScreen *screen, const VTermScreenCallbacks *callbacks, void *user);
void *vterm_screen_get_cbdata(VTermScreen *screen);

void  vterm_screen_set_unrecognised_fallbacks(VTermScreen *screen, const VTermStateFallbacks *fallbacks, void *user);
void *vterm_screen_get_unrecognised_fbdata(VTermScreen *screen);

void vterm_screen_enable_reflow(VTermScreen *screen, int reflow);


#define vterm_screen_set_reflow  vterm_screen_enable_reflow




void vterm_screen_enable_altscreen(VTermScreen *screen, int altscreen);

typedef enum {
  VTERM_DAMAGE_CELL,    
  VTERM_DAMAGE_ROW,     
  VTERM_DAMAGE_SCREEN,  
  VTERM_DAMAGE_SCROLL,  

  VTERM_N_DAMAGES
} VTermDamageSize;


void vterm_screen_flush_damage(VTermScreen *screen);

void vterm_screen_set_damage_merge(VTermScreen *screen, VTermDamageSize size);


void   vterm_screen_reset(VTermScreen *screen, int hard);


size_t vterm_screen_get_chars(const VTermScreen *screen, uint32_t *chars, size_t len, const VTermRect rect);
size_t vterm_screen_get_text(const VTermScreen *screen, char *str, size_t len, const VTermRect rect);

typedef enum {
  VTERM_ATTR_BOLD_MASK       = 1 << 0,
  VTERM_ATTR_UNDERLINE_MASK  = 1 << 1,
  VTERM_ATTR_ITALIC_MASK     = 1 << 2,
  VTERM_ATTR_BLINK_MASK      = 1 << 3,
  VTERM_ATTR_REVERSE_MASK    = 1 << 4,
  VTERM_ATTR_STRIKE_MASK     = 1 << 5,
  VTERM_ATTR_FONT_MASK       = 1 << 6,
  VTERM_ATTR_FOREGROUND_MASK = 1 << 7,
  VTERM_ATTR_BACKGROUND_MASK = 1 << 8,
  VTERM_ATTR_CONCEAL_MASK    = 1 << 9,
  VTERM_ATTR_SMALL_MASK      = 1 << 10,
  VTERM_ATTR_BASELINE_MASK   = 1 << 11,

  VTERM_ALL_ATTRS_MASK = (1 << 12) - 1
} VTermAttrMask;

int vterm_screen_get_attrs_extent(const VTermScreen *screen, VTermRect *extent, VTermPos pos, VTermAttrMask attrs);

int vterm_screen_get_cell(const VTermScreen *screen, VTermPos pos, VTermScreenCell *cell);

int vterm_screen_is_eol(const VTermScreen *screen, VTermPos pos);


void vterm_screen_convert_color_to_rgb(const VTermScreen *screen, VTermColor *col);


void vterm_screen_set_default_colors(VTermScreen *screen, const VTermColor *default_fg, const VTermColor *default_bg);





VTermValueType vterm_get_attr_type(VTermAttr attr);
VTermValueType vterm_get_prop_type(VTermProp prop);

void vterm_scroll_rect(VTermRect rect,
                       int downward,
                       int rightward,
                       int (*moverect)(VTermRect src, VTermRect dest, void *user),
                       int (*eraserect)(VTermRect rect, int selective, void *user),
                       void *user);

void vterm_copy_cells(VTermRect dest,
                      VTermRect src,
                      void (*copycell)(VTermPos dest, VTermPos src, void *user),
                      void *user);

#ifdef __cplusplus
}
#endif

#endif
