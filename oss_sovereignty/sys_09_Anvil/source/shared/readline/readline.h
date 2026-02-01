 
#ifndef MICROPY_INCLUDED_LIB_MP_READLINE_READLINE_H
#define MICROPY_INCLUDED_LIB_MP_READLINE_READLINE_H

#define CHAR_CTRL_A (1)
#define CHAR_CTRL_B (2)
#define CHAR_CTRL_C (3)
#define CHAR_CTRL_D (4)
#define CHAR_CTRL_E (5)
#define CHAR_CTRL_F (6)
#define CHAR_CTRL_K (11)
#define CHAR_CTRL_N (14)
#define CHAR_CTRL_P (16)
#define CHAR_CTRL_U (21)
#define CHAR_CTRL_W (23)

void readline_init0(void);
int readline(vstr_t *line, const char *prompt);
void readline_push_history(const char *line);

void readline_init(vstr_t *line, const char *prompt);
void readline_note_newline(const char *prompt);
int readline_process_char(int c);

#endif 
