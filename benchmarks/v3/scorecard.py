#!/usr/bin/env python3
"""Strict independent scorecard from immutable real-CLI artifacts; no SOTA claim."""
from __future__ import annotations

import argparse
import json
import pathlib

from protocol import exact_binomial_interval, verify_results


def score(root):
    plan, rows = verify_results(root)
    manifest = plan["manifest"]
    out = {"schema": 1, "source_commit": plan["source_commit"],
           "manifest_sha256": plan["manifest_sha256"],
           "eligible_for_claim": False, "sota_claim": False,
           "resource_axes": [
               "observed_waited_child_cpu_ns (CLI subtree, not pure worker)",
               "observed_total_cpu_ns (harness + waited CLI subtree)",
               "observed_total_wall_ns (full CLI harness wall)"],
           "per_fixture_policy": []}
    for fixture in manifest["fixtures"]:
        for policy in manifest["policies"]:
            trial = [r for r in rows if r["fixture"] == fixture and r["policy"] == policy]
            assert len(trial) == len(manifest["seeds"])
            n = len(trial)
            truth = fixture == "single_cliff"
            def is_true(r):
                return bool(truth and r["eligible_confirmed_under_budget"] and
                            r["synthetic_known_threshold_correct"])
            def curve(key, ceiling):
                # Every failure/censored/non-detection stays in the denominator.
                horizons = [ceiling * k // 20 for k in range(21)]
                probabilities = [sum(is_true(r) and r[key] <= h for r in trial)/n
                                 for h in horizons]
                auc = sum((probabilities[k-1]+probabilities[k])/40
                          for k in range(1, 21))
                return {"horizon_ns": horizons, "success_probability": probabilities,
                        "normalized_trapezoid_auc": auc}
            positives = sum(is_true(r) for r in trial)
            fp = sum(bool(r["synthetic_false_positive_confirmed"]) for r in trial)
            out["per_fixture_policy"].append({
                "fixture": fixture, "policy": policy, "trials": n,
                "true_confirmations": positives, "false_null_promotions": fp,
                "failed_trials": sum(bool(r["had_failure"]) for r in trial),
                "censored_no_true_confirmation": n-positives,
                "attempted_cases": sum(r["completed_cases"] for r in trial),
                "mean_cap_utilization": {
                    "full_cpu": sum(r["observed_total_cpu_ns"] /
                                    manifest["cpu_budget_ns"] for r in trial)/n,
                    "full_wall": sum(r["observed_total_wall_ns"] /
                                     manifest["wall_budget_ns"] for r in trial)/n},
                "null_false_promotion_exact_95pct_ci":
                    exact_binomial_interval(fp, n) if not truth else None,
                "success_vs_cli_subtree_cpu": curve(
                    "observed_waited_child_cpu_ns", manifest["cpu_budget_ns"]),
                "success_vs_full_system_cpu": curve(
                    "observed_total_cpu_ns", manifest["cpu_budget_ns"]),
                "success_vs_full_wall": curve(
                    "observed_total_wall_ns", manifest["wall_budget_ns"]),
            })
    out["limitations"] = [
        "Only one planted threshold and one constant-null shape; not broad heldout truth",
        "Benchmark v1/v2 generator seed conventions differ; do not infer equality",
        "AUC uses complete-trial confirmation time; no interim confirmed successes",
        "Full cap is not equal spent budget; always report utilization and curves",
        "Independent Bernoulli campaigns are assumed for displayed exact null CI",
        "A zero false-positive count with 20 trials cannot establish <=5% FWER",
        "Waited CLI subtree CPU is not a proven full process-tree or worker-only CPU",
        "No SOTA verdict without an aligned external acquisition competitor",
    ]
    return out


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--root", required=True, type=pathlib.Path)
    p.add_argument("--output", required=True, type=pathlib.Path)
    a = p.parse_args()
    if a.output.exists():
        p.error("scorecard is immutable; choose a new output path")
    data = score(a.root)
    a.output.write_text(json.dumps(data, sort_keys=True, indent=2) + "\n")
    print("PASS complete and independently cross-checked benchmark inventory; "
          "NO SOTA CLAIM")


if __name__ == "__main__":
    main()
