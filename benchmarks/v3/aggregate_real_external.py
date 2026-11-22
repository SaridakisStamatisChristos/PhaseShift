#!/usr/bin/env python3
"""Real C++ sorting/top-k V18 vs genuine Optuna TPE/Random: independent audit.

Aggregate ONLY genuine executed and replayed 20-seed development campaigns.
Selection and fresh 20-pair per-nomination evidence is audited from SQLite.
No labels of true real-algorithm boundaries exist: nominal sign-tail findings
cannot establish statistical validity without seed-independence calibration.
"""
import argparse
import json
from pathlib import Path
import random

from protocol import seal,sha256_file
from run_real_external import manifest,verify,read,write,require,POLICIES


def aggregate(directory):
    plan,by=manifest()
    names={s["id"] for s in plan["scenarios"]}
    shards={}
    for folder in directory.iterdir():
        if folder.is_dir() and folder.name in names:
            require(folder.name not in shards,"duplicate real scenario artifact shard")
            shards[folder.name]=folder
    require(set(shards)==names,"missing real algorithm scenario artifact shard")
    all_rows={}
    revisions=set()
    provenance={}
    for scenario in plan["scenarios"]:
        name=scenario["id"]
        folder=shards[name]
        seeds={}
        for trial in folder.iterdir():
            if not trial.is_dir():continue
            require((trial/"plan.json").is_file(),"orphan incomplete source trial")
            score=verify(trial)
            require(read(trial/"scorecard.json")==score and
                    score["scenario"]==name and
                    score["suite"]=="development" and not score["sota_claim"],
                    "independent real worker/confirmation scorecard replay failed")
            require(score["seed"] not in seeds,"duplicate seed")
            seeds[score["seed"]]=score
            revisions.add(score["source_revision"])
            provenance[name+"/"+str(score["seed"])]={
                "plan_sha256":sha256_file(trial/"plan.json"),
                "scorecard_sha256":sha256_file(trial/"scorecard.json")}
        require(set(seeds)==set(plan["seeds"]),"missing or extra predeclared 20-seed real campaigns")
        all_rows[name]=seeds
    require(len(revisions)==1,"mixed real-worker source revisions")
    out=[]
    for scenario in plan["scenarios"]:
        name=scenario["id"]
        policies={}
        for policy in POLICIES:
            p=[all_rows[name][seed]["policies"][policy] for seed in plan["seeds"]]
            policies[policy]={
                "trial_count":len(p),
                "proposed_pairs":sum(plan["pair_budget"] for _ in p),
                "distinct_edges_visited":sum(row["discovery_unique_edges"] for row in p),
                "discovery_candidate_nominations":sum(row["candidate_count"] for row in p),
                "fresh_seed_model_nominal_rejections":sum(row["nominal_model_rejections"] for row in p),
                "censored_confirmation_candidates":sum(row["censored_candidate_count"] for row in p),
                "campaigns_with_any_nominal_rejection":sum(row["nominal_model_rejections"]>0 for row in p),
                "discovery_parent_and_children_cpu_ns":sum(row["discovery_cpu_ns"] for row in p),
                "discovery_wall_ns":sum(row["discovery_wall_ns"] for row in p),
                "sampling_model_independence_certified":False
            }
        out.append({"scenario":name,"policies":policies})
    pairs={}
    for external in POLICIES[1:]:
        groups=[]
        for seed in plan["seeds"]:
            a=sum(all_rows[sc["id"]][seed]["policies"]["acquisition-v3-coverage"]["nominal_model_rejections"]
                  for sc in plan["scenarios"])
            b=sum(all_rows[sc["id"]][seed]["policies"][external]["nominal_model_rejections"]
                  for sc in plan["scenarios"])
            groups.append(a-b)
        rng=random.Random(int(seal([plan["protocol"],external,"paired_cluster_bootstrap"]),16))
        distribution=sorted(sum(groups[rng.randrange(len(groups))] for _ in groups)/len(groups)
                            for _ in range(2000))
        pairs["v18_minus_"+external]={
            "mean_nominal_rejections_per_matched_seed_cluster":sum(groups)/len(groups),
            "exploratory_percentile_95":[distribution[50],distribution[1949]],
            "positive_seed_clusters":sum(g>0 for g in groups),
            "negative_seed_clusters":sum(g<0 for g in groups),
            "ties":sum(g==0 for g in groups),
            "significance_claim":False
        }
    return {
        "schema":1,"suite":"development",
        "protocol":plan["protocol"],"source_commit":revisions.pop(),
        "pinned_optuna_version":plan["optuna_version"],
        "scenarios":out,"total_completed_campaigns":len(plan["seeds"])*len(plan["scenarios"])*len(POLICIES),
        "total_native_discovery_pairs":len(plan["seeds"])*len(plan["scenarios"])*len(POLICIES)*plan["pair_budget"],
        "seed_clusters":len(plan["seeds"]),
        "paired_exploratory_results":pairs,"raw_trial_plan_and_scorecard_hashes":provenance,
        "objective":"absolute_matched_log_speed_ratio_change_between_two_real_cpp_algorithms",
        "external_engine":"genuine_Optuna_4_6_TPESampler_and_RandomSampler",
        "status":"SCOPED_DEVELOPMENT_ONLY",
        "same_native_worker_and_oracle":True,
        "full_system_cost_matched":False,
        "real_target_ground_truth_available":False,
        "valid_fwer_or_independence_certificate":False,
        "sealed_synthetic_evaluation_reused":False,
        "external_sota_claim":False,
        "limitations":[
            "Optuna one CLI invocation per proposed edge versus V18 one CLI per campaign; interpreter and worker startup overhead is real, not matched",
            "Candidate objective is abs matched log ratio DIFFERENCE, an adapted Optuna maximization objective; not original HPO target",
            "Real native sorting and top-k boundaries have no exogenous known-truth labels",
            "Fresh 20-pair sign test assumes independence and model correctness, neither established by this campaign",
            "Fixed 12 query count, not matched consumed CPU or wall; costs shown separately",
            "Old synthetic sealed cohort 7101–7120 not touched or reused for tuning"
        ]
    }


def main():
    parser=argparse.ArgumentParser()
    parser.add_argument("--artifacts",required=True,type=Path)
    parser.add_argument("--out",required=True,type=Path)
    parser.add_argument("--verify-only",action="store_true")
    a=parser.parse_args()
    record=aggregate(a.artifacts)
    if a.verify_only:
        require(read(a.out)==record,"real competitor aggregate scorecard drift")
    else:
        write(a.out,record)
    print(json.dumps({
        "status":"PASS","campaigns":record["total_completed_campaigns"],
        "native_discovery_pairs":record["total_native_discovery_pairs"],
        "scenario_counts":[{
            "scenario":s["scenario"],
            "nominal_model_rejections":{k:p["fresh_seed_model_nominal_rejections"]
                                        for k,p in s["policies"].items()}
        } for s in record["scenarios"]],
        "external_sota_claim":False
    },sort_keys=True),flush=True)


if __name__=="__main__":
    main()
