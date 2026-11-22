#!/usr/bin/env python3
"""Independent adversarial V3-01e negative-control manifest/work/statistics tests."""
import copy
import json
import math
from pathlib import Path
import sqlite3
import sys
import tempfile
import unittest
from unittest.mock import patch

HERE=Path(__file__).resolve().parents[2]/"benchmarks"/"v3"
sys.path.insert(0,str(HERE))

import run_forced_null_calibration as run
from aggregate_forced_null import aggregate
from run_null_calibration import splitmix


class ForcedNullContracts(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.m,_=run.manifest()

    def test_predeclared_256_families_and_disjoint_seeds(self):
        m=self.m
        self.assertEqual(len(m["scenario"]),4)
        self.assertEqual(m["holdout_pairs_per_family"],20)
        self.assertEqual(m["families_per_stratum"],64)
        all_seeds=[v for s in m["scenario"] for v in s["seeds"]]
        self.assertEqual(len(all_seeds),len(set(all_seeds)))
        self.assertEqual(len(all_seeds),256)
        self.assertFalse(set(all_seeds)&set(range(6101,6121)))
        self.assertFalse(set(all_seeds)&set(range(7101,7121)))
        for s in m["scenario"]:
            for i,seed in enumerate(s["seeds"]):
                f=run.family_spec(m,s,seed,i)
                self.assertEqual(f["members"],[{"discovery_pair":0,
                       "direction":1 if i%2==0 else -1}])
                c=run.config(m,s,seed)
                self.assertEqual(c["fixed"]["negative"],True)
                self.assertEqual(c["axes"]["x"]["values"],[s["left"],s["right"]])
                self.assertEqual(set(c["axes"]),{"x","threshold"})
                self.assertEqual(c["axes"]["threshold"]["values"],[s["threshold"]])
                self.assertEqual(c["fixed"],{"negative":True})
                self.assertEqual(c["measurement"]["paired_confirm_blocks"],20)

    def test_exact_seed_shared_oracle_is_context_invariant(self):
        for seed in (1,137,95001,97001,(1<<62)+117):
            value=run.expected_work("calibration_seed_shared_null_v3",seed)
            expected=1024 << ((splitmix(seed^run.SEED_SHARED_SALT)>>7)&3)
            self.assertEqual(value,expected)
            self.assertIn(value,(1024,2048,4096,8192))
            self.assertEqual(run.expected_work("calibration",seed),1000)
        with self.assertRaises(ValueError):
            run.expected_work("calibration_null_fault_v3",123)

    def test_exact_95_one_sided_if_independent_only(self):
        x=run.upper_bound_if_independent
        self.assertAlmostEqual(x(0,64),1-.05**(1/64),places=12)
        self.assertGreater(x(1,64),x(0,64))
        self.assertEqual(x(64,64),1)
        with self.assertRaises(ValueError):x(65,64)
        with self.assertRaises(ValueError):x(0,0)

    def make_raw_pair(self,path,scene,seed,work1,work2):
        with sqlite3.connect(path) as db:
            db.executescript("""CREATE TABLE v3_pairs(
                 pair_number INTEGER PRIMARY KEY, seed INTEGER,status TEXT,log_contrast REAL);
                 CREATE TABLE v3_cases(case_id TEXT PRIMARY KEY,params TEXT,seed INTEGER);
                 CREATE TABLE v3_samples(pair_number INTEGER,side INTEGER,ordinal INTEGER,
                     case_id TEXT,status TEXT,work INTEGER,ns INTEGER,batch INTEGER);""")
            for side,work in enumerate((work1,work2)):
                case=str(side)
                coords={"x":scene["left"] if side==0 else scene["right"],
                        "threshold":scene["threshold"],"negative":True}
                db.execute("INSERT INTO v3_cases VALUES(?,?,?)",
                           (case,json.dumps(coords),seed))
                db.execute("INSERT INTO v3_samples VALUES(?,?,?,?,?,?,?,?)",
                           (0,side,0,case,"ok",work,1000,1))
            db.execute("INSERT INTO v3_pairs VALUES(?,?,?,?)",(0,seed,"ok",0.01))

    def test_raw_work_must_be_equal_and_equal_to_independent_oracle(self):
        scene=self.m["scenario"][2]
        seed=97001
        correct=run.expected_work(scene["algorithm"],seed)
        with tempfile.TemporaryDirectory() as tmp:
            db=Path(tmp)/"run.sqlite"
            self.make_raw_pair(db,scene,seed,correct,correct)
            r=run.work_oracle(db,scene,1)
            self.assertEqual(r[0]["work"],correct)
        with tempfile.TemporaryDirectory() as tmp:
            db=Path(tmp)/"run.sqlite"
            self.make_raw_pair(db,scene,seed,correct,correct+1)
            with self.assertRaises(ValueError):
                run.work_oracle(db,scene,1)
        with tempfile.TemporaryDirectory() as tmp:
            db=Path(tmp)/"run.sqlite"
            self.make_raw_pair(db,scene,seed,correct+1,correct+1)
            with self.assertRaises(ValueError):
                run.work_oracle(db,scene,1)

    def test_reject_reused_seeds_and_mutated_strata(self):
        bad=copy.deepcopy(self.m)
        bad["scenario"][1]["seeds"][0]=bad["scenario"][0]["seeds"][0]
        with tempfile.TemporaryDirectory() as tmp:
            file=Path(tmp)/"bad.json"
            file.write_text(json.dumps(bad))
            with patch.object(run,"MANIFEST",file):
                with self.assertRaises(ValueError):
                    run.manifest()
        bad=copy.deepcopy(self.m)
        bad["scenario"][0]["left"]=bad["scenario"][0]["right"]
        with tempfile.TemporaryDirectory() as tmp:
            file=Path(tmp)/"bad.json"
            file.write_text(json.dumps(bad))
            with patch.object(run,"MANIFEST",file):
                with self.assertRaises(ValueError):
                    run.manifest()

    def test_partial_aggregate_not_accepted(self):
        with self.assertRaises(ValueError):
            aggregate({})
        self.assertFalse(self.m["sampling_assumptions_empirically_validated"])
        self.assertFalse(self.m["population_sampling_independence_certified"])

if __name__=="__main__":
    unittest.main()
