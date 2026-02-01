 
#include <core/subdev.h>
#include <nvfw/hs.h>

const struct nvfw_hs_header *
nvfw_hs_header(struct nvkm_subdev *subdev, const void *data)
{
	const struct nvfw_hs_header *hdr = data;
	nvkm_debug(subdev, "hsHeader:\n");
	nvkm_debug(subdev, "\tsigDbgOffset     : 0x%x\n", hdr->sig_dbg_offset);
	nvkm_debug(subdev, "\tsigDbgSize       : 0x%x\n", hdr->sig_dbg_size);
	nvkm_debug(subdev, "\tsigProdOffset    : 0x%x\n", hdr->sig_prod_offset);
	nvkm_debug(subdev, "\tsigProdSize      : 0x%x\n", hdr->sig_prod_size);
	nvkm_debug(subdev, "\tpatchLoc         : 0x%x\n", hdr->patch_loc);
	nvkm_debug(subdev, "\tpatchSig         : 0x%x\n", hdr->patch_sig);
	nvkm_debug(subdev, "\thdrOffset        : 0x%x\n", hdr->hdr_offset);
	nvkm_debug(subdev, "\thdrSize          : 0x%x\n", hdr->hdr_size);
	return hdr;
}

const struct nvfw_hs_header_v2 *
nvfw_hs_header_v2(struct nvkm_subdev *subdev, const void *data)
{
	const struct nvfw_hs_header_v2 *hdr = data;

	nvkm_debug(subdev, "hsHeader:\n");
	nvkm_debug(subdev, "\tsigProdOffset    : 0x%x\n", hdr->sig_prod_offset);
	nvkm_debug(subdev, "\tsigProdSize      : 0x%x\n", hdr->sig_prod_size);
	nvkm_debug(subdev, "\tpatchLoc         : 0x%x\n", hdr->patch_loc);
	nvkm_debug(subdev, "\tpatchSig         : 0x%x\n", hdr->patch_sig);
	nvkm_debug(subdev, "\tmetadataOffset   : 0x%x\n", hdr->meta_data_offset);
	nvkm_debug(subdev, "\tmetadataSize     : 0x%x\n", hdr->meta_data_size);
	nvkm_debug(subdev, "\tnumSig           : 0x%x\n", hdr->num_sig);
	nvkm_debug(subdev, "\theaderOffset     : 0x%x\n", hdr->header_offset);
	nvkm_debug(subdev, "\theaderSize       : 0x%x\n", hdr->header_size);
	return hdr;
}

const struct nvfw_hs_load_header *
nvfw_hs_load_header(struct nvkm_subdev *subdev, const void *data)
{
	const struct nvfw_hs_load_header *hdr = data;
	int i;

	nvkm_debug(subdev, "hsLoadHeader:\n");
	nvkm_debug(subdev, "\tnonSecCodeOff    : 0x%x\n",
			   hdr->non_sec_code_off);
	nvkm_debug(subdev, "\tnonSecCodeSize   : 0x%x\n",
			   hdr->non_sec_code_size);
	nvkm_debug(subdev, "\tdataDmaBase      : 0x%x\n", hdr->data_dma_base);
	nvkm_debug(subdev, "\tdataSize         : 0x%x\n", hdr->data_size);
	nvkm_debug(subdev, "\tnumApps          : 0x%x\n", hdr->num_apps);
	for (i = 0; i < hdr->num_apps; i++) {
		nvkm_debug(subdev,
			   "\tApp[%d]           : offset 0x%x size 0x%x\n", i,
			   hdr->apps[(i * 2) + 0], hdr->apps[(i * 2) + 1]);
	}

	return hdr;
}

const struct nvfw_hs_load_header_v2 *
nvfw_hs_load_header_v2(struct nvkm_subdev *subdev, const void *data)
{
	const struct nvfw_hs_load_header_v2 *hdr = data;
	int i;

	nvkm_debug(subdev, "hsLoadHeader:\n");
	nvkm_debug(subdev, "\tosCodeOffset     : 0x%x\n", hdr->os_code_offset);
	nvkm_debug(subdev, "\tosCodeSize       : 0x%x\n", hdr->os_code_size);
	nvkm_debug(subdev, "\tosDataOffset     : 0x%x\n", hdr->os_data_offset);
	nvkm_debug(subdev, "\tosDataSize       : 0x%x\n", hdr->os_data_size);
	nvkm_debug(subdev, "\tnumApps          : 0x%x\n", hdr->num_apps);
	for (i = 0; i < hdr->num_apps; i++) {
		nvkm_debug(subdev,
			   "\tApp[%d]           : offset 0x%x size 0x%x\n", i,
			   hdr->app[i].offset, hdr->app[i].size);
	}

	return hdr;
}
