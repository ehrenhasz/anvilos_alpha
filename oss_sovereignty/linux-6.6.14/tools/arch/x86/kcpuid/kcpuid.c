
#define _GNU_SOURCE

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

typedef unsigned int u32;
typedef unsigned long long u64;

char *def_csv = "/usr/share/misc/cpuid.csv";
char *user_csv;


 
struct bits_desc {
	 
	int start, end;
	 
	int value;
	char simp[32];
	char detail[256];
};

 
struct reg_desc {
	 
	int nr;
	struct bits_desc descs[32];
};

enum cpuid_reg {
	R_EAX = 0,
	R_EBX,
	R_ECX,
	R_EDX,
	NR_REGS
};

static const char * const reg_names[] = {
	"EAX", "EBX", "ECX", "EDX",
};

struct subleaf {
	u32 index;
	u32 sub;
	u32 eax, ebx, ecx, edx;
	struct reg_desc info[NR_REGS];
};

 
struct cpuid_func {
	 
	struct subleaf *leafs;
	int nr;
};

struct cpuid_range {
	 
	struct cpuid_func *funcs;
	 
	int nr;
	bool is_ext;
};

 
struct cpuid_range *leafs_basic, *leafs_ext;

static int num_leafs;
static bool is_amd;
static bool show_details;
static bool show_raw;
static bool show_flags_only = true;
static u32 user_index = 0xFFFFFFFF;
static u32 user_sub = 0xFFFFFFFF;
static int flines;

static inline void cpuid(u32 *eax, u32 *ebx, u32 *ecx, u32 *edx)
{
	 
	asm volatile("cpuid"
	    : "=a" (*eax),
	      "=b" (*ebx),
	      "=c" (*ecx),
	      "=d" (*edx)
	    : "0" (*eax), "2" (*ecx));
}

static inline bool has_subleafs(u32 f)
{
	if (f == 0x7 || f == 0xd)
		return true;

	if (is_amd) {
		if (f == 0x8000001d)
			return true;
		return false;
	}

	switch (f) {
	case 0x4:
	case 0xb:
	case 0xf:
	case 0x10:
	case 0x14:
	case 0x18:
	case 0x1f:
		return true;
	default:
		return false;
	}
}

static void leaf_print_raw(struct subleaf *leaf)
{
	if (has_subleafs(leaf->index)) {
		if (leaf->sub == 0)
			printf("0x%08x: subleafs:\n", leaf->index);

		printf(" %2d: EAX=0x%08x, EBX=0x%08x, ECX=0x%08x, EDX=0x%08x\n",
			leaf->sub, leaf->eax, leaf->ebx, leaf->ecx, leaf->edx);
	} else {
		printf("0x%08x: EAX=0x%08x, EBX=0x%08x, ECX=0x%08x, EDX=0x%08x\n",
			leaf->index, leaf->eax, leaf->ebx, leaf->ecx, leaf->edx);
	}
}

 
static bool cpuid_store(struct cpuid_range *range, u32 f, int subleaf,
			u32 a, u32 b, u32 c, u32 d)
{
	struct cpuid_func *func;
	struct subleaf *leaf;
	int s = 0;

	if (a == 0 && b == 0 && c == 0 && d == 0)
		return true;

	 
	func = &range->funcs[f & 0xffff];

	if (!func->leafs) {
		func->leafs = malloc(sizeof(struct subleaf));
		if (!func->leafs)
			perror("malloc func leaf");

		func->nr = 1;
	} else {
		s = func->nr;
		func->leafs = realloc(func->leafs, (s + 1) * sizeof(*leaf));
		if (!func->leafs)
			perror("realloc f->leafs");

		func->nr++;
	}

	leaf = &func->leafs[s];

	leaf->index = f;
	leaf->sub = subleaf;
	leaf->eax = a;
	leaf->ebx = b;
	leaf->ecx = c;
	leaf->edx = d;

	return false;
}

static void raw_dump_range(struct cpuid_range *range)
{
	u32 f;
	int i;

	printf("%s Leafs :\n", range->is_ext ? "Extended" : "Basic");
	printf("================\n");

	for (f = 0; (int)f < range->nr; f++) {
		struct cpuid_func *func = &range->funcs[f];
		u32 index = f;

		if (range->is_ext)
			index += 0x80000000;

		 
		if (!func->nr)
			continue;

		 
		for (i = 0; i < func->nr; i++)
			leaf_print_raw(&func->leafs[i]);
	}
}

#define MAX_SUBLEAF_NUM		32
struct cpuid_range *setup_cpuid_range(u32 input_eax)
{
	u32 max_func, idx_func;
	int subleaf;
	struct cpuid_range *range;
	u32 eax, ebx, ecx, edx;
	u32 f = input_eax;
	int max_subleaf;
	bool allzero;

	eax = input_eax;
	ebx = ecx = edx = 0;

	cpuid(&eax, &ebx, &ecx, &edx);
	max_func = eax;
	idx_func = (max_func & 0xffff) + 1;

	range = malloc(sizeof(struct cpuid_range));
	if (!range)
		perror("malloc range");

