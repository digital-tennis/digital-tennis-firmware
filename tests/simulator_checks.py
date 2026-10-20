import csv,importlib.util,math,sys,tempfile,unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'simulator'));BINARY=Path(sys.argv[1])
from physics import G,IDENTITY,inertial,rz,rx,mv,generate,presets
from run import recognize
class PhysicsChecks(unittest.TestCase):
    def test_stationary_gravity(self):
        a,w,_,_=inertial(lambda t:((0,0,0),IDENTITY),1)
        self.assertEqual(a,(0,0,G));self.assertEqual(w,(0,0,0))
    def test_constant_linear_acceleration(self):
        a,w,_,_=inertial(lambda t:((t*t,0,0),IDENTITY),1)
        self.assertAlmostEqual(a[0],2,places=5);self.assertAlmostEqual(a[2],G,places=5)
    def test_rotation_centripetal_and_units(self):
        a,w,_,_=inertial(lambda t:((0,0,0),rz(2*t)),.5,offset=(0,.25,0))
        self.assertAlmostEqual(w[2],math.degrees(2),places=5)
        self.assertAlmostEqual(a[1],-1,places=5);self.assertAlmostEqual(a[2],G,places=5)
    def test_mount_rotation(self):
        a,w,_,_=inertial(lambda t:((0,0,0),IDENTITY),1,mount=rx(math.pi/2))
        self.assertAlmostEqual(a[1],G,places=6);self.assertAlmostEqual(a[2],0,places=6)
    def test_seven_nominal_through_actual_c(self):
        for name,p in presets().items():
            with self.subTest(name=name):
                data=recognize(generate(name,seed=410,noise=0),BINARY,'serve' if name=='serve' else 'rally')
                self.assertTrue(data['recognition']['calibrated']);self.assertEqual([int(e['kind']) for e in data['recognition']['events']],[int(p['kind'])])
    def test_mount_and_left_physical_roundtrip(self):
        data=recognize(generate('slice',seed=411,mount_deg=90,left=True),BINARY,'rally')
        self.assertEqual([int(e['kind']) for e in data['recognition']['events']],[6])
        # This two-axis path depends on the mount. A pure Z rotation is invariant
        # under a Z-only mount rotation and would be an invalid negative control.
        data['axis']=[v for row in IDENTITY for v in row]
        recognize(data,BINARY,'rally');self.assertNotEqual([int(e['kind']) for e in data['recognition']['events']],[6])
    def test_csv_rejects_nonfinite_profile(self):
        spec=importlib.util.spec_from_file_location('rules',ROOT/'tools/generate-gesture-rules.py');module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
        with tempfile.TemporaryDirectory() as d:
            path=Path(d)/'rules.csv';path.write_text((ROOT/'config/gesture-rules.csv').read_text().replace('0.85','nan',1))
            with self.assertRaises(ValueError):module.generate(path,Path(d)/'bad.h')
    def test_identical_overhead_needs_context(self):
        data=recognize(generate('serve',noise=0),BINARY,'unknown');self.assertEqual(data['recognition']['events'],[])
        decisions=data['recognition']['decisions'];self.assertTrue(any(int(d['candidates'])==24 and int(d['reason'])==4 for d in decisions))
    def test_expected_label_never_reaches_classifier(self):
        data=generate('forehand',noise=0);data['expected']=0
        recognize(data,BINARY,'rally');self.assertEqual([int(e['kind']) for e in data['recognition']['events']],[1])
if __name__=='__main__':unittest.main(argv=[sys.argv[0]])
