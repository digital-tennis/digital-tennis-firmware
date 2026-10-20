#!/usr/bin/env python3
"""Fit/rank on two seeds, freeze CSV, then evaluate an untouched third seed.

The C controller supplies detection windows and features. Python fits/ranks
range candidates; compiled C must agree before holdout results are accepted.
No motion name/expected label is sent to the C subprocess.
"""
import argparse,csv,hashlib,io,itertools,json,math,random,statistics,subprocess
from pathlib import Path
from physics import (generate,presets,make_pose,negative_pose,NEGATIVES,mm,mv,
                     rx,rz,smooth,sample_trajectory,write_csv)
from run import ROOT

EXTRA_NEGATIVES=('axis_roll','small_adjustment','ready_lift','wrist_circle')
FIELDS='kind,min_x,max_x,min_z,max_z,min_abs_z,max_abs_z,min_sweep,max_sweep,min_ms,max_ms,min_coherence,min_g,max_g'.split(',')
DETECTOR_FIELDS='filter_alpha,trigger_dps,release_dps,cooldown_ms,min_swing_ms,max_swing_ms'.split(',')

def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def save(path,value):Path(path).write_text(json.dumps(value,ensure_ascii=False,indent=2)+'\n')
def plan():return json.loads((ROOT/'simulator/threshold-plan.json').read_text())

def negative(name,parameters):
    speed=parameters['speed'];amplitude=parameters['amplitude'];left=parameters['left']
    old=negative_pose(name)[0] if name in NEGATIVES else None
    def pose(t):
        u=max(0,(t-1.2)*speed)
        if old:
            p,r=old(1.2+u);p=tuple(x*amplitude for x in p)
        elif name=='axis_roll':
            a=math.radians(120*amplitude)*smooth((u-.2)/.6);c,s=math.cos(a),math.sin(a)
            p=(0,0,0);r=((c,0,s),(0,1,0),(-s,0,c))
        elif name=='small_adjustment':
            a=8*amplitude*(smooth((u-.2)/.2)-smooth((u-1)/.8))
            p=(0,0,0);r=rz(math.radians(a))
        elif name=='ready_lift':
            a=60*amplitude*(smooth((u-.2)/1)-smooth((u-1.7)/1))
            p=(0,0,.04*amplitude*smooth((u-.2)/1));r=rx(math.radians(a))
        else:
            q=2*math.pi*smooth((u-.2)/.7)
            p=(0,0,0);r=mm(rx(math.radians(30*amplitude*(1-math.cos(q)))),rz(math.radians(30*amplitude*math.sin(q))))
        if left:
            reflection=((-1,0,0),(0,1,0),(0,0,1))
            p=mv(reflection,p);r=mm(mm(reflection,r),reflection)
        return p,r
    return sample_trajectory(pose,1.2,1.2+2.3/speed,1.2+3.3/speed,name,0,**parameters)

