#!/usr/bin/env python3
"""Real-worker V3-08 null stress. Never certifies wall-time independence/FWER."""
from __future__ import annotations
import argparse
from collections import defaultdict
from contextlib import contextmanager
import json
import math
import os
from pathlib import Path
import sqlite3
import statistics
import subprocess
import sys
import time

from protocol import seal, sha256_file, sqlite_snapshot_sha256

MANIFEST=Path(__file__).with_name("null_calibration_manifest.json")
MASK=(1<<64)-1
GAMMA=0x9e3779b97f4a7c15

def require(ok,message):
    if not ok:
        raise ValueError(message)

def write(path,value):
    path.write_text(json.dumps(value,sort_keys=True,indent=2,allow_nan=False)+"\n")

def read(path):
    return json.loads(path.read_text())

def splitmix(state):
    z=(state+GAMMA)&MASK
    z=((z^(z>>30))*0xbf58476d1ce4e5b9)&MASK
    z=((z^(z>>27))*0x94d049bb133111eb)&MASK
    return z^(z>>31)

def expected_work(algorithm,seed,x,threshold):
    if algorithm=="calibration":
        return 1000,False
    c=splitmix(seed^0xf20c938bda5716a1)
    state=seed^((x*GAMMA)&MASK)^((threshold*0x94d049bb133111eb)&MASK)^0xb58154fca3ad590d
    l=splitmix(state)
    magnitude=3 if l&15==0 else x%3
    exponent=6+(1 if c&1 else -1)+(magnitude if l&16 else -magnitude)
    return 64<<exponent,algorithm=="calibration_null_fault_v3" and x%4==3 and splitmix((state+GAMMA)&MASK)&7==0

def config(m,s,seed):
    return {"schema_version":1,"experiment_id":"null-"+s["id"],
            "problem_family":"calibration","algorithms":[s["algorithm"]],
            "seed":seed,"primary_axis":"x","fixed":{"negative":True},
            "axes":{"x":{"values":[0,1,2,3,4,5]},"threshold":{"values":[2,3,4]}},
            "search":{"policy_version":"v2","method":"uniform_v2","max_cases":2*m["discovery_pairs"],
                      "admissible_subspaces":[{"threshold":[2,3]},{"threshold":[4],"x":[2,3]}]},
            "measurement":{"paired_discovery_blocks":1,"paired_confirm_blocks":m["holdout_pairs"],
                           "warmup_iterations":0,"minimum_batch_ms":1},
            "min_practical_ratio":m["minimum_ratio"],"limits":{"total_wall_seconds":90,"max_search_queue":64}}

def rows(path,algorithm):
    result=[]
    with sqlite3.connect(path.resolve().as_uri()+"?mode=ro",uri=True) as db:
        for i,seed,status,stored in db.execute("SELECT pair_number,seed,status,log_contrast FROM v3_pairs ORDER BY pair_number"):
            require(i==len(result),"noncontiguous pair sequence")
            raw=db.execute("""SELECT s.side,s.algorithm,s.status,s.ns,s.batch,s.work,c.seed,c.params
                FROM v3_samples s JOIN v3_cases c USING(case_id)
                WHERE s.pair_number=? ORDER BY side,ordinal""",(i,)).fetchall()
            require(len(raw)==2 and [r[0] for r in raw]==[0,1],"wrong raw row cardinality")
            levels=[];work=[];coordinates=[];statuses=[]
            for _,a,st,ns,batch,w,case_seed,params in raw:
                p=json.loads(params);x=p["x"];t=p["threshold"]
                require(a==algorithm and case_seed==seed and p["negative"] is True,"wrong generator/seed")
                require(0<=x<=5 and t in (2,3,4) and (t!=4 or x in (2,3)),"illegal null case")
                expected,fault=expected_work(a,seed,x,t)
                if st=="ok":
                    require(not fault and w==expected and ns>0 and batch>0,"worker work/fault oracle mismatch")
                    levels.append(math.log(ns/batch))
                    work.append(math.log(w))
                else:
                    require(ns is None and batch is None and w is None,"invalid status with finite timing")
                    if st=="wrong_answer":
                        require(fault,"unexpected oracle disagreement")
                    levels.append(None);work.append(None)
                coordinates.append((t,x));statuses.append(st)
            require(coordinates[0][0]==coordinates[1][0] and coordinates[0][1]<coordinates[1][1],"mismatched structure")
            effect=levels[1]-levels[0] if None not in levels else None
            wd=work[1]-work[0] if None not in work else None
            require((status=="ok")== (effect is not None) and (stored is None)==(effect is None),"wrong pair status")
            require(effect is None or math.isclose(stored,effect,abs_tol=1e-12,rel_tol=1e-12),"wrong raw contrast")
            result.append({"pair":i,"seed":seed,"edge":[list(c) for c in coordinates],
                           "effect":effect,"work_effect":wd,"statuses":statuses,"levels":levels})
        require(len({r["seed"] for r in result})==len(result),"duplicate seed")
    return result

