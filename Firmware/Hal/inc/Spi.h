#pragma once
class SPI
{

public:
	SPI(int channel);
	virtual ~SPI();

	/** Configure the data transmission format
	 *
	 *  @param bits Number of bits per SPI frame (4 - 16)
	 *  @param mode Clock polarity and phase mode (0 - 3)
	 *
	 * @code
	 * mode | POL PHA
	 * -----+--------
	 *   0  |  0   0
	 *   1  |  0   1
	 *   2  |  1   0
	 *   3  |  1   1
	 * @endcode
	 */
	void format(int bits, int mode = 0);

	/** Set the spi bus clock frequency
	 *
	 *  @param hz SCLK frequency in hz (default = 1MHz)
	 */
	void frequency(int hz = 1000000);

	/** Write to the SPI Slave and return the response
	 *
	 *  @param value Data to be sent to the SPI slave
	 *
	 *  @returns
	 *    Response from the SPI slave
	*/
	int write(int value);

protected:
	void *_lpc_ssp;
	int _channel;
	int _bits;
	int _mode;
	int _hz;
};
