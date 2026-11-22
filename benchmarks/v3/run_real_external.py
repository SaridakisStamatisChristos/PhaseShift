#!/usr/bin/env python3
"""External real-worker V18 vs actual Optuna (TPE and RandomSampler) study.

Development ONLY. Two native algorithms per scenario. The optimizer observes
absolute matched log-SPEED-RATIO change on legal adjacent structural edges,
NOT planted truth, not a fake Optuna imitation. Fresh 20-pair native worker
replications on independently frozen candidate edges are audited separately.

External sampler proposals use one v3 CLI per pair, while v18 uses one CLI for
12 pairs: exact native worker/oracle and structural objective, *different*
process startup & scheduling overhead. Report both measured costs; no
cost-normalized SOTA or certified significance inference from this comparison.
"""
from __future__ import annotations
import argparse
import copy
import hashlib
import json
import math
from pathlib import Path
import platform
import random
import resource
import sqlite3
import statistics
import subprocess
import time

from protocol import sha256_file,seal
from run_paired_development import decode,measured
from confirmation_protocol import adjusted_pvalues

HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[1]
PLAN=HERE/"real_external_development.json"
ORIGINAL=HERE/"paired_development_manifest.json"
POLICIES=("acquisition-v3-coverage","optuna_tpe","optuna_random")
SCHEMAS={"acquisition-v3-coverage":18,"optuna_tpe":10,"optuna_random":10}
ALGORITHMS={"sort_pathology":("std_sort","quicksort_first_pivot"),
            "topk_fraction":("topk_heap","topk_nth")}


def require(ok,detail):
    if not ok:raise ValueError(detail)


def read(path):
    return json.loads(Path(path).read_text())


def write(path,data):
    with Path(path).open("x") as fd:
        json.dump(data,fd,indent=2,sort_keys=True,allow_nan=False)
        fd.write("\n")


def git_blob(path):
    p=Path(path)
    content=p.read_bytes()
    return hashlib.sha1(b"blob "+str(len(content)).encode()+b"\0"+content).hexdigest()


def manifest():
    m=read(PLAN)
    base=read(ORIGINAL)
    require(m["schema"]==1 and
            m["protocol"]=="phaseshift.v3-external-optuna-real-algorithm-development.1" and
            m["suite"]=="development" and
            m["eligible_for_sota"] is False and
            m["confirmation_not_certification"] is True and
            m["opened_sealed_evaluation"] is False and
            m["source_real_manifest"]=="benchmarks/v3/paired_development_manifest.json" and
            m["source_real_blob_sha1"]==git_blob(ORIGINAL) and
            m["retained_v18_commit"]==
              "cfd0e8bc4ee7040866a95651c8bd8b183b8fa0ac" and
            m["optuna_version"]=="4.6.0" and
            m["policies"]==list(POLICIES) and
            m["pair_budget"]==12 and m["confirmation_pairs"]==20 and
            m["max_distinct_candidates"]==2 and m["min_practical_ratio"]==1.2 and
            m["alpha"]==.05 and
            m["seeds"]==list(range(32001,32021)) and
            m["diagnostic_seeds"]==[32001] and
            m["limits"]=={"cpu_ceiling_ns":30000000000,"wall_ceiling_ns":90000000000},
            "unfrozen external real-worker competition contract")
    pinned={"include/phaseshift/v3/conditional_pair.hpp":"352649a791af583dd5e779500d2db9336119901b",
            "include/phaseshift/v3/acquisition.hpp":"4af662731363c76d87955008d8d655beb0f76566",
            "apps/phaseshift_v3_pairs.cpp":"780d32f50514bdccfc47658230d8795b10fc8918"}
    require(m["pinned_native_source_blobs"]==pinned and
            all(git_blob(ROOT/filename)==sha for filename,sha in pinned.items()),
            "native candidate changed since completed sealed V18 evaluation")
    require(set(m["seeds"]).isdisjoint(range(7101,7121)) and
            len(m["scenarios"])==2 and
            m["scenarios"]==[
              {"id":"sort_ascending","source":"sort_pathology","distribution":"ascending"},
              {"id":"topk_uniform","source":"topk_fraction","distribution":"uniform"}],
            "real algorithm scenario or sealed evaluation leakage")
    by={f["name"]:f for f in base["fixtures"]}
    for scenario in m["scenarios"]:
        f=by[scenario["source"]]
        require(f["truth"]=="unlabelled" and
                tuple(f["config"]["algorithms"])==ALGORITHMS[scenario["source"]] and
                scenario["distribution"] in f["config"]["axes"]["distribution"]["values"] and
                len(f["config"]["axes"][f["config"]["primary_axis"]]["values"])>=4,
                "unlabelled genuine native worker only")
    return m,by


