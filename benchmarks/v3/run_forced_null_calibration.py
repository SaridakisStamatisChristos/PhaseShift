#!/usr/bin/env python3
"""V3-01e: predeclared FORCED negative-control families.

Every scenario/seed produces one frozen edge before observing its effect.
Neither discovery thresholds nor candidate selection can remove a null trial.
Fresh high-range family seeds, raw SQLite evidence, exact Holm/sign-tail replay
and descriptive binomial bounds are independently reconstructed. A conditional
nominal rejection is NOT a certified error under actual wall-time sampling.
"""
from __future__ import annotations

import argparse
import contextlib
import hashlib
import json
import math
from pathlib import Path
import platform
import sqlite3
import subprocess
import sys

from confirmation_protocol import db_ro, independent_sidecar, require, read
from protocol import seal, sha256_file, sqlite_snapshot_sha256
from run_paired_development import measured
from run_null_calibration import load_stress, splitmix

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
MANIFEST = HERE / "forced_null_manifest.json"
REVISION_LENGTH = 40
SEED_SHARED_SALT = 0x5d6c9e3a712bf084
ALLOWED_ALGORITHMS = ("calibration", "calibration_seed_shared_null_v3")


def write(path, obj):
    with Path(path).open("x", encoding="utf-8") as output:
        json.dump(obj, output, sort_keys=True, indent=2, allow_nan=False)
        output.write("\n")


def manifest():
    m = read(MANIFEST)
    require(m["schema"] == 1 and
            m["protocol"] == "phaseshift.v3-01e.forced-null.1" and
            m["suite"] == "development_forced_null" and
            m["eligible_for_claim"] is False and m["sota_claim"] is False and
            m["sampling_assumptions_empirically_validated"] is False and
            m["population_sampling_independence_certified"] is False and
            m["families_per_stratum"] == 64 and
            m["holdout_pairs_per_family"] == 20 and
            m["alpha"] == .05 and m["minimum_ratio"] == 1.2 and
            m["selection"] == "one_frozen_edge_exogenous_no_effect_filter" and
            m["direction_rule"] == "index_even_positive_odd_negative",
            "unknown or non-precommitted null protocol")
    scenes = m["scenario"]
    require(len(scenes) == 4 and
            len({s["id"] for s in scenes}) == 4,
            "null stratum inventory altered")
    used = set()
    for s in scenes:
        require(s["id"] in {"flat_adjacent", "flat_wide", "seed_shared",
                            "seed_shared_contended"} and
                s["algorithm"] in ALLOWED_ALGORITHMS and
                type(s["contention"]) is bool and
                0 <= s["left"] < s["right"] <= 1024 and
                type(s["threshold"]) is int and 1 <= s["threshold"] <= 1024,
                "illegal or unbounded null shape")
        ss = s["seeds"]
        require(len(ss) == 64 and len(set(ss)) == 64 and
                all(type(x) is int and 90000 < x < 100000 for x in ss),
                "wrong forced cohort size")
        require(not used.intersection(ss), "reused null scenario discovery seed")
        used.update(ss)
        require(s["contention"] == (s["id"] == "seed_shared_contended"),
                "unplanned temporal interference")
        require((s["algorithm"] == "calibration") ==
                s["id"].startswith("flat"), "null algorithm/fixture mismatch")
    return m, sha256_file(MANIFEST)


def config(m, s, seed):
    return {
        "schema_version": 1, "experiment_id": "v3e-" + s["id"],
        "problem_family": "calibration",
        "algorithms": [s["algorithm"]], "seed": seed,
        "primary_axis": "x",
        "fixed": {"negative": True},
        "axes": {"x": {"values": [s["left"], s["right"]]},
                 "threshold": {"values": [s["threshold"]]}},
        "search": {"policy_version": "v2", "method": "uniform_v2",
                   "max_cases": 2},
        "measurement": {"paired_discovery_blocks": 1,
                        "paired_confirm_blocks": m["holdout_pairs_per_family"],
                        "warmup_iterations": 0, "minimum_batch_ms": 1},
        "detection": {"min_practical_ratio": m["minimum_ratio"]},
        "limits": {"total_wall_seconds": 90, "max_search_queue": 64,
                   "max_case_bytes": 1048576, "max_worker_rss_mb": 512,
                   "max_sample_wall_ms": 1500}
    }


