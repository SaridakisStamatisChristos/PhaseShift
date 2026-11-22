#!/usr/bin/env python3
"""Compare old-binary evidence with candidate replay; no timing equality claim."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil
import sqlite3
import subprocess

def require(condition, message):
    if not condition:
        raise RuntimeError(message)

def command(binary, *args):
    p = subprocess.run([str(binary), *map(str, args)], check=True,
                       text=True, capture_output=True, timeout=180)
    return p.stdout

def snapshot(path, *, authoritative_only=False):
    # Pager-backed logical rows, independent of WAL checkpoints and file layout.
    with sqlite3.connect(path.resolve().as_uri() + "?mode=ro", uri=True) as db:
        db.execute("BEGIN")
        version = db.execute("PRAGMA user_version").fetchone()[0]
        require(db.execute("PRAGMA integrity_check").fetchone()[0] == "ok",
                "invalid legacy DB")
        lines = list(db.iterdump())
        if authoritative_only:
            # report() intentionally refreshes derived aggregates/artifact metadata.
            # Preserve every authoritative table, schema, and raw row.
            lines = [line for line in lines if not line.startswith('INSERT INTO "aggregates" ')
                     and not line.startswith('INSERT INTO "artifacts" ')]
        digest = hashlib.sha256("\n".join(lines).encode()).hexdigest()
    return version, digest

def replay_identity(output, case):
    match = re.search(r"REPLAY OK case=([0-9a-f]{64}) input_sha256=([0-9a-f]{64}) algorithm=([^\s]+)", output)
    require(match is not None and match[1] == case, "expected exact replay identity")
    return match.groups()

def main():
    p = argparse.ArgumentParser()
    p.add_argument("--baseline", type=Path, required=True)
    p.add_argument("--candidate", type=Path, required=True)
    p.add_argument("--scratch", type=Path, required=True)
    a = p.parse_args()
    base = a.baseline.resolve()
    old = base / "build/phaseshift"
    new = a.candidate.resolve() / "build/phaseshift"
    scratch = a.scratch.resolve()
    scratch.mkdir(parents=True, exist_ok=False)
    v7 = scratch / "v7"
    command(old, "demo", "--out", v7)
    v8 = base / "build/v2_hybrid_ctest"
    v9 = base / "build/v2_campaign_worker_ctest"
    results = []
    for version, run in ((7, v7), (8, v8), (9, v9)):
        db = run / "run.sqlite"
        initial = snapshot(db)
        require(initial[0] == version, f"wrong fixture schema {initial}")
        # Report and export are legacy write operations (derived metadata).
        # Use disposable copies; replay itself must leave the entire original intact.
        old_copy, new_copy = scratch / f"old-report-{version}", scratch / f"new-report-{version}"
        shutil.copytree(run, old_copy)
        shutil.copytree(run, new_copy)
        authoritative = snapshot(db, authoritative_only=True)
        command(old, "report", "--run", old_copy, "--format", "json")
        before = json.loads((old_copy / "report.json").read_text())
        require(snapshot(old_copy / "run.sqlite", authoritative_only=True) == authoritative,
                "baseline reporting changed authoritative rows")
        cases = sorted({s["case_id"] for s in before["raw_samples"]})
        require(bool(cases), f"no old cases in schema {version}")
        # Use a deterministic small sample, including both extremes of case ID.
        selected = list(dict.fromkeys([cases[0], cases[-1]]))
        replayed = []
        for case in selected:
            expected = replay_identity(command(old, "replay", "--run", run, "--case", case), case)
            observed = replay_identity(command(new, "replay", "--run", run, "--case", case), case)
            require(expected == observed, f"replay drift: {version}/{case}")
            replayed.append(dict(case_id=case, input_sha256=observed[1], algorithm=observed[2]))
        command(new, "report", "--run", new_copy, "--format", "json")
        after = json.loads((new_copy / "report.json").read_text())
        require(snapshot(new_copy / "run.sqlite", authoritative_only=True) == authoritative,
                "candidate reporting changed authoritative rows")
        require(before["raw_samples"] == after["raw_samples"], "raw report drift")
        require(snapshot(db) == initial, f"historical evidence modified: schema {version}")
        results.append(dict(schema=version, logical_sha256=initial[1], replayed=replayed))
    bundle = scratch / "old-bundle"
    export_copy = scratch / "export-copy"
    shutil.copytree(v7, export_copy)
    command(old, "export", "--run", export_copy, "--out", bundle)
    command(new, "verify-bundle", "--bundle", bundle)
    require(snapshot(v7 / "run.sqlite")[1] == results[0]["logical_sha256"],
            "legacy bundle export modified source")
    evidence = dict(status="PASS", scope="v7/v8/v9 old-binary fixtures; sampled case/input identity; raw report equality, authoritative report-copy invariance and full replay SQLite invariance; old v7 bundle verification", fixtures=results)
    (scratch / "compatibility.json").write_text(json.dumps(evidence, indent=2) + "\n")
    print(json.dumps(evidence))

if __name__ == "__main__":
    main()
