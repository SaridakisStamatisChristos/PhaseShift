#!/usr/bin/env python3
"""Pre-registered two-ceiling end-to-end CLI benchmark. No SOTA claim.

Each exploration is paused after ONE complete case, and its full CLI-process
wall time and waited child-process CPU (including reaped workers on Linux)
are charged. Both CPU and wall ceilings are checked between complete cases.
We report one-case overshoot explicitly; equality of *consumed* CPU/time is
never claimed. Confirmation is an additional measured step and only counts
if it completes within both remaining ceilings and has a positive seal.
"""
import argparse
import copy
import csv
import hashlib
import json
import pathlib
import resource
import sqlite3
import subprocess
import time

from equal_budget import config

POLICIES = ("adaptive_partition", "uniform_v2", "stratified_v2",
            "hierarchical_boundary_v2", "hybrid_mixed_v2")
FIXTURES = ("single_cliff", "constant_null")


def usage_ns(who):
    v = resource.getrusage(who)
    return round((v.ru_utime + v.ru_stime) * 1_000_000_000)


def measured_call(args, deadline_s):
    parent_before = usage_ns(resource.RUSAGE_SELF)
    child_before = usage_ns(resource.RUSAGE_CHILDREN)
    before = time.perf_counter_ns()
    try:
        result = subprocess.run(args, capture_output=True, text=True,
                                timeout=max(.25, deadline_s), check=False)
        code, stdout, stderr = result.returncode, result.stdout, result.stderr
    except subprocess.TimeoutExpired as err:
        code = 124
        stdout = str(err.stdout or "")
        stderr = str(err.stderr or "") + "\nexternal command timed out"
    return {
        "returncode": code,
        "wall_ns": time.perf_counter_ns() - before,
        "parent_cpu_ns": max(0, usage_ns(resource.RUSAGE_SELF) - parent_before),
        "child_cpu_ns": max(0, usage_ns(resource.RUSAGE_CHILDREN) - child_before),
        "stdout": stdout, "stderr": stderr,
    }


