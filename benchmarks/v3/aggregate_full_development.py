#!/usr/bin/env python3
"""V3-01c full development inventory and independent cross-shard audit.

All six fragments must be from *one* commit and a complete, non-diagnostic
20-seed development design. Recompute every per-trial row from read-only
SQLite and raw logs. Never use the sealed evaluation suite for this campaign.
"""
from __future__ import annotations

import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path

from multidim_truth import load, order, truth
from protocol import canonical, seal, sha256_file
from run_multidim import read, verify, summarize, paired_bootstrap

def require(ok: bool, reason: str) -> None:
    if not ok:
        raise ValueError(reason)

def audit_inventory(manifest, entries):
    """Pure falsification gate; entries=(fixture, source_commit, plan, rows)."""
    fixtures={f["id"]:f for f in manifest["fixtures"]}
    expected=set(fixtures)
    require(set(entries)==expected,"missing, extra or duplicated fixture shards")
    commits=set()
    all_rows=[]
    raw_fingerprint=set()
    for fixture in fixtures:
        commit,plan,rows=entries[fixture]
        require(isinstance(commit,str) and len(commit)==40 and
                all(ch in "0123456789abcdef" for ch in commit),
                "invalid source revision")
        commits.add(commit)
        expected_order=order(manifest,diagnostic=False,shard=fixture)
        require(plan.get("suite")=="development" and
                plan.get("diagnostic") is False and
                plan.get("shard")==fixture and
                plan.get("eligible_for_claim") is False and
                plan.get("sota_claim") is False and
                plan.get("run_order")==expected_order,
                "partial, changed, or mislabelled research inventory")
        require(len(rows)==len(expected_order)==40,"incomplete fixture trials")
        keys=[(r["fixture"],r["policy"],r["seed"]) for r in rows]
        require(keys==[tuple(item) for item in expected_order],
                "out-of-order, missing, or duplicate policy/seed")
        require(all(r["raw_trial"]["status"] in ("ok","failed") for r in rows),
                "unrecognised execution status")
        for item in rows:
            require(item["measurement_endpoint"]==
                    "unconfirmed_matched_timing_nomination" and
                    item["analytic_truth_scope"]==
                    "discrete_work_unit_jump_not_timing_truth",
                    "false endpoint promotion")
            fp=(item["fixture"],item["policy"],item["seed"])
            require(fp not in raw_fingerprint,"duplicated scored experiment")
            raw_fingerprint.add(fp)
        all_rows.extend(rows)
    require(len(commits)==1,"mixed source revisions across shards")
    require(len(all_rows)==len(fixtures)*len(manifest["seeds"])*len(manifest["policies"])==240,
            "development coverage is not 240 independent policy trials")
    return all_rows,commits.pop()

def seed_cluster_comparison(manifest, rows):
    """Seed is resampling unit; six same-seed fixtures are NOT independent."""
    labels=manifest["policies"]
    positives={f["id"] for f in manifest["fixtures"] if truth(f)}
    negatives={f["id"] for f in manifest["fixtures"] if not truth(f)}
    require(positives and negatives,"positive and null controls required")
    lookup={(r["fixture"],r["policy"],r["seed"]):r for r in rows}
    left=[];right=[]
    for seed in manifest["seeds"]:
        samples=[]
        for policy in labels:
            trials=[lookup[(f["id"],policy,seed)] for f in manifest["fixtures"]]
            positive=[t for t in trials if t["fixture"] in positives]
            negative=[t for t in trials if t["fixture"] in negatives]
            valid_hits=sum(t["true_hit_under_budget"]>0 for t in positive)
            null_false=sum(t["adjudication"]["false_observations"] for t in negative)
            samples.append({"seed":seed,
                            "positive_hit_fraction":valid_hits/len(positive),
                            "null_false_nominations_per_trial":null_false/len(negative)})
        left.append(samples[0]);right.append(samples[1])
    return {
        "unit":"seed_cluster_across_fixtures",
        "n_seed_clusters":len(manifest["seeds"]),
        "positive_fixture_count":len(positives),
        "negative_control_count":len(negatives),
        "v15_minus_v14_positive_hit_fraction":
            paired_bootstrap(left,right,lambda x:x["positive_hit_fraction"],
                             ["V3-01c","positive-fraction"]),
        "v15_minus_v14_null_false_nominations_per_trial":
            paired_bootstrap(left,right,lambda x:x["null_false_nominations_per_trial"],
                             ["V3-01c","negative-nominations"]),
        "inferential_claim":False}

