#!/usr/bin/env python3
"""Local-only browser interface. Every safety decision uses the compiled C core."""
import ctypes
import json
import os
from datetime import datetime
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from threading import RLock
from urllib.parse import urlparse

ROOT = Path(__file__).resolve().parent.parent
WEB = ROOT / 'web'
STORE = ROOT / 'runtime' / 'session.json'
LIB = ctypes.CDLL(str(ROOT / 'libbanker.so'))
I = ctypes.c_int
PI = ctypes.POINTER(I)
LIB.banker_validate.argtypes = [I, I, PI, PI, PI]
LIB.banker_validate.restype = I
LIB.banker_safety.argtypes = [I, I, PI, PI, PI, PI, PI]
LIB.banker_safety.restype = I
LIB.banker_request.argtypes = [I, I, PI, PI, PI, I, PI, PI]
LIB.banker_request.restype = I
LIB.banker_release.argtypes = [I, I, PI, PI, PI, I, PI]
LIB.banker_release.restype = I
LOCK = RLock()
DATA = {'state': None, 'history': []}


def cint(items):
    return (I * len(items))(*items)


def flatten(rows):
    return [v for row in rows for v in row]


def valid_integer(value):
    return type(value) is int and 0 <= value <= 1_000_000


def make_state(data):
    if not isinstance(data, dict):
        raise ValueError('Dataset must be a JSON object.')
    n, m = data.get('n'), data.get('m')
    if type(n) is not int or type(m) is not int or not (1 <= n <= 20) or not (1 <= m <= 10):
        raise ValueError('Choose 1–20 processes and 1–10 resource types.')
    av, alloc, maxi = data.get('available'), data.get('allocation'), data.get('maximum')
    if not isinstance(av, list) or len(av) != m or not all(valid_integer(v) for v in av):
        raise ValueError(f'Available requires exactly {m} non-negative integers.')
    for key, x in [('Allocation', alloc), ('Maximum', maxi)]:
        if not isinstance(x, list) or len(x) != n or any(not isinstance(row,list) or len(row) != m or not all(valid_integer(v) for v in row) for row in x):
            raise ValueError(f'{key} must have {n} rows, each with {m} non-negative integers.')
    for p in range(n):
        for r in range(m):
            if alloc[p][r] > maxi[p][r]:
                raise ValueError(f'P{p}, R{r+1}: Allocation cannot exceed Maximum.')
    totals = [av[r] + sum(alloc[p][r] for p in range(n)) for r in range(m)]
    for p in range(n):
        for r in range(m):
            if maxi[p][r] > totals[r]:
                raise ValueError(f'P{p}, R{r+1}: Maximum ({maxi[p][r]}) is greater than all {totals[r]} units in the system.')
    if LIB.banker_validate(n,m,cint(av),cint(flatten(alloc)),cint(flatten(maxi))) != 1:
        raise ValueError('Invalid resource quantities, dimensions or limits.')
    title = str(data.get('title') or 'Untitled allocation workspace').strip()[:80]
    return {'title':title,'n':n,'m':m,'available':list(av), 'allocation':[list(x) for x in alloc], 'maximum':[list(x) for x in maxi]}


def calculate(state):
    n,m=state['n'],state['m']
    a=cint(state['available']); alloc=cint(flatten(state['allocation']));mx=cint(flatten(state['maximum']))
    seq=(I*n)(); steps=(I*(n*m))()
    status=LIB.banker_safety(n,m,a,alloc,mx,seq,steps)
    if status == -1: raise ValueError('The stored resource state is invalid.')
    return {
        'safe':status==1,
        'sequence':[int(seq[i]) for i in range(n)] if status==1 else [],
        'steps':[[int(steps[i*m+j]) for j in range(m)] for i in range(n)] if status==1 else [],
        'need':[[state['maximum'][i][j]-state['allocation'][i][j] for j in range(m)] for i in range(n)],
        'total':[state['available'][j]+sum(state['allocation'][i][j] for i in range(n)) for j in range(m)]
    }