def config(scenario,source,seed,conditional):
    c=copy.deepcopy(source["config"])
    axis=c["primary_axis"]
    context=scenario["distribution"]
    c["experiment_id"]="v3-external-real-"+scenario["id"]
    c["seed"]=seed
    c["search"]["max_cases"]=100
    c["limits"]["total_wall_seconds"]=45
    c["measurement"]["paired_discovery_blocks"]=1
    c["measurement"]["paired_confirm_blocks"]=20
    if conditional:
        c["axes"]["distribution"]={"values":[context]}
    else:
        del c["axes"]["distribution"]
        c.setdefault("fixed",{})["distribution"]=context
    require(len(c["axes"])==(2 if conditional else 1),
            "improper frozen external legal candidate domain")
    return c


def allowed_edges(source):
    a=source["config"]["axes"][source["config"]["primary_axis"]]["values"]
    require(a==sorted(set(a)),"primary axis lacks ordered numeric labels")
    return [(str(x),str(y)) for x,y in zip(a,a[1:])]


def independent_raw_pair(db,number,cfg,policy,left,right):
    info=db.execute("""SELECT seed,status,log_contrast,pair_wall_ns,
                                  worker_timed_cpu_ns
                       FROM v3_pairs WHERE pair_number=?""",(number,)).fetchone()
    require(info and info[3]>0,"missing independently committed matched pair")
    seed,status,stored,wall,cpu=info
    require(status in ("ok","censored") and
            ((stored is not None)==(status=="ok")),
            "censored status promoted into timing")
    cases=db.execute("""SELECT c.case_id,c.family,c.params,c.seed,c.input_sha256,c.input
                        FROM v3_cases c JOIN v3_pairs p
                        ON c.case_id=p.left_case_id OR c.case_id=p.right_case_id
                        WHERE p.pair_number=? ORDER BY c.case_id""",(number,)).fetchall()
    require(len(cases)==2,"expected exactly two genuine native worker cases")
    for case_id,family,parameters,case_seed,input_hash,payload in cases:
        params=json.loads(parameters)
        require(family==cfg["problem_family"] and case_seed==seed and
                hashlib.sha256(payload).hexdigest()==input_hash and
                params[cfg["primary_axis"]] in
                   [int(left),int(right)] and
                params["distribution"]==cfg.get("fixed",{}).get("distribution",
                       cfg["axes"].get("distribution",{}).get("values",[""])[0]),
                "case payload/correctness-input mismatch")
    samples=db.execute("""SELECT s.side,s.ordinal,s.algorithm,s.status,s.ns,s.batch,
                                 s.cpu_ns,c.seed
                          FROM v3_samples s JOIN v3_cases c USING(case_id)
                          WHERE s.pair_number=? ORDER BY s.side,s.ordinal""",
                       (number,)).fetchall()
    algos=cfg["algorithms"]
    require(len(samples)==8 and len(algos)==2 and
            all(s[0] in (0,1) and s[2] in algos and s[7]==seed
                for s in samples) and
            len({(s[0],s[1]) for s in samples})==8,
            "incorrect native worker sample cardinality or identity")
    observed=[]
    raw_cpu=0
    for side in (0,1):
        group=[s for s in samples if s[0]==side]
        ok=all(s[3]=="ok" and s[4] is not None and s[4]>0 and
               s[5] is not None and s[5]>0 for s in group)
        observed.append(ok)
        for alg in algos:
            require(len([s for s in group if s[2]==alg])==2,
                    "wrong worker reference/algorithm count")
    for s in samples:
        if s[6] is None:raw_cpu=None;break
        raw_cpu+=s[6]
    require(raw_cpu==cpu,"real CPU differs from raw worker samples")
    if all(observed):
        side_metric=[]
        for side in (0,1):
            group=[s for s in samples if s[0]==side]
            t=[statistics.median(s[4]/s[5] for s in group if s[2]==alg)
               for alg in algos]
            side_metric.append(math.log(t[0])-math.log(t[1]))
        effect=side_metric[1]-side_metric[0]
        require(math.isfinite(effect) and
                math.isclose(effect,stored,rel_tol=1e-12,abs_tol=1e-12),
                "native worker pair differs from independent raw timing reconstruction")
    else:
        effect=None
    require((effect is not None)==(status=="ok"),
            "stored native status contradicts independent raw timing observations")
    return {"pair_number":number,"seed":seed,"status":status,
            "effect":effect,"pair_wall_ns":wall,
            "worker_cpu_ns":raw_cpu,"left":left,"right":right}