def assemble(manifest, entries, artifacts):
    rows,commit=audit_inventory(manifest,entries)
    require(set(artifacts)==set(entries),"missing per-shard provenance")
    positions=Counter(r["fixture"] for r in rows)
    result=[]
    for f in manifest["fixtures"]:
        identifier=f["id"]
        selected=[r for r in rows if r["fixture"]==identifier]
        bypolicy={}
        for policy in manifest["policies"]:
            trials=[r for r in selected if r["policy"]==policy]
            bypolicy[policy]={
                "n":len(trials),
                "completed":sum(r["raw_trial"]["status"]=="ok" for r in trials),
                "failed":sum(r["raw_trial"]["status"]!="ok" for r in trials),
                "over_budget":sum(not r["raw_trial"]["within_consumed_ceilings"]
                                  for r in trials),
                "hit_runs":sum(r["true_hit_under_budget"]>0 for r in trials),
                "true_edges_hit_total":sum(r["true_hit_under_budget"] for r in trials),
                "false_nomination_observations":sum(
                    r["adjudication"]["false_observations"] for r in trials),
                "censored_pairs":sum(r["raw_trial"]["censored_pairs"] for r in trials),
                "observed_cpu_ns_total":sum(
                    r["raw_trial"]["observed_total_cpu_ns"] for r in trials),
                "observed_wall_ns_total":sum(
                    r["raw_trial"]["observed_total_wall_ns"] for r in trials)}
        result.append({"fixture":identifier,"truth_edges":len(truth(f)),
                       "eligible_for_sota":False,"policies":bypolicy})
    report={
        "schema":1,
        "protocol":"phaseshift.v3-01c.full-development.1",
        "source_commit":commit,
        "suite":"development",
        "diagnostic":False,
        "eligible_for_claim":False,
        "sota_claim":False,
        "endpoint":"unconfirmed_timing_nomination_vs_analytic_work_unit_jump",
        "comparison_scope":"matched_policies_within_each_fixture_on_one_runner",
        "expected_trials":240,
        "observed_trials":len(rows),
        "manifest_sha256":sha256_file(Path(__file__).resolve().parent/
                                     "multidim_development.json"),
        "seed_inventory":manifest["seeds"],
        "shard_provenance":artifacts,
        "scenario_results":result,
        "within_fixture_paired_comparison":summarize(rows,manifest),
        "seed_clustered_comparison":seed_cluster_comparison(manifest,rows),
        "limitations":[
            "Development only; predeclared evaluation seeds remain sealed",
            "Synthetic analytic work units do not establish real elapsed-time cliff truth",
            "Timing nominations lack independent fresh-seed confirmation",
            "Equal matched-pair count does not imply equal consumed CPU",
            "Each scenario uses its own CI runner; compare policies within scenario, not absolute CPU across hosts",
            "Clustered bootstrap is exploratory; not a pre-registered familywise error bound",
            "No equal-endpoint external competitor and no SOTA claim"] }
    return report

def main():
    parser=argparse.ArgumentParser()
    parser.add_argument("--artifacts",type=Path,required=True)
    parser.add_argument("--out",type=Path,required=True)
    parser.add_argument("--verify-only",action="store_true")
    args=parser.parse_args()
    manifest,mh=load("development")
    roots={}
    for child in args.artifacts.iterdir():
        if not child.is_dir():
            continue
        if (child/"frozen_plan.json").is_file():
            plan=read(child/"frozen_plan.json")
            fixture=plan["shard"]
            require(fixture not in roots,"duplicate scenario artifact")
            roots[fixture]=child
    require(len(roots)==len(manifest["fixtures"]),"missing full-run scenario artifacts")
    entries={}
    provenance={}
    for f in manifest["fixtures"]:
        fixture=f["id"]
        require(fixture in roots,"missing fixture "+fixture)
        root=roots[fixture]
        report=verify(root) # full independent SQLite + immutable source replay
        require(read(root/"scorecard.json")==report,"scorecard raw-data drift")
        plan=read(root/"frozen_plan.json")
        require(plan["manifest_sha256"]==mh and plan["manifest"]==manifest,
                "fixture silently changed frozen manifest")
        entries[fixture]=(report["source_commit"],plan,
                          read(root/"results.json")["rows"])
        provenance[fixture]={
            "frozen_plan_sha256":sha256_file(root/"frozen_plan.json"),
            "results_sha256":sha256_file(root/"results.json"),
            "scorecard_sha256":sha256_file(root/"scorecard.json"),
            "binary_sha256":plan["binary_sha256"],
            "source_commit":report["source_commit"]}
    score=assemble(manifest,entries,provenance)
    if args.verify_only:
        require(read(args.out)==score,"aggregate report differs from rerun")
    else:
        with args.out.open("x") as output:
            json.dump(score,output,sort_keys=True,indent=2,allow_nan=False)
            output.write("\n")
    print(json.dumps({"status":"PASS","trials":score["observed_trials"],
                      "fixtures":len(score["scenario_results"]),
                      "source_commit":score["source_commit"],
                      "development_only":True,"sota_claim":False},
                     sort_keys=True),flush=True)

if __name__=="__main__":
    main()
