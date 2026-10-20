#!/usr/bin/env python3
"""Generate physical synthetic IMU rows and call the firmware C executable.
The name/expected label never reaches the classifier; context is explicit match state.
"""
import argparse,csv,hashlib,io,json,subprocess,tempfile
from pathlib import Path
from physics import generate,write_csv,NEGATIVES
ROOT=Path(__file__).resolve().parents[1]
def recognize(data,binary,context='unknown'):
    if context not in ('unknown','serve','rally'):raise ValueError('Invalid context')
    with tempfile.TemporaryDirectory() as folder:
        path=Path(folder)/'imu.csv';write_csv(data,path)
        args=[str(Path(binary).resolve()),'--gestures','--diagnostics','--context='+context,'--axis='+','.join(format(x,'.9g') for x in data['axis'])]
        if data['parameters']['left']:args.append('--left')
        process=subprocess.run([*args,str(path)],capture_output=True,text=True,timeout=10)
    rows=[]
    for row in csv.DictReader(io.StringIO(process.stdout)):
        rows.append({k:(v if k=='packet_hex' else float(v)) for k,v in row.items()})
    data['recognition']={'context':context,'calibrated':process.returncode==0,'error':process.stderr.strip(),'decisions':rows,'events':[r for r in rows if r['kind']>0]}
    return data
def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--motion',default='forehand');p.add_argument('--binary',type=Path,default=ROOT/'.work/motion-simulator/imu_replay');p.add_argument('--out',type=Path,required=True)
    p.add_argument('--context',choices=['unknown','serve','rally'],default='unknown');p.add_argument('--seed',type=int,default=1);p.add_argument('--speed',type=float,default=1);p.add_argument('--amplitude',type=float,default=1);p.add_argument('--noise',type=float,default=1);p.add_argument('--mount',type=float,default=0);p.add_argument('--left',action='store_true')
    a=p.parse_args();data=recognize(generate(a.motion,a.seed,a.speed,a.amplitude,a.noise,a.mount,a.left),a.binary,a.context);a.out.parent.mkdir(parents=True,exist_ok=True);a.out.write_text(json.dumps(data,ensure_ascii=False)+'\n')
    print(json.dumps({k:v for k,v in data.items() if k!='frames'},ensure_ascii=False,indent=2))
if __name__=='__main__':main()
