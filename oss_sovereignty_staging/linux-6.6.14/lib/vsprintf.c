
 

 
 

 

#include <linux/stdarg.h>
#include <linux/build_bug.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/errname.h>
#include <linux/module.h>	 
#include <linux/types.h>
#include <linux/string.h>
#include <linux/ctype.h>
#include <linux/kernel.h>
#include <linux/kallsyms.h>
#include <linux/math64.h>
#include <linux/uaccess.h>
#include <linux/ioport.h>
#include <linux/dcache.h>
#include <linux/cred.h>
#include <linux/rtc.h>
#include <linux/sprintf.h>
#include <linux/time.h>
#include <linux/uuid.h>
#include <linux/of.h>
#include <net/addrconf.h>
#include <linux/siphash.h>
#include <linux/compiler.h>
#include <linux/property.h>
#include <linux/notifier.h>
#ifdef CONFIG_BLOCK
#include <linux/blkdev.h>
#endif

#include "../mm/internal.h"	 

#include <asm/page.h>		 
#include <asm/byteorder.h>	 
#include <asm/unaligned.h>

#include <linux/string_helpers.h>
#include "kstrtox.h"

 
bool no_hash_pointers __ro_after_init;
EXPORT_SYMBOL_GPL(no_hash_pointers);

static noinline unsigned long long simple_strntoull(const char *startp, size_t max_chars, char **endp, unsigned int base)
{
	const char *cp;
	unsigned long long result = 0ULL;
	size_t prefix_chars;
	unsigned int rv;

	cp = _parse_integer_fixup_radix(startp, &base);
	prefix_chars = cp - startp;
	if (prefix_chars < max_chars) {
		rv = _parse_integer_limit(cp, base, &result, max_chars - prefix_chars);
		 
		cp += (rv & ~KSTRTOX_OVERFLOW);
	} else {
		 
		cp = startp + max_chars;
	}

	if (endp)
		*endp = (char *)cp;

	return result;
}

 
noinline
unsigned long long simple_strtoull(const char *cp, char **endp, unsigned int base)
{
	return simple_strntoull(cp, INT_MAX, endp, base);
}
EXPORT_SYMBOL(simple_strtoull);

 
unsigned long simple_strtoul(const char *cp, char **endp, unsigned int base)
{
	return simple_strtoull(cp, endp, base);
}
EXPORT_SYMBOL(simple_strtoul);

 
long simple_strtol(const char *cp, char **endp, unsigned int base)
{
	if (*cp == '-')
		return -simple_strtoul(cp + 1, endp, base);

	return simple_strtoul(cp, endp, base);
}
EXPORT_SYMBOL(simple_strtol);

static long long simple_strntoll(const char *cp, size_t max_chars, char **endp,
				 unsigned int base)
{
	 
	if (*cp == '-' && max_chars > 0)
		return -simple_strntoull(cp + 1, max_chars - 1, endp, base);

	return simple_strntoull(cp, max_chars, endp, base);
}

 
long long simple_strtoll(const char *cp, char **endp, unsigned int base)
{
	return simple_strntoll(cp, INT_MAX, endp, base);
}
EXPORT_SYMBOL(simple_strtoll);

static noinline_for_stack
int skip_atoi(const char **s)
{
	int i = 0;

	do {
		i = i*10 + *((*s)++) - '0';
	} while (isdigit(**s));

	return i;
}

 

static const u16 decpair[100] = {
#define _(x) (__force u16) cpu_to_le16(((x % 10) | ((x / 10) << 8)) + 0x3030)
	_( 0), _( 1), _( 2), _( 3), _( 4), _( 5), _( 6), _( 7), _( 8), _( 9),
	_(10), _(11), _(12), _(13), _(14), _(15), _(16), _(17), _(18), _(19),
	_(20), _(21), _(22), _(23), _(24), _(25), _(26), _(27), _(28), _(29),
	_(30), _(31), _(32), _(33), _(34), _(35), _(36), _(37), _(38), _(39),
	_(40), _(41), _(42), _(43), _(44), _(45), _(46), _(47), _(48), _(49),
	_(50), _(51), _(52), _(53), _(54), _(55), _(56), _(57), _(58), _(59),
	_(60), _(61), _(62), _(63), _(64), _(65), _(66), _(67), _(68), _(69),
	_(70), _(71), _(72), _(73), _(74), _(75), _(76), _(77), _(78), _(79),
	_(80), _(81), _(82), _(83), _(84), _(85), _(86), _(87), _(88), _(89),
	_(90), _(91), _(92), _(93), _(94), _(95), _(96), _(97), _(98), _(99),
#undef _
};

 
static noinline_for_stack
char *put_dec_trunc8(char *buf, unsigned r)
{
	unsigned q;

	 
	if (r < 100)
		goto out_r;

	 
	q = (r * (u64)0x28f5c29) >> 32;
	*((u16 *)buf) = decpair[r - 100*q];
	buf += 2;

	 
	if (q < 100)
		goto out_q;

	 
	r = (q * (u64)0x28f5c29) >> 32;
	*((u16 *)buf) = decpair[q - 100*r];
	buf += 2;

	 
	if (r < 100)
		goto out_r;

	 
	q = (r * 0x147b) >> 19;
	*((u16 *)buf) = decpair[r - 100*q];
	buf += 2;
out_q:
	 
	r = q;
out_r:
	 
	*((u16 *)buf) = decpair[r];
	buf += r < 10 ? 1 : 2;
	return buf;
}

#if BITS_PER_LONG == 64 && BITS_PER_LONG_LONG == 64
static noinline_for_stack
char *put_dec_full8(char *buf, unsigned r)
{
	unsigned q;

	 
	q = (r * (u64)0x28f5c29) >> 32;
	*((u16 *)buf) = decpair[r - 100*q];
	buf += 2;

	 
	r = (q * (u64)0x28f5c29) >> 32;
	*((u16 *)buf) = decpair[q - 100*r];
	buf += 2;

	 
	q = (r * 0x147b) >> 19;
	*((u16 *)buf) = decpair[r - 100*q];
	buf += 2;

	 
	*((u16 *)buf) = decpair[q];
	buf += 2;
	return buf;
}

static noinline_for_stack
char *put_dec(char *buf, unsigned long long n)
{
	if (n >= 100*1000*1000)
		buf = put_dec_full8(buf, do_div(n, 100*1000*1000));
	 
	if (n >= 100*1000*1000)
		buf = put_dec_full8(buf, do_div(n, 100*1000*1000));
	 
	return put_dec_trunc8(buf, n);
}

#elif BITS_PER_LONG == 32 && BITS_PER_LONG_LONG == 64

static void
put_dec_full4(char *buf, unsigned r)
{
	unsigned q;

	 
	q = (r * 0x147b) >> 19;
	*((u16 *)buf) = decpair[r - 100*q];
	buf += 2;
	 
	*((u16 *)buf) = decpair[q];
}

 
static noinline_for_stack
unsigned put_dec_helper4(char *buf, unsigned x)
{
        uint32_t q = (x * (uint64_t)0x346DC5D7) >> 43;

        put_dec_full4(buf, x - q * 10000);
        return q;
}

 
static
char *put_dec(char *buf, unsigned long long n)
{
	uint32_t d3, d2, d1, q, h;

	if (n < 100*1000*1000)
		return put_dec_trunc8(buf, n);

	d1  = ((uint32_t)n >> 16);  
	h   = (n >> 32);
	d2  = (h      ) & 0xffff;
	d3  = (h >> 16);  

	 
	q   = 656 * d3 + 7296 * d2 + 5536 * d1 + ((uint32_t)n & 0xffff);
	q = put_dec_helper4(buf, q);

	q += 7671 * d3 + 9496 * d2 + 6 * d1;
	q = put_dec_helper4(buf+4, q);

	q += 4749 * d3 + 42 * d2;
	q = put_dec_helper4(buf+8, q);

	q += 281 * d3;
	buf += 12;
	if (q)
		buf = put_dec_trunc8(buf, q);
	else while (buf[-1] == '0')
		--buf;

	return buf;
}

#endif

 
int num_to_str(char *buf, int size, unsigned long long num, unsigned int width)
{
	 
	char tmp[sizeof(num) * 3] __aligned(2);
	int idx, len;

	 
	if (num <= 9) {
		tmp[0] = '0' + num;
		len = 1;
	} else {
		len = put_dec(tmp, num) - tmp;
	}

	if (len > size || width > size)
		return 0;

	if (width > len) {
		width = width - len;
		for (idx = 0; idx < width; idx++)
			buf[idx] = ' ';
	} else {
		width = 0;
	}

	for (idx = 0; idx < len; ++idx)
		buf[idx + width] = tmp[len - idx - 1];

	return len + width;
}

#define SIGN	1		 
#define LEFT	2		 
#define PLUS	4		 
#define SPACE	8		 
#define ZEROPAD	16		 
#define SMALL	32		 
#define SPECIAL	64		 

static_assert(SIGN == 1);
static_assert(ZEROPAD == ('0' - ' '));
static_assert(SMALL == ('a' ^ 'A'));

enum format_type {
	FORMAT_TYPE_NONE,  
	FORMAT_TYPE_WIDTH,
	FORMAT_TYPE_PRECISION,
	FORMAT_TYPE_CHAR,
	FORMAT_TYPE_STR,
	FORMAT_TYPE_PTR,
	FORMAT_TYPE_PERCENT_CHAR,
	FORMAT_TYPE_INVALID,
	FORMAT_TYPE_LONG_LONG,
	FORMAT_TYPE_ULONG,
	FORMAT_TYPE_LONG,
	FORMAT_TYPE_UBYTE,
	FORMAT_TYPE_BYTE,
	FORMAT_TYPE_USHORT,
	FORMAT_TYPE_SHORT,
	FORMAT_TYPE_UINT,
	FORMAT_TYPE_INT,
	FORMAT_TYPE_SIZE_T,
	FORMAT_TYPE_PTRDIFF
};

struct printf_spec {
	unsigned int	type:8;		 
	signed int	field_width:24;	 
	unsigned int	flags:8;	 
	unsigned int	base:8;		 
	signed int	precision:16;	 
} __packed;
static_assert(sizeof(struct printf_spec) == 8);

#define FIELD_WIDTH_MAX ((1 << 23) - 1)
#define PRECISION_MAX ((1 << 15) - 1)

static noinline_for_stack
char *number(char *buf, char *end, unsigned long long num,
	     struct printf_spec spec)
{
	 