def select(m,discovery):
    edges=defaultdict(list)
    for r in discovery:
        if r["effect"] is not None:
            edges[tuple(tuple(x) for x in r["edge"])].append(r)
    ranked=[]
    for edge,group in edges.items():
        med=statistics.median(r["effect"] for r in group)
        ranked.append((-abs(med),edge,group[0]["pair"],1 if med>=0 else -1))
    ranked.sort()
    return [{"discovery_pair":i,"direction":d} for _,_,i,d in ranked[:m["members"]]]

def family_manifest(m,s,seed,choices):
    # Exercise nominal rejection calculation as a stress diagnostic ONLY.
    # No sampling assumption is established for actual host timings.
    return {"family_id":s["id"]+"-"+str(seed),"seed_pairs":m["holdout_pairs"],"alpha":m["alpha"],
            "sign_model":"independent_seed_signs_declared","members":choices}

def tail(n,k):
    return sum(math.comb(n,j) for j in range(k,n+1))/2**n

def independent_verdict(m,choices,discovery,holdout):
    n=m["holdout_pairs"];k=len(choices)
    require(len(holdout)==n*k,"incomplete family")
    require(not ({r["seed"] for r in discovery}&{r["seed"] for r in holdout}),"holdout leak")
    p=[];out=[]
    for j,ch in enumerate(choices):
        group=holdout[j::k];edge=discovery[ch["discovery_pair"]]["edge"]
        require(all(r["edge"]==edge and r["seed"]>=2**62 for r in group),"changed frozen member")
        valid=sum(r["effect"] is not None for r in group)
        positives=sum(r["effect"] is not None and ch["direction"]*r["effect"]>math.log(m["minimum_ratio"]) for r in group)
        p.append(tail(n,positives) if valid==n else 1.)
        out.append({"member_index":j,"valid_pairs":valid,"positive_pairs":positives})
    running=0.
    for rank,j in enumerate(sorted(range(k),key=lambda j:p[j])):
        running=max(running,p[j]*(k-rank))
        out[j].update(raw_p=p[j],holm_p=min(1.,running),
                      state="ABSTAIN_CENSORED" if out[j]["valid_pairs"]!=n else
                      "REJECTED_UNDER_DECLARED_MODEL" if running<=m["alpha"] else "NOT_REJECTED")
    return out

def autocorr(values):
    # Pair chronology, not independently sampled sign observations.
    pairs=[(a,b) for a,b in zip(values,values[1:]) if a is not None and b is not None]
    if len(pairs)<3:return None
    a,b=zip(*pairs)
    try:return statistics.correlation(a,b)
    except statistics.StatisticsError:return None

