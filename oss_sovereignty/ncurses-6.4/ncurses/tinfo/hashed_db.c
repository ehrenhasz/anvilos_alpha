 

 

#include <curses.priv.h>
#include <tic.h>
#include <hashed_db.h>

#if USE_HASHED_DB

MODULE_ID("$Id: hashed_db.c,v 1.19 2020/02/02 23:34:34 tom Exp $")

#if HASHED_DB_API >= 2
static DBC *cursor;
#endif

typedef struct _myconn {
    struct _myconn *next;
    DB *db;
    char *path;
    bool modify;
} MYCONN;

static MYCONN *connections;

static void
cleanup(void)
{
    while (connections != 0) {
	_nc_db_close(connections->db);
    }
}

static DB *
find_connection(const char *path, bool modify)
{
    DB *result = 0;
    MYCONN *p;

    for (p = connections; p != 0; p = p->next) {
	if (!strcmp(p->path, path) && p->modify == modify) {
	    result = p->db;
	    break;
	}
    }

    return result;
}

static void
drop_connection(DB * db)
{
    MYCONN *p, *q;

    for (p = connections, q = 0; p != 0; q = p, p = p->next) {
	if (p->db == db) {
	    if (q != 0)
		q->next = p->next;
	    else
		connections = p->next;
	    free(p->path);
	    free(p);
	    break;
	}
    }
}

static void
make_connection(DB * db, const char *path, bool modify)
{
    MYCONN *p = typeCalloc(MYCONN, 1);

    if (p != 0) {
	p->db = db;
	p->path = strdup(path);
	p->modify = modify;
	if (p->path != 0) {
	    p->next = connections;
	    connections = p;
	} else {
	    free(p);
	}
    }
}

 
NCURSES_EXPORT(DB *)
_nc_db_open(const char *path, bool modify)
{
    DB *result = 0;
    int code;

    if (connections == 0)
	atexit(cleanup);

    if ((result = find_connection(path, modify)) == 0) {

#if HASHED_DB_API >= 4
	db_create(&result, NULL, 0);
	if ((code = result->open(result,
				 NULL,
				 path,
				 NULL,
				 DB_HASH,
				 modify ? DB_CREATE : DB_RDONLY,
				 0644)) != 0) {
	    result = 0;
	}
#elif HASHED_DB_API >= 3
	db_create(&result, NULL, 0);
	if ((code = result->open(result,
				 path,
				 NULL,
				 DB_HASH,
				 modify ? DB_CREATE : DB_RDONLY,
				 0644)) != 0) {
	    result = 0;
	}
#elif HASHED_DB_API >= 2
	if ((code = db_open(path,
			    DB_HASH,
			    modify ? DB_CREATE : DB_RDONLY,
			    0644,
			    (DB_ENV *) 0,
			    (DB_INFO *) 0,
			    &result)) != 0) {
	    result = 0;
	}
#else
	if ((result = dbopen(path,
			     modify ? (O_CREAT | O_RDWR) : O_RDONLY,
			     0644,
			     DB_HASH,
			     NULL)) == 0) {
	    code = errno;
	}
#endif
	if (result != 0) {
	    make_connection(result, path, modify);
	    T(("opened %s", path));
	} else {
	    T(("cannot open %s: %s", path, strerror(code)));
	}
    }
    return result;
}

 
NCURSES_EXPORT(int)
_nc_db_close(DB * db)
{
    int result;

    drop_connection(db);
#if HASHED_DB_API >= 2
    result = db->close(db, 0);
#else
    result = db->close(db);
#endif
    return result;
}

 
NCURSES_EXPORT(int)
_nc_db_put(DB * db, DBT * key, DBT * data)
{
    int result;
#if HASHED_DB_API >= 2
     
    (void) db->del(db, NULL, key, 0);
    result = db->put(db, NULL, key, data, DB_NOOVERWRITE);
#else
    result = db->put(db, key, data, R_NOOVERWRITE);
#endif
    return result;
}

 
NCURSES_EXPORT(int)
_nc_db_get(DB * db, DBT * key, DBT * data)
{
    int result;

    memset(data, 0, sizeof(*data));
#if HASHED_DB_API >= 2
    result = db->get(db, NULL, key, data, 0);
#else
    result = db->get(db, key, data, 0);
#endif
    return result;
}

 
NCURSES_EXPORT(int)
_nc_db_first(DB * db, DBT * key, DBT * data)
{
    int result;

    memset(key, 0, sizeof(*key));
    memset(data, 0, sizeof(*data));
#if HASHED_DB_API >= 2
    if ((result = db->cursor(db, NULL, &cursor, 0)) == 0) {
	result = cursor->c_get(cursor, key, data, DB_FIRST);
    }
#else
    result = db->seq(db, key, data, 0);
#endif
    return result;
}

 
NCURSES_EXPORT(int)
_nc_db_next(DB * db, DBT * key, DBT * data)
{
    int result;

#if HASHED_DB_API >= 2
    (void) db;
    if (cursor != 0) {
	result = cursor->c_get(cursor, key, data, DB_NEXT);
    } else {
	result = -1;
    }
#else
    result = db->seq(db, key, data, R_NEXT);
#endif
    return result;
}

 
NCURSES_EXPORT(bool)
_nc_db_have_index(DBT * key, DBT * data, char **buffer, int *size)
{
    bool result = FALSE;
    int used = (int) data->size - 1;
    char *have = (char *) data->data;

    (void) key;
    if (*have++ == 2) {
	result = TRUE;
    }
     
    *buffer = have;
    *size = used;
    return result;
}

 
NCURSES_EXPORT(bool)
_nc_db_have_data(DBT * key, DBT * data, char **buffer, int *size)
{
    bool result = FALSE;
    int used = (int) data->size - 1;
    char *have = (char *) data->data;

    if (*have++ == 0) {
	if (data->size > key->size
	    && IS_TIC_MAGIC(have)) {
	    result = TRUE;
	}
    }
     
    *buffer = have;
    *size = used;
    return result;
}

#else

extern
NCURSES_EXPORT(void)
_nc_hashed_db(void);

NCURSES_EXPORT(void)
_nc_hashed_db(void)
{
}

#endif  
