
#define MICROPY_HW_BOARD_NAME       "VK-RA6M5"
#define MICROPY_HW_MCU_NAME         "RA6M5"
#define MICROPY_HW_MCU_SYSCLK       200000000
#define MICROPY_HW_MCU_PCLK         100000000


#define MICROPY_EMIT_THUMB          (1)
#define MICROPY_EMIT_INLINE_THUMB   (1)
#define MICROPY_PY_BUILTINS_COMPLEX (1)
#define MICROPY_PY_GENERATOR_PEND_THROW (1)
#define MICROPY_PY_MATH             (1)
#define MICROPY_PY_UHEAPQ           (1)
#define MICROPY_PY_UTIMEQ           (1)
#define MICROPY_PY_THREAD           (0) 


#define MICROPY_HW_ENABLE_RTC       (1)
#define MICROPY_HW_RTC_SOURCE       (1)     
#define MICROPY_HW_ENABLE_ADC       (1)
#define MICROPY_HW_HAS_FLASH        (1)
#define MICROPY_HW_ENABLE_INTERNAL_FLASH_STORAGE (1)
#define MICROPY_HW_HAS_QSPI_FLASH   (1)
#define MICROPY_HW_HAS_SDHI_CARD    (1)




#define MICROPY_HW_UART7_TX         (pin_P613) 
#define MICROPY_HW_UART7_RX         (pin_P614) 
#define MICROPY_HW_UART6_TX         (pin_P506) 
#define MICROPY_HW_UART6_RX         (pin_P505) 
#define MICROPY_HW_UART9_TX         (pin_P109) 
#define MICROPY_HW_UART9_RX         (pin_P110) 
#define MICROPY_HW_UART_REPL        HW_UART_9
#define MICROPY_HW_UART_REPL_BAUD   115200


#define MICROPY_HW_I2C2_SCL         (pin_P415)
#define MICROPY_HW_I2C2_SDA         (pin_P414)


#define MICROPY_HW_SPI0_SSL         (pin_P301) 
#define MICROPY_HW_SPI0_RSPCK       (pin_P204) 
#define MICROPY_HW_SPI0_MISO        (pin_P202) 
#define MICROPY_HW_SPI0_MOSI        (pin_P203) 


#define MICROPY_HW_PWM_2A           (pin_P113) 
#define MICROPY_HW_PWM_2B           (pin_P114) 
#define MICROPY_HW_PWM_3A           (pin_P111) 

#define MICROPY_HW_PWM_3B           (pin_P112) 

#define MICROPY_HW_PWM_4A           (pin_P115) 

#define MICROPY_HW_PWM_4B           (pin_P608) 




#define MICROPY_HW_PWM_6B           (pin_P408) 
#define MICROPY_HW_PWM_7A           (pin_P304) 
#define MICROPY_HW_PWM_7B           (pin_P303) 
#define MICROPY_HW_PWM_8A           (pin_P605) 
#define MICROPY_HW_PWM_8B           (pin_P604) 


#define MICROPY_HW_DAC0             (pin_P014) 
#define MICROPY_HW_DAC1             (pin_P015) 


#define MICROPY_HW_HAS_SWITCH       (1)
#define MICROPY_HW_USRSW_PIN        (pin_P010)

#define MICROPY_HW_USRSW_PULL       (MP_HAL_PIN_PULL_NONE)
#define MICROPY_HW_USRSW_EXTI_MODE  (MP_HAL_PIN_TRIGGER_FALLING)
#define MICROPY_HW_USRSW_PRESSED    (0)


#define MICROPY_HW_LED1             (pin_P006)
#define MICROPY_HW_LED2             (pin_P007)
#define MICROPY_HW_LED3             (pin_P008)
#define MICROPY_HW_LED_ON(pin)      mp_hal_pin_high(pin)
#define MICROPY_HW_LED_OFF(pin)     mp_hal_pin_low(pin)
#define MICROPY_HW_LED_TOGGLE(pin)  mp_hal_pin_toggle(pin)
