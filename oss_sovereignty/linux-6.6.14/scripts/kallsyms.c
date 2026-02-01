 

#include <errno.h>
#include <getopt.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof(arr[0]))

#define KSYM_NAME_LEN		512

struct sym_entry {
	unsigned long long addr;
	unsigned int len;
	unsigned int seq;
	unsigned int start_pos;
	unsigned int percpu_absolute;
	unsigned char sym[];
};

struct addr_range {
	const char *start_sym, *end_sym;
	unsigned long long start, end;
};

static unsigned long long _text;
static unsigned long long relative_base;
static struct addr_range text_ranges[] = {
	{ "_stext",     "_etext"     },
	{ "_sinittext", "_einittext" },
};
#define text_range_text     (&text_ranges[0])
#define text_range_inittext (&text_ranges[1])

static struct addr_range percpu_range = {
	"__per_cpu_start", "__per_cpu_end", -1ULL, 0
};

static struct sym_entry **table;
static unsigned int table_size, table_cnt;
static int all_symbols;
static int absolute_percpu;
static int base_relative;
static int lto_clang;

static int token_profit[0x10000];

 
static unsigned char best_table[256][2];
static unsigned char best_table_len[256];


static void usage(void)
{
	fprintf(stderr, "Usage: kallsyms [--all-symbols] [--absolute-percpu] "
			"[--base-relative] [--lto-clang] in.map > out.S\n");
	exit(1);
}

static char *sym_name(const struct sym_entry *s)
{
	return (char *)s->sym + 1;
}

static bool is_ignored_symbol(const char *name, char type)
{
	if (type == 'u' || type == 'n')
		return true;

	if (toupper(type) == 'A') {
		 
		if (strcmp(name, "__kernel_syscall_via_break") &&
		    strcmp(name, "__kernel_syscall_via_epc") &&
		    strcmp(name, "__kernel_sigtramp") &&
		    strcmp(name, "__gp"))
			return true;
	}

	return false;
}

static void check_symbol_range(const char *sym, unsigned long long addr,
			       struct addr_range *ranges, int entries)
{
	size_t i;
	struct addr_range *ar;

	for (i = 0; i < entries; ++i) {
		ar = &ranges[i];

		if (strcmp(sym, ar->start_sym) == 0) {
			ar->start = addr;
			return;
		} else if (strcmp(sym, ar->end_sym) == 0) {
			ar->end = addr;
			return;
		}
	}
}

static struct sym_entry *read_symbol(FILE *in, char **buf, size_t *buf_len)
{
	char *name, type, *p;
	unsigned long long addr;
	size_t len;
	ssize_t readlen;
	struct sym_entry *sym;

	errno = 0;
	readlen = getline(buf, buf_len, in);
	if (readlen < 0) {
		if (errno) {
			perror("read_symbol");
			exit(EXIT_FAILURE);
		}
		return NULL;
	}

	if ((*buf)[readlen - 1] == '\n')
		(*buf)[readlen - 1] = 0;

	addr = strtoull(*buf, &p, 16);

	if (*buf == p || *p++ != ' ' || !isascii((type = *p++)) || *p++ != ' ') {
		fprintf(stderr, "line format error\n");
		exit(EXIT_FAILURE);
	}

	name = p;
	len = strlen(name);

	if (len >= KSYM_NAME_LEN) {
		fprintf(stderr, "Symbol %s too long for kallsyms (%zu >= %d).\n"
				"Please increase KSYM_NAME_LEN both in kernel and kallsyms.c\n",
			name, len, KSYM_NAME_LEN);
		return NULL;
	}

	if (strcmp(name, "_text") == 0)
		_text = addr;

	 
	if (is_ignored_symbol(name, type))
		return NULL;

	check_symbol_range(name, addr, text_ranges, ARRAY_SIZE(text_ranges));
	check_symbol_range(name, addr, &percpu_range, 1);

	 
	len++;

	sym = malloc(sizeof(*sym) + len + 1);
	if (!sym) {
		fprintf(stderr, "kallsyms failure: "
			"unable to allocate required amount of memory\n");
		exit(EXIT_FAILURE);
	}
	sym->addr = addr;
	sym->len = len;
	sym->sym[0] = type;
	strcpy(sym_name(sym), name);
	sym->percpu_absolute = 0;

	return sym;
}

static int symbol_in_range(const struct sym_entry *s,
			   const struct addr_range *ranges, int entries)
{
	size_t i;
	const struct addr_range *ar;

	for (i = 0; i < entries; ++i) {
		ar = &ranges[i];

		if (s->addr >= ar->start && s->addr <= ar->end)
			return 1;
	}

	return 0;
}