	char tmp[3 * sizeof(num)] __aligned(2);
	char sign;
	char locase;
	int need_pfx = ((spec.flags & SPECIAL) && spec.base != 10);
	int i;
	bool is_zero = num == 0LL;
	int field_width = spec.field_width;
	int precision = spec.precision;

	 
	locase = (spec.flags & SMALL);
	if (spec.flags & LEFT)
		spec.flags &= ~ZEROPAD;
	sign = 0;
	if (spec.flags & SIGN) {
		if ((signed long long)num < 0) {
			sign = '-';
			num = -(signed long long)num;
			field_width--;
		} else if (spec.flags & PLUS) {
			sign = '+';
			field_width--;
		} else if (spec.flags & SPACE) {
			sign = ' ';
			field_width--;
		}
	}
	if (need_pfx) {
		if (spec.base == 16)
			field_width -= 2;
		else if (!is_zero)
			field_width--;
	}

	 
	i = 0;
	if (num < spec.base)
		tmp[i++] = hex_asc_upper[num] | locase;
	else if (spec.base != 10) {  
		int mask = spec.base - 1;
		int shift = 3;

		if (spec.base == 16)
			shift = 4;
		do {
			tmp[i++] = (hex_asc_upper[((unsigned char)num) & mask] | locase);
			num >>= shift;
		} while (num);
	} else {  
		i = put_dec(tmp, num) - tmp;
	}

	 
	if (i > precision)
		precision = i;
	 
	field_width -= precision;
	if (!(spec.flags & (ZEROPAD | LEFT))) {
		while (--field_width >= 0) {
			if (buf < end)
				*buf = ' ';
			++buf;
		}
	}
	 
	if (sign) {
		if (buf < end)
			*buf = sign;
		++buf;
	}
	 
	if (need_pfx) {
		if (spec.base == 16 || !is_zero) {
			if (buf < end)
				*buf = '0';
			++buf;
		}
		if (spec.base == 16) {
			if (buf < end)
				*buf = ('X' | locase);
			++buf;
		}
	}
	 
	if (!(spec.flags & LEFT)) {
		char c = ' ' + (spec.flags & ZEROPAD);

		while (--field_width >= 0) {
			if (buf < end)
				*buf = c;
			++buf;
		}
	}
	 
	while (i <= --precision) {
		if (buf < end)
			*buf = '0';
		++buf;
	}
	 
	while (--i >= 0) {
		if (buf < end)
			*buf = tmp[i];
		++buf;
	}
	 
	while (--field_width >= 0) {
		if (buf < end)
			*buf = ' ';
		++buf;
	}

	return buf;
}

static noinline_for_stack
char *special_hex_number(char *buf, char *end, unsigned long long num, int size)
{
	struct printf_spec spec;

	spec.type = FORMAT_TYPE_PTR;
	spec.field_width = 2 + 2 * size;	 
	spec.flags = SPECIAL | SMALL | ZEROPAD;
	spec.base = 16;
	spec.precision = -1;

	return number(buf, end, num, spec);
}

static void move_right(char *buf, char *end, unsigned len, unsigned spaces)
{
	size_t size;
	if (buf >= end)	 
		return;
	size = end - buf;
	if (size <= spaces) {
		memset(buf, ' ', size);
		return;
	}
	if (len) {
		if (len > size - spaces)
			len = size - spaces;
		memmove(buf + spaces, buf, len);
	}
	memset(buf, ' ', spaces);
}

 
static noinline_for_stack
char *widen_string(char *buf, int n, char *end, struct printf_spec spec)
{
	unsigned spaces;

	if (likely(n >= spec.field_width))
		return buf;
	 
	spaces = spec.field_width - n;
	if (!(spec.flags & LEFT)) {
		move_right(buf - n, end, n, spaces);
		return buf + spaces;
	}
	while (spaces--) {
		if (buf < end)
			*buf = ' ';
		++buf;
	}
	return buf;
}

 
static char *string_nocheck(char *buf, char *end, const char *s,
			    struct printf_spec spec)
{
	int len = 0;
	int lim = spec.precision;

	while (lim--) {
		char c = *s++;
		if (!c)
			break;
		if (buf < end)
			*buf = c;
		++buf;
		++len;
	}
	return widen_string(buf, len, end, spec);
}

static char *err_ptr(char *buf, char *end, void *ptr,
		     struct printf_spec spec)
{
	int err = PTR_ERR(ptr);
	const char *sym = errname(err);

	if (sym)
		return string_nocheck(buf, end, sym, spec);

	 
	spec.flags |= SIGN;
	spec.base = 10;
	return number(buf, end, err, spec);
}

 
static char *error_string(char *buf, char *end, const char *s,
			  struct printf_spec spec)
{
	 
	if (spec.precision == -1)
		spec.precision = 2 * sizeof(void *);

	return string_nocheck(buf, end, s, spec);
}

 
static const char *check_pointer_msg(const void *ptr)
{
	if (!ptr)
		return "(null)";

	if ((unsigned long)ptr < PAGE_SIZE || IS_ERR_VALUE(ptr))
		return "(efault)";

	return NULL;
}

static int check_pointer(char **buf, char *end, const void *ptr,
			 struct printf_spec spec)
{
	const char *err_msg;

	err_msg = check_pointer_msg(ptr);
	if (err_msg) {
		*buf = error_string(*buf, end, err_msg, spec);
		return -EFAULT;
	}

	return 0;
}

static noinline_for_stack
char *string(char *buf, char *end, const char *s,
	     struct printf_spec spec)
{
	if (check_pointer(&buf, end, s, spec))
		return buf;

	return string_nocheck(buf, end, s, spec);
}

static char *pointer_string(char *buf, char *end,
			    const void *ptr,
			    struct printf_spec spec)
{
	spec.base = 16;
	spec.flags |= SMALL;
	if (spec.field_width == -1) {
		spec.field_width = 2 * sizeof(ptr);
		spec.flags |= ZEROPAD;
	}

	return number(buf, end, (unsigned long int)ptr, spec);
}

 
static int debug_boot_weak_hash __ro_after_init;

static int __init debug_boot_weak_hash_enable(char *str)
{
	debug_boot_weak_hash = 1;
	pr_info("debug_boot_weak_hash enabled\n");
	return 0;
}
early_param("debug_boot_weak_hash", debug_boot_weak_hash_enable);

static bool filled_random_ptr_key __read_mostly;
static siphash_key_t ptr_key __read_mostly;

static int fill_ptr_key(struct notifier_block *nb, unsigned long action, void *data)
{
	get_random_bytes(&ptr_key, sizeof(ptr_key));

	 
	smp_wmb();
	WRITE_ONCE(filled_random_ptr_key, true);
	return NOTIFY_DONE;
}

static int __init vsprintf_init_hashval(void)
{
	static struct notifier_block fill_ptr_key_nb = { .notifier_call = fill_ptr_key };
	execute_with_initialized_rng(&fill_ptr_key_nb);
	return 0;
}
subsys_initcall(vsprintf_init_hashval)

 
static inline int __ptr_to_hashval(const void *ptr, unsigned long *hashval_out)
{
	unsigned long hashval;

	if (!READ_ONCE(filled_random_ptr_key))
		return -EBUSY;

	 
	smp_rmb();

#ifdef CONFIG_64BIT
	hashval = (unsigned long)siphash_1u64((u64)ptr, &ptr_key);
	 
	hashval = hashval & 0xffffffff;
#else
	hashval = (unsigned long)siphash_1u32((u32)ptr, &ptr_key);
#endif
	*hashval_out = hashval;
	return 0;
}

int ptr_to_hashval(const void *ptr, unsigned long *hashval_out)
{
	return __ptr_to_hashval(ptr, hashval_out);
}

static char *ptr_to_id(char *buf, char *end, const void *ptr,
		       struct printf_spec spec)
{
	const char *str = sizeof(ptr) == 8 ? "(____ptrval____)" : "(ptrval)";
	unsigned long hashval;
	int ret;

	 
	if (IS_ERR_OR_NULL(ptr))
		return pointer_string(buf, end, ptr, spec);

	 
	if (unlikely(debug_boot_weak_hash)) {
		hashval = hash_long((unsigned long)ptr, 32);
		return pointer_string(buf, end, (const void *)hashval, spec);
	}

	ret = __ptr_to_hashval(ptr, &hashval);
	if (ret) {
		spec.field_width = 2 * sizeof(ptr);
		 
		return error_string(buf, end, str, spec);
	}

	return pointer_string(buf, end, (const void *)hashval, spec);
}

static char *default_pointer(char *buf, char *end, const void *ptr,
			     struct printf_spec spec)
{
	 
	if (unlikely(no_hash_pointers))
		return pointer_string(buf, end, ptr, spec);

	return ptr_to_id(buf, end, ptr, spec);
}

int kptr_restrict __read_mostly;

static noinline_for_stack
char *restricted_pointer(char *buf, char *end, const void *ptr,
			 struct printf_spec spec)
{
	switch (kptr_restrict) {
	case 0:
		 
		return default_pointer(buf, end, ptr, spec);
	case 1: {
		const struct cred *cred;

		 
		if (in_hardirq() || in_serving_softirq() || in_nmi()) {
			if (spec.field_width == -1)
				spec.field_width = 2 * sizeof(ptr);
			return error_string(buf, end, "pK-error", spec);
		}

		 
		cred = current_cred();
		if (!has_capability_noaudit(current, CAP_SYSLOG) ||
		    !uid_eq(cred->euid, cred->uid) ||
		    !gid_eq(cred->egid, cred->gid))
			ptr = NULL;
		break;
	}
	case 2:
	default:
		 
		ptr = NULL;
		break;
	}

	return pointer_string(buf, end, ptr, spec);
}

static noinline_for_stack
char *dentry_name(char *buf, char *end, const struct dentry *d, struct printf_spec spec,
		  const char *fmt)
{
	const char *array[4], *s;
	const struct dentry *p;
	int depth;
	int i, n;

	switch (fmt[1]) {
		case '2': case '3': case '4':
			depth = fmt[1] - '0';
			break;
		default:
			depth = 1;
	}

	rcu_read_lock();
	for (i = 0; i < depth; i++, d = p) {
		if (check_pointer(&buf, end, d, spec)) {
			rcu_read_unlock();
			return buf;
		}

		p = READ_ONCE(d->d_parent);
		array[i] = READ_ONCE(d->d_name.name);
		if (p == d) {
			if (i)
				array[i] = "";
			i++;
			break;
		}
	}
	s = array[--i];
	for (n = 0; n != spec.precision; n++, buf++) {
		char c = *s++;
		if (!c) {
			if (!i)
				break;
			c = '/';
			s = array[--i];
		}
		if (buf < end)
			*buf = c;
	}
	rcu_read_unlock();
	return widen_string(buf, n, end, spec);
}

