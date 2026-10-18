#!/usr/bin/env python3
"""Compile only. Never discovers, pairs, erases or flashes a physical board."""
import argparse,json,os,subprocess
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--sdk-root',type=Path,required=True,help='Existing NCS workspace containing zephyr and .west')
p.add_argument('--toolchain',type=Path,required=True,help='Existing Nordic toolchain containing environment.json')
p.add_argument('--protocol',type=Path,help='Optional checked local protocol source; otherwise pinned FetchContent')
p.add_argument('--overlay',type=Path,help='Only a reviewed external-IMU hardware overlay')
p.add_argument('--pristine',action='store_true',help='Recreate only the selected generated build directory')
p.add_argument('--out',type=Path,default=Path('.work/zephyr-build'))
a=p.parse_args();root=Path(__file__).resolve().parents[1];sdk=a.sdk_root.resolve();tool=a.toolchain.resolve();out=a.out.resolve();env=os.environ.copy()
if any(folder==out or folder.is_relative_to(out) for folder in [root,sdk,tool]):raise SystemExit('Build output must not contain source, SDK or toolchain')
if a.pristine and out.exists() and not (out/'CMakeCache.txt').exists():raise SystemExit('Refusing pristine on a directory without a generated CMake cache')
for var in json.loads((tool/'environment.json').read_text())['env_vars']:
 value=var.get('value') if var['type']=='string' else os.pathsep.join(str(tool/v) for v in var['values'])
 if var.get('existing_value_treatment')=='prepend_to' and env.get(var['key']):value+=os.pathsep+env[var['key']]
 env[var['key']]=value
env['ZEPHYR_BASE']=str(sdk/'zephyr')
args=[str(tool/'bin/python3'),'-m','west','build',*(['-p','always'] if a.pristine else []),'--no-sysbuild','-b','nu40dk_kit/nrf52840',str(root),'-d',str(out),'--','-DDT_ZEPHYR_APP=ON','-DUSER_CACHE_DIR='+str(out.parent/'zephyr-cache')]
if a.protocol:args+=['-DDT_PROTOCOL_ROOT='+str(a.protocol.resolve())]
if a.overlay:args+=['-DDTC_OVERLAY_FILE='+str(a.overlay.resolve())]
raise SystemExit(subprocess.run(args,cwd=sdk,env=env).returncode)
