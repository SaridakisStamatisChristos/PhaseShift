#!/usr/bin/env python3
"""Read-only discovery taxonomy after verified real-worker replay."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import sqlite3
import subprocess

def need(ok,msg):
    if not ok:
        raise AssertionError(msg)

def run(binary,*args,expect=0):
    r=subprocess.run([binary,*map(str,args)],capture_output=True,text=True,timeout=120)
    need(r.returncode==expect,
         f"{args} expected={expect} rc={r.returncode}\n{r.stdout}\n{r.stderr}")
    return r.stdout

def snapshot(folder):
    # Authoritative content includes any WAL commits, unlike a bare file hash.
    with sqlite3.connect((folder/"run.sqlite").resolve().as_uri()+"?mode=ro",uri=True) as db:
        return hashlib.sha256("\n".join(db.iterdump()).encode()).hexdigest()

def report(binary,folder,n,source):
    before=snapshot(folder)
    raw=run(binary,"taxonomy","--run",folder)
    result=json.loads(raw)
    need(result["schema"]=="phaseshift.v3.taxonomy.1" and result["cohort"]=="discovery"
         and not result["confirmed"] and result["localization"]=="descriptive_not_confidence_interval",
         "invalid scientific scope")
    need(result["observations"]==result["committed_pairs"]==n
         and result["planned_pairs"]==6,"stale or fabricated report")
    need(result["source_config_sha256"]==hashlib.sha256(Path(source).read_bytes()).hexdigest(),
         "source digest mismatch")
    allowed_contexts={("false","2"),("false","3"),("true","3")}
    for c in result["candidates"]+result["profiles"]:
        context=c["context"]
        need((context["negative"],context["threshold"]) in allowed_contexts,
             "illegal projected context")
        left,right=int(c["left"]),int(c["right"])
        need((left,right)==((2,3) if context["negative"]=="true" else (0,1)),
             "borrowed/missing cross-context coordinates")
        need(c["kind"] not in ("ABRUPT_CLIFF","MULTI_CLIFF"),"two bins falsely classified as cliff")
        if c["kind"]=="STATUS_FRONTIER":
            need("descriptive_effect" not in c,"status promoted to time")
    need(run(binary,"taxonomy","--run",folder)==raw,"report not reproducible on frozen data")
    need(snapshot(folder)==before,"taxonomy rewrote authoritative data")
    return result

def main():
    p=argparse.ArgumentParser()
    p.add_argument("--binary",required=True)
    p.add_argument("--config",required=True)
    p.add_argument("--out",required=True)
    a=p.parse_args()
    root=Path(a.out)
    shutil.rmtree(root,ignore_errors=True)
    root.mkdir(parents=True)
    for command in ("conditional-tree","acquisition-v3"):
        folder=root/command
        run(a.binary,command,"--config",a.config,"--out",folder,"--pairs","6","--pause-after","2")
        early=report(a.binary,folder,2,a.config)
        need(not early["candidates"] and not early["profiles"],"unsupported single observations promoted")
        run(a.binary,"resume","--run",folder)
        report(a.binary,folder,6,a.config)
        broken=root/(command+"-tampered")
        shutil.copytree(folder,broken)
        with sqlite3.connect(broken/"run.sqlite") as db:
            db.execute("""UPDATE v3_conditional_decisions
              SET arm=CASE WHEN arm='explore' THEN 'replicate' ELSE 'explore' END
              WHERE pair_number=0""")
        run(a.binary,"taxonomy","--run",broken,expect=2)
    run(a.binary,"taxonomy","--run",root/"missing",expect=2)
    run(a.binary,"taxonomy","--run",root/"conditional-tree","--pause-after","1",expect=2)
    print("PASS read-only taxonomy, committed snapshot, legal sparse projections and tamper rejection")
if __name__=="__main__":
    main()
