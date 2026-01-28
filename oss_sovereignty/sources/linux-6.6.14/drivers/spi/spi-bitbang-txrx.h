




static inline u32
bitbang_txrx_be_cpha0(struct spi_device *spi,
		unsigned nsecs, unsigned cpol, unsigned flags,
		u32 word, u8 bits)
{
	

	u32 oldbit = (!(word & (1<<(bits-1)))) << 31;
	
	for (word <<= (32 - bits); likely(bits); bits--) {

		
		if ((flags & SPI_CONTROLLER_NO_TX) == 0) {
			if ((word & (1 << 31)) != oldbit) {
				setmosi(spi, word & (1 << 31));
				oldbit = word & (1 << 31);
			}
		}
		spidelay(nsecs);	

		setsck(spi, !cpol);
		spidelay(nsecs);

		
		word <<= 1;
		if ((flags & SPI_CONTROLLER_NO_RX) == 0)
			word |= getmiso(spi);
		setsck(spi, cpol);
	}
	return word;
}

static inline u32
bitbang_txrx_be_cpha1(struct spi_device *spi,
		unsigned nsecs, unsigned cpol, unsigned flags,
		u32 word, u8 bits)
{
	

	u32 oldbit = (!(word & (1<<(bits-1)))) << 31;
	
	for (word <<= (32 - bits); likely(bits); bits--) {

		
		setsck(spi, !cpol);
		if ((flags & SPI_CONTROLLER_NO_TX) == 0) {
			if ((word & (1 << 31)) != oldbit) {
				setmosi(spi, word & (1 << 31));
				oldbit = word & (1 << 31);
			}
		}
		spidelay(nsecs); 

		setsck(spi, cpol);
		spidelay(nsecs);

		
		word <<= 1;
		if ((flags & SPI_CONTROLLER_NO_RX) == 0)
			word |= getmiso(spi);
	}
	return word;
}

static inline u32
bitbang_txrx_le_cpha0(struct spi_device *spi,
		unsigned int nsecs, unsigned int cpol, unsigned int flags,
		u32 word, u8 bits)
{
	

	u8 rxbit = bits - 1;
	u32 oldbit = !(word & 1);
	
	for (; likely(bits); bits--) {

		
		if ((flags & SPI_CONTROLLER_NO_TX) == 0) {
			if ((word & 1) != oldbit) {
				setmosi(spi, word & 1);
				oldbit = word & 1;
			}
		}
		spidelay(nsecs);	

		setsck(spi, !cpol);
		spidelay(nsecs);

		
		word >>= 1;
		if ((flags & SPI_CONTROLLER_NO_RX) == 0)
			word |= getmiso(spi) << rxbit;
		setsck(spi, cpol);
	}
	return word;
}

static inline u32
bitbang_txrx_le_cpha1(struct spi_device *spi,
		unsigned int nsecs, unsigned int cpol, unsigned int flags,
		u32 word, u8 bits)
{
	

	u8 rxbit = bits - 1;
	u32 oldbit = !(word & 1);
	
	for (; likely(bits); bits--) {

		
		setsck(spi, !cpol);
		if ((flags & SPI_CONTROLLER_NO_TX) == 0) {
			if ((word & 1) != oldbit) {
				setmosi(spi, word & 1);
				oldbit = word & 1;
			}
		}
		spidelay(nsecs); 

		setsck(spi, cpol);
		spidelay(nsecs);

		
		word >>= 1;
		if ((flags & SPI_CONTROLLER_NO_RX) == 0)
			word |= getmiso(spi) << rxbit;
	}
	return word;
}