static int symbol_valid(const struct sym_entry *s)
{
	const char *name = sym_name(s);

	 
	if (!all_symbols) {
		if (symbol_in_range(s, text_ranges,
				    ARRAY_SIZE(text_ranges)) == 0)
			return 0;
		 
		if ((s->addr == text_range_text->end &&
		     strcmp(name, text_range_text->end_sym)) ||
		    (s->addr == text_range_inittext->end &&
		     strcmp(name, text_range_inittext->end_sym)))
			return 0;
	}

	return 1;
}

 
static void shrink_table(void)
{
	unsigned int i, pos;

	pos = 0;
	for (i = 0; i < table_cnt; i++) {
		if (symbol_valid(table[i])) {
			if (pos != i)
				table[pos] = table[i];
			pos++;
		} else {
			free(table[i]);
		}
	}
	table_cnt = pos;

	 
	if (!table_cnt) {
		fprintf(stderr, "No valid symbol.\n");
		exit(1);
	}
}

static void read_map(const char *in)
{
	FILE *fp;
	struct sym_entry *sym;
	char *buf = NULL;
	size_t buflen = 0;

	fp = fopen(in, "r");
	if (!fp) {
		perror(in);
		exit(1);
	}

	while (!feof(fp)) {
		sym = read_symbol(fp, &buf, &buflen);
		if (!sym)
			continue;

		sym->start_pos = table_cnt;

		if (table_cnt >= table_size) {
			table_size += 10000;
			table = realloc(table, sizeof(*table) * table_size);
			if (!table) {
				fprintf(stderr, "out of memory\n");
				fclose(fp);
				exit (1);
			}
		}

		table[table_cnt++] = sym;
	}

	free(buf);
	fclose(fp);
}

static void output_label(const char *label)
{
	printf(".globl %s\n", label);
	printf("\tALGN\n");
	printf("%s:\n", label);
}

 
static void output_address(unsigned long long addr)
{
	if (_text <= addr)
		printf("\tPTR\t_text + %#llx\n", addr - _text);
	else
		printf("\tPTR\t_text - %#llx\n", _text - addr);
}

 
static int expand_symbol(const unsigned char *data, int len, char *result)
{
	int c, rlen, total=0;

	while (len) {
		c = *data;
		 
		if (best_table[c][0]==c && best_table_len[c]==1) {
			*result++ = c;
			total++;
		} else {
			 
			rlen = expand_symbol(best_table[c], best_table_len[c], result);
			total += rlen;
			result += rlen;
		}
		data++;
		len--;
	}
	*result=0;

	return total;
}

static int symbol_absolute(const struct sym_entry *s)
{
	return s->percpu_absolute;
}

static void cleanup_symbol_name(char *s)
{
	char *p;

	 
	p = strstr(s, ".llvm.");
	if (p)
		*p = '\0';
}

static int compare_names(const void *a, const void *b)
{
	int ret;
	const struct sym_entry *sa = *(const struct sym_entry **)a;
	const struct sym_entry *sb = *(const struct sym_entry **)b;

	ret = strcmp(sym_name(sa), sym_name(sb));
	if (!ret) {
		if (sa->addr > sb->addr)
			return 1;
		else if (sa->addr < sb->addr)
			return -1;

		 
		return (int)(sa->seq - sb->seq);
	}

	return ret;
}

static void sort_symbols_by_name(void)
{
	qsort(table, table_cnt, sizeof(table[0]), compare_names);
}

