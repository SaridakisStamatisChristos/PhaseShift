#!/usr/bin/env python3
"""Independent real worker family freeze, exact tail/Holm and resume audit."""
import argparse
import hashlib
import json
import math
import os
from pathlib import Path
import shutil
import sqlite3
import subprocess

def need(ok,msg):
    if not ok:
        raise AssertionError(msg)

def run(binary,*args,expect=0,env=None):
    r=subprocess.run([binary,*map(str,args)],capture_output=True,text=True,timeout=120,env=env)
    need(r.returncode==expect,f"{args}: rc={r.returncode} expected={expect}\n{r.stdout}\n{r.stderr}")
    return r.stdout

def snapshot(path):
    with sqlite3.connect(path.resolve().as_uri()+"?mode=ro",uri=True) as db:
        return hashlib.sha256("\n".join(db.iterdump()).encode()).hexdigest()

def manifest(path,doc):
    path.write_text(json.dumps(doc))
    return path

def inspect(folder,count,members=2):
    with sqlite3.connect(folder/"confirmation"/"family.sqlite") as db:
        need(db.execute("PRAGMA user_version").fetchone()[0]==16,"family schema")
        spec,seal=db.execute("SELECT specification,spec_sha256 FROM v3_family").fetchone()
        need(hashlib.sha256(spec.encode()).hexdigest()==seal,"frozen family digest")
        f=json.loads(spec)
        need(len(f["members"])==members and len(f["holdout_seeds"])==20*members,"frozen inventory")
        seeds=f["holdout_seeds"]
        need(len(set(seeds))==len(seeds) and min(seeds)>=2**62 and max(seeds)<2**63,"invalid holdout stream")
        rows=db.execute("SELECT pair_number,seed,left_first,status,log_contrast,worker_timed_cpu_ns,pair_wall_ns FROM v3_pairs ORDER BY pair_number").fetchall()
        need(len(rows)==count,"holdout count")
        valid=[0]*members
        positive=[0]*members
        for i,(idx,seed,order,status,effect,cpu,wall) in enumerate(rows):
            member=f["members"][i%members]
            need(idx==i and seed==seeds[i] and order==int((seed^i)&1==0) and wall>0,"frozen seed/order mismatch")
            raw=db.execute("""SELECT s.side,s.ns,s.batch,s.status,s.cpu_ns,c.seed,c.params
                  FROM v3_samples s JOIN v3_cases c USING(case_id)
                  WHERE s.pair_number=? ORDER BY side,ordinal""",(i,)).fetchall()
            need(len(raw)==2 and [r[0] for r in raw]==[0,1] and all(r[5]==seed for r in raw),"raw matched worker mismatch")
            need([json.loads(r[6])["x"] for r in raw]==[int(member["left"]),int(member["right"])],"changed frozen endpoint")
            expected_cpu=None if any(r[4] is None for r in raw) else sum(r[4] for r in raw)
            need(cpu==expected_cpu,"raw CPU mismatch")
            ok=all(r[3]=="ok" for r in raw)
            need((status=="ok")==ok and (effect is not None)==ok,"censored timing promotion")
            if ok:
                delta=math.log(raw[1][1]/raw[1][2])-math.log(raw[0][1]/raw[0][2])
                need(math.isclose(delta,effect,rel_tol=1e-12,abs_tol=1e-12),"effect not reconstructed")
                valid[i%members]+=1
                positive[i%members]+=int(member["direction"]*delta>f["minimum_log_effect"])
        return f,valid,positive,rows

def audit_report(report,f,valid,positive):
    need(report["cohort"]=="confirmation" and not report["confirmed"] and
         not report["assumptions_empirically_validated"],"unsupported scientific claim")
    n=f["seed_pairs"]
    p=[(sum(math.comb(n,k) for k in range(s,n+1))/2**n
        if v==n and f["sign_model"]=="independent_seed_signs_declared" else 1.)
       for v,s in zip(valid,positive)]
    adjusted=[1.]*len(p)
    running=0.
    for rank,i in enumerate(sorted(range(len(p)),key=lambda i:p[i])):
        running=max(running,p[i]*(len(p)-rank))
        adjusted[i]=min(1.,running)
    need(report["complete"] and len(report["members"])==len(p),"incomplete report")
    for i,row in enumerate(report["members"]):
        expected=("ABSTAIN_CENSORED" if valid[i]!=n else
                  "ABSTAIN_ASSUMPTIONS" if f["sign_model"]=="unverified" else
                  "REJECTED_UNDER_DECLARED_MODEL" if adjusted[i]<=f["alpha"] else "NOT_REJECTED")
        need(row["member_index"]==i and row["valid_pairs"]==valid[i] and
             row["positive_pairs"]==positive[i] and row["state"]==expected and
             math.isclose(row["raw_p"],p[i],abs_tol=1e-14) and
             math.isclose(row["holm_p"],adjusted[i],abs_tol=1e-14),"independent statistical oracle disagreement")

