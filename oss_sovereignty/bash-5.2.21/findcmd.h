 

 

#if !defined (_FINDCMD_H_)
#define _FINDCMD_H_

#include "stdc.h"

 
#define CMDSRCH_HASH		0x01
#define CMDSRCH_STDPATH		0x02
#define CMDSRCH_TEMPENV		0x04

extern int file_status PARAMS((const char *));
extern int executable_file PARAMS((const char *));
extern int is_directory PARAMS((const char *));
extern int executable_or_directory PARAMS((const char *));
extern char *find_user_command PARAMS((const char *));
extern char *find_in_path PARAMS((const char *, char *, int));
extern char *find_path_file PARAMS((const char *));
extern char *search_for_command PARAMS((const char *, int));
extern char *user_command_matches PARAMS((const char *, int, int));
extern void setup_exec_ignore PARAMS((char *));

extern int dot_found_in_search;

 
extern int check_hashed_filenames;

#endif  
