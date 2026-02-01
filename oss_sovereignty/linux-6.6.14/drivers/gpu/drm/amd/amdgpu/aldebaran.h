 

#ifndef __ALDEBARAN_H__
#define __ALDEBARAN_H__

#include "amdgpu.h"

int aldebaran_reset_init(struct amdgpu_device *adev);
int aldebaran_reset_fini(struct amdgpu_device *adev);

#endif
