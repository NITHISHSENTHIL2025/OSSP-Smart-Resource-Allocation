import ctypes
import json
import random
import sys
import unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'web'))
from server import LIB, I, cint, make_state, calculate

def arrays(s):
 return cint(s['available']),cint([v for row in s['allocation'] for v in row]),cint([v for row in s['maximum'] for v in row])

BASE={'title':'Test','n':5,'m':3,'available':[3,3,2], 'allocation':[[0,1,0],[2,0,0],[3,0,2],[2,1,1],[0,0,2]],'maximum':[[7,5,3],[3,2,2],[9,0,2],[2,2,2],[4,3,3]]}
class TestBankerCore(unittest.TestCase):
 def setUp(self): self.s=make_state(json.loads(json.dumps(BASE)));self.a,self.b,self.c=arrays(self.s)
 def req(self,p,vector):
  seq=(I*self.s['n'])()
  return LIB.banker_request(self.s['n'],self.s['m'],self.a,self.b,self.c,p,cint(vector),seq)
 def release(self,p,vector):return LIB.banker_release(self.s['n'],self.s['m'],self.a,self.b,self.c,p,cint(vector))
 def test_initial_safe(self):self.assertTrue(calculate(self.s)['safe'])
 def test_sequence(self):self.assertEqual(calculate(self.s)['sequence'],[1,3,4,0,2])
 def test_remaining_need(self):self.assertEqual(calculate(self.s)['need'][1],[1,2,2])
 def test_totals(self):self.assertEqual(calculate(self.s)['total'],[10,5,7])
 def test_safe_request(self):self.assertEqual(self.req(1,[1,0,2]),0);self.assertEqual(list(self.a),[2,3,0])
 def test_unsafe_request_rollback(self):
  olda=list(self.a);oldb=list(self.b);self.assertEqual(self.req(4,[3,3,0]),3);self.assertEqual(list(self.a),olda);self.assertEqual(list(self.b),oldb)
 def test_request_exceeds_need(self):self.assertEqual(self.req(1,[2,0,0]),1)
 def test_request_exceeds_available(self):self.assertEqual(self.req(2,[4,0,0]),2)
 def test_request_negative(self):self.assertEqual(self.req(1,[-1,0,0]),-1)
 def test_request_bad_pid(self):self.assertEqual(self.req(50,[0,0,1]),-1)
 def test_release_ok(self):self.assertEqual(self.release(1,[2,0,0]),0);self.assertEqual(list(self.a),[5,3,2])
 def test_release_too_much(self):self.assertEqual(self.release(1,[3,0,0]),1)
 def test_invalid_allocation(self):
  broken=json.loads(json.dumps(BASE));broken['allocation'][1][0]=4
  with self.assertRaisesRegex(ValueError,'Allocation'):make_state(broken)
 def test_invalid_system_max(self):
  broken=json.loads(json.dumps(BASE));broken['maximum'][1][0]=11
  with self.assertRaisesRegex(ValueError,'Maximum'):make_state(broken)
 def test_invalid_dimensions(self):
  broken=json.loads(json.dumps(BASE));broken['n']=0
  with self.assertRaises(ValueError):make_state(broken)
 def test_negative_in_matrix(self):
  broken=json.loads(json.dumps(BASE));broken['available'][0]=-1
  with self.assertRaises(ValueError):make_state(broken)
 def test_zero_process_need(self):
  b={'n':1,'m':1,'available':[0],'allocation':[[0]],'maximum':[[0]]}
  self.assertTrue(calculate(make_state(b))['safe'])
 def test_unsafe_state(self):
  b={'n':2,'m':1,'available':[0],'allocation':[[1],[1]],'maximum':[[2],[2]]}
  self.assertFalse(calculate(make_state(b))['safe'])
 def test_release_zero_not_overflow(self):self.assertEqual(self.release(1,[0,0,0]),0)
 def test_allocation_conserves_total(self):
  totals=calculate(self.s)['total']; self.req(1,[1,0,2]);self.assertEqual([self.a[j]+sum(self.b[i*3+j] for i in range(5)) for j in range(3)], totals)
 def test_many_safe_random_states(self):
  for seed in range(50):
   r=random.Random(seed);n=r.randint(1,8);m=r.randint(1,5)
   available=[r.randint(0,20) for j in range(m)]
   allocation=[[r.randint(0,8) for j in range(m)] for i in range(n)]
   total=[available[j]+sum(allocation[i][j] for i in range(n)) for j in range(m)]
   maximum=[[r.randint(allocation[i][j],total[j]) for j in range(m)] for i in range(n)]
   state=make_state({'n':n,'m':m,'available':available,'allocation':allocation,'maximum':maximum})
   result=calculate(state)
   if result['safe']:
    self.assertEqual(sorted(result['sequence']),list(range(n)))
    self.assertEqual(result['steps'][-1],total)

if __name__=='__main__':unittest.main()
