 
#include "outp.h"
#include "dp.h"
#include "ior.h"

#include <subdev/bios.h>
#include <subdev/bios/dcb.h>
#include <subdev/i2c.h>

void
nvkm_outp_route(struct nvkm_disp *disp)
{
	struct nvkm_outp *outp;
	struct nvkm_ior *ior;

	list_for_each_entry(ior, &disp->iors, head) {
		if ((outp = ior->arm.outp) && ior->arm.outp != ior->asy.outp) {
			OUTP_DBG(outp, "release %s", ior->name);
			if (ior->func->route.set)
				ior->func->route.set(outp, NULL);
			ior->arm.outp = NULL;
		}
	}

	list_for_each_entry(ior, &disp->iors, head) {
		if ((outp = ior->asy.outp)) {
			OUTP_DBG(outp, "acquire %s", ior->name);
			if (ior->asy.outp != ior->arm.outp) {
				if (ior->func->route.set)
					ior->func->route.set(outp, ior);
				ior->arm.outp = ior->asy.outp;
			}
		}
	}
}

static enum nvkm_ior_proto
nvkm_outp_xlat(struct nvkm_outp *outp, enum nvkm_ior_type *type)
{
	switch (outp->info.location) {
	case 0:
		switch (outp->info.type) {
		case DCB_OUTPUT_ANALOG: *type = DAC; return  CRT;
		case DCB_OUTPUT_TV    : *type = DAC; return   TV;
		case DCB_OUTPUT_TMDS  : *type = SOR; return TMDS;
		case DCB_OUTPUT_LVDS  : *type = SOR; return LVDS;
		case DCB_OUTPUT_DP    : *type = SOR; return   DP;
		default:
			break;
		}
		break;
	case 1:
		switch (outp->info.type) {
		case DCB_OUTPUT_TMDS: *type = PIOR; return TMDS;
		case DCB_OUTPUT_DP  : *type = PIOR; return TMDS;  
		default:
			break;
		}
		break;
	default:
		break;
	}
	WARN_ON(1);
	return UNKNOWN;
}

void
nvkm_outp_release(struct nvkm_outp *outp, u8 user)
{
	struct nvkm_ior *ior = outp->ior;
	OUTP_TRACE(outp, "release %02x &= %02x %p", outp->acquired, ~user, ior);
	if (ior) {
		outp->acquired &= ~user;
		if (!outp->acquired) {
			if (outp->func->release && outp->ior)
				outp->func->release(outp);
			outp->ior->asy.outp = NULL;
			outp->ior = NULL;
		}
	}
}

static inline int
nvkm_outp_acquire_ior(struct nvkm_outp *outp, u8 user, struct nvkm_ior *ior)
{
	outp->ior = ior;
	outp->ior->asy.outp = outp;
	outp->ior->asy.link = outp->info.sorconf.link;
	outp->acquired |= user;
	return 0;
}

static inline int
nvkm_outp_acquire_hda(struct nvkm_outp *outp, enum nvkm_ior_type type,
		      u8 user, bool hda)
{
	struct nvkm_ior *ior;

	 
	list_for_each_entry(ior, &outp->disp->iors, head) {
		if (!ior->identity && ior->hda == hda &&
		    !ior->asy.outp && ior->type == type && !ior->arm.outp &&
		    (ior->func->route.set || ior->id == __ffs(outp->info.or)))
			return nvkm_outp_acquire_ior(outp, user, ior);
	}

	 
	list_for_each_entry(ior, &outp->disp->iors, head) {
		if (!ior->identity && ior->hda == hda &&
		    !ior->asy.outp && ior->type == type &&
		    (ior->func->route.set || ior->id == __ffs(outp->info.or)))
			return nvkm_outp_acquire_ior(outp, user, ior);
	}

	return -ENOSPC;
}

