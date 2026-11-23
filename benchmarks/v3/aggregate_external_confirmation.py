#!/usr/bin/env python3
"""Independent audit and frozen-hypothesis decision for the V3-10 confirmation.

Recomputes every discovery and confirmation effect from the authoritative
native SQLite runs, re-derives each policy's nominations, every exact sign
tail and within-campaign Holm status, then the per-seed-cluster endpoint and
the frozen H1/H2 decisions. Stored runner summaries are only compared, never
trusted. Missing trials make the confirmation INCOMPLETE_NO_CLAIM; any
inconsistency is a hard failure.
"""
from __future__ import annotations
import argparse
import hashlib
import json
import math
from pathlib import Path

from run_external_confirmation import manifest,POLICIES,PROTOCOL,PLAN
from run_real_external import read,require,audit_native_run,config
from protocol import sha256_file

REJECTED="REJECTED_UNDER_UNVERIFIED_SIGN_INDEPENDENCE"
COMPARATORS=(("H1","smac3_hpo"),("H2","optuna_tpe"))


def sign_tail(n,k):
    """P(Binomial(n, 1/2) >= k); 1.0 when no informative cluster exists."""
    require(0<=k<=n<=100,"invalid sign test counts")
    return 1.0 if n==0 else sum(math.comb(n,t) for t in range(k,n+1))/(1<<n)


def holm(raw):
    order=sorted(range(len(raw)),key=lambda i:(raw[i],i))
    adjusted=[1.0]*len(raw);running=0.0
    for rank,index in enumerate(order):
        running=max(running,min(1.0,raw[index]*(len(raw)-rank)))
        adjusted[index]=running
    return adjusted


def nominate(rows,plan):
    """Independent re-implementation of the frozen nomination rule."""
    threshold=math.log(plan["min_practical_ratio"]);best={}
    for index,row in enumerate(rows):
        effect=row["effect"]
        if effect is None or abs(effect)<threshold:
            continue
        edge=(row["left"],row["right"]);rank=(-abs(effect),index)
        if edge not in best or rank<best[edge][0]:
            best[edge]=(rank,{"source_pair":index,"left":edge[0],"right":edge[1],
                               "direction":1 if effect>0 else -1})
    return [item for _,item in sorted(best.values(),key=lambda x:x[0])][:plan["max_distinct_candidates"]]


def decide(clusters,alpha):
    """Frozen H1/H2: one-sided exact sign tests over seed clusters, Holm over both."""
    out=[]
    for hid,comparator in COMPARATORS:
        diffs=[c["R"]["acquisition-v3-coverage"]-c["R"][comparator] for c in clusters]
        positive=sum(d>0 for d in diffs);negative=sum(d<0 for d in diffs)
        out.append({"id":hid,"comparator":comparator,"positive_clusters":positive,
                    "negative_clusters":negative,"ties":len(diffs)-positive-negative,
                    "mean_difference":sum(diffs)/len(diffs),
                    "raw_p":sign_tail(positive+negative,positive)})
    for item,adjusted in zip(out,holm([x["raw_p"] for x in out])):
        item["holm_p"]=adjusted
        item["decision"]="SUPPORTED" if adjusted<=alpha else "NOT_SUPPORTED"
    return out


