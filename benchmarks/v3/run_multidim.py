#!/usr/bin/env python3
"""V3-01b: real worker, frozen multidimensional cases, independent truth scoring.

No holdout promotion: a timing nomination is not a confirmed cliff. Recompute
all summary rows from authoritative SQLite, raw invocation logs and analytic
truth on --verify-only. Same seeds/configuration/pair count for v14/v15.
"""
import argparse
import json
import math
from pathlib import Path
import platform
import random
import sqlite3
import subprocess

import run_paired_development as pilot
from protocol import seal, sha256_file
from multidim_truth import (classify, config, groups, key, load, order, split_check,
                             truth)

HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[1]

def need(ok,why):
    if not ok:
        raise ValueError(why)

def read(path):
    return json.loads(path.read_text())

def write(path,obj):
    with path.open("x") as f:
        json.dump(obj,f,sort_keys=True,indent=2,allow_nan=False)
        f.write("\n")

def as_pilot(f):
    return {"name":f["id"],"truth":"calibration_threshold",
            "config":config(f,0)}

def adjudication(f,path,pairs):
    cfg=config(f,0)
    allowed=groups(cfg)
    legal={context:[p["x"] for p in points] for context,points in allowed.items()}
    proposals=[]
    with sqlite3.connect(path.resolve().as_uri()+"?mode=ro",uri=True) as db:
        db.execute("PRAGMA query_only=ON")
        rows=db.execute("""SELECT p.pair_number,p.status,p.log_contrast,
                           d.axis,d.left_coords,d.right_coords
                           FROM v3_pairs p JOIN v3_conditional_decisions d
                           USING(pair_number) ORDER BY p.pair_number""").fetchall()
        need(len(rows)==pairs,"missing/duplicate paired decision")
        for j,(number,status,effect,axis,l,r) in enumerate(rows):
            need(number==j and axis=="x" and status in ("ok","censored"),
                 "pair ordering/status/axis")
            a=pilot.decode(l);b=pilot.decode(r)
            need(set(a)==set(b)==set(cfg["axes"]) and
                 all(a[k]==b[k] for k in a if k!="x"),"illegal conditional pair")
            ctx=tuple((k,a[k]) for k in cfg["axes"] if k!="x")
            points=legal.get(ctx)
            need(points is not None,"absent stratum")
            x,y=int(a["x"]),int(b["x"])
            need(x in points and y in points and
                 points.index(y)==points.index(x)+1,
                 "nonadjacent/nonlegal edge")
            need((effect is not None)==(status=="ok") and
                 (effect is None or math.isfinite(effect)),
                 "invalid/censored effect promoted")
            proposals.append({"pair_number":number,"context":[list(v) for v in ctx],
                              "left":x,"right":y,"effect":effect,
                              "nominate":effect is not None and
                                abs(effect)>=math.log(cfg["detection"]["min_practical_ratio"])})
    return classify(f,proposals)

def row(dest,f,policy,seed,m):
    pf=as_pilot(f)
    ident=[f["id"],policy,seed]
    base=pilot.trial(dest.parent,ident,read(dest/"commands.json"),{"pairs":m["pairs"],
          "cpu_ceiling_ns":m["cpu_ceiling_ns"],"wall_ceiling_ns":m["wall_ceiling_ns"],
          "fixtures":[pf]})
    # The independent analytic scorer uses only committed v3 pairs, not a
    # search policy's internal boundary labels.
    db=dest/"run"/"run.sqlite"
    adjud=adjudication(f,db,base["completed_pairs"]) if db.is_file() else {
        "truth_edge_count":len(truth(f)),"true_edges_hit":0,
        "missed_truth_edges":len(truth(f)),"unique_false_edges":0,
        "false_observations":0,"repeat_true_nominations":0,
        "nomination_observations":0,"first_true_pair":None}
    # Censored and failed campaigns never earn eligible discoveries.
    eligible=base["status"]=="ok" and base["within_consumed_ceilings"]
    return {"fixture":f["id"],"policy":policy,"seed":seed,
            "measurement_endpoint":"unconfirmed_matched_timing_nomination",
            "analytic_truth_scope":"discrete_work_unit_jump_not_timing_truth",
            "eligible":eligible,"raw_trial":base,"adjudication":adjud,
            "true_hit_under_budget":adjud["true_edges_hit"] if eligible else 0,
            "truth_sha256":seal(truth(f)),
            "commands_sha256":sha256_file(dest/"commands.json")}

