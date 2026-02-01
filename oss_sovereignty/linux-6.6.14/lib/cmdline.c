
 

#include <linux/export.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/ctype.h>

 

static int get_range(char **str, int *pint, int n)
{
	int x, inc_counter, upper_range;

	(*str)++;
	upper_range = simple_strtol((*str), NULL, 0);
	inc_counter = upper_range - *pint;
	for (x = *pint; n && x < upper_range; x++, n--)
		*pint++ = x;
	return inc_counter;
}

 

int get_option(char **str, int *pint)
{
	char *cur = *str;
	int value;

	if (!cur || !(*cur))
		return 0;
	if (*cur == '-')
		value = -simple_strtoull(++cur, str, 0);
	else
		value = simple_strtoull(cur, str, 0);
	if (pint)
		*pint = value;
	if (cur == *str)
		return 0;
	if (**str == ',') {
		(*str)++;
		return 2;
	}
	if (**str == '-')
		return 3;

	return 1;
}
EXPORT_SYMBOL(get_option);

 

char *get_options(const char *str, int nints, int *ints)
{
	bool validate = (nints == 0);
	int res, i = 1;

	while (i < nints || validate) {
		int *pint = validate ? ints : ints + i;

		res = get_option((char **)&str, pint);
		if (res == 0)
			break;
		if (res == 3) {
			int n = validate ? 0 : nints - i;
			int range_nums;

			range_nums = get_range((char **)&str, pint, n);
			if (range_nums < 0)
				break;
			 
			i += (range_nums - 1);
		}
		i++;
		if (res == 1)
			break;
	}
	ints[0] = i - 1;
	return (char *)str;
}
EXPORT_SYMBOL(get_options);

 

unsigned long long memparse(const char *ptr, char **retptr)
{
	char *endptr;	 

	unsigned long long ret = simple_strtoull(ptr, &endptr, 0);

	switch (*endptr) {
	case 'E':
	case 'e':
		ret <<= 10;
		fallthrough;
	case 'P':
	case 'p':
		ret <<= 10;
		fallthrough;
	case 'T':
	case 't':
		ret <<= 10;
		fallthrough;
	case 'G':
	case 'g':
		ret <<= 10;
		fallthrough;
	case 'M':
	case 'm':
		ret <<= 10;
		fallthrough;
	case 'K':
	case 'k':
		ret <<= 10;
		endptr++;
		fallthrough;
	default:
		break;
	}

	if (retptr)
		*retptr = endptr;

	return ret;
}
EXPORT_SYMBOL(memparse);

 
bool parse_option_str(const char *str, const char *option)
{
	while (*str) {
		if (!strncmp(str, option, strlen(option))) {
			str += strlen(option);
			if (!*str || *str == ',')
				return true;
		}

		while (*str && *str != ',')
			str++;

		if (*str == ',')
			str++;
	}

	return false;
}

 
char *next_arg(char *args, char **param, char **val)
{
	unsigned int i, equals = 0;
	int in_quote = 0, quoted = 0;

	if (*args == '"') {
		args++;
		in_quote = 1;
		quoted = 1;
	}

	for (i = 0; args[i]; i++) {
		if (isspace(args[i]) && !in_quote)
			break;
		if (equals == 0) {
			if (args[i] == '=')
				equals = i;
		}
		if (args[i] == '"')
			in_quote = !in_quote;
	}

	*param = args;
	if (!equals)
		*val = NULL;
	else {
		args[equals] = '\0';
		*val = args + equals + 1;

		 
		if (**val == '"') {
			(*val)++;
			if (args[i-1] == '"')
				args[i-1] = '\0';
		}
	}
	if (quoted && i > 0 && args[i-1] == '"')
		args[i-1] = '\0';

	if (args[i]) {
		args[i] = '\0';
		args += i + 1;
	} else
		args += i;

	 
	return skip_spaces(args);
}
EXPORT_SYMBOL(next_arg);