def audit_native_run(folder,cfg,policy,left_right=None):
    sqlite_file=folder/"run.sqlite"
    require(sqlite_file.is_file(),"missing original authoritative run.sqlite")
    with sqlite3.connect(sqlite_file.resolve().as_uri()+"?mode=ro",uri=True) as db:
        db.execute("PRAGMA query_only=ON")
        schema=db.execute("PRAGMA user_version").fetchone()[0]
        require(schema==SCHEMAS[policy],"native worker schema mismatch")
        source=db.execute("SELECT source_json,max_pairs,left_label,right_label FROM v3_plan WHERE id=1").fetchone()
        require(source and json.loads(source[0])==cfg,"changed frozen real algorithm parameters")
        rows=db.execute("SELECT pair_number,seed FROM v3_pairs ORDER BY pair_number").fetchall()
        require(len(rows)==source[1] and
                rows==sorted(rows) and
                [n for n,_ in rows]==list(range(source[1])) and
                len({s for _,s in rows})==len(rows),
                "native worker pair cap/order or seed reuse")
        if left_right is not None:
            require((source[2],source[3])==tuple(left_right),"external endpoint changed")
        out=[]
        for n,_ in rows:
            if policy=="acquisition-v3-coverage":
                row=db.execute("""SELECT axis,left_coords,right_coords,arm FROM
                                  v3_conditional_decisions WHERE pair_number=?""",
                               (n,)).fetchone()
                require(row and row[0]==cfg["primary_axis"] and
                        row[3] in ("explore","replicate"),
                        "illegal v18 proposal or replay audit missing")
                a,b=decode(row[1]),decode(row[2])
                require(a["distribution"]==b["distribution"]==
                        cfg["axes"]["distribution"]["values"][0],
                        "conditional context mutated")
                left,right=a[cfg["primary_axis"]],b[cfg["primary_axis"]]
                arm=row[3]
            else:
                left,right=source[2],source[3];arm="external"
            require((left,right) in allowed_edges({"config":cfg}),
                    "nonadjacent or illegal native boundary query")
            item=independent_raw_pair(db,n,cfg,policy,left,right)
            item["arm"]=arm
            out.append(item)
        require(len(out)==len(rows) and
                db.execute("SELECT COUNT(*) FROM v3_samples").fetchone()[0]==8*len(rows),
                "orphan raw sample or missing observation")
        return out


def invoke(binary,*args):
    start=time.monotonic_ns()
    u0=resource.getrusage(resource.RUSAGE_SELF)
    c0=resource.getrusage(resource.RUSAGE_CHILDREN)
    try:
        p=subprocess.run([str(binary),*map(str,args)],capture_output=True,text=True,
                         timeout=75)
        code,output,error=p.returncode,p.stdout,p.stderr
    except subprocess.TimeoutExpired as e:
        code=124
        output=e.stdout or "";error=e.stderr or ""
        if isinstance(output,bytes):output=output.decode(errors="replace")
        if isinstance(error,bytes):error=error.decode(errors="replace")
    u1=resource.getrusage(resource.RUSAGE_SELF)
    c1=resource.getrusage(resource.RUSAGE_CHILDREN)
    ns=lambda a,b:round(((b.ru_utime+b.ru_stime)-(a.ru_utime+a.ru_stime))*1e9)
    return {"command":[str(x) for x in args],"code":code,
            "stdout":output,"stderr":error,
            "wall_ns":time.monotonic_ns()-start,
            "harness_cpu_ns":max(0,ns(u0,u1)),
            "child_cpu_ns":max(0,ns(c0,c1))}