def direction_for(index):
    return 1 if index % 2 == 0 else -1


def expected_work(algorithm, seed):
    if algorithm == "calibration":
        return 1000
    require(algorithm == "calibration_seed_shared_null_v3",
            "unknown equal-work oracle")
    return 1024 << ((splitmix(seed ^ SEED_SHARED_SALT) >> 7) & 3)


def family_spec(m, s, seed, index):
    return {
        "family_id": "v3e-" + s["id"] + "-" + str(seed),
        "seed_pairs": m["holdout_pairs_per_family"], "alpha": m["alpha"],
        "sign_model": "independent_seed_signs_declared",
        # Deliberately independent of ANY measured discovery effect.
        "members": [{"discovery_pair": 0, "direction": direction_for(index)}]
    }


def work_oracle(path, s, expected_rows, discovery=False):
    """Independently check work values and equality for EVERY stored real pair."""
    out = []
    with db_ro(path) as db:
        db.execute("PRAGMA query_only=ON")
        pairs = db.execute("""SELECT pair_number,seed,status,log_contrast
                       FROM v3_pairs ORDER BY pair_number""").fetchall()
        require(len(pairs) == expected_rows, "missing equal-work null evidence")
        for i, (number, seed, status, effect) in enumerate(pairs):
            require(number == i and status in ("ok", "censored") and
                    (effect is None) == (status == "censored"),
                    "invalid equal-work pair status")
            rows = db.execute("""SELECT s.side,s.status,s.work,c.seed,c.params,
                                        s.ns,s.batch
                   FROM v3_samples s JOIN v3_cases c USING(case_id)
                   WHERE s.pair_number=? ORDER BY s.side,s.ordinal""",
                              (i,)).fetchall()
            require(len(rows) == 2 and [x[0] for x in rows] == [0, 1],
                    "null paired side cardinality")
            vals = []
            for side, st, work, recorded_seed, params, ns, batch in rows:
                obj = json.loads(params)
                want_x = s["left"] if side == 0 else s["right"]
                require(recorded_seed == seed and obj["x"] == want_x and
                        obj["threshold"] == s["threshold"] and
                        obj["negative"] is True,
                        "forced null coordinate/seed changed")
                if st == "ok":
                    require(work == expected_work(s["algorithm"], seed) and
                            ns is not None and ns > 0 and
                            batch is not None and batch > 0,
                            "null work differs from independent seeded oracle")
                    vals.append(work)
                else:
                    require(work is None and ns is None and batch is None,
                            "censored null promoted as successful sample")
                    vals.append(None)
            require(vals[0] == vals[1] if status == "ok" else True,
                    "null endpoints do not perform equal work")
            out.append({"pair_number": number, "seed": seed,
                        "status": status, "work": vals[0] if status == "ok" else None})
    return out


def upper_bound_if_independent(k, n, alpha=.05):
    """Exact one-sided Clopper-Pearson upper endpoint; conditional only."""
    require(type(k) is int and type(n) is int and 0 <= k <= n and n > 0,
            "invalid population event count")
    if k == n:
        return 1.0
    lo, hi = 0.0, 1.0
    for _ in range(90):
        p = (lo + hi) / 2
        # P[Binomial(n,p) <= k] = alpha
        cdf = sum(math.comb(n, j) * (p ** j) * ((1-p) ** (n-j))
                  for j in range(k + 1))
        if cdf > alpha:
            lo = p
        else:
            hi = p
    return (lo + hi) / 2