def curve(rows,key,ceiling):
    horizons=[ceiling*i//20 for i in range(21)]
    values=[sum(r["true_hit_under_budget"]>0 and
                r["raw_trial"][key]<=h for r in rows)/len(rows) for h in horizons]
    return {"horizons_ns":horizons,"hit_probability":values,
            "normalized_auc":sum((a+b)/40 for a,b in zip(values,values[1:]))}

def paired_bootstrap(rows_a,rows_b,metric,identity,replicates=2000):
    aa={r["seed"]:r for r in rows_a};bb={r["seed"]:r for r in rows_b}
    need(set(aa)==set(bb) and len(aa)>0,"nonmatched seed cohorts")
    differences=[metric(bb[s])-metric(aa[s]) for s in sorted(aa)]
    n=len(differences)
    rng=random.Random(int(seal([identity,"paired_bootstrap_seed_1"]),16))
    sampled=sorted(sum(differences[rng.randrange(n)] for _ in range(n))/n
                   for _ in range(replicates))
    return {"n":n,"mean_v15_minus_v14":sum(differences)/n,
            "exploratory_percentile95":[sampled[int(.025*replicates)],
                                        sampled[int(.975*replicates)-1]],
            "positive":sum(d>0 for d in differences),
            "negative":sum(d<0 for d in differences),
            "ties":sum(d==0 for d in differences),
            "inferential_claim":False}

def summarize(rows,m):
    result=[]
    for f in m["fixtures"]:
        rs=[r for r in rows if r["fixture"]==f["id"]]
        if not rs:continue
        for policy in m["policies"]:
            selected=[r for r in rs if r["policy"]==policy]
            need(bool(selected),"missing policy cohort")
            result.append({"fixture":f["id"],"policy":policy,
                "trials":len(selected),"truth_edges":len(truth(f)),
                "failed":sum(r["raw_trial"]["status"]!="ok" for r in selected),
                "over_ceiling":sum(not r["raw_trial"]["within_consumed_ceilings"] for r in selected),
                "censored_pairs":sum(r["raw_trial"]["censored_pairs"] for r in selected),
                "valid_pairs":sum(r["raw_trial"]["valid_pairs"] for r in selected),
                "true_hit_trials":sum(r["true_hit_under_budget"]>0 for r in selected),
                "total_distinct_true_hits":sum(r["true_hit_under_budget"] for r in selected),
                "false_nomination_observations":sum(r["adjudication"]["false_observations"] for r in selected),
                "mean_full_cpu_ns":sum(r["raw_trial"]["observed_total_cpu_ns"] for r in selected)/len(selected),
                "mean_full_wall_ns":sum(r["raw_trial"]["observed_total_wall_ns"] for r in selected)/len(selected),
                "curves":{"full_cpu":curve(selected,"observed_total_cpu_ns",m["cpu_ceiling_ns"]),
                          "full_wall":curve(selected,"observed_total_wall_ns",m["wall_ceiling_ns"])}})
        left=[r for r in rs if r["policy"]==m["policies"][0]]
        right=[r for r in rs if r["policy"]==m["policies"][1]]
        result.append({"fixture":f["id"],"comparison":"v15_vs_v14_developmental",
                       "matched_paired_true_hit_delta":paired_bootstrap(left,right,
                          lambda r:int(r["true_hit_under_budget"]>0),[m["suite"],f["id"],"hit"]),
                       "matched_paired_cpu_delta":paired_bootstrap(left,right,
                          lambda r:r["raw_trial"]["observed_total_cpu_ns"],
                          [m["suite"],f["id"],"cpu"]),
                       "comparable_endpoint":True,"sota_claim":False})
    return result

def verify(root):
    plan=read(root/"frozen_plan.json")
    digest=plan.pop("sha256")
    m,mh=load(plan["suite"]);split_check()
    need(seal(plan)==digest and plan["manifest_sha256"]==mh and
         plan["manifest"]==m and plan["run_order"]==order(m,plan["diagnostic"],plan["shard"])
         and plan["eligible_for_claim"] is False and plan["sota_claim"] is False,
         "modified frozen plan")
    by={f["id"]:f for f in m["fixtures"]}
    rows=[row(root/(fid+"_"+p+"_"+str(s)),by[fid],p,s,m)
          for fid,p,s in plan["run_order"]]
    need(read(root/"results.json")=={"plan_sha256":digest,"rows":rows},
         "raw trial/SQLite/truth mismatch")
    return {"schema":1,"source_commit":plan["source_commit"],
            "suite":plan["suite"],"diagnostic":plan["diagnostic"],
            "plan_sha256":digest,"eligible_for_claim":False,"sota_claim":False,
            "summary":summarize(rows,m),
            "limitations":["Only planted work-unit discontinuities are labelled",
                "V3 measures timing; nominations lack independent confirmation",
                "Smooth slopes may be legitimate gradients, not jump discoveries",
                "Equal pair counts are not equal consumed CPU or wall time",
                "Terminal cost assigned conservatively; proof of first-hit CPU time unavailable",
                "Paired percentile bootstrap exploratory and not calibrated FWER",
                "Waited CLI-subtree CPU misses escaped descendants",
                "No cross-engine V1/V2 versus V3 superior-performance verdict"]}

def main():
    p=argparse.ArgumentParser()
    p.add_argument("--binary",type=Path)
    p.add_argument("--out",required=True,type=Path)
    p.add_argument("--suite",choices=("development","evaluation"),default="development")
    p.add_argument("--diagnostic",action="store_true")
    p.add_argument("--shard")
    p.add_argument("--verify-only",action="store_true")
    a=p.parse_args()
    root=a.out.resolve()
    if a.verify_only:
        need(read(root/"scorecard.json")==verify(root),"scorecard drift")
        print("PASS independent multidimensional replay and truth audit; NO SOTA")
        return
    need(a.binary and a.binary.is_file(),"v3 CLI binary required")
    m,mh=load(a.suite);split_check()
    need(not root.exists(),"immutable output directory exists")
    revision=subprocess.check_output(["git","rev-parse","HEAD"],cwd=ROOT,text=True).strip()
    need(len(revision)==40,"source commit unavailable")
    plan={"schema":1,"source_commit":revision,"binary_sha256":sha256_file(a.binary),
          "suite":a.suite,"manifest":m,"manifest_sha256":mh,
          "run_order":order(m,a.diagnostic,a.shard),
          "shard":a.shard,"diagnostic":a.diagnostic,
          "host":platform.platform(),"eligible_for_claim":False,"sota_claim":False}
    root.mkdir(parents=True)
    digest=seal(plan)
    write(root/"frozen_plan.json",{**plan,"sha256":digest})
    by={f["id"]:f for f in m["fixtures"]}
    rows=[]
    for fid,policy,seed in plan["run_order"]:
        dest=root/(fid+"_"+policy+"_"+str(seed))
        dest.mkdir()
        pilot.write(dest/"config.json",config(by[fid],seed))
        run=pilot.measured([str(a.binary.resolve()),policy,"--config",str(dest/"config.json"),
                            "--out",str(dest/"run"),"--pairs",str(m["pairs"])],65)
        confirm=pilot.measured([str(a.binary.resolve()),"verify","--run",str(dest/"run")],65) if run["returncode"]==0 else None
        pilot.write(dest/"commands.json",{"explore":run,"verify":confirm})
        result=row(dest,by[fid],policy,seed,m)
        rows.append(result)
        print(json.dumps({"fixture":fid,"policy":policy,"seed":seed,
                          "true_hit_under_budget":result["true_hit_under_budget"],
                          "status":result["raw_trial"]["status"]}),flush=True)
    write(root/"results.json",{"plan_sha256":digest,"rows":rows})
    write(root/"scorecard.json",verify(root))
    need(not any(r["raw_trial"]["status"]!="ok" for r in rows),
         "command failure recorded; evidence preserved")

if __name__=="__main__":
    main()
