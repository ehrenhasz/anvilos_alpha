
 

#include "mpi-internal.h"

int mpi_sub_ui(MPI w, MPI u, unsigned long vval)
{
	if (u->nlimbs == 0) {
		if (mpi_resize(w, 1) < 0)
			return -ENOMEM;
		w->d[0] = vval;
		w->nlimbs = (vval != 0);
		w->sign = (vval != 0);
		return 0;
	}

	 
	if (mpi_resize(w, u->nlimbs + 1))
		return -ENOMEM;

	if (u->sign) {
		mpi_limb_t cy;

		cy = mpihelp_add_1(w->d, u->d, u->nlimbs, (mpi_limb_t) vval);
		w->d[u->nlimbs] = cy;
		w->nlimbs = u->nlimbs + cy;
		w->sign = 1;
	} else {
		 
		if (u->nlimbs == 1 && u->d[0] < vval) {
			w->d[0] = vval - u->d[0];
			w->nlimbs = 1;
			w->sign = 1;
		} else {
			mpihelp_sub_1(w->d, u->d, u->nlimbs, (mpi_limb_t) vval);
			 
			w->nlimbs = (u->nlimbs - (w->d[u->nlimbs - 1] == 0));
			w->sign = 0;
		}
	}

	mpi_normalize(w);
	return 0;
}
EXPORT_SYMBOL_GPL(mpi_sub_ui);