def result_row(source, destination, m, scene, seed, index, revision):
    require(read(destination / "config.json") == config(m, scene, seed),
            "source config mutated")
    frozen = family_spec(m, scene, seed, index)
    require(read(destination / "family.json") == frozen,
            "exogenous family direction/spec mutated")
    original = destination / "run" / "run.sqlite"
    require(original.is_file(), "missing forced null discovery")
    with db_ro(original) as db:
        require(db.execute("PRAGMA user_version").fetchone()[0] in (14, 15),
                "unexpected conditional discovery schema")
        stored, cap = db.execute("SELECT source_json,max_pairs FROM v3_plan WHERE id=1").fetchone()
        require(json.loads(stored) == config(m, scene, seed) and cap == 1,
                "unfrozen discovery design")
    original_work = work_oracle(original, scene, 1, discovery=True)
    report = read(destination / "family_report.json")
    holdout = destination / "run" / "confirmation" / "family.sqlite"
    independent = independent_sidecar(original, holdout, frozen, report)
    require(independent["source_revision"] == revision,
            "confirmation compiled from different source revision")
    work = work_oracle(holdout, scene, m["holdout_pairs_per_family"])
    require(len(work) == 20 and
            independent["observed_holdout_pairs"] == 20 and
            independent["planned_holdout_pairs"] == 20,
            "did not expose the complete forced null family")
    commands = read(destination / "commands.json")
    kinds = [c["argv"][1] for c in commands]
    require(kinds[0] == "conditional-tree" and
            "family-freeze" in kinds and
            "family-confirm" in kinds and
            kinds[-1] == "family-report" and
            all(c["returncode"] == 0 for c in commands),
            "missing or failed null worker command")
    require(report["complete"] is True and report["confirmed"] is False and
            independent["sampling_assumptions_empirically_validated"] is False,
            "conditional null reported as established truth")
    return {
        "seed": seed, "scenario": scene["id"],
        "predeclared_direction": direction_for(index),
        "discovery_effect_is_not_selection": True,
        "discovery_snapshot_sha256": independent["source_snapshot_sha256"],
        "holdout_snapshot_sha256": independent["holdout_snapshot_sha256"],
        "command_sha256": sha256_file(destination/"commands.json"),
        "complete": True,
        "censored_holdout_pairs": sum(x["status"] != "ok" for x in work),
        "null_work_proven_equal_per_pair": True,
        "nominal_model_rejection": independent["nominal_rejections"] == 1,
        "member": independent["members"][0],
        "signed_effects_are_not_independent_proof": True,
        "source_revision": revision
    }


def reconstruct(root):
    frozen = read(root / "frozen_plan.json")
    m, digest = manifest()
    require(frozen["schema"] == 1 and
            frozen["manifest_sha256"] == digest and
            frozen["suite"] == "development_forced_null" and
            frozen["manifest"] == m and
            frozen["diagnostic"] in (True, False) and
            frozen["sota_claim"] is False and
            frozen["population_fwer_certified"] is False and
            frozen["null_sampling_independence_established"] is False,
            "frozen null source protocol modified")
    scene = next((s for s in m["scenario"] if s["id"] == frozen["scenario"]), None)
    require(scene is not None, "unknown null scenario")
    seeds = scene["seeds"][:1] if frozen["diagnostic"] else scene["seeds"]
    require(frozen["seed_inventory"] == seeds and
            len(frozen["source_commit"]) == REVISION_LENGTH,
            "predeclared null inventory/source missing")
    rows = []
    for i, seed in enumerate(seeds):
        folder = root / str(seed)
        row = result_row(root, folder, m, scene, seed, i, frozen["source_commit"])
        require(read(folder/"terminal.json") == row,
                "raw null trial result corrupted")
        rows.append(row)
    n = len(rows)
    k = sum(r["nominal_model_rejection"] for r in rows)
    # Only the full preregistered 64-family cohort gets an interval.
    upper = upper_bound_if_independent(k, n) if n == 64 else None
    return {
        "schema": 1, "protocol": m["protocol"], "suite": m["suite"],
        "source_commit": frozen["source_commit"],
        "manifest_sha256": digest, "scenario": scene["id"],
        "diagnostic": frozen["diagnostic"],
        "planned_families": n, "complete_families": n,
        "planned_holdout_pairs": n * 20, "observed_holdout_pairs": n * 20,
        "nominal_model_rejections": k,
        "nominal_rate": k / n,
        "exact_95_one_sided_upper_if_independent_families": upper,
        "population_independence_established": False,
        "population_fwer_certified": False,
        "sota_claim": False, "eligible_for_claim": False,
        "null_scope": "same deterministic loop work at matched seed, not proven equal wall-time distributions",
        "results": rows
    }


def invoke(binary, *args):
    cmd = [str(binary), *map(str, args)]
    record = measured(cmd, timeout=100)
    require(record["returncode"] == 0,
            "CLI " + str(args[0]) + " failed: " + str(record["stderr"])[:1100])
    return record


