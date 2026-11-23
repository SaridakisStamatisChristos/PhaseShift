#!/usr/bin/env python3
"""Independent one-time sealed 360-trial three-policy native evaluation aggregate.

Do not treat 120 repeated fixture observations as 120 independent clusters.
Seed is the matched resampling unit across all six fixtures.
Retain nulls, misses, censored, over-budget trials and cost consumption.
"""
from __future__ import annotations
import argparse
import json
from pathlib import Path
import random

from multidim_truth import truth
from protocol import seal,sha256_file
from run_coverage_evaluation import manifest,verification,require,read,POLICIES


def aggregate(roots):
    m,mh=manifest()
    names={f["id"] for f in m["fixtures"]}
    require(set(roots)==names,"incomplete, extra or duplicated fixture shards")
    revisions=set()
    all_rows=[]
    source={}
    for f in m["fixtures"]:
        id=f["id"];root=roots[id]
        result=verification(root)
        require(read(root/"scorecard.json")==result and
                result["source_commit"] and
                result["manifest_sha256"]==mh and
                result["scenario"]==id and
                result["diagnostic"] is False and
                result["expected_trials"]==result["observed_trials"]==60 and
                result["eligible_for_claim"] is False and
                result["sota_claim"] is False,
                "not all 60 native-workload trials independently validated")
        revisions.add(result["source_commit"])
        all_rows.extend(result["results"])
        source[id]={
            "plan_sha256":sha256_file(root/"frozen_plan.json"),
            "scorecard_sha256":sha256_file(root/"scorecard.json"),
            "source_commit":result["source_commit"]}
    require(len(revisions)==1,"mixed source revisions")
    lookup={(r["fixture"],r["seed"],r["policy"]):r for r in all_rows}
    expected={(f["id"],seed,p) for f in m["fixtures"]
             for seed in m["seeds"] for p in POLICIES}
    require(len(all_rows)==len(expected)==360 and set(lookup)==expected,
            "incomplete paired 360-trial matched experiment")
    positive={f["id"] for f in m["fixtures"] if truth(f)}
    negative={f["id"] for f in m["fixtures"] if not truth(f)}
    require(len(positive)==4 and len(negative)==2,
            "positive and negative-control fixture separation changed")
    by_fixture=[]
    for f in m["fixtures"]:
        out={}
        for p in POLICIES:
            rs=[lookup[(f["id"],seed,p)] for seed in m["seeds"]]
            out[p]={
                "trials":20,
                "under_budget_true_hit_trials":sum(r["true_hit_under_budget"]>0 for r in rs),
                "distinct_planted_truth_edges_nominated":sum(r["true_hit_under_budget"] for r in rs),
                "false_nomination_events":sum(r["adjudication"]["false_observations"] for r in rs),
                "replication_queries":sum(r["replication_pairs"] for r in rs),
                "exploration_queries":sum(r["exploratory_pairs"] for r in rs),
                "failures":sum(r["raw_trial"]["status"]!="ok" for r in rs),
                "out_of_consumed_budget":sum(not r["raw_trial"]["within_consumed_ceilings"] for r in rs),
                "total_cpu_ns":sum(r["raw_trial"]["observed_total_cpu_ns"] for r in rs),
                "total_wall_ns":sum(r["raw_trial"]["observed_total_wall_ns"] for r in rs)
            }
        by_fixture.append({"fixture":f["id"],
                           "truth_edges":len(truth(f)),
                           "policies":out})
    cluster=[]
    for seed in m["seeds"]:
        arms={}
        for policy in POLICIES:
            rows=[lookup[(f["id"],seed,policy)] for f in m["fixtures"]]
            pp=[r for r in rows if r["fixture"] in positive]
            nn=[r for r in rows if r["fixture"] in negative]
            arms[policy]={
                "positive_hit_fraction":sum(r["true_hit_under_budget"]>0 for r in pp)/len(pp),
                "null_false_per_trial":sum(r["adjudication"]["false_observations"] for r in nn)/len(nn),
                "replications_per_trial":sum(r["replication_pairs"] for r in rows)/len(rows),
                "observed_total_cpu_ns":sum(r["raw_trial"]["observed_total_cpu_ns"] for r in rows),
            }
        cluster.append({"seed":seed,"arms":arms})

    def compare(new,old,metric):
        diffs=[c["arms"][new][metric]-c["arms"][old][metric] for c in cluster]
        n=len(diffs)
        mean=sum(diffs)/n
        rng=random.Random(int(seal([m["protocol"],metric,new,old,"bootstrap_v1"]),16))
        draws=sorted(sum(diffs[rng.randrange(n)] for _ in range(n))/n for _ in range(2000))
        return {"seed_clusters":n,"mean_new_minus_old":mean,
                "descriptive_seed_cluster_percentile95":
                    [draws[int(.025*len(draws))],draws[int(.975*len(draws))-1]],
                "new_better_seed_clusters":sum(d>0 for d in diffs),
                "new_worse_seed_clusters":sum(d<0 for d in diffs),
                "tied_seed_clusters":sum(d==0 for d in diffs),
                "statistical_superiority_claim":False}
    comparative={}
    for old in POLICIES[:2]:
        comparative["coverage_vs_"+old]={
            metric:compare("acquisition-v3-coverage",old,metric)
            for metric in ("positive_hit_fraction","null_false_per_trial",
                           "replications_per_trial","observed_total_cpu_ns")}

    # One-time sealed endpoint. This is descriptive, NOT an inferential
    # declaration or license to tune using these previously withheld seeds.
    total_positive={
        p:sum(f["policies"][p]["under_budget_true_hit_trials"]
              for f in by_fixture if f["fixture"] in positive)
        for p in POLICIES
    }
    total_negative_false={
        p:sum(f["policies"][p]["false_nomination_events"]
              for f in by_fixture if f["fixture"] in negative)
        for p in POLICIES
    }
    total_all_false={
        p:sum(f["policies"][p]["false_nomination_events"]
              for f in by_fixture)
        for p in POLICIES
    }
    hit_advantage=all(
        total_positive["acquisition-v3-coverage"]>total_positive[p]
        for p in POLICIES[:2])
    guardrail=all(
        total_negative_false["acquisition-v3-coverage"]<=total_negative_false[p]
        for p in POLICIES[:2])
    decision={
        "evaluation_datasets_exposed":True,
        "primary_descriptive_v18_hit_advantage_over_both":hit_advantage,
        "negative_control_false_nomination_no_worse_than_both":guardrail,
        "total_positive_hit_trials":total_positive,
        "negative_control_false_nomination_events":total_negative_false,
        "all_scenario_false_nomination_events":total_all_false,
        "primary_endpoint":"at_least_one_correct_timing_nomination_on_four_planted_work_unit_positive_fixtures",
        "point_estimate_only":True,
        "superiority_established":False,
        "external_sota_established":False,
    }
    return {
        "schema":1,"protocol":m["protocol"],"suite":"evaluation",
        "source_commit":revisions.pop(),
        "source_manifest_sha256":mh,
        "expected_trials":360,"observed_trials":360,
        "seed_cluster_count":20,"positive_fixtures":sorted(positive),
        "null_fixtures":sorted(negative),
        "scenario_results":by_fixture,
        "seed_cluster_paired_differences":comparative,
        "raw_cluster_metrics":cluster,"source_provenance":source,
        "eligible_for_claim":False,"sota_claim":False,"evaluation_opened":True,
        "decision":decision,
        "limitations":[
            "Only synthetic work-unit edges are labelled ground truth",
            "Observed CLI CPU is not exact pure-worker CPU nor a standardized hardware benchmark",
            "Equal query counts and same CI fixture do not imply exactly equal consumed CPU",
            "Twenty seed clusters support only exploratory paired bootstrap intervals",
            "One-time final holdout has been opened; no new tuning on its observations",
            "No fresh-seed confirmation of these timed nominations",
            "This held-out comparison tests only the three predeclared internal policies",
            "No external competitor, fresh-seed confirmation or general SOTA certification"]
    }


