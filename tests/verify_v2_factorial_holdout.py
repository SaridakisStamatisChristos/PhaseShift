#!/usr/bin/env python3
"""Independent raw SQLite evidence integrity for experimental factorial holdout."""
import argparse
import json
import pathlib
import sqlite3

ap=argparse.ArgumentParser()
ap.add_argument("--run",required=True,type=pathlib.Path)
a=ap.parse_args()
root=a.run/"factorial_holdout"
plan=json.loads((root/"frozen_plan.json").read_text())
result=json.loads((root/"result.json").read_text())
assert plan["schema_version"]==1
assert plan["fixed_cohorts"]==20
assert result["overall_confirmed"] is False
assert result["fixed_cohorts"]==20
assert result["state"]=="FROZEN_HOLDOUT_SUPPORT_REQUIRES_CAMPAIGN_SEAL"
assert result["supports_predeclared_effect"] is True
assert result["unadjusted_one_sided_p"]<=.01
with sqlite3.connect(root/"holdout.sqlite") as db:
    members=db.execute(
        "SELECT c.case_id,c.params,c.seed FROM cases c ORDER BY c.case_id"
    ).fetchall()
    samples=db.execute(
        "SELECT COUNT(*),SUM(holdout),SUM(status='ok') FROM samples"
    ).fetchone()
    event_count=db.execute(
        "SELECT COUNT(*) FROM events WHERE event='factorial_holdout_frozen_before_first_sample'"
    ).fetchone()[0]
assert len(members)==80
assert samples==(80,80,80),samples
assert event_count==1
assert len({case_id for case_id,_,_ in members})==80
assert {seed for _,_,seed in members}==set(
    range(plan["seed_start"],plan["seed_start"]+20))
with sqlite3.connect(a.run/"run.sqlite") as original:
    discovery={row[0] for row in original.execute(
        "SELECT DISTINCT seed FROM cases c JOIN samples s USING(case_id) "
        "WHERE s.holdout=0")}
assert discovery.isdisjoint({seed for _,_,seed in members})
print("PASS frozen factorial plan, 80 real-worker holdouts, disjoint seeds, no false global seal")