static noinline_for_stack
char *file_dentry_name(char *buf, char *end, const struct file *f,
			struct printf_spec spec, const char *fmt)
{
	if (check_pointer(&buf, end, f, spec))
		return buf;

	return dentry_name(buf, end, f->f_path.dentry, spec, fmt);
}
#ifdef CONFIG_BLOCK
static noinline_for_stack
char *bdev_name(char *buf, char *end, struct block_device *bdev,
		struct printf_spec spec, const char *fmt)
{
	struct gendisk *hd;

	if (check_pointer(&buf, end, bdev, spec))
		return buf;

	hd = bdev->bd_disk;
	buf = string(buf, end, hd->disk_name, spec);
	if (bdev->bd_partno) {
		if (isdigit(hd->disk_name[strlen(hd->disk_name)-1])) {
			if (buf < end)
				*buf = 'p';
			buf++;
		}
		buf = number(buf, end, bdev->bd_partno, spec);
	}
	return buf;
}
#endif

static noinline_for_stack
char *symbol_string(char *buf, char *end, void *ptr,
		    struct printf_spec spec, const char *fmt)
{
	unsigned long value;
#ifdef CONFIG_KALLSYMS
	char sym[KSYM_SYMBOL_LEN];
#endif

	if (fmt[1] == 'R')
		ptr = __builtin_extract_return_addr(ptr);
	value = (unsigned long)ptr;

#ifdef CONFIG_KALLSYMS
	if (*fmt == 'B' && fmt[1] == 'b')
		sprint_backtrace_build_id(sym, value);
	else if (*fmt == 'B')
		sprint_backtrace(sym, value);
	else if (*fmt == 'S' && (fmt[1] == 'b' || (fmt[1] == 'R' && fmt[2] == 'b')))
		sprint_symbol_build_id(sym, value);
	else if (*fmt != 's')
		sprint_symbol(sym, value);
	else
		sprint_symbol_no_offset(sym, value);

	return string_nocheck(buf, end, sym, spec);
#else
	return special_hex_number(buf, end, value, sizeof(void *));
#endif
}

static const struct printf_spec default_str_spec = {
	.field_width = -1,
	.precision = -1,
};

static const struct printf_spec default_flag_spec = {
	.base = 16,
	.precision = -1,
	.flags = SPECIAL | SMALL,
};

static const struct printf_spec default_dec_spec = {
	.base = 10,
	.precision = -1,
};

static const struct printf_spec default_dec02_spec = {
	.base = 10,
	.field_width = 2,
	.precision = -1,
	.flags = ZEROPAD,
};

static const struct printf_spec default_dec04_spec = {
	.base = 10,
	.field_width = 4,
	.precision = -1,
	.flags = ZEROPAD,
};

static noinline_for_stack
char *resource_string(char *buf, char *end, struct resource *res,
		      struct printf_spec spec, const char *fmt)
{
#ifndef IO_RSRC_PRINTK_SIZE
#define IO_RSRC_PRINTK_SIZE	6
#endif

#ifndef MEM_RSRC_PRINTK_SIZE
#define MEM_RSRC_PRINTK_SIZE	10
#endif
	static const struct printf_spec io_spec = {
		.base = 16,
		.field_width = IO_RSRC_PRINTK_SIZE,
		.precision = -1,
		.flags = SPECIAL | SMALL | ZEROPAD,
	};
	static const struct printf_spec mem_spec = {
		.base = 16,
		.field_width = MEM_RSRC_PRINTK_SIZE,
		.precision = -1,
		.flags = SPECIAL | SMALL | ZEROPAD,
	};
	static const struct printf_spec bus_spec = {
		.base = 16,
		.field_width = 2,
		.precision = -1,
		.flags = SMALL | ZEROPAD,
	};
	static const struct printf_spec str_spec = {
		.field_width = -1,
		.precision = 10,
		.flags = LEFT,
	};

	 
#define RSRC_BUF_SIZE		((2 * sizeof(resource_size_t)) + 4)
#define FLAG_BUF_SIZE		(2 * sizeof(res->flags))
#define DECODED_BUF_SIZE	sizeof("[mem - 64bit pref window disabled]")
#define RAW_BUF_SIZE		sizeof("[mem - flags 0x]")
	char sym[max(2*RSRC_BUF_SIZE + DECODED_BUF_SIZE,
		     2*RSRC_BUF_SIZE + FLAG_BUF_SIZE + RAW_BUF_SIZE)];

	char *p = sym, *pend = sym + sizeof(sym);
	int decode = (fmt[0] == 'R') ? 1 : 0;
	const struct printf_spec *specp;

	if (check_pointer(&buf, end, res, spec))
		return buf;

	*p++ = '[';
	if (res->flags & IORESOURCE_IO) {
		p = string_nocheck(p, pend, "io  ", str_spec);
		specp = &io_spec;
	} else if (res->flags & IORESOURCE_MEM) {
		p = string_nocheck(p, pend, "mem ", str_spec);
		specp = &mem_spec;
	} else if (res->flags & IORESOURCE_IRQ) {
		p = string_nocheck(p, pend, "irq ", str_spec);
		specp = &default_dec_spec;
	} else if (res->flags & IORESOURCE_DMA) {
		p = string_nocheck(p, pend, "dma ", str_spec);
		specp = &default_dec_spec;
	} else if (res->flags & IORESOURCE_BUS) {
		p = string_nocheck(p, pend, "bus ", str_spec);
		specp = &bus_spec;
	} else {
		p = string_nocheck(p, pend, "??? ", str_spec);
		specp = &mem_spec;
		decode = 0;
	}
	if (decode && res->flags & IORESOURCE_UNSET) {
		p = string_nocheck(p, pend, "size ", str_spec);
		p = number(p, pend, resource_size(res), *specp);
	} else {
		p = number(p, pend, res->start, *specp);
		if (res->start != res->end) {
			*p++ = '-';
			p = number(p, pend, res->end, *specp);
		}
	}
	if (decode) {
		if (res->flags & IORESOURCE_MEM_64)
			p = string_nocheck(p, pend, " 64bit", str_spec);
		if (res->flags & IORESOURCE_PREFETCH)
			p = string_nocheck(p, pend, " pref", str_spec);
		if (res->flags & IORESOURCE_WINDOW)
			p = string_nocheck(p, pend, " window", str_spec);
		if (res->flags & IORESOURCE_DISABLED)
			p = string_nocheck(p, pend, " disabled", str_spec);
	} else {
		p = string_nocheck(p, pend, " flags ", str_spec);
		p = number(p, pend, res->flags, default_flag_spec);
	}
	*p++ = ']';
	*p = '\0';

	return string_nocheck(buf, end, sym, spec);
}

static noinline_for_stack
char *hex_string(char *buf, char *end, u8 *addr, struct printf_spec spec,
		 const char *fmt)
{
	int i, len = 1;		 
	char separator;

	if (spec.field_width == 0)
		 
		return buf;

	if (check_pointer(&buf, end, addr, spec))
		return buf;

	switch (fmt[1]) {
	case 'C':
		separator = ':';
		break;
	case 'D':
		separator = '-';
		break;
	case 'N':
		separator = 0;
		break;
	default:
		separator = ' ';
		break;
	}

	if (spec.field_width > 0)
		len = min_t(int, spec.field_width, 64);

	for (i = 0; i < len; ++i) {
		if (buf < end)
			*buf = hex_asc_hi(addr[i]);
		++buf;
		if (buf < end)
			*buf = hex_asc_lo(addr[i]);
		++buf;

		if (separator && i != len - 1) {
			if (buf < end)
				*buf = separator;
			++buf;
		}
	}

	return buf;
}

static noinline_for_stack
char *bitmap_string(char *buf, char *end, const unsigned long *bitmap,
		    struct printf_spec spec, const char *fmt)
{
	const int CHUNKSZ = 32;
	int nr_bits = max_t(int, spec.field_width, 0);
	int i, chunksz;
	bool first = true;

	if (check_pointer(&buf, end, bitmap, spec))
		return buf;

	 
	spec = (struct printf_spec){ .flags = SMALL | ZEROPAD, .base = 16 };

	chunksz = nr_bits & (CHUNKSZ - 1);
	if (chunksz == 0)
		chunksz = CHUNKSZ;

	i = ALIGN(nr_bits, CHUNKSZ) - CHUNKSZ;
	for (; i >= 0; i -= CHUNKSZ) {
		u32 chunkmask, val;
		int word, bit;

		chunkmask = ((1ULL << chunksz) - 1);
		word = i / BITS_PER_LONG;
		bit = i % BITS_PER_LONG;
		val = (bitmap[word] >> bit) & chunkmask;

		if (!first) {
			if (buf < end)
				*buf = ',';
			buf++;
		}
		first = false;

		spec.field_width = DIV_ROUND_UP(chunksz, 4);
		buf = number(buf, end, val, spec);

		chunksz = CHUNKSZ;
	}
	return buf;
}

static noinline_for_stack
char *bitmap_list_string(char *buf, char *end, const unsigned long *bitmap,
			 struct printf_spec spec, const char *fmt)
{
	int nr_bits = max_t(int, spec.field_width, 0);
	bool first = true;
	int rbot, rtop;

	if (check_pointer(&buf, end, bitmap, spec))
		return buf;

	for_each_set_bitrange(rbot, rtop, bitmap, nr_bits) {
		if (!first) {
			if (buf < end)
				*buf = ',';
			buf++;
		}
		first = false;

		buf = number(buf, end, rbot, default_dec_spec);
		if (rtop == rbot + 1)
			continue;

		if (buf < end)
			*buf = '-';
		buf = number(++buf, end, rtop - 1, default_dec_spec);
	}
	return buf;
}

static noinline_for_stack
char *mac_address_string(char *buf, char *end, u8 *addr,
			 struct printf_spec spec, const char *fmt)
{
	char mac_addr[sizeof("xx:xx:xx:xx:xx:xx")];
	char *p = mac_addr;
	int i;
	char separator;
	bool reversed = false;

	if (check_pointer(&buf, end, addr, spec))
		return buf;

	switch (fmt[1]) {
	case 'F':
		separator = '-';
		break;

	case 'R':
		reversed = true;
		fallthrough;

	default:
		separator = ':';
		break;
	}

	for (i = 0; i < 6; i++) {
		if (reversed)
			p = hex_byte_pack(p, addr[5 - i]);
		else
			p = hex_byte_pack(p, addr[i]);

		if (fmt[0] == 'M' && i != 5)
			*p++ = separator;
	}
	*p = '\0';

