#!/usr/bin/env python3
"""V3-04/05 development-only real-worker comparison, with independent score reconstruction.

This is a fixed pair-count design, not equal consumed resources or confirmation.
Negative results and failed trials are retained. No external/SOTA claim.
"""
from __future__ import annotations
import argparse
import copy
import json
import math
from pathlib import Path
import platform
import random
import resource
import sqlite3
import subprocess
import time

from protocol import canonical, seal, sha256_file, sqlite_snapshot_sha256

HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[1]
MANIFEST=HERE/"paired_development_manifest.json"
POLICIES=("conditional-tree","acquisition-v3")
SCHEMAS={**dict(zip(POLICIES,(14,15))),"acquisition-v3-coverage":18}

def need(ok,msg):
    if not ok:
        raise ValueError(msg)

def read(path):
    return json.loads(path.read_text())

def write(path,data):
    path.write_text(json.dumps(data,sort_keys=True,indent=2,allow_nan=False)+"\n")

def manifest():
    m=read(MANIFEST)
    need(m["schema"]==1 and m["suite"]=="development" and
         m["eligible_for_claim"] is False and m["sota_claim"] is False,
         "development-only manifest")
    need(tuple(m["policies"])==POLICIES and 1<=m["pairs"]<=100,"policy contract")
    need(len(m["seeds"])==20 and len(set(m["seeds"]))==20 and
         len(m["diagnostic_seeds"])==1 and set(m["diagnostic_seeds"])<=set(m["seeds"]),
         "frozen development seeds")
    need(len({f["name"] for f in m["fixtures"]})==4,"fixture inventory")
    return m

def order(m,diagnostic):
    rows=[]
    for f in m["fixtures"]:
        for seed in m["diagnostic_seeds"] if diagnostic else m["seeds"]:
            arms=list(POLICIES)
            random.Random(int(seal([f["name"],seed]),16)).shuffle(arms)
            rows.extend([[f["name"],p,seed] for p in arms])
    return rows

def trial_config(f,seed):
    cfg=copy.deepcopy(f["config"])
    cfg["seed"]=seed
    return cfg

def usage(which):
    r=resource.getrusage(which)
    return round((r.ru_utime+r.ru_stime)*1e9)

def measured(args,timeout):
    self0,child0=usage(resource.RUSAGE_SELF),usage(resource.RUSAGE_CHILDREN)
    start=time.perf_counter_ns()
    try:
        p=subprocess.run(args,capture_output=True,text=True,timeout=timeout)
        rc,stdout,stderr=p.returncode,p.stdout,p.stderr
    except subprocess.TimeoutExpired as e:
        rc=124
        stdout=(e.stdout or b"")
        stderr=(e.stderr or b"")
        stdout=stdout.decode(errors="replace") if isinstance(stdout,bytes) else stdout
        stderr=stderr.decode(errors="replace") if isinstance(stderr,bytes) else stderr
    return {"argv":list(map(str,args)),"returncode":rc,
            "wall_ns":time.perf_counter_ns()-start,
            "parent_cpu_ns":max(0,usage(resource.RUSAGE_SELF)-self0),
            "waited_child_cpu_ns":max(0,usage(resource.RUSAGE_CHILDREN)-child0),
            "stdout":stdout,"stderr":stderr}

def decode(s):
    pos=0
    out={}
    while pos<len(s):
        def field():
            nonlocal pos
            colon=s.index(":",pos)
            n=int(s[pos:colon])
            need(n>=1 and colon+1+n<=len(s),"bad coordinate length")
            value=s[colon+1:colon+1+n]
            pos=colon+1+n
            return value
        key=field()
        need(key not in out,"duplicate coordinate")
        out[key]=field()
    return out

def allowed(f,coords):
    cfg=f["config"]
    def label(x):
        return str(x).lower() if isinstance(x,bool) else str(x)
    if set(coords)!=set(cfg["axes"]):
        return False
    if any(coords[k] not in [label(v) for v in axis["values"]]
           for k,axis in cfg["axes"].items()):
        return False
    scopes=cfg["search"].get("admissible_subspaces",[])
    return not scopes or any(all(coords[k] in [label(v) for v in values]
                                 for k,values in scope.items()) for scope in scopes)

def oracle(f,left,right):
    truth=f["truth"]
    if truth=="unlabelled":
        return None
    if truth=="calibration_null" or left.get("negative")=="true":
        return False
    threshold=int(left["threshold"])
    return int(left["x"])<threshold<=int(right["x"])

