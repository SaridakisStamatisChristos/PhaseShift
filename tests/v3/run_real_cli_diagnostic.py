#!/usr/bin/env python3
"""V3-01 natural-command end-to-end real-worker smoke, including independent score."""
import argparse
import json
import pathlib
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parents[2]
V3 = ROOT / "benchmarks" / "v3"
sys.path.insert(0, str(V3))
from protocol import verify_results


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--binary", type=pathlib.Path, required=True)
    p.add_argument("--out", type=pathlib.Path, required=True)
    a = p.parse_args()
    if a.out.exists():
        p.error("refusing to overwrite previous real-worker experiment")
    development = json.loads((V3 / "train_manifest.json").read_text())
    evaluation = json.loads((V3 / "evaluation_manifest.json").read_text())
    assert not set(development["seeds"]) & set(evaluation["seeds"])
    assert len(evaluation["seeds"]) == len(development["seeds"]) == 20
    subprocess.run([sys.executable, str(V3 / "run_internal.py"),
                    "--binary", str(a.binary.resolve()), "--out", str(a.out),
                    "--suite", "development", "--diagnostic"],
                   cwd=ROOT, check=True)
    card = a.out / "scorecard.json"
    subprocess.run([sys.executable, str(V3 / "scorecard.py"),
                    "--root", str(a.out), "--output", str(card)],
                   cwd=ROOT, check=True)
    plan, rows = verify_results(a.out)
    score = json.loads(card.read_text())
    assert plan["eligible_for_claim"] is False and score["sota_claim"] is False
    assert len(rows) == 4 and len(score["per_fixture_policy"]) == 4
    assert {r["fixture"] for r in rows} == {"single_cliff", "constant_null"}
    assert all(r["completed_cases"] > 0 for r in rows)
    print("PASS v3 real-worker calibration/null CLI, persisted SQLite and independent scorecard")


if __name__ == "__main__":
    main()
