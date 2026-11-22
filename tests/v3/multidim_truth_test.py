#!/usr/bin/env python3
"""Adversarial independent oracle and replay-shape checks for V3-01b."""
import copy
import math
from pathlib import Path
import sqlite3
import sys
import tempfile
import unittest

HERE=Path(__file__).resolve().parents[2]/"benchmarks"/"v3"
sys.path.insert(0,str(HERE))
from multidim_truth import config, domain, groups, truth, classify, order, load, split_check
from run_multidim import adjudication, paired_bootstrap

class TruthTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.dev,_=load("development")
        cls.fs={f["id"]:f for f in cls.dev["fixtures"]}
    def test_truth_and_split(self):
        split_check()
        self.assertEqual(len(truth(self.fs["multi_context_steps"])),5)
        self.assertEqual(len(truth(self.fs["rare_hidden_cliff"])),1)
        self.assertEqual(len(truth(self.fs["smooth_no_jump"])),0)
        self.assertEqual(len(truth(self.fs["multidim_null"])),0)
        self.assertEqual(len(truth(self.fs["sparse_multi_step"])),4)
        self.assertEqual(len(truth(self.fs["masked_interactions"])),2)
        self.assertEqual(len(order(self.dev,False)),20*len(self.fs)*2)
        self.assertEqual(order(self.dev,True),order(self.dev,True))
        self.assertEqual(len(order(self.dev,False,"rare_hidden_cliff")),40)
        self.assertEqual(len(set(map(tuple,order(self.dev,False)))),len(order(self.dev,False)))
    def test_rare_context_and_analytic_smooth(self):
        f=self.fs["rare_hidden_cliff"]
        self.assertEqual(len(groups(config(f,0))),3)
        for point in domain(config(f,0)):
            if not point["negative"]:
                self.assertEqual(point["threshold"],12)
        self.assertEqual({p["threshold"] for p in domain(config(f,0)) if p["negative"]}, {4,12})
    def test_one_to_one_false_and_sign(self):
        f=self.fs["multi_context_steps"]
        edge=truth(f)[0]
        p={"context":edge["context"],"left":edge["left"],"right":edge["right"],
           "effect":math.log(2.0),"nominate":True,"pair_number":0}
        entries=[p,{**p,"pair_number":1},{**p,"pair_number":2,"effect":-1.0}]
        x=classify(f,entries)
        self.assertEqual(x["true_edges_hit"],1)
        self.assertEqual(x["repeat_true_nominations"],1)
        self.assertEqual(x["false_observations"],1)
        self.assertEqual(x["first_true_pair"],1)
        self.assertEqual(x["missed_truth_edges"],4)
        self.assertEqual(classify(self.fs["smooth_no_jump"],entries)["true_edges_hit"],0)
    def test_matched_bootstrap_seeds_only(self):
        a=[{"seed":3,"true_hit_under_budget":0},{"seed":5,"true_hit_under_budget":1}]
        b=[{"seed":3,"true_hit_under_budget":1},{"seed":5,"true_hit_under_budget":1}]
        x=paired_bootstrap(a,b,lambda r:r["true_hit_under_budget"],["smoke"],200)
        self.assertEqual(x["n"],2)
        self.assertEqual(x["positive"],1)
        self.assertFalse(x["inferential_claim"])
        with self.assertRaises(ValueError):
            paired_bootstrap(a,b[:1],lambda r:r["true_hit_under_budget"],["mismatch"])
    def test_illegal_edge_or_censored_effect_cannot_promote(self):
        f=self.fs["multi_context_steps"]
        e=truth(f)[0]
        def enc(coords):
            return "".join(str(len(str(k)))+":"+str(k)+str(len(str(v)))+":"+str(v)
                           for k,v in coords.items())
        def run(left,right,status="ok",effect=1.0):
            with tempfile.TemporaryDirectory() as tmp:
                db=Path(tmp)/"run.sqlite"
                with sqlite3.connect(db) as cx:
                    cx.executescript("""CREATE TABLE v3_pairs(pair_number INTEGER,status TEXT,log_contrast REAL);
                         CREATE TABLE v3_conditional_decisions(pair_number INTEGER,axis TEXT,
                             left_coords TEXT,right_coords TEXT);""")
                    cx.execute("INSERT INTO v3_pairs VALUES(0,?,?)",(status,effect))
                    cx.execute("INSERT INTO v3_conditional_decisions VALUES(0,'x',?,?)",
                               (enc(left),enc(right)))
                return adjudication(f,db,1)
        context=dict(e["context"])
        left={"x":e["left"],"threshold":int(context["threshold"]),
              "negative":context["negative"]=="true"}
        right={**left,"x":e["right"]}
        self.assertEqual(run(left,right)["true_edges_hit"],1)
        with self.assertRaises(ValueError):
            run(left,{**right,"x":right["x"]+2})
        with self.assertRaises(ValueError):
            run(left,right,"censored",1.0)
    def test_smooth_is_not_flat_work_claim(self):
        self.assertEqual(truth(self.fs["smooth_no_jump"]),[])
        self.assertGreater(len(domain(config(self.fs["smooth_no_jump"],0))),10)

if __name__=="__main__":
    unittest.main()
