#!/usr/bin/env python3
import argparse
import csv
import hashlib
import json
import pathlib
import sqlite3

p=argparse.ArgumentParser()
p.add_argument("--root",type=pathlib.Path,required=True)
a=p.parse_args()
plan=json.loads((a.root/"frozen_plan.json").read_text())
digest=plan.pop("sha256")
assert hashlib.sha256(json.dumps(plan,sort_keys=True).encode()).hexdigest()==digest
summary=json.loads((a.root/"results.json").read_text())
assert summary["plan_sha256"]==digest
rows=summary["rows"]
assert len(rows)==len(plan["policies"])*len(plan["fixtures"])*len(plan["seeds"])
assert {(r["fixture"],r["policy"],r["seed"]) for r in rows} == {
    (f,p,s) for f in plan["fixtures"] for p in plan["policies"] for s in plan["seeds"]
}
with (a.root/"results.csv").open(newline="") as handle:
    assert len(list(csv.DictReader(handle)))==len(rows)
for row in rows:
    assert not row["had_failure"],row
    assert row["observed_total_cpu_ns"] == (
        row["observed_parent_cpu_ns"]+row["observed_waited_child_cpu_ns"])
    assert row["observed_total_wall_ns"]>0
    assert row["cpu_overshoot_ns"]==max(
        0,row["observed_total_cpu_ns"]-plan["cpu_ceiling_ns"])
    assert row["wall_overshoot_ns"]==max(
        0,row["observed_total_wall_ns"]-plan["wall_ceiling_ns"])
    assert 0<=row["completed_cases"]<=plan["cases"]
    assert row["verified_sota_claim"] is False
    if row["policy"].endswith("_v2"):
        assert row["phase_events"] == row["completed_cases"]
        assert row["measured_case_proposal_wall_ns"] >= 0
        assert row["measured_case_worker_wall_ns"] >= 0
        assert row["total_unattributed_wall_ns"] >= 0
        assert 0 <= row["proposal_fraction_of_total_wall"] <= 1
        assert 0 <= row["non_worker_wall_upper_bound_fraction"] <= 1
    else:
        assert row["phase_events"] is None
        assert row["proposal_fraction_of_total_wall"] is None
    folder=a.root/f"{row['fixture']}_{row['policy']}_{row['seed']}"
    assert folder.is_dir()
    with sqlite3.connect(folder/"run.sqlite") as db:
        assert db.execute("SELECT COUNT(*) FROM scheduler_decisions").fetchone()[0] == row["completed_cases"]
        assert db.execute("SELECT COUNT(*) FROM samples WHERE holdout=1").fetchone()[0] == row["holdout_samples"]
print("PASS dual-ceiling frozen plan, complete seed/policy accounting and raw SQLite provenance")
