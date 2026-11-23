#!/usr/bin/env python3
"""Reaudit 40 complete SMAC3-versus-V18 source/worker campaigns."""
from __future__ import annotations
import argparse
import json
from pathlib import Path
import random
from protocol import sha256_file,seal
from run_real_smac import manifest,verify,read,write,require,POLICIES

def aggregate(directory):
    p,_=manifest()
    expected={s["id"] for s in p["scenarios"]}
    require(directory.is_dir() and
            {f.name for f in directory.iterdir() if f.is_dir()}==expected,
            "missing or extra SMAC3 scenario shards")
    all_trials={}
    revisions=set()
    evidence={}
    for sc in p["scenarios"]:
        folder=directory/sc["id"]
        trials={}
        for trial in folder.iterdir():
            if not trial.is_dir():continue
            require(trial.name.startswith("seed_") and
                    (trial/"plan.json").is_file(),
                    "unidentified SMAC3 experiment directory")
            result=verify(trial)
            require(result==read(trial/"scorecard.json") and
                    result["scenario"]==sc["id"] and
                    result["seed"] not in trials and
                    result["sota_claim"] is False and
                    result["sealed_evaluation_reused"] is False,
                    "independent real-worker SQLite verification failed")
            trials[result["seed"]]=result
            revisions.add((result["source_commit"],result["binary_sha256"]))
            evidence[sc["id"]+"/"+str(result["seed"])]=sha256_file(trial/"scorecard.json")
        require(set(trials)==set(p["seeds"]),
                "SMAC3/V18 experiment missing predeclared campaigns")
        all_trials[sc["id"]]=trials
    require(len(revisions)==1,"mixed SMAC3 benchmark binaries/source SHA")
    summaries=[]
    for sc in p["scenarios"]:
        named={}
        for policy in POLICIES:
            samples=[all_trials[sc["id"]][seed]["policies"][policy] for seed in p["seeds"]]
            named[policy]={
                "campaign_count":20,
                "query_count":sum(x["queries"] for x in samples),
                "distinct_edges_queried":sum(x["unique_edges"] for x in samples),
                "nominated_candidates":sum(x["candidates"] for x in samples),
                "fresh_nominal_sign_rejections":sum(x["nominal_sign_rejections"] for x in samples),
                "campaigns_with_rejection":sum(x["nominal_sign_rejections"]>0 for x in samples),
                "censored_candidates":sum(x["censored_candidates"] for x in samples),
                "discovery_cpu_ns":sum(x["discovery_cpu_ns"] for x in samples),
                "discovery_wall_ns":sum(x["discovery_wall_ns"] for x in samples)
            }
        summaries.append({"scenario":sc["id"],"policies":named})
    diffs=[]
    for seed in p["seeds"]:
        diffs.append(sum(all_trials[s["id"]][seed]["policies"]["acquisition-v3-coverage"]["nominal_sign_rejections"]-
                         all_trials[s["id"]][seed]["policies"]["smac3_hpo"]["nominal_sign_rejections"]
                         for s in p["scenarios"]))
    rng=random.Random(int(seal([p["protocol"],"seed_cluster_bootstrap_v1"]),16))
    boots=sorted(sum(diffs[rng.randrange(20)] for _ in range(20))/20 for _ in range(2000))
    rev,binsha=next(iter(revisions))
    return {
      "schema":1,"protocol":p["protocol"],"suite":"development",
      "source_commit":rev,"binary_sha256":binsha,
      "scenarios":summaries,
      "expected_complete_campaigns":80,"observed_complete_campaigns":80,
      "total_native_discovery_queries":960,
      "pinned_smac_version":p["smac_version"],
      "paired_v18_minus_smac_nominal_sign_rejections_per_seed_cluster":{
          "mean":sum(diffs)/20,"exploratory_cluster_percentile95":[boots[50],boots[1949]],
          "positive_seed_clusters":sum(v>0 for v in diffs),
          "negative_seed_clusters":sum(v<0 for v in diffs),
          "ties":sum(v==0 for v in diffs),
          "statistical_superiority_established":False},
      "trial_scorecard_sha256":evidence,
      "full_system_cost_matched":False,
      "independent_real_boundary_truth_available":False,
      "sign_test_independence_certified":False,
      "external_sota_claim":False,
      "sealed_evaluation_reused":False
    }

def main():
    ap=argparse.ArgumentParser()
    ap.add_argument("--artifacts",required=True,type=Path)
    ap.add_argument("--out",required=True,type=Path)
    ap.add_argument("--verify-only",action="store_true")
    a=ap.parse_args()
    final=aggregate(a.artifacts)
    if a.verify_only:require(read(a.out)==final,"SMAC3 aggregate content drift")
    else:write(a.out,final)
    print(json.dumps({"status":"PASS","campaigns":80,"queries":960,
            "summaries":final["scenarios"],"sota_claim":False},sort_keys=True),flush=True)
if __name__=="__main__":
    main()
