#!/usr/bin/env python3
import copy
import hashlib
import json
import pathlib
import tempfile

from summarize_dual_cap import analyze

with tempfile.TemporaryDirectory() as tmp:
    root=pathlib.Path(tmp)
    plan={"schema":1,"fixtures":["single_cliff","constant_null"],
          "policies":["adaptive_partition","uniform_v2"],"seeds":list(range(20)),
          "cpu_ceiling_ns":1000000,"wall_ceiling_ns":1000000}
    plan["sha256"]=hashlib.sha256(json.dumps(plan,sort_keys=True).encode()).hexdigest()
    (root/"frozen_plan.json").write_text(json.dumps(plan))
    rows=[]
    for fixture in plan["fixtures"]:
        for policy in plan["policies"]:
            for seed in plan["seeds"]:
                rows.append({
                  "fixture":fixture,"policy":policy,"seed":seed,
                  "observed_total_wall_ns":40000 if policy=="uniform_v2" else 50000,
                  "observed_total_cpu_ns":10000,
                  "observed_parent_cpu_ns":2000,
                  "observed_waited_child_cpu_ns":8000,
                  "completed_cases":4,"valid_samples":4,"invalid_samples":0,
                  "cpu_overshoot_ns":0,"wall_overshoot_ns":0,
                  "within_both_ceilings":True,"had_failure":False,
                  "eligible_confirmed_under_budget":0,
                  "verified_sota_claim":False,
                })
    (root/"results.json").write_text(json.dumps({
        "plan_sha256":plan["sha256"],"sota_claim":False,"rows":rows}))
    result=analyze(root)
    assert not result["sota_claim"]
    assert len(result["policy_rows"])==4
    assert len(result["paired_diagnostics"])==2
    assert all(p["median_wall_budget_utilization"] in (0.04,0.05)
        and p["median_cpu_budget_utilization"]==0.01
        and p["localization_eligible_seeds"]==0
        and p["median_confirmed_correct_boundary_width"] is None
        for p in result["policy_rows"])
    assert all(p["localization_eligible_seed_pairs"]==0 and
        p["median_baseline_minus_policy_interval_width"] is None
        for p in result["paired_diagnostics"])
    assert all(r["eligible_seed_pairs"]==20 and
         r["median_baseline_minus_policy_wall_ns"]==10000 and
         r["exploratory_bootstrap95_low_ns"]==10000
        for r in result["paired_diagnostics"])
    dup=copy.deepcopy(rows);dup.append(rows[0])
    (root/"results.json").write_text(json.dumps({
        "plan_sha256":plan["sha256"],"sota_claim":False,"rows":dup}))
    try:analyze(root)
    except AssertionError:pass
    else:raise AssertionError("duplicate/fabricated seed row accepted")
print("PASS dual cap scorecard independent ledger and reproducible exploratory estimates")