def median(v):
    v=sorted(v)
    n=len(v)
    return v[n//2] if n%2 else (v[n//2-1]+v[n//2])/2

def read_trial(dbpath,f,policy,seed,pairs,complete):
    if not dbpath.is_file():
        need(not complete,"successful CLI has no SQLite evidence")
        return {"completed_pairs":0,"valid_pairs":0,"censored_pairs":0,
                "distinct_edges":0,"nomination_count":0,"true_edge_count":None,
                "false_nomination_count":None,"first_true_nomination_pair":None,
                "worker_timed_cpu_ns":None,"pair_measurement_wall_ns":0,
                "cost_enabled_pairs":None}
    cfg=f["config"]
    with sqlite3.connect(dbpath.resolve().as_uri()+"?mode=ro",uri=True) as db:
        db.execute("PRAGMA query_only=ON")
        need(db.execute("PRAGMA user_version").fetchone()[0]==SCHEMAS[policy],"schema")
        source=db.execute("SELECT source_json,max_pairs FROM v3_plan WHERE id=1").fetchone()
        need(json.loads(source[0])==trial_config(f,seed) and source[1]==pairs,
             "frozen input mismatch")
        rows=db.execute("""SELECT p.pair_number,p.seed,p.status,p.log_contrast,
                              p.pair_wall_ns,p.worker_timed_cpu_ns,
                              d.axis,d.left_coords,d.right_coords,d.arm
                       FROM v3_pairs p JOIN v3_conditional_decisions d USING(pair_number)
                       ORDER BY p.pair_number""").fetchall()
        need(len(rows)<=pairs and (not complete or len(rows)==pairs),"pair cap/count")
        need(len({r[1] for r in rows})==len(rows),"reused replicate seed")
        need(db.execute("SELECT COUNT(*) FROM v3_pairs").fetchone()[0]==len(rows),
             "incomplete committed paired decision")
        valid=nominees=false_nom=0
        truths=set()
        unique=set()
        first=None
        cpu=0
        known=True
        total_wall=0
        enabled=0 if policy in ("acquisition-v3","acquisition-v3-coverage") else None
        for i,(number,rep,status,effect,wall,stored_cpu,axis,l,r,arm) in enumerate(rows):
            need(number==i and wall>0 and arm in ("explore","replicate"),"pair identity")
            left,right=decode(l),decode(r)
            need(allowed(f,left) and allowed(f,right) and axis==cfg["primary_axis"] and
                 all(left[k]==right[k] for k in left if k!=axis) and
                 int(left[axis])<int(right[axis]),"illegal pair")
            edge=(l,r)
            unique.add(edge)
            groups=[[],[]]
            raw=db.execute("""SELECT s.side,s.algorithm,s.status,s.ns,s.batch,s.cpu_ns,c.seed,c.params
                FROM v3_samples s JOIN v3_cases c USING(case_id) WHERE s.pair_number=?
                ORDER BY s.side,s.ordinal""",(i,)).fetchall()
            need(len(raw)==(2 if len(cfg["algorithms"])==1 else 8),"raw worker cardinality")
            for side,algo,st,ns,batch,cp,case_seed,params in raw:
                need(side in (0,1) and case_seed==rep and algo in cfg["algorithms"],
                     "raw seed/algorithm mismatch")
                coord=left if side==0 else right
                obj=json.loads(params)
                for k,v in coord.items():
                    interpreted=str(obj[k]).lower() if isinstance(obj[k],bool) else str(obj[k])
                    need(interpreted==v,"raw worker coordinate mismatch")
                need((st=="ok")== (ns is not None and batch is not None and ns>0 and batch>0),
                     "failed sample promoted to speed")
                groups[side].append((algo,st,ns,batch))
            raw_cpu=None if any(x[5] is None for x in raw) else sum(x[5] for x in raw)
            need(raw_cpu==stored_cpu,"CPU differs from raw samples")
            if raw_cpu is None:
                known=False
            else:
                cpu+=raw_cpu
            total_wall+=wall
            ok=all(x[1]=="ok" for g in groups for x in g)
            need((status=="ok")==ok and (effect is not None)==ok,"censor mismatch")
            if ok:
                side_metric=[]
                for group in groups:
                    times=[median([ns/batch for a,_,ns,batch in group if a==algo])
                           for algo in cfg["algorithms"]]
                    side_metric.append(math.log(times[0])-(math.log(times[1]) if len(times)==2 else 0))
                need(math.isclose(effect,side_metric[1]-side_metric[0],rel_tol=1e-12,abs_tol=1e-12),
                     "independent raw effect disagreement")
                valid+=1
                nominate=abs(effect)>=math.log(cfg["detection"]["min_practical_ratio"])
                actual=oracle(f,left,right)
                if nominate:
                    nominees+=1
                    if actual is False:
                        false_nom+=1
                    if actual is True and effect>0:
                        truths.add(edge)
                        if first is None:
                            first=i+1
            if enabled is not None:
                audit=db.execute("SELECT diagnostic,actual_wall_ns,actual_worker_cpu_ns "
                                 "FROM v3_acquisition_decisions WHERE pair_number=?",(i,)).fetchone()
                need(audit is not None and audit[1:]==(wall,stored_cpu),"v15 cost binding")
                diag=dict(x.split("=",1) for x in audit[0].split(";"))
                need(diag["enabled"] in ("0","1"),"invalid cost eligibility")
                enabled+=int(diag["enabled"])
        labelled=f["truth"]!="unlabelled"
        return {"completed_pairs":len(rows),"valid_pairs":valid,
                "censored_pairs":len(rows)-valid,"distinct_edges":len(unique),
                "nomination_count":nominees,"true_edge_count":len(truths) if labelled else None,
                "false_nomination_count":false_nom if labelled else None,
                "first_true_nomination_pair":first,"worker_timed_cpu_ns":cpu if known else None,
                "pair_measurement_wall_ns":total_wall,"cost_enabled_pairs":enabled}

def trial(root,ident,log,m):
    fixture,policy,seed=ident
    f=next(f for f in m["fixtures"] if f["name"]==fixture)
    folder=root/f"{fixture}_{policy}_{seed}"
    cfg=folder/"config.json"
    need(read(cfg)==trial_config(f,seed),"changed config")
    run=log["explore"]
    for k in ("wall_ns","parent_cpu_ns","waited_child_cpu_ns"):
        need(type(run[k]) is int and run[k]>=0,"invalid resource account")
    need(run["wall_ns"]>0,"missing elapsed wall")
    total=run["parent_cpu_ns"]+run["waited_child_cpu_ns"]
    dbpath=folder/"run"/"run.sqlite"
    complete=run["returncode"]==0
    score=read_trial(dbpath,f,policy,seed,m["pairs"],complete)
    verified=log["verify"] is not None and log["verify"]["returncode"]==0
    return {"fixture":fixture,"policy":policy,"seed":seed,
            "source_config_sha256":sha256_file(cfg),
            "sqlite_snapshot_sha256":sqlite_snapshot_sha256(dbpath) if dbpath.is_file() else None,
            "status":"ok" if complete and verified else "failed",
            "observed_parent_cpu_ns":run["parent_cpu_ns"],
            "observed_waited_cli_subtree_cpu_ns":run["waited_child_cpu_ns"],
            "observed_total_cpu_ns":total,"observed_total_wall_ns":run["wall_ns"],
            "within_consumed_ceilings":total<=m["cpu_ceiling_ns"] and run["wall_ns"]<=m["wall_ceiling_ns"],
            **score}

def summarize(rows,m):
    result=[]
    for f in m["fixtures"]:
        for p in POLICIES:
            rs=[r for r in rows if r["fixture"]==f["name"] and r["policy"]==p]
            labelled=f["truth"]!="unlabelled"
            eligible=lambda r:r["status"]=="ok" and r["within_consumed_ceilings"]
            success=lambda r:eligible(r) and (r["true_edge_count"] or 0)>0
            n=len(rs)
            # Conservative terminal-time success curve: no interim full-system
            # CPU attribution, no claim of independently confirmed hypotheses.
            curves={}
            if labelled and f["truth"]!="calibration_null":
                for key,cap in (("observed_total_cpu_ns",m["cpu_ceiling_ns"]),
                                ("observed_total_wall_ns",m["wall_ceiling_ns"])):
                    horizons=[cap*i//20 for i in range(21)]
                    probabilities=[sum(success(r) and r[key]<=h for r in rs)/n for h in horizons]
                    curves[key]={"horizons_ns":horizons,"discovery_fraction":probabilities}
            result.append({"fixture":f["name"],"policy":p,"trials":n,
                           "failed_trials":sum(r["status"]!="ok" for r in rs),
                           "over_ceiling_trials":sum(not r["within_consumed_ceilings"] for r in rs),
                           "completed_pairs":sum(r["completed_pairs"] for r in rs),
                           "distinct_edges_across_trials":sum(r["distinct_edges"] for r in rs),
                           "true_nomination_trials":sum(success(r) for r in rs) if labelled else None,
                           "false_nomination_count":sum(r["false_nomination_count"] or 0 for r in rs) if labelled else None,
                           "total_cpu_ns":sum(r["observed_total_cpu_ns"] for r in rs),
                           "total_wall_ns":sum(r["observed_total_wall_ns"] for r in rs),
                           "cost_enabled_pairs":sum(r["cost_enabled_pairs"] or 0 for r in rs) if p=="acquisition-v3" else None,
                           "conservative_discovery_curves":curves})
    return result

def execute(binary,root,diagnostic):
    m=manifest()
    need(binary.is_file(),"binary missing")
    need(not root.exists(),"immutable output exists")
    revision=subprocess.check_output(["git","rev-parse","HEAD"],cwd=ROOT,text=True).strip()
    need(len(revision)==40,"source revision unavailable")
    plan={"schema":1,"manifest":m,"manifest_sha256":sha256_file(MANIFEST),
          "binary_sha256":sha256_file(binary),"source_commit":revision,
          "diagnostic":diagnostic,"run_order":order(m,diagnostic),
          "host":{"platform":platform.platform(),"machine":platform.machine()},
          "eligible_for_claim":False,"sota_claim":False,
          "accounting":"fixed_pair_count; waited CLI subtree + harness invocation CPU; no audit cost in search budget"}
    root.mkdir(parents=True)
    write(root/"frozen_plan.json",{**plan,"sha256":seal(plan)})
    rows=[]
    for ident in plan["run_order"]:
        f=next(f for f in m["fixtures"] if f["name"]==ident[0])
        dest=root/f"{ident[0]}_{ident[1]}_{ident[2]}"
        dest.mkdir()
        write(dest/"config.json",trial_config(f,ident[2]))
        run=measured([str(binary),ident[1],"--config",str(dest/"config.json"),
                      "--out",str(dest/"run"),"--pairs",str(m["pairs"])],65)
        verify=None
        if run["returncode"]==0:
            verify=measured([str(binary),"verify","--run",str(dest/"run")],65)
        log={"explore":run,"verify":verify}
        write(dest/"commands.json",log)
        row=trial(root,ident,log,m)
        row["commands_sha256"]=sha256_file(dest/"commands.json")
        rows.append(row)
        print(json.dumps(row,sort_keys=True),flush=True)
    write(root/"results.json",{"plan_sha256":seal(plan),"eligible_for_claim":False,
                               "sota_claim":False,"rows":rows})

def verify(root):
    m=manifest()
    plan=read(root/"frozen_plan.json")
    digest=plan.pop("sha256")
    need(seal(plan)==digest and plan["manifest"]==m and
         plan["manifest_sha256"]==sha256_file(MANIFEST) and
         plan["run_order"]==order(m,plan["diagnostic"]) and
         plan["eligible_for_claim"] is False and plan["sota_claim"] is False,"plan provenance")
    stored=read(root/"results.json")
    need(stored["plan_sha256"]==digest and stored["eligible_for_claim"] is False and
         stored["sota_claim"] is False,"result provenance")
    rows=[]
    for ident in plan["run_order"]:
        dest=root/f"{ident[0]}_{ident[1]}_{ident[2]}"
        row=trial(root,ident,read(dest/"commands.json"),m)
        row["commands_sha256"]=sha256_file(dest/"commands.json")
        rows.append(row)
    need(rows==stored["rows"],"result inventory, cost or raw evidence mismatch")
    return {"schema":1,"source_commit":plan["source_commit"],"plan_sha256":digest,
            "diagnostic":plan["diagnostic"],"eligible_for_claim":False,"sota_claim":False,
            "summary":summarize(rows,m),
            "limitations":["Discovery nominations, not confirmed hypotheses or FWER",
                           "Fixed pair count, resource overshoots retained; not equal CPU",
                           "Synthetic truth only; sort/Top-K unlabelled",
                           "Only v14/v15 internal comparators; no SOTA claim",
                           "CPU restricted to waited CLI subtree plus invocation harness; verification excluded"]}

def main():
    p=argparse.ArgumentParser()
    p.add_argument("--binary",type=Path)
    p.add_argument("--out",type=Path,required=True)
    p.add_argument("--diagnostic",action="store_true")
    p.add_argument("--verify-only",action="store_true")
    a=p.parse_args()
    root=a.out.resolve()
    if not a.verify_only:
        if a.binary is None:
            p.error("--binary required")
        execute(a.binary.resolve(),root,a.diagnostic)
    report=verify(root)
    if not a.verify_only:
        write(root/"scorecard.json",report)
        need(all(r["failed_trials"]==0 for r in report["summary"]),
             "recorded worker failure; evidence retained")
    else:
        need(read(root/"scorecard.json")==report,"scorecard differs from raw evidence")
    print("PASS multi-family paired development comparison; no confirmation/SOTA claim",flush=True)
if __name__=="__main__":
    main()