	return string_nocheck(buf, end, mac_addr, spec);
}

static noinline_for_stack
char *ip4_string(char *p, const u8 *addr, const char *fmt)
{
	int i;
	bool leading_zeros = (fmt[0] == 'i');
	int index;
	int step;

	switch (fmt[2]) {
	case 'h':
#ifdef __BIG_ENDIAN
		index = 0;
		step = 1;
#else
		index = 3;
		step = -1;
#endif
		break;
	case 'l':
		index = 3;
		step = -1;
		break;
	case 'n':
	case 'b':
	default:
		index = 0;
		step = 1;
		break;
	}
	for (i = 0; i < 4; i++) {
		char temp[4] __aligned(2);	 
		int digits = put_dec_trunc8(temp, addr[index]) - temp;
		if (leading_zeros) {
			if (digits < 3)
				*p++ = '0';
			if (digits < 2)
				*p++ = '0';
		}
		 
		while (digits--)
			*p++ = temp[digits];
		if (i < 3)
			*p++ = '.';
		index += step;
	}
	*p = '\0';

	return p;
}

static noinline_for_stack
char *ip6_compressed_string(char *p, const char *addr)
{
	int i, j, range;
	unsigned char zerolength[8];
	int longest = 1;
	int colonpos = -1;
	u16 word;
	u8 hi, lo;
	bool needcolon = false;
	bool useIPv4;
	struct in6_addr in6;

	memcpy(&in6, addr, sizeof(struct in6_addr));

	useIPv4 = ipv6_addr_v4mapped(&in6) || ipv6_addr_is_isatap(&in6);

	memset(zerolength, 0, sizeof(zerolength));

	if (useIPv4)
		range = 6;
	else
		range = 8;

	 
	for (i = 0; i < range; i++) {
		for (j = i; j < range; j++) {
			if (in6.s6_addr16[j] != 0)
				break;
			zerolength[i]++;
		}
	}
	for (i = 0; i < range; i++) {
		if (zerolength[i] > longest) {
			longest = zerolength[i];
			colonpos = i;
		}
	}
	if (longest == 1)		 
		colonpos = -1;

	 
	for (i = 0; i < range; i++) {
		if (i == colonpos) {
			if (needcolon || i == 0)
				*p++ = ':';
			*p++ = ':';
			needcolon = false;
			i += longest - 1;
			continue;
		}
		if (needcolon) {
			*p++ = ':';
			needcolon = false;
		}
		 
		word = ntohs(in6.s6_addr16[i]);
		hi = word >> 8;
		lo = word & 0xff;
		if (hi) {
			if (hi > 0x0f)
				p = hex_byte_pack(p, hi);
			else
				*p++ = hex_asc_lo(hi);
			p = hex_byte_pack(p, lo);
		}
		else if (lo > 0x0f)
			p = hex_byte_pack(p, lo);
		else
			*p++ = hex_asc_lo(lo);
		needcolon = true;
	}

	if (useIPv4) {
		if (needcolon)
			*p++ = ':';
		p = ip4_string(p, &in6.s6_addr[12], "I4");
	}
	*p = '\0';

	return p;
}

static noinline_for_stack
char *ip6_string(char *p, const char *addr, const char *fmt)
{
	int i;

	for (i = 0; i < 8; i++) {
		p = hex_byte_pack(p, *addr++);
		p = hex_byte_pack(p, *addr++);
		if (fmt[0] == 'I' && i != 7)
			*p++ = ':';
	}
	*p = '\0';

	return p;
}

static noinline_for_stack
char *ip6_addr_string(char *buf, char *end, const u8 *addr,
		      struct printf_spec spec, const char *fmt)
{
	char ip6_addr[sizeof("xxxx:xxxx:xxxx:xxxx:xxxx:xxxx:255.255.255.255")];

	if (fmt[0] == 'I' && fmt[2] == 'c')
		ip6_compressed_string(ip6_addr, addr);
	else
		ip6_string(ip6_addr, addr, fmt);

	return string_nocheck(buf, end, ip6_addr, spec);
}

static noinline_for_stack
char *ip4_addr_string(char *buf, char *end, const u8 *addr,
		      struct printf_spec spec, const char *fmt)
{
	char ip4_addr[sizeof("255.255.255.255")];

	ip4_string(ip4_addr, addr, fmt);

	return string_nocheck(buf, end, ip4_addr, spec);
}

static noinline_for_stack
char *ip6_addr_string_sa(char *buf, char *end, const struct sockaddr_in6 *sa,
			 struct printf_spec spec, const char *fmt)
{
	bool have_p = false, have_s = false, have_f = false, have_c = false;
	char ip6_addr[sizeof("[xxxx:xxxx:xxxx:xxxx:xxxx:xxxx:255.255.255.255]") +
		      sizeof(":12345") + sizeof("/123456789") +
		      sizeof("%1234567890")];
	char *p = ip6_addr, *pend = ip6_addr + sizeof(ip6_addr);
	const u8 *addr = (const u8 *) &sa->sin6_addr;
	char fmt6[2] = { fmt[0], '6' };
	u8 off = 0;

	fmt++;
	while (isalpha(*++fmt)) {
		switch (*fmt) {
		case 'p':
			have_p = true;
			break;
		case 'f':
			have_f = true;
			break;
		case 's':
			have_s = true;
			break;
		case 'c':
			have_c = true;
			break;
		}
	}

	if (have_p || have_s || have_f) {
		*p = '[';
		off = 1;
	}

	if (fmt6[0] == 'I' && have_c)
		p = ip6_compressed_string(ip6_addr + off, addr);
	else
		p = ip6_string(ip6_addr + off, addr, fmt6);

	if (have_p || have_s || have_f)
		*p++ = ']';

	if (have_p) {
		*p++ = ':';
		p = number(p, pend, ntohs(sa->sin6_port), spec);
	}
	if (have_f) {
		*p++ = '/';
		p = number(p, pend, ntohl(sa->sin6_flowinfo &
					  IPV6_FLOWINFO_MASK), spec);
	}
	if (have_s) {
		*p++ = '%';
		p = number(p, pend, sa->sin6_scope_id, spec);
	}
	*p = '\0';

	return string_nocheck(buf, end, ip6_addr, spec);
}

static noinline_for_stack
char *ip4_addr_string_sa(char *buf, char *end, const struct sockaddr_in *sa,
			 struct printf_spec spec, const char *fmt)
{
	bool have_p = false;
	char *p, ip4_addr[sizeof("255.255.255.255") + sizeof(":12345")];
	char *pend = ip4_addr + sizeof(ip4_addr);
	const u8 *addr = (const u8 *) &sa->sin_addr.s_addr;
	char fmt4[3] = { fmt[0], '4', 0 };

	fmt++;
	while (isalpha(*++fmt)) {
		switch (*fmt) {
		case 'p':
			have_p = true;
			break;
		case 'h':
		case 'l':
		case 'n':
		case 'b':
			fmt4[2] = *fmt;
			break;
		}
	}

	p = ip4_string(ip4_addr, addr, fmt4);
	if (have_p) {
		*p++ = ':';
		p = number(p, pend, ntohs(sa->sin_port), spec);
	}
	*p = '\0';

	return string_nocheck(buf, end, ip4_addr, spec);
}

static noinline_for_stack
char *ip_addr_string(char *buf, char *end, const void *ptr,
		     struct printf_spec spec, const char *fmt)
{
	char *err_fmt_msg;

	if (check_pointer(&buf, end, ptr, spec))
		return buf;

	switch (fmt[1]) {
	case '6':
		return ip6_addr_string(buf, end, ptr, spec, fmt);
	case '4':
		return ip4_addr_string(buf, end, ptr, spec, fmt);
	case 'S': {
		const union {
			struct sockaddr		raw;
			struct sockaddr_in	v4;
			struct sockaddr_in6	v6;
		} *sa = ptr;

		switch (sa->raw.sa_family) {
		case AF_INET:
			return ip4_addr_string_sa(buf, end, &sa->v4, spec, fmt);
		case AF_INET6:
			return ip6_addr_string_sa(buf, end, &sa->v6, spec, fmt);
		default:
			return error_string(buf, end, "(einval)", spec);
		}}
	}

	err_fmt_msg = fmt[0] == 'i' ? "(%pi?)" : "(%pI?)";
	return error_string(buf, end, err_fmt_msg, spec);
}

static noinline_for_stack
char *escaped_string(char *buf, char *end, u8 *addr, struct printf_spec spec,
		     const char *fmt)
{
	bool found = true;
	int count = 1;
	unsigned int flags = 0;
	int len;

	if (spec.field_width == 0)
		return buf;				 

	if (check_pointer(&buf, end, addr, spec))
		return buf;

	do {
		switch (fmt[count++]) {
		case 'a':
			flags |= ESCAPE_ANY;
			break;
		case 'c':
			flags |= ESCAPE_SPECIAL;
			break;
		case 'h':
			flags |= ESCAPE_HEX;
			break;
		case 'n':
			flags |= ESCAPE_NULL;
			break;
		case 'o':
			flags |= ESCAPE_OCTAL;
			break;
		case 'p':
			flags |= ESCAPE_NP;
			break;
		case 's':
			flags |= ESCAPE_SPACE;
			break;
		default:
			found = false;
			break;
		}
	} while (found);

	if (!flags)
		flags = ESCAPE_ANY_NP;

	len = spec.field_width < 0 ? 1 : spec.field_width;

	 
	buf += string_escape_mem(addr, len, buf, buf < end ? end - buf : 0, flags, NULL);

	return buf;
}

static char *va_format(char *buf, char *end, struct va_format *va_fmt,
		       struct printf_spec spec, const char *fmt)
{
	va_list va;

	if (check_pointer(&buf, end, va_fmt, spec))
		return buf;

	va_copy(va, *va_fmt->va);
	buf += vsnprintf(buf, end > buf ? end - buf : 0, va_fmt->fmt, va);
	va_end(va);

	return buf;
}

static noinline_for_stack
char *uuid_string(char *buf, char *end, const u8 *addr,
		  struct printf_spec spec, const char *fmt)
{
	char uuid[UUID_STRING_LEN + 1];
	char *p = uuid;
	int i;
	const u8 *index = uuid_index;
	bool uc = false;

	if (check_pointer(&buf, end, addr, spec))
		return buf;

	switch (*(++fmt)) {
	case 'L':
		uc = true;
		fallthrough;
	case 'l':
		index = guid_index;
		break;
	case 'B':
		uc = true;
		break;
	}

