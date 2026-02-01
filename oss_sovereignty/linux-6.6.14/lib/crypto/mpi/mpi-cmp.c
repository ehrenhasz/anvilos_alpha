 

#include "mpi-internal.h"

int mpi_cmp_ui(MPI u, unsigned long v)
{
	mpi_limb_t limb = v;

	mpi_normalize(u);
	if (u->nlimbs == 0) {
		if (v == 0)
			return 0;
		else
			return -1;
	}
	if (u->sign)
		return -1;
	if (u->nlimbs > 1)
		return 1;

	if (u->d[0] == limb)
		return 0;
	else if (u->d[0] > limb)
		return 1;
	else
		return -1;
}
EXPORT_SYMBOL_GPL(mpi_cmp_ui);

static int do_mpi_cmp(MPI u, MPI v, int absmode)
{
	mpi_size_t usize;
	mpi_size_t vsize;
	int usign;
	int vsign;
	int cmp;

	mpi_normalize(u);
	mpi_normalize(v);

	usize = u->nlimbs;
	vsize = v->nlimbs;
	usign = absmode ? 0 : u->sign;
	vsign = absmode ? 0 : v->sign;

	 

	if (!usign && vsign)
		return 1;
	if (usign && !vsign)
		return -1;

	 

	if (usize != vsize && !usign && !vsign)
		return usize - vsize;
	if (usize != vsize && usign && vsign)
		return vsize + usize;
	if (!usize)
		return 0;
	cmp = mpihelp_cmp(u->d, v->d, usize);
	if (!cmp)
		return 0;
	if ((cmp < 0?1:0) == (usign?1:0))
		return 1;

	return -1;
}

int mpi_cmp(MPI u, MPI v)
{
	return do_mpi_cmp(u, v, 0);
}
EXPORT_SYMBOL_GPL(mpi_cmp);

int mpi_cmpabs(MPI u, MPI v)
{
	return do_mpi_cmp(u, v, 1);
}
EXPORT_SYMBOL_GPL(mpi_cmpabs);