def response_state():
    if DATA['state'] is None: return {'ready':False,'history': DATA['history']}
    return {'ready':True, **DATA['state'], **calculate(DATA['state']), 'history': DATA['history']}


def save_data():
    STORE.parent.mkdir(parents=True,exist_ok=True)
    temp=STORE.with_suffix('.tmp')
    temp.write_text(json.dumps(DATA, indent=2),encoding='utf-8')
    os.replace(temp,STORE)


def log_event(kind, message, good=True):
    DATA['history'].insert(0,{'time':datetime.now().strftime('%H:%M:%S'),'kind':kind,'message':message,'good':good})
    DATA['history']=DATA['history'][:100]
    save_data()


def load_saved():
    if STORE.exists():
        try:
            raw=json.loads(STORE.read_text(encoding='utf-8'))
            state=make_state(raw['state']) if raw.get('state') else None
            history=raw.get('history',[])
            if not isinstance(history,list): history=[]
            DATA.update(state=state,history=history[:100])
        except (OSError,ValueError,TypeError,KeyError):
            print('[Notice] Invalid previous workspace ignored; please enter fresh inputs.')


def parse_request_vector(payload, m):
    req=payload.get('vector')
    if not isinstance(req,list) or len(req)!=m or not all(valid_integer(v) for v in req):
        raise ValueError(f'Enter exactly {m} non-negative resource values.')
    pid=payload.get('pid')
    if type(pid) is not int or not (0 <= pid < DATA['state']['n']):
        raise ValueError('Select an existing process.')
    if not any(req): raise ValueError('Enter at least one resource unit to change.')
    return pid,req


def modify(kind, payload):
    s=DATA['state']
    if s is None: raise ValueError('Create a workspace before submitting requests.')
    pid,req=parse_request_vector(payload,s['m'])
    available=cint(s['available']); alloc=cint(flatten(s['allocation']));maximum=cint(flatten(s['maximum']))
    seq=(I*s['n'])(); vec=cint(req)
    if kind=='request':
        status=LIB.banker_request(s['n'],s['m'],available,alloc,maximum,pid,vec,seq)
        msgs={0:'Request approved. The resulting state is SAFE.', 1:'Denied: request exceeds the process remaining Need.',2:'Denied: not enough Available resources.',3:'Denied: tentative allocation would be UNSAFE. Changes rolled back.',-1:'Denied: invalid allocation request.'}
    else:
        status=LIB.banker_release(s['n'],s['m'],available,alloc,maximum,pid,vec)
        msgs={0:'Release approved. Resources returned to Available.',1:'Denied: cannot release more resources than P%d holds.'%pid,-1:'Denied: invalid release.'}
    if status == 1 and kind == 'request':
        for j, r in enumerate(req):
            need = s['maximum'][pid][j] - s['allocation'][pid][j]
            if r > need:
                msgs[status] = f'Denied: P{pid} requests {r} of R{j+1}, but remaining Need is only {need}.'
                break
    if status == 2 and kind == 'request':
        for j, r in enumerate(req):
            if r > s['available'][j]:
                msgs[status] = f'Denied: {r} units of R{j+1} requested, but only {s["available"][j]} are available.'
                break
    if status == 1 and kind == 'release':
        for j, r in enumerate(req):
            if r > s['allocation'][pid][j]:
                msgs[status] = f'Denied: P{pid} holds only {s["allocation"][pid][j]} units of R{j+1}, cannot release {r}.'
                break
    if status == 0:
        s['available']=[int(available[j]) for j in range(s['m'])]
        s['allocation']=[[int(alloc[i*s['m']+j]) for j in range(s['m'])] for i in range(s['n'])]
    action_name = ('GRANTED' if kind == 'request' else 'RELEASED') if status == 0 else 'DENIED'
    message=f'{action_name} | P{pid} {kind} {req}: {msgs[status]}'
    log_event(kind.upper(), message, status==0)
    return {'success':status==0, 'reason':msgs[status], 'decision': 'GRANTED' if status==0 else 'DENIED', 'state':response_state()}


