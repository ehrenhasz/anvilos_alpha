#ifndef __GK20A_VOLT_H__
#define __GK20A_VOLT_H__
struct cvb_coef {
	int c0;
	int c1;
	int c2;
	int c3;
	int c4;
	int c5;
};
struct gk20a_volt {
	struct nvkm_volt base;
	struct regulator *vdd;
};
int gk20a_volt_ctor(struct nvkm_device *device, enum nvkm_subdev_type, int,
		    const struct cvb_coef *coefs, int nb_coefs,
		    int vmin, struct gk20a_volt *volt);
#endif
