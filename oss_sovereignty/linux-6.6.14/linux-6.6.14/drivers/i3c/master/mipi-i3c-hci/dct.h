#ifndef DCT_H
#define DCT_H
void i3c_hci_dct_get_val(struct i3c_hci *hci, unsigned int dct_idx,
			 u64 *pid, unsigned int *dcr, unsigned int *bcr);
#endif
