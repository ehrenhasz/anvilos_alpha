

 

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
#include "utils.h"
#include "nxu.h"
#include "nx.h"

int nx_dbg;
FILE *nx_gzip_log;

#define NX_MIN(X, Y) (((X) < (Y)) ? (X) : (Y))
#define FNAME_MAX 1024
#define FEXT ".nx.gz"

#define SYSFS_MAX_REQ_BUF_PATH "devices/vio/ibm,compression-v1/nx_gzip_caps/req_max_processed_len"

 
static int compress_fht_sample(char *src, uint32_t srclen, char *dst,
				uint32_t dstlen, int with_count,
				struct nx_gzip_crb_cpb_t *cmdp, void *handle)
{
	uint32_t fc;

	assert(!!cmdp);

	put32(cmdp->crb, gzip_fc, 0);   
	fc = (with_count) ? GZIP_FC_COMPRESS_RESUME_FHT_COUNT :
			    GZIP_FC_COMPRESS_RESUME_FHT;
	putnn(cmdp->crb, gzip_fc, fc);
	putnn(cmdp->cpb, in_histlen, 0);  
	memset((void *) &cmdp->crb.csb, 0, sizeof(cmdp->crb.csb));

	 
	if (!with_count)
		put32(cmdp->cpb, out_spbc_comp, 0);
	else
		put32(cmdp->cpb, out_spbc_comp_with_count, 0);

	 
	put64(cmdp->crb, csb_address, 0);
	put64(cmdp->crb, csb_address,
	      (uint64_t) &cmdp->crb.csb & csb_address_mask);

	 
	clear_dde(cmdp->crb.source_dde);
	putnn(cmdp->crb.source_dde, dde_count, 0);
	put32(cmdp->crb.source_dde, ddebc, srclen);
	put64(cmdp->crb.source_dde, ddead, (uint64_t) src);

	 
	clear_dde(cmdp->crb.target_dde);
	putnn(cmdp->crb.target_dde, dde_count, 0);
	put32(cmdp->crb.target_dde, ddebc, dstlen);
	put64(cmdp->crb.target_dde, ddead, (uint64_t) dst);

	 
	return nxu_submit_job(cmdp, handle);
}

 
int gzip_header_blank(char *buf)
{
	int i = 0;

	buf[i++] = 0x1f;  
	buf[i++] = 0x8b;  
	buf[i++] = 0x08;  
	buf[i++] = 0x00;  
	buf[i++] = 0x00;  
	buf[i++] = 0x00;  
	buf[i++] = 0x00;  
	buf[i++] = 0x00;  
	buf[i++] = 0x04;  
	buf[i++] = 0x03;  

	return i;
}

 
int append_sync_flush(char *buf, int tebc, int final)
{
	uint64_t flush;
	int shift = (tebc & 0x7);

	if (tebc > 0) {
		 
		buf = buf - 1;
		*buf = *buf & (unsigned char) ((1<<tebc)-1);
	} else
		*buf = 0;
	flush = ((0x1ULL & final) << shift) | *buf;
	shift = shift + 3;  
	shift = (shift <= 8) ? 8 : 16;
	flush |= (0xFFFF0000ULL) << shift;  
	shift = shift + 32;
	while (shift > 0) {
		*buf++ = (unsigned char) (flush & 0xffULL);
		flush = flush >> 8;
		shift = shift - 8;
	}
	return(((tebc > 5) || (tebc == 0)) ? 5 : 4);
}

 
static void set_bfinal(void *buf, int bfinal)
{
	char *b = buf;

	if (bfinal)
		*b = *b | (unsigned char) 0x01;
	else
		*b = *b & (unsigned char) 0xfe;
}