	for (i = 0; i < 16; i++) {
		if (uc)
			p = hex_byte_pack_upper(p, addr[index[i]]);
		else
			p = hex_byte_pack(p, addr[index[i]]);
		switch (i) {
		case 3:
		case 5:
		case 7:
		case 9:
			*p++ = '-';
			break;
		}
	}

	*p = 0;

	return string_nocheck(buf, end, uuid, spec);
}

static noinline_for_stack
char *netdev_bits(char *buf, char *end, const void *addr,
		  struct printf_spec spec,  const char *fmt)
{
	unsigned long long num;
	int size;

	if (check_pointer(&buf, end, addr, spec))
		return buf;

	switch (fmt[1]) {
	case 'F':
		num = *(const netdev_features_t *)addr;
		size = sizeof(netdev_features_t);
		break;
	default:
		return error_string(buf, end, "(%pN?)", spec);
	}

	return special_hex_number(buf, end, num, size);
}

static noinline_for_stack
char *fourcc_string(char *buf, char *end, const u32 *fourcc,
		    struct printf_spec spec, const char *fmt)
{
	char output[sizeof("0123 little-endian (0x01234567)")];
	char *p = output;
	unsigned int i;
	u32 orig, val;

	if (fmt[1] != 'c' || fmt[2] != 'c')
		return error_string(buf, end, "(%p4?)", spec);

	if (check_pointer(&buf, end, fourcc, spec))
		return buf;

	orig = get_unaligned(fourcc);
	val = orig & ~BIT(31);

	for (i = 0; i < sizeof(u32); i++) {
		unsigned char c = val >> (i * 8);

		 
		*p++ = isascii(c) && isprint(c) ? c : '.';
	}

	*p++ = ' ';
	strcpy(p, orig & BIT(31) ? "big-endian" : "little-endian");
	p += strlen(p);

	*p++ = ' ';
	*p++ = '(';
	p = special_hex_number(p, output + sizeof(output) - 2, orig, sizeof(u32));
	*p++ = ')';
	*p = '\0';

	return string(buf, end, output, spec);
}

static noinline_for_stack
char *address_val(char *buf, char *end, const void *addr,
		  struct printf_spec spec, const char *fmt)
{
	unsigned long long num;
	int size;

	if (check_pointer(&buf, end, addr, spec))
		return buf;

	switch (fmt[1]) {
	case 'd':
		num = *(const dma_addr_t *)addr;
		size = sizeof(dma_addr_t);
		break;
	case 'p':
	default:
		num = *(const phys_addr_t *)addr;
		size = sizeof(phys_addr_t);
		break;
	}

	return special_hex_number(buf, end, num, size);
}

static noinline_for_stack
char *date_str(char *buf, char *end, const struct rtc_time *tm, bool r)
{
	int year = tm->tm_year + (r ? 0 : 1900);
	int mon = tm->tm_mon + (r ? 0 : 1);

	buf = number(buf, end, year, default_dec04_spec);
	if (buf < end)
		*buf = '-';
	buf++;

	buf = number(buf, end, mon, default_dec02_spec);
	if (buf < end)
		*buf = '-';
	buf++;

	return number(buf, end, tm->tm_mday, default_dec02_spec);
}

static noinline_for_stack
char *time_str(char *buf, char *end, const struct rtc_time *tm, bool r)
{
	buf = number(buf, end, tm->tm_hour, default_dec02_spec);
	if (buf < end)
		*buf = ':';
	buf++;

	buf = number(buf, end, tm->tm_min, default_dec02_spec);
	if (buf < end)
		*buf = ':';
	buf++;

	return number(buf, end, tm->tm_sec, default_dec02_spec);
}

static noinline_for_stack
char *rtc_str(char *buf, char *end, const struct rtc_time *tm,
	      struct printf_spec spec, const char *fmt)
{
	bool have_t = true, have_d = true;
	bool raw = false, iso8601_separator = true;
	bool found = true;
	int count = 2;

	if (check_pointer(&buf, end, tm, spec))
		return buf;

	switch (fmt[count]) {
	case 'd':
		have_t = false;
		count++;
		break;
	case 't':
		have_d = false;
		count++;
		break;
	}

	do {
		switch (fmt[count++]) {
		case 'r':
			raw = true;
			break;
		case 's':
			iso8601_separator = false;
			break;
		default:
			found = false;
			break;
		}
	} while (found);

	if (have_d)
		buf = date_str(buf, end, tm, raw);
	if (have_d && have_t) {
		if (buf < end)
			*buf = iso8601_separator ? 'T' : ' ';
		buf++;
	}
	if (have_t)
		buf = time_str(buf, end, tm, raw);

	return buf;
}

static noinline_for_stack
char *time64_str(char *buf, char *end, const time64_t time,
		 struct printf_spec spec, const char *fmt)
{
	struct rtc_time rtc_time;
	struct tm tm;

	time64_to_tm(time, 0, &tm);

	rtc_time.tm_sec = tm.tm_sec;
	rtc_time.tm_min = tm.tm_min;
	rtc_time.tm_hour = tm.tm_hour;
	rtc_time.tm_mday = tm.tm_mday;
	rtc_time.tm_mon = tm.tm_mon;
	rtc_time.tm_year = tm.tm_year;
	rtc_time.tm_wday = tm.tm_wday;
	rtc_time.tm_yday = tm.tm_yday;

	rtc_time.tm_isdst = 0;

	return rtc_str(buf, end, &rtc_time, spec, fmt);
}

static noinline_for_stack
char *time_and_date(char *buf, char *end, void *ptr, struct printf_spec spec,
		    const char *fmt)
{
	switch (fmt[1]) {
	case 'R':
		return rtc_str(buf, end, (const struct rtc_time *)ptr, spec, fmt);
	case 'T':
		return time64_str(buf, end, *(const time64_t *)ptr, spec, fmt);
	default:
		return error_string(buf, end, "(%pt?)", spec);
	}
}

static noinline_for_stack
char *clock(char *buf, char *end, struct clk *clk, struct printf_spec spec,
	    const char *fmt)
{
	if (!IS_ENABLED(CONFIG_HAVE_CLK))
		return error_string(buf, end, "(%pC?)", spec);

	if (check_pointer(&buf, end, clk, spec))
		return buf;

	switch (fmt[1]) {
	case 'n':
	default:
#ifdef CONFIG_COMMON_CLK
		return string(buf, end, __clk_get_name(clk), spec);
#else
		return ptr_to_id(buf, end, clk, spec);
#endif
	}
}

static
char *format_flags(char *buf, char *end, unsigned long flags,
					const struct trace_print_flags *names)
{
	unsigned long mask;

	for ( ; flags && names->name; names++) {
		mask = names->mask;
		if ((flags & mask) != mask)
			continue;

		buf = string(buf, end, names->name, default_str_spec);

		flags &= ~mask;
		if (flags) {
			if (buf < end)
				*buf = '|';
			buf++;
		}
	}

	if (flags)
		buf = number(buf, end, flags, default_flag_spec);

	return buf;
}

struct page_flags_fields {
	int width;
	int shift;
	int mask;
	const struct printf_spec *spec;
	const char *name;
};

static const struct page_flags_fields pff[] = {
	{SECTIONS_WIDTH, SECTIONS_PGSHIFT, SECTIONS_MASK,
	 &default_dec_spec, "section"},
	{NODES_WIDTH, NODES_PGSHIFT, NODES_MASK,
	 &default_dec_spec, "node"},
	{ZONES_WIDTH, ZONES_PGSHIFT, ZONES_MASK,
	 &default_dec_spec, "zone"},
	{LAST_CPUPID_WIDTH, LAST_CPUPID_PGSHIFT, LAST_CPUPID_MASK,
	 &default_flag_spec, "lastcpupid"},
	{KASAN_TAG_WIDTH, KASAN_TAG_PGSHIFT, KASAN_TAG_MASK,
	 &default_flag_spec, "kasantag"},
};

static
char *format_page_flags(char *buf, char *end, unsigned long flags)
{
	unsigned long main_flags = flags & PAGEFLAGS_MASK;
	bool append = false;
	int i;

	buf = number(buf, end, flags, default_flag_spec);
	if (buf < end)
		*buf = '(';
	buf++;

	 
	if (main_flags) {
		buf = format_flags(buf, end, main_flags, pageflag_names);
		append = true;
	}

	 
	for (i = 0; i < ARRAY_SIZE(pff); i++) {
		 
		if (!pff[i].width)
			continue;

		 
		if (append) {
			if (buf < end)
				*buf = '|';
			buf++;
		}

		buf = string(buf, end, pff[i].name, default_str_spec);
		if (buf < end)
			*buf = '=';
		buf++;
		buf = number(buf, end, (flags >> pff[i].shift) & pff[i].mask,
			     *pff[i].spec);

		append = true;
	}
	if (buf < end)
		*buf = ')';
	buf++;

	return buf;
}

static
char *format_page_type(char *buf, char *end, unsigned int page_type)
{
	buf = number(buf, end, page_type, default_flag_spec);

	if (buf < end)
		*buf = '(';
	buf++;

	if (page_type_has_type(page_type))
		buf = format_flags(buf, end, ~page_type, pagetype_names);

	if (buf < end)
		*buf = ')';
	buf++;

	return buf;
}

static noinline_for_stack
char *flags_string(char *buf, char *end, void *flags_ptr,
		   struct printf_spec spec, const char *fmt)
{
	unsigned long flags;
	const struct trace_print_flags *names;

	if (check_pointer(&buf, end, flags_ptr, spec))
		return buf;

	switch (fmt[1]) {
	case 'p':
		return format_page_flags(buf, end, *(unsigned long *)flags_ptr);
	case 't':
		return format_page_type(buf, end, *(unsigned int *)flags_ptr);
	case 'v':
		flags = *(unsigned long *)flags_ptr;
		names = vmaflag_names;
		break;
	case 'g':
		flags = (__force unsigned long)(*(gfp_t *)flags_ptr);
		names = gfpflag_names;
		break;
	default:
		return error_string(buf, end, "(%pG?)", spec);
	}

	return format_flags(buf, end, flags, names);
}

static noinline_for_stack
char *fwnode_full_name_string(struct fwnode_handle *fwnode, char *buf,
			      char *end)
{
	int depth;

	 
	for (depth = fwnode_count_parents(fwnode); depth >= 0; depth--) {
		 
		struct fwnode_handle *__fwnode = depth ?
			fwnode_get_nth_parent(fwnode, depth) : fwnode;

		buf = string(buf, end, fwnode_get_name_prefix(__fwnode),
			     default_str_spec);
		buf = string(buf, end, fwnode_get_name(__fwnode),
			     default_str_spec);

		if (depth)
			fwnode_handle_put(__fwnode);
	}

	return buf;
}

