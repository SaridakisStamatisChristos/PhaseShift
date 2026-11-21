#!/usr/bin/env python3
"""Predeclared synthetic calibration comparison. Not external SOTA evidence.

Runs the actual PHASESHIFT CLI/worker/SQLite/confirmation path for v1 adaptive
versus v2 hierarchical policy under matched *selected-case* budget.
Records actual CPU/wall time separately: equal case count IS NOT equal compute.
No adaptive tuning or skipped negative outcomes; every seed/policy is retained.
"""
import argparse
import csv
import json
import pathlib
import sqlite3
import subprocess
import time


def config(policy, seed, count, negative=False):
    modern = policy in ("hierarchical_boundary_v2", "uniform_v2", "stratified_v2", "hybrid_mixed_v2")
    return {
        "schema_version": 1,
        "experiment_id": f"frozen-calibration-{policy}-{seed}",
        "problem_family": "calibration",
        "algorithms": ["calibration"],
        "seed": seed,
        "primary_axis": "x",
        "axes": {"x": {"values": list(range(64))}},
        "fixed": {"threshold": 24, **({"negative": True} if negative else {})},
        "search": {
            "method": policy,
            "max_cases": count,
            **({"policy_version": "v2"} if modern else {}),
        },
        "measurement": {
            "paired_discovery_blocks": 1,
            "paired_confirm_blocks": 20,
            "warmup_iterations": 0,
        },
        "limits": {"total_wall_seconds": 120, "max_search_queue": 80},
    }


def run(binary, root, policy, seed, count, negative=False):
    folder = root / f"{policy}_{seed}"
    if folder.exists():
        raise RuntimeError(f"refusing to overwrite prior benchmark: {folder}")
    conf = root / f"config_{policy}_{seed}.json"
    conf.write_text(json.dumps(config(policy, seed, count, negative), indent=2) + "\n")
    start = time.perf_counter_ns()
    explored = subprocess.run(
        [str(binary), "explore", "--config", str(conf), "--out", str(folder)],
        stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, check=False,
    )
    exploration_wall_ns = time.perf_counter_ns() - start
    if explored.returncode != 0:
        raise RuntimeError(f"{policy}/{seed} explore returned {explored.returncode}: "
                           f"{explored.stderr[-3000:]}")
    conn = sqlite3.connect(folder / "run.sqlite")
    try:
        candidates = conn.execute(
            "SELECT status, kind, left_value, right_value, effect "
            "FROM hypotheses WHERE id='H1'"
        ).fetchall()
    finally:
        conn.close()
    status, kind, left, right, effect = (
        candidates[0] if candidates else ("NO_HYPOTHESIS", "", None, None, None)
    )
    confirmed = None
    confirmation_wall_ns = None
    if status == "HYPOTHESIZED":
        started = time.perf_counter_ns()
        result = subprocess.run(
            [str(binary), "confirm", "--run", str(folder)],
            stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, check=False,
        )
        confirmation_wall_ns = time.perf_counter_ns() - started
        if result.returncode != 0:
            # An unsuccessful confirmation is evidence, not a win. Preserve it.
            confirmed = f"CONFIRM_COMMAND_FAILED_{result.returncode}"
        else:
            confirmed = "COMMAND_COMPLETED"
    with sqlite3.connect(folder / "run.sqlite") as db:
        costs = db.execute(
            "SELECT COUNT(DISTINCT CASE WHEN holdout=0 THEN case_id END),"
            " SUM(CASE WHEN holdout=0 THEN COALESCE(cpu_ns,0) ELSE 0 END),"
            " SUM(CASE WHEN holdout=0 AND status='ok' THEN 1 ELSE 0 END),"
            " SUM(CASE WHEN holdout=0 AND status!='ok' THEN 1 ELSE 0 END),"
            " MAX(peak_rss_bytes) FROM samples"
        ).fetchone()
        hyp = db.execute("SELECT status FROM hypotheses WHERE id='H1'").fetchone()
        seals = db.execute("SELECT COUNT(*) FROM confirmation_seals").fetchone()[0]
        held = db.execute("SELECT COUNT(*) FROM samples WHERE holdout=1").fetchone()[0]
    final_status = hyp[0] if hyp else "NO_HYPOTHESIS"
    # Presence of seal is mandatory for a counted confirmed discovery.
    confirmed_true = int(final_status == "CONFIRMED" and seals > 0 and
                         left is not None and right is not None and left < 24 <= right)
    return {
        "policy": policy,
        "seed": seed,
        "case_budget": count,
        "discovery_cases": costs[0],
        "discovery_worker_cpu_ns": costs[1],
        "discovery_wall_ns": exploration_wall_ns,
        "discovery_valid_samples": costs[2],
        "discovery_invalid_samples": costs[3],
        "peak_worker_rss_bytes": costs[4],
        "candidate_status": status,
        "candidate_kind": kind,
        "candidate_left": left,
        "candidate_right": right,
        "candidate_effect": effect,
        "localization_error": (
            0 if left is not None and right is not None and left < 24 <= right
            else (min(abs(left - 24), abs(right - 24)) if left is not None else None)
        ),
        "confirmation_command": confirmed,
        "confirmation_wall_ns": confirmation_wall_ns,
        "final_status": final_status,
        "holdout_samples": held,
        "confirmation_seals": seals,
        "confirmed_true_boundary": confirmed_true,
    }


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--binary", required=True, type=pathlib.Path)
    parser.add_argument("--out", required=True, type=pathlib.Path)
    parser.add_argument("--seeds", default="31,47,83")
    parser.add_argument("--cases", type=int, default=20)
    args = parser.parse_args()
    if not 4 <= args.cases <= 64:
        parser.error("--cases must be 4..64")
    if args.out.exists():
        parser.error("output directory already exists; preserve prior evidence")
    args.out.mkdir(parents=True)
    rows = []
    seeds = [int(x) for x in args.seeds.split(",")]
    if not seeds or len(set(seeds)) != len(seeds):
        parser.error("--seeds requires distinct integer seeds")
    for seed in seeds:
        for policy in ("adaptive_partition", "hierarchical_boundary_v2"):
            result = run(args.binary.resolve(), args.out, policy, seed, args.cases)
            rows.append(result)
            print(json.dumps(result, sort_keys=True))
    with (args.out / "results.csv").open("w", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=list(rows[0]))
        writer.writeheader()
        writer.writerows(rows)
    (args.out / "results.json").write_text(
        json.dumps({
            "benchmark_schema_version": 1,
            "benchmark_id": "frozen-single-cliff-calibration-v1-v2",
            "synthetic_fixture": True,
            "ground_truth_threshold": 24,
            "prespecified_seeds": seeds,
            "discovery_selected_case_budget": args.cases,
            "same_case_generators": False,
            "comparability_note": (
                "calibration is seed-invariant; v1 case seeds vary by grid index "
                "and v2 shares a seed; actual CPU/wall budgets are measured "
                "but not equalized. Head-to-head is diagnostic, not SOTA proof."
            ),
            "rows": rows
        }, indent=2) + "\n"
    )


if __name__ == "__main__":
    main()
