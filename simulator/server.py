#!/usr/bin/env python3
"""Local-only lab viewer. No BLE, network game, physical device or Unity control."""
import argparse,json
from http.server import BaseHTTPRequestHandler,ThreadingHTTPServer
from pathlib import Path
from urllib.parse import urlsplit
from physics import generate,presets,NEGATIVES
from run import recognize,ROOT
class Handler(BaseHTTPRequestHandler):
    binary=None
    def respond(self,status,body,kind='application/json'):
        encoded=body if isinstance(body,bytes) else json.dumps(body,ensure_ascii=False,allow_nan=False).encode()
        self.send_response(status);self.send_header('Content-Type',kind+'; charset=utf-8');self.send_header('Content-Length',str(len(encoded)));self.send_header('Cache-Control','no-store');self.send_header('X-Content-Type-Options','nosniff');self.end_headers();self.wfile.write(encoded)
    def do_GET(self):
        if self.path=='/':self.respond(200,Path(__file__).with_name('index.html').read_bytes(),'text/html')
        elif self.path=='/api/info':self.respond(200,{'motions':list(presets())+list(NEGATIVES),'mode':'synthetic-lab-only','binary':'same firmware C controller and protocol'})
        else:self.respond(404,{'error':'없는 페이지입니다.'})
    def do_POST(self):
        if self.path!='/api/simulate':return self.respond(404,{'error':'없는 요청입니다.'})
        try:
            origin=self.headers.get('Origin')
            if origin and urlsplit(origin).netloc!=self.headers.get('Host'):return self.respond(403,{'error':'같은 실험 화면에서 요청해 주세요.'})
            length=int(self.headers.get('Content-Length','0'))
            if not 0<length<=4096 or self.headers.get_content_type()!='application/json':raise ValueError('request')
            data=json.loads(self.rfile.read(length))
            if not isinstance(data,dict) or set(data)-{'motion','seed','speed','amplitude','noise','mount_deg','left'}:raise ValueError('fields')
            seed=data.get('seed',1)
            if type(seed) is not int or not 0<=seed<=2147483647 or type(data.get('left',False)) is not bool:raise ValueError('type')
            numeric={k:data.get(k,v) for k,v in [('speed',1),('amplitude',1),('noise',1),('mount_deg',0)]}
            if any(type(v) not in [int,float] for v in numeric.values()):raise ValueError('number')
            case=generate(data.get('motion','forehand'),seed=seed,left=data.get('left',False),**numeric)
            self.respond(200,recognize(case,self.binary))
        except (ValueError,TypeError,KeyError):self.respond(400,{'error':'실험 값과 물리 궤적을 확인해 주세요.'})
        except Exception:self.respond(500,{'error':'C 재생기를 실행하지 못했습니다. 빌드와 서버 로그를 확인해 주세요.'})
    def log_message(self,format,*args):pass
def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--port',type=int,default=8766);p.add_argument('--binary',type=Path,default=ROOT/'.work/motion-simulator/imu_replay');a=p.parse_args()
    if not a.binary.is_file():raise SystemExit('먼저 CMake로 imu_replay를 빌드하세요.')
    Handler.binary=a.binary.resolve();server=ThreadingHTTPServer(('127.0.0.1',a.port),Handler)
    print(f'가상 센서 실험실 http://127.0.0.1:{server.server_port} · 실제 장치 연결 없음',flush=True)
    try:server.serve_forever()
    except KeyboardInterrupt:pass
    finally:server.server_close()
if __name__=='__main__':main()
