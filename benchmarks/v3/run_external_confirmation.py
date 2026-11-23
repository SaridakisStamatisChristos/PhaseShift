#!/usr/bin/env python3
"""One-time frozen V3-10 external confirmation: V18 vs SMAC3 2.4.1 and Optuna TPE.

Runs, for one (scenario, seed), the unchanged V18 native policy once and both
genuine external optimizers on the same real C++ workers, then tests every
nominated edge on fresh disjoint seeds. The adapters are imported unchanged
from the development runners. The native binary must be built from the pinned
development source commit whose V18 blobs produced the frozen hypotheses.
No cohort trial may run unless the frozen manifest validates.
"""
from __future__ import annotations
import argparse
from importlib.metadata import version as dist_version
import hashlib
import json
from pathlib import Path
import resource
import subprocess
import time

from protocol import sha256_file
from run_real_external import (read,write,require,git_blob,config,allowed_edges,
                               execute_run,select,confirm)
from run_real_smac import map_edge_coordinate,smac_loss

HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[1]
PLAN=HERE/"external_confirmation.json"
PROTOCOL="phaseshift.v3-external-confirmation.1"
POLICIES=("acquisition-v3-coverage","smac3_hpo","optuna_tpe")
SCENARIOS=[{"id":"sort_ascending","source":"sort_pathology","distribution":"ascending"},
           {"id":"topk_uniform","source":"topk_fraction","distribution":"uniform"}]
PINNED={"include/phaseshift/v3/conditional_pair.hpp":"352649a791af583dd5e779500d2db9336119901b",
        "include/phaseshift/v3/acquisition.hpp":"4af662731363c76d87955008d8d655beb0f76566",
        "apps/phaseshift_v3_pairs.cpp":"780d32f50514bdccfc47658230d8795b10fc8918"}
PRIOR_EXTERNAL=("real_external_development.json","real_smac_development.json")


def derived_seeds(seed):
    """Every seed a trial with base seed S can feed to a native run."""
    return {seed,*(seed+100_000+1000*n for n in range(12)),
            *(1_000_000_000+10*seed+j for j in range(2))}


def repository_seed_lists(exclude):
    """All integer seed lists declared anywhere in benchmark/config manifests."""
    found={}
    def walk(node,where):
        if isinstance(node,dict):
            for key,value in node.items():
                if ("seed" in key.lower() and isinstance(value,list) and value and
                        all(isinstance(x,int) and not isinstance(x,bool) for x in value)):
                    found.setdefault(where,set()).update(value)
                walk(value,where)
        elif isinstance(node,list):
            for value in node:
                walk(value,where)
    for path in sorted([*ROOT.glob("benchmarks/**/*.json"),*ROOT.glob("configs/**/*.json")]):
        if path.resolve()==exclude.resolve():
            continue
        walk(json.loads(path.read_text(encoding="utf-8")),str(path.relative_to(ROOT)))
    return found


