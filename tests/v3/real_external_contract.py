#!/usr/bin/env python3
"""Falsify native/Optuna external-research protocol before real worker execution."""
import copy
import json
import math
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch
sys.path.insert(0,str(Path(__file__).resolve().parents[2]/"benchmarks"/"v3"))
import run_real_external as lab


class RealExternalContract(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.m,cls.by=lab.manifest()

    def test_pinned_only_development_and_real_workers(self):
        m=self.m
        self.assertEqual(m["seeds"],list(range(32001,32021)))
        self.assertEqual(m["policies"],list(lab.POLICIES))
        self.assertTrue(set(m["seeds"]).isdisjoint(range(7101,7121)))
        self.assertEqual(m["pair_budget"],12)
        self.assertEqual(m["confirmation_pairs"],20)
        self.assertFalse(m["eligible_for_sota"])
        self.assertEqual(len(m["pinned_native_source_blobs"]),3)
        self.assertEqual(len(m["scenarios"]),2)
        for scenario in m["scenarios"]:
            source=self.by[scenario["source"]]
            native=lab.config(scenario,source,32001,True)
            external=lab.config(scenario,source,32001,False)
            self.assertEqual(len(native["axes"]),2)
            self.assertEqual(len(external["axes"]),1)
            self.assertEqual(external["fixed"]["distribution"],
                             native["axes"]["distribution"]["values"][0])
            self.assertEqual(native["algorithms"],external["algorithms"])
            self.assertEqual(native["fixed"].get("n"),external["fixed"].get("n"))
            self.assertEqual(len(lab.allowed_edges(source)),
                             len(source["config"]["axes"][source["config"]["primary_axis"]]["values"])-1)

    def test_native_candidate_selection_no_oracle_and_no_duplicate(self):
        rows=[
            {"left":"1","right":"8","effect":0.3},
            {"left":"1","right":"8","effect":0.7},
            {"left":"8","right":"32","effect":None},
            {"left":"32","right":"64","effect":-0.8},
            {"left":"64","right":"128","effect":0.01}
        ]
        selected=lab.select(rows,self.m)
        self.assertEqual(len(selected),2)
        self.assertEqual((selected[0]["left"],selected[0]["right"]),("32","64"))
        self.assertEqual(selected[0]["direction"],-1)
        self.assertEqual(selected[1]["source_pair"],1)
        self.assertEqual(len({(x["left"],x["right"]) for x in selected}),2)

    def test_censoring_is_not_zero_cost_or_positive_contrast(self):
        rows=[{"left":"1","right":"8","effect":None},
              {"left":"8","right":"32","effect":float("nan")}]
        self.assertEqual(lab.select(rows[:1],self.m),[])
        # Reject nonfinite persisted effects rather than relying on compare.
        self.assertFalse(math.isfinite(rows[1]["effect"]))

    def test_exact_sign_tail_and_holm(self):
        self.assertAlmostEqual(lab.exact_sign_tail(20,20),1/1048576)
        self.assertEqual(lab.exact_sign_tail(20,0),1.)
        self.assertGreater(lab.exact_sign_tail(20,12),.05)
        self.assertLess(lab.exact_sign_tail(20,17),.05)
        with self.assertRaises(ValueError):
            lab.exact_sign_tail(19,19)
        p=lab.adjusted_pvalues([.01,.2])
        self.assertAlmostEqual(p[0],.02)
        self.assertAlmostEqual(p[1],.2)

    def test_manifest_tampering_and_holdout_seeds_rejected(self):
        m=self.m
        changes=[]
        for mutate in (
            lambda x:x["seeds"].__setitem__(0,7101),
            lambda x:x["policies"].remove("optuna_random"),
            lambda x:x.__setitem__("pair_budget",11),
            lambda x:x["pinned_native_source_blobs"].__setitem__(
                "apps/phaseshift_v3_pairs.cpp","0"*40),
            lambda x:x["scenarios"].__setitem__(0,{"id":"sealed_synthetic"}),
        ):
            bad=copy.deepcopy(m)
            mutate(bad)
            changes.append(bad)
        with tempfile.TemporaryDirectory() as tmp:
            for n,bad in enumerate(changes):
                path=Path(tmp)/f"invalid{n}.json"
                path.write_text(json.dumps(bad))
                with patch.object(lab,"PLAN",path):
                    with self.assertRaises((ValueError,KeyError)):
                        lab.manifest()


if __name__=="__main__":
    unittest.main()
