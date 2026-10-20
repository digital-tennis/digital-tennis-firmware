#!/usr/bin/env python3
"""Compile reviewed CSV ranges, without executing text from a profile."""
import csv,math,sys
from pathlib import Path
FIELDS='kind,min_x,max_x,min_z,max_z,min_abs_z,max_abs_z,min_sweep,max_sweep,min_ms,max_ms,min_coherence,min_g,max_g'.split(',')
def generate(source,destination,detector=None):
    with source.open(newline='') as f:
        reader=csv.DictReader(f)
        if reader.fieldnames!=FIELDS: raise ValueError('Unexpected gesture CSV header')
        rows=list(reader)
    if len(rows)!=7: raise ValueError('Exactly seven tennis rules are required')
    generated=[]
    for kind,row in enumerate(rows,1):
        if set(row)!=set(FIELDS) or any(v is None for v in row.values()): raise ValueError('Malformed rule')
        values=[float(row[key]) for key in FIELDS]
        if any(not math.isfinite(v) for v in values) or values[0]!=kind: raise ValueError('Invalid kind/value')
        v=dict(zip(FIELDS,values))
        for lo,hi,low,high in [('min_x','max_x',-1,1),('min_z','max_z',-1,1),('min_abs_z','max_abs_z',0,1),('min_sweep','max_sweep',0,360),('min_ms','max_ms',10,1200),('min_g','max_g',1,28)]:
            if not low<=v[lo]<=v[hi]<=high: raise ValueError('Invalid '+lo+'/'+hi)
        if not 0<=v['min_coherence']<=1:raise ValueError('Invalid coherence')
        generated.append('{'+str(kind)+','+','.join(format(x,'.9e')+'f' for x in values[1:])+'}')
    header='// Generated from reviewed gesture CSV files. Do not edit.\n#ifdef DT_GESTURE_INCLUDE_RULES\nstatic const dt_gesture_rule gesture_rules[7]={\n'+',\n'.join(generated)+'\n};\n#endif\n'
    if detector:
        keys='filter_alpha,trigger_dps,release_dps,cooldown_ms,min_swing_ms,max_swing_ms'.split(',')
        with detector.open(newline='') as f:
            reader=csv.DictReader(f)
            if reader.fieldnames!=keys:raise ValueError('Unexpected detector header')
            settings=list(reader)
        if len(settings)!=1 or set(settings[0])!=set(keys):raise ValueError('Exactly one detector row required')
        v={k:float(settings[0][k]) for k in keys}
        if any(not math.isfinite(x) for x in v.values()):raise ValueError('Non-finite detector')
        if not .05<=v['filter_alpha']<=1 or not 20<=v['release_dps']<v['trigger_dps']<=1000:raise ValueError('Invalid detector thresholds')
        if not 20<=v['cooldown_ms']<=5000 or not 10<=v['min_swing_ms']<v['max_swing_ms']<=1200:raise ValueError('Invalid detector duration')
        if any(v[k]!=int(v[k]) for k in keys[3:]):raise ValueError('Detector durations must be integer milliseconds')
        for k,value in v.items():header+='#define DT_GESTURE_'+k.upper()+' '+(str(int(value))+'U' if k.endswith('_ms') else format(value,'.9e')+'f')+'\n'
    destination.parent.mkdir(parents=True,exist_ok=True)
    destination.write_text(header)
if __name__=='__main__':
    try:generate(Path(sys.argv[1]),Path(sys.argv[2]),Path(sys.argv[3]) if len(sys.argv)>3 else None)
    except (ValueError,IndexError) as error:raise SystemExit(str(error))
