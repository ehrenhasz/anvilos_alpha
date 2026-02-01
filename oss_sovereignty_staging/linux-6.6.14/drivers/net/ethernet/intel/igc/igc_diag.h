 
 

bool igc_reg_test(struct igc_adapter *adapter, u64 *data);
bool igc_eeprom_test(struct igc_adapter *adapter, u64 *data);
bool igc_link_test(struct igc_adapter *adapter, u64 *data);

struct igc_reg_test {
	u16 reg;
	u8 array_len;
	u8 test_type;
	u32 mask;
	u32 write;
};

 

#define PATTERN_TEST	1
#define SET_READ_TEST	2
#define TABLE32_TEST	3
#define TABLE64_TEST_LO	4
#define TABLE64_TEST_HI	5
