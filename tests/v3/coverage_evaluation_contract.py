#!/usr/bin/env python3
"""Pre-opening static falsifiers for the ONE-TIME v18 sealed holdout.

No evaluation worker is executed here. These tests inspect only frozen
protocol identity, source hashes, budgets and legal complete inventories.
"""
import copy
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0,str(Path(__file__).resolve().parents[2]/"benchmarks"/"v3"))
import run_coverage_evaluation as evaluation
from aggregate_coverage_evaluation import aggregate


class FrozenEvaluationContract(unittest.TestCase):
    def test_exact_original_evaluation_cohort(self):
        m, digest=evaluation.manifest()
        self.assertEqual(m["suite"],"evaluation")
        self.assertEqual(m["seeds"],list(range(7101,7121)))
        self.assertEqual(m["policies"],["conditional-tree","acquisition-v3",
                                        "acquisition-v3-coverage"])
        self.assertEqual(m["pairs"],12)
        self.assertEqual(len(m["fixtures"]),6)
        self.assertEqual(len(digest),64)
        self.assertTrue(m["sealed_evaluation_one_time"])
        self.assertTrue(m["no_reruns_for_policy_selection"])
        self.assertFalse(m["eligible_for_claim"])
        self.assertFalse(m["sota_claim"])
        for f in m["fixtures"]:
            order=evaluation.order(m,False,f["id"])
            self.assertEqual(len(order),60)
            self.assertEqual(len({tuple(row) for row in order}),60)
            self.assertEqual({row[2] for row in order},set(m["seeds"]))
            self.assertEqual({row[1] for row in order},set(m["policies"]))
        self.assertEqual(evaluation.PINNED_SCHEMA,
                         {"conditional-tree":14,"acquisition-v3":15,
                          "acquisition-v3-coverage":18})

    def test_no_diagnostic_or_cherry_picked_subsets(self):
        m,_=evaluation.manifest()
        for f in m["fixtures"]:
            with self.assertRaises(ValueError):
                evaluation.order(m,True,f["id"])
        with self.assertRaises(ValueError):
            evaluation.order(m,False,"unexpected")

    def test_evaluation_inputs_and_policy_pins_are_immutable(self):
        m,_=evaluation.manifest()
        variations=[]
        copy_fixture=copy.deepcopy(m)
        copy_fixture["fixtures"][0]["threshold"]=[1,2]
        variations.append(copy_fixture)
        copy_seed=copy.deepcopy(m)
        copy_seed["seeds"][0]=6301
        variations.append(copy_seed)
        copy_code=copy.deepcopy(m)
        copy_code["pinned_code_blobs"]["include/phaseshift/v3/conditional_pair.hpp"]="0"*40
        variations.append(copy_code)
        copy_policy=copy.deepcopy(m)
        copy_policy["policies"]=["acquisition-v3-coverage"]
        variations.append(copy_policy)
        copy_rerun=copy.deepcopy(m)
        copy_rerun["no_reruns_for_policy_selection"]=False
        variations.append(copy_rerun)
        with tempfile.TemporaryDirectory() as directory:
            for i,invalid in enumerate(variations):
                path=Path(directory)/f"modified_{i}.json"
                path.write_text(json.dumps(invalid))
                with patch.object(evaluation,"MANIFEST",path):
                    with self.assertRaises((ValueError,KeyError)):
                        evaluation.manifest()

    def test_original_evaluation_fixture_blob_immutable(self):
        m,_=evaluation.manifest()
        with tempfile.TemporaryDirectory() as directory:
            old=json.loads(evaluation.OLDER.read_text())
            old["fixtures"][0]["x"].append(19)
            path=Path(directory)/"tampered_old.json"
            path.write_text(json.dumps(old))
            with patch.object(evaluation,"OLDER",path):
                with self.assertRaises((ValueError,KeyError)):
                    evaluation.manifest()

    def test_aggregate_refuses_partial_evidence(self):
        with self.assertRaises(ValueError):
            aggregate({})


if __name__=="__main__":
    unittest.main()
