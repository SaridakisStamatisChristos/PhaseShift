#!/usr/bin/env python3
"""Independent v15 real-worker consumed cost and prequential replay oracle."""
import argparse
import json
import math
import os
from pathlib import Path
import shutil
import sqlite3
import struct
import subprocess

def check(ok, msg):
    if not ok:
        raise AssertionError(msg)

def run(binary, *args, expect=0, env=None):
    p=subprocess.run([binary,*map(str,args)],capture_output=True,text=True,timeout=120,env=env)
    check(p.returncode==expect,f"{args}: expected={expect} actual={p.returncode}\n{p.stderr}\n{p.stdout}")
    return p.stdout

def lower_median(values):
    return sorted(values)[(len(values)-1)//2] if values else 0

def real(bits):
    return struct.unpack(">d",int(bits).to_bytes(8,"big"))[0]

def inspect(folder,count):
    with sqlite3.connect(str(folder/"run.sqlite")) as db:
        check(db.execute("PRAGMA user_version").fetchone()[0]==15,"schema")
        rows=db.execute("""
          SELECT p.pair_number,p.seed,p.status,p.log_contrast,p.pair_wall_ns,
                 p.worker_timed_cpu_ns,d.left_coords,d.right_coords,d.arm,
                 d.prior_evidence_digest,a.diagnostic,a.actual_wall_ns,a.actual_worker_cpu_ns
          FROM v3_pairs p JOIN v3_conditional_decisions d USING(pair_number)
          JOIN v3_acquisition_decisions a USING(pair_number) ORDER BY p.pair_number
        """).fetchall()
        check(len(rows)==count,"atomic cost/model decision count")
        check(len({r[1] for r in rows})==count,"independent seed reused")
        training=[]
        heldout=covered=failed=0
        for i,row in enumerate(rows):
            number,seed,status,effect,wall,cpu,left,right,arm,digest,audit,actual_wall,actual_cpu=row
            check(number==i and wall>0 and actual_wall==wall and actual_cpu==cpu,"cost provenance")
            check((status=="ok")== (effect is not None),"failed status promoted into speed")
            check(len(digest)==64 and arm in ("explore","replicate"),"proposal identity")
            # Independently reconstruct exact worker timed CPU from rows.
            raw=db.execute("SELECT cpu_ns FROM v3_samples WHERE pair_number=?",(i,)).fetchall()
            check(len(raw)==2,"not two worker records")
            raw_cpu=None if any(r[0] is None for r in raw) else sum(r[0] for r in raw)
            check(raw_cpu==cpu,"worker CPU differs from sum of raw observations")
            context=left.split("1:x")[0]
            check(context==right.split("1:x")[0],"mismatched context")
            check(("5:false" in context and left.endswith("1:0") and right.endswith("1:1")) or
                  ("4:true" in context and "1:3" in context and left.endswith("1:2") and right.endswith("1:3")),
                  "illegal conditional edge")
            values=dict(kv.split("=",1) for kv in audit.split(";"))
            walls=[w for _,w,_ in training]
            locals_=[w for c,w,_ in training if c==context]
            pooled=lower_median(walls)
            expected=lower_median(locals_) if len(locals_)>=3 else pooled
            upper=max(walls,default=0)
            check(int(values["wall"])==expected and int(values["pooled_wall"])==pooled and
                  int(values["upper"])==upper,"independent wall estimate mismatch")
            cpus=[c for _,_,c in training if c is not None]
            local_cpus=[c for cctx,_,c in training if cctx==context and c is not None]
            cpu_expected=lower_median(local_cpus) if len(local_cpus)>=3 else lower_median(cpus)
            check(values["cpu"]==(str(cpu_expected) if cpus else "unknown"),"CPU estimate mismatch")
            check(values["cpu_upper"]==(str(max(cpus)) if cpus else "unknown"),"CPU envelope mismatch")
            enabled=len(training)>=6 and heldout>=2 and covered*5>=heldout*4 and failed==0
            check((int(values["train"]),int(values["heldout"]),int(values["covered"]),
                   int(values["failed"]),int(values["enabled"]))==
                   (len(training),heldout,covered,failed,int(enabled)),
                   "prequential model history or fallback mismatch")
            components=[real(values[name+"_bits"]) for name in
                        ("contrast","uncertainty","diversity","interaction","status")]
            check(all(math.isfinite(x) and 0<=x<=1 for x in components),"invalid ranking component")
            score=sum(a*b for a,b in zip(components,(.30,.25,.25,.10,.10)))
            penalty=math.sqrt(max(.25,min(4.,expected/pooled))) if enabled else 1.
            check(math.isclose(real(values["rank_bits"]),score/penalty,rel_tol=1e-12,abs_tol=1e-12),
                  "score is not independently reconstructible")
            if (i+1)%4==0:
                heldout+=1
                covered+=int(status=="ok" and upper>0 and wall<=upper)
            elif status=="ok":
                training.append((context,wall,cpu))
            if status!="ok":
                failed+=1
        return rows

def tamper(binary, root, source, label, sql):
    dest=root/label
    shutil.copytree(source,dest)
    with sqlite3.connect(str(dest/"run.sqlite")) as db:
        db.execute(sql)
    run(binary,"verify","--run",dest,expect=2)

def main():
    p=argparse.ArgumentParser()
    p.add_argument("--binary",required=True)
    p.add_argument("--config",required=True)
    p.add_argument("--out",required=True)
    a=p.parse_args()
    root=Path(a.out)
    shutil.rmtree(root,ignore_errors=True)
    root.mkdir(parents=True)
    config=json.loads(Path(a.config).read_text())
    config["search"]["max_cases"]=24
    cfg=root/"frozen_training.json"
    cfg.write_text(json.dumps(config))
    path=root/"resumed"
    common=("--config",cfg,"--pairs","12")
    run(a.binary,"acquisition-v3",*common,"--out",path,"--pause-after","5")
    before=inspect(path,5)
    run(a.binary,"verify","--run",path)
    run(a.binary,"resume","--run",path)
    after=inspect(path,12)
    check(before==after[:5],"resume mutated committed observations")
    run(a.binary,"verify","--run",path)
    run(a.binary,"resume","--run",path)
    check(inspect(path,12)==after,"idempotent resume")
    for fault,code,count in (("before_pair_commit",88,0),("after_pair_commit",89,1)):
        target=root/fault
        env=dict(os.environ,PHASESHIFT_V3_FAILPOINT=fault)
        run(a.binary,"acquisition-v3",*common,"--out",target,env=env,expect=code)
        inspect(target,count)
        run(a.binary,"resume","--run",target)
        inspect(target,12)
        run(a.binary,"verify","--run",target)
    tamper(a.binary,root,path,"diagnostic","UPDATE v3_acquisition_decisions SET diagnostic='forged' WHERE pair_number=0")
    tamper(a.binary,root,path,"actual_cost","UPDATE v3_acquisition_decisions SET actual_wall_ns=actual_wall_ns+1 WHERE pair_number=11")
    tamper(a.binary,root,path,"pair_cost","UPDATE v3_pairs SET pair_wall_ns=pair_wall_ns+1 WHERE pair_number=11")
    tamper(a.binary,root,path,"cpu","UPDATE v3_pairs SET worker_timed_cpu_ns=COALESCE(worker_timed_cpu_ns,0)+1 WHERE pair_number=11")
    tamper(a.binary,root,path,"arm","UPDATE v3_conditional_decisions SET arm=CASE WHEN arm='explore' THEN 'replicate' ELSE 'explore' END WHERE pair_number=0")
    print("PASS v15 real-worker cost ledger, independent prequential oracle, ranking, crash and tamper replay")
if __name__=="__main__":
    main()