static void write_src(void)
{
	unsigned int i, k, off;
	unsigned int best_idx[256];
	unsigned int *markers;
	char buf[KSYM_NAME_LEN];

	printf("#include <asm/bitsperlong.h>\n");
	printf("#if BITS_PER_LONG == 64\n");
	printf("#define PTR .quad\n");
	printf("#define ALGN .balign 8\n");
	printf("#else\n");
	printf("#define PTR .long\n");
	printf("#define ALGN .balign 4\n");
	printf("#endif\n");

	printf("\t.section .rodata, \"a\"\n");

	output_label("kallsyms_num_syms");
	printf("\t.long\t%u\n", table_cnt);
	printf("\n");

	 
	markers = malloc(sizeof(unsigned int) * ((table_cnt + 255) / 256));
	if (!markers) {
		fprintf(stderr, "kallsyms failure: "
			"unable to allocate required memory\n");
		exit(EXIT_FAILURE);
	}

	output_label("kallsyms_names");
	off = 0;
	for (i = 0; i < table_cnt; i++) {
		if ((i & 0xFF) == 0)
			markers[i >> 8] = off;
		table[i]->seq = i;

		 
		if (table[i]->len == 0) {
			fprintf(stderr, "kallsyms failure: "
				"unexpected zero symbol length\n");
			exit(EXIT_FAILURE);
		}

		 
		if (table[i]->len > 0x3FFF) {
			fprintf(stderr, "kallsyms failure: "
				"unexpected huge symbol length\n");
			exit(EXIT_FAILURE);
		}

		 
		if (table[i]->len <= 0x7F) {
			 
			printf("\t.byte 0x%02x", table[i]->len);
			off += table[i]->len + 1;
		} else {
			 
			printf("\t.byte 0x%02x, 0x%02x",
				(table[i]->len & 0x7F) | 0x80,
				(table[i]->len >> 7) & 0x7F);
			off += table[i]->len + 2;
		}
		for (k = 0; k < table[i]->len; k++)
			printf(", 0x%02x", table[i]->sym[k]);
		printf("\n");
	}
	printf("\n");

	 
	for (i = 0; i < table_cnt; i++) {
		expand_symbol(table[i]->sym, table[i]->len, buf);
		strcpy((char *)table[i]->sym, buf);
	}

	output_label("kallsyms_markers");
	for (i = 0; i < ((table_cnt + 255) >> 8); i++)
		printf("\t.long\t%u\n", markers[i]);
	printf("\n");

	free(markers);

	output_label("kallsyms_token_table");
	off = 0;
	for (i = 0; i < 256; i++) {
		best_idx[i] = off;
		expand_symbol(best_table[i], best_table_len[i], buf);
		printf("\t.asciz\t\"%s\"\n", buf);
		off += strlen(buf) + 1;
	}
	printf("\n");

	output_label("kallsyms_token_index");
	for (i = 0; i < 256; i++)
		printf("\t.short\t%d\n", best_idx[i]);
	printf("\n");

	if (!base_relative)
		output_label("kallsyms_addresses");
	else
		output_label("kallsyms_offsets");

	for (i = 0; i < table_cnt; i++) {
		if (base_relative) {
			 

			long long offset;
			int overflow;

			if (!absolute_percpu) {
				offset = table[i]->addr - relative_base;
				overflow = (offset < 0 || offset > UINT_MAX);
			} else if (symbol_absolute(table[i])) {
				offset = table[i]->addr;
				overflow = (offset < 0 || offset > INT_MAX);
			} else {
				offset = relative_base - table[i]->addr - 1;
				overflow = (offset < INT_MIN || offset >= 0);
			}
			if (overflow) {
				fprintf(stderr, "kallsyms failure: "
					"%s symbol value %#llx out of range in relative mode\n",
					symbol_absolute(table[i]) ? "absolute" : "relative",
					table[i]->addr);
				exit(EXIT_FAILURE);
			}
			printf("\t.long\t%#x	/* %s */\n", (int)offset, table[i]->sym);
		} else if (!symbol_absolute(table[i])) {
			output_address(table[i]->addr);
		} else {
			printf("\tPTR\t%#llx\n", table[i]->addr);
		}
	}
	printf("\n");

	if (base_relative) {
		output_label("kallsyms_relative_base");
		output_address(relative_base);
		printf("\n");
	}

	if (lto_clang)
		for (i = 0; i < table_cnt; i++)
			cleanup_symbol_name((char *)table[i]->sym);

	sort_symbols_by_name();
	output_label("kallsyms_seqs_of_names");
	for (i = 0; i < table_cnt; i++)
		printf("\t.byte 0x%02x, 0x%02x, 0x%02x\n",
			(unsigned char)(table[i]->seq >> 16),
			(unsigned char)(table[i]->seq >> 8),
			(unsigned char)(table[i]->seq >> 0));
	printf("\n");
}


 

 
static void learn_symbol(const unsigned char *symbol, int len)
{
	int i;

	for (i = 0; i < len - 1; i++)
		token_profit[ symbol[i] + (symbol[i + 1] << 8) ]++;
}

 
static void forget_symbol(const unsigned char *symbol, int len)
{
	int i;

	for (i = 0; i < len - 1; i++)
		token_profit[ symbol[i] + (symbol[i + 1] << 8) ]--;
}

 
static void build_initial_token_table(void)
{
	unsigned int i;

	for (i = 0; i < table_cnt; i++)
		learn_symbol(table[i]->sym, table[i]->len);
}

