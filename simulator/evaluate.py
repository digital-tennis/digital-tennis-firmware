#!/usr/bin/env python3
"""Seed-separated physical synthetic campaigns; no human-accuracy claims."""
import argparse,hashlib,json,random,statistics,sys
from pathlib import Path
from physics import generate,presets,NEGATIVES
from run import recognize,ROOT
def campaign(binary,seed,repetitions,spread):
    rng=random.Random(seed);matrix=[[0]*8 for _ in range(8)];records=[];latencies=[];duplicates=0;wrong_time=0
    for name,p in presets().items():
        for i in range(repetitions):
            parameters={'speed':rng.uniform(1-spread,1+spread),'amplitude':rng.uniform(1-spread*.6,1+spread*.6),'noise':rng.uniform(.3,1.3),'mount_deg':rng.choice([0,90,180,270]),'left':bool(i%2),'lever_m':rng.uniform(.20,.30)}
            d=recognize(generate(name,seed*1000+i,**parameters),binary,'serve' if name=='serve' else 'rally')
            events=d['recognition']['events'];expected=int(p['kind']);predicted=int(events[0]['kind']) if len(events)==1 else 0
            matrix[expected][predicted]+=1;duplicates+=max(0,len(events)-1)
            if events:
                force_peak=max((f for f in d['frames'] if d['start_ms']<=f['time_ms']<=d['end_ms']),key=lambda f:sum(v*v for v in f['sensor'][:3]))['time_ms']
                latencies.append(events[0]['time_ms']-force_peak)
                wrong_time+=int(not d['start_ms']<=events[0]['time_ms']<=d['end_ms']+250)
            records.append({'motion':name,'seed':seed*1000+i,'expected':expected,'predicted':predicted,'events':len(events),'calibrated':d['recognition']['calibrated'],'parameters':parameters,'clippedAxes':d['clippedAxes']})
    for name in NEGATIVES:
        for i in range(repetitions):
            d=recognize(generate(name,seed*1000+i,noise=rng.uniform(.3,1.3)),binary,'rally');events=d['recognition']['events'];predicted=int(events[0]['kind']) if events else 0;matrix[0][predicted]+=1
            records.append({'motion':name,'seed':seed*1000+i,'expected':0,'predicted':predicted,'events':len(events),'calibrated':d['recognition']['calibrated']})
    positive=sum(sum(r) for r in matrix[1:]);correct=sum(matrix[i][i] for i in range(1,8));negative=sum(matrix[0]);fp=negative-matrix[0][0]
    per_class={str(i):{'precision':matrix[i][i]/max(1,sum(r[i] for r in matrix)),'recall':matrix[i][i]/max(1,sum(matrix[i]))} for i in range(1,8)}
    return {'seed':seed,'repetitionsPerMotion':repetitions,'matrixRowsExpectedColumnsPredicted':matrix,'positiveCases':positive,'positiveAccuracy':correct/positive,'negativeCases':negative,'negativeFalsePositiveCases':fp,'duplicates':duplicates,'wrongTimeEvents':wrong_time,'perClass':per_class,'decisionFromForcePeakMs':{'median':statistics.median(latencies) if latencies else None,'max':max(latencies) if latencies else None},'records':records}
def ambiguity(binary):
    base=generate('serve',20261020,noise=0);unknown=recognize(generate('serve',20261020,noise=0),binary,'unknown');serve=recognize(generate('serve',20261020,noise=0),binary,'serve');rally=recognize(generate('serve',20261020,noise=0),binary,'rally')
    def digest(d):return hashlib.sha256(json.dumps([[f['time_ms'],f['sensor']] for f in d['frames']],separators=(',',':')).encode()).hexdigest()
    assert digest(base)==digest(unknown)==digest(serve)==digest(rally)
    assert not unknown['recognition']['events'];assert [e['kind'] for e in serve['recognition']['events']]==[3];assert [e['kind'] for e in rally['recognition']['events']]==[4]
    assert any(int(e['candidates'])==24 and int(e['reason'])==4 for e in unknown['recognition']['decisions'])
    accidental=generate('forehand',20261020,noise=0);accidental['expected']=0;recognize(accidental,binary,'rally')
    assert [e['kind'] for e in accidental['recognition']['events']]==[1]
    return {'sameSixAxisValues':True,'sensorTraceSha256':digest(base),'unknownContext':'rejects ambiguous serve/smash','serveContext':3,'rallyContext':4,'identicalIntentCounterexample':{'metadataExpected':0,'observedKind':1,'meaning':'Changing an intention label cannot change identical inertial inputs; this hard negative is not one of the 500 benign negative cases.'},'note':'Context disambiguation, not distinct IMU patterns or a new learned skill.'}
def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--binary',type=Path,default=ROOT/'.work/motion-simulator/imu_replay');p.add_argument('--out',type=Path,required=True);p.add_argument('--repetitions',type=int,default=24);p.add_argument('--validation-seed',type=int,default=8129);a=p.parse_args()
    if not 1<=a.repetitions<=200 or a.validation_seed in [1701] or not 0<a.validation_seed<1000000:raise SystemExit('Invalid campaign parameters')
    calibration=campaign(a.binary,1701,12,.10)
    # These rules are not fitted here. Freeze the reviewed CSV before evaluating this seed.
    held_out=campaign(a.binary,a.validation_seed,a.repetitions,.25)
    nominal={}
    for name in presets():
        d=recognize(generate(name,noise=0),a.binary,'serve' if name=='serve' else 'rally');nominal[name]=[int(e['kind']) for e in d['recognition']['events']]
    result={'scope':'Kinematic synthetic consistency only; not human gesture accuracy','ruleSha256':hashlib.sha256((ROOT/'config/gesture-rules.csv').read_bytes()).hexdigest(),'motionPresetSha256':hashlib.sha256((ROOT/'simulator/motions.csv').read_bytes()).hexdigest(),'calibration':calibration,'heldOut':held_out,'nominal':nominal,'ambiguity':ambiguity(a.binary),'limitations':['Assumed fixed grip/body frame and stylized paths; no anatomy/soft tissue/ball impulse model','Full-window recognition waits for rotation release: reports latency; ordinary board app remains the earlier peak profile','A deliberate stroke and an identical accidental motion cannot be distinguished by these six channels','Match context is supplied explicitly; no current BLE command sends it to the board','No physical IMU, flash or additional NU54 button testing']}
    a.out.parent.mkdir(parents=True,exist_ok=True);a.out.write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n')
    print(json.dumps({k:v for k,v in result.items() if k not in ['calibration','heldOut']},ensure_ascii=False,indent=2))
    for label,c in [('calibration',calibration),('heldOut',held_out)]:print(label,json.dumps({k:v for k,v in c.items() if k!='records'},ensure_ascii=False))
    assert all(nominal[n]==[int(p['kind'])] for n,p in presets().items()),'Nominal gesture failure'
    assert held_out['positiveAccuracy']>=.80 and held_out['negativeFalsePositiveCases']==0 and held_out['duplicates']==0 and held_out['wrongTimeEvents']==0,'Synthetic acceptance failed'
if __name__=='__main__':main()
