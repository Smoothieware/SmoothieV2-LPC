/*
        Copyright 2006 Arastra, Inc.
        Copyright 2001, 2002 Georges Menie (www.menie.org)
    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.
    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Lesser General Public License for more details.
    You should have received a copy of the GNU Lesser General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

// Modified to work within smoothie and ported to c++

#include "ymodem.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>

#include "FreeRTOS.h"
#include "task.h"

YModem::YModem(txfunc_t txfunc) : txfnc(txfunc)
{
	xbuff= (unsigned char*) malloc(1030); /* 1024 for YModem 1k + 3 head chars + 2 crc + nul */
}

YModem::~YModem()
{
	free(xbuff);
}

void YModem::add(char c)
{
	while(inbuf.full()) {
		vTaskDelay(pdMS_TO_TICKS(10));
	}
	inbuf.push_back(c);
}

int YModem::_inbyte(int msec)
{
	while (inbuf.empty()) {
		vTaskDelay(pdMS_TO_TICKS(1));
		if (msec-- <= 0)
			return -1;
	}
	int c= inbuf.pop_front();
	return c;
}

void YModem::_outbyte(unsigned char c)
{
	txfnc(c);
}

unsigned short YModem::crc16_ccitt(const unsigned char *buf, int sz)
{
	unsigned short crc = 0;
	while (--sz >= 0) {
		int i;
		crc ^= (unsigned short) *buf++ << 8;
		for (i = 0; i < 8; i++)
			if (crc & 0x8000)
				crc = crc << 1 ^ 0x1021;
			else
				crc <<= 1;
	}
	return crc;
}
#define SOH  0x01
#define STX  0x02
#define EOT  0x04
#define ACK  0x06
#define NAK  0x15
#define CAN  0x18
#define CTRLZ 0x1A
#define DLY_1S 1000
#define MAXRETRANS 25

int YModem::check(int crc, const unsigned char *buf, int sz)
{
	if (crc) {
		unsigned short xcrc = crc16_ccitt(buf, sz);
		unsigned short tcrc = (buf[sz]<<8)+buf[sz+1];
		if (xcrc == tcrc)
			return 1;
	}
	else {
		int i;
		unsigned char cks = 0;
		for (i = 0; i < sz; ++i) {
			cks += buf[i];
		}
		if (cks == buf[sz])
		return 1;
	}
	return 0;
}
void YModem::flushinput(void)
{
	while (_inbyte(((DLY_1S)*3)>>1) >= 0)
		;
}

int YModem::receive()
{
	int file_size= 0;
	int err_ret;
	int first_packet= 1;
	char fn[132];
	FILE *fp= NULL;
	int filecnt= 0;
	unsigned char *p;
	int bufsz, crc = 0;
	unsigned char trychar = 'C';
	unsigned char packetno = 0;
	int i, c, len= 0;
	int retry, retrans = MAXRETRANS;

restart:
	for(;;) {
		for( retry = 0; retry < 16; ++retry) {
			if (trychar) _outbyte(trychar);
			if ((c = _inbyte((DLY_1S)<<1)) >= 0) {
				switch (c) {
				case SOH:
					bufsz = 128;
					goto start_recv;
				case STX:
					bufsz = 1024;
					goto start_recv;
				case EOT:
					// ymodem doesn't end here
					_outbyte(ACK);
					if(fp != NULL) {
						// close file
						fclose(fp);
						fp= NULL;
						filecnt++;
					}
					trychar = 'C';
					packetno = 0;
					retrans = MAXRETRANS;
					first_packet= 1;
					len= 0;
					goto restart;
				case CAN:
					if ((c = _inbyte(DLY_1S)) == CAN) {
						flushinput();
						_outbyte(ACK);
						return -1; /* canceled by remote */
					}
					break;
				default:
					break;
				}
			}
		}
		if (trychar == 'C') { trychar = NAK; continue; }
		flushinput();
		_outbyte(CAN);
		_outbyte(CAN);
		_outbyte(CAN);
		return -2; /* sync error */
	start_recv:
		if (trychar == 'C') crc = 1;
		trychar = 0;
		p = xbuff;
		*p++ = c;
		for (i = 0;  i < (bufsz+(crc?1:0)+3); ++i) {
			if ((c = _inbyte(DLY_1S)) < 0) goto reject;
			*p++ = c;
		}
		if (xbuff[1] == (unsigned char)(~xbuff[2]) &&
			(xbuff[1] == packetno || xbuff[1] == (unsigned char)packetno-1) &&
			check(crc, &xbuff[3], bufsz)) {
			if (xbuff[1] == packetno)	{
				if(first_packet) {
					first_packet= 0;
					// get filename and size starting at offset 3
					if(xbuff[3] == 0) {
						// end of batch
						flushinput();
						_outbyte(ACK);
						return filecnt; /* normal end */
					}

					// get filename
					strncpy(fn, (char *)&xbuff[3], sizeof(fn)-1);
					// get file size
					char s[16];
					for (size_t j = 0; j < sizeof(s)-1; ++j) {
						s[j]= 0;
						char cc = xbuff[3+strlen(fn)+1+j];
					    if(!isdigit(cc)) break;
					    s[j]= cc;
					}
					file_size= atoi(s);
					//printf("DEBUG: ymodem filename: <%s>, file size: %d\n", fn, file_size);
					fp= fopen(fn, "w");
					if(fp == NULL) {
						err_ret= -4;
						goto cancel;
					}
					trychar= 'C';

				}else{
					size_t n= bufsz;
					if((len+bufsz) > file_size) {
						// last packet, so truncate to file_size
						n= file_size-len;
					}
					if(fwrite(&xbuff[3], 1, n, fp) != n) {
						fclose(fp);
						err_ret= -5;
						goto cancel;
					}
					len += bufsz;
				}

				++packetno;
				retrans = MAXRETRANS+1;
			}
			if (--retrans <= 0) {
				err_ret= -3; /* too many retry error */
cancel: 		flushinput();
				_outbyte(CAN);
				_outbyte(CAN);
				_outbyte(CAN);
				return err_ret;
			}
			_outbyte(ACK);
			continue;
		}
reject:
		flushinput();
		_outbyte(NAK);
	}
}
