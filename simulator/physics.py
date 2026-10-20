"""Independent kinematic virtual IMU. The equations never read recognition rules.
The preset kind is evaluation metadata, never an input to inertial() or to C.

World: X right, Y toward net, Z up (right handed). Neutral racket Y is handle->head.
f_sensor = (R_world_body M_mount)^T (p_sensor'' - gravity).
The body offset produces tangential and centripetal acceleration without a fake hit spike.
"""
import csv,math,random
from pathlib import Path
G=9.80665
IDENTITY=((1.,0.,0.),(0.,1.,0.),(0.,0.,1.))
def transpose(a):return tuple(zip(*a))
def mm(a,b):return tuple(tuple(sum(a[i][k]*b[k][j] for k in range(3)) for j in range(3)) for i in range(3))
def mv(a,v):return tuple(sum(a[i][j]*v[j] for j in range(3)) for i in range(3))
def add(a,b):return tuple(x+y for x,y in zip(a,b))
def rx(a):
    c,s=math.cos(a),math.sin(a);return ((1,0,0),(0,c,-s),(0,s,c))
def rz(a):
    c,s=math.cos(a),math.sin(a);return ((c,-s,0),(s,c,0),(0,0,1))
def smooth(u):
    u=max(0,min(1,u));return u*u*u*(10+u*(-15+6*u))
def rotation_vector(r):
    angle=math.acos(max(-1,min(1,(sum(r[i][i] for i in range(3))-1)/2)))
    axis=(r[2][1]-r[1][2],r[0][2]-r[2][0],r[1][0]-r[0][1])
    scale=.5 if angle<1e-7 else angle/(2*math.sin(angle))
    return tuple(x*scale for x in axis)
def inertial(pose,t,mount=IDENTITY,offset=(0,.25,0),h=.001):
    """Finite difference positions and SO(3) log; not a stroke classifier."""
    states=[pose(t+d) for d in [-h,0,h]]
    positions=[add(p,mv(r,offset)) for p,r in states]
    world_acc=tuple((positions[2][i]-2*positions[1][i]+positions[0][i])/(h*h) for i in range(3))
    sensor_rotation=mm(states[1][1],mount)
    specific=mv(transpose(sensor_rotation),(world_acc[0],world_acc[1],world_acc[2]+G))
    before,after=mm(states[0][1],mount),mm(states[2][1],mount)
    angular=tuple(math.degrees(x/(2*h)) for x in rotation_vector(mm(transpose(before),after)))
    return specific,angular,positions[1],states[1][1]
def presets():
    with (Path(__file__).with_name('motions.csv')).open() as f:
        return {r['name']:{k:(v if k=='name' else float(v)) for k,v in r.items()} for r in csv.DictReader(f)}
def make_pose(profile,speed=1,amplitude=1,left=False):
    prefix=1.2;prepare=profile['prepare_s']/speed;hold=profile['hold_s']/speed;forward=profile['forward_s']/speed
    start=prefix+prepare+hold;end=start+forward;reset=1.8/speed
    def pose(t):
        if t<prefix:ax=az=push=0
        elif t<prefix+prepare:
            f=smooth((t-prefix)/prepare);ax=profile['back_x']*f;az=profile['back_z']*f;push=0
        elif t<start:ax=profile['back_x'];az=profile['back_z'];push=0
        elif t<end:
            f=smooth((t-start)/forward);ax=profile['back_x']+(profile['front_x']-profile['back_x'])*f;az=profile['back_z']+(profile['front_z']-profile['back_z'])*f;push=profile['push_m']*f
        else:
            f=1-smooth((t-end)/reset);ax=profile['front_x']*f;az=profile['front_z']*f;push=profile['push_m']*f
        r=mm(rz(math.radians(az*amplitude)),rx(math.radians(ax*amplitude)))
        p=(0,push,0)
        if left:
            mirror=((-1,0,0),(0,1,0),(0,0,1));r=mm(mm(mirror,r),mirror);p=mv(mirror,p)
        return p,r
    return pose,start,end,end+reset+.7
def negative_pose(name):
    def pose(t):
        u=max(0,t-1.2);envelope=smooth(u/.5)
        if name=='idle':return (0,0,0),IDENTITY
        if name=='walking':return (0,envelope*.035*math.sin(2*math.pi*1.5*u),envelope*.018*math.sin(2*math.pi*3*u)),rz(envelope*.06*math.sin(2*math.pi*1.5*u))
        if name=='translation_jolt':return (0,.22*math.exp(-((u-.7)/.07)**2),0),IDENTITY
        if name=='tremor':return (0,0,0),rz(envelope*.014*math.sin(2*math.pi*9*u))
        if name=='racket_twirl':return (0,0,0),rz(2*math.pi*smooth((u-.4)/.65))
        raise ValueError('Unknown negative motion')
    return pose,1.2,3.5,4.5
NEGATIVES=('idle','walking','translation_jolt','tremor','racket_twirl')
def generate(name,seed=1,speed=1,amplitude=1,noise=1,mount_deg=0,left=False,lever_m=.25):
    if name not in presets() and name not in NEGATIVES:raise ValueError('Unknown motion')
    if not .6<=speed<=1.5 or not .7<=amplitude<=1.3 or not 0<=noise<=3 or not 0<=mount_deg<=360 or not .1<=lever_m<=.4:raise ValueError('Parameter out of range')
    profile=presets().get(name);pose,start,end,total=make_pose(profile,speed,amplitude,left) if profile else negative_pose(name)
    return sample_trajectory(pose,start,end,total,name,int(profile['kind']) if profile else 0,seed,speed,amplitude,noise,mount_deg,left,lever_m)
def sample_trajectory(pose,start,end,total,name,expected,seed=1,speed=1,amplitude=1,noise=1,mount_deg=0,left=False,lever_m=.25):
    """Sensor equations shared with independent experimental motion families."""
    rng=random.Random(seed);mount=rz(math.radians(mount_deg));bias=[rng.uniform(-1,1) for _ in range(3)]
    frames=[];clipped=0
    for i in range(int(total*100)+1):
        t=i/100;a,w,position,r=inertial(pose,t,mount,(0,lever_m,0))
        values=[]
        for j,value in enumerate((*a,*w)):
            value+=rng.gauss(0,.02*G*noise if j<3 else 1.5*noise)+(0 if j<3 else bias[j-3])
            limit=16*G if j<3 else 2000
            if abs(value)>limit:clipped+=1
            value=max(-limit,min(limit,value));value=round(value/(limit/32768))*(limit/32768);values.append(value)
        frames.append({'time_ms':i*10,'sensor':values,'position':position,'rotation':r})
    return {'frames':frames,'axis':[x for row in mount for x in row],'start_ms':round(start*1000),'end_ms':round(end*1000),'expected':expected,'clippedAxes':clipped,'parameters':{'motion':name,'seed':seed,'speed':speed,'amplitude':amplitude,'noise':noise,'mount_deg':mount_deg,'left':left,'lever_m':lever_m}}
def write_csv(data,path):
    with Path(path).open('w',newline='') as f:
        w=csv.writer(f);w.writerow(['time_ms','ax_mps2','ay_mps2','az_mps2','gx_dps','gy_dps','gz_dps'])
        for frame in data['frames']:w.writerow([frame['time_ms'],*[format(x,'.8f') for x in frame['sensor']]])
