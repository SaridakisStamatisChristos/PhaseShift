#!/usr/bin/env python3
"""V3-01f temporal dependence audit of immutable V3-01e raw null evidence.

Chronology is the persisted v16 pair_number order within a family, NOT CPU
timestamps or globally synchronized ordering across independent GitHub jobs.
No significance p-values or population independence claims are made.
"""
from __future__ import annotations
import argparse
import json
import math
from pathlib import Path
import statistics
from confirmation_protocol import db_ro, require, read
from protocol import sha256_file
from run_forced_null_calibration import manifest as null_manifest, reconstruct as replay_null


def temporal_metrics(values):
    """Descriptive consecutive-pair diagnostics; None is a censored pair."""
    n=len(values)
    require(n==20,"temporal sign audit requires exactly 20 planned pairs")
    require(all(v is None or isinstance(v,(float,int)) and math.isfinite(v)
                for v in values),"non-finite null time contrast")
    valid=[(i,float(v)) for i,v in enumerate(values) if v is not None]
    signs={i:(1 if value>0 else -1 if value<0 else 0) for i,value in valid}
    adj=[(signs[i],signs[i+1]) for i in range(n-1)
         if i in signs and i+1 in signs and signs[i] and signs[i+1]]
    same=sum(a==b for a,b in adj)
    nonzero=[v for v in signs.values() if v]
    positives=sum(v==1 for v in nonzero)
    negatives=sum(v==-1 for v in nonzero)
    # Conditional on the 20 observed signs: probability an unordered
    # adjacent pair agrees under random permutation of that sign multiset.
    random_order_same=(
        (positives*(positives-1)+negatives*(negatives-1))
        /(len(nonzero)*(len(nonzero)-1))
        if len(nonzero)>1 else None)
    best=length=0
    previous=None
    for i in range(n):
        sign=signs.get(i)
        if sign is None or sign==0:
            length=0;previous=None
        else:
            length=length+1 if sign==previous else 1
            best=max(best,length);previous=sign
    lags=[(a,b) for a,b in zip(values,values[1:]) if a is not None and b is not None]
    correlation=None
    if len(lags)>=3:
        left,right=zip(*lags)
        if len(set(left))>1 and len(set(right))>1:
            correlation=statistics.correlation(left,right)
    first=[v for v in values[:10] if v is not None]
    last=[v for v in values[10:] if v is not None]
    half_shift=statistics.median(last)-statistics.median(first) if first and last else None
    return {
      "planned_pairs":20,"valid_pairs":len(valid),"censored_pairs":n-len(valid),
      "positive_contrasts":positives,"negative_contrasts":negatives,
      "zero_contrasts":len(valid)-len(nonzero),
      "adjacent_comparable":len(adj),"adjacent_same_signs":same,
      "conditional_random_order_same_probability":random_order_same,
      "longest_same_sign_run":best,"lag1_timing_contrast_correlation":correlation,
      "late_minus_early_median_log_contrast":half_shift,
      "risk_flag_abs_lag1_ge_half":correlation is not None and abs(correlation)>=.5,
      "risk_flag_sign_run_ge_16":best>=16,
      "risk_flag_median_half_shift_ge_log_1_2":
           half_shift is not None and abs(half_shift)>=math.log(1.2)
    }


def independent_contrasts(path):
    with db_ro(path) as db:
        db.execute("PRAGMA query_only=ON")
        rows=db.execute("""SELECT pair_number,seed,status,log_contrast
            FROM v3_pairs ORDER BY pair_number""").fetchall()
        require(len(rows)==20,"incomplete temporal holdout")
        output=[]
        for i,(number,seed,status,claimed) in enumerate(rows):
            require(number==i and status in ("ok","censored"),"pair order/status tampered")
            samples=db.execute("""SELECT s.side,s.status,s.ns,s.batch,c.seed
                FROM v3_samples s JOIN v3_cases c USING(case_id)
                WHERE s.pair_number=? ORDER BY s.side,s.ordinal""",(i,)).fetchall()
            require(len(samples)==2 and
                    [s[0] for s in samples]==[0,1] and
                    all(x[4]==seed for x in samples),"missing raw timeline evidence")
            ok=all(st=="ok" and ns is not None and batch is not None and
                   ns>0 and batch>0 for _,st,ns,batch,_ in samples)
            require(ok==(status=="ok") and ok==(claimed is not None),
                    "temporal sample censorship was promoted")
            if ok:
                delta=(math.log(samples[1][2]/samples[1][3])-
                       math.log(samples[0][2]/samples[0][3]))
                require(math.isclose(delta,claimed,rel_tol=1e-12,abs_tol=1e-12),
                        "stored time contrast not reproduced")
            else:
                delta=None
            output.append(delta)
        return output


