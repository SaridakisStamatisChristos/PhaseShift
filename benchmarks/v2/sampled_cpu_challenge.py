#!/usr/bin/env python3
"""Reproducible real-worker v2 sampled-CPU-ceiling challenge.

This does NOT equalize complete process CPU, startup, host scheduling or wall
time. The last case may overshoot sampled-worker CPU budget. No SOTA claims.
"""
import argparse
import csv
import hashlib
import json
import pathlib
import resource
import sqlite3
import subprocess
import time
from equal_budget import config

POLICIES = ("uniform_v2", "stratified_v2", "hierarchical_boundary_v2", "hybrid_mixed_v2")
FIXTURES = ("single_cliff", "constant_null")
DEFAULT_SEEDS = ",".join(str(31+i) for i in range(20))


def execute(binary, root, policy, fixture, seed, cases, sampled_cpu_ns):
    name = f"{fixture}_{policy}_{seed}"
    out = root / name
    cfgpath = root / f"{name}.json"
    cfg = config(policy, seed, cases, negative=fixture == "constant_null")
    cfg["limits"]["max_sampled_cpu_ns"] = sampled_cpu_ns
    cfg["limits"]["total_wall_seconds"] = 120
    cfg["search"]["policy_version"] = "v2"
    cfgpath.write_text(json.dumps(cfg, sort_keys=True, indent=2) + "\n")
    parent_before = resource.getrusage(resource.RUSAGE_SELF)
    child_before = resource.getrusage(resource.RUSAGE_CHILDREN)
    started = time.perf_counter_ns()
    try:
        result = subprocess.run([str(binary), "explore", "--config", str(cfgpath),
                                 "--out", str(out)], check=False, text=True,
                                capture_output=True, timeout=130)
        code = result.returncode
        (root / f"{name}.stdout.txt").write_text(result.stdout)
        (root / f"{name}.stderr.txt").write_text(result.stderr)
    except subprocess.TimeoutExpired as exc:
        code = 124
        (root / f"{name}.stderr.txt").write_text(
            f"external subprocess deadline expired; partial evidence retained: {exc}\n")
    wall = time.perf_counter_ns() - started
    child_after = resource.getrusage(resource.RUSAGE_CHILDREN)
    parent_after = resource.getrusage(resource.RUSAGE_SELF)
    def cpu_delta_ns(now, prior):
        return max(0, round(
            ((now.ru_utime + now.ru_stime) -
             (prior.ru_utime + prior.ru_stime)) * 1_000_000_000
        ))
    child_cpu_ns = cpu_delta_ns(child_after, child_before)
    parent_cpu_ns = cpu_delta_ns(parent_after, parent_before)
    row = {
        "fixture": fixture, "policy": policy, "seed": seed,
        "sampled_worker_cpu_ceiling_ns": sampled_cpu_ns,
        "max_cases": cases, "exit_code": code,
        "exploration_wall_ns": wall,
        "command_child_cpu_ns": child_cpu_ns,
        "harness_parent_cpu_ns": parent_cpu_ns,
        "observed_total_cpu_ns": child_cpu_ns + parent_cpu_ns,
        "completed_cases": None,
        "sampled_worker_cpu_ns": None, "invalid_samples": None,
        "last_ledger_cpu_ns": None, "last_ledger_max_ns": None,
        "budget_limit_event": None, "h1_status": None,
        "accounting_verified": False,
        "eligible_full_system_sota_comparison": False,
    }
    if not (out / "run.sqlite").exists():
        return row
    with sqlite3.connect(out / "run.sqlite") as db:
        cost = db.execute(
            "SELECT COUNT(DISTINCT c.case_id),"
            " COALESCE(SUM(CASE WHEN s.status='ok' THEN s.cpu_ns ELSE 0 END),0),"
            " COUNT(CASE WHEN s.status!='ok' OR s.cpu_ns IS NULL THEN 1 END)"
            " FROM samples s JOIN cases c ON c.case_id=s.case_id WHERE s.holdout=0"
        ).fetchone()
        evidence = db.execute(
            "SELECT COUNT(*), MAX(cumulative_sampled_cpu_ns),"
            " MAX(max_sampled_cpu_ns), MIN(accounting_complete) FROM budget_ledger"
        ).fetchone()
        observed = db.execute("SELECT COUNT(*) FROM scheduler_decisions").fetchone()[0]
        cap_event = db.execute(
            "SELECT COUNT(*) FROM events WHERE event='v2_sampled_cpu_limit_reached'"
        ).fetchone()[0]
        h1 = db.execute("SELECT status FROM hypotheses WHERE id='H1'").fetchone()
        row.update(
            completed_cases=cost[0], sampled_worker_cpu_ns=cost[1],
            invalid_samples=cost[2], last_ledger_cpu_ns=evidence[1],
            last_ledger_max_ns=evidence[2], budget_limit_event=cap_event,
            h1_status=h1[0] if h1 else None,
        )
        row["accounting_verified"] = (
            code == 0 and cost[2] == 0 and observed == cost[0] and
            evidence[0] == observed and evidence[3] == 1 and
            evidence[1] == cost[1] and evidence[2] == sampled_cpu_ns and
            (cap_event == 0 or cost[1] >= sampled_cpu_ns)
        )
    return row


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--binary", required=True, type=pathlib.Path)
    p.add_argument("--out", required=True, type=pathlib.Path)
    p.add_argument("--seeds", default=DEFAULT_SEEDS)
    p.add_argument("--cases", type=int, default=32)
    p.add_argument("--sampled-cpu-ns", type=int, default=2500000)
    args = p.parse_args()
    if not 4 <= args.cases <= 64:
        p.error("cases must be 4..64")
    if not 1 <= args.sampled_cpu_ns <= 2**63-1:
        p.error("sampled CPU must be 1..2^63-1 ns")
    try:
        seeds = [int(s) for s in args.seeds.split(",")]
    except ValueError:
        p.error("invalid seed integer")
    if not seeds or len(seeds) != len(set(seeds)) or any(s < 0 or s > 2**31-1 for s in seeds):
        p.error("seeds must be distinct integers 0..2^31-1")
    if args.out.exists():
        p.error("cannot overwrite immutable benchmark output")
    args.out.mkdir(parents=True)
    plan = {
        "schema": 1, "fixture": FIXTURES, "policies": POLICIES,
        "seeds": seeds, "cases": args.cases,
        "sampled_worker_cpu_ceiling_ns": args.sampled_cpu_ns,
        "case_budget_overrun": "one final complete case allowed",
        "full_process_cpu_equal": False, "full_wall_equal": False,
        "full_process_cpu_measured": True,
        "cpu_measurement": "Linux getrusage RUSAGE_CHILDREN and RUSAGE_SELF deltas",
        "sota_claim": False, "predeclared": True,
    }
    plan["sha256"] = hashlib.sha256(
        json.dumps(plan, sort_keys=True).encode()).hexdigest()
    (args.out / "frozen_plan.json").write_text(
        json.dumps(plan, sort_keys=True, indent=2) + "\n")
    rows = []
    binary = args.binary.resolve()
    for fixture in FIXTURES:
        for seed in seeds:
            for policy in POLICIES:
                result = execute(binary, args.out, policy, fixture, seed,
                                 args.cases, args.sampled_cpu_ns)
                rows.append(result)
                print(json.dumps(result, sort_keys=True), flush=True)
    assert len(rows) == len(FIXTURES) * len(POLICIES) * len(seeds)
    with (args.out / "results.csv").open("w", newline="") as output:
        writer = csv.DictWriter(output, fieldnames=list(rows[0]))
        writer.writeheader()
        writer.writerows(rows)
    (args.out / "results.json").write_text(json.dumps({
        "plan_sha256": plan["sha256"], "rows": rows,
        "fully_audited": all(row["accounting_verified"] for row in rows),
        "sota_claim": False,
        "limitations": "Only sampled timed-region worker CPU has a common ceiling;"
                       " whole-system CPU and wall not matched, one-case overshoot allowed",
    }, sort_keys=True, indent=2) + "\n")
    if not all(row["accounting_verified"] for row in rows):
        raise SystemExit("At least one trial failed its immutable sampled-CPU audit")


if __name__ == "__main__":
    main()
