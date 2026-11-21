#!/usr/bin/env python3
"""Independent, fail-closed v3 benchmark contracts (not PhaseShift policy code).

All success calculations use independently read SQLite and immutable raw logs.
The measured child CPU is the *waited CLI subtree*, not proven pure worker CPU.
"""
from __future__ import annotations

import hashlib
import json
import math
import pathlib
import sqlite3
import tempfile

ALLOWED_POLICIES = frozenset((
    "adaptive_partition", "grid", "random", "stratified",
    "uniform_v2", "stratified_v2", "weighted_v2",
    "hierarchical_boundary_v2", "cliff_bracket_v2", "hybrid_mixed_v2",
))
TRUTH = {"single_cliff": 24, "constant_null": None}


def canonical(data):
    return json.dumps(data, sort_keys=True, separators=(",", ":"),
                      allow_nan=False).encode("utf-8")


def sha256_file(path):
    digest = hashlib.sha256()
    with open(path, "rb") as source:
        for block in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def sqlite_snapshot_sha256(path):
    """Hash a consistent read-only SQLite *logical snapshot*, including WAL.

    Hashing run.sqlite raw bytes is not safe while the database uses WAL:
    checkpointing can change the main file after a hash was taken without
    changing a single committed observation. SQLite backup reads through
    the pager and produces a fully committed snapshot without mutating the
    source; the temporary snapshot is immediately removed.
    """
    path = pathlib.Path(path)
    if not path.is_file():
        raise ValueError("authoritative run.sqlite missing")
    with tempfile.TemporaryDirectory(prefix="phaseshift-v3-sqlite-snapshot-") as tmp:
        snapshot = pathlib.Path(tmp) / "snapshot.sqlite"
        source = sqlite3.connect(path.resolve().as_uri() + "?mode=ro", uri=True)
        target = None
        try:
            source.execute("PRAGMA query_only=ON")
            target = sqlite3.connect(snapshot)
            source.backup(target, pages=256)
        finally:
            if target is not None:
                target.close()
            source.close()
        return sha256_file(snapshot)


def seal(data):
    return hashlib.sha256(canonical(data)).hexdigest()


def assert_manifest(manifest):
    assert manifest["schema"] == 1
    assert manifest["suite"] in ("development", "evaluation")
    seeds, policies, fixtures = (manifest[x] for x in ("seeds", "policies", "fixtures"))
    assert isinstance(seeds, list) and len(seeds) >= 1
    assert all(type(s) is int and 0 <= s < 2**31 for s in seeds)
    assert len(seeds) == len(set(seeds))
    assert isinstance(policies, list) and policies and len(set(policies)) == len(policies)
    assert set(policies) <= ALLOWED_POLICIES
    assert isinstance(fixtures, list) and fixtures and len(set(fixtures)) == len(fixtures)
    assert set(fixtures) <= set(TRUTH)
    assert manifest["ground_truth"] == {k: TRUTH[k] for k in fixtures}
    assert type(manifest["cases"]) is int and 4 <= manifest["cases"] <= 64
    for key in ("cpu_budget_ns", "wall_budget_ns"):
        assert type(manifest[key]) is int and 1_000_000 <= manifest[key] <= 2**63 - 1
    assert not manifest.get("sota_claim", False)


def load_plan(root):
    plan = json.loads((root / "frozen_plan.json").read_text())
    digest = plan.pop("sha256")
    assert isinstance(digest, str) and seal(plan) == digest, "benchmark plan seal mismatch"
    assert_manifest(plan["manifest"])
    assert plan["schema"] == 1 and plan["manifest_sha256"]
    assert plan["source_commit"] and len(plan["source_commit"]) == 40
    assert plan["binary_sha256"] and len(plan["binary_sha256"]) == 64
    assert plan["accounting"] == "waited_cli_subtree_cpu_plus_harness_cpu"
    assert plan["sqlite_evidence_digest"] == "read_only_pager_backup_sha256_v1"
    assert plan["eligible_for_claim"] is False
    expected = {(f, p, s) for f in plan["manifest"]["fixtures"]
                for p in plan["manifest"]["policies"]
                for s in plan["manifest"]["seeds"]}
    order = [tuple(row) for row in plan["run_order"]]
    assert len(order) == len(expected) and set(order) == expected
    return plan, digest, expected


