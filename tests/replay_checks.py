"""Exercise the real capture converter and C classifier, using synthetic rows only."""
import csv,io,subprocess,sys,tempfile
from pathlib import Path
binary=Path(sys.argv[1]);converter=Path(__file__).resolve().parents[1]/'tools/imu-trace-to-csv.py'
with tempfile.TemporaryDirectory() as folder:
    root=Path(folder);rows=[];time=0
    for y,z,peak in [(0,500,4),(0,-500,8),(500,0,14)]:
        for _ in range(100):time+=10;rows.append([time,0,0,9807,0,0,0])
        for i in range(14):
            time+=10;rows.append([time,0,0,round((1+(peak-1)*i/13)*9806.65),0,y*1000,z*1000])
        time+=10;rows.append([time,0,0,round((peak-.25)*9806.65),0,y*1000,z*1000])
    for _ in range(30):time+=10;rows.append([time,0,0,9807,0,0,0])
    log=root/'uart.log';data=root/'data.csv'
    log.write_text('ordinary boot log\n'+'\n'.join('DTIMU,'+','.join(map(str,r)) for r in rows)+'\n')
    subprocess.run([sys.executable,str(converter),str(log),str(data)],check=True)
    output=subprocess.run([str(binary),str(data)],capture_output=True,text=True,check=True)
    packets=list(csv.DictReader(io.StringIO(output.stdout)))
    assert [int(p['kind']) for p in packets]==[1,2,3]
    assert [int(p['power']) for p in packets]==[17,58,100]
    assert all(len(bytes.fromhex(p['packet_hex']))==20 for p in packets)
    assert int(packets[0]['yaw_cdeg'])>0 and int(packets[1]['yaw_cdeg'])<0 and int(packets[2]['pitch_cdeg'])>0
    # A malformed row, non-finite source or absent stationary calibration fails.
    for value in ['time_ms,ax_mps2,ay_mps2,az_mps2,gx_dps,gy_dps,gz_dps\n1,0,0,nan,0,0,0\n','bad\n','1,0,0,9.8,0,500,0\n']:
        data.write_text(value);assert subprocess.run([str(binary),str(data)],capture_output=True).returncode!=0
    log.write_text('DTIMU,1,0,0,9807,0,0,not-a-number\n')
    assert subprocess.run([sys.executable,str(converter),str(log),str(root/'bad.csv')],capture_output=True).returncode!=0
print('PASS: UART extraction -> actual C calibration/classification -> three 20-byte packets; malformed inputs fail')
