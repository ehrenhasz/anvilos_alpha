 

 

#include <tparm_type.h>

MODULE_ID("$Id: tparm_type.c,v 1.4 2020/10/24 17:30:32 tom Exp $")

 
TParams
tparm_type(const char *name)
{
#define TD(code, longname, ti, tc) \
    	{code, {longname} }, \
	{code, {ti} }, \
	{code, {tc} }
    TParams result = Numbers;
     
    static const struct {
	TParams code;
	const char name[12];
    } table[] = {
	TD(Num_Str,	"pkey_key",	"pfkey",	"pk"),
	TD(Num_Str,	"pkey_local",	"pfloc",	"pl"),
	TD(Num_Str,	"pkey_xmit",	"pfx",		"px"),
	TD(Num_Str,	"plab_norm",	"pln",		"pn"),
	TD(Num_Str_Str, "pkey_plab",	"pfxl",		"xl"),
    };
     

    unsigned n;
    for (n = 0; n < SIZEOF(table); n++) {
	if (!strcmp(name, table[n].name)) {
	    result = table[n].code;
	    break;
	}
    }
    return result;
}

TParams
guess_tparm_type(int nparam, char **p_is_s)
{
    TParams result = Other;
    switch (nparam) {
    case 0:
    case 1:
	if (!p_is_s[0])
	    result = Numbers;
	break;
    case 2:
	if (!p_is_s[0] && !p_is_s[1])
	    result = Numbers;
	if (!p_is_s[0] && p_is_s[1])
	    result = Num_Str;
	break;
    case 3:
	if (!p_is_s[0] && !p_is_s[1] && !p_is_s[2])
	    result = Numbers;
	if (!p_is_s[0] && p_is_s[1] && p_is_s[2])
	    result = Num_Str_Str;
	break;
    default:
	break;
    }
    return result;
}
