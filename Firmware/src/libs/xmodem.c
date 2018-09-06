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

// Modified to work within smoothie

#include "xmodem.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>

#include "FreeRTOS.h"
#include "task.h"

static char *inbuff;
static volatile int inptr= 0;
static volatile int outptr= 0;
void add_to_xmodem_inbuff(char c)
{
	taskENTER_CRITICAL();
	inbuff[inptr++]= c;
	inptr = inptr & 2047;
	taskEXIT_CRITICAL();
	while(inptr == outptr) {
		vTaskDelay(pdMS_TO_TICKS(10));
	}
}

static void (*txc)(char c);
void init_xmodem(void (*tx)(char c))
{
	inbuff= malloc(2048);
	inptr= outptr= 0;
	txc= tx;
}

void deinit_xmodem()
{
	free(inbuff);
}

static int _inbyte(int msec)
{
	while (outptr == inptr) {
		vTaskDelay(pdMS_TO_TICKS(1));
		if (msec-- <= 0)
			return -1;
	}
	taskENTER_CRITICAL();
	int c= inbuff[outptr++];
	outptr = outptr & 2047;
	taskEXIT_CRITICAL();
	return c;
}

static void _outbyte(unsigned char c)
{
	txc(c);
}
static unsigned short crc16_ccitt(const unsigned char *buf, int sz)
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
static int check(int crc, const unsigned char *buf, int sz)
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
static void flushinput(void)
{
	while (_inbyte(((DLY_1S)*3)>>1) >= 0)
		;
}
int xmodemReceive(FILE **fp, int use_ymodem, char *fn, int *fsize)
{
	unsigned char xbuff[1030]; /* 1024 for XModem 1k + 3 head chars + 2 crc + nul */
	int file_size= 0;
	int err_ret;
	int first_packet= 1;
	unsigned char *p;
	int bufsz, crc = 0;
	unsigned char trychar = 'C';
	unsigned char packetno = use_ymodem ? 0 : 1;
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
					if(use_ymodem) {
						//printf("DEBUG: got EOT\n");
						// ymodem we don't end here
						_outbyte(ACK);
						trychar = 'C';
						packetno = 0;
						retrans = MAXRETRANS;
						first_packet= 1;
						goto restart;
					}
done:				flushinput();
					_outbyte(ACK);
					return len; /* normal end */
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
				if(use_ymodem && first_packet) {
					first_packet= 0;
					// get filename and size starting at offset 3
					if(xbuff[3] == 0) {
						// end of batch
						//printf("DEBUG: got end of batch\n");
						goto done;
					}

					if(len > 0) {
						// we already got one file and we only allow one file at the moment
						// not sure this will work though
						err_ret= len;
						goto cancel;
					}

					// get filename
					strncpy(fn, (char *)&xbuff[3], 132-1);
					// get file size
					char s[16];
					for (int j = 0; j < sizeof(s)-1; ++j) {
						s[j]= 0;
						char cc = xbuff[3+strlen(fn)+1+j];
					    if(!isdigit(cc)) break;
					    s[j]= cc;
					}
					file_size= atoi(s);
					*fsize= file_size;
					//printf("DEBUG: ymodem filename: <%s>, file size: %d\n", fn, file_size);
					*fp= fopen(fn, "w");
					if(*fp == NULL) {
						err_ret= -4;
						goto cancel;
					}
					trychar= 'C';

				}else{
					if(fwrite(&xbuff[3], 1, bufsz, *fp) != bufsz) {
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
