#include "Spi.h"

#include "board.h"

SPI::SPI(int channel)
{
    /* SSP initialization */
	if(channel == 0) {
    	_lpc_ssp= LPC_SSP0;
    }else{
    	_lpc_ssp= LPC_SSP1;
    }

    _channel= channel;

  	Chip_SSP_Init((LPC_SSP_T*)_lpc_ssp);
	frequency(100000);
	format(8, 3);
	Chip_SSP_Enable((LPC_SSP_T*)_lpc_ssp);
}

SPI::~SPI()
{
	Chip_SSP_Disable((LPC_SSP_T*)_lpc_ssp);
	Chip_SSP_DeInit((LPC_SSP_T*)_lpc_ssp);
}

void SPI::format(int bits, int mode)
{
	SSP_ConfigFormat ssp_format;
	ssp_format.frameFormat = SSP_FRAMEFORMAT_SPI;
	switch(bits) {
		case 4: ssp_format.bits= SSP_BITS_4; break;
		case 5: ssp_format.bits= SSP_BITS_5; break;
		case 6: ssp_format.bits= SSP_BITS_6; break;
		case 7: ssp_format.bits= SSP_BITS_7; break;
		case 8: ssp_format.bits= SSP_BITS_8; break;
		case 9: ssp_format.bits= SSP_BITS_9; break;
		case 10: ssp_format.bits= SSP_BITS_10; break;
		case 11: ssp_format.bits= SSP_BITS_11; break;
		case 12: ssp_format.bits= SSP_BITS_12; break;
		case 13: ssp_format.bits= SSP_BITS_13; break;
		case 14: ssp_format.bits= SSP_BITS_14; break;
		case 15: ssp_format.bits= SSP_BITS_15; break;
		case 16: ssp_format.bits= SSP_BITS_16; break;
		default: return;
	}

	switch(mode) {
		case 0: ssp_format.clockMode = SSP_CLOCK_MODE0; break;
		case 1: ssp_format.clockMode = SSP_CLOCK_MODE1; break;
		case 2: ssp_format.clockMode = SSP_CLOCK_MODE2; break;
		case 3: ssp_format.clockMode = SSP_CLOCK_MODE3; break;
		default: return;
	}

    Chip_SSP_SetFormat((LPC_SSP_T*)_lpc_ssp, ssp_format.bits, ssp_format.frameFormat, ssp_format.clockMode);
    _bits= bits;
    _mode= mode;
}

void SPI::frequency(int hz)
{
	Chip_SSP_SetBitRate((LPC_SSP_T*)_lpc_ssp, hz);
	_hz= hz;
}

int SPI::write(int value)
{
	Chip_SSP_DATA_SETUP_T xf_setup;
	int n= _bits>8?2:1;
	uint8_t tx_buf[n];
	uint8_t rx_buf[n];

	tx_buf[0]= value&0xFF;
	if(n>1){
		tx_buf[1]= value>>8;
	}
	xf_setup.length = n;
	xf_setup.tx_data = tx_buf;
	xf_setup.rx_data = rx_buf;
	xf_setup.rx_cnt = xf_setup.tx_cnt = 0;

	Chip_SSP_RWFrames_Blocking((LPC_SSP_T*)_lpc_ssp, &xf_setup);
	int ret= rx_buf[0];
	if(n>1) {
		ret |= (rx_buf[1]<<8);
	}

	return ret;
}