def run_one(binary, root, policy, fixture, seed, cases, cpu_budget_ns,
            wall_budget_ns, prepared_config=None):
    name = f"{fixture}_{policy}_{seed}"
    folder = root / name
    manifest = root / (name + ".config.json")
    if prepared_config is None:
        cfg = config(policy, seed, cases, negative=(fixture == "constant_null"))
    else:
        cfg = copy.deepcopy(prepared_config)
        cfg["experiment_id"] = "dual-cap-" + name
        cfg["seed"] = seed
        cfg["search"]["method"] = policy
        cfg["search"]["max_cases"] = cases
        if policy.endswith("_v2"):
            cfg["search"]["policy_version"] = "v2"
        else:
            cfg["search"].pop("policy_version", None)
            cfg.pop("policy_version", None)
    cfg.setdefault("limits", {})["total_wall_seconds"] = max(
        5, min(86400, (wall_budget_ns // 1_000_000_000) + 20))
    manifest.write_text(json.dumps(cfg, sort_keys=True, indent=2) + "\n")
    commands = []
    used_cpu = used_wall = used_parent = used_child = 0
    reached = 0
    had_failure = False
    stopped = "unknown"
    pre_case_count = 0
    confirm_attempted = False
    max_calls = cases + 1
    binary = str(binary)
    for iteration in range(max_calls):
        if used_cpu >= cpu_budget_ns or used_wall >= wall_budget_ns:
            stopped = "dual_cap_reached"
            break
        command = ([binary, "explore", "--config", str(manifest),
                    "--out", str(folder), "--pause-after", "1"]
                   if iteration == 0 else
                   [binary, "resume", "--run", str(folder), "--pause-after", "1"])
        remaining_wall = max(.25, (wall_budget_ns-used_wall)/1_000_000_000)
        trial = measured_call(command, remaining_wall)
        with (root / f"{name}.commands.jsonl").open("a") as handle:
            safe = {k:v for k,v in trial.items() if k not in ("stdout", "stderr")}
            safe["iteration"] = iteration
            safe["command"] = command
            handle.write(json.dumps(safe, sort_keys=True) + "\n")
        (root / f"{name}.log").open("a").write(
            f"iteration={iteration} returncode={trial['returncode']}\n"
            + trial["stdout"] + "\nSTDERR:\n" + trial["stderr"] + "\n")
        used_parent += trial["parent_cpu_ns"]
        used_child += trial["child_cpu_ns"]
        used_cpu += trial["parent_cpu_ns"] + trial["child_cpu_ns"]
        used_wall += trial["wall_ns"]
        if trial["returncode"] != 0:
            stopped = "command_failed_or_timeout"
            had_failure = True
            break
        if not (folder / "run.sqlite").exists():
            stopped = "missing_persisted_run"
            had_failure = True
            break
        with sqlite3.connect(folder / "run.sqlite") as db:
            reached = db.execute("SELECT COUNT(*) FROM scheduler_decisions").fetchone()[0]
            terminal = bool(db.execute(
                "SELECT EXISTS(SELECT 1 FROM events WHERE event='search_complete')"
            ).fetchone()[0])
        if terminal:
            # A completed search may exhaust its legal structural candidate
            # universe before the caller's larger case ceiling. Never call
            # resume on a finalized hypothesis just to manufacture a failure.
            if reached < pre_case_count or reached-pre_case_count>1:
                stopped = "terminal_transcript_inconsistent"
                had_failure = True
            elif reached>=cases:
                stopped = "case_ceiling"
            else:
                stopped = "search_space_exhausted_or_engine_completed"
            break
        if reached <= pre_case_count:
            stopped = "no_new_case_or_exhausted"
            break
        if reached - pre_case_count != 1:
            stopped = "unexpected_multi_case_commit"
            had_failure = True
            break
        pre_case_count = reached
        if reached >= cases:
            stopped = "case_ceiling"
            break
    else:
        stopped = "harness_iteration_guard"

    # Confirmation only after the detector has finalized and a candidate
    # exists. Its entire external cost is charged; overshooting is NOT a win.
    if not had_failure and (folder / "run.sqlite").exists():
        with sqlite3.connect(folder / "run.sqlite") as db:
            row = db.execute("SELECT status FROM hypotheses WHERE id='H1'").fetchone()
        if row and row[0] == "HYPOTHESIZED" and used_cpu < cpu_budget_ns and used_wall < wall_budget_ns:
            confirm_attempted = True
            trial = measured_call([binary, "confirm", "--run", str(folder)],
                                  max(.25,(wall_budget_ns-used_wall)/1_000_000_000))
            with (root / f"{name}.log").open("a") as handle:
                handle.write("confirmation returncode=" + str(trial["returncode"]) +
                             "\n" + trial["stdout"] + trial["stderr"] + "\n")
            used_parent += trial["parent_cpu_ns"]
            used_child += trial["child_cpu_ns"]
            used_cpu += trial["parent_cpu_ns"] + trial["child_cpu_ns"]
            used_wall += trial["wall_ns"]
            if trial["returncode"] != 0:
                had_failure = True
                stopped = "confirmation_command_failed"

    measured_case_proposal_wall_ns = None
    measured_case_worker_wall_ns = None
    budget_phase_events = None
    status = "NO_HYPOTHESIS"
    h1_kind = None
    h1_left = h1_right = None
    seals = holdout_samples = valid = invalid = 0
    if (folder / "run.sqlite").exists():
        with sqlite3.connect(folder / "run.sqlite") as db:
            value = db.execute(
                "SELECT status,kind,left_value,right_value FROM hypotheses WHERE id='H1'"
            ).fetchone()
            if value:
                status,h1_kind,h1_left,h1_right = value
            seals = db.execute("SELECT COUNT(*) FROM confirmation_seals").fetchone()[0]
            holdout_samples = db.execute(
                "SELECT COUNT(*) FROM samples WHERE holdout=1").fetchone()[0]
            # Read real, transactionally committed v2 phase events. V1
            # legitimately has none; never invent a measured overhead.
            phases = db.execute(
                "SELECT detail FROM events WHERE event='v2_phase_budget'"
            ).fetchall()
            if phases:
                decoded = [json.loads(item[0]) for item in phases]
                for event in decoded:
                    assert event["proposal_wall_ns"]>=0 and event["worker_invocation_wall_ns"]>=0
                budget_phase_events = len(decoded)
                measured_case_proposal_wall_ns = sum(
                    event["proposal_wall_ns"] for event in decoded)
                measured_case_worker_wall_ns = sum(
                    event["worker_invocation_wall_ns"] for event in decoded)
            valid, invalid = db.execute(
                "SELECT SUM(CASE WHEN status='ok' THEN 1 ELSE 0 END),"
                " SUM(CASE WHEN status!='ok' THEN 1 ELSE 0 END) "
                "FROM samples WHERE holdout=0").fetchone()
    valid,invalid = valid or 0, invalid or 0
    within = used_cpu <= cpu_budget_ns and used_wall <= wall_budget_ns
    return {
        "fixture":fixture, "policy":policy, "seed":seed,
        "discovery_case_ceiling":cases, "completed_cases":reached,
        "full_wall_ceiling_ns":wall_budget_ns, "accounted_cpu_ceiling_ns":cpu_budget_ns,
        "observed_total_wall_ns":used_wall, "observed_total_cpu_ns":used_cpu,
        "observed_parent_cpu_ns":used_parent, "observed_waited_child_cpu_ns":used_child,
        "phase_events":budget_phase_events,
        "measured_case_proposal_wall_ns":measured_case_proposal_wall_ns,
        "measured_case_worker_wall_ns":measured_case_worker_wall_ns,
        "total_unattributed_wall_ns":(
            max(0,used_wall-measured_case_proposal_wall_ns-measured_case_worker_wall_ns)
            if measured_case_proposal_wall_ns is not None else None),
        "proposal_fraction_of_total_wall":(
            measured_case_proposal_wall_ns / used_wall
            if measured_case_proposal_wall_ns is not None and used_wall else None),
        "non_worker_wall_upper_bound_fraction":(
            max(0,used_wall-measured_case_worker_wall_ns)/used_wall
            if measured_case_worker_wall_ns is not None and used_wall else None),
        "cpu_overshoot_ns":max(0, used_cpu-cpu_budget_ns),
        "wall_overshoot_ns":max(0, used_wall-wall_budget_ns),
        "within_both_ceilings":within, "stop_reason":stopped,
        "had_failure":had_failure, "valid_samples":valid,
        "invalid_samples":invalid, "confirmation_attempted":confirm_attempted,
        "holdout_samples":holdout_samples, "confirmation_seals":seals,
        "final_h1_status":status,
        "h1_kind":h1_kind,"h1_left":h1_left,"h1_right":h1_right,
        "synthetic_known_threshold_correct":int(
            fixture=="single_cliff" and h1_left is not None and
            h1_right is not None and h1_left < 24 <= h1_right),
        "synthetic_false_positive_confirmed":int(
            fixture=="constant_null" and within and
            not had_failure and status=="CONFIRMED" and seals>0),
        "eligible_confirmed_under_budget":int(
            within and not had_failure and status=="CONFIRMED" and seals>0),
        "verified_sota_claim":False,
    }


def main():
    p=argparse.ArgumentParser()
    p.add_argument("--binary",required=True,type=pathlib.Path)
    p.add_argument("--out",required=True,type=pathlib.Path)
    p.add_argument("--seeds",default="31,47,83")
    p.add_argument("--cases",type=int,default=12)
    p.add_argument("--wall-budget-ns",type=int,default=20_000_000_000)
    p.add_argument("--cpu-budget-ns",type=int,default=5_000_000_000)
    args=p.parse_args()
    if not args.binary.is_file():p.error("binary must exist")
    if not 4<=args.cases<=64:p.error("cases must be between 4 and 64")
    if not 1_000_000<=args.wall_budget_ns<=86400*10**9:
        p.error("invalid wall budget ns")
    if not 1_000_000<=args.cpu_budget_ns<=2**63-1:
        p.error("invalid CPU budget ns")
    try:seeds=[int(x) for x in args.seeds.split(",")]
    except ValueError:p.error("invalid seeds")
    if not seeds or len(seeds)!=len(set(seeds)) or any(s<0 or s>2**31-1 for s in seeds):
        p.error("seeds must be distinct 0..2^31-1")
    if args.out.exists():p.error("immutable benchmark output already exists")
    args.out.mkdir(parents=True)
    plan={
        "schema":1,"name":"complete-cli-dual-ceiling-v2",
        "policies":POLICIES,"fixtures":FIXTURES,
        "seeds":seeds,"cases":args.cases,
        "wall_ceiling_ns":args.wall_budget_ns,
        "cpu_ceiling_ns":args.cpu_budget_ns,
        "accounting":"getrusage RUSAGE_SELF/RUSAGE_CHILDREN per complete CLI invocation",
        "sampling_stop":"both ceilings checked after each complete case, max one-case overshoot",
        "policy_specific_case_seed_semantics":True,
        "equal_consumed_resources":False,"external_competitors_included":False,
        "sota_claim":False,"no_exclusions":True,
    }
    encoded=json.dumps(plan,sort_keys=True).encode()
    plan["sha256"]=hashlib.sha256(encoded).hexdigest()
    (args.out/"frozen_plan.json").write_text(json.dumps(plan,sort_keys=True,indent=2)+"\n")
    rows=[]
    for fixture in FIXTURES:
        for seed in seeds:
            for policy in POLICIES:
                row=run_one(args.binary.resolve(),args.out,policy,fixture,seed,
                            args.cases,args.cpu_budget_ns,args.wall_budget_ns)
                rows.append(row)
                print(json.dumps(row,sort_keys=True),flush=True)
    with (args.out/"results.csv").open("w",newline="") as handle:
        writer=csv.DictWriter(handle,fieldnames=list(rows[0]))
        writer.writeheader()
        writer.writerows(rows)
    (args.out/"results.json").write_text(json.dumps({
        "plan_sha256":plan["sha256"],"rows":rows,
        "sota_claim":False,
        "complete":not any(r["had_failure"] for r in rows),
        "limitations":"Dual ceilings, not equal consumed CPU/wall; case-boundary "
          "overshoots recorded; OS accounting may depend on child wait behavior; "
          "the v1/v2 seed generators differ; synthetic fixture only.",
    },sort_keys=True,indent=2)+"\n")
    if any(r["had_failure"] for r in rows):
        raise SystemExit("one or more complete-cost runs failed; raw evidence preserved")


if __name__=="__main__":
    main()
