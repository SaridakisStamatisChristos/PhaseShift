#!/usr/bin/env python3
import argparse, json, pathlib, sqlite3, subprocess, tempfile
p=argparse.ArgumentParser()
for key in ("run","binary","config"):p.add_argument("--"+key,required=True)
args=p.parse_args()
with sqlite3.connect(pathlib.Path(args.run)/"run.sqlite") as db:
 rows=db.execute("SELECT c.params,c.seed FROM cases c JOIN scheduler_decisions d ON d.case_id=c.case_id ORDER BY d.pool_index").fetchall()
 decisions=db.execute("SELECT COUNT(*),COUNT(DISTINCT pool_index),COUNT(DISTINCT case_id) FROM scheduler_decisions").fetchone()
assert decisions==(12,12,12),decisions
observed=set()
for params,seed in rows:
 c=json.loads(params)
 x,neg,t=c["x"],c["negative"],c["threshold"]
 assert ((not neg and x in (0,1) and t in (2,3)) or (neg and x in (2,3) and t==3)),c
 assert seed in (144,145),seed
 observed.add((x,neg,t,seed))
expected={(x,neg,t,seed) for seed in (144,145) for neg,xs,ts in ((False,(0,1),(2,3)),(True,(2,3),(3,))) for x in xs for t in ts}
assert observed==expected,(observed,expected)
base=json.loads(pathlib.Path(args.config).read_text())
with tempfile.TemporaryDirectory() as root:
 def invalid(edit):
  cfg=json.loads(json.dumps(base));edit(cfg)
  path=pathlib.Path(root)/"bad.json";path.write_text(json.dumps(cfg))
  res=subprocess.run([args.binary,"validate","--config",str(path)],capture_output=True,text=True,timeout=15)
  assert res.returncode!=0,("accepted invalid config",cfg)
 invalid(lambda c:c["search"]["admissible_subspaces"].append({"negative":[False]}))
 invalid(lambda c:c["search"]["admissible_subspaces"][0].update({"x":[0,0]}))
 invalid(lambda c:c["search"]["admissible_subspaces"][0].update({"ghost":["x"]}))
 invalid(lambda c:c["search"]["admissible_subspaces"][0].update({"x":[999]}))
 invalid(lambda c:c["search"].update({"policy_version":"v1"}))
print("verified exact admissible union, seed cohorts, resume transcript, malformed rejections")
