#ifndef __AMDGPU_I2C_H__
#define __AMDGPU_I2C_H__
struct amdgpu_i2c_chan *amdgpu_i2c_create(struct drm_device *dev,
					  const struct amdgpu_i2c_bus_rec *rec,
					  const char *name);
void amdgpu_i2c_destroy(struct amdgpu_i2c_chan *i2c);
void amdgpu_i2c_init(struct amdgpu_device *adev);
void amdgpu_i2c_fini(struct amdgpu_device *adev);
void amdgpu_i2c_add(struct amdgpu_device *adev,
		    const struct amdgpu_i2c_bus_rec *rec,
		    const char *name);
struct amdgpu_i2c_chan *
amdgpu_i2c_lookup(struct amdgpu_device *adev,
		  const struct amdgpu_i2c_bus_rec *i2c_bus);
void
amdgpu_i2c_router_select_ddc_port(const struct amdgpu_connector *connector);
void
amdgpu_i2c_router_select_cd_port(const struct amdgpu_connector *connector);
#endif