static noinline_for_stack
char *device_node_string(char *buf, char *end, struct device_node *dn,
			 struct printf_spec spec, const char *fmt)
{
	char tbuf[sizeof("xxxx") + 1];
	const char *p;
	int ret;
	char *buf_start = buf;
	struct property *prop;
	bool has_mult, pass;

	struct printf_spec str_spec = spec;
	str_spec.field_width = -1;

	if (fmt[0] != 'F')
		return error_string(buf, end, "(%pO?)", spec);

	if (!IS_ENABLED(CONFIG_OF))
		return error_string(buf, end, "(%pOF?)", spec);

	if (check_pointer(&buf, end, dn, spec))
		return buf;

	 
	fmt++;
	if (fmt[0] == '\0' || strcspn(fmt,"fnpPFcC") > 0)
		fmt = "f";

	for (pass = false; strspn(fmt,"fnpPFcC"); fmt++, pass = true) {
		int precision;
		if (pass) {
			if (buf < end)
				*buf = ':';
			buf++;
		}

		switch (*fmt) {
		case 'f':	 
			buf = fwnode_full_name_string(of_fwnode_handle(dn), buf,
						      end);
			break;
		case 'n':	 
			p = fwnode_get_name(of_fwnode_handle(dn));
			precision = str_spec.precision;
			str_spec.precision = strchrnul(p, '@') - p;
			buf = string(buf, end, p, str_spec);
			str_spec.precision = precision;
			break;
		case 'p':	 
			buf = number(buf, end, (unsigned int)dn->phandle, default_dec_spec);
			break;
		case 'P':	 
			p = fwnode_get_name(of_fwnode_handle(dn));
			if (!p[1])
				p = "/";
			buf = string(buf, end, p, str_spec);
			break;
		case 'F':	 
			tbuf[0] = of_node_check_flag(dn, OF_DYNAMIC) ? 'D' : '-';
			tbuf[1] = of_node_check_flag(dn, OF_DETACHED) ? 'd' : '-';
			tbuf[2] = of_node_check_flag(dn, OF_POPULATED) ? 'P' : '-';
			tbuf[3] = of_node_check_flag(dn, OF_POPULATED_BUS) ? 'B' : '-';
			tbuf[4] = 0;
			buf = string_nocheck(buf, end, tbuf, str_spec);
			break;
		case 'c':	 
			ret = of_property_read_string(dn, "compatible", &p);
			if (!ret)
				buf = string(buf, end, p, str_spec);
			break;
		case 'C':	 
			has_mult = false;
			of_property_for_each_string(dn, "compatible", prop, p) {
				if (has_mult)
					buf = string_nocheck(buf, end, ",", str_spec);
				buf = string_nocheck(buf, end, "\"", str_spec);
				buf = string(buf, end, p, str_spec);
				buf = string_nocheck(buf, end, "\"", str_spec);

				has_mult = true;
			}
			break;
		default:
			break;
		}
	}

	return widen_string(buf, buf - buf_start, end, spec);
}

static noinline_for_stack
char *fwnode_string(char *buf, char *end, struct fwnode_handle *fwnode,
		    struct printf_spec spec, const char *fmt)
{
	struct printf_spec str_spec = spec;
	char *buf_start = buf;

	str_spec.field_width = -1;

	if (*fmt != 'w')
		return error_string(buf, end, "(%pf?)", spec);

	if (check_pointer(&buf, end, fwnode, spec))
		return buf;

	fmt++;

	switch (*fmt) {
	case 'P':	 
		buf = string(buf, end, fwnode_get_name(fwnode), str_spec);
		break;
	case 'f':	 
	default:
		buf = fwnode_full_name_string(fwnode, buf, end);
		break;
	}

	return widen_string(buf, buf - buf_start, end, spec);
}

int __init no_hash_pointers_enable(char *str)
{
	if (no_hash_pointers)
		return 0;

	no_hash_pointers = true;

	pr_warn("**********************************************************\n");
	pr_warn("**   NOTICE NOTICE NOTICE NOTICE NOTICE NOTICE NOTICE   **\n");
	pr_warn("**                                                      **\n");
	pr_warn("** This system shows unhashed kernel memory addresses   **\n");
	pr_warn("** via the console, logs, and other interfaces. This    **\n");
	pr_warn("** might reduce the security of your system.            **\n");
	pr_warn("**                                                      **\n");
	pr_warn("** If you see this message and you are not debugging    **\n");
	pr_warn("** the kernel, report this immediately to your system   **\n");
	pr_warn("** administrator!                                       **\n");
	pr_warn("**                                                      **\n");
	pr_warn("**   NOTICE NOTICE NOTICE NOTICE NOTICE NOTICE NOTICE   **\n");
	pr_warn("**********************************************************\n");

	return 0;
}
early_param("no_hash_pointers", no_hash_pointers_enable);

 
char *rust_fmt_argument(char *buf, char *end, void *ptr);

 
static noinline_for_stack
char *pointer(const char *fmt, char *buf, char *end, void *ptr,
	      struct printf_spec spec)
{
	switch (*fmt) {
	case 'S':
	case 's':
		ptr = dereference_symbol_descriptor(ptr);
		fallthrough;
	case 'B':
		return symbol_string(buf, end, ptr, spec, fmt);
	case 'R':
	case 'r':
		return resource_string(buf, end, ptr, spec, fmt);
	case 'h':
		return hex_string(buf, end, ptr, spec, fmt);
	case 'b':
		switch (fmt[1]) {
		case 'l':
			return bitmap_list_string(buf, end, ptr, spec, fmt);
		default:
			return bitmap_string(buf, end, ptr, spec, fmt);
		}
	case 'M':			 
	case 'm':			 
					 
					 
		return mac_address_string(buf, end, ptr, spec, fmt);
	case 'I':			 
	case 'i':			 
		return ip_addr_string(buf, end, ptr, spec, fmt);
	case 'E':
		return escaped_string(buf, end, ptr, spec, fmt);
	case 'U':
		return uuid_string(buf, end, ptr, spec, fmt);
	case 'V':
		return va_format(buf, end, ptr, spec, fmt);
	case 'K':
		return restricted_pointer(buf, end, ptr, spec);
	case 'N':
		return netdev_bits(buf, end, ptr, spec, fmt);
	case '4':
		return fourcc_string(buf, end, ptr, spec, fmt);
	case 'a':
		return address_val(buf, end, ptr, spec, fmt);
	case 'd':
		return dentry_name(buf, end, ptr, spec, fmt);
	case 't':
		return time_and_date(buf, end, ptr, spec, fmt);
	case 'C':
		return clock(buf, end, ptr, spec, fmt);
	case 'D':
		return file_dentry_name(buf, end, ptr, spec, fmt);
#ifdef CONFIG_BLOCK
	case 'g':
		return bdev_name(buf, end, ptr, spec, fmt);
#endif

	case 'G':
		return flags_string(buf, end, ptr, spec, fmt);
	case 'O':
		return device_node_string(buf, end, ptr, spec, fmt + 1);
	case 'f':
		return fwnode_string(buf, end, ptr, spec, fmt + 1);
	case 'A':
		if (!IS_ENABLED(CONFIG_RUST)) {
			WARN_ONCE(1, "Please remove %%pA from non-Rust code\n");
			return error_string(buf, end, "(%pA?)", spec);
		}
		return rust_fmt_argument(buf, end, ptr);
	case 'x':
		return pointer_string(buf, end, ptr, spec);
	case 'e':
		 
		if (!IS_ERR(ptr))
			return default_pointer(buf, end, ptr, spec);
		return err_ptr(buf, end, ptr, spec);
	case 'u':
	case 'k':
		switch (fmt[1]) {
		case 's':
			return string(buf, end, ptr, spec);
		default:
			return error_string(buf, end, "(einval)", spec);
		}
	default:
		return default_pointer(buf, end, ptr, spec);
	}
}

 
static noinline_for_stack
int format_decode(const char *fmt, struct printf_spec *spec)
{
	const char *start = fmt;
	char qualifier;

	 
	if (spec->type == FORMAT_TYPE_WIDTH) {
		if (spec->field_width < 0) {
			spec->field_width = -spec->field_width;
			spec->flags |= LEFT;
		}
		spec->type = FORMAT_TYPE_NONE;
		goto precision;
	}

	 
	if (spec->type == FORMAT_TYPE_PRECISION) {
		if (spec->precision < 0)
			spec->precision = 0;

		spec->type = FORMAT_TYPE_NONE;
		goto qualifier;
	}

	 
	spec->type = FORMAT_TYPE_NONE;

	for (; *fmt ; ++fmt) {
		if (*fmt == '%')
			break;
	}

	 
	if (fmt != start || !*fmt)
		return fmt - start;

	 
	spec->flags = 0;

	while (1) {  
		bool found = true;

		++fmt;

		switch (*fmt) {
		case '-': spec->flags |= LEFT;    break;
		case '+': spec->flags |= PLUS;    break;
		case ' ': spec->flags |= SPACE;   break;
		case '#': spec->flags |= SPECIAL; break;
		case '0': spec->flags |= ZEROPAD; break;
		default:  found = false;
		}

		if (!found)
			break;
	}

	 
	spec->field_width = -1;

	if (isdigit(*fmt))
		spec->field_width = skip_atoi(&fmt);
	else if (*fmt == '*') {
		 
		spec->type = FORMAT_TYPE_WIDTH;
		return ++fmt - start;
	}

precision:
	 
	spec->precision = -1;
	if (*fmt == '.') {
		++fmt;
		if (isdigit(*fmt)) {
			spec->precision = skip_atoi(&fmt);
			if (spec->precision < 0)
				spec->precision = 0;
		} else if (*fmt == '*') {
			 
			spec->type = FORMAT_TYPE_PRECISION;
			return ++fmt - start;
		}
	}

qualifier:
	 
	qualifier = 0;
	if (*fmt == 'h' || _tolower(*fmt) == 'l' ||
	    *fmt == 'z' || *fmt == 't') {
		qualifier = *fmt++;
		if (unlikely(qualifier == *fmt)) {
			if (qualifier == 'l') {
				qualifier = 'L';
				++fmt;
			} else if (qualifier == 'h') {
				qualifier = 'H';
				++fmt;
			}
		}
	}

	 
	spec->base = 10;
	switch (*fmt) {
	case 'c':
		spec->type = FORMAT_TYPE_CHAR;
		return ++fmt - start;

	case 's':
		spec->type = FORMAT_TYPE_STR;
		return ++fmt - start;

	case 'p':
		spec->type = FORMAT_TYPE_PTR;
		return ++fmt - start;

	case '%':
		spec->type = FORMAT_TYPE_PERCENT_CHAR;
		return ++fmt - start;

	 
	case 'o':
		spec->base = 8;
		break;

	case 'x':
		spec->flags |= SMALL;
		fallthrough;

	case 'X':
		spec->base = 16;
		break;

	case 'd':
	case 'i':
		spec->flags |= SIGN;
		break;
	case 'u':
		break;

	case 'n':
		 
		fallthrough;

	default:
		WARN_ONCE(1, "Please remove unsupported %%%c in format string\n", *fmt);
		spec->type = FORMAT_TYPE_INVALID;
		return fmt - start;
	}

	if (qualifier == 'L')
		spec->type = FORMAT_TYPE_LONG_LONG;
	else if (qualifier == 'l') {
		BUILD_BUG_ON(FORMAT_TYPE_ULONG + SIGN != FORMAT_TYPE_LONG);
		spec->type = FORMAT_TYPE_ULONG + (spec->flags & SIGN);
	} else if (qualifier == 'z') {
		spec->type = FORMAT_TYPE_SIZE_T;
	} else if (qualifier == 't') {
		spec->type = FORMAT_TYPE_PTRDIFF;
	} else if (qualifier == 'H') {
		BUILD_BUG_ON(FORMAT_TYPE_UBYTE + SIGN != FORMAT_TYPE_BYTE);
		spec->type = FORMAT_TYPE_UBYTE + (spec->flags & SIGN);
	} else if (qualifier == 'h') {
		BUILD_BUG_ON(FORMAT_TYPE_USHORT + SIGN != FORMAT_TYPE_SHORT);
		spec->type = FORMAT_TYPE_USHORT + (spec->flags & SIGN);
	} else {
		BUILD_BUG_ON(FORMAT_TYPE_UINT + SIGN != FORMAT_TYPE_INT);
		spec->type = FORMAT_TYPE_UINT + (spec->flags & SIGN);
	}

	return ++fmt - start;
}

