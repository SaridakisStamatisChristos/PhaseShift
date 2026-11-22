#!/usr/bin/env python3
"""V3-01c: fail-closed inventory and independent aggregation falsifiers."""
import copy
import pathlib
import sys
import unittest

sys.path.insert(0,str(pathlib.Path(__file__).resolve().parents[2]/"benchmarks"/"v3"))
from aggregate_full_development import audit_inventory, seed_cluster_comparison
from multidim_truth import load, order, truth

class FullDevelopmentContract(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.m,_=load("development")
        cls.entries={}
        for f in cls.m["fixtures"]:
            fid=f["id"]
            plan={"suite":"development","diagnostic":False,"shard":fid,
                  "eligible_for_claim":False,"sota_claim":False,
                  "run_order":order(cls.m,False,fid)}
            rows=[{"fixture":i,"policy":p,"seed":seed,
                   "measurement_endpoint":"unconfirmed_matched_timing_nomination",
                   "analytic_truth_scope":"discrete_work_unit_jump_not_timing_truth",
                   "true_hit_under_budget":0,
                   "adjudication":{"false_observations":0},
                   "raw_trial":{"status":"ok"}}
                  for i,p,seed in plan["run_order"]]
            cls.entries[fid]=("a"*40,plan,rows)
    def test_exact_240_trial_inventory(self):
        rows,sha=audit_inventory(self.m,self.entries)
        self.assertEqual(len(rows),240)
        self.assertEqual(sha,"a"*40)
        self.assertEqual(len({(r["fixture"],r["policy"],r["seed"]) for r in rows}),240)
    def test_all_fixtures_have_identical_seed_policy_sets(self):
        for fid,(_,plan,rows) in self.entries.items():
            self.assertEqual([tuple(v) for v in plan["run_order"]],
                             [(r["fixture"],r["policy"],r["seed"]) for r in rows])
    def test_missing_seed_and_unsealed_or_diagnostic_fails(self):
        for key,change in [
            ("rows",lambda e:e[2].pop()),
            ("diagnostic",lambda e:e[1].update(diagnostic=True)),
            ("suite",lambda e:e[1].update(suite="evaluation")),
            ("order",lambda e:e[1]["run_order"].reverse()),
            ("claim",lambda e:e[1].update(sota_claim=True)),
            ("endpoint",lambda e:e[2][0].update(measurement_endpoint="confirmed")),
            ("wrong_seed",lambda e:e[2][0].update(seed=9999))]:
            with self.subTest(case=key):
                entries=copy.deepcopy(self.entries)
                change(entries["smooth_no_jump"])
                with self.assertRaises(ValueError):
                    audit_inventory(self.m,entries)
    def test_absent_scenario_and_cross_revision_fails(self):
        entries=copy.deepcopy(self.entries)
        entries.pop("rare_hidden_cliff")
        with self.assertRaises(ValueError):
            audit_inventory(self.m,entries)
        entries=copy.deepcopy(self.entries)
        c,plan,rows=entries["rare_hidden_cliff"]
        entries["rare_hidden_cliff"]=("b"*40,plan,rows)
        with self.assertRaises(ValueError):
            audit_inventory(self.m,entries)
    def test_full_seed_cluster_count_is_20_not_120(self):
        rows,_=audit_inventory(self.m,self.entries)
        out=seed_cluster_comparison(self.m,rows)
        self.assertEqual(out["n_seed_clusters"],20)
        self.assertEqual(out["positive_fixture_count"],4)
        self.assertEqual(out["negative_control_count"],2)
        self.assertFalse(out["inferential_claim"])
        self.assertEqual(out["v15_minus_v14_positive_hit_fraction"]["mean_v15_minus_v14"],0)
        self.assertEqual(out["v15_minus_v14_positive_hit_fraction"]["n"],20)

if __name__=="__main__":
    unittest.main()
