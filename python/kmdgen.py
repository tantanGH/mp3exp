import sys
import os

# command line help
if len(sys.argv) < 8:
  print("usage: micropython kmdgen.py <total-seconds> <bpm> <beat-interval> <beat-skip> <event-offset> <erase-offset> <out-file>")
  sys.exit(1)

# command line parameters
total_seconds = int(sys.argv[1])
bpm = int(sys.argv[2])
beat_interval = int(sys.argv[3])
beat_skip = int(sys.argv[4])
event_offset = int(sys.argv[5])
erase_offset = int(sys.argv[6])
out_file = sys.argv[7]

try:

  if total_seconds <= 0:
    raise Exception("total-seconds must be 1 or more.")

  if bpm <= 0:
    raise Exception("bpm must be 1 or more.")

  if beat_interval <= 0:
    raise Exception("beat-interval must be 1 or more.")

  # output file existence check (MicroPython.x does not support os.stat,os.path yet)
  file_existence = False
  if sys.platform == 'human68k':
    import x68k
    from struct import pack
    from uctypes import addressof
    filbuf = bytearray(53)
    file_existence = True if x68k.dos(x68k.d.FILES,pack('llh',addressof(filbuf),addressof(out_file),0x23)) >= 0 else False
  else:
    file_existence = os.path.exists(out_file)

  if file_existence:
    print(f"Output file ({out_file}) already exists. Overwrite? (y/n)", end="")
    ans = input()
    if ans != 'y' and ans != 'Y':
      print("canceled.")
      sys.exit(1)

except Exception as e:
  print(e)
  sys.exit(1)


# total ticks (1 tick = 10 msec)
total_ticks = total_seconds * 100

# bpm * tick counter for down sampling
bpm_ticks = 0

# beat counter
beats = 0

# event counter
events = 0

# crlf code (MicroPython.x automatically convert \n to \r\n even in binary mode)
crlf = "\n" if sys.platform == 'human68k' else "\r\n"

# open output file
with open(out_file,"w") as f:

  # KMD header, KMD requires CRLF
  f.write("KMD100"+crlf)           
  f.write(crlf)

  # event tracking
  for t in range(1,total_ticks+1):

    # beat detection with down sampling, without floating point calculation
    bpm_ticks += bpm
    if bpm_ticks >= 60 * 100:
      beats += 1
      bpm_ticks -= 60 * 100

      # event detection with beat skips and intervals
      if beats >= beat_skip and ( beats - beat_skip ) % beat_interval == 0:

        # x,y position
        x = 0
        y = ( events % 2 ) * 2

        # event start time
        st = t - event_offset
        st_min = st // 100 // 60 
        st_sec = ( st // 100 ) % 60
        st_msec = st % 100

        # event end time
        et = st + int( ( 4 - y ) // 2 * beat_interval * 60 * 100 / bpm ) - erase_offset - 1
        et_min = et // 100 // 60
        et_sec = ( et // 100 ) % 60
        et_msec = et % 100

        # event template line
        ev = f"x{x},y{y},s{st_min:02d}:{st_sec:02d}:{st_msec:02d},e{et_min:02d}:{et_sec:02d}:{et_msec:02d},\"event {events+1}\""
        f.write(ev + crlf)

        # event counter
        events += 1