def audit_policy(home,policy,sc,source,seed,plan):
    disc=read(home/"discovery.json");stored=read(home/"confirmation.json")["results"]
    rows=[]
    if policy=="acquisition-v3-coverage":
        cfg=read(home/"discovery"/"config.json")
        require(cfg==config(sc,source,seed,True),"V18 discovery configuration changed")
        rows=audit_native_run(home/"discovery"/"run",cfg,policy)
    else:
        label="optuna_random" if policy=="smac3_hpo" else "optuna_tpe"
        require(len(disc["proposals"])==plan["pair_budget"],"external proposal transcript incomplete")
        for n,prop in enumerate(disc["proposals"]):
            folder=home/"discovery"/("query_"+str(n).zfill(2))
            cfg=read(folder/"config.json")
            require(cfg==config(sc,source,seed+100_000+1000*n,False),"external query configuration changed")
            one=audit_native_run(folder/"run",cfg,label,(prop["left"],prop["right"]))
            require(len(one)==1,"external query must hold exactly one matched pair")
            rows.append({**one[0],"pair_number":n})
    require(len(rows)==plan["pair_budget"],"discovery budget not exactly consumed")
    for mine,theirs in zip(rows,disc["observations"]):
        require((mine["left"],mine["right"],mine["status"],mine["effect"])==
                (theirs["left"],theirs["right"],theirs["status"],theirs["effect"]),
                "stored discovery observation differs from native evidence")
    chosen=nominate(rows,plan)
    require([(c["left"],c["right"],c["direction"],c["source_pair"]) for c in chosen]==
            [(c["left"],c["right"],c["direction"],c["source_pair"]) for c in disc["chosen"]],
            "nominations differ from the frozen rule applied to native evidence")
    require(len(stored)==len(chosen),"confirmation results do not match nominations")
    used={r["seed"] for r in rows};raw=[];counts=[]
    for j,(item,result) in enumerate(zip(chosen,stored)):
        folder=home/("confirmation_"+str(j))
        cfg=read(folder/"config.json")
        require(cfg==config(sc,source,1_000_000_000+10*seed+j,False),"confirmation configuration changed")
        obs=audit_native_run(folder/"run",cfg,"optuna_random",(item["left"],item["right"]))
        seeds={o["seed"] for o in obs}
        require(len(obs)==plan["confirmation_pairs"]==len(seeds) and not seeds&used,
                "confirmation seeds reused or incomplete")
        used|=seeds
        valid=sum(o["effect"] is not None for o in obs)
        positive=sum(o["effect"] is not None and
                     o["effect"]*item["direction"]>math.log(plan["min_practical_ratio"]) for o in obs)
        p=sign_tail(plan["confirmation_pairs"],positive) if valid==plan["confirmation_pairs"] else 1.0
        require((result["left"],result["right"],result["direction"],result["valid"],result["positive"])==
                (item["left"],item["right"],item["direction"],valid,positive) and
                math.isclose(result["raw_p"],p,rel_tol=1e-12,abs_tol=1e-15),
                "stored confirmation differs from independent recomputation")
        raw.append(p);counts.append(valid)
    statuses=["ABSTAIN_CENSORED" if valid<plan["confirmation_pairs"] else
              REJECTED if adjusted<=plan["alpha"] else "NOT_REJECTED"
              for valid,adjusted in zip(counts,holm(raw))]
    require(statuses==[r["status"] for r in stored],"stored Holm status differs from recomputation")
    return {"rejections":statuses.count(REJECTED),"censored":statuses.count("ABSTAIN_CENSORED"),
            "nominations":len(chosen),"distinct_edges":len({(r["left"],r["right"]) for r in rows}),
            "cost":disc["cost"]}