def execute(binary, root, target, diagnostic):
    m, mh = manifest()
    scene = next((s for s in m["scenario"] if s["id"] == target), None)
    require(scene is not None, "unknown scenario")
    require(not root.exists() and binary.is_file(), "immutable destination/binary contract")
    revision = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=ROOT,
                                       text=True).strip()
    require(len(revision) == REVISION_LENGTH, "real worker provenance unavailable")
    root.mkdir(parents=True)
    inventory = scene["seeds"][:1] if diagnostic else scene["seeds"]
    write(root/"frozen_plan.json",{
        "schema": 1, "suite": "development_forced_null",
        "manifest_sha256": mh, "manifest": m,
        "binary_sha256": sha256_file(binary), "source_commit": revision,
        "scenario": target, "seed_inventory": inventory, "diagnostic": diagnostic,
        "host": platform.platform(), "eligible_for_claim": False,
        "population_fwer_certified": False, "sota_claim": False,
        "null_sampling_independence_established": False
    })
    failed = []
    for index, seed in enumerate(inventory):
        folder = root / str(seed)
        folder.mkdir()
        write(folder/"config.json",config(m,scene,seed))
        write(folder/"family.json",family_spec(m,scene,seed,index))
        log = []
        try:
            with load_stress(scene["contention"]):
                def call(*args):
                    record = invoke(binary,*args)
                    log.append(record)
                    # Crash-durable completed command inventory for postmortem.
                    (folder/"commands.json").write_text(
                        json.dumps(log,sort_keys=True,indent=2,allow_nan=False)+"\n")
                    return record
                call("conditional-tree","--config",folder/"config.json",
                     "--out",folder/"run","--pairs","1")
                call("verify","--run",folder/"run")
                initial=call("family-freeze","--run",folder/"run",
                             "--manifest",folder/"family.json")
                require(json.loads(initial["stdout"])["complete"] is False,
                        "premature family report")
                progress=-1
                for _ in range(6):
                    result=json.loads(call("family-confirm","--run",folder/"run")["stdout"])
                    if result["complete"]:
                        break
                    require(result["committed_pairs"] > progress,
                            "holdout made no progress")
                    progress=result["committed_pairs"]
                final=json.loads(call("family-report","--run",folder/"run")["stdout"])
                require(final["complete"], "planned 20 fresh pairs not completed")
                write(folder/"family_report.json",final)
            row=result_row(root,folder,m,scene,seed,index,revision)
            write(folder/"terminal.json",row)
            print(json.dumps({"scenario":target,"seed":seed,
                 "null_tested":True,"nominal_rejection":row["nominal_model_rejection"]}),
                 flush=True)
        except (OSError,ValueError,RuntimeError,KeyError,TypeError,sqlite3.Error,
                subprocess.TimeoutExpired, json.JSONDecodeError) as e:
            failed.append({"seed":seed,"error":repr(e)})
            write(folder/"terminal.json",{"seed":seed,"scenario":target,
                         "complete":False,"status":"FAILED","error":repr(e)})
            print(json.dumps({"scenario":target,"seed":seed,"status":"FAILED",
                              "error":repr(e)}),flush=True)
    if failed:
        write(root/"failures.json",failed)
        raise ValueError(str(len(failed))+" predeclared null trials failed; evidence retained")
    score=reconstruct(root)
    write(root/"scorecard.json",score)
    print(json.dumps({"status":"PASS","scenario":target,
                      "families":score["complete_families"],
                      "holdout_pairs":score["observed_holdout_pairs"],
                      "nominal_rejections":score["nominal_model_rejections"],
                      "sota_claim":False}),flush=True)


def main():
    p=argparse.ArgumentParser()
    p.add_argument("--binary",type=Path)
    p.add_argument("--out",required=True,type=Path)
    p.add_argument("--scenario")
    p.add_argument("--diagnostic",action="store_true")
    p.add_argument("--verify-only",action="store_true")
    args=p.parse_args()
    root=args.out.resolve()
    if args.verify_only:
        result=reconstruct(root)
        require(read(root/"scorecard.json")==result,"modified null scorecard")
        print("PASS independently reconstructed forced-null sample/SQLite evidence")
    else:
        require(args.scenario and args.binary,
                "execution requires scenario and actual C++ binary")
        execute(args.binary.resolve(),root,args.scenario,args.diagnostic)


if __name__=="__main__":
    main()
