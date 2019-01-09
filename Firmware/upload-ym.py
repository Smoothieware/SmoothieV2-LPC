#!/usr/bin/python3
import serial
import sys
import subprocess
import signal
import traceback
import os
import argparse

def signal_term_handler(signal, frame):
   global intrflg
   print('got SIGTERM...')
   sys.quit()

signal.signal(signal.SIGTERM, signal_term_handler)

# Define command line argument interface
parser = argparse.ArgumentParser(description='ymodem upload file to smoothie')
parser.add_argument('file', help='filename to be uploaded')
parser.add_argument('device', help='Smoothie Serial Device')
parser.add_argument('-v','--verbose',action='store_true', default=False, help='verbose output')
parser.add_argument('-f','--flash',action='store_true', default=False, help='flash')
args = parser.parse_args()

file_path= args.file
dev= "/dev/tty{}".format(args.device)

print("Uploading file: {} to {}".format(file_path, dev))

fin= open(dev, "rb", buffering=0)
fout= open(dev, "wb", buffering=0)

fout.write(b'\n')
rep1= fin.readline()
fout.write(b'ry -q\n')
#rep2= fin.readline()

if args.verbose:
    print(rep1.decode('utf-8'))
    print(rep2.decode('utf-8'))
    varg= '-vvv'
else:
    varg= '-q'

try:
    p = subprocess.Popen(['sx', '--ymodem', varg, file_path], bufsize=0, stdin=fin, stdout=fout, stderr=sys.stderr)
    result, err = p.communicate()
    if p.returncode != 0:
        print("Failed")

    else:
        print("uploaded ok")
        if args.flash:
            fout.write(b'rm flashme.bin\n')
            fout.write(b'mv smoothiev2.bin flashme.bin\n')
            print("now do flash")
            
except:
    print('Exception: {}'.format(traceback.format_exc()))

fin.close()
fout.close()