def validate(p,plan_path=PLAN):
    """Fail closed unless the confirmation contract is exactly the frozen one."""
    require(p.get("schema")==1 and p.get("protocol")==PROTOCOL and p.get("suite")=="confirmation" and
            p.get("status")=="FROZEN_NOT_OPENED" and p.get("one_time") is True and
            p.get("open_token")=="OPEN-51001-51020" and
            p.get("native_source_commit")=="848e2b7df770a50c5070c0a80caf920a5d939765" and
            p.get("pinned_native_source_blobs")==PINNED and
            p.get("fixture_manifest")=="benchmarks/v3/paired_development_manifest.json" and
            p.get("fixture_manifest_blob_sha1")==git_blob(ROOT/p["fixture_manifest"]) and
            p.get("smac_version")=="2.4.1" and p.get("optuna_version")=="4.6.0" and
            p.get("policies")==list(POLICIES) and p.get("pair_budget")==12 and
            p.get("confirmation_pairs")==20 and p.get("max_distinct_candidates")==2 and
            p.get("min_practical_ratio")==1.2 and p.get("alpha")==0.05 and
            p.get("scenarios")==SCENARIOS and
            p.get("seeds")==list(range(51001,51021)) and p.get("diagnostic_seeds")==[50500] and
            p.get("family_alpha")==0.05 and p.get("multiplicity")=="Holm over {H1, H2}" and
            [h.get("id") for h in p.get("hypotheses",[])]==["H1","H2"] and
            [h.get("difference") for h in p["hypotheses"]]==[
                "R_acquisition-v3-coverage - R_smac3_hpo",
                "R_acquisition-v3-coverage - R_optuna_tpe"] and
            all(h.get("alternative")=="greater" for h in p["hypotheses"]) and
            p.get("sign_independence_certified") is False and p.get("sota_claim") is False,
            "external confirmation contract is not the frozen protocol")
    for name,item in p["development_evidence"].items():
        require(hashlib.sha256((ROOT/item["file"]).read_bytes()).hexdigest()==item["sha256"],
                f"archived {name} development evidence changed")
    cohort=set()
    for seed in p["seeds"]+p["diagnostic_seeds"]:
        mine=derived_seeds(seed)
        require(not mine&cohort,"confirmation seeds collide in derived seed space")
        cohort|=mine
    prior=set()
    for name in PRIOR_EXTERNAL:
        for seed in read(HERE/name)["seeds"]:
            prior|=derived_seeds(seed)
    declared=set().union(*repository_seed_lists(plan_path).values())
    require(not cohort&(prior|declared),
            "confirmation cohort reuses a declared or derived seed of a prior study")
    return p


def manifest():
    p=validate(read(PLAN))
    fixtures={f["name"]:f for f in read(ROOT/p["fixture_manifest"])["fixtures"]}
    return p,fixtures


def check_native_source(source_dir,p):
    head=subprocess.check_output(["git","-C",str(source_dir),"rev-parse","HEAD"],text=True).strip()
    require(head==p["native_source_commit"],"native binary source is not the pinned development commit")
    for name,blob in p["pinned_native_source_blobs"].items():
        require(git_blob(source_dir/name)==blob,"pinned native source blob changed: "+name)


def usage():
    a=resource.getrusage(resource.RUSAGE_SELF);b=resource.getrusage(resource.RUSAGE_CHILDREN)
    return a.ru_utime+a.ru_stime,b.ru_utime+b.ru_stime


def run_policy(policy,binary,home,sc,source,seed,p):
    """One discovery arm; returns (observations, proposals). Adapters as in development."""
    rows,proposals=[],[]
    if policy=="acquisition-v3-coverage":
        return execute_run(binary,home/"discovery",config(sc,source,seed,True),policy,p["pair_budget"]),proposals
    edges=allowed_edges(source)
    if policy=="smac3_hpo":
        from ConfigSpace import ConfigurationSpace
        from smac import HyperparameterOptimizationFacade as HPOFacade,Scenario
        from smac.runhistory.dataclasses import TrialValue
        cs=ConfigurationSpace({"edge_coordinate":(0.0,float(len(edges)))})
        optimizer=HPOFacade(Scenario(cs,deterministic=False,n_trials=p["pair_budget"],seed=seed,
                                     output_directory=home/"smac_state"),None,overwrite=True)
        for n in range(p["pair_budget"]):
            info=optimizer.ask()
            coordinate=float(info.config["edge_coordinate"])
            idx=map_edge_coordinate(coordinate,len(edges))
            cfg=config(sc,source,seed+100_000+1000*n,False)
            one={**execute_run(binary,home/"discovery"/("query_"+str(n).zfill(2)),cfg,
                               "optuna_random",1,edges[idx])[0],"pair_number":n}
            rows.append(one)
            loss=smac_loss(one["effect"])
            optimizer.tell(info,TrialValue(cost=loss))
            proposals.append({"pair_number":n,"edge_coordinate":coordinate,"edge_index":idx,
                              "left":edges[idx][0],"right":edges[idx][1],"reported_loss":loss})
        return rows,proposals
    import optuna
    study=optuna.create_study(direction="maximize",
        sampler=optuna.samplers.TPESampler(seed=seed,n_startup_trials=4,n_ei_candidates=24))
    for n in range(p["pair_budget"]):
        proposal=study.ask()
        idx=proposal.suggest_int("edge_index",0,len(edges)-1)
        cfg=config(sc,source,seed+100_000+1000*n,False)
        one={**execute_run(binary,home/"discovery"/("query_"+str(n).zfill(2)),cfg,
                           "optuna_tpe",1,edges[idx])[0],"pair_number":n}
        rows.append(one)
        reward=abs(one["effect"]) if one["effect"] is not None else 0.
        study.tell(proposal,reward)
        proposals.append({"pair_number":n,"edge_index":idx,"left":edges[idx][0],
                          "right":edges[idx][1],"reported_reward":reward})
    return rows,proposals


