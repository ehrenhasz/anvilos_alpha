 

 

#include <curses.priv.h>
#include <tic.h>

MODULE_ID("$Id: report_hashing.c,v 1.3 2020/02/02 23:34:34 tom Exp $")

static void
check_names(const char *name, NCURSES_CONST char *const *table, int termcap)
{
    int errs = 0;
    int n;
    struct name_table_entry const *entry_ptr;
    const HashValue *hash_table = _nc_get_hash_table(termcap);

    printf("%s:\n", name);
    for (n = 0; table[n] != NULL; ++n) {
	entry_ptr = _nc_find_entry(table[n], hash_table);
	if (entry_ptr == 0) {
	    printf("  %s\n", table[n]);
	    errs++;
	}
    }
    if (errs)
	printf("%d errors\n", errs);
}

int
main(void)
{
#define CHECK_TI(name) check_names(#name, name, 0)
#define CHECK_TC(name) check_names(#name, name, 1)

    CHECK_TI(boolnames);
    CHECK_TI(numnames);
    CHECK_TI(strnames);

    CHECK_TC(boolcodes);
    CHECK_TC(numcodes);
    CHECK_TC(strcodes);

    return EXIT_SUCCESS;
}
