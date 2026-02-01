 

 

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/nvpair.h>
#include <sys/fs/zfs.h>

#include <libzutil.h>

static void
dump_ddt_stat(const ddt_stat_t *dds, int h)
{
	char refcnt[6];
	char blocks[6], lsize[6], psize[6], dsize[6];
	char ref_blocks[6], ref_lsize[6], ref_psize[6], ref_dsize[6];

	if (dds == NULL || dds->dds_blocks == 0)
		return;

	if (h == -1)
		(void) strcpy(refcnt, "Total");
	else
		zfs_nicenum(1ULL << h, refcnt, sizeof (refcnt));

	zfs_nicenum(dds->dds_blocks, blocks, sizeof (blocks));
	zfs_nicebytes(dds->dds_lsize, lsize, sizeof (lsize));
	zfs_nicebytes(dds->dds_psize, psize, sizeof (psize));
	zfs_nicebytes(dds->dds_dsize, dsize, sizeof (dsize));
	zfs_nicenum(dds->dds_ref_blocks, ref_blocks, sizeof (ref_blocks));
	zfs_nicebytes(dds->dds_ref_lsize, ref_lsize, sizeof (ref_lsize));
	zfs_nicebytes(dds->dds_ref_psize, ref_psize, sizeof (ref_psize));
	zfs_nicebytes(dds->dds_ref_dsize, ref_dsize, sizeof (ref_dsize));

	(void) printf("%6s   %6s   %5s   %5s   %5s   %6s   %5s   %5s   %5s\n",
	    refcnt,
	    blocks, lsize, psize, dsize,
	    ref_blocks, ref_lsize, ref_psize, ref_dsize);
}

 
void
zpool_dump_ddt(const ddt_stat_t *dds_total, const ddt_histogram_t *ddh)
{
	int h;

	(void) printf("\n");

	(void) printf("bucket   "
	    "           allocated             "
	    "          referenced          \n");
	(void) printf("______   "
	    "______________________________   "
	    "______________________________\n");

	(void) printf("%6s   %6s   %5s   %5s   %5s   %6s   %5s   %5s   %5s\n",
	    "refcnt",
	    "blocks", "LSIZE", "PSIZE", "DSIZE",
	    "blocks", "LSIZE", "PSIZE", "DSIZE");

	(void) printf("%6s   %6s   %5s   %5s   %5s   %6s   %5s   %5s   %5s\n",
	    "------",
	    "------", "-----", "-----", "-----",
	    "------", "-----", "-----", "-----");

	for (h = 0; h < 64; h++)
		dump_ddt_stat(&ddh->ddh_stat[h], h);

	dump_ddt_stat(dds_total, -1);

	(void) printf("\n");
}

 
int
zpool_history_unpack(char *buf, uint64_t bytes_read, uint64_t *leftover,
    nvlist_t ***records, uint_t *numrecords)
{
	uint64_t reclen;
	nvlist_t *nv;
	int i;
	void *tmp;

	while (bytes_read > sizeof (reclen)) {

		 
		for (i = 0, reclen = 0; i < sizeof (reclen); i++)
			reclen += (uint64_t)(((uchar_t *)buf)[i]) << (8*i);

		if (bytes_read < sizeof (reclen) + reclen)
			break;

		 
		int err = nvlist_unpack(buf + sizeof (reclen), reclen, &nv, 0);
		if (err != 0)
			return (err);
		bytes_read -= sizeof (reclen) + reclen;
		buf += sizeof (reclen) + reclen;

		 
		(*numrecords)++;
		if (ISP2(*numrecords + 1)) {
			tmp = realloc(*records,
			    *numrecords * 2 * sizeof (nvlist_t *));
			if (tmp == NULL) {
				nvlist_free(nv);
				(*numrecords)--;
				return (ENOMEM);
			}
			*records = tmp;
		}
		(*records)[*numrecords - 1] = nv;
	}

	*leftover = bytes_read;
	return (0);
}