def execute(binary,native_source,root,scenario,seed,diagnostic):
    p,fixtures=manifest()
    check_native_source(native_source,p)
    require(seed in (p["diagnostic_seeds"] if diagnostic else p["seeds"]),"undeclared confirmation seed")
    require(binary.is_file() and not root.exists(),"missing binary or existing immutable trial folder")
    import optuna
    optuna.logging.set_verbosity(optuna.logging.ERROR)
    require(optuna.__version__==p["optuna_version"] and dist_version("smac")==p["smac_version"],
            "external optimizer versions are not the pinned ones")
    sc=next(s for s in p["scenarios"] if s["id"]==scenario)
    source=fixtures[sc["source"]]
    root.mkdir(parents=True)
    trace={"schema":1,"protocol":PROTOCOL,"suite":"diagnostic" if diagnostic else "confirmation",
           "scenario":scenario,"seed":seed,"binary_sha256":sha256_file(binary),
           "native_source_commit":p["native_source_commit"],
           "harness_revision":subprocess.check_output(["git","rev-parse","HEAD"],cwd=ROOT,text=True).strip(),
           "manifest_sha256":sha256_file(PLAN),"smac_version":dist_version("smac"),
           "optuna_version":optuna.__version__,"policies":{}}
    for policy in POLICIES:
        home=root/policy;home.mkdir()
        start=time.monotonic_ns();cpu0=usage()
        rows,proposals=run_policy(policy,binary,home,sc,source,seed,p)
        cpu1=usage()
        cost={"wall_ns":time.monotonic_ns()-start,
              "harness_cpu_ns":round((cpu1[0]-cpu0[0])*1e9),"child_cpu_ns":round((cpu1[1]-cpu0[1])*1e9)}
        chosen=select(rows,p)
        write(home/"discovery.json",{"observations":rows,"proposals":proposals,"chosen":chosen,"cost":cost})
        proof=confirm(binary,home,sc,source,seed,policy,chosen,p,rows)
        write(home/"confirmation.json",{"results":proof})
        trace["policies"][policy]={
            "queries":len(rows),"distinct_edges":len({(r["left"],r["right"]) for r in rows}),
            "nominations":len(chosen),"cost":cost,
            "rejections":sum(x["status"]=="REJECTED_UNDER_UNVERIFIED_SIGN_INDEPENDENCE" for x in proof),
            "censored":sum(x["status"]=="ABSTAIN_CENSORED" for x in proof)}
    write(root/"trial.json",trace)
    print(json.dumps({"status":"PASS","suite":trace["suite"],"scenario":scenario,"seed":seed,
                      "rejections":{k:v["rejections"] for k,v in trace["policies"].items()}},
                     sort_keys=True),flush=True)


def main():
    cli=argparse.ArgumentParser()
    cli.add_argument("--binary",type=Path)
    cli.add_argument("--native-source",type=Path)
    cli.add_argument("--out",type=Path)
    cli.add_argument("--scenario",choices=[s["id"] for s in SCENARIOS])
    cli.add_argument("--seed",type=int)
    cli.add_argument("--diagnostic",action="store_true")
    cli.add_argument("--validate-only",action="store_true")
    a=cli.parse_args()
    if a.validate_only:
        manifest()
        print(json.dumps({"status":"FROZEN_PROTOCOL_VALID","protocol":PROTOCOL}),flush=True)
        return
    require(None not in (a.binary,a.native_source,a.out,a.scenario,a.seed),"missing trial arguments")
    execute(a.binary.resolve(),a.native_source.resolve(),a.out.resolve(),a.scenario,a.seed,a.diagnostic)


if __name__=="__main__":
    main()
