#!/usr/bin/env python3
import argparse
import sqlite3
p = argparse.ArgumentParser()
p.add_argument("--run", required=True)
args = p.parse_args()
with sqlite3.connect(args.run + "/run.sqlite") as db:
    cohorts = db.execute(
        "SELECT seed, COUNT(*) FROM cases WHERE case_id IN "
        "(SELECT case_id FROM samples WHERE holdout=0) GROUP BY seed ORDER BY seed"
    ).fetchall()
    decisions = db.execute("SELECT COUNT(*), COUNT(DISTINCT pool_index) FROM scheduler_decisions").fetchone()
assert cohorts == [(141,4),(142,4),(143,4)], cohorts
assert decisions == (12,12), decisions
print("verified independent replicate cohorts and replay", cohorts, decisions)