	if (input_eax & 0x80000000)
		range->is_ext = true;
	else
		range->is_ext = false;

	range->funcs = malloc(sizeof(struct cpuid_func) * idx_func);
	if (!range->funcs)
		perror("malloc range->funcs");

	range->nr = idx_func;
	memset(range->funcs, 0, sizeof(struct cpuid_func) * idx_func);

	for (; f <= max_func; f++) {
		eax = f;
		subleaf = ecx = 0;

		cpuid(&eax, &ebx, &ecx, &edx);
		allzero = cpuid_store(range, f, subleaf, eax, ebx, ecx, edx);
		if (allzero)
			continue;
		num_leafs++;

		if (!has_subleafs(f))
			continue;

		max_subleaf = MAX_SUBLEAF_NUM;

		 
		if (f == 0x7 || f == 0x14 || f == 0x17 || f == 0x18)
			max_subleaf = (eax & 0xff) + 1;

		if (f == 0xb)
			max_subleaf = 2;

		for (subleaf = 1; subleaf < max_subleaf; subleaf++) {
			eax = f;
			ecx = subleaf;

			cpuid(&eax, &ebx, &ecx, &edx);
			allzero = cpuid_store(range, f, subleaf,
						eax, ebx, ecx, edx);
			if (allzero)
				continue;
			num_leafs++;
		}

	}

	return range;
}

 
static int parse_line(char *line)
{
	char *str;
	int i;
	struct cpuid_range *range;
	struct cpuid_func *func;
	struct subleaf *leaf;
	u32 index;
	u32 sub;
	char buffer[512];
	char *buf;
	 
	char *tokens[6];
	struct reg_desc *reg;
	struct bits_desc *bdesc;
	int reg_index;
	char *start, *end;

	 
	if (line[0] == '#' || line[0] == '\n')
		return 0;

	strncpy(buffer, line, 511);
	buffer[511] = 0;
	str = buffer;
	for (i = 0; i < 5; i++) {
		tokens[i] = strtok(str, ",");
		if (!tokens[i])
			goto err_exit;
		str = NULL;
	}
	tokens[5] = strtok(str, "\n");
	if (!tokens[5])
		goto err_exit;

	 
	index = strtoull(tokens[0], NULL, 0);

	if (index & 0x80000000)
		range = leafs_ext;
	else
		range = leafs_basic;

	index &= 0x7FFFFFFF;
	 
	if ((int)index >= range->nr)
		return -1;

	func = &range->funcs[index];

	 
	if (!func->nr)
		return 0;

	 
	sub = strtoul(tokens[1], NULL, 0);
	if ((int)sub > func->nr)
		return -1;

	leaf = &func->leafs[sub];
	buf = tokens[2];

	if (strcasestr(buf, "EAX"))
		reg_index = R_EAX;
	else if (strcasestr(buf, "EBX"))
		reg_index = R_EBX;
	else if (strcasestr(buf, "ECX"))
		reg_index = R_ECX;
	else if (strcasestr(buf, "EDX"))
		reg_index = R_EDX;
	else
		goto err_exit;

	reg = &leaf->info[reg_index];
	bdesc = &reg->descs[reg->nr++];

	 
	buf = tokens[3];

	end = strtok(buf, ":");
	bdesc->end = strtoul(end, NULL, 0);
	bdesc->start = bdesc->end;

	 
	start = strtok(NULL, ":");
	if (start)
		bdesc->start = strtoul(start, NULL, 0);

	strcpy(bdesc->simp, tokens[4]);
	strcpy(bdesc->detail, tokens[5]);
	return 0;

err_exit:
	printf("Warning: wrong line format:\n");
	printf("\tline[%d]: %s\n", flines, line);
	return -1;
}

 
static void parse_text(void)
{
	FILE *file;
	char *filename, *line = NULL;
	size_t len = 0;
	int ret;

	if (show_raw)
		return;

	filename = user_csv ? user_csv : def_csv;
	file = fopen(filename, "r");
	if (!file) {
		 
		file = fopen("./cpuid.csv", "r");
	}

	if (!file) {
		printf("Fail to open '%s'\n", filename);
		return;
	}

	while (1) {
		ret = getline(&line, &len, file);
		flines++;
		if (ret > 0)
			parse_line(line);

		if (feof(file))
			break;
	}

	fclose(file);
}


 
static void decode_bits(u32 value, struct reg_desc *rdesc, enum cpuid_reg reg)
{
	struct bits_desc *bdesc;
	int start, end, i;
	u32 mask;

	if (!rdesc->nr) {
		if (show_details)
			printf("\t %s: 0x%08x\n", reg_names[reg], value);
		return;
	}

	for (i = 0; i < rdesc->nr; i++) {
		bdesc = &rdesc->descs[i];

		start = bdesc->start;
		end = bdesc->end;
		if (start == end) {
			 
			if (value & (1 << start))
				printf("\t%-20s %s%s\n",
					bdesc->simp,
					show_details ? "-" : "",
					show_details ? bdesc->detail : ""
					);
		} else {
			 
			if (show_flags_only)
				continue;

			mask = ((u64)1 << (end - start + 1)) - 1;
			printf("\t%-20s\t: 0x%-8x\t%s%s\n",
					bdesc->simp,
					(value >> start) & mask,
					show_details ? "-" : "",
					show_details ? bdesc->detail : ""
					);
		}
	}
}

