#!/usr/bin/env python3
"""V3-01d: run and independently reconstruct sealed fresh-seed confirmations.

Input is a frozen V3-01c development shard, not a sample/holdout search space.
The *old* immutable C++ engine commit must be rebuilt, supplied via --binary.
The output retains the complete confirmation sidecars and command provenance.
"""
from __future__ import annotations
import argparse
import json
import math
from pathlib import Path
import shutil
import subprocess

from confirmation_protocol import (ALPHA,MAX_MEMBERS,MIN_RATIO,SEED_PAIRS,
    SIGN_MODEL,nominate,family_manifest,independent_sidecar,require,read)
from multidim_truth import load,order
from protocol import seal,sha256_file,sqlite_snapshot_sha256
from run_multidim import verify as verify_discovery
from run_paired_development import measured

SOURCE_COMMIT="b8a713f2430578f3ae7ec5e00cadb348348bbcd6"
SOURCE_RUN_ID=45026057899

def write(path,obj):
    with Path(path).open("x") as f:
        json.dump(obj,f,sort_keys=True,indent=2,allow_nan=False)
        f.write("\n")

def invoke(binary,*args):
    cmd=[str(binary),*map(str,args)]
    result=measured(cmd,90)
    require(result["returncode"]==0,
            "failed CLI "+args[0]+": "+str(result["stderr"])[:1200])
    return result

def plan_check(source):
    m,mh=load("development")
    plan=read(source/"frozen_plan.json")
    require(plan["source_commit"]==SOURCE_COMMIT and
            plan["manifest_sha256"]==mh and
            plan["manifest"]==m and
            plan["suite"]=="development" and
            plan["diagnostic"] is False and
            plan["shard"] in {f["id"] for f in m["fixtures"]} and
            plan["run_order"]==order(m,False,plan["shard"]) and
            plan["eligible_for_claim"] is False and
            plan["sota_claim"] is False,"frozen full-run input mismatch")
    verified=verify_discovery(source)
    require(read(source/"scorecard.json")==verified and
            verified["source_commit"]==SOURCE_COMMIT,
            "source scorecard not independently reproduced")
    return m,plan,read(source/"results.json")["rows"]

def select_map(source,rows):
    output={}
    for row in rows:
        identifier=(row["fixture"],row["policy"],row["seed"])
        folder=source/(str(identifier[0])+"_"+str(identifier[1])+"_"+str(identifier[2]))
        candidate=nominate(folder/"run"/"run.sqlite",
            row["raw_trial"]["status"]=="ok" and row["raw_trial"]["within_consumed_ceilings"])
        output[identifier]=candidate
    return output

def audit_trial(source,root,row,choices):
    fixture,policy,seed=row["fixture"],row["policy"],row["seed"]
    dest=root/(fixture+"_"+policy+"_"+str(seed))
    frozen=read(dest/"selection.json")
    require(frozen["fixture"]==fixture and frozen["policy"]==policy and
            frozen["seed"]==seed and frozen["source_commit"]==SOURCE_COMMIT and
            frozen["selected"]==choices and frozen["selection_rule"]==
            "top_two_distinct_eligible_discovery_edges_by_abs_matched_contrast",
            "selection tampered or not precommitted")
    original=source/(fixture+"_"+policy+"_"+str(seed))/"run"/"run.sqlite"
    expected_digest=sqlite_snapshot_sha256(original)
    require(frozen["source_sqlite_snapshot_sha256"]==expected_digest,
            "source discovery SQLite changed")
    outcome=read(dest/"terminal.json")
    require(outcome["fixture"]==fixture and outcome["policy"]==policy and
            outcome["seed"]==seed and
            outcome["source_sqlite_snapshot_sha256"]==expected_digest,
            "terminal evidence provenance mismatch")
    if not choices:
        require(outcome["status"]=="NO_NOMINATION" and
                not (dest/"run").exists() and
                not (dest/"commands.json").exists(),"false empty family")
        return outcome
    require((dest/"run"/"run.sqlite").is_file() and
            sqlite_snapshot_sha256(dest/"run"/"run.sqlite")==expected_digest,
            "frozen family modified its discovery parent")
    commands=read(dest/"commands.json")
    require(type(commands) is list and len(commands)>=2 and
            commands[0]["argv"][1]=="family-freeze" and
            commands[-1]["argv"][1]=="family-report" and
            all(isinstance(cmd["returncode"],int) and cmd["returncode"]==0
                for cmd in commands),"missing or failed executed family CLI")
    family=family_manifest(fixture,policy,seed,choices)
    require(read(dest/"family.json")==family,"frozen candidate family changed")
    report=read(dest/"family_report.json")
    reconstructed=independent_sidecar(original,
                      dest/"run"/"confirmation"/"family.sqlite",family,report)
    require(outcome["status"]=="FAMILY_COMPLETE_UNDER_UNVERIFIED_MODEL" and
            outcome["audit"]==reconstructed and
            outcome["command_log_sha256"]==sha256_file(dest/"commands.json") and
            outcome["family_json_sha256"]==sha256_file(dest/"family.json"),
            "independent family replay disagrees")
    return outcome

