#!/usr/bin/env python3
"""V3-01d truth-blind selection and fixed-family protocol falsifiers."""
import math
from pathlib import Path
import sqlite3
import sys
import tempfile
import unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[2]/"benchmarks"/"v3"))
from confirmation_protocol import (nominate,family_manifest,sign_tail,
                                    adjusted_pvalues,MAX_MEMBERS,SEED_PAIRS,ALPHA)
from run_confirmation_development import SOURCE_COMMIT,SOURCE_RUN_ID
from aggregate_confirmations import structural_truth_labels
from multidim_truth import load,truth

def encode(d):
    return "".join(str(len(k))+":"+k+str(len(str(v)))+":"+str(v)
                   for k,v in d.items())

def dummy(path,items):
    with sqlite3.connect(path) as db:
        db.executescript("""CREATE TABLE v3_pairs(pair_number INTEGER PRIMARY KEY,
                status TEXT,log_contrast REAL);
                CREATE TABLE v3_conditional_decisions(pair_number INTEGER PRIMARY KEY,
                axis TEXT,left_coords TEXT,right_coords TEXT);""")
        for i,(status,effect,ctx,left,right) in enumerate(items):
            db.execute("INSERT INTO v3_pairs VALUES(?,?,?)",(i,status,effect))
            a={"x":left,"threshold":ctx,"negative":"false"}
            b={"x":right,"threshold":ctx,"negative":"false"}
            db.execute("INSERT INTO v3_conditional_decisions VALUES(?,?,?,?)",
                       (i,"x",encode(a),encode(b)))

class ConfirmationContract(unittest.TestCase):
    def test_sealed_constants(self):
        self.assertEqual(SEED_PAIRS,20)
        self.assertEqual(MAX_MEMBERS,2)
        self.assertEqual(ALPHA,.05)
        self.assertEqual(len(SOURCE_COMMIT),40)
        self.assertEqual(SOURCE_RUN_ID,45026057899)
    def test_selection_is_truth_blind_and_repeats_do_not_fill_family(self):
        with tempfile.TemporaryDirectory() as folder:
            p=Path(folder)/"run.sqlite"
            dummy(p,[("ok",.6,5,1,2),("ok",.9,5,1,2),
                     ("ok",-.7,3,5,6),("censored",None,3,2,3),
                     ("ok",.01,3,3,4)])
            self.assertEqual(nominate(p,True),[
                {"discovery_pair":1,"direction":1},
                {"discovery_pair":2,"direction":-1}])
            self.assertEqual(nominate(p,False),[])
            manifest=family_manifest("multi_context_steps","acquisition-v3",6101,
                                     nominate(p,True))
            self.assertEqual(manifest["seed_pairs"],20)
            self.assertEqual(len(manifest["members"]),2)
            self.assertEqual(manifest["sign_model"],"independent_seed_signs_declared")
    def test_null_and_flat_does_not_manufacture_candidates(self):
        with tempfile.TemporaryDirectory() as folder:
            p=Path(folder)/"run.sqlite"
            dummy(p,[("censored",None,5,1,2),("ok",.03,5,3,4),
                     ("ok",-.02,5,4,5)])
            self.assertEqual(nominate(p,True),[])
            with self.assertRaises(ValueError):
                family_manifest("smooth_no_jump","conditional-tree",6101,[])
    def test_reject_censor_promotions_and_illegal_edges(self):
        for entry in [("censored",.3,5,1,2),("ok",None,5,1,2),
                      ("ok",math.nan,5,1,2),("ok",.5,5,2,1)]:
            with self.subTest(entry=entry):
                with tempfile.TemporaryDirectory() as folder:
                    p=Path(folder)/"run.sqlite"
                    dummy(p,[entry])
                    with self.assertRaises(ValueError):
                        nominate(p,True)
    def test_exact_tail_and_holm_are_monotone(self):
        self.assertEqual(sign_tail(20,20),1/2**20)
        self.assertAlmostEqual(sign_tail(20,10),sum(math.comb(20,k)
                               for k in range(10,21))/2**20)
        self.assertEqual(sign_tail(20,0),1.0)
        with self.assertRaises(ValueError):
            sign_tail(19,19)
        out=adjusted_pvalues([.01,.02,.9])
        self.assertEqual(out,[.03,.04,.9])
    def test_posthoc_truth_is_independent_of_sqlite_axis_serialization_order(self):
        m,_=load("development")
        fixture=next(f for f in m["fixtures"] if f["id"]=="multi_context_steps")
        edge=truth(fixture)[0]
        context=dict(edge["context"])
        fid="multi_context_steps";policy="conditional-tree";seed=6101
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp)
            folder=root/(fid+"_"+policy+"_"+str(seed))/"run"
            folder.mkdir(parents=True)
            dbfile=folder/"run.sqlite"
            dummy(dbfile,[("ok",.8,int(context["threshold"]),edge["left"],edge["right"])])
            # sqlite serialization orders axes differently from the oracle
            # declaration; this must not change the planted truth label.
            left={"negative":context["negative"],
                  "threshold":context["threshold"],"x":edge["left"]}
            right={**left,"x":edge["right"]}
            with sqlite3.connect(dbfile) as db:
                db.execute("UPDATE v3_conditional_decisions SET left_coords=?,right_coords=? WHERE pair_number=0",
                           (encode(left),encode(right)))
            out=structural_truth_labels(root,fixture,
                {"fixture":fid,"policy":policy,"seed":seed},
                [{"discovery_pair":0,"direction":1}])
            self.assertEqual(len(out),1)
            self.assertTrue(out[0]["positive_jump_with_matching_direction"])

    def test_no_multiplicity_shortcut(self):
        self.assertGreater(adjusted_pvalues([.02,.02])[0],.02)
        self.assertEqual(len(adjusted_pvalues([1.0,1.0])),2)
if __name__=="__main__":
    unittest.main()
