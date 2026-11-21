#!/usr/bin/env python3
import argparse,json,sqlite3,pathlib
p=argparse.ArgumentParser();p.add_argument("--run",required=True);a=p.parse_args()
with sqlite3.connect(pathlib.Path(a.run)/"run.sqlite") as db:
 rows=db.execute("SELECT d.pool_index,c.params,c.seed FROM scheduler_decisions d JOIN cases c ON c.case_id=d.case_id ORDER BY d.seq").fetchall()
# Exact deterministic primary-stratum warmup and complete valid-union sampling.
assert len(rows)==12,len(rows)
assert len({int(row[0]) for row in rows})==12
assert len({json.loads(r[1])["x"] for r in rows[:4]})==4,rows[:4]
observed={(json.loads(params)["x"],json.loads(params)["negative"],
           json.loads(params)["threshold"],seed) for _,params,seed in rows}
expected={(x,neg,t,seed) for seed in (144,145)
 for neg,xs,ts in ((False,(0,1),(2,3)),(True,(2,3),(3,)))
 for x in xs for t in ts}
assert observed==expected,(observed,expected)
print("v2 stratified: all four primary strata reached in first four cases; complete unique union")
