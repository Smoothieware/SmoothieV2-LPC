#pragma once

#include <functional>

#include "RingBuffer.h"

class YModem
{
public:
	using txfunc_t= std::function<void(char c)>;
	YModem(txfunc_t);
	virtual ~YModem();
	void add(char c);
	int receive();
	bool is_ok() const { return inbuf.is_ok(); }

private:
	int _inbyte(int msec);
	void _outbyte(unsigned char c);
	unsigned short crc16_ccitt(const unsigned char *buf, int sz);
	int check(int crc, const unsigned char *buf, int sz);
	void flushinput(void);

	txfunc_t txfnc;
	RingBuffer<char, 2048> inbuf;
	unsigned char xbuff[1030]; /* 1024 for YModem 1k + 3 head chars + 2 crc + nul */
};


