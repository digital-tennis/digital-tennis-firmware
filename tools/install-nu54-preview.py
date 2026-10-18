#!/usr/bin/env python3
"""Guarded NU54 Tennis application replacement. Default is offline plan only.

Requires pyocd/intelhex. --execute is a separate explicit hardware-write decision.
Never mass-erases/unlocks, writes UICR, or reads/mounts the wallet storage area.
"""
import argparse,hashlib,json,os
from pathlib import Path
from intelhex import IntelHex
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--image',type=Path,required=True);p.add_argument('--expected-sha256',required=True)
p.add_argument('--previous-image',type=Path,help='For a Tennis-only update: exact previously installed image')
p.add_argument('--previous-receipt',type=Path,help='Matching private successful installation receipt')
p.add_argument('--backup',type=Path);p.add_argument('--execute',action='store_true')
a=p.parse_args();image=a.image.resolve();digest=hashlib.sha256(image.read_bytes()).hexdigest()
if digest!=a.expected_sha256:raise SystemExit('Image differs from reviewed SHA256')
h=IntelHex(str(image));segments=h.segments()
if not segments or not all(0<=start<end<=0x30000 for start,end in segments):raise SystemExit('Only reviewed code range 0x0..0x30000 is allowed; UICR/other regions forbidden')
plan={'imageSha256':digest,'codeSegments':[[hex(s),hex(e)] for s,e in segments],'tennisSettings':['0x168000','0x170000'],'walletStorageExcluded':['0x174000','0x17d000'],'execute':a.execute}
if not a.execute:print(json.dumps(plan,indent=2));raise SystemExit(0)
if a.backup is None or a.backup.exists():raise SystemExit('Supply a NEW private backup directory')
os.umask(0o077);a.backup.mkdir(parents=True)
from pyocd.core.helpers import ConnectHelper
from pyocd.flash.file_programmer import FileProgrammer
session=ConnectHelper.session_with_chosen_probe(target_override='nrf54l',connect_mode='attach',options={'frequency':1000000,'auto_unlock':False,'halt_on_connect':False,'resume_on_disconnect':False})
if session is None:raise SystemExit('No probe selected')
def read_range(target,start,size):
 # Small transfers avoid USB probe bulk-read timeouts during final verification.
 return b''.join(bytes(target.read_memory_block8(at,min(512,start+size-at))) for at in range(start,start+size,512))
with session:
 t=session.target;region=t.get_memory_map().get_region_for_address(0)
 if region.blocksize!=4096:raise SystemExit('Unexpected flash sector geometry')
 # Updates verify previous Tennis code and never read bond keys or wallet storage.
 if a.previous_image or a.previous_receipt:
  if not (a.previous_image and a.previous_receipt):raise SystemExit('Supply both prior Tennis image and receipt')
  prior=json.loads(a.previous_receipt.read_text());old=IntelHex(str(a.previous_image))
  if not prior.get('writtenAndVerified') or hashlib.sha256(a.previous_image.read_bytes()).hexdigest()!=prior.get('imageSha256'):raise SystemExit('Prior Tennis receipt mismatch')
  if not all(0<=start<stop<=0x30000 for start,stop in old.segments()):raise SystemExit('Invalid prior code range')
  for start,stop in old.segments():
   if read_range(t,start,stop-start)!=bytes(old.tobinarray(start=start,end=stop-1)):raise SystemExit('Device code differs from previous Tennis image; stop')
  plan['verifiedTennisUpdate']=True
 else:
  # First installation only: the separate Tennis settings volume must be unused.
  unused=read_range(t,0x168000,0x8000)
  if any(b!=0xff for b in unused):raise SystemExit('Planned Tennis storage is not blank; stop without erasing it')
 t.halt();end=(max(e for s,e in segments)+4095)//4096*4096
 original=read_range(t,0,end);backup=a.backup/'previous-code.hex';restore=IntelHex();restore.frombytes(original,offset=0);restore.write_hex_file(str(backup))
 (a.backup/'installation-plan.json').write_text(json.dumps(plan|{'backupSha256':hashlib.sha256(backup.read_bytes()).hexdigest(),'backupEnd':hex(end)},indent=2)+'\n')
 FileProgrammer(session,chip_erase='sector',smart_flash=True,trust_crc=False,keep_unwritten=True).program(str(image),file_format='hex')
 for start,stop in segments:
  if read_range(t,start,stop-start)!=bytes(h.tobinarray(start=start,end=stop-1)):raise RuntimeError('Program verification mismatch; backup retained, do not proceed')
 plan['writtenAndVerified']=True
 (a.backup/'result.json').write_text(json.dumps(plan,indent=2)+'\n')
 t.reset()
print(json.dumps(plan,indent=2))