def audit(m,s,seed,folder):
    discovery_path=folder/"run"/"run.sqlite"
    d=rows(discovery_path,s["algorithm"])
    require(len(d)==m["discovery_pairs"],"incomplete discovery")
    choices=select(m,d)
    require(read(folder/"config.json")==config(m,s,seed),"changed trial config")
    with sqlite3.connect(discovery_path.resolve().as_uri()+"?mode=ro",uri=True) as db:
        require(db.execute("PRAGMA user_version").fetchone()[0]==15,"wrong discovery schema")
        source,cap=db.execute("SELECT source_json,max_pairs FROM v3_plan").fetchone()
        require(json.loads(source)==config(m,s,seed) and cap==m["discovery_pairs"],"changed source plan")
    base={"scenario":s["id"],"seed":seed,"discovery_pairs":len(d),"selected_members":len(choices),
          "discovery_digest":sqlite_snapshot_sha256(discovery_path),
          "discovery_lag1":autocorr([r["effect"] for r in d]),
          "discovery_wrong_answers":sum(st=="wrong_answer" for r in d for st in r["statuses"])}
    if not choices:
        require(not (folder/"run"/"confirmation").exists(),"unexpected family without candidates")
        return {**base,"state":"NO_ELIGIBLE_MEMBER","holdout_pairs":0,"nominal_rejection":False}
    require(read(folder/"family.json")==family_manifest(m,s,seed,choices),"changed adaptive selection")
    path=folder/"run"/"confirmation"/"family.sqlite"
    h=rows(path,s["algorithm"])
    with sqlite3.connect(path.resolve().as_uri()+"?mode=ro",uri=True) as db:
        manifest,spec,digest=db.execute("SELECT manifest,specification,spec_sha256 FROM v3_family").fetchone()
        import hashlib
        require(hashlib.sha256(spec.encode()).hexdigest()==digest and json.loads(manifest)==read(folder/"family.json"),"family seal mismatch")
        spec=json.loads(spec)
        require([r["seed"] for r in h]==spec["holdout_seeds"],"frozen seed inventory mismatch")
    expected=independent_verdict(m,choices,d,h)
    report=read(folder/"family_report.json")
    require(report["complete"] and not report["confirmed"] and not report["assumptions_empirically_validated"] and
            report["planned_pairs"]==len(h) and report["family_sha256"]==digest,"unsupported report")
    require(len(report["members"])==len(expected),"changed denominator")
    for a,b in zip(report["members"],expected):
        require(all(math.isclose(a[k],v,abs_tol=1e-14) if k in ("raw_p","holm_p") else a[k]==v
                    for k,v in b.items()),"independent Holm oracle mismatch")
    return {**base,"state":"COMPLETE","holdout_pairs":len(h),
            "holdout_digest":sqlite_snapshot_sha256(path),
            "holdout_lag1":autocorr([r["effect"] for r in h]),
            "holdout_wrong_answers":sum(st=="wrong_answer" for r in h for st in r["statuses"]),
            "members":expected,"nominal_rejection":any(r["state"]=="REJECTED_UNDER_DECLARED_MODEL" for r in expected)}

@contextmanager
def load_stress(enabled):
    if not enabled:
        yield;return
    # Linux: pin controller and all descendant workers to one allowed CPU,
    # then contend with a real process toggling long busy/idle bursts.
    # No claim that this achieves a prescribed wall-time correlation.
    affinity=os.sched_getaffinity(0)
    os.sched_setaffinity(0,{min(affinity)})
    child=None
    try:
        child=subprocess.Popen([sys.executable,"-c",
            "import time\nwhile True:\n end=time.monotonic()+0.075\n while time.monotonic()<end: pass\n time.sleep(0.075)\n"])
        yield
    finally:
        if child is not None:
            child.terminate()
            try:child.wait(timeout=3)
            except subprocess.TimeoutExpired:child.kill();child.wait()
        os.sched_setaffinity(0,affinity)

def execute(binary,folder,m,s,seed):
    folder.mkdir()
    write(folder/"config.json",config(m,s,seed))
    commands=[]
    def cmd(*args):
        start=time.monotonic_ns()
        try:
            r=subprocess.run([str(binary),*map(str,args)],capture_output=True,text=True,timeout=120)
            entry={"args":list(map(str,args)),"rc":r.returncode,"stdout":r.stdout,"stderr":r.stderr}
        except subprocess.TimeoutExpired:
            entry={"args":list(map(str,args)),"rc":124,"stdout":"","stderr":"command timeout"}
        entry["wall_ns"]=time.monotonic_ns()-start
        commands.append(entry);write(folder/"commands.json",commands)
        require(entry["rc"]==0,"CLI failed: "+str(args[0]))
        return entry["stdout"]
    run=folder/"run"
    with load_stress(s["load"]):
        cmd(m["policy"],"--config",folder/"config.json","--out",run,"--pairs",m["discovery_pairs"],"--pause-after",4)
        cmd("resume","--run",run)
        cmd("verify","--run",run)
        d=rows(run/"run.sqlite",s["algorithm"])
        require(len(d)==m["discovery_pairs"],"discovery incomplete")
        choices=select(m,d)
        if choices:
            write(folder/"family.json",family_manifest(m,s,seed,choices))
            frozen=json.loads(cmd("family-freeze","--run",run,"--manifest",folder/"family.json"))
            require(not frozen["complete"] and "members" not in frozen,"early outcomes")
            interim=json.loads(cmd("family-confirm","--run",run,"--pause-after",5))
            require(not interim["complete"] and "members" not in interim,"interim outcomes")
            cmd("family-confirm","--run",run)
            report=json.loads(cmd("family-report","--run",run))
            write(folder/"family_report.json",report)
    return audit(m,s,seed,folder)