def verify(root,source):
    m,plan,rows=plan_check(source)
    frozen=read(root/"plan.json")
    require(frozen["schema"]==1 and frozen["protocol"]=="v3-01d.development.1" and
            frozen["suite"]=="development" and
            frozen["source_commit"]==SOURCE_COMMIT and
            frozen["source_run_id"]==SOURCE_RUN_ID and
            frozen["source_shard_sha256"]==sha256_file(source/"frozen_plan.json") and
            frozen["policy_inference_model"]==SIGN_MODEL and
            frozen["seed_pairs"]==SEED_PAIRS and frozen["max_members"]==MAX_MEMBERS and
            frozen["candidate_min_ratio"]==MIN_RATIO and frozen["alpha"]==ALPHA and
            frozen["evaluation_opened"] is False and
            frozen["expected_trials"]==40 and
            frozen["source_binary_revision"]==SOURCE_COMMIT,
            "invalid confirmation source/frozen protocol")
    candidates=select_map(source,rows)
    result=[]
    for row in rows:
        ident=(row["fixture"],row["policy"],row["seed"])
        dest=root/(ident[0]+"_"+ident[1]+"_"+str(ident[2]))
        result.append(audit_trial(source,root,row,candidates[ident]))
    require(len(result)==40 and
            len({(a["fixture"],a["policy"],a["seed"]) for a in result})==40,
            "incomplete/duplicate confirmation trial inventory")
    score={
      "schema":1,"source_commit":SOURCE_COMMIT,"source_run_id":SOURCE_RUN_ID,
      "protocol":"v3-01d.development.1","suite":"development",
      "fixture":plan["shard"],"trials":40,
      "no_nomination":sum(a["status"]=="NO_NOMINATION" for a in result),
      "completed_families":sum(a["status"]=="FAMILY_COMPLETE_UNDER_UNVERIFIED_MODEL" for a in result),
      "failed":sum(a["status"]=="FAILED" for a in result),
      "selected_members":sum(a["selected_members"] for a in result),
      "observed_holdout_pairs":sum(a["audit"]["observed_holdout_pairs"] for a in result if "audit" in a),
      "nominal_rejections":sum(a["audit"]["nominal_rejections"] for a in result if "audit" in a),
      "model_assumptions_validated":False,
      "familywise_error_certified":False,"sota_claim":False,
      "raw_results":result,"frozen_plan_sha256":sha256_file(root/"plan.json"),
      "warnings":["Exact sign test is conditional on unverified independence/null sign model",
        "No adjusted familywise error calibration for real elapsed timing",
        "No evaluation seeds were used",
        "No observed confirmation automatically becomes a validated SOTA discovery"]}
    return score

