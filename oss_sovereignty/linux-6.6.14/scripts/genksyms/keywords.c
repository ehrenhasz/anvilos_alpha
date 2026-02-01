
static struct resword {
	const char *name;
	int token;
} keywords[] = {
	{ "__GENKSYMS_EXPORT_SYMBOL", EXPORT_SYMBOL_KEYW },
	{ "__asm", ASM_KEYW },
	{ "__asm__", ASM_KEYW },
	{ "__attribute", ATTRIBUTE_KEYW },
	{ "__attribute__", ATTRIBUTE_KEYW },
	{ "__const", CONST_KEYW },
	{ "__const__", CONST_KEYW },
	{ "__extension__", EXTENSION_KEYW },
	{ "__inline", INLINE_KEYW },
	{ "__inline__", INLINE_KEYW },
	{ "__signed", SIGNED_KEYW },
	{ "__signed__", SIGNED_KEYW },
	{ "__typeof", TYPEOF_KEYW },
	{ "__typeof__", TYPEOF_KEYW },
	{ "__volatile", VOLATILE_KEYW },
	{ "__volatile__", VOLATILE_KEYW },
	{ "__builtin_va_list", VA_LIST_KEYW },

	{ "__int128", BUILTIN_INT_KEYW },
	{ "__int128_t", BUILTIN_INT_KEYW },
	{ "__uint128_t", BUILTIN_INT_KEYW },

	
	{ "_Bool", BOOL_KEYW },
	{ "__restrict", RESTRICT_KEYW },
	{ "__restrict__", RESTRICT_KEYW },
	{ "restrict", RESTRICT_KEYW },
	{ "asm", ASM_KEYW },

	
	{ "_Static_assert", STATIC_ASSERT_KEYW },

	
	
	
	

	{ "auto", AUTO_KEYW },
	{ "char", CHAR_KEYW },
	{ "const", CONST_KEYW },
	{ "double", DOUBLE_KEYW },
	{ "enum", ENUM_KEYW },
	{ "extern", EXTERN_KEYW },
	{ "float", FLOAT_KEYW },
	{ "inline", INLINE_KEYW },
	{ "int", INT_KEYW },
	{ "long", LONG_KEYW },
	{ "register", REGISTER_KEYW },
	{ "short", SHORT_KEYW },
	{ "signed", SIGNED_KEYW },
	{ "static", STATIC_KEYW },
	{ "struct", STRUCT_KEYW },
	{ "typedef", TYPEDEF_KEYW },
	{ "typeof", TYPEOF_KEYW },
	{ "union", UNION_KEYW },
	{ "unsigned", UNSIGNED_KEYW },
	{ "void", VOID_KEYW },
	{ "volatile", VOLATILE_KEYW },
};

#define NR_KEYWORDS (sizeof(keywords)/sizeof(struct resword))

static int is_reserved_word(register const char *str, register unsigned int len)
{
	int i;
	for (i = 0; i < NR_KEYWORDS; i++) {
		struct resword *r = keywords + i;
		int l = strlen(r->name);
		if (len == l && !memcmp(str, r->name, len))
			return r->token;
	}
	return -1;
}
