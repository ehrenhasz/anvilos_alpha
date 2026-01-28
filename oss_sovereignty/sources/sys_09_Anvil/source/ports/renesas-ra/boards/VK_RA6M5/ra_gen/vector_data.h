
#ifndef VECTOR_DATA_H
#define VECTOR_DATA_H
#ifdef __cplusplus
extern "C" {
#endif

#ifndef VECTOR_DATA_IRQ_COUNT
#define VECTOR_DATA_IRQ_COUNT    (31)
#endif

void sci_uart_rxi_isr(void);
void sci_uart_txi_isr(void);
void sci_uart_tei_isr(void);
void sci_uart_eri_isr(void);
void rtc_alarm_periodic_isr(void);
void rtc_carry_isr(void);
void agt_int_isr(void);
void r_icu_isr(void);
void spi_rxi_isr(void);
void spi_txi_isr(void);
void spi_tei_isr(void);
void spi_eri_isr(void);
void iic_master_rxi_isr(void);
void iic_master_txi_isr(void);
void iic_master_tei_isr(void);
void iic_master_eri_isr(void);
void sdhimmc_accs_isr(void);
void sdhimmc_card_isr(void);
void sdhimmc_dma_req_isr(void);


#define VECTOR_NUMBER_SCI6_RXI ((IRQn_Type)0)  
#define SCI6_RXI_IRQn          ((IRQn_Type)0)  
#define VECTOR_NUMBER_SCI6_TXI ((IRQn_Type)1)  
#define SCI6_TXI_IRQn          ((IRQn_Type)1)  
#define VECTOR_NUMBER_SCI6_TEI ((IRQn_Type)2)  
#define SCI6_TEI_IRQn          ((IRQn_Type)2)  
#define VECTOR_NUMBER_SCI6_ERI ((IRQn_Type)3)  
#define SCI6_ERI_IRQn          ((IRQn_Type)3)  
#define VECTOR_NUMBER_SCI7_RXI ((IRQn_Type)4)  
#define SCI7_RXI_IRQn          ((IRQn_Type)4)  
#define VECTOR_NUMBER_SCI7_TXI ((IRQn_Type)5)  
#define SCI7_TXI_IRQn          ((IRQn_Type)5)  
#define VECTOR_NUMBER_SCI7_TEI ((IRQn_Type)6)  
#define SCI7_TEI_IRQn          ((IRQn_Type)6)  
#define VECTOR_NUMBER_SCI7_ERI ((IRQn_Type)7)  
#define SCI7_ERI_IRQn          ((IRQn_Type)7)  
#define VECTOR_NUMBER_SCI9_RXI ((IRQn_Type)8)  
#define SCI9_RXI_IRQn          ((IRQn_Type)8)  
#define VECTOR_NUMBER_SCI9_TXI ((IRQn_Type)9)  
#define SCI9_TXI_IRQn          ((IRQn_Type)9)  
#define VECTOR_NUMBER_SCI9_TEI ((IRQn_Type)10)  
#define SCI9_TEI_IRQn          ((IRQn_Type)10)  
#define VECTOR_NUMBER_SCI9_ERI ((IRQn_Type)11)  
#define SCI9_ERI_IRQn          ((IRQn_Type)11)  
#define VECTOR_NUMBER_RTC_ALARM ((IRQn_Type)12)  
#define RTC_ALARM_IRQn          ((IRQn_Type)12)  
#define VECTOR_NUMBER_RTC_PERIOD ((IRQn_Type)13)  
#define RTC_PERIOD_IRQn          ((IRQn_Type)13)  
#define VECTOR_NUMBER_RTC_CARRY ((IRQn_Type)14)  
#define RTC_CARRY_IRQn          ((IRQn_Type)14)  
#define VECTOR_NUMBER_AGT0_INT ((IRQn_Type)15)  
#define AGT0_INT_IRQn          ((IRQn_Type)15)  
#define VECTOR_NUMBER_AGT1_INT ((IRQn_Type)16)  
#define AGT1_INT_IRQn          ((IRQn_Type)16)  
#define VECTOR_NUMBER_ICU_IRQ7 ((IRQn_Type)17)  
#define ICU_IRQ7_IRQn          ((IRQn_Type)17)  
#define VECTOR_NUMBER_ICU_IRQ13 ((IRQn_Type)18)  
#define ICU_IRQ13_IRQn          ((IRQn_Type)18)  
#define VECTOR_NUMBER_ICU_IRQ14 ((IRQn_Type)19)  
#define ICU_IRQ14_IRQn          ((IRQn_Type)19)  
#define VECTOR_NUMBER_SPI0_RXI ((IRQn_Type)20)  
#define SPI0_RXI_IRQn          ((IRQn_Type)20)  
#define VECTOR_NUMBER_SPI0_TXI ((IRQn_Type)21)  
#define SPI0_TXI_IRQn          ((IRQn_Type)21)  
#define VECTOR_NUMBER_SPI0_TEI ((IRQn_Type)22)  
#define SPI0_TEI_IRQn          ((IRQn_Type)22)  
#define VECTOR_NUMBER_SPI0_ERI ((IRQn_Type)23)  
#define SPI0_ERI_IRQn          ((IRQn_Type)23)  
#define VECTOR_NUMBER_IIC2_RXI ((IRQn_Type)24)  
#define IIC2_RXI_IRQn          ((IRQn_Type)24)  
#define VECTOR_NUMBER_IIC2_TXI ((IRQn_Type)25)  
#define IIC2_TXI_IRQn          ((IRQn_Type)25)  
#define VECTOR_NUMBER_IIC2_TEI ((IRQn_Type)26)  
#define IIC2_TEI_IRQn          ((IRQn_Type)26)  
#define VECTOR_NUMBER_IIC2_ERI ((IRQn_Type)27)  
#define IIC2_ERI_IRQn          ((IRQn_Type)27)  
#define VECTOR_NUMBER_SDHIMMC0_ACCS ((IRQn_Type)28)  
#define SDHIMMC0_ACCS_IRQn          ((IRQn_Type)28)  
#define VECTOR_NUMBER_SDHIMMC0_CARD ((IRQn_Type)29)  
#define SDHIMMC0_CARD_IRQn          ((IRQn_Type)29)  
#define VECTOR_NUMBER_SDHIMMC0_DMA_REQ ((IRQn_Type)30)  
#define SDHIMMC0_DMA_REQ_IRQn          ((IRQn_Type)30)  
#ifdef __cplusplus
}
#endif
#endif 
