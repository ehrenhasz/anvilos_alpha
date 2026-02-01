 

#ifndef __DAL_DDC_SERVICE_H__
#define __DAL_DDC_SERVICE_H__

#include "link.h"

#define AUX_POWER_UP_WA_DELAY 500
#define I2C_OVER_AUX_DEFER_WA_DELAY 70
#define DPVGA_DONGLE_AUX_DEFER_WA_DELAY 40
#define I2C_OVER_AUX_DEFER_WA_DELAY_1MS 1
#define LINK_AUX_DEFAULT_LTTPR_TIMEOUT_PERIOD 3200  
#define LINK_AUX_DEFAULT_TIMEOUT_PERIOD 552  

#define EDID_SEGMENT_SIZE 256

struct ddc_service *link_create_ddc_service(
		struct ddc_service_init_data *ddc_init_data);

void link_destroy_ddc_service(struct ddc_service **ddc);

void set_ddc_transaction_type(
		struct ddc_service *ddc,
		enum ddc_transaction_type type);

uint32_t link_get_aux_defer_delay(struct ddc_service *ddc);

bool link_is_in_aux_transaction_mode(struct ddc_service *ddc);

bool try_to_configure_aux_timeout(struct ddc_service *ddc,
		uint32_t timeout);

bool link_query_ddc_data(
		struct ddc_service *ddc,
		uint32_t address,
		uint8_t *write_buf,
		uint32_t write_size,
		uint8_t *read_buf,
		uint32_t read_size);

 
bool link_aux_transfer_with_retries_no_mutex(struct ddc_service *ddc,
		struct aux_payload *payload);

bool link_configure_fixed_vs_pe_retimer(
		struct ddc_service *ddc,
		const uint8_t *data,
		uint32_t length);

bool link_query_fixed_vs_pe_retimer(
		struct ddc_service *ddc,
		uint8_t *data,
		uint32_t length);

uint32_t link_get_fixed_vs_pe_retimer_read_address(struct dc_link *link);
uint32_t link_get_fixed_vs_pe_retimer_write_address(struct dc_link *link);


void write_scdc_data(
		struct ddc_service *ddc_service,
		uint32_t pix_clk,
		bool lte_340_scramble);

void read_scdc_data(
		struct ddc_service *ddc_service);

void set_dongle_type(struct ddc_service *ddc,
		enum display_dongle_type dongle_type);

struct ddc *get_ddc_pin(struct ddc_service *ddc_service);

int link_aux_transfer_raw(struct ddc_service *ddc,
		struct aux_payload *payload,
		enum aux_return_code_type *operation_result);
#endif  

