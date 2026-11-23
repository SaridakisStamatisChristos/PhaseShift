#!/usr/bin/env python3
"""Independent V3 trajectory mechanism tests; no evaluation files consulted."""
import json
import math
from pathlib import Path
import sqlite3
import sys
import tempfile
import unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[2]/"benchmarks"/"v3"))
from analyze_policy_trajectory import trajectory,require,BASELINE_COMMIT
from multidim_truth import load,truth


def coords(values):
    return "".join(f"{len(k)}:{k}{len(str(v).lower())}:{str(v).lower()}"
                   for k,v in values.items())


class TrajectoryFalsifiers(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        m,_=load("development")
        cls.f=next(f for f in m["fixtures"] if f["id"]=="multi_context_steps")
        assert truth(cls.f)

    def build(self,path,all_noise=False,broken=False):
        with sqlite3.connect(path) as db:
            db.executescript("""
                CREATE TABLE v3_pairs(pair_number INTEGER PRIMARY KEY,
                   status TEXT,log_contrast REAL);
                CREATE TABLE v3_conditional_decisions(pair_number INTEGER PRIMARY KEY,
                   arm TEXT,axis TEXT,left_coords TEXT,right_coords TEXT);
            """)
            for i in range(12):
                pair=(0,1) if i%2==0 else (2,3)
                act="explore" if i<2 else "replicate"
                if broken and i==2:act="explore"
                left={"x":pair[0],"threshold":3,"negative":False}
                right={"x":pair[1],"threshold":3,"negative":False}
                effect=0.01 if all_noise else (.4 if i%2 else 0.)
                db.execute("INSERT INTO v3_pairs VALUES(?,?,?)",
                           (i,"ok",effect))
                db.execute("INSERT INTO v3_conditional_decisions VALUES(?,?,?,?,?)",
                           (i,act,"x",coords(left),coords(right)))

    def test_exposure_and_qualification_are_distinct(self):
        with tempfile.TemporaryDirectory() as directory:
            path=Path(directory)/"raw.sqlite"
            self.build(path)
            row=trajectory(path,self.f,"conditional-tree",6101)
            self.assertEqual(row["miss_mechanism"],"hit")
            self.assertEqual(row["first_true_exposure_pair"],2)
            self.assertEqual(row["first_true_nomination_pair"],2)
            self.assertEqual(row["distinct_edges_visited"],2)
            self.assertEqual(row["replication_queries"],10)
            self.assertEqual(row["distinct_true_edges_visited"],1)
            self.assertEqual(row["distinct_true_edges_nominated"],1)
            self.assertEqual(row["false_nomination_events"],0)

    def test_exposed_but_not_nominated_is_a_separate_miss(self):
        with tempfile.TemporaryDirectory() as directory:
            path=Path(directory)/"raw.sqlite"
            self.build(path,all_noise=True)
            row=trajectory(path,self.f,"acquisition-v3",6101)
            self.assertEqual(row["miss_mechanism"],
                             "visited_true_edge_no_qualified_nomination")
            self.assertEqual(row["distinct_true_edges_visited"],1)
            self.assertIsNone(row["first_true_nomination_pair"])

    def test_fake_new_edge_query_is_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            path=Path(directory)/"raw.sqlite"
            self.build(path,broken=True)
            with self.assertRaises(ValueError):
                trajectory(path,self.f,"acquisition-v3",6101)

    def test_source_is_pinned(self):
        self.assertEqual(len(BASELINE_COMMIT),40)


if __name__=="__main__":
    unittest.main()