int compress_file(int argc, char **argv, void *handle)
{
	char *inbuf, *outbuf, *srcbuf, *dstbuf;
	char outname[FNAME_MAX];
	uint32_t srclen, dstlen;
	uint32_t flushlen, chunk;
	size_t inlen, outlen, dsttotlen, srctotlen;
	uint32_t crc, spbc, tpbc, tebc;
	int lzcounts = 0;
	int cc;
	int num_hdr_bytes;
	struct nx_gzip_crb_cpb_t *cmdp;
	uint32_t pagelen = 65536;
	int fault_tries = NX_MAX_FAULTS;
	char buf[32];

	cmdp = (void *)(uintptr_t)
		aligned_alloc(sizeof(struct nx_gzip_crb_cpb_t),
			      sizeof(struct nx_gzip_crb_cpb_t));

	if (argc != 2) {
		fprintf(stderr, "usage: %s <fname>\n", argv[0]);
		exit(-1);
	}
	if (read_file_alloc(argv[1], &inbuf, &inlen))
		exit(-1);
	fprintf(stderr, "file %s read, %ld bytes\n", argv[1], inlen);

	 
	outlen = 2 * inlen + 1024;

	assert(NULL != (outbuf = (char *)malloc(outlen)));
	nxu_touch_pages(outbuf, outlen, pagelen, 1);

	 
	if (!read_sysfs_file(SYSFS_MAX_REQ_BUF_PATH, buf, sizeof(buf))) {
		chunk = atoi(buf);
	} else {
		 
		 
		chunk = 1<<22;
	}

	 
	num_hdr_bytes = gzip_header_blank(outbuf);
	dstbuf    = outbuf + num_hdr_bytes;
	outlen    = outlen - num_hdr_bytes;
	dsttotlen = num_hdr_bytes;

	srcbuf    = inbuf;
	srctotlen = 0;

	 
	memset(&cmdp->crb, 0, sizeof(cmdp->crb));

	 
	put32(cmdp->cpb, in_crc, 0);

	while (inlen > 0) {

		 
		srclen = NX_MIN(chunk, inlen);
		 
		dstlen = NX_MIN(2*srclen, outlen);

		 

		 
		nxu_touch_pages(cmdp, sizeof(struct nx_gzip_crb_cpb_t), pagelen,
				1);
		nxu_touch_pages(srcbuf, srclen, pagelen, 0);
		nxu_touch_pages(dstbuf, dstlen, pagelen, 1);

		cc = compress_fht_sample(
			srcbuf, srclen,
			dstbuf, dstlen,
			lzcounts, cmdp, handle);

		if (cc != ERR_NX_OK && cc != ERR_NX_TPBC_GT_SPBC &&
		    cc != ERR_NX_AT_FAULT) {
			fprintf(stderr, "nx error: cc= %d\n", cc);
			exit(-1);
		}

		 
		if (cc == ERR_NX_AT_FAULT) {
			NXPRT(fprintf(stderr, "page fault: cc= %d, ", cc));
			NXPRT(fprintf(stderr, "try= %d, fsa= %08llx\n",
				  fault_tries,
				  (unsigned long long) cmdp->crb.csb.fsaddr));
			fault_tries--;
			if (fault_tries > 0) {
				continue;
			} else {
				fprintf(stderr, "error: cannot progress; ");
				fprintf(stderr, "too many faults\n");
				exit(-1);
			}
		}

		fault_tries = NX_MAX_FAULTS;  

		inlen     = inlen - srclen;
		srcbuf    = srcbuf + srclen;
		srctotlen = srctotlen + srclen;

		 
		spbc = (!lzcounts) ? get32(cmdp->cpb, out_spbc_comp) :
			get32(cmdp->cpb, out_spbc_comp_with_count);
		assert(spbc == srclen);

		 
		tpbc = get32(cmdp->crb.csb, tpbc);
		 
		tebc = getnn(cmdp->cpb, out_tebc);
		NXPRT(fprintf(stderr, "compressed chunk %d ", spbc));
		NXPRT(fprintf(stderr, "to %d bytes, tebc= %d\n", tpbc, tebc));

		if (inlen > 0) {  
			set_bfinal(dstbuf, 0);
			dstbuf    = dstbuf + tpbc;
			dsttotlen = dsttotlen + tpbc;
			outlen    = outlen - tpbc;
			 
			flushlen  = append_sync_flush(dstbuf, tebc, 0);
			dsttotlen = dsttotlen + flushlen;
			outlen    = outlen - flushlen;
			dstbuf    = dstbuf + flushlen;
			NXPRT(fprintf(stderr, "added sync_flush %d bytes\n",
					flushlen));
		} else {   
			 
			set_bfinal(dstbuf, 1);
			dstbuf    = dstbuf + tpbc;
			dsttotlen = dsttotlen + tpbc;
			outlen    = outlen - tpbc;
		}

		 
		crc = get32(cmdp->cpb, out_crc);
		put32(cmdp->cpb, in_crc, crc);
		crc = be32toh(crc);
	}

	 
	memcpy(dstbuf, &crc, 4);
	memcpy(dstbuf+4, &srctotlen, 4);
	dsttotlen = dsttotlen + 8;
	outlen    = outlen - 8;

	assert(FNAME_MAX > (strlen(argv[1]) + strlen(FEXT)));
	strcpy(outname, argv[1]);
	strcat(outname, FEXT);
	if (write_file(outname, outbuf, dsttotlen)) {
		fprintf(stderr, "write error: %s\n", outname);
		exit(-1);
	}

	fprintf(stderr, "compressed %ld to %ld bytes total, ", srctotlen,
		dsttotlen);
	fprintf(stderr, "crc32 checksum = %08x\n", crc);

	if (inbuf != NULL)
		free(inbuf);

	if (outbuf != NULL)
		free(outbuf);

	return 0;
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

	rc = compress_file(argc, argv, handle);

	nx_function_end(handle);

	return rc;
}
