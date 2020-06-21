#!/usr/bin/env python
# boot-uart.py
# load a bootstrap into an LPC4330 over a UART

from __future__ import print_function
import sys
import argparse
import serial
import threading
import time
import signal
import sys
import os
from struct import *

errorflg = False
intrflg = False


def signal_term_handler(signal, frame):
    global intrflg
    print('got SIGTERM...')
    intrflg = True


signal.signal(signal.SIGTERM, signal_term_handler)

# Define command line argument interface
parser = argparse.ArgumentParser(description='Bootloader over UART')
parser.add_argument('file', type=argparse.FileType('rb'), help='image to be loaded')
parser.add_argument('device', help='Serial Device')
parser.add_argument('-q', '--quiet', action='store_true', default=False, help='suppress output text')
args = parser.parse_args()

f = args.file
dev = args.device
verbose = not args.quiet

# Open port
s = serial.Serial(dev, 115200, timeout=2)

old_file_position = f.tell()
f.seek(0, os.SEEK_END)
filelen = f.tell()
f.seek(old_file_position, os.SEEK_SET)

print("Loading {} ({}) to {}".format(f.name, filelen, args.device))

cnt = 0
# startup/sync sequence
while True:
    s.write("?".encode('latin1'))
    rep = s.read_until()
    if len(rep) > 2:
        ll = rep.decode('latin1')
        if 'OK' in ll:
            break
    cnt += 1
    if cnt > 10:
        # give up
        print("Timed out waiting for sync")
        sys.exit(1)

print("Synced")

flen = round((filelen+512)/512)

# send header
b1 = bytes.fromhex('DA FF')
# unlike the docs it seems to want the number of 512 byte blocks
b2 = pack('<HLLL', flen, 0, 0, 0xFFFFFFFF)
s.write(b1)
s.write(b2)

cnt = 0
# send file
try:
    while(True):
        b = f.read(1024)
        if len(b) == 0:
            break
        s.write(b)
        cnt += len(b)

except KeyboardInterrupt:
    print("Interrupted...")
    intrflg = True

# pad to next 512 byte block
while cnt < (flen*512):
    s.write(bytes.fromhex('00'))
    cnt += 1

# print("pad with: {}".format(len(b3)))

print("Sent file")

# we do not seem to get any response
# cnt = 0
# while True:
#     rep = s.read_until()
#     if len(rep) > 0:
#         print(rep.decode('latin1'))
#         break

#     cnt += 1
#     if cnt > 5:
#         # give up
#         print("Timed out waiting for response")
#         break

# Close file and serial port
f.close()
s.close()
