#!/usr/bin/env python3
"""Compile reviewed CSV ranges, without executing text from a profile."""
import csv,math,sys
from pathlib import Path
FIELDS='kind,min_x,max_x,min_z,max_z,min_abs_z,max_abs_z,min_sweep,max_sweep,min_ms,max_ms,min_coherence,min_g,max_g'.split(',')
def generate(source,destination):
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
    destination.parent.mkdir(parents=True,exist_ok=True)
    destination.write_text('// Generated from config/gesture-rules.csv. Do not edit.\nstatic const dt_gesture_rule gesture_rules[7]={\n'+',\n'.join(generated)+'\n};\n')
if __name__=='__main__':
    try:generate(Path(sys.argv[1]),Path(sys.argv[2]))
    except (ValueError,IndexError) as error:raise SystemExit(str(error))