def verify_results(root):
    """Verify complete trial inventory, raw artifacts, SQLite state and accounting."""
    root = pathlib.Path(root)
    plan, digest, expected = load_plan(root)
    summary = json.loads((root / "results.json").read_text())
    assert summary["plan_sha256"] == digest and summary["sota_claim"] is False
    rows = summary["rows"]
    assert isinstance(rows, list) and len(rows) == len(expected)
    assert [tuple(r[k] for k in ("fixture", "policy", "seed")) for r in rows] == [
        tuple(x) for x in plan["run_order"]], "missing/duplicated/reordered trial"
    manifest = plan["manifest"]
    for row in rows:
        ident = tuple(row[k] for k in ("fixture", "policy", "seed"))
        assert ident in expected
        fixture, policy, seed = ident
        name = f"{fixture}_{policy}_{seed}"
        work = root / name
        command_path = root / (name + ".commands.jsonl")
        sqlite_path = work / "run.sqlite"
        assert work.is_dir() and command_path.is_file() and sqlite_path.is_file()
        assert sha256_file(command_path) == row["commands_sha256"], name
        assert sqlite_snapshot_sha256(sqlite_path) == row["sqlite_snapshot_sha256"], name
        events = [json.loads(line) for line in command_path.read_text().splitlines()]
        assert 1 <= len(events) <= manifest["cases"] + 1
        assert [e["iteration"] for e in events] == list(range(len(events)))
        assert all(e["wall_ns"] > 0 and e["parent_cpu_ns"] >= 0 and
                   e["child_cpu_ns"] >= 0 and type(e["returncode"]) is int
                   for e in events)
        command_cpu = sum(e["parent_cpu_ns"] + e["child_cpu_ns"] for e in events)
        command_wall = sum(e["wall_ns"] for e in events)
        total_cpu, total_wall = (row[k] for k in
                                 ("observed_total_cpu_ns", "observed_total_wall_ns"))
        assert command_cpu <= total_cpu and command_wall <= total_wall
        assert total_cpu == (row["observed_parent_cpu_ns"] +
                             row["observed_waited_child_cpu_ns"])
        assert all(type(row[k]) is int and row[k] >= 0 for k in (
            "observed_total_cpu_ns", "observed_total_wall_ns",
            "observed_parent_cpu_ns", "observed_waited_child_cpu_ns",
            "valid_samples", "invalid_samples", "holdout_samples",
            "confirmation_seals", "completed_cases"))
        assert total_wall > 0
        assert row["completed_cases"] <= manifest["cases"]
        assert row["cpu_overshoot_ns"] == max(0, total_cpu - manifest["cpu_budget_ns"])
        assert row["wall_overshoot_ns"] == max(0, total_wall - manifest["wall_budget_ns"])
        assert row["within_both_ceilings"] == (
            total_cpu <= manifest["cpu_budget_ns"] and total_wall <= manifest["wall_budget_ns"])
        assert row["discovery_case_ceiling"] == manifest["cases"]
        assert row["accounted_cpu_ceiling_ns"] == manifest["cpu_budget_ns"]
        assert row["full_wall_ceiling_ns"] == manifest["wall_budget_ns"]
        assert row["verified_sota_claim"] is False
        if not row["confirmation_attempted"]:
            assert command_cpu == total_cpu and command_wall == total_wall
        # A failed invocation cannot create an eligible confirmation.
        if row["had_failure"] or any(e["returncode"] != 0 for e in events):
            assert row["eligible_confirmed_under_budget"] == 0
        with sqlite3.connect(f"file:{sqlite_path}?mode=ro", uri=True) as conn:
            actual_cases = conn.execute("SELECT COUNT(*) FROM scheduler_decisions").fetchone()[0]
            item = conn.execute(
                "SELECT status,kind,left_value,right_value FROM hypotheses WHERE id='H1'"
            ).fetchone()
            seals = conn.execute("SELECT COUNT(*) FROM confirmation_seals").fetchone()[0]
            holdout = conn.execute("SELECT COUNT(*) FROM samples WHERE holdout=1").fetchone()[0]
            valid, invalid = conn.execute(
                "SELECT SUM(CASE WHEN status='ok' THEN 1 ELSE 0 END),"
                " SUM(CASE WHEN status!='ok' THEN 1 ELSE 0 END)"
                " FROM samples WHERE holdout=0"
            ).fetchone()
        assert actual_cases == row["completed_cases"]
        assert (item[0] if item else "NO_HYPOTHESIS") == row["final_h1_status"]
        assert (item[1] if item else None) == row["h1_kind"]
        assert (item[2] if item else None) == row["h1_left"]
        assert (item[3] if item else None) == row["h1_right"]
        assert seals == row["confirmation_seals"] and holdout == row["holdout_samples"]
        assert (valid or 0) == row["valid_samples"] and (invalid or 0) == row["invalid_samples"]
        is_confirmed = (not row["had_failure"] and row["within_both_ceilings"] and
                        row["final_h1_status"] == "CONFIRMED" and seals > 0)
        assert row["eligible_confirmed_under_budget"] == int(is_confirmed)
        if not is_confirmed:
            assert row["synthetic_false_positive_confirmed"] == 0
        if fixture == "single_cliff":
            good = (row["h1_left"] is not None and row["h1_right"] is not None
                    and row["h1_left"] < TRUTH[fixture] <= row["h1_right"])
            assert row["synthetic_known_threshold_correct"] == int(good)
            assert row["synthetic_false_positive_confirmed"] == 0
        else:
            assert row["synthetic_known_threshold_correct"] == 0
            assert row["synthetic_false_positive_confirmed"] == int(is_confirmed)
    return plan, rows


def exact_binomial_interval(k, n, alpha=0.05):
    """Two-sided Clopper-Pearson interval for independent Bernoulli campaigns."""
    assert type(k) is int and type(n) is int and 0 <= k <= n and n > 0
    def cdf(k0, p):
        return sum(math.comb(n, j) * p**j * (1-p)**(n-j) for j in range(k0+1))
    lo = 0.0
    if k:
        a, b = 0.0, 1.0
        for _ in range(70):
            mid = (a+b)/2
            # P(X >= k) increases with p.
            if 1 - cdf(k-1, mid) > alpha/2:
                b = mid
            else:
                a = mid
        lo = (a+b)/2
    hi = 1.0
    if k < n:
        a, b = 0.0, 1.0
        for _ in range(70):
            mid = (a+b)/2
            # P(X <= k) decreases with p.
            if cdf(k, mid) > alpha/2:
                a = mid
            else:
                b = mid
        hi = (a+b)/2
    return [lo, hi]
