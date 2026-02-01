 

 

#if !defined (_INPUT_H_)
#define _INPUT_H_

#include "stdc.h"

 
#if !defined (_FUNCTION_DEF)
#  define _FUNCTION_DEF
typedef int Function ();
typedef void VFunction ();
typedef char *CPFunction ();		 
typedef char **CPPFunction ();		 
#endif  

typedef int sh_cget_func_t PARAMS((void));		 
typedef int sh_cunget_func_t PARAMS((int));	 

enum stream_type {st_none, st_stdin, st_stream, st_string, st_bstream};

#if defined (BUFFERED_INPUT)

 
#undef B_EOF
#undef B_ERROR		 
#undef B_UNBUFF

#define B_EOF		0x01
#define B_ERROR		0x02
#define B_UNBUFF	0x04
#define B_WASBASHINPUT	0x08
#define B_TEXT		0x10
#define B_SHAREDBUF	0x20	 

 
typedef struct BSTREAM
{
  int	 b_fd;
  char	*b_buffer;		 
  size_t b_size;		 
  size_t b_used;		 
  int	 b_flag;		 
  size_t b_inputp;		 
} BUFFERED_STREAM;

#if 0
extern BUFFERED_STREAM **buffers;
#endif

extern int default_buffered_input;
extern int bash_input_fd_changed;

#endif  

typedef union {
  FILE *file;
  char *string;
#if defined (BUFFERED_INPUT)
  int buffered_fd;
#endif
} INPUT_STREAM;

typedef struct {
  enum stream_type type;
  char *name;
  INPUT_STREAM location;
  sh_cget_func_t *getter;
  sh_cunget_func_t *ungetter;
} BASH_INPUT;

extern BASH_INPUT bash_input;

 
extern void initialize_bash_input PARAMS((void));
extern void init_yy_io PARAMS((sh_cget_func_t *, sh_cunget_func_t *, enum stream_type, const char *, INPUT_STREAM));
extern char *yy_input_name PARAMS((void));
extern void with_input_from_stdin PARAMS((void));
extern void with_input_from_string PARAMS((char *, const char *));
extern void with_input_from_stream PARAMS((FILE *, const char *));
extern void push_stream PARAMS((int));
extern void pop_stream PARAMS((void));
extern int stream_on_stack PARAMS((enum stream_type));
extern char *read_secondary_line PARAMS((int));
extern int find_reserved_word PARAMS((char *));
extern void gather_here_documents PARAMS((void));
extern void execute_variable_command PARAMS((char *, char *));

extern int *save_token_state PARAMS((void));
extern void restore_token_state PARAMS((int *));

 
extern int getc_with_restart PARAMS((FILE *));
extern int ungetc_with_restart PARAMS((int, FILE *));

#if defined (BUFFERED_INPUT)
 
extern int fd_is_bash_input PARAMS((int));
extern int set_bash_input_fd PARAMS((int));
extern int save_bash_input PARAMS((int, int));
extern int check_bash_input PARAMS((int));
extern int duplicate_buffered_stream PARAMS((int, int));
extern BUFFERED_STREAM *fd_to_buffered_stream PARAMS((int));
extern BUFFERED_STREAM *set_buffered_stream PARAMS((int, BUFFERED_STREAM *));
extern BUFFERED_STREAM *open_buffered_stream PARAMS((char *));
extern void free_buffered_stream PARAMS((BUFFERED_STREAM *));
extern int close_buffered_stream PARAMS((BUFFERED_STREAM *));
extern int close_buffered_fd PARAMS((int));
extern int sync_buffered_stream PARAMS((int));
extern int buffered_getchar PARAMS((void));
extern int buffered_ungetchar PARAMS((int));
extern void with_input_from_buffered_stream PARAMS((int, char *));
#endif  

#endif  