def aggregate(artifacts,diagnostic):
    plan,fixtures=manifest()
    trials={}
    for path in sorted(artifacts.rglob("trial.json")):
        t=read(path)
        key=(t["scenario"],t["seed"])
        require(key not in trials,"duplicate trial "+str(key))
        trials[key]=(path.parent,t)
    seeds=plan["diagnostic_seeds"] if diagnostic else plan["seeds"]
    expected={(s["id"],seed) for s in plan["scenarios"] for seed in seeds}
    require(set(trials)<=expected,"undeclared trial present: "+str(sorted(set(trials)-expected)))
    binaries={t["binary_sha256"] for _,t in trials.values()}
    require(len(binaries)<=1,"trials used different native binaries")
    by_scenario={s["id"]:s for s in plan["scenarios"]}
    audited={};hashes={}
    for key,(folder,t) in sorted(trials.items()):
        require(t["protocol"]==PROTOCOL and t["suite"]==("diagnostic" if diagnostic else "confirmation") and
                t["manifest_sha256"]==sha256_file(PLAN) and
                t["native_source_commit"]==plan["native_source_commit"] and
                t["smac_version"]==plan["smac_version"] and t["optuna_version"]==plan["optuna_version"],
                "trial provenance differs from the frozen protocol: "+str(key))
        sc=by_scenario[key[0]]
        audited[key]={policy:audit_policy(folder/policy,policy,sc,fixtures[sc["source"]],key[1],plan)
                      for policy in POLICIES}
        for policy in POLICIES:
            require(audited[key][policy]["rejections"]==t["policies"][policy]["rejections"],
                    "trial summary rejection count differs from audit")
        name=f"{key[0]}/{key[1]}"
        hashes[name]={str(f.relative_to(folder)).replace("\\","/"):sha256_file(f)
                      for f in sorted(folder.rglob("*.json")) if f.parent==folder or f.name in
                      ("discovery.json","confirmation.json")}
    missing=sorted(f"{s}/{seed}" for s,seed in expected-set(trials))
    result={"schema":1,"protocol":PROTOCOL,"suite":"diagnostic" if diagnostic else "confirmation",
            "manifest_sha256":sha256_file(PLAN),"native_source_commit":plan["native_source_commit"],
            "binary_sha256":next(iter(binaries),None),"expected_trials":len(expected),
            "observed_trials":len(trials),"missing_trials":missing,"evidence_sha256":hashes,
            "sign_independence_certified":False,"sota_claim":False}
    totals={s["id"]:{p:{"rejections":0,"censored":0,"nominations":0,"wall_ns":0,"child_cpu_ns":0,
                        "harness_cpu_ns":0,"distinct_edges":0} for p in POLICIES} for s in plan["scenarios"]}
    for (scenario,_),arms in audited.items():
        for policy,a in arms.items():
            row=totals[scenario][policy]
            for k in ("rejections","censored","nominations","distinct_edges"):
                row[k]+=a[k]
            for k in ("wall_ns","child_cpu_ns","harness_cpu_ns"):
                row[k]+=a["cost"][k]
    result["descriptive_totals"]=totals
    if diagnostic:
        result["status"]="DIAGNOSTIC_NO_HYPOTHESIS_TEST"
    elif missing:
        result["status"]="INCOMPLETE_NO_CLAIM"
        result["statement"]=plan["decision_rules"]["incomplete"]
    else:
        clusters=[{"seed":seed,"R":{p:sum(audited[(s["id"],seed)][p]["rejections"] for s in plan["scenarios"])
                                    for p in POLICIES}} for seed in plan["seeds"]]
        result["status"]="COMPLETE"
        result["clusters"]=clusters
        result["hypotheses"]=decide(clusters,plan["family_alpha"])
        for h in result["hypotheses"]:
            h["statement"]=plan["decision_rules"]["supported" if h["decision"]=="SUPPORTED" else "not_supported"]
    result["limitations"]=[plan["truth_status"],
        "Sign-test independence across seed clusters is assumed, not certified",
        "Discovery cost is descriptive only: Python optimizer and process startup overhead are not matched",
        "Single CI host class; no hardware-general or multi-host claim"]
    return result


def main():
    cli=argparse.ArgumentParser()
    cli.add_argument("--artifacts",required=True,type=Path)
    cli.add_argument("--out",required=True,type=Path)
    cli.add_argument("--diagnostic",action="store_true")
    cli.add_argument("--verify-only",action="store_true")
    a=cli.parse_args()
    result=aggregate(a.artifacts.resolve(),a.diagnostic)
    text=json.dumps(result,indent=1,sort_keys=True,allow_nan=False)+"\n"
    if a.verify_only:
        require(a.out.read_text()==text,"recorded confirmation scorecard differs from independent recomputation")
        print(json.dumps({"status":"VERIFIED","scorecard_sha256":hashlib.sha256(text.encode()).hexdigest()}))
        return
    require(not a.out.exists(),"refusing to overwrite a confirmation scorecard")
    a.out.write_text(text)
    print(json.dumps({"status":result["status"],
                      "hypotheses":[{k:h[k] for k in ("id","decision","raw_p","holm_p")}
                                    for h in result.get("hypotheses",[])]}))


if __name__=="__main__":
    main()
