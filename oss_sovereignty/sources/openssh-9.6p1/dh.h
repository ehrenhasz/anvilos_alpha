


#ifndef DH_H
#define DH_H

#ifdef WITH_OPENSSL

struct dhgroup {
	int size;
	BIGNUM *g;
	BIGNUM *p;
};

DH	*choose_dh(int, int, int);
DH	*dh_new_group_asc(const char *, const char *);
DH	*dh_new_group(BIGNUM *, BIGNUM *);
DH	*dh_new_group1(void);
DH	*dh_new_group14(void);
DH	*dh_new_group16(void);
DH	*dh_new_group18(void);
DH	*dh_new_group_fallback(int);

int	 dh_gen_key(DH *, int);
int	 dh_pub_is_valid(const DH *, const BIGNUM *);

u_int	 dh_estimate(int);
void	 dh_set_moduli_file(const char *);


#define DH_GRP_MIN	2048
#define DH_GRP_MAX	8192


#define MODULI_TYPE_UNKNOWN		(0)
#define MODULI_TYPE_UNSTRUCTURED	(1)
#define MODULI_TYPE_SAFE		(2)
#define MODULI_TYPE_SCHNORR		(3)
#define MODULI_TYPE_SOPHIE_GERMAIN	(4)
#define MODULI_TYPE_STRONG		(5)


#define MODULI_TESTS_UNTESTED		(0x00)
#define MODULI_TESTS_COMPOSITE		(0x01)
#define MODULI_TESTS_SIEVE		(0x02)
#define MODULI_TESTS_MILLER_RABIN	(0x04)
#define MODULI_TESTS_JACOBI		(0x08)
#define MODULI_TESTS_ELLIPTIC		(0x10)

#endif 

#endif 
