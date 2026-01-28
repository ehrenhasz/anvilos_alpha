#ifndef _PP_ENDIAN_H_
#define _PP_ENDIAN_H_
#define PP_HOST_TO_SMC_UL(X) cpu_to_be32(X)
#define PP_SMC_TO_HOST_UL(X) be32_to_cpu(X)
#define PP_HOST_TO_SMC_US(X) cpu_to_be16(X)
#define PP_SMC_TO_HOST_US(X) be16_to_cpu(X)
#define CONVERT_FROM_HOST_TO_SMC_UL(X) ((X) = PP_HOST_TO_SMC_UL(X))
#define CONVERT_FROM_SMC_TO_HOST_UL(X) ((X) = PP_SMC_TO_HOST_UL(X))
#define CONVERT_FROM_HOST_TO_SMC_US(X) ((X) = PP_HOST_TO_SMC_US(X))
#endif  
