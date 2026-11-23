#!/usr/bin/env python3
"""Independent falsifiers for the v3 consumed-resource benchmark contract."""
import copy
import json
import pathlib
import sqlite3
import sys
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "benchmarks" / "v3"))
from protocol import (assert_manifest, exact_binomial_interval,
                      seal, sha256_file, sqlite_snapshot_sha256, verify_results)
from scorecard import score


def make_fixture(root):
    manifest = {
        "schema": 1, "suite": "development",
        "seeds": [7], "policies": ["adaptive_partition", "cliff_bracket_v2"],
        "fixtures": ["single_cliff"], "ground_truth": {"single_cliff": 24},
        "cases": 16, "cpu_budget_ns": 100_000_000,
        "wall_budget_ns": 100_000_000, "sota_claim": False,
    }
    plan = {
        "schema": 1, "manifest": manifest,
        "manifest_sha256": "a"*64, "source_commit": "b"*40,
        "binary_sha256": "c"*64,
        "run_order": [["single_cliff", p, 7] for p in manifest["policies"]],
        "accounting": "waited_cli_subtree_cpu_plus_harness_cpu",
        "sqlite_evidence_digest": "read_only_pager_backup_sha256_v1",
        "eligible_for_claim": False,
    }
    (root / "frozen_plan.json").write_text(
        json.dumps({**plan, "sha256": seal(plan)}))
    rows = []
    for policy, full in (("adaptive_partition", 20_000_000),
                         ("cliff_bracket_v2", 90_000_000)):
        name = f"single_cliff_{policy}_7"
        folder = root / name
        folder.mkdir()
        path = folder / "run.sqlite"
        with sqlite3.connect(path) as db:
            db.executescript("""
                CREATE TABLE scheduler_decisions(id INTEGER);
                CREATE TABLE hypotheses(id TEXT,status TEXT,kind TEXT,left_value REAL,right_value REAL);
                CREATE TABLE confirmation_seals(id INTEGER);
                CREATE TABLE samples(holdout INTEGER,status TEXT);
                INSERT INTO scheduler_decisions VALUES(1);
                INSERT INTO hypotheses VALUES('H1','CONFIRMED','cliff',23,24);
                INSERT INTO confirmation_seals VALUES(1);
                INSERT INTO samples VALUES(1,'ok');
                INSERT INTO samples VALUES(0,'ok');
            """)
        events = [{
            "iteration": 0, "returncode": 0,
            "wall_ns": full//2, "parent_cpu_ns": full//8,
            "child_cpu_ns": full//4, "command": ["phaseshift", "explore"],
        }]
        commands = root / (name + ".commands.jsonl")
        commands.write_text(json.dumps(events[0]) + "\n")
        rows.append({
            "fixture": "single_cliff", "policy": policy, "seed": 7,
            "commands_sha256": sha256_file(commands),
            "sqlite_snapshot_sha256": sqlite_snapshot_sha256(path),
            "observed_total_cpu_ns": full,
            "observed_total_wall_ns": full,
            "observed_parent_cpu_ns": full//4,
            "observed_waited_child_cpu_ns": 3*full//4,
            "cpu_overshoot_ns": 0, "wall_overshoot_ns": 0,
            "within_both_ceilings": True, "discovery_case_ceiling": 16,
            "accounted_cpu_ceiling_ns": 100_000_000,
            "full_wall_ceiling_ns": 100_000_000, "completed_cases": 1,
            "valid_samples": 1, "invalid_samples": 0,
            "holdout_samples": 1, "confirmation_seals": 1,
            "final_h1_status": "CONFIRMED", "h1_kind": "cliff",
            "h1_left": 23, "h1_right": 24,
            "confirmation_attempted": True, "had_failure": False,
            "eligible_confirmed_under_budget": 1,
            "synthetic_known_threshold_correct": 1,
            "synthetic_false_positive_confirmed": 0,
            "verified_sota_claim": False,
        })
    (root / "results.json").write_text(json.dumps({
        "plan_sha256": seal(plan), "sota_claim": False, "rows": rows,
    }))
    return rows


class BenchmarkContractTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = pathlib.Path(self.temp.name)
        self.rows = make_fixture(self.root)

    def save(self, rows):
        p = self.root / "results.json"
        value = json.loads(p.read_text())
        value["rows"] = rows
        p.write_text(json.dumps(value))

    def test_complete_independently_read_rows_and_consumption_curve(self):
        _, rows = verify_results(self.root)
        self.assertEqual(len(rows), 2)
        result = score(self.root)
        entries = {x["policy"]: x for x in result["per_fixture_policy"]}
        fast = entries["adaptive_partition"]
        slow = entries["cliff_bracket_v2"]
        self.assertEqual(fast["true_confirmations"], slow["true_confirmations"])
        self.assertLess(fast["mean_cap_utilization"]["full_cpu"],
                        slow["mean_cap_utilization"]["full_cpu"])
        self.assertGreater(fast["success_vs_full_system_cpu"]["normalized_trapezoid_auc"],
                           slow["success_vs_full_system_cpu"]["normalized_trapezoid_auc"])
        self.assertFalse(result["sota_claim"])

    def test_wal_checkpoint_does_not_change_logical_digest(self):
        path = self.root / "wal_state.sqlite"
        conn = sqlite3.connect(path)
        self.assertEqual(conn.execute("PRAGMA journal_mode=WAL").fetchone()[0], "wal")
        conn.execute("CREATE TABLE events(id INTEGER PRIMARY KEY, value TEXT)")
        conn.execute("INSERT INTO events VALUES(1,'committed')")
        conn.commit()
        # WAL main-file bytes lack the newest committed page until checkpoint.
        before_raw = sha256_file(path)
        before_snapshot = sqlite_snapshot_sha256(path)
        conn.close()
        after_raw = sha256_file(path)
        after_snapshot = sqlite_snapshot_sha256(path)
        self.assertNotEqual(before_raw, after_raw)
        self.assertEqual(before_snapshot, after_snapshot)
        with sqlite3.connect(path) as replay:
            self.assertEqual(replay.execute("SELECT value FROM events").fetchone(),
                             ("committed",))

    def test_missing_trial_rejected(self):
        self.save(self.rows[:1])
        with self.assertRaises(AssertionError):
            verify_results(self.root)

    def test_forged_equal_consumption_rejected(self):
        rows = copy.deepcopy(self.rows)
        rows[1]["observed_total_cpu_ns"] = rows[0]["observed_total_cpu_ns"]
        self.save(rows)
        with self.assertRaises(AssertionError):
            verify_results(self.root)

    def test_wrong_status_promotion_rejected(self):
        rows = copy.deepcopy(self.rows)
        rows[0]["final_h1_status"] = "wrong_answer"
        self.save(rows)
        with self.assertRaises(AssertionError):
            verify_results(self.root)

    def test_missing_log_and_sqlite_seals_rejected(self):
        path = self.root / "single_cliff_adaptive_partition_7.commands.jsonl"
        path.write_text(path.read_text() + "garbage\n")
        with self.assertRaises(AssertionError):
            verify_results(self.root)

    def test_null_is_not_ground_truth_positive(self):
        manifest = json.loads((self.root / "frozen_plan.json").read_text())
        manifest.pop("sha256")
        manifest["manifest"]["fixtures"] = ["constant_null"]
        manifest["manifest"]["ground_truth"] = {"constant_null": None}
        assert_manifest(manifest["manifest"])
        self.assertEqual(exact_binomial_interval(0, 20)[0], 0)
        self.assertGreater(exact_binomial_interval(0, 20)[1], .05)

    def test_invalid_duplicate_and_leaked_seeds(self):
        manifest = json.loads((self.root / "frozen_plan.json").read_text())["manifest"]
        manifest["seeds"] = [7, 7]
        with self.assertRaises(AssertionError):
            assert_manifest(manifest)
        manifest["seeds"] = [7, -1]
        with self.assertRaises(AssertionError):
            assert_manifest(manifest)


if __name__ == "__main__":
    unittest.main()
