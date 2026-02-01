 
extern bool convert_entire_line;

 
extern size_t max_column_width;

 
extern int exit_status;

 
extern void
add_tab_stop (uintmax_t tabval);

 
extern void
parse_tab_stops (char const *stops) _GL_ATTRIBUTE_NONNULL ();

 
extern uintmax_t
get_next_tab_column (const uintmax_t column, size_t *tab_index,
                     bool *last_tab)
  _GL_ATTRIBUTE_NONNULL ((3));

 
extern void
finalize_tab_stops (void);




 
extern void
set_file_list (char **file_list);

 
extern FILE *
next_file (FILE *fp);

 
extern void
cleanup_file_list_stdin (void);


extern void
emit_tab_list_info (void);
