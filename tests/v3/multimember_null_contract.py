#!/usr/bin/env python3
"""V3-01f adversarial preflight: forced two-member null and temporal replay."""
import copy
import json
import math
from pathlib import Path
import sqlite3
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0,str(Path(__file__).resolve().parents[2]/"benchmarks"/"v3"))
import run_multimember_null as two
from run_forced_null_calibration import expected_work, upper_bound_if_independent
from temporal_null_audit import temporal_metrics, independent_contrasts
from aggregate_multimember_null import assemble


class NullDependenceFalsifiers(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.m,_=two.manifest()

    def test_precommitted_multiplicity_and_seed_fencing(self):
        m=self.m
        self.assertEqual(len(m["strata"]),2)
        self.assertEqual(m["families_per_stratum"],64)
        self.assertEqual(m["members_per_family"],2)
        self.assertEqual(m["fresh_pairs_per_member"],20)
        seeds={n for s in m["strata"] for n in s["seeds"]}
        self.assertEqual(len(seeds),128)
        self.assertFalse(seeds & set(range(6101,6121)))
        self.assertFalse(seeds & set(range(7101,7121)))
        self.assertFalse(seeds & set(range(95001,98065)))
        for s in m["strata"]:
            for i,seed in enumerate(s["seeds"]):
                f=two.family_spec(m,s,seed,i)
                self.assertEqual([v["discovery_pair"] for v in f["members"]],[0,1])
                self.assertEqual([v["direction"] for v in f["members"]],
                                 [1,-1] if i%2==0 else [-1,1])
                self.assertEqual(f["sign_model"],"independent_seed_signs_declared")
                cfg=two.config(m,s,seed)
                self.assertEqual(set(cfg["axes"]),{"x","threshold"})
                self.assertEqual(cfg["axes"]["x"]["values"],s["x_values"])
                self.assertEqual(cfg["measurement"]["paired_confirm_blocks"],20)

    def test_temporal_perfect_runs_and_constant_correlation_absence(self):
        output=temporal_metrics([.05]*20)
        self.assertEqual(output["longest_same_sign_run"],20)
        self.assertTrue(output["risk_flag_sign_run_ge_16"])
        self.assertEqual(output["lag1_timing_contrast_correlation"],None)
        self.assertEqual(output["adjacent_comparable"],19)
        self.assertEqual(output["conditional_random_order_same_probability"],1.)

    def test_temporal_alternation_and_reversal(self):
        seq=[.15 if i%2==0 else -.15 for i in range(20)]
        output=temporal_metrics(seq)
        self.assertEqual(output["longest_same_sign_run"],1)
        self.assertEqual(output["adjacent_same_signs"],0)
        self.assertAlmostEqual(output["lag1_timing_contrast_correlation"],-1.,places=12)
        self.assertTrue(output["risk_flag_abs_lag1_ge_half"])
        self.assertFalse(output["risk_flag_sign_run_ge_16"])

    def test_missing_and_nonfinite_temporal_pairs(self):
        q=temporal_metrics([None]*20)
        self.assertEqual(q["valid_pairs"],0)
        self.assertEqual(q["adjacent_comparable"],0)
        self.assertIsNone(q["conditional_random_order_same_probability"])
        seq=[.01]*10+[None]+[-.3]*9
        r=temporal_metrics(seq)
        self.assertEqual(r["censored_pairs"],1)
        self.assertEqual(r["longest_same_sign_run"],10)
        self.assertTrue(r["risk_flag_median_half_shift_ge_log_1_2"])
        with self.assertRaises(ValueError):
            temporal_metrics([float("nan")]+[0.]*19)
        with self.assertRaises(ValueError):
            temporal_metrics([.1]*19)

    @staticmethod
    def build_trial(path,scene,mutate=""):
        with sqlite3.connect(path) as db:
            db.executescript("""
                CREATE TABLE v3_pairs(pair_number INTEGER PRIMARY KEY,seed INTEGER,status TEXT,
                    log_contrast REAL,left_case_id TEXT,right_case_id TEXT);
                CREATE TABLE v3_cases(case_id TEXT PRIMARY KEY,params TEXT,seed INTEGER);
                CREATE TABLE v3_samples(pair_number INTEGER,side INTEGER,ordinal INTEGER,
                    case_id TEXT,algorithm TEXT,status TEXT,work INTEGER,ns INTEGER,batch INTEGER);
            """)
            for pair,(left,right) in enumerate(zip(scene["x_values"],scene["x_values"][1:])):
                seed=101001+pair
                ids=[]
                for side,x in enumerate((left,right)):
                    case=f"{pair}-{side}"
                    params={"x":x,"threshold":scene["threshold"],"negative":True}
                    ids.append(case)
                    db.execute("INSERT INTO v3_cases VALUES(?,?,?)",
                               (case,json.dumps(params),seed))
                    work=expected_work(scene["algorithm"],seed)
                    if mutate=="unequal" and pair==0 and side==1:
                        work+=1
                    if mutate=="wrong_edge" and pair==1 and side==1:
                        params["x"]=100
                        db.execute("UPDATE v3_cases SET params=? WHERE case_id=?",
                                   (json.dumps(params),case))
                    db.execute("INSERT INTO v3_samples VALUES(?,?,?,?,?,?,?,?,?)",
                               (pair,side,0,case,scene["algorithm"],"ok",work,1000,1))
                db.execute("INSERT INTO v3_pairs VALUES(?,?,?,?,?,?)",
                           (pair,seed,"ok",0.0,*ids))

    def test_realistic_raw_work_and_distinct_edges_are_falsifiable(self):
        scene=self.m["strata"][1]
        with tempfile.TemporaryDirectory() as folder:
            db=Path(folder)/"synthetic.sqlite"
            self.build_trial(db,scene)
            self.assertEqual(two.independent_work_and_edges(db,scene,2,True),[0.,0.])
        for kind in ("unequal","wrong_edge"):
            with self.subTest(kind=kind):
                with tempfile.TemporaryDirectory() as folder:
                    db=Path(folder)/"broken.sqlite"
                    self.build_trial(db,scene,kind)
                    with self.assertRaises(ValueError):
                        two.independent_work_and_edges(db,scene,2,True)

    def test_conditional_binomial_bound_not_certified(self):
        bound=upper_bound_if_independent(0,64)
        self.assertAlmostEqual(bound,1-.05**(1/64),places=12)
        self.assertFalse(self.m["population_fwer_certified"])
        self.assertFalse(self.m["wall_time_null_certified"])
        self.assertFalse(self.m["independent_sign_assumptions_certified"])

    def test_malformed_inventory_and_missing_artifacts_veto(self):
        with self.assertRaises(ValueError):
            assemble({},{"protocol":"phaseshift.v3-01f.temporal-replay.1",
                         "source_run_id":45038398487,
                         "family_count":256,"paired_measurements":5120,
                         "sign_independence_certified":False,
                         "fwer_certified":False})
        bad=copy.deepcopy(self.m)
        bad["strata"][1]["seeds"][0]=bad["strata"][0]["seeds"][0]
        with tempfile.TemporaryDirectory() as folder:
            path=Path(folder)/"manifest.json";path.write_text(json.dumps(bad))
            with patch.object(two,"MANIFEST",path):
                with self.assertRaises(ValueError):
                    two.manifest()


if __name__=="__main__":
    unittest.main()
