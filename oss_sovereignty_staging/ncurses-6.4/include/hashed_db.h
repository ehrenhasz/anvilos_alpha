 

 

 

#ifndef HASHED_DB_H
#define HASHED_DB_H 1

#include <ncurses_cfg.h>

#include <curses.h>

#if USE_HASHED_DB

#define DB_DBM_HSEARCH 0	 

#include <db.h>

#ifndef DBM_SUFFIX
#define DBM_SUFFIX ".db"
#endif

#ifdef DB_VERSION_MAJOR
#define HASHED_DB_API DB_VERSION_MAJOR
#else
#define HASHED_DB_API 1		 
#endif

extern NCURSES_EXPORT(DB *) _nc_db_open(const char *  , bool  );
extern NCURSES_EXPORT(bool) _nc_db_have_data(DBT *  , DBT *  , char **  , int *  );
extern NCURSES_EXPORT(bool) _nc_db_have_index(DBT *  , DBT *  , char **  , int *  );
extern NCURSES_EXPORT(int) _nc_db_close(DB *  );
extern NCURSES_EXPORT(int) _nc_db_first(DB *  , DBT *  , DBT *  );
extern NCURSES_EXPORT(int) _nc_db_next(DB *  , DBT *  , DBT *  );
extern NCURSES_EXPORT(int) _nc_db_get(DB *  , DBT *  , DBT *  );
extern NCURSES_EXPORT(int) _nc_db_put(DB *  , DBT *  , DBT *  );

#endif

#endif  
