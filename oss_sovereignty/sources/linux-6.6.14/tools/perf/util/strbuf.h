
#ifndef __PERF_STRBUF_H
#define __PERF_STRBUF_H



#include <assert.h>
#include <stdarg.h>
#include <stddef.h>
#include <string.h>
#include <linux/compiler.h>
#include <sys/types.h>

extern char strbuf_slopbuf[];
struct strbuf {
	size_t alloc;
	size_t len;
	char *buf;
};

#define STRBUF_INIT  { 0, 0, strbuf_slopbuf }


int strbuf_init(struct strbuf *buf, ssize_t hint);
void strbuf_release(struct strbuf *buf);
char *strbuf_detach(struct strbuf *buf, size_t *);


static inline ssize_t strbuf_avail(const struct strbuf *sb) {
	return sb->alloc ? sb->alloc - sb->len - 1 : 0;
}

int strbuf_grow(struct strbuf *buf, size_t);

static inline int strbuf_setlen(struct strbuf *sb, size_t len) {
	if (!sb->alloc) {
		int ret = strbuf_grow(sb, 0);
		if (ret)
			return ret;
	}
	assert(len < sb->alloc);
	sb->len = len;
	sb->buf[len] = '\0';
	return 0;
}


int strbuf_addch(struct strbuf *sb, int c);

int strbuf_add(struct strbuf *buf, const void *, size_t);
static inline int strbuf_addstr(struct strbuf *sb, const char *s) {
	return strbuf_add(sb, s, strlen(s));
}

int strbuf_addf(struct strbuf *sb, const char *fmt, ...) __printf(2, 3);


ssize_t strbuf_read(struct strbuf *, int fd, ssize_t hint);

#endif 
