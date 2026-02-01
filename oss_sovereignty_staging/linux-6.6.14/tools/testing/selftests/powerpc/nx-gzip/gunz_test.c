

 

#define _ISOC11_SOURCE	
#define _DEFAULT_SOURCE	

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/fcntl.h>
#include <sys/mman.h>
#include <endian.h>
#include <bits/endian.h>
#include <sys/ioctl.h>
#include <assert.h>
#include <errno.h>
#include <signal.h>
#include "nxu.h"
#include "nx.h"
#include "crb.h"

int nx_dbg;
FILE *nx_gzip_log;

#define NX_MIN(X, Y) (((X) < (Y))?(X):(Y))
#define NX_MAX(X, Y) (((X) > (Y))?(X):(Y))

#define GETINPC(X) fgetc(X)
#define FNAME_MAX 1024

 
#define fifo_used_bytes(used) (used)
#define fifo_free_bytes(used, len) ((len)-(used))
 
#define fifo_free_first_bytes(cur, used, len)  ((((cur)+(used)) <= (len)) \
						  ? (len)-((cur)+(used)) : 0)
#define fifo_free_last_bytes(cur, used, len)   ((((cur)+(used)) <= (len)) \
						  ? (cur) : (len)-(used))
 
#define fifo_used_first_bytes(cur, used, len)  ((((cur)+(used)) <= (len)) \
						  ? (used) : (len)-(cur))
#define fifo_used_last_bytes(cur, used, len)   ((((cur)+(used)) <= (len)) \
						  ? 0 : ((used)+(cur))-(len))
 
#define fifo_free_first_offset(cur, used)      ((cur)+(used))
#define fifo_free_last_offset(cur, used, len)  \
					   fifo_used_last_bytes(cur, used, len)
 
#define fifo_used_first_offset(cur)            (cur)
#define fifo_used_last_offset(cur)             (0)

const int fifo_in_len = 1<<24;
const int fifo_out_len = 1<<24;
const int page_sz = 1<<16;
const int line_sz = 1<<7;
const int window_max = 1<<15;

 
static inline uint32_t nx_append_dde(struct nx_dde_t *ddl, void *addr,
					uint32_t len)
{
	uint32_t ddecnt;
	uint32_t bytes;

	if (addr == NULL && len == 0) {
		clearp_dde(ddl);
		return 0;
	}

	NXPRT(fprintf(stderr, "%d: %s addr %p len %x\n", __LINE__, addr,
			__func__, len));

	 
	ddecnt = getpnn(ddl, dde_count);
	bytes = getp32(ddl, ddebc);

	if (ddecnt == 0 && bytes == 0) {
		 
		bytes = len;
		putp32(ddl, ddebc, bytes);
		putp64(ddl, ddead, (uint64_t) addr);
	} else if (ddecnt == 0) {
		 
		ddl[1] = ddl[0];

		 
		clear_dde(ddl[2]);
		put32(ddl[2], ddebc, len);
		put64(ddl[2], ddead, (uint64_t) addr);

		 
		ddecnt = 2;
		putpnn(ddl, dde_count, ddecnt);
		bytes = bytes + len;
		putp32(ddl, ddebc, bytes);
		 
		putp64(ddl, ddead, (uint64_t) &ddl[1]);
	} else {
		 
		++ddecnt;
		clear_dde(ddl[ddecnt]);
		put64(ddl[ddecnt], ddead, (uint64_t) addr);
		put32(ddl[ddecnt], ddebc, len);

		putpnn(ddl, dde_count, ddecnt);
		bytes = bytes + len;
		putp32(ddl, ddebc, bytes);  
	}
	return bytes;
}

 
static int nx_touch_pages_dde(struct nx_dde_t *ddep, long buf_sz, long page_sz,
				int wr)
{
	uint32_t indirect_count;
	uint32_t buf_len;
	long total;
	uint64_t buf_addr;
	struct nx_dde_t *dde_list;
	int i;

	assert(!!ddep);

	indirect_count = getpnn(ddep, dde_count);

	NXPRT(fprintf(stderr, "%s dde_count %d request len ", __func__,
			indirect_count));
	NXPRT(fprintf(stderr, "0x%lx\n", buf_sz));

	if (indirect_count == 0) {
		 
		buf_len = getp32(ddep, ddebc);
		buf_addr = getp64(ddep, ddead);

		NXPRT(fprintf(stderr, "touch direct ddebc 0x%x ddead %p\n",
				buf_len, (void *)buf_addr));

		if (buf_sz == 0)
			nxu_touch_pages((void *)buf_addr, buf_len, page_sz, wr);
		else
			nxu_touch_pages((void *)buf_addr, NX_MIN(buf_len,
					buf_sz), page_sz, wr);

		return ERR_NX_OK;
	}

	 
	if (indirect_count > MAX_DDE_COUNT)
		return ERR_NX_EXCESSIVE_DDE;

	 
	dde_list = (struct nx_dde_t *) getp64(ddep, ddead);

	if (buf_sz == 0)
		buf_sz = getp32(ddep, ddebc);

	total = 0;
	for (i = 0; i < indirect_count; i++) {
		buf_len = get32(dde_list[i], ddebc);
		buf_addr = get64(dde_list[i], ddead);
		total += buf_len;

		NXPRT(fprintf(stderr, "touch loop len 0x%x ddead %p total ",
				buf_len, (void *)buf_addr));
		NXPRT(fprintf(stderr, "0x%lx\n", total));

		 
		if (total > buf_sz) {
			buf_len = NX_MIN(buf_len, total - buf_sz);
			nxu_touch_pages((void *)buf_addr, buf_len, page_sz, wr);
			NXPRT(fprintf(stderr, "touch loop break len 0x%x ",
				      buf_len));
			NXPRT(fprintf(stderr, "ddead %p\n", (void *)buf_addr));
			break;
		}
		nxu_touch_pages((void *)buf_addr, buf_len, page_sz, wr);
	}
	return ERR_NX_OK;
}

 
static int nx_submit_job(struct nx_dde_t *src, struct nx_dde_t *dst,
			 struct nx_gzip_crb_cpb_t *cmdp, void *handle)
{
	uint64_t csbaddr;

