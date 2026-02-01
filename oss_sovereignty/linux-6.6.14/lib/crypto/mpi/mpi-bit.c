 

#include "mpi-internal.h"
#include "longlong.h"

#define A_LIMB_1 ((mpi_limb_t) 1)

 
void mpi_normalize(MPI a)
{
	for (; a->nlimbs && !a->d[a->nlimbs - 1]; a->nlimbs--)
		;
}
EXPORT_SYMBOL_GPL(mpi_normalize);

 
unsigned mpi_get_nbits(MPI a)
{
	unsigned n;

	mpi_normalize(a);

	if (a->nlimbs) {
		mpi_limb_t alimb = a->d[a->nlimbs - 1];
		if (alimb)
			n = count_leading_zeros(alimb);
		else
			n = BITS_PER_MPI_LIMB;
		n = BITS_PER_MPI_LIMB - n + (a->nlimbs - 1) * BITS_PER_MPI_LIMB;
	} else
		n = 0;
	return n;
}
EXPORT_SYMBOL_GPL(mpi_get_nbits);

 
int mpi_test_bit(MPI a, unsigned int n)
{
	unsigned int limbno, bitno;
	mpi_limb_t limb;

	limbno = n / BITS_PER_MPI_LIMB;
	bitno  = n % BITS_PER_MPI_LIMB;

	if (limbno >= a->nlimbs)
		return 0;  
	limb = a->d[limbno];
	return (limb & (A_LIMB_1 << bitno)) ? 1 : 0;
}
EXPORT_SYMBOL_GPL(mpi_test_bit);

 
void mpi_set_bit(MPI a, unsigned int n)
{
	unsigned int i, limbno, bitno;

	limbno = n / BITS_PER_MPI_LIMB;
	bitno  = n % BITS_PER_MPI_LIMB;

	if (limbno >= a->nlimbs) {
		for (i = a->nlimbs; i < a->alloced; i++)
			a->d[i] = 0;
		mpi_resize(a, limbno+1);
		a->nlimbs = limbno+1;
	}
	a->d[limbno] |= (A_LIMB_1<<bitno);
}

 
void mpi_set_highbit(MPI a, unsigned int n)
{
	unsigned int i, limbno, bitno;

	limbno = n / BITS_PER_MPI_LIMB;
	bitno  = n % BITS_PER_MPI_LIMB;

	if (limbno >= a->nlimbs) {
		for (i = a->nlimbs; i < a->alloced; i++)
			a->d[i] = 0;
		mpi_resize(a, limbno+1);
		a->nlimbs = limbno+1;
	}
	a->d[limbno] |= (A_LIMB_1<<bitno);
	for (bitno++; bitno < BITS_PER_MPI_LIMB; bitno++)
		a->d[limbno] &= ~(A_LIMB_1 << bitno);
	a->nlimbs = limbno+1;
}
EXPORT_SYMBOL_GPL(mpi_set_highbit);

 
void mpi_clear_highbit(MPI a, unsigned int n)
{
	unsigned int limbno, bitno;

	limbno = n / BITS_PER_MPI_LIMB;
	bitno  = n % BITS_PER_MPI_LIMB;

	if (limbno >= a->nlimbs)
		return;  

	for ( ; bitno < BITS_PER_MPI_LIMB; bitno++)
		a->d[limbno] &= ~(A_LIMB_1 << bitno);
	a->nlimbs = limbno+1;
}

 
void mpi_clear_bit(MPI a, unsigned int n)
{
	unsigned int limbno, bitno;

	limbno = n / BITS_PER_MPI_LIMB;
	bitno  = n % BITS_PER_MPI_LIMB;

	if (limbno >= a->nlimbs)
		return;  
	a->d[limbno] &= ~(A_LIMB_1 << bitno);
}
EXPORT_SYMBOL_GPL(mpi_clear_bit);


 
void mpi_rshift_limbs(MPI a, unsigned int count)
{
	mpi_ptr_t ap = a->d;
	mpi_size_t n = a->nlimbs;
	unsigned int i;

	if (count >= n) {
		a->nlimbs = 0;
		return;
	}

	for (i = 0; i < n - count; i++)
		ap[i] = ap[i+count];
	ap[i] = 0;
	a->nlimbs -= count;
}

 
void mpi_rshift(MPI x, MPI a, unsigned int n)
{
	mpi_size_t xsize;
	unsigned int i;
	unsigned int nlimbs = (n/BITS_PER_MPI_LIMB);
	unsigned int nbits = (n%BITS_PER_MPI_LIMB);

	if (x == a) {
		 
		if (nlimbs >= x->nlimbs) {
			x->nlimbs = 0;
			return;
		}

		if (nlimbs) {
			for (i = 0; i < x->nlimbs - nlimbs; i++)
				x->d[i] = x->d[i+nlimbs];
			x->d[i] = 0;
			x->nlimbs -= nlimbs;
		}
		if (x->nlimbs && nbits)
			mpihelp_rshift(x->d, x->d, x->nlimbs, nbits);
	} else if (nlimbs) {
		 
		xsize = a->nlimbs;
		x->sign = a->sign;
		RESIZE_IF_NEEDED(x, xsize);
		x->nlimbs = xsize;
		for (i = 0; i < a->nlimbs; i++)
			x->d[i] = a->d[i];
		x->nlimbs = i;

		if (nlimbs >= x->nlimbs) {
			x->nlimbs = 0;
			return;
		}

		if (nlimbs) {
			for (i = 0; i < x->nlimbs - nlimbs; i++)
				x->d[i] = x->d[i+nlimbs];
			x->d[i] = 0;
			x->nlimbs -= nlimbs;
		}

		if (x->nlimbs && nbits)
			mpihelp_rshift(x->d, x->d, x->nlimbs, nbits);
	} else {
		 
		xsize = a->nlimbs;
		x->sign = a->sign;
		RESIZE_IF_NEEDED(x, xsize);
		x->nlimbs = xsize;

		if (xsize) {
			if (nbits)
				mpihelp_rshift(x->d, a->d, x->nlimbs, nbits);
			else {
				 
				for (i = 0; i < x->nlimbs; i++)
					x->d[i] = a->d[i];
			}
		}
	}
	MPN_NORMALIZE(x->d, x->nlimbs);
}
EXPORT_SYMBOL_GPL(mpi_rshift);

 
void mpi_lshift_limbs(MPI a, unsigned int count)
{
	mpi_ptr_t ap;
	int n = a->nlimbs;
	int i;

	if (!count || !n)
		return;

	RESIZE_IF_NEEDED(a, n+count);

	ap = a->d;
	for (i = n-1; i >= 0; i--)
		ap[i+count] = ap[i];
	for (i = 0; i < count; i++)
		ap[i] = 0;
	a->nlimbs += count;
}

 
void mpi_lshift(MPI x, MPI a, unsigned int n)
{
	unsigned int nlimbs = (n/BITS_PER_MPI_LIMB);
	unsigned int nbits = (n%BITS_PER_MPI_LIMB);

	if (x == a && !n)
		return;   

	if (x != a) {
		 
		unsigned int alimbs = a->nlimbs;
		int asign = a->sign;
		mpi_ptr_t xp, ap;

		RESIZE_IF_NEEDED(x, alimbs+nlimbs+1);
		xp = x->d;
		ap = a->d;
		MPN_COPY(xp, ap, alimbs);
		x->nlimbs = alimbs;
		x->flags = a->flags;
		x->sign = asign;
	}

	if (nlimbs && !nbits) {
		 
		mpi_lshift_limbs(x, nlimbs);
	} else if (n) {
		 
		mpi_lshift_limbs(x, nlimbs+1);
		mpi_rshift(x, x, BITS_PER_MPI_LIMB - nbits);
	}

	MPN_NORMALIZE(x->d, x->nlimbs);
}
