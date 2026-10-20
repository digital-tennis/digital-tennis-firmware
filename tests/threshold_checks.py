"""Threshold study failure gates and C replay configuration boundaries."""
import importlib.util,subprocess,sys,tempfile,unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'simulator'))
from threshold_study import plan,predict,score,feasible
from physics import presets,make_pose,sample_trajectory
from run import recognize

class ThresholdChecks(unittest.TestCase):
    def test_both_stroke_sides_and_hands_through_actual_c(self):
        for name in ['volley','slice','lob']:
            for side in [-1,1]:
                for left in [False,True]:
                    with self.subTest(motion=name,side=side,left=left):
                        p=presets()[name];p.update(back_z=p['back_z']*side,front_z=p['front_z']*side)
                        pose,start,end,total=make_pose(p,left=left)
                        data=sample_trajectory(pose,start,end,total,name,int(p['kind']),seed=451,left=left)
                        recognize(data,sys.argv[1],'rally')
                        self.assertEqual([int(e['kind']) for e in data['recognition']['events']],[int(p['kind'])])

    def test_context_is_not_a_sensor_label(self):
        feature=dict(active_ms=200,direction_x=-1,direction_z=0,sweep_deg=100,coherence=1,peak_g=5)
        rules=[dict(kind=k,min_x=-1,max_x=-.8,min_z=-.2,max_z=.2,min_abs_z=0,max_abs_z=.2,min_sweep=40,max_sweep=200,min_ms=40,max_ms=900,min_coherence=.8,min_g=2,max_g=28) for k in [3,4]]
        self.assertEqual(predict(feature,rules,'unknown'),0)
        self.assertEqual(predict(feature,rules,'serve'),3)
        self.assertEqual(predict(feature,rules,'rally'),4)

    def test_duplicate_negative_cannot_hide_in_unknown_matrix_cell(self):
        record={'case':{'expected':0,'start_ms':1200,'end_ms':2000,'forcePeakMs':1600,'parameters':{}},'decisions':[{'kind':1,'time_ms':1500},{'kind':1,'time_ms':1700}]}
        result=score([record])
        self.assertEqual(result['matrixRowsExpectedColumnsPredicted'][0][0],1)
        self.assertEqual(result['negativeFalsePositiveCases'],1)
        self.assertEqual(result['duplicateCases'],1)
        self.assertFalse(feasible(result))

    def test_split_and_acceptance_plan(self):
        p=plan();self.assertEqual(len({p[k]['seed'] for k in ['train','selection','holdout']}),3)
        self.assertNotIn(p['holdout']['seed'],[1701,8129,731091])
        self.assertEqual(p['acceptance']['maximumNegativeFalsePositives'],0)

    def test_invalid_detector_configuration_fails_generation(self):
        spec=importlib.util.spec_from_file_location('rules',ROOT/'tools/generate-gesture-rules.py');module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
        header='filter_alpha,trigger_dps,release_dps,cooldown_ms,min_swing_ms,max_swing_ms\n'
        for row in ['nan,180,80,200,20,1000','.25,80,180,200,20,1000','.25,180,80,200.5,20,1000']:
            with self.subTest(row=row),tempfile.TemporaryDirectory() as d:
                path=Path(d)/'detector.csv';path.write_text(header+row+'\n')
                with self.assertRaises(ValueError):module.generate(ROOT/'config/gesture-rules.csv',Path(d)/'invalid.h',path)

    def test_invalid_host_override_is_rejected(self):
        for option in ['--trigger-dps=nan','--trigger-dps=80oops','--release-dps=9000','--release-dps=0','--filter-alpha=0']:
            with self.subTest(option=option):
                result=subprocess.run([sys.argv[1],'--gestures',option,'/dev/null'],capture_output=True)
                self.assertEqual(result.returncode,2)

if __name__=='__main__':unittest.main(argv=[sys.argv[0]])