def main():
    p=argparse.ArgumentParser()
    p.add_argument("--binary",required=True)
    p.add_argument("--config",required=True)
    p.add_argument("--out",required=True)
    a=p.parse_args()
    root=Path(a.out)
    shutil.rmtree(root,ignore_errors=True)
    root.mkdir(parents=True)
    cfg=json.loads(Path(a.config).read_text())
    cfg["axes"]={"x":{"values":[0,1]},"negative":{"values":[False,True]}}
    cfg["fixed"]={"threshold":1}
    cfg["search"]["admissible_subspaces"]=[{"negative":[False]},{"negative":[True]}]
    cfg["search"]["max_cases"]=12
    cfgpath=manifest(root/"config.json",cfg)
    folder=root/"discovery"
    run(a.binary,"conditional-tree","--config",cfgpath,"--out",folder,"--pairs","6","--pause-after","2")
    doc={"family_id":"frozen-test","seed_pairs":20,"alpha":.05,
         "sign_model":"independent_seed_signs_declared",
         "members":[{"discovery_pair":0,"direction":1},{"discovery_pair":1,"direction":1}]}
    mp=root/"manifest.json"
    freeze=lambda d,expect=0:run(a.binary,"family-freeze","--run",folder,"--manifest",manifest(mp,d),expect=expect)
    freeze(doc,expect=2) # partial discovery cannot be frozen
    need(not (folder/"confirmation").exists(),"partial freeze produced sidecar")
    run(a.binary,"resume","--run",folder)
    raw_db=folder/"run.sqlite"
    digest=snapshot(raw_db)
    with sqlite3.connect(raw_db) as db:
        discovery=list(db.execute("SELECT seed FROM v3_pairs"))
        need(len(discovery)==6,"discovery incomplete")
    freeze({**doc,"members":[doc["members"][0],doc["members"][0]]},expect=2)
    freeze({**doc,"seed_pairs":19},expect=2)
    freeze({**doc,"sign_model":"pretend_validated"},expect=2)
    freeze({**doc,"alpha":.5},expect=2)
    freeze({**doc,"members":[{"discovery_pair":99,"direction":1}]},expect=2)
    freeze({**doc,"members":[{"discovery_pair":0,"direction":0}]},expect=2)
    run(a.binary,"family-confirm","--run",folder,expect=2)
    first=json.loads(freeze(doc))
    need(not first["complete"] and "members" not in first,"premature member report")
    freeze(doc,expect=2)
    f,_,_,_=inspect(folder,0)
    need(set(f["holdout_seeds"]).isdisjoint(s[0] for s in discovery),"discovery leakage")
    def crash(fault,rc):
        run(a.binary,"family-confirm","--run",folder,"--pause-after","1",
            expect=rc,env=dict(os.environ,PHASESHIFT_V3_FAILPOINT=fault))
    crash("before_holdout_commit",88)
    inspect(folder,0)
    crash("after_holdout_commit",89)
    inspect(folder,1)
    partial=json.loads(run(a.binary,"family-report","--run",folder))
    need(partial["committed_pairs"]==1 and "members" not in partial,"interim p-value leak")
    run(a.binary,"family-confirm","--run",folder,"--pause-after","4")
    inspect(folder,5)
    result=json.loads(run(a.binary,"family-confirm","--run",folder))
    f,valid,positive,rows=inspect(folder,40)
    audit_report(result,f,valid,positive)
    holdout=folder/"confirmation"/"family.sqlite"
    sidecar_digest=snapshot(holdout)
    need(json.loads(run(a.binary,"family-report","--run",folder))==result,"report drift")
    need(json.loads(run(a.binary,"family-confirm","--run",folder))==result,"completed resume changed verdict")
    need(snapshot(raw_db)==digest and snapshot(holdout)==sidecar_digest,"read-only replay mutated evidence")
    for name,sql in (
        ("alter-effect","UPDATE v3_pairs SET log_contrast=log_contrast+0.75 WHERE pair_number=0"),
        ("alter-order","UPDATE v3_pairs SET left_first=1-left_first WHERE pair_number=0"),
        ("alter-seed","UPDATE v3_pairs SET seed=seed+1 WHERE pair_number=0"),
        ("alter-spec","UPDATE v3_family SET specification='{}'"),
        ("alter-raw-cpu","UPDATE v3_samples SET cpu_ns=COALESCE(cpu_ns,0)+123 WHERE pair_number=0 AND side=0")):
        broken=root/name
        shutil.copytree(folder,broken)
        with sqlite3.connect(broken/"confirmation"/"family.sqlite") as db:
            db.execute(sql)
        run(a.binary,"family-report","--run",broken,expect=2)
    broken=root/"alter-source"
    shutil.copytree(folder,broken)
    with sqlite3.connect(broken/"run.sqlite") as db:
        db.execute("UPDATE v3_pairs SET pair_wall_ns=pair_wall_ns+17 WHERE pair_number=0")
    run(a.binary,"family-report","--run",broken,expect=2)
    # Separate unverified-sign-model family: do not reuse holdout data.
    cfg["seed"]+=17
    other=root/"unverified"
    run(a.binary,"conditional-tree","--config",manifest(root/"other.json",cfg),"--out",other,"--pairs","6")
    otherdoc={**doc,"family_id":"other","sign_model":"unverified","members":[doc["members"][0]]}
    run(a.binary,"family-freeze","--run",other,"--manifest",manifest(root/"other-manifest.json",otherdoc))
    outcome=json.loads(run(a.binary,"family-confirm","--run",other))
    f2,v2,p2,_=inspect(other,20,1)
    audit_report(outcome,f2,v2,p2)
    need(set(f2["holdout_seeds"]).isdisjoint(f["holdout_seeds"]),"different source reused holdout seed")
    print("PASS real-worker frozen family: disjoint seeds, both crash boundaries, exact Holm oracle, no peeking, tamper rejection, abstention")
if __name__=="__main__":
    main()