class Handler(BaseHTTPRequestHandler):
    def log_message(self, format, *args):
        print('[HTTP]', format % args)

    def send_json(self, data, status=200, filename=None):
        b=json.dumps(data,ensure_ascii=False).encode('utf-8')
        self.send_response(status)
        self.send_header('Content-Type','application/json; charset=utf-8')
        self.send_header('Cache-Control','no-store')
        self.send_header('X-Content-Type-Options','nosniff')
        if filename:self.send_header('Content-Disposition',f'attachment; filename="{filename}"')
        self.send_header('Content-Length',str(len(b)))
        self.end_headers();self.wfile.write(b)

    def do_GET(self):
        path=urlparse(self.path).path
        if path=='/api/state':
            with LOCK:self.send_json(response_state())
            return
        if path=='/api/export':
            with LOCK:
                if DATA['state'] is None:self.send_json({'error':'No workspace to export.'},400)
                else:self.send_json(DATA['state'],200,'banker-workspace.json')
            return
        if path in ('/','/index.html'):
            asset=WEB/'index.html';typ='text/html; charset=utf-8'
        elif path=='/app.js':
            asset=WEB/'app.js';typ='text/javascript; charset=utf-8'
        elif path=='/style.css':
            asset=WEB/'style.css';typ='text/css; charset=utf-8'
        else:self.send_error(404);return
        b=asset.read_bytes()
        self.send_response(200); self.send_header('Content-Type',typ)
        self.send_header('Content-Length',str(len(b)))
        self.send_header('Cache-Control','no-store'); self.send_header('X-Content-Type-Options','nosniff')
        self.end_headers();self.wfile.write(b)

    def do_POST(self):
        path=urlparse(self.path).path
        if path not in ('/api/initialize','/api/request','/api/release','/api/reset'):
            self.send_json({'error':'Unknown action'},404);return
        origin=self.headers.get('Origin')
        if origin and origin not in ('http://127.0.0.1:8765','http://localhost:8765'):
            self.send_json({'error':'Only local application requests are allowed.'},403);return
        try:
            length=int(self.headers.get('Content-Length','0'))
            if length<0 or length>64_000:raise ValueError('Request too large.')
            payload=json.loads(self.rfile.read(length) if length else b'{}')
            if not isinstance(payload,dict):raise ValueError('Expected a JSON object.')
            with LOCK:
                if path=='/api/initialize':
                    s=make_state(payload)
                    DATA['state']=s;DATA['history']=[]
                    log_event('WORKSPACE', 'Entered custom dataset: '+s['title'],True)
                    result={'success':True,'reason':'Workspace initialized from your custom inputs.', 'state':response_state()}
                elif path=='/api/reset':
                    if payload.get('confirm')!='RESET':raise ValueError('Reset requires confirmation.')
                    DATA['state']=None;DATA['history']=[];save_data()
                    result={'success':True,'reason':'Workspace cleared.', 'state':response_state()}
                else:
                    result=modify('request' if path=='/api/request' else 'release',payload)
            self.send_json(result)
        except (ValueError,TypeError,KeyError,OverflowError) as error:
            self.send_json({'error':str(error)},400)

if __name__=='__main__':
    if not (ROOT/'libbanker.so').exists():raise SystemExit('Run make first to compile C core.')
    load_saved()
    address=('127.0.0.1',8765)
    print('\nBANKER OS | Interactive Linux Resource Manager')
    print('C Algorithm: loaded from libbanker.so')
    print('Open http://localhost:8765 in your browser')
    print('Press Ctrl+C to shut down. Your entered workspace is saved.\n')
    try:ThreadingHTTPServer(address,Handler).serve_forever()
    except KeyboardInterrupt:print('\nServer stopped. Your workspace remains saved.')