static void show_leaf(struct subleaf *leaf)
{
	if (!leaf)
		return;

	if (show_raw) {
		leaf_print_raw(leaf);
	} else {
		if (show_details)
			printf("CPUID_0x%x_ECX[0x%x]:\n",
				leaf->index, leaf->sub);
	}

	decode_bits(leaf->eax, &leaf->info[R_EAX], R_EAX);
	decode_bits(leaf->ebx, &leaf->info[R_EBX], R_EBX);
	decode_bits(leaf->ecx, &leaf->info[R_ECX], R_ECX);
	decode_bits(leaf->edx, &leaf->info[R_EDX], R_EDX);

	if (!show_raw && show_details)
		printf("\n");
}

static void show_func(struct cpuid_func *func)
{
	int i;

	if (!func)
		return;

	for (i = 0; i < func->nr; i++)
		show_leaf(&func->leafs[i]);
}

static void show_range(struct cpuid_range *range)
{
	int i;

	for (i = 0; i < range->nr; i++)
		show_func(&range->funcs[i]);
}

static inline struct cpuid_func *index_to_func(u32 index)
{
	struct cpuid_range *range;
	u32 func_idx;

	range = (index & 0x80000000) ? leafs_ext : leafs_basic;
	func_idx = index & 0xffff;

	if ((func_idx + 1) > (u32)range->nr) {
		printf("ERR: invalid input index (0x%x)\n", index);
		return NULL;
	}
	return &range->funcs[func_idx];
}

static void show_info(void)
{
	struct cpuid_func *func;

	if (show_raw) {
		 
		raw_dump_range(leafs_basic);
		raw_dump_range(leafs_ext);
		return;
	}

	if (user_index != 0xFFFFFFFF) {
		 
		func = index_to_func(user_index);
		if (!func)
			return;

		 
		show_raw = true;

		if (user_sub != 0xFFFFFFFF) {
			if (user_sub + 1 <= (u32)func->nr) {
				show_leaf(&func->leafs[user_sub]);
				return;
			}

			printf("ERR: invalid input subleaf (0x%x)\n", user_sub);
		}

		show_func(func);
		return;
	}

	printf("CPU features:\n=============\n\n");
	show_range(leafs_basic);
	show_range(leafs_ext);
}

static void setup_platform_cpuid(void)
{
	 u32 eax, ebx, ecx, edx;

	 
	eax = ebx = ecx = edx = 0;
	cpuid(&eax, &ebx, &ecx, &edx);

	 
	if (ebx == 0x68747541)
		is_amd = true;

	 
	leafs_basic = setup_cpuid_range(0x0);
	leafs_ext = setup_cpuid_range(0x80000000);
}

static void usage(void)
{
	printf("kcpuid [-abdfhr] [-l leaf] [-s subleaf]\n"
		"\t-a|--all             Show both bit flags and complex bit fields info\n"
		"\t-b|--bitflags        Show boolean flags only\n"
		"\t-d|--detail          Show details of the flag/fields (default)\n"
		"\t-f|--flags           Specify the cpuid csv file\n"
		"\t-h|--help            Show usage info\n"
		"\t-l|--leaf=index      Specify the leaf you want to check\n"
		"\t-r|--raw             Show raw cpuid data\n"
		"\t-s|--subleaf=sub     Specify the subleaf you want to check\n"
	);
}

static struct option opts[] = {
	{ "all", no_argument, NULL, 'a' },		 
	{ "bitflags", no_argument, NULL, 'b' },		 
	{ "detail", no_argument, NULL, 'd' },		 
	{ "file", required_argument, NULL, 'f' },	 
	{ "help", no_argument, NULL, 'h'},		 
	{ "leaf", required_argument, NULL, 'l'},	 
	{ "raw", no_argument, NULL, 'r'},		 
	{ "subleaf", required_argument, NULL, 's'},	 
	{ NULL, 0, NULL, 0 }
};

static int parse_options(int argc, char *argv[])
{
	int c;

	while ((c = getopt_long(argc, argv, "abdf:hl:rs:",
					opts, NULL)) != -1)
		switch (c) {
		case 'a':
			show_flags_only = false;
			break;
		case 'b':
			show_flags_only = true;
			break;
		case 'd':
			show_details = true;
			break;
		case 'f':
			user_csv = optarg;
			break;
		case 'h':
			usage();
			exit(1);
			break;
		case 'l':
			 
			user_index = strtoul(optarg, NULL, 0);
			break;
		case 'r':
			show_raw = true;
			break;
		case 's':
			 
			user_sub = strtoul(optarg, NULL, 0);
			break;
		default:
			printf("%s: Invalid option '%c'\n", argv[0], optopt);
			return -1;
	}

	return 0;
}

 
int main(int argc, char *argv[])
{
	if (parse_options(argc, argv))
		return -1;

	 
	setup_platform_cpuid();

	 
	parse_text();

	show_info();
	return 0;
}