def dataset(phase,folder):
    """Only the explicitly requested phase is generated or opened."""
    specification=plan()[phase];rng=random.Random(specification['seed']);folder=Path(folder)/phase
    folder.mkdir(parents=True,exist_ok=True);cases=[];variation=plan()['variation']
    for name in [*presets(),*NEGATIVES,*EXTRA_NEGATIVES]:
        for index in range(specification['perMotion']):
            parameters={key:rng.uniform(*bounds) for key,bounds in variation.items()}
            parameters.update(seed=rng.randrange(1,2147483647),mount_deg=rng.choice([0,90,180,270]),left=bool(index%2))
            profile=presets().get(name)
            side=-1 if profile and int(profile['kind']) in plan()['bothStrokeSidesForKinds'] and (index//2)%2 else 1
            expected_parameters=dict(parameters,motion=name,stroke_side=side)
            path=folder/f'{name}-{index:03d}.csv';metadata=path.with_suffix('.json')
            if metadata.exists():
                case=json.loads(metadata.read_text())
                if case['parameters']!=expected_parameters or case['csvSha256']!=sha(path):raise ValueError('Stale dataset; use a new output folder')
            else:
                if profile:
                    profile=dict(profile,back_z=profile['back_z']*side,front_z=profile['front_z']*side)
                    pose,start,end,total=make_pose(profile,parameters['speed'],parameters['amplitude'],parameters['left'])
                    data=sample_trajectory(pose,start,end,total,name,int(profile['kind']),**parameters)
                else:data=negative(name,parameters)
                data['parameters']['stroke_side']=side
                write_csv(data,path)
                force_peak=max((f for f in data['frames'] if data['start_ms']<=f['time_ms']<=data['end_ms']),key=lambda f:sum(v*v for v in f['sensor'][:3]))['time_ms']
                case={k:data[k] for k in ['expected','parameters','axis','start_ms','end_ms','clippedAxes']}
                case.update(csvSha256=sha(path),forcePeakMs=force_peak);save(metadata,case)
            cases.append(dict(case,path=str(path)))
    return cases

def observe(cases,binary,detector=None):
    records=[]
    for case in cases:
        args=[str(Path(binary).resolve()),'--physical','--diagnostics','--axis='+','.join(format(x,'.9g') for x in case['axis'])]
        if case['parameters']['left']:args.append('--left')
        if detector:
            args.extend('--'+key.replace('_','-')+'='+str(detector[key]) for key in ['trigger_dps','release_dps','filter_alpha'])
        result=subprocess.run([*args,case['path']],capture_output=True,text=True,timeout=10)
        if result.returncode:raise RuntimeError('C replay/calibration failed: '+result.stderr)
        decisions=[{k:(v if k=='packet_hex' else float(v)) for k,v in row.items()} for row in csv.DictReader(io.StringIO(result.stdout))]
        records.append({'case':case,'decisions':decisions})
    return records

def predict(feature,rules):
    if feature['active_ms']<=0:return 0
    x,z=feature['direction_x'],feature['direction_z'];matched=[]
    for r in rules:
        if (r['min_x']<=x<=r['max_x'] and r['min_z']<=z<=r['max_z'] and
            r['min_abs_z']<=abs(z)<=r['max_abs_z'] and r['min_sweep']<=feature['sweep_deg']<=r['max_sweep'] and
            r['min_ms']<=feature['active_ms']<=r['max_ms'] and feature['coherence']>=r['min_coherence'] and
            r['min_g']<=feature['peak_g']<=r['max_g']):matched.append(int(r['kind']))
    eligible=matched
    return eligible[0] if len(eligible)==1 else 0

def fitted_rules(records,margin):
    # Coordinate families encode the stroke hypotheses; numerical ranges come
    # from C features on the training split, with declared expansion margins.
    groups={kind:[] for kind in range(1,7)}
    for record in records:
        case=record['case'];expected=case['expected']
        if expected:
            groups[expected].extend(d for d in record['decisions'] if case['start_ms']<=d['time_ms']<=case['end_ms']+250 and d['active_ms']>0)
    rules=[]
    for kind,features in groups.items():
        if not features:return None
        def bounds(key,padding,lo,hi):
            return round(max(lo,min(f[key] for f in features)-padding*margin),3),round(min(hi,max(f[key] for f in features)+padding*margin),3)
        x=bounds('direction_x',.15,-1,1);z=bounds('direction_z',.15,-1,1)
        abs_z=(round(max(0,min(abs(f['direction_z']) for f in features)-.15*margin),3),round(min(1,max(abs(f['direction_z']) for f in features)+.15*margin),3))
        sweep=bounds('sweep_deg',10,1,300);duration=bounds('active_ms',40,10,1100)
        g=bounds('peak_g',.3,1.15,28)
        coherence=round(max(.5,min(f['coherence'] for f in features)-.08*margin),3)
        # The absolute Z interval retains both volley/slice/lob stroke sides
        # while rejecting rotations around the handle axis between them.
        rules.append(dict(zip(FIELDS,[kind,*x,*z,*abs_z,*sweep,*duration,coherence,g[0],28])))
    return rules

def score(records,rules=None):
    matrix=[[0]*7 for _ in range(7)];duplicate_cases=wrong_time=wrong_classes=0;latencies=[];misses=[]
    for record in records:
        case=record['case'];events=[]
        for decision in record['decisions']:
            kind=predict(decision,rules) if rules is not None else int(decision['kind'])
            if kind:events.append((kind,decision['time_ms']))
        expected=case['expected'];predicted=events[0][0] if len(events)==1 else 0;matrix[expected][predicted]+=1
        duplicate_cases+=int(len(events)>1)
        wrong_classes+=sum(int(expected>0 and kind!=expected) for kind,_ in events)
        wrong_time+=int(expected>0 and any(not case['start_ms']<=time<=case['end_ms']+250 for _,time in events))
        if expected and events:latencies.append(events[0][1]-case['forcePeakMs'])
        if expected and predicted!=expected:misses.append({'motion':case['parameters']['motion'],'expected':expected,'predicted':predicted,'events':events,'parameters':case['parameters']})
    recalls=[matrix[k][k]/max(1,sum(matrix[k])) for k in range(1,7)]
    # Count *cases with events*, including a duplicate pair whose matrix entry is 0.
    negative_fp=sum(int(r['case']['expected']==0 and any((predict(d,rules) if rules is not None else d['kind'])>0 for d in r['decisions'])) for r in records)
    return {'matrixRowsExpectedColumnsPredicted':matrix,'positiveCases':sum(sum(row) for row in matrix[1:]),'negativeCases':sum(matrix[0]),'correctPositive':sum(matrix[k][k] for k in range(1,7)),'macroRecall':statistics.mean(recalls),'worstClassRecall':min(recalls),'classRecall':recalls,'negativeFalsePositiveCases':negative_fp,'duplicateCases':duplicate_cases,'wrongTimeCases':wrong_time,'wrongClasses':wrong_classes,'decisionFromForcePeakMs':{'median':statistics.median(latencies) if latencies else None,'max':max(latencies) if latencies else None},'misses':misses}

def feasible(s):return not any(s[k] for k in ['negativeFalsePositiveCases','duplicateCases','wrongTimeCases','wrongClasses'])
def write_rules(path,rules):
    with Path(path).open('w',newline='') as f:
        writer=csv.DictWriter(f,FIELDS,lineterminator='\n');writer.writeheader();writer.writerows(rules)
def write_detector(path,detector):
    with Path(path).open('w',newline='') as f:
        writer=csv.DictWriter(f,DETECTOR_FIELDS,lineterminator='\n');writer.writeheader();writer.writerow(detector)

def tune(args):
    folder=args.out;folder.mkdir(parents=True,exist_ok=True)
    plan_hash=sha(ROOT/'simulator/threshold-plan.json')
    inputs={'plan':plan_hash,'physics':sha(ROOT/'simulator/physics.py'),'study':sha(Path(__file__)),'motions':sha(ROOT/'simulator/motions.csv'),'binary':sha(args.binary)}
    input_file=folder/'inputs.json'
    if input_file.exists() and json.loads(input_file.read_text())!=inputs:raise ValueError('Frozen study inputs changed; use a fresh output folder')
    if not input_file.exists() and (folder/'train').exists():raise ValueError('Unversioned dataset cache; use a fresh output folder')
    save(input_file,inputs)
    frozen=folder/'plan.json'
    if frozen.exists() and sha(frozen)!=plan_hash:raise ValueError('Study plan changed; use a fresh output folder')
    frozen.write_bytes((ROOT/'simulator/threshold-plan.json').read_bytes())
    train=dataset('train',folder);selection=dataset('selection',folder)
    print(f'Train {len(train)}, selection {len(selection)}; holdout not generated',flush=True)
    candidates=[];best=None
    grid=plan()['detectors']
    for trigger,release,alpha in itertools.product(grid['trigger_dps'],grid['release_dps'],grid['filter_alpha']):
        detector=dict(zip(DETECTOR_FIELDS,[alpha,trigger,release,200,20,1000]))
        cache=folder/f'features-{trigger}-{release}-{alpha}.json'
        if cache.exists():data=json.loads(cache.read_text())
        else:
            data={'train':observe(train,args.binary,detector),'selection':observe(selection,args.binary,detector)};save(cache,data)
        for margin in plan()['rangeMarginMultipliers']:
            rules=fitted_rules(data['train'],margin)
            if rules is None:continue
            train_score=score(data['train'],rules);selected_score=score(data['selection'],rules)
            candidate={'detector':detector,'margin':margin,'rules':rules,'train':train_score,'selection':selected_score}
            candidates.append(candidate)
            if not feasible(train_score) or not feasible(selected_score):continue
            rank=(selected_score['macroRecall'],selected_score['worstClassRecall'],-selected_score['decisionFromForcePeakMs']['max'],trigger,-margin)
            if best is None or rank>best[0]:best=(rank,candidate)
        print(f'gate {trigger}/{release}, alpha {alpha}: best selection '+(str(round(best[1]['selection']['macroRecall'],4)) if best else 'none feasible'),flush=True)
    save(folder/'candidates.json',candidates)
    if best is None:raise RuntimeError('No candidate satisfies false-input constraints; holdout remains unopened')
    chosen=best[1];write_rules(folder/'selected-rules.csv',chosen['rules']);write_detector(folder/'selected-detector.csv',chosen['detector'])
    manifest={'planSha256':plan_hash,'physicsSha256':sha(ROOT/'simulator/physics.py'),'studySha256':sha(Path(__file__)),'motionCsvSha256':sha(ROOT/'simulator/motions.csv'),'binarySha256':sha(args.binary),'selectedRulesSha256':sha(folder/'selected-rules.csv'),'selectedDetectorSha256':sha(folder/'selected-detector.csv'),'candidates':len(candidates),'selection':chosen}
    save(folder/'selection.json',manifest)
    print('FROZEN',json.dumps({k:chosen[k] for k in ['detector','margin','selection']},ensure_ascii=False),flush=True)

def evaluate(args):
    folder=args.out;manifest=json.loads((folder/'selection.json').read_text())
    for source,key in [(ROOT/'config/gesture-rules.csv','selectedRulesSha256'),(ROOT/'config/gesture-detector.csv','selectedDetectorSha256'),(ROOT/'simulator/threshold-plan.json','planSha256'),(ROOT/'simulator/physics.py','physicsSha256'),(Path(__file__),'studySha256'),(ROOT/'simulator/motions.csv','motionCsvSha256')]:
        if sha(source)!=manifest[key]:raise ValueError('Frozen inputs changed: '+str(source))
    # Before opening holdout, prove the Python selection matcher agrees with
    # the newly compiled C pipeline on BOTH fitting and selection cases.
    parity={}
    for phase in ['train','selection']:
        cases=dataset(phase,folder);records=observe(cases,args.binary);actual=score(records)
        predicted=score(records,manifest['selection']['rules'])
        if actual!=predicted:raise AssertionError('Python/C range parity failure on '+phase)
        if actual!=manifest['selection'][phase]:raise AssertionError('Compiled C differs from frozen candidate on '+phase)
        parity[phase]=actual
    print('Compiled C matches frozen candidate; opening untouched holdout',flush=True)
    cases=dataset('holdout',folder);records=observe(cases,args.binary);result=score(records)
    baseline=score(observe(cases,args.baseline)) if args.baseline else None
    acceptance=plan()['acceptance'];passed=(feasible(result) and result['macroRecall']>=acceptance['minimumMacroRecall'] and result['worstClassRecall']>=acceptance['minimumClassRecall'])
    report={'scope':'Synthetic evidence only, no physical IMU or human accuracy','frozen':{k:v for k,v in manifest.items() if k!='selection'},'detector':manifest['selection']['detector'],'rules':manifest['selection']['rules'],'train':parity['train'],'selection':parity['selection'],'holdout':result,'baselineSameHoldout':baseline,'acceptance':acceptance,'passed':passed,'finalBinarySha256':sha(args.binary),'perCase':records}
    save(folder/'evaluation.json',report)
    print(json.dumps({k:v for k,v in report.items() if k not in ['perCase','rules','train','selection','frozen']},ensure_ascii=False,indent=2))
    if not passed:raise SystemExit('Heldout acceptance failed; report failure, do not tune against this seed')

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('phase',choices=['tune','evaluate']);p.add_argument('--binary',type=Path,default=ROOT/'.work/motion-simulator/imu_replay');p.add_argument('--out',type=Path,required=True);p.add_argument('--baseline',type=Path)
    args=p.parse_args();(tune if args.phase=='tune' else evaluate)(args)
if __name__=='__main__':main()