	memset((void *)&cmdp->crb.csb, 0, sizeof(cmdp->crb.csb));

	cmdp->crb.source_dde = *src;
	cmdp->crb.target_dde = *dst;

	 
	csbaddr = ((uint64_t) &cmdp->crb.csb) & csb_address_mask;
	put64(cmdp->crb, csb_address, csbaddr);

	 
	cmdp->cpb.out_spbc_comp_wrap = 0;
	cmdp->cpb.out_spbc_comp_with_count = 0;
	cmdp->cpb.out_spbc_decomp = 0;

	 
	put32(cmdp->cpb, out_crc, INIT_CRC);
	put32(cmdp->cpb, out_adler, INIT_ADLER);

	 
	return nxu_submit_job(cmdp, handle);
}

int decompress_file(int argc, char **argv, void *devhandle)
{
	FILE *inpf = NULL;
	FILE *outf = NULL;

	int c, expect, i, cc, rc = 0;
	char gzfname[FNAME_MAX];

	 
	char *fifo_in, *fifo_out;
	int used_in, cur_in, used_out, cur_out, read_sz, n;
	int first_free, last_free, first_used, last_used;
	int first_offset, last_offset;
	int write_sz, free_space, source_sz;
	int source_sz_estimate, target_sz_estimate;
	uint64_t last_comp_ratio = 0;  
	uint64_t total_out = 0;
	int is_final, is_eof;

	 
	int sfbt, subc, spbc, tpbc, nx_ce, fc, resuming = 0;
	int history_len = 0;
	struct nx_gzip_crb_cpb_t cmd, *cmdp;
	struct nx_dde_t *ddl_in;
	struct nx_dde_t dde_in[6] __aligned(128);
	struct nx_dde_t *ddl_out;
	struct nx_dde_t dde_out[6] __aligned(128);
	int pgfault_retries;

	 
	off_t input_file_offset;

	if (argc > 2) {
		fprintf(stderr, "usage: %s <fname> or stdin\n", argv[0]);
		fprintf(stderr, "    writes to stdout or <fname>.nx.gunzip\n");
		return -1;
	}

	if (argc == 1) {
		inpf = stdin;
		outf = stdout;
	} else if (argc == 2) {
		char w[1024];
		char *wp;

		inpf = fopen(argv[1], "r");
		if (inpf == NULL) {
			perror(argv[1]);
			return -1;
		}

		 
		wp = (NULL != (wp = strrchr(argv[1], '/'))) ? (wp+1) : argv[1];
		strcpy(w, wp);
		strcat(w, ".nx.gunzip");

		outf = fopen(w, "w");
		if (outf == NULL) {
			perror(w);
			return -1;
		}
	}

	 
	c = GETINPC(inpf); expect = 0x1f;  
	if (c != expect)
		goto err1;

	c = GETINPC(inpf); expect = 0x8b;  
	if (c != expect)
		goto err1;

	c = GETINPC(inpf); expect = 0x08;  
	if (c != expect)
		goto err1;

	int flg = GETINPC(inpf);  

	if (flg & 0xE0 || flg & 0x4 || flg == EOF)
		goto err2;

	fprintf(stderr, "gzHeader FLG %x\n", flg);

	 
	for (i = 0; i < 6; i++) {
		char tmp[10];

		tmp[i] = GETINPC(inpf);
		if (tmp[i] == EOF)
			goto err3;
		fprintf(stderr, "%02x ", tmp[i]);
		if (i == 5)
			fprintf(stderr, "\n");
	}
	fprintf(stderr, "gzHeader MTIME, XFL, OS ignored\n");

	 
	if (flg & 0x8) {
		int k = 0;

		do {
			c = GETINPC(inpf);
			if (c == EOF || k >= FNAME_MAX)
				goto err3;
			gzfname[k++] = c;
		} while (c);
		fprintf(stderr, "gzHeader FNAME: %s\n", gzfname);
	}

	 
	if (flg & 0x2) {
		c = GETINPC(inpf);
		if (c == EOF)
			goto err3;
		c = GETINPC(inpf);
		if (c == EOF)
			goto err3;
		fprintf(stderr, "gzHeader FHCRC: ignored\n");
	}

	used_in = cur_in = used_out = cur_out = 0;
	is_final = is_eof = 0;

	 
	assert((fifo_in  = (char *)(uintptr_t)aligned_alloc(line_sz,
				   fifo_in_len + page_sz)) != NULL);
	assert((fifo_out = (char *)(uintptr_t)aligned_alloc(line_sz,
				   fifo_out_len + page_sz + line_sz)) != NULL);
	 
	fifo_out = fifo_out + line_sz;
	nxu_touch_pages(fifo_out, fifo_out_len, page_sz, 1);

	ddl_in  = &dde_in[0];
	ddl_out = &dde_out[0];
	cmdp = &cmd;
	memset(&cmdp->crb, 0, sizeof(cmdp->crb));

read_state:

	 

	NXPRT(fprintf(stderr, "read_state:\n"));

	if (is_eof != 0)
		goto write_state;

	 

	 
	cur_in = (used_in == 0) ? 0 : cur_in;

	 
	free_space = NX_MAX(0, fifo_free_bytes(used_in, fifo_in_len)
			    - line_sz);

	 
	first_free = fifo_free_first_bytes(cur_in, used_in, fifo_in_len);
	last_free  = fifo_free_last_bytes(cur_in, used_in, fifo_in_len);

	 
	first_offset = fifo_free_first_offset(cur_in, used_in);
	last_offset  = fifo_free_last_offset(cur_in, used_in, fifo_in_len);

	 
	read_sz = NX_MIN(free_space, first_free);
	n = 0;
	if (read_sz > 0) {
		 
		n = fread(fifo_in + first_offset, 1, read_sz, inpf);
		used_in = used_in + n;
		free_space = free_space - n;
		assert(n <= read_sz);
		if (n != read_sz) {
			 
			is_eof = 1;
			goto write_state;
		}
	}

	 
	if (last_free > 0) {
		 
		read_sz = NX_MIN(free_space, last_free);
		n = 0;
		if (read_sz > 0) {
			n = fread(fifo_in + last_offset, 1, read_sz, inpf);
			used_in = used_in + n;        
			free_space = free_space - n;  
			assert(n <= read_sz);
			if (n != read_sz) {
				 
				is_eof = 1;
				goto write_state;
			}
		}
	}

	 

write_state:

	 

	NXPRT(fprintf(stderr, "write_state:\n"));

	if (used_out == 0)
		goto decomp_state;

	 

	first_used = fifo_used_first_bytes(cur_out, used_out, fifo_out_len);
	last_used  = fifo_used_last_bytes(cur_out, used_out, fifo_out_len);

	write_sz = first_used;

	n = 0;
	if (write_sz > 0) {
		n = fwrite(fifo_out + cur_out, 1, write_sz, outf);
		used_out = used_out - n;
		 
		cur_out = (cur_out + n) % fifo_out_len;
		assert(n <= write_sz);
		if (n != write_sz) {
			fprintf(stderr, "error: write\n");
			rc = -1;
			goto err5;
		}
	}

	if (last_used > 0) {  
		write_sz = last_used;  
		n = 0;
		if (write_sz > 0) {
			n = fwrite(fifo_out, 1, write_sz, outf);
			used_out = used_out - n;
			cur_out = (cur_out + n) % fifo_out_len;
			assert(n <= write_sz);
			if (n != write_sz) {
				fprintf(stderr, "error: write\n");
				rc = -1;
				goto err5;
			}
		}
	}

decomp_state:

	 

	NXPRT(fprintf(stderr, "decomp_state:\n"));

	if (is_final)
		goto finish_state;

	 
	clearp_dde(ddl_in);
	clearp_dde(ddl_out);

	 
	if (resuming) {
		 
		fc = GZIP_FC_DECOMPRESS_RESUME;

		cmdp->cpb.in_crc   = cmdp->cpb.out_crc;
		cmdp->cpb.in_adler = cmdp->cpb.out_adler;

		 
		history_len = (history_len + 15) / 16;
		putnn(cmdp->cpb, in_histlen, history_len);
		history_len = history_len * 16;  

		if (history_len > 0) {
			 
			if (cur_out >= history_len) {
				nx_append_dde(ddl_in, fifo_out
					      + (cur_out - history_len),
					      history_len);
			} else {
				nx_append_dde(ddl_in, fifo_out
					      + ((fifo_out_len + cur_out)
					      - history_len),
					      history_len - cur_out);
				 
				nx_append_dde(ddl_in, fifo_out, cur_out);
			}

		}
	} else {
		 
		fc = GZIP_FC_DECOMPRESS;

		history_len = 0;
		 
		cmdp->cpb.in_histlen = 0;
		total_out = 0;

		put32(cmdp->cpb, in_crc, INIT_CRC);
		put32(cmdp->cpb, in_adler, INIT_ADLER);
		put32(cmdp->cpb, out_crc, INIT_CRC);
		put32(cmdp->cpb, out_adler, INIT_ADLER);

		 
		last_comp_ratio = 100UL;
	}
	cmdp->crb.gzip_fc = 0;
	putnn(cmdp->crb, gzip_fc, fc);

	 
	first_used = fifo_used_first_bytes(cur_in, used_in, fifo_in_len);
	last_used = fifo_used_last_bytes(cur_in, used_in, fifo_in_len);

	if (first_used > 0)
		nx_append_dde(ddl_in, fifo_in + cur_in, first_used);

	if (last_used > 0)
		nx_append_dde(ddl_in, fifo_in, last_used);

	 
	first_free = fifo_free_first_bytes(cur_out, used_out, fifo_out_len);
	last_free = fifo_free_last_bytes(cur_out, used_out, fifo_out_len);

	 
	int target_max = NX_MAX(0, fifo_free_bytes(used_out, fifo_out_len)
				- (1<<16));

	NXPRT(fprintf(stderr, "target_max %d (0x%x)\n", target_max,
		      target_max));

	first_free = NX_MIN(target_max, first_free);
	if (first_free > 0) {
		first_offset = fifo_free_first_offset(cur_out, used_out);
		nx_append_dde(ddl_out, fifo_out + first_offset, first_free);
	}

	if (last_free > 0) {
		last_free = NX_MIN(target_max - first_free, last_free);
		if (last_free > 0) {
			last_offset = fifo_free_last_offset(cur_out, used_out,
							    fifo_out_len);
			nx_append_dde(ddl_out, fifo_out + last_offset,
				      last_free);
		}
	}

	 

	 
	source_sz = getp32(ddl_in, ddebc);
	assert(source_sz > history_len);
	source_sz = source_sz - history_len;

	 

	source_sz_estimate = ((uint64_t)target_max * last_comp_ratio * 3UL)
				/ 4000;

	if (source_sz_estimate < source_sz) {
		 
		source_sz = source_sz_estimate;
		target_sz_estimate = target_max;
	} else {
		 
		target_sz_estimate = ((uint64_t)source_sz * 1000UL)
					/ (last_comp_ratio + 1);
		target_sz_estimate = NX_MIN(2 * target_sz_estimate,
					    target_max);
	}

	source_sz = source_sz + history_len;

	 
	pgfault_retries = NX_MAX_FAULTS;

restart_nx:

	putp32(ddl_in, ddebc, source_sz);

	 
	nxu_touch_pages(cmdp, sizeof(struct nx_gzip_crb_cpb_t), page_sz, 1);
	nx_touch_pages_dde(ddl_in, 0, page_sz, 0);
	nx_touch_pages_dde(ddl_out, target_sz_estimate, page_sz, 1);

	 
	cc = nx_submit_job(ddl_in, ddl_out, cmdp, devhandle);

	switch (cc) {

	case ERR_NX_AT_FAULT:

		 
		NXPRT(fprintf(stderr, "ERR_NX_AT_FAULT %p\n",
			      (void *)cmdp->crb.csb.fsaddr));

		if (pgfault_retries == NX_MAX_FAULTS) {
			 
			--pgfault_retries;
			goto restart_nx;
		} else if (pgfault_retries > 0) {
			 
			if (source_sz > page_sz)
				source_sz = NX_MAX(source_sz / 2, page_sz);
			--pgfault_retries;
			goto restart_nx;
		} else {
			fprintf(stderr, "cannot make progress; too many ");
			fprintf(stderr, "page fault retries cc= %d\n", cc);
			rc = -1;
			goto err5;
		}

	case ERR_NX_DATA_LENGTH:

		NXPRT(fprintf(stderr, "ERR_NX_DATA_LENGTH; "));
		NXPRT(fprintf(stderr, "stream may have trailing data\n"));

		 
		nx_ce = get_csb_ce_ms3b(cmdp->crb.csb);

		if (!csb_ce_termination(nx_ce) &&
		    csb_ce_partial_completion(nx_ce)) {
			 
			sfbt = getnn(cmdp->cpb, out_sfbt);  
			subc = getnn(cmdp->cpb, out_subc);  
			spbc = get32(cmdp->cpb, out_spbc_decomp);
			tpbc = get32(cmdp->crb.csb, tpbc);
			assert(target_max >= tpbc);

			goto ok_cc3;  
		} else {
			 
			rc = -1;
			fprintf(stderr, "history length error cc= %d\n", cc);
			goto err5;
		}

	case ERR_NX_TARGET_SPACE:

		 
		assert(source_sz > history_len);
		source_sz = ((source_sz - history_len + 2) / 2) + history_len;
		NXPRT(fprintf(stderr, "ERR_NX_TARGET_SPACE; retry with "));
		NXPRT(fprintf(stderr, "smaller input data src %d hist %d\n",
			      source_sz, history_len));
		goto restart_nx;

	case ERR_NX_OK:

		 
		fprintf(stderr, "ERR_NX_OK\n");
		spbc = get32(cmdp->cpb, out_spbc_decomp);
		tpbc = get32(cmdp->crb.csb, tpbc);
		assert(target_max >= tpbc);
		assert(spbc >= history_len);
		source_sz = spbc - history_len;
		goto offsets_state;

	default:
		fprintf(stderr, "error: cc= %d\n", cc);
		rc = -1;
		goto err5;
	}

ok_cc3:

	NXPRT(fprintf(stderr, "cc3: sfbt: %x\n", sfbt));

	assert(spbc > history_len);
	source_sz = spbc - history_len;

	 

	switch (sfbt) {
		int dhtlen;

	case 0x0:  

		 

		source_sz = source_sz - subc / 8;
		is_final = 1;
		break;

		 

	case 0x8:  
	case 0x9:  

		 
		source_sz = source_sz - ((subc + 7) / 8);

		 
		cmdp->cpb.in_subc = 0;
		cmdp->cpb.in_sfbt = 0;
		putnn(cmdp->cpb, in_subc, subc % 8);
		putnn(cmdp->cpb, in_sfbt, sfbt);
		putnn(cmdp->cpb, in_rembytecnt, getnn(cmdp->cpb,
						      out_rembytecnt));
		break;

	case 0xA:  
	case 0xB:  

		source_sz = source_sz - ((subc + 7) / 8);

		 
		cmdp->cpb.in_subc = 0;
		cmdp->cpb.in_sfbt = 0;
		putnn(cmdp->cpb, in_subc, subc % 8);
		putnn(cmdp->cpb, in_sfbt, sfbt);
		break;

	case 0xC:  
	case 0xD:  

		source_sz = source_sz - ((subc + 7) / 8);

		 
		cmdp->cpb.in_subc = 0;
		cmdp->cpb.in_sfbt = 0;
		putnn(cmdp->cpb, in_subc, subc % 8);
		putnn(cmdp->cpb, in_sfbt, sfbt);

		dhtlen = getnn(cmdp->cpb, out_dhtlen);
		putnn(cmdp->cpb, in_dhtlen, dhtlen);
		assert(dhtlen >= 42);

		 
		dhtlen = (dhtlen + 127) / 128;

		while (dhtlen > 0) {  
			--dhtlen;
			cmdp->cpb.in_dht[dhtlen] = cmdp->cpb.out_dht[dhtlen];
		}
		break;

	case 0xE:  
		      
	case 0xF:  

		source_sz = source_sz - ((subc + 7) / 8);

		 
		cmdp->cpb.in_subc = 0;
		cmdp->cpb.in_sfbt = 0;
		putnn(cmdp->cpb, in_subc, subc % 8);
		putnn(cmdp->cpb, in_sfbt, sfbt);

		 
		if (is_eof && (source_sz == 0))
			is_final = 1;
	}

offsets_state:

	 

	NXPRT(fprintf(stderr, "offsets_state:\n"));

	 
	used_in = used_in - source_sz;
	cur_in = (cur_in + source_sz) % fifo_in_len;
	input_file_offset = input_file_offset + source_sz;

	 
	used_out = used_out + tpbc;

	assert(used_out <= fifo_out_len);

	total_out = total_out + tpbc;

	 
	history_len = (total_out > window_max) ? window_max : total_out;

	 
	last_comp_ratio = (1000UL * ((uint64_t)source_sz + 1))
			  / ((uint64_t)tpbc + 1);
	last_comp_ratio = NX_MAX(NX_MIN(1000UL, last_comp_ratio), 1);
	NXPRT(fprintf(stderr, "comp_ratio %ld source_sz %d spbc %d tpbc %d\n",
		      last_comp_ratio, source_sz, spbc, tpbc));

	resuming = 1;

finish_state:

	NXPRT(fprintf(stderr, "finish_state:\n"));

	if (is_final) {
		if (used_out)
			goto write_state;  
		else if (used_in < 8) {
			 
			rc = -1;
			goto err4;
		} else {
			 
			int i;
			unsigned char tail[8];
			uint32_t cksum, isize;

			for (i = 0; i < 8; i++)
				tail[i] = fifo_in[(cur_in + i) % fifo_in_len];
			fprintf(stderr, "computed checksum %08x isize %08x\n",
				cmdp->cpb.out_crc, (uint32_t) (total_out
				% (1ULL<<32)));
			cksum = ((uint32_t) tail[0] | (uint32_t) tail[1]<<8
				 | (uint32_t) tail[2]<<16
				 | (uint32_t) tail[3]<<24);
			isize = ((uint32_t) tail[4] | (uint32_t) tail[5]<<8
				 | (uint32_t) tail[6]<<16
				 | (uint32_t) tail[7]<<24);
			fprintf(stderr, "stored   checksum %08x isize %08x\n",
				cksum, isize);

			if (cksum == cmdp->cpb.out_crc && isize == (uint32_t)
			    (total_out % (1ULL<<32))) {
				rc = 0;	goto ok1;
			} else {
				rc = -1; goto err4;
			}
		}
	} else
		goto read_state;

	return -1;

err1:
	fprintf(stderr, "error: not a gzip file, expect %x, read %x\n",
		expect, c);
	return -1;

err2:
	fprintf(stderr, "error: the FLG byte is wrong or not being handled\n");
	return -1;

err3:
	fprintf(stderr, "error: gzip header\n");
	return -1;

err4:
	fprintf(stderr, "error: checksum missing or mismatch\n");

err5:
ok1:
	fprintf(stderr, "decomp is complete: fclose\n");
	fclose(outf);

	return rc;
}


int main(int argc, char **argv)
{
	int rc;
	struct sigaction act;
	void *handle;

	nx_dbg = 0;
	nx_gzip_log = NULL;
	act.sa_handler = 0;
	act.sa_sigaction = nxu_sigsegv_handler;
	act.sa_flags = SA_SIGINFO;
	act.sa_restorer = 0;
	sigemptyset(&act.sa_mask);
	sigaction(SIGSEGV, &act, NULL);

	handle = nx_function_begin(NX_FUNC_COMP_GZIP, 0);
	if (!handle) {
		fprintf(stderr, "Unable to init NX, errno %d\n", errno);
		exit(-1);
	}

	rc = decompress_file(argc, argv, handle);

	nx_function_end(handle);

	return rc;
}
