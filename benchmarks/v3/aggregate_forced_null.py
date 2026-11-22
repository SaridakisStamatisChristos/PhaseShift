#!/usr/bin/env python3
"""Independent V3-01e complete forced-null campaign audit and calibration report.

Never infer a guaranteed population false-positive rate from correlated
CI-runner repetitions. Exact binomial bounds here are *conditional* on
independently drawn exchangeable family-level Bernoulli trials.
"""
from __future__ import annotations

import argparse
import json
from pathlib import Path

from confirmation_protocol import require,read
from protocol import sha256_file
from run_forced_null_calibration import manifest,reconstruct,upper_bound_if_independent


def aggregate(roots):
    m, mh = manifest()
    scenarios={s["id"] for s in m["scenario"]}
    require(set(roots)==scenarios,"missing or duplicate null scenario shard")
    per_stratum=[]
    source_sha=set()
    bystratum=[]
    provenance={}
    for s in m["scenario"]:
        id=s["id"]
        folder=roots[id]
        verified=reconstruct(folder)
        require(read(folder/"scorecard.json")==verified,
                "per-scenario scorecard not reproduced from SQLite")
        frozen=read(folder/"frozen_plan.json")
        require(frozen["manifest_sha256"]==mh and
                frozen["scenario"]==id and
                frozen["diagnostic"] is False and
                frozen["seed_inventory"]==s["seeds"] and
                verified["diagnostic"] is False and
                verified["complete_families"]==m["families_per_stratum"] and
                verified["observed_holdout_pairs"]==
                64*m["holdout_pairs_per_family"] and
                not verified["population_fwer_certified"] and
                not verified["population_independence_established"] and
                not verified["sota_claim"],
                "non-precommitted or unverified null inventory")
        source_sha.add(frozen["source_commit"])
        labels=verified["results"]
        require(len(labels)==64 and
                [v["seed"] for v in labels]==s["seeds"] and
                sum(v["predeclared_direction"]==1 for v in labels)==32 and
                sum(v["predeclared_direction"]==-1 for v in labels)==32 and
                all(v["null_work_proven_equal_per_pair"] and v["complete"]
                    for v in labels),
                "selected/censored/short-circuited/unequal-work negative control")
        rejections=verified["nominal_model_rejections"]
        upper=upper_bound_if_independent(rejections,64)
        require(verified["exact_95_one_sided_upper_if_independent_families"]==upper,
                "incorrect exact one-sided descriptive interval")
        out={
          "stratum":id,
          "algorithm":s["algorithm"],
          "contention":s["contention"],
          "predeclared_exogenous_family_count":64,
          "zero_discovery_selection_omissions":True,
          "holdout_pairs":verified["observed_holdout_pairs"],
          "nominal_false_rejections_relative_to_equal_work":rejections,
          "observed_rejection_fraction":rejections/64,
          "conditional_exact_one_sided_95_upper_if_independent":upper,
          "censored_holdout_pairs":sum(x["censored_holdout_pairs"] for x in labels),
          "balanced_direction_counts":[32,32],
          "independence_established":False,
          "certified_error_control":False
        }
        bystratum.append(out)
        provenance[id]={
            "plan_sha256":sha256_file(folder/"frozen_plan.json"),
            "scorecard_sha256":sha256_file(folder/"scorecard.json"),
            "source_commit":frozen["source_commit"],
            "binary_sha256":frozen["binary_sha256"]}
    require(len(source_sha)==1,"mixed source revisions in null campaign")
    return {
        "schema":1,"protocol":m["protocol"],"suite":"development_forced_null",
        "source_commit":source_sha.pop(),"manifest_sha256":mh,
        "planned_strata":4,"observed_strata":4,
        "planned_families":256,"observed_complete_families":256,
        "planned_holdout_pairs":5120,"observed_holdout_pairs":5120,
        "nominal_model_rejections":sum(x["nominal_false_rejections_relative_to_equal_work"]
                                        for x in bystratum),
        "strata":bystratum,"provenance":provenance,
        "all_exogenous_null_families_exposed":True,
        "selection_bias_eliminated_for_this_experiment":True,
        "equal_work_is_not_a_proven_wall_time_null":True,
        "family_independence_empirically_validated":False,
        "population_fwer_certified":False,
        "eligible_for_sota":False,"sota_claim":False,
        "interpretation":[
          "All predeclared equal-work negative controls are tested, even absent a discovery signal",
          "The one-sided exact sign test still depends on unverified sign-tail assumptions",
          "64 families per stratum may share CPU resources and environmental drift",
          "Exact Clopper-Pearson upper bounds are descriptive conditional limits, not guaranteed coverage",
          "One frozen member per family means Holm correction is trivial in this calibration",
          "Original null worker is correct in deterministic loop-work units, not a proof of identical wall-time distributions",
          "Separate selected-family confirmation and population null calibration estimands",
          "No evaluation cohort used and no benchmark/SOTA winner selected"]
    }


def main():
    parser=argparse.ArgumentParser()
    parser.add_argument("--artifacts",required=True,type=Path)
    parser.add_argument("--out",required=True,type=Path)
    parser.add_argument("--verify-only",action="store_true")
    args=parser.parse_args()
    roots={}
    for path in args.artifacts.iterdir():
        if path.is_dir() and (path/"frozen_plan.json").is_file():
            scenario=read(path/"frozen_plan.json")["scenario"]
            require(scenario not in roots,"duplicated frozen null scenario artifact")
            roots[scenario]=path
    result=aggregate(roots)
    if args.verify_only:
        require(read(args.out)==result,"mutated forced-null aggregate report")
    else:
        with args.out.open("x") as f:
            json.dump(result,f,sort_keys=True,indent=2,allow_nan=False)
            f.write("\n")
    print(json.dumps({"status":"PASS","families":result["observed_complete_families"],
          "holdout_pairs":result["observed_holdout_pairs"],
          "nominal_equal_work_null_rejections":result["nominal_model_rejections"],
          "population_fwer_certified":False,"sota_claim":False},sort_keys=True),
          flush=True)


if __name__=="__main__":
    main()