def execute(binary,source,root):
    m,plan,rows=plan_check(source)
    require(not root.exists(),"immutable destination already exists")
    require(binary.is_file(),"missing pinned real worker")
    # A CLI built from the frozen source must be used. The v16 sidecar
    # independently rechecks the embedded source revision after freezing.
    root.mkdir(parents=True)
    frozen={"schema":1,"protocol":"v3-01d.development.1",
        "suite":"development","source_commit":SOURCE_COMMIT,
        "source_binary_revision":SOURCE_COMMIT,"source_run_id":SOURCE_RUN_ID,
        "source_shard_sha256":sha256_file(source/"frozen_plan.json"),
        "binary_sha256":sha256_file(binary),
        "policy_inference_model":SIGN_MODEL,"seed_pairs":SEED_PAIRS,
        "max_members":MAX_MEMBERS,"candidate_min_ratio":MIN_RATIO,"alpha":ALPHA,
        "evaluation_opened":False,"expected_trials":40}
    write(root/"plan.json",frozen)
    selected=select_map(source,rows)
    failures=0
    for row in rows:
        fid,policy,seed=row["fixture"],row["policy"],row["seed"]
        ident=(fid,policy,seed);choices=selected[ident]
        dest=root/(fid+"_"+policy+"_"+str(seed))
        dest.mkdir()
        origin=source/dest.name/"run"/"run.sqlite"
        hash_before=sqlite_snapshot_sha256(origin)
        write(dest/"selection.json",{
            "fixture":fid,"policy":policy,"seed":seed,
            "source_commit":SOURCE_COMMIT,
            "source_sqlite_snapshot_sha256":hash_before,
            "selection_rule":"top_two_distinct_eligible_discovery_edges_by_abs_matched_contrast",
            "selected":choices})
        terminal={"fixture":fid,"policy":policy,"seed":seed,
                  "source_sqlite_snapshot_sha256":hash_before,
                  "selected_members":len(choices)}
        if not choices:
            terminal["status"]="NO_NOMINATION"
        else:
            commands=[]
            try:
                shutil.copytree(source/dest.name/"run",dest/"run")
                write(dest/"family.json",family_manifest(fid,policy,seed,choices))
                def call(*args):
                    outcome=invoke(binary,*args)
                    commands.append(outcome)
                    # Log every successful invocation before the next one.
                    if (dest/"commands.json").exists():
                        (dest/"commands.json").unlink()
                    write(dest/"commands.json",commands)
                    return json.loads(outcome["stdout"])
                first=call("family-freeze","--run",dest/"run","--manifest",dest/"family.json")
                require(first["complete"] is False and "members" not in first,
                        "premature family outcome disclosure")
                partial_pairs=-1
                for attempt in range(5):
                    current=call("family-confirm","--run",dest/"run")
                    if current["complete"]:
                        break
                    require(current["committed_pairs"]>partial_pairs,
                            "family made no progress after resumption")
                    partial_pairs=current["committed_pairs"]
                final=call("family-report","--run",dest/"run")
                require(final["complete"] is True,"family never finished planned fresh seeds")
                write(dest/"family_report.json",final)
                reconstruct=independent_sidecar(origin,
                                dest/"run"/"confirmation"/"family.sqlite",
                                read(dest/"family.json"),final)
                require(reconstruct["source_revision"]==SOURCE_COMMIT,
                        "holdout binary revision differs from frozen discovery source")
                require(sqlite_snapshot_sha256(origin)==hash_before,
                        "original discovery was mutated by fresh holdout")
                terminal.update(status="FAMILY_COMPLETE_UNDER_UNVERIFIED_MODEL",
                                audit=reconstruct,
                                command_log_sha256=sha256_file(dest/"commands.json"),
                                family_json_sha256=sha256_file(dest/"family.json"))
            except (ValueError,RuntimeError,KeyError,OSError,TypeError,
                    json.JSONDecodeError,subprocess.TimeoutExpired) as e:
                terminal.update(status="FAILED",failure=repr(e))
                failures+=1
        write(dest/"terminal.json",terminal)
        print(json.dumps({"trial":dest.name,"status":terminal["status"],
                          "selected":len(choices)},sort_keys=True),flush=True)
    if failures:
        # Preserve all failed sidecars and logs for repair; do not rewrite
        # success scorecards over incomplete evidence.
        raise ValueError(str(failures)+" family execution(s) failed; raw evidence retained")
    score=verify(root,source)
    write(root/"scorecard.json",score)
    print(json.dumps({"status":"PASS","trial_count":score["trials"],
                      "completed_families":score["completed_families"],
                      "nominal_rejections":score["nominal_rejections"],
                      "confirmed":False},sort_keys=True),flush=True)

def main():
    p=argparse.ArgumentParser()
    p.add_argument("--source",type=Path,required=True)
    p.add_argument("--out",type=Path,required=True)
    p.add_argument("--binary",type=Path)
    p.add_argument("--verify-only",action="store_true")
    a=p.parse_args()
    source=a.source.resolve();root=a.out.resolve()
    if a.verify_only:
        score=verify(root,source)
        require(read(root/"scorecard.json")==score,"scorecard not independently reproducible")
        print("PASS independent frozen v3 holdout replay, unverified scientific model")
    else:
        require(a.binary is not None,"pinned --binary required")
        execute(a.binary.resolve(),source,root)

if __name__=="__main__":
    main()
