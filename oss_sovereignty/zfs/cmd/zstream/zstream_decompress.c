#include <err.h>
#include <search.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/zfs_ioctl.h>
#include <sys/zio_checksum.h>
#include <sys/zstd/zstd.h>
#include "zfs_fletcher.h"
#include "zstream.h"
static int
dump_record(dmu_replay_record_t *drr, void *payload, int payload_len,
    zio_cksum_t *zc, int outfd)
{
	assert(offsetof(dmu_replay_record_t, drr_u.drr_checksum.drr_checksum)
	    == sizeof (dmu_replay_record_t) - sizeof (zio_cksum_t));
	fletcher_4_incremental_native(drr,
	    offsetof(dmu_replay_record_t, drr_u.drr_checksum.drr_checksum), zc);
	if (drr->drr_type != DRR_BEGIN) {
		assert(ZIO_CHECKSUM_IS_ZERO(&drr->drr_u.
		    drr_checksum.drr_checksum));
		drr->drr_u.drr_checksum.drr_checksum = *zc;
	}
	fletcher_4_incremental_native(&drr->drr_u.drr_checksum.drr_checksum,
	    sizeof (zio_cksum_t), zc);
	if (write(outfd, drr, sizeof (*drr)) == -1)
		return (errno);
	if (payload_len != 0) {
		fletcher_4_incremental_native(payload, payload_len, zc);
		if (write(outfd, payload, payload_len) == -1)
			return (errno);
	}
	return (0);
}
int
zstream_do_decompress(int argc, char *argv[])
{
	const int KEYSIZE = 64;
	int bufsz = SPA_MAXBLOCKSIZE;
	char *buf = safe_malloc(bufsz);
	dmu_replay_record_t thedrr;
	dmu_replay_record_t *drr = &thedrr;
	zio_cksum_t stream_cksum;
	int c;
	boolean_t verbose = B_FALSE;
	while ((c = getopt(argc, argv, "v")) != -1) {
		switch (c) {
		case 'v':
			verbose = B_TRUE;
			break;
		case '?':
			(void) fprintf(stderr, "invalid option '%c'\n",
			    optopt);
			zstream_usage();
			break;
		}
	}
	argc -= optind;
	argv += optind;
	if (argc < 0)
		zstream_usage();
	if (hcreate(argc) == 0)
		errx(1, "hcreate");
	for (int i = 0; i < argc; i++) {
		uint64_t object, offset;
		char *obj_str;
		char *offset_str;
		char *key;
		char *end;
		enum zio_compress type = ZIO_COMPRESS_LZ4;
		obj_str = strsep(&argv[i], ",");
		if (argv[i] == NULL) {
			zstream_usage();
			exit(2);
		}
		errno = 0;
		object = strtoull(obj_str, &end, 0);
		if (errno || *end != '\0')
			errx(1, "invalid value for object");
		offset_str = strsep(&argv[i], ",");
		offset = strtoull(offset_str, &end, 0);
		if (errno || *end != '\0')
			errx(1, "invalid value for offset");
		if (argv[i]) {
			if (0 == strcmp("off", argv[i]))
				type = ZIO_COMPRESS_OFF;
			else if (0 == strcmp("lz4", argv[i]))
				type = ZIO_COMPRESS_LZ4;
			else if (0 == strcmp("lzjb", argv[i]))
				type = ZIO_COMPRESS_LZJB;
			else if (0 == strcmp("gzip", argv[i]))
				type = ZIO_COMPRESS_GZIP_1;
			else if (0 == strcmp("zle", argv[i]))
				type = ZIO_COMPRESS_ZLE;
			else if (0 == strcmp("zstd", argv[i]))
				type = ZIO_COMPRESS_ZSTD;
			else {
				fprintf(stderr, "Invalid compression type %s.\n"
				    "Supported types are off, lz4, lzjb, gzip, "
				    "zle, and zstd\n",
				    argv[i]);
				exit(2);
			}
		}
		if (asprintf(&key, "%llu,%llu", (u_longlong_t)object,
		    (u_longlong_t)offset) < 0) {
			err(1, "asprintf");
		}
		ENTRY e = {.key = key};
		ENTRY *p;
		p = hsearch(e, ENTER);
		if (p == NULL)
			errx(1, "hsearch");
		p->data = (void*)(intptr_t)type;
	}
	if (isatty(STDIN_FILENO)) {
		(void) fprintf(stderr,
		    "Error: The send stream is a binary format "
		    "and can not be read from a\n"
		    "terminal.  Standard input must be redirected.\n");
		exit(1);
	}
	fletcher_4_init();
	int begin = 0;
	boolean_t seen = B_FALSE;
	while (sfread(drr, sizeof (*drr), stdin) != 0) {
		struct drr_write *drrw;
		uint64_t payload_size = 0;
		if (drr->drr_type != DRR_BEGIN) {
			memset(&drr->drr_u.drr_checksum.drr_checksum, 0,
			    sizeof (drr->drr_u.drr_checksum.drr_checksum));
		}
		switch (drr->drr_type) {
		case DRR_BEGIN:
		{
			ZIO_SET_CHECKSUM(&stream_cksum, 0, 0, 0, 0);
			VERIFY0(begin++);
			seen = B_TRUE;
			uint32_t sz = drr->drr_payloadlen;
			VERIFY3U(sz, <=, 1U << 28);
			if (sz != 0) {
				if (sz > bufsz) {
					buf = realloc(buf, sz);
					if (buf == NULL)
						err(1, "realloc");
					bufsz = sz;
				}
				(void) sfread(buf, sz, stdin);
			}
			payload_size = sz;
			break;
		}
		case DRR_END:
		{
			struct drr_end *drre = &drr->drr_u.drr_end;
			VERIFY3B(seen, ==, B_TRUE);
			begin--;
			if (!ZIO_CHECKSUM_IS_ZERO(&drre->drr_checksum))
				drre->drr_checksum = stream_cksum;
			break;
		}
		case DRR_OBJECT:
		{
			struct drr_object *drro = &drr->drr_u.drr_object;
			VERIFY3S(begin, ==, 1);
			if (drro->drr_bonuslen > 0) {
				payload_size = DRR_OBJECT_PAYLOAD_SIZE(drro);
				(void) sfread(buf, payload_size, stdin);
			}
			break;
		}
		case DRR_SPILL:
		{
			struct drr_spill *drrs = &drr->drr_u.drr_spill;
			VERIFY3S(begin, ==, 1);
			payload_size = DRR_SPILL_PAYLOAD_SIZE(drrs);
			(void) sfread(buf, payload_size, stdin);
			break;
		}
		case DRR_WRITE_BYREF:
			VERIFY3S(begin, ==, 1);
			fprintf(stderr,
			    "Deduplicated streams are not supported\n");
			exit(1);
			break;
		case DRR_WRITE:
		{
			VERIFY3S(begin, ==, 1);
			drrw = &thedrr.drr_u.drr_write;
			payload_size = DRR_WRITE_PAYLOAD_SIZE(drrw);
			ENTRY *p;
			char key[KEYSIZE];
			snprintf(key, KEYSIZE, "%llu,%llu",
			    (u_longlong_t)drrw->drr_object,
			    (u_longlong_t)drrw->drr_offset);
			ENTRY e = {.key = key};
			p = hsearch(e, FIND);
			if (p != NULL) {
				zio_decompress_func_t *xfunc = NULL;
				switch ((enum zio_compress)(intptr_t)p->data) {
				case ZIO_COMPRESS_OFF:
					xfunc = NULL;
					break;
				case ZIO_COMPRESS_LZJB:
					xfunc = lzjb_decompress;
					break;
				case ZIO_COMPRESS_GZIP_1:
					xfunc = gzip_decompress;
					break;
				case ZIO_COMPRESS_ZLE:
					xfunc = zle_decompress;
					break;
				case ZIO_COMPRESS_LZ4:
					xfunc = lz4_decompress_zfs;
					break;
				case ZIO_COMPRESS_ZSTD:
					xfunc = zfs_zstd_decompress;
					break;
				default:
					assert(B_FALSE);
				}
				char *lzbuf = safe_calloc(payload_size);
				(void) sfread(lzbuf, payload_size, stdin);
				if (xfunc == NULL) {
					memcpy(buf, lzbuf, payload_size);
					drrw->drr_compressiontype =
					    ZIO_COMPRESS_OFF;
					if (verbose)
						fprintf(stderr, "Resetting "
						    "compression type to off "
						    "for ino %llu offset "
						    "%llu\n",
						    (u_longlong_t)
						    drrw->drr_object,
						    (u_longlong_t)
						    drrw->drr_offset);
				} else if (0 != xfunc(lzbuf, buf,
				    payload_size, payload_size, 0)) {
					warnx("decompression failed for "
					    "ino %llu offset %llu",
					    (u_longlong_t)drrw->drr_object,
					    (u_longlong_t)drrw->drr_offset);
					memcpy(buf, lzbuf, payload_size);
				} else if (verbose) {
					drrw->drr_compressiontype =
					    ZIO_COMPRESS_OFF;
					fprintf(stderr, "successfully "
					    "decompressed ino %llu "
					    "offset %llu\n",
					    (u_longlong_t)drrw->drr_object,
					    (u_longlong_t)drrw->drr_offset);
				} else {
					drrw->drr_compressiontype =
					    ZIO_COMPRESS_OFF;
				}
				free(lzbuf);
			} else {
				(void) sfread(buf, payload_size, stdin);
			}
			break;
		}
		case DRR_WRITE_EMBEDDED:
		{
			VERIFY3S(begin, ==, 1);
			struct drr_write_embedded *drrwe =
			    &drr->drr_u.drr_write_embedded;
			payload_size =
			    P2ROUNDUP((uint64_t)drrwe->drr_psize, 8);
			(void) sfread(buf, payload_size, stdin);
			break;
		}
		case DRR_FREEOBJECTS:
		case DRR_FREE:
		case DRR_OBJECT_RANGE:
			VERIFY3S(begin, ==, 1);
			break;
		default:
			(void) fprintf(stderr, "INVALID record type 0x%x\n",
			    drr->drr_type);
			assert(B_FALSE);
		}
		if (feof(stdout)) {
			fprintf(stderr, "Error: unexpected end-of-file\n");
			exit(1);
		}
		if (ferror(stdout)) {
			fprintf(stderr, "Error while reading file: %s\n",
			    strerror(errno));
			exit(1);
		}
		if (drr->drr_type != DRR_BEGIN) {
			memset(&drr->drr_u.drr_checksum.drr_checksum, 0,
			    sizeof (drr->drr_u.drr_checksum.drr_checksum));
		}
		if (dump_record(drr, buf, payload_size,
		    &stream_cksum, STDOUT_FILENO) != 0)
			break;
		if (drr->drr_type == DRR_END) {
			ZIO_SET_CHECKSUM(&stream_cksum, 0, 0, 0, 0);
		}
	}
	free(buf);
	fletcher_4_fini();
	hdestroy();
	return (0);
}
