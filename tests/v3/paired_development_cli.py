#!/usr/bin/env python3
"""Execute the predeclared eight-trial real-worker smoke and falsify its scorecard."""
import argparse
import json
from pathlib import Path
import shutil
import subprocess
import sys

def check(ok,msg):
    if not ok:
        raise AssertionError(msg)

def main():
    p=argparse.ArgumentParser()
    p.add_argument("--binary",required=True)
    p.add_argument("--runner",required=True)
    p.add_argument("--out",required=True)
    a=p.parse_args()
    root=Path(a.out)
    shutil.rmtree(root,ignore_errors=True)
    cmd=[sys.executable,a.runner,"--out",str(root)]
    def run(args,expect=0):
        r=subprocess.run(cmd+args,capture_output=True,text=True,timeout=240)
        check(r.returncode==expect,f"returncode={r.returncode} expected={expect}\n{r.stdout}\n{r.stderr}")
        return r
    run(["--binary",a.binary,"--diagnostic"])
    report=json.loads((root/"scorecard.json").read_text())
    check(report["diagnostic"] and not report["sota_claim"] and not report["eligible_for_claim"],"claim escalation")
    summary=report["summary"]
    check(len(summary)==8 and len({(x["fixture"],x["policy"]) for x in summary})==8,
          "complete multi-family matrix")
    check(all(x["trials"]==1 and x["failed_trials"]==0 and x["completed_pairs"]==12
              and x["total_wall_ns"]>0 and x["total_cpu_ns"]>=0 for x in summary),
          "incomplete or invalid real-worker costs")
    check(all(x["true_nomination_trials"] is None and x["false_nomination_count"] is None
              for x in summary if x["fixture"] in ("sort_pathology","topk_fraction")),
          "fabricated real-family ground truth")
    run(["--verify-only"])
    path=root/"results.json"
    original=path.read_bytes()
    try:
        changed=json.loads(original)
        changed["rows"][0]["observed_total_cpu_ns"]+=1
        path.write_text(json.dumps(changed))
        result=subprocess.run(cmd+["--verify-only"],capture_output=True,text=True,timeout=90)
        check(result.returncode!=0 and "result inventory, cost or raw evidence mismatch" in result.stderr,
              "cost fabrication was not independently rejected")
    finally:
        path.write_bytes(original)
    run(["--verify-only"])
    print("PASS eight real-worker multi-family comparisons, immutable scorecard, cost tamper rejection")
if __name__=="__main__":
    main()
