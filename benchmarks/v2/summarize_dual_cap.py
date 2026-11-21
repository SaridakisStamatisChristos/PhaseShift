#!/usr/bin/env python3
"""Reproducible, fail-closed descriptive dual-cap cross-policy scorecard.

Exactly matched scenario/seed inventories are required; no missing/lost trials
are silently excluded. No SOTA or familywise test is inferred. Independent
non-Gaussian resampling is reported as exploratory even with 20+ seeds.
"""
import argparse
import csv
import hashlib
import json
import pathlib
import random
import statistics

def percentile(sorted_values, p):
    assert sorted_values
    i = (len(sorted_values)-1)*p
    lo=int(i)
    hi=min(lo+1,len(sorted_values)-1)
    return sorted_values[lo]*(1-(i-lo))+sorted_values[hi]*(i-lo)

def analyze(root):
    plan=json.loads((root/"frozen_plan.json").read_text())
    digest=plan.pop("sha256")
    assert hashlib.sha256(json.dumps(plan,sort_keys=True).encode()).hexdigest()==digest
    result=json.loads((root/"results.json").read_text())
    assert result["plan_sha256"]==digest and not result["sota_claim"]
    rows=result["rows"]
    expected={(fixture,policy,seed) for fixture in plan["fixtures"]
              for policy in plan["policies"] for seed in plan["seeds"]}
    indexed={(r["fixture"],r["policy"],r["seed"]):r for r in rows}
    assert len(indexed)==len(rows)==len(expected) and set(indexed)==expected
    for r in rows:
        assert r["verified_sota_claim"] is False and r["observed_total_wall_ns"]>=0
        assert r["observed_total_cpu_ns"]>=0 and r["completed_cases"]>=0
        assert r["observed_total_cpu_ns"]==(
            r["observed_parent_cpu_ns"]+r["observed_waited_child_cpu_ns"])
        assert r["cpu_overshoot_ns"]==max(
            0,r["observed_total_cpu_ns"]-plan["cpu_ceiling_ns"])
        assert r["wall_overshoot_ns"]==max(
            0,r["observed_total_wall_ns"]-plan["wall_ceiling_ns"])
    per=[]
    for fixture in plan["fixtures"]:
        for policy in plan["policies"]:
            trial=[indexed[fixture,policy,s] for s in plan["seeds"]]
            cpu=[t["observed_total_cpu_ns"] for t in trial]
            wall=[t["observed_total_wall_ns"] for t in trial]
            per.append({
                "fixture":fixture,"policy":policy,"seeds":len(trial),
                "failed_commands":sum(t["had_failure"] for t in trial),
                "budget_overshoots":sum(not t["within_both_ceilings"] for t in trial),
                "median_observed_cpu_ns":statistics.median(cpu),
                "median_observed_wall_ns":statistics.median(wall),
                "completed_cases":sum(t["completed_cases"] for t in trial),
                "valid_samples":sum(t["valid_samples"] for t in trial),
                "invalid_samples":sum(t["invalid_samples"] for t in trial),
                "eligible_confirmations":sum(t["eligible_confirmed_under_budget"] for t in trial),
                "correct_synthetic_thresholds":sum(
                    t.get("synthetic_known_threshold_correct",0) and
                    t["eligible_confirmed_under_budget"] for t in trial),
                "null_false_positives":sum(
                    t.get("synthetic_false_positive_confirmed",0) for t in trial),
            })
    # Report a paired distribution only where both policies cover same
    # fixture and seed. It is not a claim of calibrated treatment effect.
    paired=[]
    baseline="adaptive_partition"
    if baseline in plan["policies"]:
        for fixture in plan["fixtures"]:
            for policy in plan["policies"]:
                if policy==baseline:continue
                differences=[]
                for seed in plan["seeds"]:
                    a=indexed[fixture,baseline,seed]
                    b=indexed[fixture,policy,seed]
                    if a["had_failure"] or b["had_failure"] or not (
                            a["within_both_ceilings"] and b["within_both_ceilings"]):
                        continue
                    differences.append(
                        a["observed_total_wall_ns"]-b["observed_total_wall_ns"])
                point=statistics.median(differences) if differences else None
                ci_low=ci_high=None
                if len(differences)>=20:
                    rng=random.Random(int.from_bytes(
                        hashlib.sha256(f"{fixture}/{policy}/{digest}".encode()).digest()[:8],
                        "big"))
                    n=len(differences)
                    estimates=sorted(statistics.median(
                        [differences[rng.randrange(n)] for _ in range(n)])
                        for _ in range(1000))
                    ci_low=percentile(estimates,0.025)
                    ci_high=percentile(estimates,0.975)
                paired.append({
                    "fixture":fixture,"baseline":baseline,"policy":policy,
                    "eligible_seed_pairs":len(differences),
                    "median_baseline_minus_policy_wall_ns":point,
                    "exploratory_bootstrap95_low_ns":ci_low,
                    "exploratory_bootstrap95_high_ns":ci_high,
                    "confirmatory_claim":False,
                })
    return {
        "source_plan_sha256":digest,
        "predeclared_seed_count":len(plan["seeds"]),
        "policy_rows":per,"paired_diagnostics":paired,
        "sota_claim":False,
        "confirmatory_claim":False,
        "limitations":[
            "Equal caps do not imply equal consumed resources or case semantics",
            "One complete case can overshoot either cap; logged, never a win",
            "CPU process descendants not reaped by the measured CLI are unavailable",
            "No adjustment for optional discovery or candidate-selection multiplicity",
            "Exploratory percentile bootstrap is not a calibrated PhaseShift result",
            "Synthetic calibration fixtures are not independent real-world findings",
        ],
    }

def main():
    p=argparse.ArgumentParser()
    p.add_argument("--root",required=True,type=pathlib.Path)
    p.add_argument("--output",required=True,type=pathlib.Path)
    a=p.parse_args()
    if a.output.exists():p.error("immutable scorecard output already exists")
    score=analyze(a.root)
    a.output.write_text(json.dumps(score,sort_keys=True,indent=2)+"\n")
    print(json.dumps(score,sort_keys=True))

if __name__=="__main__":
    main()
