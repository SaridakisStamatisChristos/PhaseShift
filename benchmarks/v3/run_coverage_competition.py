#!/usr/bin/env python3
"""V18 coverage-first: fresh matched native-worker development competition.

Three *unchanged endpoint* C++ policies, 20 fresh development seeds x six
previously frozen synthetic fixtures x 12 matched-pair decisions each.
This is a development study, NOT release approval or SOTA validation.
Every trial is independently rechecked from raw SQLite and executed logs.
"""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import platform
import random
import sqlite3
import subprocess

from multidim_truth import config,truth,split_check
from protocol import seal,sha256_file,sqlite_snapshot_sha256
from run_multidim import row as audit_trial,read,write
import run_paired_development as pilot

HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[1]
MANIFEST=HERE/"coverage_development.json"
OLDER=HERE/"multidim_development.json"
POLICIES=["conditional-tree","acquisition-v3","acquisition-v3-coverage"]
PINNED_SCHEMA={"conditional-tree":14,"acquisition-v3":15,"acquisition-v3-coverage":18}


def require(ok,reason):
    if not ok:raise ValueError(reason)


def manifest():
    m=read(MANIFEST); old=read(OLDER)
    expected=hashlib.sha1(b"blob "+str(len(OLDER.read_bytes())).encode()+b"\0"+
                          OLDER.read_bytes()).hexdigest()
    require(m["schema"]==1 and
            m["protocol"]=="phaseshift.v18.coverage-cost.3way-development.1" and
            m["suite"]=="development" and
            m["eligible_for_claim"] is False and
            m["sota_claim"] is False and
            m["policy_inference_status"]==
              "unconfirmed_timing_nominations_on_planted_work_unit_boundaries" and
            m["source_fixture_blob_sha1"]==expected and
            m["source_fixture_path"]=="benchmarks/v3/multidim_development.json" and
            m["fixtures"]==old["fixtures"] and
            m["policies"]==POLICIES and
            m["pairs"]==old["pairs"]==12 and
            m["cpu_ceiling_ns"]==old["cpu_ceiling_ns"] and
            m["wall_ceiling_ns"]==old["wall_ceiling_ns"],
            "competition does not use exact frozen previous six fixtures/budgets")
    seeds=m["seeds"]
    require(len(seeds)==20 and
            seeds==list(range(6301,6321)) and
            m["diagnostic_seeds"]==[6301] and
            set(seeds).isdisjoint(old["seeds"]) and
            set(seeds).isdisjoint(range(7101,7121)),
            "not 20 fresh development-only seed clusters")
    require(set(pilot.SCHEMAS).issuperset(PINNED_SCHEMA) and
            all(pilot.SCHEMAS[k]==v for k,v in PINNED_SCHEMA.items()),
            "raw schema versions differ from frozen competitive endpoints")
    split_check()
    return m,sha256_file(MANIFEST)


def order(m,diagnostic,shard):
    require(shard in {f["id"] for f in m["fixtures"]},
            "unknown fixture shard")
    rows=[]
    for seed in m["diagnostic_seeds"] if diagnostic else m["seeds"]:
        policies=POLICIES[:]
        random.Random(int(seal(["v18-coverage-competition",shard,seed]),16)).shuffle(policies)
        rows.extend([[shard,p,seed] for p in policies])
    return rows