def execute_run(binary,folder,cfg,policy,pairs,left_right=None):
    folder.mkdir(parents=True)
    file=folder/"config.json"
    write(file,cfg)
    cmd=["--config",file,"--out",folder/"run","--pairs",pairs]
    if left_right is None:
        result=invoke(binary,"acquisition-v3-coverage",*cmd)
    else:
        result=invoke(binary,"run",*cmd,"--left",left_right[0],
                      "--right",left_right[1])
    verify=invoke(binary,"verify","--run",folder/"run") if result["code"]==0 else None
    write(folder/"commands.json",{"explore":result,"verify":verify})
    require(result["code"]==0 and verify and verify["code"]==0,
            "native real worker/independent CLI verification failed: "+
            result["stderr"][:450])
    rows=audit_native_run(folder/"run",cfg,policy,left_right)
    require(len(rows)==pairs,"partially executed genuine worker experiment")
    return rows


def select(rows,plan):
    by={}
    threshold=math.log(plan["min_practical_ratio"])
    for i,row in enumerate(rows):
        effect=row["effect"]
        if effect is None:continue
        require(math.isfinite(effect),"nonfinite discovery effect in external proposal")
        if abs(effect)<threshold:continue
        edge=(row["left"],row["right"])
        rank=(-abs(effect),i)
        if edge not in by or rank<by[edge][0]:
            by[edge]=(rank,{"source_pair":i,
                             "left":row["left"],"right":row["right"],
                             "direction":1 if effect>0 else -1,
                             "discovery_effect":effect})
    return [x[1] for x in sorted(by.values(),key=lambda x:x[0])
            [:plan["max_distinct_candidates"]]]


def exact_sign_tail(n,k):
    require(0<=k<=n and n==20,"exact test requires twenty independent planned seed pairs")
    return sum(math.comb(n,t) for t in range(k,n+1))/(1<<n)


def confirm(binary,folder,sc,source,seed,policy,candidates,plan,discovery):
    results=[];anyseed={x["seed"] for x in discovery}
    for j,item in enumerate(candidates):
        # Seed stream independent of observed discovery and frozen by campaign
        # identity. All policies use the same base for their nominated edge j.
        confirm_seed=1_000_000_000+seed*10+j
        cfg=config(sc,source,confirm_seed,False)
        path=folder/("confirmation_"+str(j))
        confirmations=execute_run(binary,path,cfg,"optuna_random",
                                  plan["confirmation_pairs"],
                                  (item["left"],item["right"]))
        seeds={x["seed"] for x in confirmations}
        require(not seeds.intersection(anyseed) and len(seeds)==20,
                "confirmation reused any discovery seed")
        anyseed.update(seeds)
        valid=sum(x["effect"] is not None for x in confirmations)
        positive=sum(x["effect"] is not None and
                     x["effect"]*item["direction"]>math.log(plan["min_practical_ratio"])
                     for x in confirmations)
        p=exact_sign_tail(20,positive) if valid==20 else 1.
        results.append({**item,"confirmation_source_seed":confirm_seed,
                        "valid":valid,"positive":positive,"raw_p":p,
                        "observations":confirmations,
                        "fresh_seed_overlap":False})
    adjusted=adjusted_pvalues([x["raw_p"] for x in results])
    for i,x in enumerate(results):
        x["holm_p"]=adjusted[i]
        x["status"]=("ABSTAIN_CENSORED" if x["valid"]<20 else
                     "REJECTED_UNDER_UNVERIFIED_SIGN_INDEPENDENCE" if
                     x["holm_p"]<=plan["alpha"] else
                     "NOT_REJECTED")
    return results