static void
set_field_width(struct printf_spec *spec, int width)
{
	spec->field_width = width;
	if (WARN_ONCE(spec->field_width != width, "field width %d too large", width)) {
		spec->field_width = clamp(width, -FIELD_WIDTH_MAX, FIELD_WIDTH_MAX);
	}
}

static void
set_precision(struct printf_spec *spec, int prec)
{
	spec->precision = prec;
	if (WARN_ONCE(spec->precision != prec, "precision %d too large", prec)) {
		spec->precision = clamp(prec, 0, PRECISION_MAX);
	}
}

 
int vsnprintf(char *buf, size_t size, const char *fmt, va_list args)
{
	unsigned long long num;
	char *str, *end;
	struct printf_spec spec = {0};

	 
	if (WARN_ON_ONCE(size > INT_MAX))
		return 0;

	str = buf;
	end = buf + size;

	 
	if (end < buf) {
		end = ((void *)-1);
		size = end - buf;
	}

	while (*fmt) {
		const char *old_fmt = fmt;
		int read = format_decode(fmt, &spec);

		fmt += read;

		switch (spec.type) {
		case FORMAT_TYPE_NONE: {
			int copy = read;
			if (str < end) {
				if (copy > end - str)
					copy = end - str;
				memcpy(str, old_fmt, copy);
			}
			str += read;
			break;
		}

		case FORMAT_TYPE_WIDTH:
			set_field_width(&spec, va_arg(args, int));
			break;

		case FORMAT_TYPE_PRECISION:
			set_precision(&spec, va_arg(args, int));
			break;

		case FORMAT_TYPE_CHAR: {
			char c;

			if (!(spec.flags & LEFT)) {
				while (--spec.field_width > 0) {
					if (str < end)
						*str = ' ';
					++str;

				}
			}
			c = (unsigned char) va_arg(args, int);
			if (str < end)
				*str = c;
			++str;
			while (--spec.field_width > 0) {
				if (str < end)
					*str = ' ';
				++str;
			}
			break;
		}

		case FORMAT_TYPE_STR:
			str = string(str, end, va_arg(args, char *), spec);
			break;

		case FORMAT_TYPE_PTR:
			str = pointer(fmt, str, end, va_arg(args, void *),
				      spec);
			while (isalnum(*fmt))
				fmt++;
			break;

		case FORMAT_TYPE_PERCENT_CHAR:
			if (str < end)
				*str = '%';
			++str;
			break;

		case FORMAT_TYPE_INVALID:
			 
			goto out;

		default:
			switch (spec.type) {
			case FORMAT_TYPE_LONG_LONG:
				num = va_arg(args, long long);
				break;
			case FORMAT_TYPE_ULONG:
				num = va_arg(args, unsigned long);
				break;
			case FORMAT_TYPE_LONG:
				num = va_arg(args, long);
				break;
			case FORMAT_TYPE_SIZE_T:
				if (spec.flags & SIGN)
					num = va_arg(args, ssize_t);
				else
					num = va_arg(args, size_t);
				break;
			case FORMAT_TYPE_PTRDIFF:
				num = va_arg(args, ptrdiff_t);
				break;
			case FORMAT_TYPE_UBYTE:
				num = (unsigned char) va_arg(args, int);
				break;
			case FORMAT_TYPE_BYTE:
				num = (signed char) va_arg(args, int);
				break;
			case FORMAT_TYPE_USHORT:
				num = (unsigned short) va_arg(args, int);
				break;
			case FORMAT_TYPE_SHORT:
				num = (short) va_arg(args, int);
				break;
			case FORMAT_TYPE_INT:
				num = (int) va_arg(args, int);
				break;
			default:
				num = va_arg(args, unsigned int);
			}

			str = number(str, end, num, spec);
		}
	}

out:
	if (size > 0) {
		if (str < end)
			*str = '\0';
		else
			end[-1] = '\0';
	}

	 
	return str-buf;

}
EXPORT_SYMBOL(vsnprintf);

 
int vscnprintf(char *buf, size_t size, const char *fmt, va_list args)
{
	int i;

	if (unlikely(!size))
		return 0;

	i = vsnprintf(buf, size, fmt, args);

	if (likely(i < size))
		return i;

	return size - 1;
}
EXPORT_SYMBOL(vscnprintf);

 
int snprintf(char *buf, size_t size, const char *fmt, ...)
{
	va_list args;
	int i;

	va_start(args, fmt);
	i = vsnprintf(buf, size, fmt, args);
	va_end(args);

	return i;
}
EXPORT_SYMBOL(snprintf);

 

int scnprintf(char *buf, size_t size, const char *fmt, ...)
{
	va_list args;
	int i;

	va_start(args, fmt);
	i = vscnprintf(buf, size, fmt, args);
	va_end(args);

	return i;
}
EXPORT_SYMBOL(scnprintf);

 
int vsprintf(char *buf, const char *fmt, va_list args)
{
	return vsnprintf(buf, INT_MAX, fmt, args);
}
EXPORT_SYMBOL(vsprintf);

 
int sprintf(char *buf, const char *fmt, ...)
{
	va_list args;
	int i;

	va_start(args, fmt);
	i = vsnprintf(buf, INT_MAX, fmt, args);
	va_end(args);

	return i;
}
EXPORT_SYMBOL(sprintf);

