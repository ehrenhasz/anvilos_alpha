 
#ifndef SMU_13_0_6_DRIVER_IF_H
#define SMU_13_0_6_DRIVER_IF_H




#define SMU13_0_6_DRIVER_IF_VERSION 0x08042024


#define NUM_I2C_CONTROLLERS                8
#define I2C_CONTROLLER_ENABLED             1
#define I2C_CONTROLLER_DISABLED            0

#define MAX_SW_I2C_COMMANDS                24

typedef enum {
  I2C_CONTROLLER_PORT_0, 
  I2C_CONTROLLER_PORT_1, 
  I2C_CONTROLLER_PORT_COUNT,
} I2cControllerPort_e;

typedef enum {
  UNSUPPORTED_1,              
  I2C_SPEED_STANDARD_100K,    
  I2C_SPEED_FAST_400K,        
  I2C_SPEED_FAST_PLUS_1M,     
  UNSUPPORTED_2,              
  UNSUPPORTED_3,              
  I2C_SPEED_COUNT,
} I2cSpeed_e;

typedef enum {
  I2C_CMD_READ,
  I2C_CMD_WRITE,
  I2C_CMD_COUNT,
} I2cCmdType_e;

#define CMDCONFIG_STOP_BIT             0
#define CMDCONFIG_RESTART_BIT          1
#define CMDCONFIG_READWRITE_BIT        2 

#define CMDCONFIG_STOP_MASK           (1 << CMDCONFIG_STOP_BIT)
#define CMDCONFIG_RESTART_MASK        (1 << CMDCONFIG_RESTART_BIT)
#define CMDCONFIG_READWRITE_MASK      (1 << CMDCONFIG_READWRITE_BIT)

typedef struct {
  uint8_t ReadWriteData;  
  uint8_t CmdConfig; 
} SwI2cCmd_t; 

typedef struct {
  uint8_t    I2CcontrollerPort; 
  uint8_t    I2CSpeed;          
  uint8_t    SlaveAddress;      
  uint8_t    NumCmds;           
  SwI2cCmd_t SwI2cCmds[MAX_SW_I2C_COMMANDS];
} SwI2cRequest_t; 

typedef struct {
  SwI2cRequest_t SwI2cRequest;
  uint32_t       Spare[8];
  uint32_t       MmHubPadding[8]; 
} SwI2cRequestExternal_t;

typedef enum {
  PPCLK_VCLK,
  PPCLK_DCLK,
  PPCLK_SOCCLK,
  PPCLK_UCLK,
  PPCLK_FCLK,
  PPCLK_LCLK,
  PPCLK_COUNT,
} PPCLK_e;

typedef enum {
  GPIO_INT_POLARITY_ACTIVE_LOW,
  GPIO_INT_POLARITY_ACTIVE_HIGH,
} GpioIntPolarity_e;


typedef enum {
  UCLK_DPM_MODE_BANDWIDTH,
  UCLK_DPM_MODE_LATENCY,
} UCLK_DPM_MODE_e;

typedef struct {
  
  uint16_t avgPsmCount[30];
  uint16_t minPsmCount[30];
  float    avgPsmVoltage[30];
  float    minPsmVoltage[30];
} AvfsDebugTableAid_t;

typedef struct {
  
  uint16_t avgPsmCount[30];
  uint16_t minPsmCount[30];
  float    avgPsmVoltage[30];
  float    minPsmVoltage[30];
} AvfsDebugTableXcd_t;


#define IH_INTERRUPT_ID_TO_DRIVER                   0xFE
#define IH_INTERRUPT_CONTEXT_ID_THERMAL_THROTTLING  0x7


#define THROTTLER_PROCHOT_BIT           0
#define THROTTLER_PPT_BIT               1
#define THROTTLER_THERMAL_SOCKET_BIT    2
#define THROTTLER_THERMAL_VR_BIT        3
#define THROTTLER_THERMAL_HBM_BIT       4


















#endif