def execute(binary,root,sc,seed,diagnostic):
    plan,by=manifest()
    require(seed in (plan["diagnostic_seeds"] if diagnostic else plan["seeds"]),
            "undeclared seed")
    require(not root.exists(),"immutable evidence folder already exists")
    require(binary.is_file(),"missing real native C++ executable")
    import optuna
    require(optuna.__version__==plan["optuna_version"],"pinned external Optuna version")
    optuna.logging.set_verbosity(optuna.logging.ERROR)
    source=by[sc["source"]]
    root.mkdir(parents=True)
    trace={"schema":1,"scenario":sc["id"],"source":sc["source"],"seed":seed,
           "optuna_version":optuna.__version__,
           "binary_sha256":sha256_file(binary),
           "frozen_manifest_sha256":sha256_file(PLAN),
           "original_manifest_blob_sha1":git_blob(ORIGINAL),
           "source_revision":subprocess.check_output(["git","rev-parse","HEAD"],
                                cwd=ROOT,text=True).strip(),
           "suite":"development","evaluation_cohort_opened":False,
           "discovery":{},"confirmation":{}}
    for policy in plan["policies"]:
        dest=root/policy; dest.mkdir()
        start=time.monotonic_ns()
        u0=resource.getrusage(resource.RUSAGE_SELF)
        c0=resource.getrusage(resource.RUSAGE_CHILDREN)
        rows=[]
        if policy=="acquisition-v3-coverage":
            cfg=config(sc,source,seed,True)
            rows=execute_run(binary,dest/"discovery",cfg,policy,plan["pair_budget"])
        else:
            sampler=(optuna.samplers.TPESampler(seed=seed,n_startup_trials=4,
                                               n_ei_candidates=24) if policy=="optuna_tpe"
                     else optuna.samplers.RandomSampler(seed=seed))
            study=optuna.create_study(direction="maximize",sampler=sampler)
            edges=allowed_edges(source)
            for n in range(plan["pair_budget"]):
                proposal=study.ask()
                edgeindex=proposal.suggest_int("edge_index",0,len(edges)-1)
                cfg=config(sc,source,seed+1000*n+100_000,False)
                result=execute_run(binary,dest/"discovery"/("query_"+str(n).zfill(2)),
                                   cfg,policy,1,edges[edgeindex])
                one={**result[0],"pair_number":n}
                rows.append(one)
                # Original Optuna TPE reward is *adapted*, not an unmodified
                # global-max objective. Censored work gets zero reward, but
                # remains a fully costed failure in the evidence.
                study.tell(proposal,abs(one["effect"]) if one["effect"] is not None else 0.)
        c1=resource.getrusage(resource.RUSAGE_CHILDREN)
        u1=resource.getrusage(resource.RUSAGE_SELF)
        cost={"wall_ns":time.monotonic_ns()-start,
              "harness_cpu_ns":round(((u1.ru_utime+u1.ru_stime)-
                                     (u0.ru_utime+u0.ru_stime))*1e9),
              "child_cpu_ns":round(((c1.ru_utime+c1.ru_stime)-
                                   (c0.ru_utime+c0.ru_stime))*1e9)}
        chosen=select(rows,plan)
        write(dest/"discovery.json",{"observations":rows,"chosen":chosen,"cost":cost})
        proof=confirm(binary,dest,sc,source,seed,policy,chosen,plan,rows)
        write(dest/"confirmation.json",{"results":proof})
        trace["discovery"][policy]={"observations":len(rows),
                                     "nominations":len(chosen),"cost":cost}
        trace["confirmation"][policy]={
            "tested":len(proof),
            "nominal_model_rejections":sum(
                 x["status"]=="REJECTED_UNDER_UNVERIFIED_SIGN_INDEPENDENCE"
                 for x in proof)}
    write(root/"plan.json",trace)
    print(json.dumps({"status":"PASS","scenario":sc["id"],"seed":seed,
                      "policies":trace["confirmation"],"scope":"DEVELOPMENT_NOT_SOTA"},
                     sort_keys=True),flush=True)