def main():
    p=argparse.ArgumentParser()
    p.add_argument("--artifacts",required=True,type=Path)
    p.add_argument("--out",required=True,type=Path)
    p.add_argument("--verify-only",action="store_true")
    a=p.parse_args()
    roots={}
    for folder in a.artifacts.iterdir():
        if folder.is_dir() and (folder/"frozen_plan.json").is_file():
            id=read(folder/"frozen_plan.json")["shard"]
            require(id not in roots,"duplicate source evidence shard")
            roots[id]=folder
    report=aggregate(roots)
    if a.verify_only:
        require(read(a.out)==report,"independent competitive aggregate scorecard drift")
    else:
        with a.out.open("x") as out:
            json.dump(report,out,indent=2,sort_keys=True,allow_nan=False)
            out.write("\n")
    print(json.dumps({
        "status":"PASS","trials":report["observed_trials"],
        "source_revision":report["source_commit"],"matched_seed_clusters":20,
        "scenarios":[{
            "fixture":f["fixture"],
            "hit":{k:v["under_budget_true_hit_trials"] for k,v in f["policies"].items()},
            "replication_pairs":{k:v["replication_queries"] for k,v in f["policies"].items()}
        } for f in report["scenario_results"]],
        "sota_claim":False,"decision":report["decision"]
    },sort_keys=True),flush=True)


if __name__=="__main__":
    main()
