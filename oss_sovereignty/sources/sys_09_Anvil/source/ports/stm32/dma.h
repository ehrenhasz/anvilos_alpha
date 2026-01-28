
#ifndef MICROPY_INCLUDED_STM32_DMA_H
#define MICROPY_INCLUDED_STM32_DMA_H

#include "py/mpconfig.h"

typedef struct _dma_descr_t dma_descr_t;

#if defined(STM32H5)


#define DMA_CIRCULAR 0x00000001
#endif

#if defined(STM32F0) || defined(STM32F4) || defined(STM32F7) || defined(STM32G0) || defined(STM32H5) || defined(STM32H7)

extern const dma_descr_t dma_I2C_1_RX;
extern const dma_descr_t dma_SPI_3_RX;
extern const dma_descr_t dma_I2C_4_RX;
extern const dma_descr_t dma_I2C_3_RX;
extern const dma_descr_t dma_I2C_2_RX;
extern const dma_descr_t dma_SPI_2_RX;
extern const dma_descr_t dma_SPI_2_TX;
extern const dma_descr_t dma_I2C_3_TX;
extern const dma_descr_t dma_I2C_4_TX;
extern const dma_descr_t dma_DAC_1_TX;
extern const dma_descr_t dma_DAC_2_TX;
extern const dma_descr_t dma_SPI_3_TX;
extern const dma_descr_t dma_I2C_1_TX;
extern const dma_descr_t dma_I2C_2_TX;
extern const dma_descr_t dma_SDMMC_2;
extern const dma_descr_t dma_SPI_1_RX;
extern const dma_descr_t dma_SPI_5_RX;
extern const dma_descr_t dma_SDIO_0;
extern const dma_descr_t dma_SPI_4_RX;
extern const dma_descr_t dma_SPI_5_TX;
extern const dma_descr_t dma_SPI_4_TX;
extern const dma_descr_t dma_SPI_6_TX;
extern const dma_descr_t dma_SPI_1_TX;
extern const dma_descr_t dma_SDMMC_2;
extern const dma_descr_t dma_SPI_6_RX;
extern const dma_descr_t dma_SDIO_0;
extern const dma_descr_t dma_DCMI_0;
extern const dma_descr_t dma_I2S_1_RX;
extern const dma_descr_t dma_I2S_1_TX;
extern const dma_descr_t dma_I2S_2_RX;
extern const dma_descr_t dma_I2S_2_TX;

#elif defined(STM32G4)

extern const dma_descr_t dma_SPI_1_RX;
extern const dma_descr_t dma_SPI_1_TX;
extern const dma_descr_t dma_SPI_2_RX;
extern const dma_descr_t dma_SPI_2_TX;
extern const dma_descr_t dma_I2C_1_RX;
extern const dma_descr_t dma_I2C_1_TX;
extern const dma_descr_t dma_I2C_2_RX;
extern const dma_descr_t dma_I2C_2_TX;
extern const dma_descr_t dma_I2C_3_RX;
extern const dma_descr_t dma_I2C_3_TX;
extern const dma_descr_t dma_UART_3_RX;
extern const dma_descr_t dma_UART_3_TX;
extern const dma_descr_t dma_DAC_1_TX;
extern const dma_descr_t dma_DAC_2_TX;
extern const dma_descr_t dma_UART_1_RX;
extern const dma_descr_t dma_UART_1_TX;
extern const dma_descr_t dma_LPUART_1_RX;
extern const dma_descr_t dma_LPUART_1_TX;
extern const dma_descr_t dma_ADC_1;
extern const dma_descr_t dma_MEM_2_MEM;

#elif defined(STM32L0)

extern const dma_descr_t dma_SPI_1_RX;
extern const dma_descr_t dma_I2C_3_TX;
extern const dma_descr_t dma_SPI_1_TX;
extern const dma_descr_t dma_I2C_3_RX;
extern const dma_descr_t dma_DAC_1_TX;
extern const dma_descr_t dma_SPI_2_RX;
extern const dma_descr_t dma_I2C_2_TX;
extern const dma_descr_t dma_DAC_2_TX;
extern const dma_descr_t dma_SPI_2_TX;
extern const dma_descr_t dma_I2C_2_RX;
extern const dma_descr_t dma_I2C_1_TX;
extern const dma_descr_t dma_I2C_1_RX;

#elif defined(STM32L1)
extern const dma_descr_t dma_SPI_1_RX;
extern const dma_descr_t dma_SPI_3_TX;
extern const dma_descr_t dma_SPI_1_TX;
extern const dma_descr_t dma_SPI_3_RX;
extern const dma_descr_t dma_DAC_1_TX;
extern const dma_descr_t dma_SPI_2_RX;
extern const dma_descr_t dma_I2C_2_TX;
extern const dma_descr_t dma_DAC_2_TX;
extern const dma_descr_t dma_SPI_2_TX;
extern const dma_descr_t dma_I2C_2_RX;
extern const dma_descr_t dma_I2C_1_TX;
extern const dma_descr_t dma_I2C_1_RX;

#elif defined(STM32L4) || defined(STM32WB) || defined(STM32WL)

extern const dma_descr_t dma_ADC_1_RX;
extern const dma_descr_t dma_ADC_2_RX;
extern const dma_descr_t dma_SPI_1_RX;
extern const dma_descr_t dma_I2C_3_TX;
extern const dma_descr_t dma_ADC_3_RX;
extern const dma_descr_t dma_SPI_1_TX;
extern const dma_descr_t dma_I2C_3_RX;
extern const dma_descr_t dma_DAC_1_TX;
extern const dma_descr_t dma_SPI_2_RX;
extern const dma_descr_t dma_I2C_2_TX;
extern const dma_descr_t dma_DAC_2_TX;
extern const dma_descr_t dma_SPI_2_TX;
extern const dma_descr_t dma_I2C_2_RX;
extern const dma_descr_t dma_I2C_1_TX;
extern const dma_descr_t dma_I2C_1_RX;
extern const dma_descr_t dma_SPI_3_RX;
extern const dma_descr_t dma_SPI_3_TX;
extern const dma_descr_t dma_SDIO_0;
extern const dma_descr_t dma_I2C_4_TX;
extern const dma_descr_t dma_I2C_4_RX;
extern const dma_descr_t dma_SPI_SUBGHZ_TX;
extern const dma_descr_t dma_SPI_SUBGHZ_RX;

#endif


void dma_init(DMA_HandleTypeDef *dma, const dma_descr_t *dma_descr, uint32_t dir, void *data);
void dma_init_handle(DMA_HandleTypeDef *dma, const dma_descr_t *dma_descr, uint32_t dir, void *data);
void dma_deinit(const dma_descr_t *dma_descr);
void dma_invalidate_channel(const dma_descr_t *dma_descr);


void dma_nohal_init(const dma_descr_t *descr, uint32_t config);
void dma_nohal_deinit(const dma_descr_t *descr);
void dma_nohal_start(const dma_descr_t *descr, uint32_t src_addr, uint32_t dst_addr, uint16_t len);




void dma_external_acquire(uint32_t controller, uint32_t stream);
void dma_external_release(uint32_t controller, uint32_t stream);

#if __DCACHE_PRESENT












void dma_protect_rx_region(void *dest, size_t len);








void dma_unprotect_rx_region(void *dest, size_t len);

#else

inline static void dma_protect_rx_region(uint8_t *dest, size_t len) {
    
}

inline static void dma_unprotect_rx_region(void *dest, size_t len) {

}

#endif 

#endif 
