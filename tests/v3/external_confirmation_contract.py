#!/usr/bin/env python3
"""Contract tests for the frozen V3-10 external confirmation (no SMAC/Optuna needed).

Freezing: the validator must reject every edit to the pinned protocol, a cohort
that reuses any declared or derived prior seed, and changed archived
development evidence. Statistics: independent exact sign tail, Holm, the
nomination rule and the frozen H1/H2 decisions. Aggregation: no trial means
INCOMPLETE_NO_CLAIM, never a verdict; an undeclared trial fails closed.
"""
import copy
import json
import math
from pathlib import Path
import sys
import tempfile
import unittest

HERE=Path(__file__).resolve().parent
sys.path.insert(0,str(HERE.parents[1]/"benchmarks"/"v3"))
import run_external_confirmation as runner  # noqa: E402
import aggregate_external_confirmation as agg  # noqa: E402


def frozen():
    return json.loads(runner.PLAN.read_text())


class Freezing(unittest.TestCase):
    def test_frozen_manifest_validates(self):
        runner.validate(frozen())
        plan,fixtures=runner.manifest()
        self.assertEqual(set(fixtures)>={"sort_pathology","topk_fraction"},True)
        self.assertEqual(plan["status"],"FROZEN_NOT_OPENED")

    def reject(self,edit):
        p=frozen();edit(p)
        with self.assertRaises(ValueError):
            runner.validate(p)

    def test_protocol_edits_rejected(self):
        edits=[
            lambda p:p.__setitem__("seeds",list(range(33001,33021))),           # prior SMAC cohort
            lambda p:p.__setitem__("seeds",list(range(36001,36021))),           # derived-seed collision with SMAC dev
            lambda p:p.__setitem__("diagnostic_seeds",[50001]),                 # collides with cohort query seeds
            lambda p:p.__setitem__("seeds",list(range(51001,51020))),           # 19 clusters
            lambda p:p["hypotheses"][0].__setitem__("alternative","two-sided"),
            lambda p:p["hypotheses"].reverse(),
            lambda p:p.__setitem__("alpha",0.1),
            lambda p:p.__setitem__("family_alpha",0.1),
            lambda p:p.__setitem__("multiplicity","none"),
            lambda p:p.__setitem__("policies",["acquisition-v3-coverage","smac3_hpo"]),
            lambda p:p.__setitem__("pair_budget",24),
            lambda p:p.__setitem__("status","OPENED"),
            lambda p:p.__setitem__("native_source_commit","0"*40),
            lambda p:p["pinned_native_source_blobs"].__setitem__("apps/phaseshift_v3_pairs.cpp","0"*40),
            lambda p:p["development_evidence"]["smac"].__setitem__("sha256","0"*64),
            lambda p:p.__setitem__("smac_version","2.4.0"),
            lambda p:p.__setitem__("sota_claim",True),
            lambda p:p.__setitem__("sign_independence_certified",True),
        ]
        for edit in edits:
            with self.subTest(edit=edit):
                self.reject(edit)

    def test_cohort_is_fresh_in_declared_and_derived_seed_space(self):
        declared=set().union(*runner.repository_seed_lists(runner.PLAN).values())
        self.assertTrue({7101,33001,32020,2101}<=declared)
        cohort=set()
        for seed in frozen()["seeds"]+frozen()["diagnostic_seeds"]:
            cohort|=runner.derived_seeds(seed)
        self.assertFalse(cohort&declared)
        self.assertEqual(runner.derived_seeds(51001)>={51001,151001,162001,1_000_510_010,1_000_510_011},True)
        self.assertEqual(len(runner.derived_seeds(51001)),15)


class Statistics(unittest.TestCase):
    def test_sign_tail(self):
        self.assertEqual(agg.sign_tail(0,0),1.0)
        self.assertEqual(agg.sign_tail(20,20),2.0**-20)
        self.assertAlmostEqual(agg.sign_tail(17,14),sum(math.comb(17,k) for k in range(14,18))/2**17,places=15)
        with self.assertRaises(ValueError):
            agg.sign_tail(3,4)

    def test_holm(self):
        self.assertEqual(agg.holm([0.03,0.04]),[0.06,0.06])
        self.assertEqual(agg.holm([0.001,0.2]),[0.002,0.2])
        self.assertEqual(agg.holm([0.6,0.6]),[1.0,1.0])

    def clusters(self,smac,tpe,v18=2):
        return [{"seed":51001+i,"R":{"acquisition-v3-coverage":v18,"smac3_hpo":smac(i),"optuna_tpe":tpe(i)}}
                for i in range(20)]

    def test_decisions(self):
        out=agg.decide(self.clusters(lambda i:0,lambda i:2),0.05)
        self.assertEqual([(h["id"],h["decision"]) for h in out],[("H1","SUPPORTED"),("H2","NOT_SUPPORTED")])
        self.assertEqual(out[0]["raw_p"],2.0**-20)
        self.assertEqual((out[1]["ties"],out[1]["raw_p"],out[1]["holm_p"]),(20,1.0,1.0))
        # 14 better, 3 worse, 3 ties against SMAC; TPE never worse but only 2 better.
        smac=lambda i:0 if i<14 else (4 if i<17 else 2)
        tpe=lambda i:1 if i<2 else 2
        out=agg.decide(self.clusters(smac,tpe),0.05)
        h1,h2=out
        self.assertEqual((h1["positive_clusters"],h1["negative_clusters"],h1["ties"]),(14,3,3))
        self.assertAlmostEqual(h1["raw_p"],agg.sign_tail(17,14))
        self.assertEqual(h1["decision"],"SUPPORTED")
        self.assertEqual((h2["positive_clusters"],h2["raw_p"],h2["decision"]),(2,0.25,"NOT_SUPPORTED"))
        # A competitor that is better must never yield support.
        out=agg.decide(self.clusters(lambda i:4,lambda i:4),0.05)
        self.assertTrue(all(h["decision"]=="NOT_SUPPORTED" and h["negative_clusters"]==20 for h in out))

    def test_nomination_rule(self):
        plan={"min_practical_ratio":1.2,"max_distinct_candidates":2}
        rows=[{"left":"1","right":"2","effect":0.1},       # below practical threshold
              {"left":"2","right":"3","effect":-0.5},
              {"left":"2","right":"3","effect":0.9},       # same edge, larger magnitude wins
              {"left":"3","right":"4","effect":None},      # censored never nominated
              {"left":"4","right":"5","effect":0.4},
              {"left":"5","right":"6","effect":0.3}]       # capped at two distinct edges
        got=agg.nominate(rows,plan)
        self.assertEqual([(g["left"],g["direction"],g["source_pair"]) for g in got],[("2",1,2),("4",1,4)])


class Aggregation(unittest.TestCase):
    def test_no_trials_is_incomplete_not_a_verdict(self):
        with tempfile.TemporaryDirectory() as empty:
            result=agg.aggregate(Path(empty),False)
        self.assertEqual(result["status"],"INCOMPLETE_NO_CLAIM")
        self.assertEqual((result["expected_trials"],result["observed_trials"],len(result["missing_trials"])),(40,0,40))
        self.assertNotIn("hypotheses",result)
        self.assertFalse(result["sota_claim"])

    def test_undeclared_trial_fails_closed(self):
        with tempfile.TemporaryDirectory() as root:
            trial=Path(root)/"x";trial.mkdir()
            (trial/"trial.json").write_text(json.dumps({"scenario":"sort_ascending","seed":33001}))
            with self.assertRaises(ValueError):
                agg.aggregate(Path(root),False)


if __name__=="__main__":
    unittest.main(verbosity=1)
