#
#  shuffle_mp3.py - MP3 file random player on MicroPython for X680x0
#

import x68k
import os
import sys
import time
import random
from struct import pack
from uctypes import addressof

def list_files(wildcard_name):

  #files = os.listdir(".")    # seems os.listdir() is not implemented yet

  files = []
  filbuf = bytearray(53)
  if x68k.dos(x68k.d.FILES,pack('llh',addressof(filbuf),addressof(wildcard_name),0x20)) < 0:
    print("error: files dos call error.")
    return None

  while True:

    #files.append(filbuf[30:52].rstrip(b'\x00'))   # seems bytearray.rstrip() is not implemented yet

    packedname = filbuf[30:52]
    while packedname[-1] == 0x00:
      packedname = packedname[:-1]

    files.append(packedname.decode())

    if x68k.dos(x68k.d.NFILES,pack('l',addressof(filbuf))) < 0:
      break

  return files
  
def main():

  # randomize
  random.seed(int(time.time() * 10))

  # list mp3 files in the current directory
  files = list_files(".\\*.mp3")

  # infinite loop
  loop_count = int(sys.argv[1]) if len(sys.argv) > 1 else 1
  
  # return code
  rc = 0

  # main loop
  for i in range(loop_count):

    # shuffle files
  #  files = random.sample(files, len(files)):     # seems random.sample() is not implemented
    for i in range(len(files)*3):
      a = random.randint(0,len(files)-1)
      b = random.randint(0,len(files)-1)
      f = files[a]
      files[a] = files[b]
      files[b] = f

    # execute mp3exp as a child process
    for f in files:
      x68k.crtmod(16,True)            # reset screen
      cmd = f"mp3exp -u -t50 -q1 {f}"
      print(f"COMMAND: {cmd}\n")
      rc = os.system(cmd)
      if rc != 0:                     # seems os.system() cannot return program exit code
        break

    # abort check
    if rc != 0:
      break

if __name__ == "__main__":
  main()