def verify(root):
    plan,by=manifest()
    record=read(root/"plan.json")
    require(record["suite"]=="development" and
            record["evaluation_cohort_opened"] is False and
            record["frozen_manifest_sha256"]==sha256_file(PLAN) and
            record["original_manifest_blob_sha1"]==git_blob(ORIGINAL) and
            record["optuna_version"]==plan["optuna_version"] and
            len(record["binary_sha256"])==64 and
            len(record["source_revision"])==40 and
            record["seed"] in plan["seeds"],
            "source/binary/evaluation provenance drift")
    sc=next((s for s in plan["scenarios"] if s["id"]==record["scenario"]),None)
    require(sc is not None and sc["source"]==record["source"],"unknown/unfrozen scenario")
    source=by[sc["source"]]
    conclusions={}
    for policy in plan["policies"]:
        folder=root/policy
        frozen=read(folder/"discovery.json")
        observed=[]
        if policy=="acquisition-v3-coverage":
            observed=audit_native_run(folder/"discovery"/"run",
                 config(sc,source,record["seed"],True),policy)
        else:
            for n in range(plan["pair_budget"]):
                orig=frozen["observations"][n]
                observed.extend(audit_native_run(
                    folder/"discovery"/("query_"+str(n).zfill(2))/"run",
                    config(sc,source,record["seed"]+1000*n+100_000,False),
                    policy,(orig["left"],orig["right"])))
                observed[-1]["pair_number"]=n
        require(observed==frozen["observations"] and
                select(observed,plan)==frozen["chosen"] and
                len(observed)==plan["pair_budget"] and
                all(record["discovery"][policy][key]==value for key,value in
                    (("observations",len(observed)),("nominations",len(frozen["chosen"])),
                     ("cost",frozen["cost"]))),
                "raw independent discovery replay or truth-blind nomination mismatch")
        chosen=frozen["chosen"]
        verified=[]
        discovery_seeds={v["seed"] for v in observed}
        for j,item in enumerate(chosen):
            sub=folder/("confirmation_"+str(j))
            confirmation_seed=1_000_000_000+record["seed"]*10+j
            cfg=config(sc,source,confirmation_seed,False)
            replay=audit_native_run(sub/"run",cfg,"optuna_random",
                                    (item["left"],item["right"]))
            require(len(replay)==20 and
                    not discovery_seeds.intersection({x["seed"] for x in replay}),
                    "confirmation seed collision or missing 20 fresh pairs")
            discovery_seeds.update(x["seed"] for x in replay)
            k=sum(x["effect"] is not None and
                  item["direction"]*x["effect"]>math.log(plan["min_practical_ratio"])
                  for x in replay)
            valid=sum(x["effect"] is not None for x in replay)
            p=exact_sign_tail(20,k) if valid==20 else 1.
            verified.append({**item,
                             "confirmation_source_seed":confirmation_seed,
                             "valid":valid,"positive":k,"raw_p":p,
                             "observations":replay,"fresh_seed_overlap":False})
        adj=adjusted_pvalues([v["raw_p"] for v in verified])
        for j,v in enumerate(verified):
            v["holm_p"]=adj[j]
            v["status"]=("ABSTAIN_CENSORED" if v["valid"]<20 else
                 "REJECTED_UNDER_UNVERIFIED_SIGN_INDEPENDENCE" if
                 adj[j]<=plan["alpha"] else "NOT_REJECTED")
        require(read(folder/"confirmation.json")=={"results":verified},
                "raw independent confirmation replay or Holm mismatch")
        require(record["confirmation"][policy]=={
            "tested":len(verified),
            "nominal_model_rejections":sum(v["status"]==
               "REJECTED_UNDER_UNVERIFIED_SIGN_INDEPENDENCE" for v in verified)},
            "confirmation summary differs from raw observations")
        conclusions[policy]={
            "candidate_count":len(verified),
            "nominal_model_rejections":record["confirmation"][policy]["nominal_model_rejections"],
            "censored_candidate_count":sum(v["valid"]<20 for v in verified),
            "discovery_unique_edges":len({(x["left"],x["right"]) for x in observed}),
            "discovery_cpu_ns":frozen["cost"]["harness_cpu_ns"]+
                               frozen["cost"]["child_cpu_ns"],
            "discovery_wall_ns":frozen["cost"]["wall_ns"],
            "status":"UNVERIFIED_INDEPENDENCE_NOT_SOTA"}
    return {"schema":1,"suite":"development","scenario":sc["id"],"seed":record["seed"],
            "source_revision":record["source_revision"],
            "binary_sha256":record["binary_sha256"],
            "policies":conclusions,
            "external_objective":"absolute_matched_log_speed_ratio_change",
            "same_native_workers":True,
            "full_system_cost_comparable":False,
            "confirmation_independence_certified":False,
            "real_algorithm_ground_truth_known":False,
            "sota_claim":False}


def main():
    p=argparse.ArgumentParser()
    p.add_argument("--binary",type=Path)
    p.add_argument("--out",required=True,type=Path)
    p.add_argument("--scenario",choices=("sort_ascending","topk_uniform"))
    p.add_argument("--seed",type=int)
    p.add_argument("--verify-only",action="store_true")
    a=p.parse_args()
    root=a.out.resolve()
    if a.verify_only:
        score=verify(root)
        require(read(root/"scorecard.json")==score,"raw real-worker scorecard changed")
        print(json.dumps({"status":"PASS","verified_real_workers":True,
                          "result":score["policies"],"certified":False},
                         sort_keys=True))
    else:
        require(a.binary is not None and a.scenario is not None and a.seed is not None,
                "real C++ binary, named scenario and predeclared seed are required")
        m,_=manifest()
        sc=next(s for s in m["scenarios"] if s["id"]==a.scenario)
        execute(a.binary.resolve(),root,sc,a.seed,False)
        write(root/"scorecard.json",verify(root))
        print("PASS 3 exact native policies/external adapted samplers and fresh-seed real-worker confirmation")


if __name__=="__main__":
    main()