def summarize(plan,root):
    m=plan["manifest"]
    results=[]
    for scenario,seed in plan["order"]:
        s=next(s for s in m["scenarios"] if s["id"]==scenario)
        folder=root/(scenario+"-"+str(seed))
        terminal=read(folder/"terminal.json")
        if terminal["state"]=="FAILED":
            # Failed attempts never disappear or enter completed denominators.
            results.append({"scenario":scenario,"seed":seed,"state":"FAILED","reason":terminal["reason"]})
            continue
        row=audit(m,s,seed,folder)
        require(row==terminal,"tampered terminal audit")
        results.append(row)
    groups={}
    for s in m["scenarios"]:
        subset=[r for r in results if r["scenario"]==s["id"]]
        resolved=[r for r in subset if r["state"]!="FAILED"]
        complete=[r for r in subset if r["state"]=="COMPLETE"]
        rejections=sum(r["nominal_rejection"] for r in resolved)
        failures=len(subset)-len(resolved)
        groups[s["id"]]={"planned":len(subset),"completed_families":len(complete),
                         "no_eligible_members":len(resolved)-len(complete),"failed":failures,
                         "nominal_rejections":rejections,
                         "fraction_bounds_over_planned":[rejections/len(subset),(rejections+failures)/len(subset)],
                         "complete_family_rejection_fraction":rejections/len(complete) if complete else None,
                         "binomial_population_interval":None,
                         "interval_reason":"shared host; independent campaign outcomes not established"}
    return {"plan_sha256":seal(plan),"eligible_for_claim":False,"population_fwer_certified":False,
            "truth_scope":m["truth"],"groups":groups,"trials":results}

def main():
    p=argparse.ArgumentParser()
    p.add_argument("--binary",required=True,type=Path)
    p.add_argument("--out",required=True,type=Path)
    p.add_argument("--diagnostic",action="store_true")
    p.add_argument("--verify-only",action="store_true")
    a=p.parse_args();binary=a.binary.resolve();root=a.out.resolve()
    m=read(MANIFEST)
    require(m["schema"]==1 and not m["eligible_for_claim"],"manifest scope")
    seeds=m["diagnostic_seeds"] if a.diagnostic else m["campaign_seeds"]
    plan={"manifest":m,"manifest_sha256":sha256_file(MANIFEST),"binary_sha256":sha256_file(binary),
          "worker_sha256":sha256_file(binary.with_name("phaseshift_worker")),
          "runner_sha256":sha256_file(Path(__file__)),"protocol_sha256":sha256_file(Path(__file__).with_name("protocol.py")),
          "diagnostic":a.diagnostic,"order":[[s["id"],seed] for seed in seeds for s in m["scenarios"]]}
    if a.verify_only:
        frozen=read(root/"frozen_plan.json")
        require(frozen=={"plan":plan,"sha256":seal(plan)},"invalid frozen plan")
        require(summarize(plan,root)==read(root/"scorecard.json"),"scorecard tampering")
        for row in read(root/"scorecard.json")["trials"]:
            if row["state"]!="COMPLETE":continue
            folder=root/(row["scenario"]+"-"+str(row["seed"]))
            check=subprocess.run([str(binary),"family-report","--run",str(folder/"run")],capture_output=True,text=True,timeout=120)
            require(check.returncode==0 and json.loads(check.stdout)==read(folder/"family_report.json"),"raw family replay disagrees")
        print("PASS reconstructed null calibration scorecard");return
    root.mkdir(parents=True)
    write(root/"frozen_plan.json",{"plan":plan,"sha256":seal(plan)})
    for scenario,seed in plan["order"]:
        s=next(s for s in m["scenarios"] if s["id"]==scenario)
        folder=root/(scenario+"-"+str(seed))
        try:row=execute(binary,folder,m,s,seed)
        except (ValueError,RuntimeError,OSError,sqlite3.Error) as e:
            row={"state":"FAILED","reason":str(e)}
        write(folder/"terminal.json",row)
    report=summarize(plan,root)
    write(root/"scorecard.json",report)
    print(json.dumps(report["groups"],sort_keys=True))
    require(not any(g["failed"] for g in report["groups"].values()),"failed campaigns retained in scorecard")

if __name__=="__main__":
    main()
