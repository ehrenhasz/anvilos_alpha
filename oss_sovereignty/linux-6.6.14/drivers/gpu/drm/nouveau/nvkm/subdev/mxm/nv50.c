 
#include "mxms.h"

#include <subdev/bios.h>
#include <subdev/bios/conn.h>
#include <subdev/bios/dcb.h>
#include <subdev/bios/mxm.h>

struct context {
	u32 *outp;
	struct mxms_odev desc;
};

static bool
mxm_match_tmds_partner(struct nvkm_mxm *mxm, u8 *data, void *info)
{
	struct context *ctx = info;
	struct mxms_odev desc;

	mxms_output_device(mxm, data, &desc);
	if (desc.outp_type == 2 &&
	    desc.dig_conn == ctx->desc.dig_conn)
		return false;
	return true;
}

static bool
mxm_match_dcb(struct nvkm_mxm *mxm, u8 *data, void *info)
{
	struct nvkm_bios *bios = mxm->subdev.device->bios;
	struct context *ctx = info;
	u64 desc = *(u64 *)data;

	mxms_output_device(mxm, data, &ctx->desc);

	 
	if ((ctx->outp[0] & 0x0000000f) != ctx->desc.outp_type)
		return true;

	 
	if ((desc & 0x00000000000000f0) >= 0x20) {
		 
		u8 link = mxm_sor_map(bios, ctx->desc.dig_conn);
		if ((ctx->outp[0] & 0x0f000000) != (link & 0x0f) << 24)
			return true;

		 
		link = (link & 0x30) >> 4;
		if ((link & ((ctx->outp[1] & 0x00000030) >> 4)) != link)
			return true;
	}

	 
	data[0] &= ~0xf0;
	if (ctx->desc.outp_type == 6 && ctx->desc.conn_type == 6 &&
	    mxms_foreach(mxm, 0x01, mxm_match_tmds_partner, ctx)) {
		data[0] |= 0x20;  
	} else {
		data[0] |= 0xf0;
	}

	return false;
}

static int
mxm_dcb_sanitise_entry(struct nvkm_bios *bios, void *data, int idx, u16 pdcb)
{
	struct nvkm_mxm *mxm = data;
	struct context ctx = { .outp = (u32 *)(bios->data + pdcb) };
	u8 type, i2cidx, link, ver, len;
	u8 *conn;

	 
	if (mxms_foreach(mxm, 0x01, mxm_match_dcb, &ctx)) {
		nvkm_debug(&mxm->subdev, "disable %d: %08x %08x\n",
			   idx, ctx.outp[0], ctx.outp[1]);
		ctx.outp[0] |= 0x0000000f;
		return 0;
	}

	 
	i2cidx = mxm_ddc_map(bios, ctx.desc.ddc_port);
	if ((ctx.outp[0] & 0x0000000f) != DCB_OUTPUT_DP)
		i2cidx = (i2cidx & 0x0f) << 4;
	else
		i2cidx = (i2cidx & 0xf0);

	if (i2cidx != 0xf0) {
		ctx.outp[0] &= ~0x000000f0;
		ctx.outp[0] |= i2cidx;
	}

	 
	switch (ctx.desc.outp_type) {
	case 0x00:  
	case 0x01:  
		break;
	default:
		link = mxm_sor_map(bios, ctx.desc.dig_conn) & 0x30;
		ctx.outp[1] &= ~0x00000030;
		ctx.outp[1] |= link;
		break;
	}

	 
	conn  = bios->data;
	conn += nvbios_connEe(bios, (ctx.outp[0] & 0x0000f000) >> 12, &ver, &len);
	type  = conn[0];
	switch (ctx.desc.conn_type) {
	case 0x01:  
		ctx.outp[1] |= 0x00000004;  
		 
		break;
	case 0x02:  
		type = DCB_CONNECTOR_HDMI_1;
		break;
	case 0x03:  
		type = DCB_CONNECTOR_DVI_D;
		break;
	case 0x0e:  
		ctx.outp[1] |= 0x00010000;
		fallthrough;
	case 0x07:  
		ctx.outp[1] |= 0x00000004;  
		type = DCB_CONNECTOR_eDP;
		break;
	default:
		break;
	}

	if (mxms_version(mxm) >= 0x0300)
		conn[0] = type;

	return 0;
}

static bool
mxm_show_unmatched(struct nvkm_mxm *mxm, u8 *data, void *info)
{
	struct nvkm_subdev *subdev = &mxm->subdev;
	u64 desc = *(u64 *)data;
	if ((desc & 0xf0) != 0xf0)
		nvkm_info(subdev, "unmatched output device %016llx\n", desc);
	return true;
}

static void
mxm_dcb_sanitise(struct nvkm_mxm *mxm)
{
	struct nvkm_subdev *subdev = &mxm->subdev;
	struct nvkm_bios *bios = subdev->device->bios;
	u8  ver, hdr, cnt, len;
	u16 dcb = dcb_table(bios, &ver, &hdr, &cnt, &len);
	if (dcb == 0x0000 || (ver != 0x40 && ver != 0x41)) {
		nvkm_warn(subdev, "unsupported DCB version\n");
		return;
	}

	dcb_outp_foreach(bios, mxm, mxm_dcb_sanitise_entry);
	mxms_foreach(mxm, 0x01, mxm_show_unmatched, NULL);
}

int
nv50_mxm_new(struct nvkm_device *device, enum nvkm_subdev_type type, int inst,
	     struct nvkm_subdev **pmxm)
{
	struct nvkm_mxm *mxm;
	int ret;

	ret = nvkm_mxm_new_(device, type, inst, &mxm);
	if (mxm)
		*pmxm = &mxm->subdev;
	if (ret)
		return ret;

	if (mxm->action & MXM_SANITISE_DCB)
		mxm_dcb_sanitise(mxm);

	return 0;
}
