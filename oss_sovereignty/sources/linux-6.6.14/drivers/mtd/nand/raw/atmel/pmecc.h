


#ifndef ATMEL_PMECC_H
#define ATMEL_PMECC_H

#define ATMEL_PMECC_MAXIMIZE_ECC_STRENGTH	0
#define ATMEL_PMECC_SECTOR_SIZE_AUTO		0
#define ATMEL_PMECC_OOBOFFSET_AUTO		-1

struct atmel_pmecc_user_req {
	int pagesize;
	int oobsize;
	struct {
		int strength;
		int bytes;
		int sectorsize;
		int nsectors;
		int ooboffset;
	} ecc;
};

struct atmel_pmecc *devm_atmel_pmecc_get(struct device *dev);

struct atmel_pmecc_user *
atmel_pmecc_create_user(struct atmel_pmecc *pmecc,
			struct atmel_pmecc_user_req *req);
void atmel_pmecc_destroy_user(struct atmel_pmecc_user *user);

void atmel_pmecc_reset(struct atmel_pmecc *pmecc);
int atmel_pmecc_enable(struct atmel_pmecc_user *user, int op);
void atmel_pmecc_disable(struct atmel_pmecc_user *user);
int atmel_pmecc_wait_rdy(struct atmel_pmecc_user *user);
int atmel_pmecc_correct_sector(struct atmel_pmecc_user *user, int sector,
			       void *data, void *ecc);
bool atmel_pmecc_correct_erased_chunks(struct atmel_pmecc_user *user);
void atmel_pmecc_get_generated_eccbytes(struct atmel_pmecc_user *user,
					int sector, void *ecc);

#endif 
