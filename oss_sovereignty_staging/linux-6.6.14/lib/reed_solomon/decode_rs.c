
 
{
	struct rs_codec *rs = rsc->codec;
	int deg_lambda, el, deg_omega;
	int i, j, r, k, pad;
	int nn = rs->nn;
	int nroots = rs->nroots;
	int fcr = rs->fcr;
	int prim = rs->prim;
	int iprim = rs->iprim;
	uint16_t *alpha_to = rs->alpha_to;
	uint16_t *index_of = rs->index_of;
	uint16_t u, q, tmp, num1, num2, den, discr_r, syn_error;
	int count = 0;
	int num_corrected;
	uint16_t msk = (uint16_t) rs->nn;

	 
	uint16_t *lambda = rsc->buffers + RS_DECODE_LAMBDA * (nroots + 1);
	uint16_t *syn = rsc->buffers + RS_DECODE_SYN * (nroots + 1);
	uint16_t *b = rsc->buffers + RS_DECODE_B * (nroots + 1);
	uint16_t *t = rsc->buffers + RS_DECODE_T * (nroots + 1);
	uint16_t *omega = rsc->buffers + RS_DECODE_OMEGA * (nroots + 1);
	uint16_t *root = rsc->buffers + RS_DECODE_ROOT * (nroots + 1);
	uint16_t *reg = rsc->buffers + RS_DECODE_REG * (nroots + 1);
	uint16_t *loc = rsc->buffers + RS_DECODE_LOC * (nroots + 1);

	 
	pad = nn - nroots - len;
	BUG_ON(pad < 0 || pad >= nn - nroots);

	 
	if (s != NULL) {
		for (i = 0; i < nroots; i++) {
			 
			if (s[i] != nn)
				goto decode;
		}

		 
		return 0;
	}

	 
	for (i = 0; i < nroots; i++)
		syn[i] = (((uint16_t) data[0]) ^ invmsk) & msk;

	for (j = 1; j < len; j++) {
		for (i = 0; i < nroots; i++) {
			if (syn[i] == 0) {
				syn[i] = (((uint16_t) data[j]) ^
					  invmsk) & msk;
			} else {
				syn[i] = ((((uint16_t) data[j]) ^
					   invmsk) & msk) ^
					alpha_to[rs_modnn(rs, index_of[syn[i]] +
						       (fcr + i) * prim)];
			}
		}
	}

	for (j = 0; j < nroots; j++) {
		for (i = 0; i < nroots; i++) {
			if (syn[i] == 0) {
				syn[i] = ((uint16_t) par[j]) & msk;
			} else {
				syn[i] = (((uint16_t) par[j]) & msk) ^
					alpha_to[rs_modnn(rs, index_of[syn[i]] +
						       (fcr+i)*prim)];
			}
		}
	}
	s = syn;

	 
	syn_error = 0;
	for (i = 0; i < nroots; i++) {
		syn_error |= s[i];
		s[i] = index_of[s[i]];
	}

	if (!syn_error) {
		 
		return 0;
	}

 decode:
	memset(&lambda[1], 0, nroots * sizeof(lambda[0]));
	lambda[0] = 1;

	if (no_eras > 0) {
		 
		lambda[1] = alpha_to[rs_modnn(rs,
					prim * (nn - 1 - (eras_pos[0] + pad)))];
		for (i = 1; i < no_eras; i++) {
			u = rs_modnn(rs, prim * (nn - 1 - (eras_pos[i] + pad)));
			for (j = i + 1; j > 0; j--) {
				tmp = index_of[lambda[j - 1]];
				if (tmp != nn) {
					lambda[j] ^=
						alpha_to[rs_modnn(rs, u + tmp)];
				}
			}
		}
	}

	for (i = 0; i < nroots + 1; i++)
		b[i] = index_of[lambda[i]];

	 
	r = no_eras;
	el = no_eras;
	while (++r <= nroots) {	 
		 
		discr_r = 0;
		for (i = 0; i < r; i++) {
			if ((lambda[i] != 0) && (s[r - i - 1] != nn)) {
				discr_r ^=
					alpha_to[rs_modnn(rs,
							  index_of[lambda[i]] +
							  s[r - i - 1])];
			}
		}
		discr_r = index_of[discr_r];	 
		if (discr_r == nn) {
			 
			memmove (&b[1], b, nroots * sizeof (b[0]));
			b[0] = nn;
		} else {
			 
			t[0] = lambda[0];
			for (i = 0; i < nroots; i++) {
				if (b[i] != nn) {
					t[i + 1] = lambda[i + 1] ^
						alpha_to[rs_modnn(rs, discr_r +
								  b[i])];
				} else
					t[i + 1] = lambda[i + 1];
			}
			if (2 * el <= r + no_eras - 1) {
				el = r + no_eras - el;
				 
				for (i = 0; i <= nroots; i++) {
					b[i] = (lambda[i] == 0) ? nn :
						rs_modnn(rs, index_of[lambda[i]]
							 - discr_r + nn);
				}
			} else {
				 
				memmove(&b[1], b, nroots * sizeof(b[0]));
				b[0] = nn;
			}
			memcpy(lambda, t, (nroots + 1) * sizeof(t[0]));
		}
	}

	 
	deg_lambda = 0;
	for (i = 0; i < nroots + 1; i++) {
		lambda[i] = index_of[lambda[i]];
		if (lambda[i] != nn)
			deg_lambda = i;
	}

	if (deg_lambda == 0) {
		 
		return -EBADMSG;
	}

	 
	memcpy(&reg[1], &lambda[1], nroots * sizeof(reg[0]));
	count = 0;		 
	for (i = 1, k = iprim - 1; i <= nn; i++, k = rs_modnn(rs, k + iprim)) {
		q = 1;		 
		for (j = deg_lambda; j > 0; j--) {
			if (reg[j] != nn) {
				reg[j] = rs_modnn(rs, reg[j] + j);
				q ^= alpha_to[reg[j]];
			}
		}
		if (q != 0)
			continue;	 

		if (k < pad) {
			 
			return -EBADMSG;
		}

		 
		root[count] = i;
		loc[count] = k;
		 
		if (++count == deg_lambda)
			break;
	}
	if (deg_lambda != count) {
		 
		return -EBADMSG;
	}
	 
	deg_omega = deg_lambda - 1;
	for (i = 0; i <= deg_omega; i++) {
		tmp = 0;
		for (j = i; j >= 0; j--) {
			if ((s[i - j] != nn) && (lambda[j] != nn))
				tmp ^=
				    alpha_to[rs_modnn(rs, s[i - j] + lambda[j])];
		}
		omega[i] = index_of[tmp];
	}

	 
	num_corrected = 0;
	for (j = count - 1; j >= 0; j--) {
		num1 = 0;
		for (i = deg_omega; i >= 0; i--) {
			if (omega[i] != nn)
				num1 ^= alpha_to[rs_modnn(rs, omega[i] +
							i * root[j])];
		}

		if (num1 == 0) {
			 
			b[j] = 0;
			continue;
		}

		num2 = alpha_to[rs_modnn(rs, root[j] * (fcr - 1) + nn)];
		den = 0;

		 
		for (i = min(deg_lambda, nroots - 1) & ~1; i >= 0; i -= 2) {
			if (lambda[i + 1] != nn) {
				den ^= alpha_to[rs_modnn(rs, lambda[i + 1] +
						       i * root[j])];
			}
		}

		b[j] = alpha_to[rs_modnn(rs, index_of[num1] +
					       index_of[num2] +
					       nn - index_of[den])];
		num_corrected++;
	}

	 
	for (i = 0; i < nroots; i++) {
		tmp = 0;
		for (j = 0; j < count; j++) {
			if (b[j] == 0)
				continue;

			k = (fcr + i) * prim * (nn-loc[j]-1);
			tmp ^= alpha_to[rs_modnn(rs, index_of[b[j]] + k)];
		}

		if (tmp != alpha_to[s[i]])
			return -EBADMSG;
	}

	 
	if (corr && eras_pos) {
		j = 0;
		for (i = 0; i < count; i++) {
			if (b[i]) {
				corr[j] = b[i];
				eras_pos[j++] = loc[i] - pad;
			}
		}
	} else if (data && par) {
		 
		for (i = 0; i < count; i++) {
			if (loc[i] < (nn - nroots))
				data[loc[i] - pad] ^= b[i];
			else
				par[loc[i] - pad - len] ^= b[i];
		}
	}

	return  num_corrected;
}
