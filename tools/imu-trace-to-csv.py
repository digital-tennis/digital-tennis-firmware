#!/usr/bin/env python3
"""Extract bounded developer DTIMU rows; never opens a serial port or Bluetooth."""
import argparse,csv,re
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('log',type=Path);p.add_argument('output',type=Path);a=p.parse_args()
rows=[]
for line in a.log.read_text().splitlines():
    if 'DTIMU,' not in line: continue
    fields=line[line.index('DTIMU,')+6:].split(',')
    if len(fields)!=7 or any(not re.fullmatch(r'-?\d+',f) for f in fields): raise SystemExit('Malformed DTIMU row')
    time,*values=map(int,fields)
    if not 0<=time<=0xffffffff or any(abs(v)>8000000 for v in values): raise SystemExit('Out of range DTIMU row')
    rows.append([time,*[v/1000 for v in values]])
if not rows: raise SystemExit('No DTIMU rows')
with a.output.open('x',newline='') as f:
    writer=csv.writer(f);writer.writerow(['time_ms','ax_mps2','ay_mps2','az_mps2','gx_dps','gy_dps','gz_dps']);writer.writerows(rows)
print(f'{len(rows)} samples written; timestamp gaps are preserved, not interpolated')