static unsigned char *find_token(unsigned char *str, int len,
				 const unsigned char *token)
{
	int i;

	for (i = 0; i < len - 1; i++) {
		if (str[i] == token[0] && str[i+1] == token[1])
			return &str[i];
	}
	return NULL;
}

 
static void compress_symbols(const unsigned char *str, int idx)
{
	unsigned int i, len, size;
	unsigned char *p1, *p2;

	for (i = 0; i < table_cnt; i++) {

		len = table[i]->len;
		p1 = table[i]->sym;

		 
		p2 = find_token(p1, len, str);
		if (!p2) continue;

		 
		forget_symbol(table[i]->sym, len);

		size = len;

		do {
			*p2 = idx;
			p2++;
			size -= (p2 - p1);
			memmove(p2, p2 + 1, size);
			p1 = p2;
			len--;

			if (size < 2) break;

			 
			p2 = find_token(p1, size, str);

		} while (p2);

		table[i]->len = len;

		 
		learn_symbol(table[i]->sym, len);
	}
}

 
static int find_best_token(void)
{
	int i, best, bestprofit;

	bestprofit=-10000;
	best = 0;

	for (i = 0; i < 0x10000; i++) {
		if (token_profit[i] > bestprofit) {
			best = i;
			bestprofit = token_profit[i];
		}
	}
	return best;
}

 
static void optimize_result(void)
{
	int i, best;

	 
	for (i = 255; i >= 0; i--) {

		 
		if (!best_table_len[i]) {

			 
			best = find_best_token();
			if (token_profit[best] == 0)
				break;

			 
			best_table_len[i] = 2;
			best_table[i][0] = best & 0xFF;
			best_table[i][1] = (best >> 8) & 0xFF;

			 
			compress_symbols(best_table[i], i);
		}
	}
}

 
static void insert_real_symbols_in_table(void)
{
	unsigned int i, j, c;

	for (i = 0; i < table_cnt; i++) {
		for (j = 0; j < table[i]->len; j++) {
			c = table[i]->sym[j];
			best_table[c][0]=c;
			best_table_len[c]=1;
		}
	}
}

static void optimize_token_table(void)
{
	build_initial_token_table();

	insert_real_symbols_in_table();

	optimize_result();
}

 
static int may_be_linker_script_provide_symbol(const struct sym_entry *se)
{
	const char *symbol = sym_name(se);
	int len = se->len - 1;

	if (len < 8)
		return 0;

	if (symbol[0] != '_' || symbol[1] != '_')
		return 0;

	 
	if (!memcmp(symbol + 2, "start_", 6))
		return 1;

	 
	if (!memcmp(symbol + 2, "stop_", 5))
		return 1;

	 
	if (!memcmp(symbol + 2, "end_", 4))
		return 1;

	 
	if (!memcmp(symbol + len - 6, "_start", 6))
		return 1;

	 
	if (!memcmp(symbol + len - 4, "_end", 4))
		return 1;

	return 0;
}

static int compare_symbols(const void *a, const void *b)
{
	const struct sym_entry *sa = *(const struct sym_entry **)a;
	const struct sym_entry *sb = *(const struct sym_entry **)b;
	int wa, wb;

	 
	if (sa->addr > sb->addr)
		return 1;
	if (sa->addr < sb->addr)
		return -1;

	 
	wa = (sa->sym[0] == 'w') || (sa->sym[0] == 'W');
	wb = (sb->sym[0] == 'w') || (sb->sym[0] == 'W');
	if (wa != wb)
		return wa - wb;

	 
	wa = may_be_linker_script_provide_symbol(sa);
	wb = may_be_linker_script_provide_symbol(sb);
	if (wa != wb)
		return wa - wb;

	 
	wa = strspn(sym_name(sa), "_");
	wb = strspn(sym_name(sb), "_");
	if (wa != wb)
		return wa - wb;

	 
	return sa->start_pos - sb->start_pos;
}

static void sort_symbols(void)
{
	qsort(table, table_cnt, sizeof(table[0]), compare_symbols);
}

static void make_percpus_absolute(void)
{
	unsigned int i;

	for (i = 0; i < table_cnt; i++)
		if (symbol_in_range(table[i], &percpu_range, 1)) {
			 
			table[i]->sym[0] = 'A';
			table[i]->percpu_absolute = 1;
		}
}

 
static void record_relative_base(void)
{
	unsigned int i;

	for (i = 0; i < table_cnt; i++)
		if (!symbol_absolute(table[i])) {
			 
			relative_base = table[i]->addr;
			return;
		}
}

int main(int argc, char **argv)
{
	while (1) {
		static const struct option long_options[] = {
			{"all-symbols",     no_argument, &all_symbols,     1},
			{"absolute-percpu", no_argument, &absolute_percpu, 1},
			{"base-relative",   no_argument, &base_relative,   1},
			{"lto-clang",       no_argument, &lto_clang,       1},
			{},
		};

		int c = getopt_long(argc, argv, "", long_options, NULL);

		if (c == -1)
			break;
		if (c != 0)
			usage();
	}

	if (optind >= argc)
		usage();

	read_map(argv[optind]);
	shrink_table();
	if (absolute_percpu)
		make_percpus_absolute();
	sort_symbols();
	if (base_relative)
		record_relative_base();
	optimize_token_table();
	write_src();

	return 0;
}
