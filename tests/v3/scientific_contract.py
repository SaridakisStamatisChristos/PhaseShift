#!/usr/bin/env python3
"""Independent unit falsifiers for the successor benchmark."""
import pathlib
import sys
import unittest
sys.path.insert(0,str(pathlib.Path(__file__).resolve().parents[2]/"benchmarks"/"v3"))
from run_scientific import check_splits, manifest, order, truth_edges, matches, curve, delta, config

class ScientificContract(unittest.TestCase):
    def test_disjoint_and_complete(self):
        check_splits()
        m,_=manifest("development")
        self.assertEqual(len(order(m,False)),20*sum(len(f["policies"]) for f in m["scenarios"]))
        self.assertEqual(len(order(m,False)),len({tuple(r) for r in order(m,False)}))
        self.assertEqual(order(m,True),order(m,True))
        self.assertEqual(len(order(m,False,"early_cliff")),200)
    def test_oracle_late_sparse_null(self):
        m,_=manifest("development")
        f={x["id"]:x for x in m["scenarios"]}
        self.assertEqual(truth_edges(f["late_cliff"]),[(94,95)])
        self.assertEqual(truth_edges(f["sparse_cliff"]),[(45,48)])
        self.assertEqual(truth_edges(f["dense_null"]),[])
        self.assertIsNone(truth_edges(f["graph"]))
        self.assertTrue(all(v==1 for v in config(f["late_cliff"],"weighted_v2",4101)["search"]["domain_priors"]["x"].values()))
    def test_no_double_count_and_matching_augment(self):
        h=[["H1","SYNTHETIC_CALIBRATION",0,10],["H2","SYNTHETIC_CALIBRATION",4,5],
           ["H3","SYNTHETIC_CALIBRATION",4,5],["H4","STATUS_FRONTIER",7,8]]
        self.assertEqual(matches([(4,5),(7,8)],h)["true"],2)
        self.assertEqual(matches([(4,5),(7,8)],h)["false"],2)
        self.assertEqual(matches([],h)["false"],4)
    def test_resource_and_censor(self):
        rows=[{"true_within_budget":1,"cost":20},{"true_within_budget":0,"cost":1},
              {"true_within_budget":1,"cost":None}]
        self.assertAlmostEqual(curve(rows,"cost",100)["success"][-1],1/3)
        self.assertGreater(curve([rows[0]],"cost",100)["auc"],
                           curve([{"true_within_budget":1,"cost":90}],"cost",100)["auc"])
        with self.assertRaises(ValueError):
            delta({"wall":4,"child":1,"parent":1},{"wall":3,"child":1,"parent":1})
if __name__=="__main__":
    unittest.main()