#ifdef CONFIG_BINARY_PRINTF
 

 
int vbin_printf(u32 *bin_buf, size_t size, const char *fmt, va_list args)
{
	struct printf_spec spec = {0};
	char *str, *end;
	int width;

	str = (char *)bin_buf;
	end = (char *)(bin_buf + size);

#define save_arg(type)							\
({									\
	unsigned long long value;					\
	if (sizeof(type) == 8) {					\
		unsigned long long val8;				\
		str = PTR_ALIGN(str, sizeof(u32));			\
		val8 = va_arg(args, unsigned long long);		\
		if (str + sizeof(type) <= end) {			\
			*(u32 *)str = *(u32 *)&val8;			\
			*(u32 *)(str + 4) = *((u32 *)&val8 + 1);	\
		}							\
		value = val8;						\
	} else {							\
		unsigned int val4;					\
		str = PTR_ALIGN(str, sizeof(type));			\
		val4 = va_arg(args, int);				\
		if (str + sizeof(type) <= end)				\
			*(typeof(type) *)str = (type)(long)val4;	\
		value = (unsigned long long)val4;			\
	}								\
	str += sizeof(type);						\
	value;								\
})

	while (*fmt) {
		int read = format_decode(fmt, &spec);

		fmt += read;

		switch (spec.type) {
		case FORMAT_TYPE_NONE:
		case FORMAT_TYPE_PERCENT_CHAR:
			break;
		case FORMAT_TYPE_INVALID:
			goto out;

		case FORMAT_TYPE_WIDTH:
		case FORMAT_TYPE_PRECISION:
			width = (int)save_arg(int);
			 
			if (*fmt == 'p')
				set_field_width(&spec, width);
			break;

		case FORMAT_TYPE_CHAR:
			save_arg(char);
			break;

		case FORMAT_TYPE_STR: {
			const char *save_str = va_arg(args, char *);
			const char *err_msg;
			size_t len;

			err_msg = check_pointer_msg(save_str);
			if (err_msg)
				save_str = err_msg;

			len = strlen(save_str) + 1;
			if (str + len < end)
				memcpy(str, save_str, len);
			str += len;
			break;
		}

		case FORMAT_TYPE_PTR:
			 
			switch (*fmt) {
			 
			case 'S':
			case 's':
			case 'x':
			case 'K':
			case 'e':
				save_arg(void *);
				break;
			default:
				if (!isalnum(*fmt)) {
					save_arg(void *);
					break;
				}
				str = pointer(fmt, str, end, va_arg(args, void *),
					      spec);
				if (str + 1 < end)
					*str++ = '\0';
				else
					end[-1] = '\0';  
			}
			 
			while (isalnum(*fmt))
				fmt++;
			break;

		default:
			switch (spec.type) {

			case FORMAT_TYPE_LONG_LONG:
				save_arg(long long);
				break;
			case FORMAT_TYPE_ULONG:
			case FORMAT_TYPE_LONG:
				save_arg(unsigned long);
				break;
			case FORMAT_TYPE_SIZE_T:
				save_arg(size_t);
				break;
			case FORMAT_TYPE_PTRDIFF:
				save_arg(ptrdiff_t);
				break;
			case FORMAT_TYPE_UBYTE:
			case FORMAT_TYPE_BYTE:
				save_arg(char);
				break;
			case FORMAT_TYPE_USHORT:
			case FORMAT_TYPE_SHORT:
				save_arg(short);
				break;
			default:
				save_arg(int);
			}
		}
	}

out:
	return (u32 *)(PTR_ALIGN(str, sizeof(u32))) - bin_buf;
#undef save_arg
}
EXPORT_SYMBOL_GPL(vbin_printf);

 
int bstr_printf(char *buf, size_t size, const char *fmt, const u32 *bin_buf)
{
	struct printf_spec spec = {0};
	char *str, *end;
	const char *args = (const char *)bin_buf;

	if (WARN_ON_ONCE(size > INT_MAX))
		return 0;

	str = buf;
	end = buf + size;

#define get_arg(type)							\
({									\
	typeof(type) value;						\
	if (sizeof(type) == 8) {					\
		args = PTR_ALIGN(args, sizeof(u32));			\
		*(u32 *)&value = *(u32 *)args;				\
		*((u32 *)&value + 1) = *(u32 *)(args + 4);		\
	} else {							\
		args = PTR_ALIGN(args, sizeof(type));			\
		value = *(typeof(type) *)args;				\
	}								\
	args += sizeof(type);						\
	value;								\
})

	 
	if (end < buf) {
		end = ((void *)-1);
		size = end - buf;
	}

	while (*fmt) {
		const char *old_fmt = fmt;
		int read = format_decode(fmt, &spec);

		fmt += read;

		switch (spec.type) {
		case FORMAT_TYPE_NONE: {
			int copy = read;
			if (str < end) {
				if (copy > end - str)
					copy = end - str;
				memcpy(str, old_fmt, copy);
			}
			str += read;
			break;
		}

		case FORMAT_TYPE_WIDTH:
			set_field_width(&spec, get_arg(int));
			break;

		case FORMAT_TYPE_PRECISION:
			set_precision(&spec, get_arg(int));
			break;

		case FORMAT_TYPE_CHAR: {
			char c;

			if (!(spec.flags & LEFT)) {
				while (--spec.field_width > 0) {
					if (str < end)
						*str = ' ';
					++str;
				}
			}
			c = (unsigned char) get_arg(char);
			if (str < end)
				*str = c;
			++str;
			while (--spec.field_width > 0) {
				if (str < end)
					*str = ' ';
				++str;
			}
			break;
		}

		case FORMAT_TYPE_STR: {
			const char *str_arg = args;
			args += strlen(str_arg) + 1;
			str = string(str, end, (char *)str_arg, spec);
			break;
		}

		case FORMAT_TYPE_PTR: {
			bool process = false;
			int copy, len;
			 
			switch (*fmt) {
			case 'S':
			case 's':
			case 'x':
			case 'K':
			case 'e':
				process = true;
				break;
			default:
				if (!isalnum(*fmt)) {
					process = true;
					break;
				}
				 
				if (str < end) {
					len = copy = strlen(args);
					if (copy > end - str)
						copy = end - str;
					memcpy(str, args, copy);
					str += len;
					args += len + 1;
				}
			}
			if (process)
				str = pointer(fmt, str, end, get_arg(void *), spec);

			while (isalnum(*fmt))
				fmt++;
			break;
		}

		case FORMAT_TYPE_PERCENT_CHAR:
			if (str < end)
				*str = '%';
			++str;
			break;

		case FORMAT_TYPE_INVALID:
			goto out;

		default: {
			unsigned long long num;

			switch (spec.type) {

			case FORMAT_TYPE_LONG_LONG:
				num = get_arg(long long);
				break;
			case FORMAT_TYPE_ULONG:
			case FORMAT_TYPE_LONG:
				num = get_arg(unsigned long);
				break;
			case FORMAT_TYPE_SIZE_T:
				num = get_arg(size_t);
				break;
			case FORMAT_TYPE_PTRDIFF:
				num = get_arg(ptrdiff_t);
				break;
			case FORMAT_TYPE_UBYTE:
				num = get_arg(unsigned char);
				break;
			case FORMAT_TYPE_BYTE:
				num = get_arg(signed char);
				break;
			case FORMAT_TYPE_USHORT:
				num = get_arg(unsigned short);
				break;
			case FORMAT_TYPE_SHORT:
				num = get_arg(short);
				break;
			case FORMAT_TYPE_UINT:
				num = get_arg(unsigned int);
				break;
			default:
				num = get_arg(int);
			}

			str = number(str, end, num, spec);
		}  
		}  
	}  

out:
	if (size > 0) {
		if (str < end)
			*str = '\0';
		else
			end[-1] = '\0';
	}

#undef get_arg

	 
	return str - buf;
}
EXPORT_SYMBOL_GPL(bstr_printf);

 
int bprintf(u32 *bin_buf, size_t size, const char *fmt, ...)
{
	va_list args;
	int ret;

	va_start(args, fmt);
	ret = vbin_printf(bin_buf, size, fmt, args);
	va_end(args);

	return ret;
}
EXPORT_SYMBOL_GPL(bprintf);

#endif  

 
int vsscanf(const char *buf, const char *fmt, va_list args)
{
	const char *str = buf;
	char *next;
	char digit;
	int num = 0;
	u8 qualifier;
	unsigned int base;
	union {
		long long s;
		unsigned long long u;
	} val;
	s16 field_width;
	bool is_sign;

	while (*fmt) {
		 
		 
		if (isspace(*fmt)) {
			fmt = skip_spaces(++fmt);
			str = skip_spaces(str);
		}

		 
		if (*fmt != '%' && *fmt) {
			if (*fmt++ != *str++)
				break;
			continue;
		}

		if (!*fmt)
			break;
		++fmt;

		 
		if (*fmt == '*') {
			if (!*str)
				break;
			while (!isspace(*fmt) && *fmt != '%' && *fmt) {
				 
				if (*fmt == '[')
					return num;
				fmt++;
			}
			while (!isspace(*str) && *str)
				str++;
			continue;
		}

		 
		field_width = -1;
		if (isdigit(*fmt)) {
			field_width = skip_atoi(&fmt);
			if (field_width <= 0)
				break;
		}

		 
		qualifier = -1;
		if (*fmt == 'h' || _tolower(*fmt) == 'l' ||
		    *fmt == 'z') {
			qualifier = *fmt++;
			if (unlikely(qualifier == *fmt)) {
				if (qualifier == 'h') {
					qualifier = 'H';
					fmt++;
				} else if (qualifier == 'l') {
					qualifier = 'L';
					fmt++;
				}
			}
		}

		if (!*fmt)
			break;

		if (*fmt == 'n') {
			 
			*va_arg(args, int *) = str - buf;
			++fmt;
			continue;
		}

		if (!*str)
			break;

		base = 10;
		is_sign = false;

		switch (*fmt++) {
		case 'c':
		{
			char *s = (char *)va_arg(args, char*);
			if (field_width == -1)
				field_width = 1;
			do {
				*s++ = *str++;
			} while (--field_width > 0 && *str);
			num++;
		}
		continue;
		case 's':
		{
			char *s = (char *)va_arg(args, char *);
			if (field_width == -1)
				field_width = SHRT_MAX;
			 
			str = skip_spaces(str);

			 
			while (*str && !isspace(*str) && field_width--)
				*s++ = *str++;
			*s = '\0';
			num++;
		}
		continue;
		 
		case '[':
		{
			char *s = (char *)va_arg(args, char *);
			DECLARE_BITMAP(set, 256) = {0};
			unsigned int len = 0;
			bool negate = (*fmt == '^');

			 
			if (field_width == -1)
				return num;

			if (negate)
				++fmt;

			for ( ; *fmt && *fmt != ']'; ++fmt, ++len)
				__set_bit((u8)*fmt, set);

			 
			if (!*fmt || !len)
				return num;
			++fmt;

			if (negate) {
				bitmap_complement(set, set, 256);
				 
				__clear_bit(0, set);
			}

			 
			if (!test_bit((u8)*str, set))
				return num;

			while (test_bit((u8)*str, set) && field_width--)
				*s++ = *str++;
			*s = '\0';
			++num;
		}
		continue;
		case 'o':
			base = 8;
			break;
		case 'x':
		case 'X':
			base = 16;
			break;
		case 'i':
			base = 0;
			fallthrough;
		case 'd':
			is_sign = true;
			fallthrough;
		case 'u':
			break;
		case '%':
			 
			if (*str++ != '%')
				return num;
			continue;
		default:
			 
			return num;
		}

		 
		str = skip_spaces(str);

		digit = *str;
		if (is_sign && digit == '-') {
			if (field_width == 1)
				break;

			digit = *(str + 1);
		}

		if (!digit
		    || (base == 16 && !isxdigit(digit))
		    || (base == 10 && !isdigit(digit))
		    || (base == 8 && !isodigit(digit))
		    || (base == 0 && !isdigit(digit)))
			break;

		if (is_sign)
			val.s = simple_strntoll(str,
						field_width >= 0 ? field_width : INT_MAX,
						&next, base);
		else
			val.u = simple_strntoull(str,
						 field_width >= 0 ? field_width : INT_MAX,
						 &next, base);

		switch (qualifier) {
		case 'H':	 
			if (is_sign)
				*va_arg(args, signed char *) = val.s;
			else
				*va_arg(args, unsigned char *) = val.u;
			break;
		case 'h':
			if (is_sign)
				*va_arg(args, short *) = val.s;
			else
				*va_arg(args, unsigned short *) = val.u;
			break;
		case 'l':
			if (is_sign)
				*va_arg(args, long *) = val.s;
			else
				*va_arg(args, unsigned long *) = val.u;
			break;
		case 'L':
			if (is_sign)
				*va_arg(args, long long *) = val.s;
			else
				*va_arg(args, unsigned long long *) = val.u;
			break;
		case 'z':
			*va_arg(args, size_t *) = val.u;
			break;
		default:
			if (is_sign)
				*va_arg(args, int *) = val.s;
			else
				*va_arg(args, unsigned int *) = val.u;
			break;
		}
		num++;

		if (!next)
			break;
		str = next;
	}

	return num;
}
EXPORT_SYMBOL(vsscanf);

 
int sscanf(const char *buf, const char *fmt, ...)
{
	va_list args;
	int i;

	va_start(args, fmt);
	i = vsscanf(buf, fmt, args);
	va_end(args);

	return i;
}
EXPORT_SYMBOL(sscanf);