def audit(roots):
    manifest,digest=null_manifest()
    names={s["id"] for s in manifest["scenario"]}
    require(set(roots)==names,"missing, duplicated, or extra V3-01e source stratum")
    results=[]
    revisions=set()
    for scene in manifest["scenario"]:
        name=scene["id"];root=roots[name]
        score=replay_null(root)
        require(read(root/"scorecard.json")==score and not score["diagnostic"] and
                score["planned_families"]==64 and
                score["manifest_sha256"]==digest and
                score["nominal_model_rejections"]==0,
                "nonvalidated V3-01e reference evidence")
        revisions.add(score["source_commit"])
        families=[]
        for seed in scene["seeds"]:
            path=root/str(seed)/"run"/"confirmation"/"family.sqlite"
            values=independent_contrasts(path)
            metrics=temporal_metrics(values)
            families.append({"seed":seed,
                             "source_sqlite_sha256":
                                  next(v["holdout_snapshot_sha256"]
                                       for v in score["results"] if v["seed"]==seed),
                             **metrics})
        require(len(families)==64,"missing frozen temporal family")
        comparisons=sum(x["adjacent_comparable"] for x in families)
        same=sum(x["adjacent_same_signs"] for x in families)
        results.append({
            "stratum":name,"families":64,"chronological_pairs_per_family":20,
            "adjacent_comparisons":comparisons,
            "observed_adjacent_sign_agreement_fraction":
                same/comparisons if comparisons else None,
            "family_max_run":max(x["longest_same_sign_run"] for x in families),
            "families_abs_lag1_ge_half":sum(x["risk_flag_abs_lag1_ge_half"] for x in families),
            "families_sign_run_ge_16":sum(x["risk_flag_sign_run_ge_16"] for x in families),
            "families_half_shift_ge_log_1_2":
                sum(x["risk_flag_median_half_shift_ge_log_1_2"] for x in families),
            "censored_pairs":sum(x["censored_pairs"] for x in families),
            "mean_observed_minus_random_order_agreement":
                statistics.fmean(
                    x["adjacent_same_signs"]/x["adjacent_comparable"]-
                    x["conditional_random_order_same_probability"]
                    for x in families
                    if x["adjacent_comparable"] and
                    x["conditional_random_order_same_probability"] is not None),
            "families_raw":families
        })
    require(len(revisions)==1,"mixed original V3-01e source revisions")
    return {
      "schema":1,"protocol":"phaseshift.v3-01f.temporal-replay.1",
      "source_run_id":45038398487,"source_commit":revisions.pop(),
      "suite":"read_only_development_null",
      "source_manifest_sha256":digest,"family_count":256,"paired_measurements":5120,
      "strata":results,"sign_independence_certified":False,
      "actual_wall_time_null_certified":False,"fwer_certified":False,
      "sota_claim":False,
      "interpretation":[
       "Order is family-local SQLite pair_number, not global wall clock",
       "Lag-1, sign agreement, run length and half-drift are descriptive, not calibrated p-values",
       "The random-order baseline is conditional on the observed within-family signs",
       "Repetitions on a shared runner may be dependent; no population CI asserted",
       "No evaluation seeds were opened and no new worker trials were run"
      ]
    }


def main():
    p=argparse.ArgumentParser()
    p.add_argument("--artifacts",required=True,type=Path)
    p.add_argument("--out",required=True,type=Path)
    p.add_argument("--verify-only",action="store_true")
    args=p.parse_args()
    roots={}
    for child in args.artifacts.iterdir():
        if child.is_dir() and (child/"frozen_plan.json").is_file():
            plan=read(child/"frozen_plan.json")
            label=plan["scenario"]
            require(label not in roots,"duplicate null source")
            roots[label]=child
    report=audit(roots)
    if args.verify_only:
        require(read(args.out)==report,"temporal report not independently reproducible")
    else:
        with args.out.open("x") as output:
            json.dump(report,output,indent=2,sort_keys=True,allow_nan=False)
            output.write("\n")
    print(json.dumps({
       "status":"PASS","families":report["family_count"],
       "pairs":report["paired_measurements"],
       "flagged_run_families":sum(s["families_sign_run_ge_16"] for s in report["strata"]),
       "sign_independence_certified":False
    },sort_keys=True),flush=True)


if __name__=="__main__":
    main()
