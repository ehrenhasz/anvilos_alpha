
 

#include <linux/efi.h>
#include <asm/efi.h>

#include "efistub.h"

typedef union efi_rng_protocol efi_rng_protocol_t;

union efi_rng_protocol {
	struct {
		efi_status_t (__efiapi *get_info)(efi_rng_protocol_t *,
						  unsigned long *,
						  efi_guid_t *);
		efi_status_t (__efiapi *get_rng)(efi_rng_protocol_t *,
						 efi_guid_t *, unsigned long,
						 u8 *out);
	};
	struct {
		u32 get_info;
		u32 get_rng;
	} mixed_mode;
};

 
efi_status_t efi_get_random_bytes(unsigned long size, u8 *out)
{
	efi_guid_t rng_proto = EFI_RNG_PROTOCOL_GUID;
	efi_status_t status;
	efi_rng_protocol_t *rng = NULL;

	status = efi_bs_call(locate_protocol, &rng_proto, NULL, (void **)&rng);
	if (status != EFI_SUCCESS)
		return status;

	return efi_call_proto(rng, get_rng, NULL, size, out);
}

 
efi_status_t efi_random_get_seed(void)
{
	efi_guid_t rng_proto = EFI_RNG_PROTOCOL_GUID;
	efi_guid_t rng_algo_raw = EFI_RNG_ALGORITHM_RAW;
	efi_guid_t rng_table_guid = LINUX_EFI_RANDOM_SEED_TABLE_GUID;
	struct linux_efi_random_seed *prev_seed, *seed = NULL;
	int prev_seed_size = 0, seed_size = EFI_RANDOM_SEED_SIZE;
	unsigned long nv_seed_size = 0, offset = 0;
	efi_rng_protocol_t *rng = NULL;
	efi_status_t status;

	status = efi_bs_call(locate_protocol, &rng_proto, NULL, (void **)&rng);
	if (status != EFI_SUCCESS)
		seed_size = 0;

	
	get_efi_var(L"RandomSeed", &rng_table_guid, NULL, &nv_seed_size, NULL);
	if (!seed_size && !nv_seed_size)
		return status;

	seed_size += nv_seed_size;

	 
	prev_seed = get_efi_config_table(rng_table_guid);
	if (prev_seed && prev_seed->size <= 512U) {
		prev_seed_size = prev_seed->size;
		seed_size += prev_seed_size;
	}

	 
	status = efi_bs_call(allocate_pool, EFI_ACPI_RECLAIM_MEMORY,
			     struct_size(seed, bits, seed_size),
			     (void **)&seed);
	if (status != EFI_SUCCESS) {
		efi_warn("Failed to allocate memory for RNG seed.\n");
		goto err_warn;
	}

	if (rng) {
		status = efi_call_proto(rng, get_rng, &rng_algo_raw,
					EFI_RANDOM_SEED_SIZE, seed->bits);

		if (status == EFI_UNSUPPORTED)
			 
			status = efi_call_proto(rng, get_rng, NULL,
						EFI_RANDOM_SEED_SIZE, seed->bits);

		if (status == EFI_SUCCESS)
			offset = EFI_RANDOM_SEED_SIZE;
	}

	if (nv_seed_size) {
		status = get_efi_var(L"RandomSeed", &rng_table_guid, NULL,
				     &nv_seed_size, seed->bits + offset);

		if (status == EFI_SUCCESS)
			 
			status = set_efi_var(L"RandomSeed", &rng_table_guid, 0,
					     0, NULL);

		if (status == EFI_SUCCESS)
			offset += nv_seed_size;
		else
			memzero_explicit(seed->bits + offset, nv_seed_size);
	}

	if (!offset)
		goto err_freepool;

	if (prev_seed_size) {
		memcpy(seed->bits + offset, prev_seed->bits, prev_seed_size);
		offset += prev_seed_size;
	}

	seed->size = offset;
	status = efi_bs_call(install_configuration_table, &rng_table_guid, seed);
	if (status != EFI_SUCCESS)
		goto err_freepool;

	if (prev_seed_size) {
		 
		memzero_explicit(prev_seed->bits, prev_seed_size);
		efi_bs_call(free_pool, prev_seed);
	}
	return EFI_SUCCESS;

err_freepool:
	memzero_explicit(seed, struct_size(seed, bits, seed_size));
	efi_bs_call(free_pool, seed);
	efi_warn("Failed to obtain seed from EFI_RNG_PROTOCOL or EFI variable\n");
err_warn:
	if (prev_seed)
		efi_warn("Retaining bootloader-supplied seed only");
	return status;
}
