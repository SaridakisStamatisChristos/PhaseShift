#!/usr/bin/env python3
"""V18 coverage-first: ONE-TIME sealed native-worker evaluation competition.

Three frozen C++ policies, 20 previously sealed evaluation seeds x six
previously frozen evaluation fixtures x 12 matched-pair decisions each.
No policy tuning, preflight diagnostic on evaluation, or selective reruns.
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
MANIFEST=HERE/"coverage_evaluation.json"
OLDER=HERE/"multidim_evaluation.json"
POLICIES=["conditional-tree","acquisition-v3","acquisition-v3-coverage"]
PINNED_SCHEMA={"conditional-tree":14,"acquisition-v3":15,"acquisition-v3-coverage":18}


def require(ok,reason):
    if not ok:raise ValueError(reason)


def git_blob_sha(path):
    data=path.read_bytes()
    return hashlib.sha1(b"blob "+str(len(data)).encode()+b"\0"+data).hexdigest()


def manifest():
    """Frozen before first evaluation; independently pin both inputs and code."""
    m=read(MANIFEST); old=read(OLDER)
    expected=git_blob_sha(OLDER)
    require(m["schema"]==1 and
            m["protocol"]=="phaseshift.v18.coverage-cost.3way-one-time-evaluation.1" and
            m["suite"]=="evaluation" and
            m["eligible_for_claim"] is False and
            m["sota_claim"] is False and
            m["sealed_evaluation_one_time"] is True and
            m["no_reruns_for_policy_selection"] is True and
            m["benchmarked_since_initial_dev"] is False and
            m["endpoint"]=="unconfirmed_native_timing_nomination_vs_planted_work_unit_edges" and
            m["source_fixture_blob_sha1"]==expected and
            m["source_fixture_path"]=="benchmarks/v3/multidim_evaluation.json" and
            m["fixtures"]==old["fixtures"] and
            m["policies"]==POLICIES and
            m["pairs"]==old["pairs"]==12 and
            m["cpu_ceiling_ns"]==old["cpu_ceiling_ns"] and
            m["wall_ceiling_ns"]==old["wall_ceiling_ns"] and
            m["primary_decision"]==
              "compare_under_budget_one_or_more_correct_edge_nominations_in_all_four_positive_scenarios_20_seed_clusters;report_all_false_nomination_and_cpu_costs;do_not_tune_after_opening",
            "one-time evaluation protocol/fixture drift")
    require(m["frozen_candidate_base_commit"]==
            "cfd0e8bc4ee7040866a95651c8bd8b183b8fa0ac",
            "unfrozen candidate base revision")
    pinned={
       "include/phaseshift/v3/conditional_pair.hpp":"352649a791af583dd5e779500d2db9336119901b",
       "include/phaseshift/v3/acquisition.hpp":"4af662731363c76d87955008d8d655beb0f76566",
       "apps/phaseshift_v3_pairs.cpp":"780d32f50514bdccfc47658230d8795b10fc8918"}
    require(m["pinned_code_blobs"]==pinned,
            "policy code identity changed before evaluation")
    for source,oldsha in pinned.items():
        require(git_blob_sha(ROOT/source)==oldsha,
                "native discovery candidate source mutated: "+source)
    require(m["seeds"]==old["seeds"]==list(range(7101,7121)) and
            m["diagnostic_seeds"]==[] and
            len(m["seeds"])==20 and
            set(m["seeds"]).isdisjoint(range(6101,6121)) and
            set(m["seeds"]).isdisjoint(range(6301,6321)),
            "sealed evaluation inventory/seed isolation drift")
    require(set(pilot.SCHEMAS).issuperset(PINNED_SCHEMA) and
            all(pilot.SCHEMAS[k]==v for k,v in PINNED_SCHEMA.items()),
            "real-worker schema identities differ from policy contract")
    split_check()
    return m,sha256_file(MANIFEST)


def order(m,diagnostic,shard):
    require(not diagnostic,"no evaluation diagnostic or partial-seed opening allowed")
    require(shard in {f["id"] for f in m["fixtures"]},
            "unknown fixture shard")
    rows=[]
    for seed in m["seeds"]:
        policies=POLICIES[:]
        random.Random(int(seal(["v18-sealed-one-time-evaluation",shard,seed]),16)).shuffle(policies)
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
            plan["suite"]=="evaluation" and
            plan["eligible_for_claim"] is False and
            plan["sota_claim"] is False and
            plan["run_order"]==order(m,plan["diagnostic"],plan["shard"]),
            "frozen sealed 3-way evaluation is not reproducible")
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
        "suite":"evaluation","source_commit":plan["source_commit"],
        "manifest_sha256":mh,"frozen_plan_sha256":sha256_file(root/"frozen_plan.json"),
        "scenario":plan["shard"],"diagnostic":plan["diagnostic"],
        "expected_trials":len(plan["run_order"]),"observed_trials":len(outputs),
        "results":outputs,"eligible_for_claim":False,
        "sota_claim":False,"sealed_evaluation_touched":True,
        "evaluation_opened":True
    }


def execute(binary,root,shard,diagnostic):
    require(not diagnostic,"no selected evaluation smoke/subsampling")
    m,mh=manifest()
    require(binary.is_file() and not root.exists(),"real worker or immutable destination unavailable")
    source_commit=subprocess.check_output(["git","rev-parse","HEAD"],cwd=ROOT,text=True).strip()
    require(len(source_commit)==40,"missing exact source revision")
    plan={
        "schema":1,"source_commit":source_commit,
        "binary_sha256":sha256_file(binary),
        "manifest":m,"manifest_sha256":mh,
        "suite":"evaluation","shard":shard,
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
    p.add_argument("--diagnostic",action="store_true",help="FORBIDDEN for sealed evaluation")
    p.add_argument("--verify-only",action="store_true")
    args=p.parse_args()
    require(not args.diagnostic,"evaluation is one-time full 20-seed cohorts only")
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
