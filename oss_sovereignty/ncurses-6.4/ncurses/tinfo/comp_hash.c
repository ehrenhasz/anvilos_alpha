 

 

 

#define USE_TERMLIB 1
#include <curses.priv.h>

#include <tic.h>
#include <hashsize.h>

MODULE_ID("$Id: comp_hash.c,v 1.53 2020/02/02 23:34:34 tom Exp $")

 
 
NCURSES_EXPORT(struct name_table_entry const *)
_nc_find_entry(const char *string,
	       const HashValue * hash_table)
{
    bool termcap = (hash_table != _nc_get_hash_table(FALSE));
    const HashData *data = _nc_get_hash_info(termcap);
    int hashvalue;
    struct name_table_entry const *ptr = 0;
    struct name_table_entry const *real_table;

    hashvalue = data->hash_of(string);

    if (hashvalue >= 0
	&& (unsigned) hashvalue < data->table_size
	&& data->table_data[hashvalue] >= 0) {

	real_table = _nc_get_table(termcap);
	ptr = real_table + data->table_data[hashvalue];
	while (!data->compare_names(ptr->nte_name, string)) {
	    if (ptr->nte_link < 0) {
		ptr = 0;
		break;
	    }
	    ptr = real_table + (ptr->nte_link
				+ data->table_data[data->table_size]);
	}
    }

    return (ptr);
}

 
NCURSES_EXPORT(struct name_table_entry const *)
_nc_find_type_entry(const char *string,
		    int type,
		    bool termcap)
{
    struct name_table_entry const *ptr = NULL;
    const HashData *data = _nc_get_hash_info(termcap);
    int hashvalue = data->hash_of(string);

    if (hashvalue >= 0
	&& (unsigned) hashvalue < data->table_size
	&& data->table_data[hashvalue] >= 0) {
	const struct name_table_entry *const table = _nc_get_table(termcap);

	ptr = table + data->table_data[hashvalue];
	while (ptr->nte_type != type
	       || !data->compare_names(ptr->nte_name, string)) {
	    if (ptr->nte_link < 0) {
		ptr = 0;
		break;
	    }
	    ptr = table + (ptr->nte_link + data->table_data[data->table_size]);
	}
    }

    return ptr;
}

#if NCURSES_XNAMES
NCURSES_EXPORT(struct user_table_entry const *)
_nc_find_user_entry(const char *string)
{
    const HashData *data = _nc_get_hash_user();
    int hashvalue;
    struct user_table_entry const *ptr = 0;
    struct user_table_entry const *real_table;

    hashvalue = data->hash_of(string);

    if (hashvalue >= 0
	&& (unsigned) hashvalue < data->table_size
	&& data->table_data[hashvalue] >= 0) {

	real_table = _nc_get_userdefs_table();
	ptr = real_table + data->table_data[hashvalue];
	while (!data->compare_names(ptr->ute_name, string)) {
	    if (ptr->ute_link < 0) {
		ptr = 0;
		break;
	    }
	    ptr = real_table + (ptr->ute_link
				+ data->table_data[data->table_size]);
	}
    }

    return (ptr);
}
#endif  
