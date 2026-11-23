#!/usr/bin/env python3
"""Independent real-worker, transactional-resume, and transcript-falsification gate."""
import argparse
import os
from pathlib import Path
import shutil
import sqlite3
import subprocess

def check(ok, msg):
    if not ok:
        raise AssertionError(msg)

def invoke(binary, *args, env=None, expect=0):
    process = subprocess.run([binary, *map(str, args)], text=True,
                             capture_output=True, env=env, timeout=95)
    check(process.returncode == expect,
          f"{args} -> {process.returncode}, expected {expect}\n{process.stdout}\n{process.stderr}")
    return process

def inspect(path, count):
    with sqlite3.connect(str(path / "run.sqlite")) as db:
        check(db.execute("PRAGMA user_version").fetchone()[0] == 10, "v3 schema version")
        pairs = db.execute("""
          SELECT pair_number,seed,left_first,status,log_contrast,pair_wall_ns
          FROM v3_pairs ORDER BY pair_number""").fetchall()
        check(len(pairs) == count, "pair count")
        check([r[0] for r in pairs] == list(range(count)), "gapless transcript")
        check(len({r[1] for r in pairs}) == count, "fresh independent seeds")
        for index, seed, order, status, value, wall in pairs:
            check(order in (0, 1) and wall > 0, "bad order/cost")
            check((status == "ok") == (value is not None), "censoring contract")
            raw = db.execute("""
              SELECT s.side,c.seed,s.status,s.ns,s.algorithm,c.input_sha256,
                     length(c.input)
              FROM v3_samples s JOIN v3_cases c ON s.case_id=c.case_id
              WHERE s.pair_number=? ORDER BY s.side,s.ordinal""", (index,)).fetchall()
            check(len(raw) == 2, "paired real-worker observations")
            check([r[0] for r in raw] == [0, 1], "pair-side order")
            check(all(r[1] == seed and r[4] == "calibration" and r[6] > 0 for r in raw),
                  "real worker identity or input missing")
            check(all((r[2] == "ok") == (r[3] is not None) for r in raw),
                  "invalid sample converted into numeric speed")
        return pairs

def main():
    p = argparse.ArgumentParser()
    p.add_argument("--binary", required=True)
    p.add_argument("--config", required=True)
    p.add_argument("--out", required=True)
    args = p.parse_args()
    root = Path(args.out)
    shutil.rmtree(root, ignore_errors=True)
    root.mkdir(parents=True)
    normal = root / "normal"
    common = ["--config", args.config, "--left", "0", "--right", "3", "--pairs", "3"]
    invoke(args.binary, "run", *common, "--out", normal, "--pause-after", "1")
    inspect(normal, 1)
    invoke(args.binary, "verify", "--run", normal)
    invoke(args.binary, "resume", "--run", normal, "--pause-after", "2")
    first = inspect(normal, 3)
    invoke(args.binary, "verify", "--run", normal)
    invoke(args.binary, "resume", "--run", normal)
    check(first == inspect(normal, 3), "idempotent replay altered completed observations")
    # Exact deterministic fresh-seed+order transcript through a different worker run.
    repeat = root / "repeat"
    invoke(args.binary, "run", *common, "--out", repeat)
    second = inspect(repeat, 3)
    check([(r[1], r[2]) for r in first] == [(r[1], r[2]) for r in second],
          "run/resume changed deterministic seed+order proposals")
    # Reject an attempt to rerun into immutable evidence.
    invoke(args.binary, "run", *common, "--out", normal, expect=2)
    # Crash after workers but before BEGIN: no orphaned half-pair or spent valid sample.
    crash_before = root / "before"
    poisoned = dict(os.environ, PHASESHIFT_V3_FAILPOINT="before_pair_commit")
    invoke(args.binary, "run", *common, "--out", crash_before, env=poisoned, expect=88)
    inspect(crash_before, 0)
    invoke(args.binary, "resume", "--run", crash_before)
    inspect(crash_before, 3)
    # Crash after COMMIT: exact committed block should replay, never rerun/double count.
    crash_after = root / "after"
    poisoned["PHASESHIFT_V3_FAILPOINT"] = "after_pair_commit"
    invoke(args.binary, "run", *common, "--out", crash_after, env=poisoned, expect=89)
    inspect(crash_after, 1)
    invoke(args.binary, "resume", "--run", crash_after)
    inspect(crash_after, 3)
    # Logical tampering with a seed is detected, not 'repaired' by overwriting evidence.
    tampered = root / "tampered"
    shutil.copytree(normal, tampered)
    with sqlite3.connect(str(tampered / "run.sqlite")) as db:
        db.execute("UPDATE v3_pairs SET seed=seed+1000 WHERE pair_number=0")
    invoke(args.binary, "verify", "--run", tampered, expect=2)
    # A stored summary is not authoritative over independently reconstructed
    # raw worker observations. SQL CHECK cannot detect a finite effect forged
    # without changing its 'ok' status.
    forged_effect = root / "forged_effect"
    shutil.copytree(normal, forged_effect)
    with sqlite3.connect(str(forged_effect / "run.sqlite")) as db:
        db.execute("UPDATE v3_pairs SET log_contrast=log_contrast+0.5 WHERE pair_number=0")
    invoke(args.binary, "verify", "--run", forged_effect, expect=2)
    forged_raw = root / "forged_raw"
    shutil.copytree(normal, forged_raw)
    with sqlite3.connect(str(forged_raw / "run.sqlite")) as db:
        db.execute("UPDATE v3_samples SET ns=ns*2 WHERE pair_number=0 AND side=0")
    invoke(args.binary, "verify", "--run", forged_raw, expect=2)
    # Removing one record leaves a gap that cannot silently become a full pair.
    missing_sample = root / "missing_sample"
    shutil.copytree(normal, missing_sample)
    with sqlite3.connect(str(missing_sample / "run.sqlite")) as db:
        db.execute("DELETE FROM v3_samples WHERE pair_number=0 AND side=0")
    invoke(args.binary, "verify", "--run", missing_sample, expect=2)
    # Reject corrupted configuration bytes even if the worker rows are untouched.
    forged_config = root / "forged_config"
    shutil.copytree(normal, forged_config)
    with sqlite3.connect(str(forged_config / "run.sqlite")) as db:
        db.execute("UPDATE v3_plan SET source_sha256='not-a-real-hash'")
    invoke(args.binary, "verify", "--run", forged_config, expect=2)
    # Invalid promoted status must be vetoed even if an ns field was left behind.
    bad_status = root / "bad_status"
    shutil.copytree(normal, bad_status)
    with sqlite3.connect(str(bad_status / "run.sqlite")) as db:
        db.execute("""UPDATE v3_samples SET status='wrong_answer', ns=NULL,
                   batch=NULL, work=NULL WHERE pair_number=0 AND side=0""")
    invoke(args.binary, "verify", "--run", bad_status, expect=2)
    print("PASS v3 matched-seed real worker, resume, pre/post-commit crash, vetoed tamper and status")

if __name__ == "__main__":
    main()