def verification(root):
    plan=read(root/"frozen_plan.json")
    digest=plan.pop("sha256")
    m,mh=manifest()
    require(seal(plan)==digest and
            plan["schema"]==1 and
            plan["source_commit"] and len(plan["source_commit"])==40 and
            plan["binary_sha256"] and len(plan["binary_sha256"])==64 and
            plan["manifest"]==m and plan["manifest_sha256"]==mh and
            plan["suite"]=="development" and
            plan["eligible_for_claim"] is False and
            plan["sota_claim"] is False and
            plan["run_order"]==order(m,plan["diagnostic"],plan["shard"]),
            "frozen 3-way comparison is not reproducible")
    f=next(f for f in m["fixtures"] if f["id"]==plan["shard"])
    outputs=[]
    for fixture,policy,seed in plan["run_order"]:
        trial=root/(fixture+"_"+policy+"_"+str(seed))
        require(read(trial/"config.json")==config(f,seed),
                "silent real-worker fixture change")
        audited=audit_trial(trial,f,policy,seed,m)
        raw=audited["raw_trial"]
        require(raw["status"] in ("ok","failed") and
                audited["measurement_endpoint"]==
                "unconfirmed_matched_timing_nomination" and
                audited["analytic_truth_scope"]==
                "discrete_work_unit_jump_not_timing_truth",
                "true elapsed-time superiority falsely inferred")
        with sqlite3.connect((trial/"run"/"run.sqlite").resolve().as_uri()+"?mode=ro",uri=True) as db:
            db.execute("PRAGMA query_only=ON")
            require(db.execute("PRAGMA user_version").fetchone()[0]==PINNED_SCHEMA[policy],
                    "unsupported experimental policy SQLite")
            arms=[x[0] for x in db.execute(
                "SELECT arm FROM v3_conditional_decisions ORDER BY pair_number")]
            require(len(arms)==m["pairs"] and
                    all(arm in ("explore","replicate") for arm in arms),
                    "incomplete exploration/replication accounting")
        audited["exploratory_pairs"]=arms.count("explore")
        audited["replication_pairs"]=arms.count("replicate")
        outputs.append(audited)
    return {
        "schema":1,"protocol":m["protocol"],
        "suite":"development","source_commit":plan["source_commit"],
        "manifest_sha256":mh,"frozen_plan_sha256":sha256_file(root/"frozen_plan.json"),
        "scenario":plan["shard"],"diagnostic":plan["diagnostic"],
        "expected_trials":len(plan["run_order"]),"observed_trials":len(outputs),
        "results":outputs,"eligible_for_claim":False,
        "sota_claim":False,"sealed_evaluation_touched":False
    }


def execute(binary,root,shard,diagnostic):
    m,mh=manifest()
    require(binary.is_file() and not root.exists(),"real worker or immutable destination unavailable")
    source_commit=subprocess.check_output(["git","rev-parse","HEAD"],cwd=ROOT,text=True).strip()
    require(len(source_commit)==40,"missing exact source revision")
    plan={
        "schema":1,"source_commit":source_commit,
        "binary_sha256":sha256_file(binary),
        "manifest":m,"manifest_sha256":mh,
        "suite":"development","shard":shard,
        "diagnostic":diagnostic,"run_order":order(m,diagnostic,shard),
        "host":platform.platform(),"eligible_for_claim":False,
        "sota_claim":False
    }
    root.mkdir(parents=True)
    write(root/"frozen_plan.json",{**plan,"sha256":seal(plan)})
    f=next(f for f in m["fixtures"] if f["id"]==shard)
    failures=[]
    for fixture,policy,seed in plan["run_order"]:
        trial=root/(fixture+"_"+policy+"_"+str(seed))
        trial.mkdir()
        pilot.write(trial/"config.json",config(f,seed))
        run=pilot.measured([str(binary),policy,"--config",str(trial/"config.json"),
                           "--out",str(trial/"run"),"--pairs",str(m["pairs"])],65)
        check=pilot.measured([str(binary),"verify","--run",str(trial/"run")],65) if run["returncode"]==0 else None
        pilot.write(trial/"commands.json",{"explore":run,"verify":check})
        try:
            audit_trial(trial,f,policy,seed,m)
        except Exception as e:
            failures.append({"fixture":fixture,"policy":policy,"seed":seed,
                             "error":repr(e)})
        if run["returncode"] or check is None or check["returncode"]:
            failures.append({"fixture":fixture,"policy":policy,"seed":seed,
                             "returncode":run["returncode"],
                             "verify":None if check is None else check["returncode"],
                             "stderr":run["stderr"][:800]})
        print(json.dumps({"fixture":fixture,"policy":policy,"seed":seed,
                          "returncode":run["returncode"],
                          "verify":None if check is None else check["returncode"]}),
              flush=True)
    if failures:
        write(root/"failed_trials.json",failures)
        raise ValueError(str(len(failures))+" genuine worker trials failed; all raw evidence retained")
    out=verification(root)
    write(root/"scorecard.json",out)
    print(json.dumps({"status":"PASS","scenario":shard,
                      "trials":out["observed_trials"],
                      "policies":m["policies"],"sota_claim":False}),flush=True)


def main():
    p=argparse.ArgumentParser()
    p.add_argument("--binary",type=Path)
    p.add_argument("--out",type=Path,required=True)
    p.add_argument("--shard")
    p.add_argument("--diagnostic",action="store_true")
    p.add_argument("--verify-only",action="store_true")
    args=p.parse_args()
    root=args.out.resolve()
    if args.verify_only:
        report=verification(root)
        require(read(root/"scorecard.json")==report,"unreproducible three-policy scorecard")
        print("PASS independent native 3-way raw SQLite and command replay")
    else:
        require(args.binary is not None and args.shard is not None,
                "real worker and chosen fixture shard required")
        execute(args.binary.resolve(),root,args.shard,args.diagnostic)


if __name__=="__main__":
    main()