int
nvkm_outp_acquire(struct nvkm_outp *outp, u8 user, bool hda)
{
	struct nvkm_ior *ior = outp->ior;
	enum nvkm_ior_proto proto;
	enum nvkm_ior_type type;

	OUTP_TRACE(outp, "acquire %02x |= %02x %p", outp->acquired, user, ior);
	if (ior) {
		outp->acquired |= user;
		return 0;
	}

	 
	proto = nvkm_outp_xlat(outp, &type);
	if (proto == UNKNOWN)
		return -ENOSYS;

	 
	if (outp->identity) {
		ior = nvkm_ior_find(outp->disp, SOR, ffs(outp->info.or) - 1);
		if (WARN_ON(!ior))
			return -ENOSPC;
		return nvkm_outp_acquire_ior(outp, user, ior);
	}

	 
	list_for_each_entry(ior, &outp->disp->iors, head) {
		if (!ior->identity && !ior->asy.outp && ior->arm.outp == outp) {
			 
			WARN_ON(hda && !ior->hda);
			return nvkm_outp_acquire_ior(outp, user, ior);
		}
	}

	 
	if (!hda) {
		if (!nvkm_outp_acquire_hda(outp, type, user, false))
			return 0;

		 
		return nvkm_outp_acquire_hda(outp, type, user, true);
	}

	 
	if (!nvkm_outp_acquire_hda(outp, type, user, true))
		return 0;

	 
	return nvkm_outp_acquire_hda(outp, type, user, false);
}

void
nvkm_outp_fini(struct nvkm_outp *outp)
{
	if (outp->func->fini)
		outp->func->fini(outp);
}

static void
nvkm_outp_init_route(struct nvkm_outp *outp)
{
	struct nvkm_disp *disp = outp->disp;
	enum nvkm_ior_proto proto;
	enum nvkm_ior_type type;
	struct nvkm_ior *ior;
	int id, link;

	 
	proto = nvkm_outp_xlat(outp, &type);
	if (proto == UNKNOWN)
		return;

	ior = nvkm_ior_find(disp, type, -1);
	if (!ior) {
		WARN_ON(1);
		return;
	}

	 
	if (ior->func->route.get) {
		id = ior->func->route.get(outp, &link);
		if (id < 0) {
			OUTP_DBG(outp, "no route");
			return;
		}
	} else {
		 
		id   = ffs(outp->info.or) - 1;
		link = (ior->type == SOR) ? outp->info.sorconf.link : 0;
	}

	ior = nvkm_ior_find(disp, type, id);
	if (!ior) {
		WARN_ON(1);
		return;
	}

	 
	ior->func->state(ior, &ior->arm);
	if (!ior->arm.head || ior->arm.proto != proto) {
		OUTP_DBG(outp, "no heads (%x %d %d)", ior->arm.head,
			 ior->arm.proto, proto);

		 
		if (ior->func->route.get && !ior->arm.head && outp->info.type == DCB_OUTPUT_DP)
			nvkm_dp_disable(outp, ior);

		return;
	}

	OUTP_DBG(outp, "on %s link %x", ior->name, ior->arm.link);
	ior->arm.outp = outp;
}

void
nvkm_outp_init(struct nvkm_outp *outp)
{
	nvkm_outp_init_route(outp);
	if (outp->func->init)
		outp->func->init(outp);
}

void
nvkm_outp_del(struct nvkm_outp **poutp)
{
	struct nvkm_outp *outp = *poutp;
	if (outp && !WARN_ON(!outp->func)) {
		if (outp->func->dtor)
			*poutp = outp->func->dtor(outp);
		kfree(*poutp);
		*poutp = NULL;
	}
}

int
nvkm_outp_new_(const struct nvkm_outp_func *func, struct nvkm_disp *disp,
	       int index, struct dcb_output *dcbE, struct nvkm_outp **poutp)
{
	struct nvkm_i2c *i2c = disp->engine.subdev.device->i2c;
	struct nvkm_outp *outp;
	enum nvkm_ior_proto proto;
	enum nvkm_ior_type type;

	if (!(outp = *poutp = kzalloc(sizeof(*outp), GFP_KERNEL)))
		return -ENOMEM;

	outp->func = func;
	outp->disp = disp;
	outp->index = index;
	outp->info = *dcbE;
	outp->i2c = nvkm_i2c_bus_find(i2c, dcbE->i2c_index);

	OUTP_DBG(outp, "type %02x loc %d or %d link %d con %x "
		       "edid %x bus %d head %x",
		 outp->info.type, outp->info.location, outp->info.or,
		 outp->info.type >= 2 ? outp->info.sorconf.link : 0,
		 outp->info.connector, outp->info.i2c_index,
		 outp->info.bus, outp->info.heads);

	 
	proto = nvkm_outp_xlat(outp, &type);
	if (proto == UNKNOWN)
		return -ENODEV;

	return 0;
}

static const struct nvkm_outp_func
nvkm_outp = {
};

int
nvkm_outp_new(struct nvkm_disp *disp, int index, struct dcb_output *dcbE,
	      struct nvkm_outp **poutp)
{
	return nvkm_outp_new_(&nvkm_outp, disp, index, dcbE, poutp);
}
