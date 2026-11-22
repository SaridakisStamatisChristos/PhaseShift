#!/usr/bin/env python3
"""Exercise frozen null experiment, independent audit, retained evidence."""
import argparse
import json
from pathlib import Path
import shutil
import subprocess
import sys

def need(ok,msg):
    if not ok:
        raise AssertionError(msg)

def main():
    p=argparse.ArgumentParser()
    p.add_argument("--binary",required=True)
    p.add_argument("--runner",required=True)
    p.add_argument("--out",required=True)
    a=p.parse_args()
    root=Path(a.out)
    shutil.rmtree(root,ignore_errors=True)
    args=[sys.executable,a.runner,"--binary",a.binary,"--out",a.out,"--diagnostic"]
    def go(*extra,expect=0):
        r=subprocess.run([*args,*extra],capture_output=True,text=True,timeout=260)
        need((r.returncode==0)==(expect==0),r.stdout+"\n"+r.stderr)
    go()
    score=root/"scorecard.json"
    data=json.loads(score.read_text())
    need(not data["eligible_for_claim"] and not data["population_fwer_certified"],"scientific scope")
    need(set(data["groups"])=={"constant","seeded_heteroscedastic","fault_censor","temporal_load"},"missing scenarios")
    for s,r in data["groups"].items():
        need(r["planned"]==1 and r["completed_families"]==1 and r["failed"]==0 and
             r["binomial_population_interval"] is None,"incomplete diagnostic "+s)
    for row in data["trials"]:
        need(row["discovery_pairs"]==12 and row["selected_members"]==2 and row["holdout_pairs"]==40,
             "fixed sample or family inventory changed")
        need(row["nominal_rejection"] in (True,False),"missing nominal result")
    go("--verify-only")
    original=score.read_bytes()
    data["groups"]["constant"]["nominal_rejections"]+=1
    score.write_text(json.dumps(data))
    go("--verify-only",expect=1)
    score.write_bytes(original)
    print("PASS four real-worker null campaigns, frozen selection, replay and scorecard tamper rejection")
if __name__=="__main__":
    main()
