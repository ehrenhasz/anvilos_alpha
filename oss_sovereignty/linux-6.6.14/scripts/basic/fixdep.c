 

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

static void usage(void)
{
	fprintf(stderr, "Usage: fixdep <depfile> <target> <cmdline>\n");
	exit(1);
}

struct item {
	struct item	*next;
	unsigned int	len;
	unsigned int	hash;
	char		name[];
};

#define HASHSZ 256
static struct item *config_hashtab[HASHSZ], *file_hashtab[HASHSZ];

static unsigned int strhash(const char *str, unsigned int sz)
{
	 
	unsigned int i, hash = 2166136261U;

	for (i = 0; i < sz; i++)
		hash = (hash ^ str[i]) * 0x01000193;
	return hash;
}

 
static void add_to_hashtable(const char *name, int len, unsigned int hash,
			     struct item *hashtab[])
{
	struct item *aux = malloc(sizeof(*aux) + len);

	if (!aux) {
		perror("fixdep:malloc");
		exit(1);
	}
	memcpy(aux->name, name, len);
	aux->len = len;
	aux->hash = hash;
	aux->next = hashtab[hash % HASHSZ];
	hashtab[hash % HASHSZ] = aux;
}

 
static bool in_hashtable(const char *name, int len, struct item *hashtab[])
{
	struct item *aux;
	unsigned int hash = strhash(name, len);

	for (aux = hashtab[hash % HASHSZ]; aux; aux = aux->next) {
		if (aux->hash == hash && aux->len == len &&
		    memcmp(aux->name, name, len) == 0)
			return true;
	}

	add_to_hashtable(name, len, hash, hashtab);

	return false;
}

 
static void use_config(const char *m, int slen)
{
	if (in_hashtable(m, slen, config_hashtab))
		return;

	 
	printf("    $(wildcard include/config/%.*s) \\\n", slen, m);
}

 
static int str_ends_with(const char *s, int slen, const char *sub)
{
	int sublen = strlen(sub);

	if (sublen > slen)
		return 0;

	return !memcmp(s + slen - sublen, sub, sublen);
}

static void parse_config_file(const char *p)
{
	const char *q, *r;
	const char *start = p;

	while ((p = strstr(p, "CONFIG_"))) {
		if (p > start && (isalnum(p[-1]) || p[-1] == '_')) {
			p += 7;
			continue;
		}
		p += 7;
		q = p;
		while (isalnum(*q) || *q == '_')
			q++;
		if (str_ends_with(p, q - p, "_MODULE"))
			r = q - 7;
		else
			r = q;
		if (r > p)
			use_config(p, r - p);
		p = q;
	}
}

static void *read_file(const char *filename)
{
	struct stat st;
	int fd;
	char *buf;

	fd = open(filename, O_RDONLY);
	if (fd < 0) {
		fprintf(stderr, "fixdep: error opening file: ");
		perror(filename);
		exit(2);
	}
	if (fstat(fd, &st) < 0) {
		fprintf(stderr, "fixdep: error fstat'ing file: ");
		perror(filename);
		exit(2);
	}
	buf = malloc(st.st_size + 1);
	if (!buf) {
		perror("fixdep: malloc");
		exit(2);
	}
	if (read(fd, buf, st.st_size) != st.st_size) {
		perror("fixdep: read");
		exit(2);
	}
	buf[st.st_size] = '\0';
	close(fd);

	return buf;
}

 
static int is_ignored_file(const char *s, int len)
{
	return str_ends_with(s, len, "include/generated/autoconf.h");
}

 
static int is_no_parse_file(const char *s, int len)
{
	 
	return str_ends_with(s, len, ".rlib") ||
	       str_ends_with(s, len, ".rmeta") ||
	       str_ends_with(s, len, ".so");
}

 
static void parse_dep_file(char *p, const char *target)
{
	bool saw_any_target = false;
	bool is_target = true;
	bool is_source = false;
	bool need_parse;
	char *q, saved_c;

	while (*p) {
		 
		switch (*p) {
		case '#':
			 
			p++;
			while (*p != '\0' && *p != '\n') {
				 
				if (*p == '\\')
					p++;
				p++;
			}
			continue;
		case ' ':
		case '\t':
			 
			p++;
			continue;
		case '\\':
			 
			if (*(p + 1) == '\n') {
				p += 2;
				continue;
			}
			break;
		case '\n':
			 
			p++;
			is_target = true;
			continue;
		case ':':
			 
			p++;
			is_target = false;
			is_source = true;
			continue;
		}

		 
		q = p;
		while (*q != ' ' && *q != '\t' && *q != '\n' && *q != '#' && *q != ':') {
			if (*q == '\\') {
				 
				if (*(q + 1) == '\n')
					break;

				 
				if (*(q + 1) == '#' || *(q + 1) == ':') {
					memmove(p + 1, p, q - p);
					p++;
				}

				q++;
			}

			if (*q == '\0')
				break;
			q++;
		}

		 
		if (is_target) {
			p = q;
			continue;
		}

		saved_c = *q;
		*q = '\0';
		need_parse = false;

		 
		if (is_source) {
			 
			if (!saw_any_target) {
				saw_any_target = true;
				printf("source_%s := %s\n\n", target, p);
				printf("deps_%s := \\\n", target);
				need_parse = true;
			}
		} else if (!is_ignored_file(p, q - p) &&
			   !in_hashtable(p, q - p, file_hashtab)) {
			printf("  %s \\\n", p);
			need_parse = true;
		}

		if (need_parse && !is_no_parse_file(p, q - p)) {
			void *buf;

			buf = read_file(p);
			parse_config_file(buf);
			free(buf);
		}

		is_source = false;
		*q = saved_c;
		p = q;
	}

	if (!saw_any_target) {
		fprintf(stderr, "fixdep: parse error; no targets found\n");
		exit(1);
	}

	printf("\n%s: $(deps_%s)\n\n", target, target);
	printf("$(deps_%s):\n", target);
}

int main(int argc, char *argv[])
{
	const char *depfile, *target, *cmdline;
	void *buf;

	if (argc != 4)
		usage();

	depfile = argv[1];
	target = argv[2];
	cmdline = argv[3];

	printf("savedcmd_%s := %s\n\n", target, cmdline);

	buf = read_file(depfile);
	parse_dep_file(buf, target);
	free(buf);

	fflush(stdout);

	 
	if (ferror(stdout)) {
		fprintf(stderr, "fixdep: not all data was written to the output\n");
		exit(1);
	}

	return 0;
}
