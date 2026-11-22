#!/usr/bin/env python3
"""Compare old-binary evidence with candidate replay; no timing equality claim."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import sqlite3
import subprocess

def require(condition, message):
    if not condition:
        raise RuntimeError(message)

def command(binary, *args):
    p = subprocess.run([str(binary), *map(str, args)], check=True,
                       text=True, capture_output=True, timeout=180)
    return p.stdout

def snapshot(path):
    # Pager-backed logical rows, independent of WAL checkpoints and file layout.
    with sqlite3.connect(path.resolve().as_uri() + "?mode=ro", uri=True) as db:
        db.execute("BEGIN")
        version = db.execute("PRAGMA user_version").fetchone()[0]
        require(db.execute("PRAGMA integrity_check").fetchone()[0] == "ok",
                "invalid legacy DB")
        digest = hashlib.sha256("\n".join(db.iterdump()).encode()).hexdigest()
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
        command(old, "report", "--run", run, "--format", "json")
        before = json.loads((run / "report.json").read_text())
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
        command(new, "report", "--run", run, "--format", "json")
        after = json.loads((run / "report.json").read_text())
        require(before["raw_samples"] == after["raw_samples"], "raw report drift")
        require(snapshot(db) == initial, f"historical evidence modified: schema {version}")
        results.append(dict(schema=version, logical_sha256=initial[1], replayed=replayed))
    bundle = scratch / "old-bundle"
    command(old, "export", "--run", v7, "--out", bundle)
    command(new, "verify-bundle", "--bundle", bundle)
    require(snapshot(v7 / "run.sqlite")[1] == results[0]["logical_sha256"],
            "legacy bundle export modified source")
    evidence = dict(status="PASS", scope="v7/v8/v9 old-binary fixtures; sampled case/input identity; raw report and SQLite invariance; old v7 bundle verification", fixtures=results)
    (scratch / "compatibility.json").write_text(json.dumps(evidence, indent=2) + "\n")
    print(json.dumps(evidence))

if __name__ == "__main__":
    main()
