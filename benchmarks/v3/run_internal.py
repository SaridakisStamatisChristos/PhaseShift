#!/usr/bin/env python3
"""Run real PhaseShift v1/v2 workers with a predeclared v3 fairness contract.

Diagnostic-only V3-01 benchmark: no v3 search policy or SOTA claim yet.
The authoritative observations remain each trial's run.sqlite.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import pathlib
import random
import subprocess
import sys

HERE = pathlib.Path(__file__).resolve().parent
ROOT = HERE.parents[1]
sys.path.insert(0, str(ROOT / "benchmarks" / "v2"))
from dual_cap_challenge import run_one  # existing real CLI, holdout and oracle

from protocol import assert_manifest, canonical, seal, sha256_file


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--binary", type=pathlib.Path, required=True)
    p.add_argument("--out", type=pathlib.Path, required=True)
    p.add_argument("--suite", choices=("development", "evaluation"), required=True)
    p.add_argument("--diagnostic", action="store_true",
                   help="one seed / two policies; explicitly ineligible for research claims")
    a = p.parse_args()
    if not a.binary.is_file():
        p.error("PhaseShift binary does not exist")
    if a.out.exists():
        p.error("immutable result directory already exists")
    manifest_path = HERE / (("train" if a.suite == "development" else "evaluation") + "_manifest.json")
    raw_manifest = manifest_path.read_bytes()
    manifest = json.loads(raw_manifest)
    assert_manifest(manifest)
    if manifest["suite"] != a.suite:
        p.error("manifest suite mismatch")
    if a.diagnostic:
        manifest = dict(manifest)
        manifest["seeds"] = manifest["seeds"][:1]
        manifest["policies"] = manifest["policies"][:2]
    # Reject workspace checkouts with unavailable source provenance.
    revision = subprocess.run(["git", "rev-parse", "HEAD"], cwd=ROOT,
                              capture_output=True, text=True, check=True).stdout.strip()
    if len(revision) != 40:
        p.error("missing source revision")
    root = a.out.resolve()
    if root == ROOT or ROOT in root.parents:
        # Build-directory outputs inside the checkout are legitimate ignored data.
        if root == ROOT:
            p.error("output cannot overwrite repository root")
    planned = []
    for fixture in manifest["fixtures"]:
        for seed in manifest["seeds"]:
            arms = list(manifest["policies"])
            entropy = hashlib.sha256(canonical([a.suite, fixture, seed])).digest()
            random.Random(int.from_bytes(entropy, "big")).shuffle(arms)
            planned.extend([[fixture, policy, seed] for policy in arms])
    plan = {
        "schema": 1,
        "document_id": "PHASESHIFT-V3-SOTA-ARCH-001",
        "manifest": manifest,
        "manifest_sha256": hashlib.sha256(raw_manifest).hexdigest(),
        "source_commit": revision,
        "binary_sha256": sha256_file(a.binary),
        "run_order": planned,
        "accounting": "waited_cli_subtree_cpu_plus_harness_cpu",
        "eligible_for_claim": False,
        "sota_claim": False,
        "limitations": [
            "V3-01 diagnostics only; only one planted cliff and one null landscape",
            "Waited CLI subtree CPU includes CLI+worker, not isolated worker CPU",
            "Equal ceilings do not imply equal consumed CPU or wall",
            "No external competitors; no new v3 acquisition policy",
        ],
    }
    digest = seal(plan)
    root.mkdir(parents=True)
    (root / "frozen_plan.json").write_text(
        json.dumps({**plan, "sha256": digest}, sort_keys=True, indent=2) + "\n")
    rows = []
    for fixture, policy, seed in planned:
        row = run_one(a.binary.resolve(), root, policy, fixture, seed,
                      manifest["cases"], manifest["cpu_budget_ns"],
                      manifest["wall_budget_ns"])
        name = f"{fixture}_{policy}_{seed}"
        row["commands_sha256"] = sha256_file(root / (name + ".commands.jsonl"))
        row["sqlite_sha256"] = sha256_file(root / name / "run.sqlite")
        rows.append(row)
        print(json.dumps({"fixture": fixture, "policy": policy, "seed": seed,
                          "completed_cases": row["completed_cases"],
                          "observed_total_cpu_ns": row["observed_total_cpu_ns"],
                          "observed_total_wall_ns": row["observed_total_wall_ns"],
                          "had_failure": row["had_failure"]}, sort_keys=True), flush=True)
    (root / "results.json").write_text(json.dumps({
        "plan_sha256": digest, "rows": rows,
        "complete": not any(r["had_failure"] for r in rows),
        "sota_claim": False,
    }, sort_keys=True, indent=2) + "\n")
    if any(r["had_failure"] for r in rows):
        raise SystemExit("worker failures recorded; see immutable run outputs")


if __name__ == "__main__":
    main()
