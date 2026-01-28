#include <ctype.h>
#include <string.h>
#include "testcases.h"
struct _aarch64_ctx *get_header(struct _aarch64_ctx *head, uint32_t magic,
				size_t resv_sz, size_t *offset)
{
	size_t offs = 0;
	struct _aarch64_ctx *found = NULL;
	if (!head || resv_sz < HDR_SZ)
		return found;
	while (offs <= resv_sz - HDR_SZ &&
	       head->magic != magic && head->magic) {
		offs += head->size;
		head = GET_RESV_NEXT_HEAD(head);
	}
	if (head->magic == magic) {
		found = head;
		if (offset)
			*offset = offs;
	}
	return found;
}
bool validate_extra_context(struct extra_context *extra, char **err,
			    void **extra_data, size_t *extra_size)
{
	struct _aarch64_ctx *term;
	if (!extra || !err)
		return false;
	fprintf(stderr, "Validating EXTRA...\n");
	term = GET_RESV_NEXT_HEAD(&extra->head);
	if (!term || term->magic || term->size) {
		*err = "Missing terminator after EXTRA context";
		return false;
	}
	if (extra->datap & 0x0fUL)
		*err = "Extra DATAP misaligned";
	else if (extra->size & 0x0fUL)
		*err = "Extra SIZE misaligned";
	else if (extra->datap != (uint64_t)term + 0x10UL)
		*err = "Extra DATAP misplaced (not contiguous)";
	if (*err)
		return false;
	*extra_data = (void *)extra->datap;
	*extra_size = extra->size;
	return true;
}
bool validate_sve_context(struct sve_context *sve, char **err)
{
	size_t regs_size
		= ((SVE_SIG_CONTEXT_SIZE(sve_vq_from_vl(sve->vl)) + 15) / 16) * 16;
	if (!sve || !err)
		return false;
	if ((sve->head.size != sizeof(struct sve_context)) &&
	    (sve->head.size != regs_size)) {
		*err = "bad size for SVE context";
		return false;
	}
	if (!sve_vl_valid(sve->vl)) {
		*err = "SVE VL invalid";
		return false;
	}
	return true;
}
bool validate_za_context(struct za_context *za, char **err)
{
	size_t regs_size
		= ((ZA_SIG_CONTEXT_SIZE(sve_vq_from_vl(za->vl)) + 15) / 16) * 16;
	if (!za || !err)
		return false;
	if ((za->head.size != sizeof(struct za_context)) &&
	    (za->head.size != regs_size)) {
		*err = "bad size for ZA context";
		return false;
	}
	if (!sve_vl_valid(za->vl)) {
		*err = "SME VL in ZA context invalid";
		return false;
	}
	return true;
}
bool validate_zt_context(struct zt_context *zt, char **err)
{
	if (!zt || !err)
		return false;
	if (zt->nregs == 0) {
		*err = "no registers";
		return false;
	}
	if (zt->head.size != ZT_SIG_CONTEXT_SIZE(zt->nregs)) {
		*err = "register count does not match size";
		return false;
	}
	return true;
}
bool validate_reserved(ucontext_t *uc, size_t resv_sz, char **err)
{
	bool terminated = false;
	size_t offs = 0;
	int flags = 0;
	int new_flags, i;
	struct extra_context *extra = NULL;
	struct sve_context *sve = NULL;
	struct za_context *za = NULL;
	struct zt_context *zt = NULL;
	struct _aarch64_ctx *head =
		(struct _aarch64_ctx *)uc->uc_mcontext.__reserved;
	void *extra_data = NULL;
	size_t extra_sz = 0;
	char magic[4];
	if (!err)
		return false;
	while (head && !terminated && offs < resv_sz) {
		if ((uint64_t)head & 0x0fUL) {
			*err = "Misaligned HEAD";
			return false;
		}
		new_flags = 0;
		switch (head->magic) {
		case 0:
			if (head->size) {
				*err = "Bad size for terminator";
			} else if (extra_data) {
				head = extra_data;
				resv_sz = extra_sz;
				offs = 0;
				extra_data = NULL;
				extra_sz = 0;
				continue;
			} else {
				terminated = true;
			}
			break;
		case FPSIMD_MAGIC:
			if (flags & FPSIMD_CTX)
				*err = "Multiple FPSIMD_MAGIC";
			else if (head->size !=
				 sizeof(struct fpsimd_context))
				*err = "Bad size for fpsimd_context";
			new_flags |= FPSIMD_CTX;
			break;
		case ESR_MAGIC:
			if (head->size != sizeof(struct esr_context))
				*err = "Bad size for esr_context";
			break;
		case TPIDR2_MAGIC:
			if (head->size != sizeof(struct tpidr2_context))
				*err = "Bad size for tpidr2_context";
			break;
		case SVE_MAGIC:
			if (flags & SVE_CTX)
				*err = "Multiple SVE_MAGIC";
			sve = (struct sve_context *)head;
			new_flags |= SVE_CTX;
			break;
		case ZA_MAGIC:
			if (flags & ZA_CTX)
				*err = "Multiple ZA_MAGIC";
			za = (struct za_context *)head;
			new_flags |= ZA_CTX;
			break;
		case ZT_MAGIC:
			if (flags & ZT_CTX)
				*err = "Multiple ZT_MAGIC";
			zt = (struct zt_context *)head;
			new_flags |= ZT_CTX;
			break;
		case EXTRA_MAGIC:
			if (flags & EXTRA_CTX)
				*err = "Multiple EXTRA_MAGIC";
			else if (head->size !=
				 sizeof(struct extra_context))
				*err = "Bad size for extra_context";
			new_flags |= EXTRA_CTX;
			extra = (struct extra_context *)head;
			break;
		case KSFT_BAD_MAGIC:
			*err = "BAD MAGIC !";
			break;
		default:
			memcpy(magic, &head->magic, sizeof(magic));
			for (i = 0; i < sizeof(magic); i++)
				if (!isalnum(magic[i]))
					magic[i] = '?';
			fprintf(stdout,
				"SKIP Unknown MAGIC: 0x%X (%c%c%c%c) - Is KSFT arm64/signal up to date ?\n",
				head->magic,
				magic[3], magic[2], magic[1], magic[0]);
			break;
		}
		if (*err)
			return false;
		offs += head->size;
		if (resv_sz < offs + sizeof(*head)) {
			*err = "HEAD Overrun";
			return false;
		}
		if (new_flags & EXTRA_CTX)
			if (!validate_extra_context(extra, err,
						    &extra_data, &extra_sz))
				return false;
		if (new_flags & SVE_CTX)
			if (!validate_sve_context(sve, err))
				return false;
		if (new_flags & ZA_CTX)
			if (!validate_za_context(za, err))
				return false;
		if (new_flags & ZT_CTX)
			if (!validate_zt_context(zt, err))
				return false;
		flags |= new_flags;
		head = GET_RESV_NEXT_HEAD(head);
	}
	if (terminated && !(flags & FPSIMD_CTX)) {
		*err = "Missing FPSIMD";
		return false;
	}
	if (terminated && (flags & ZT_CTX) && !(flags & ZA_CTX)) {
		*err = "ZT context but no ZA context";
		return false;
	}
	return true;
}
struct _aarch64_ctx *get_starting_head(struct _aarch64_ctx *shead,
				       size_t need_sz, size_t resv_sz,
				       size_t *offset)
{
	size_t offs = 0;
	struct _aarch64_ctx *head;
	head = get_terminator(shead, resv_sz, &offs);
	if (!head)
		return head;
	if (resv_sz - offs < need_sz) {
		fprintf(stderr, "Low on space:%zd. Discarding extra_context.\n",
			resv_sz - offs);
		head = get_header(shead, EXTRA_MAGIC, resv_sz, &offs);
		if (!head || resv_sz - offs < need_sz) {
			fprintf(stderr,
				"Failed to reclaim space on sigframe.\n");
			return NULL;
		}
	}
	fprintf(stderr, "Available space:%zd\n", resv_sz - offs);
	if (offset)
		*offset = offs;
	return head;
}
