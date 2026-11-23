#!/usr/bin/env python3
"""Independent falsification of V3-09 scope-preserving witness minimization.

Two real-worker fixtures: a calibration cliff whose only legal shrink destroys
the boundary (original must be preserved and reproduced), and a std_sort n-axis
boundary whose non-scope value range can shrink (reduced witness must be
reproduced). Without using the engine this test re-derives every fresh seed,
recomputes every side metric, contrast, sign tail and decision from the raw
signed observations, checks scope preservation and shrink monotonicity, links
the parent signed witness byte-for-byte, and verifies Ed25519 with openssl.
Forged, hidden, re-signed and post-hoc altered records must fail for their
asserted reason.
"""
import argparse
import hashlib
import json
import math
import os
from pathlib import Path
import shutil
import sqlite3
import subprocess

MASK64=(1<<64)-1
BASE=1<<60
SIG="signature invalid"
DIFFERS="replayed minimization differs from signed witness"
IDENTITY="signed pair identity differs from deterministic replay"


def need(ok,msg):
    if not ok:
        raise AssertionError(msg)


def run(binary,*args,expect=0,reason=None):
    env=dict(os.environ)
    env.pop("PHASESHIFT_V3_FAILPOINT",None)
    p=subprocess.run([str(binary),*map(str,args)],capture_output=True,text=True,timeout=240,env=env)
    need(p.returncode==expect,f"{args}: rc={p.returncode} expected={expect}\n{p.stdout}\n{p.stderr}")
    reasons=(reason,) if isinstance(reason,str) else reason
    need(reasons is None or any(r in p.stderr for r in reasons),f"{args}: expected {reasons!r}\n{p.stderr}")
    return p.stdout


def openssl(*args):
    p=subprocess.run(["openssl",*map(str,args)],capture_output=True,timeout=60)
    need(p.returncode==0,f"openssl {args}: {p.stderr!r}")
    return p.stdout


def ro(path):
    return sqlite3.connect(path.resolve().as_uri()+"?mode=ro",uri=True)


def snapshot(path):
    with ro(path) as db:
        version=db.execute("PRAGMA user_version").fetchone()[0]
        return hashlib.sha256("\n".join([f"user_version={version}",*db.iterdump()]).encode()).hexdigest()


def splitmix64(state):
    state=(state+0x9e3779b97f4a7c15)&MASK64
    z=state
    z=((z^(z>>30))*0xbf58476d1ce4e5b9)&MASK64
    z=((z^(z>>27))*0x94d049bb133111eb)&MASK64
    return state,z^(z>>31)


def block_seeds(family_sha,member,block,count):
    digest=hashlib.sha256(f"{family_sha}/phaseshift.v3.boundary_minimization.1/{member}/{block}".encode()).hexdigest()
    state=int(digest[:16],16)
    out=[]
    while len(out)<count:
        state,value=splitmix64(state)
        seed=BASE|(value&(BASE-1))
        if seed not in out:
            out.append(seed)
    return out


def side_metric(rows,algorithm):
    if not rows or any(r["status"]!="ok" or r["ns"]<=0 or r["batch"]<=0 for r in rows):
        return None
    values=sorted(r["ns"]/r["batch"] for r in rows if r["algorithm"]==algorithm)
    if not values:
        return None
    mid=len(values)//2
    median=values[mid] if len(values)%2 else (values[mid-1]+values[mid])/2
    return math.log(median) if median>0 else None


def sign_tail(n,k):
    return min(1.,sum(math.comb(n,i) for i in range(k,n+1))/2**n)


SHRINKABLE={"shrink_threshold":{"threshold"},
            "shrink_length":{"n","k","unique_count"},
            "shrink_value_range":{"value_range","unique_count"},
            "shrink_run_length":{"run_length"},
            "shrink_unique_values":{"unique_count"},
            "shrink_topk_output":{"k"},
            "shrink_vertices":{"vertices","community_count","edges","density_ppm"},
            "shrink_edges":{"edges","density_ppm"},
            "shrink_weight_range":{"max_weight"},
            "shrink_communities":{"community_count"}}


def audit(folder,out,parent_dir,public_pem,member_index,budget,algorithm):
    """Engine-independent reconstruction of a signed minimization record."""
    wfile,sig=out/"minimized_witness.json",out/"minimized_witness.sig"
    need(b"Signature Verified Successfully" in openssl("pkeyutl","-verify","-pubin","-inkey",public_pem,"-rawin",
                                                         "-in",wfile,"-sigfile",sig),"openssl rejected minimized witness")
    need(not (out/"STARTED").exists(),"completed record kept STARTED marker")
    doc=json.loads(wfile.read_text())
    e=doc["evidence"]
    parent_bytes=(parent_dir/"witness.json").read_bytes()
    marker=b'"evidence":'
    need(parent_bytes.endswith(b"}\n") and parent_bytes.count(marker)==1,"parent witness layout")
    parent_evidence=parent_bytes[parent_bytes.index(marker)+len(marker):-2]
    need(e["parent_evidence_sha256"]==hashlib.sha256(parent_evidence).hexdigest(),"parent witness linkage")
    parent=json.loads(parent_evidence)
    need(e["member"]==parent["members"][member_index],"member descriptor")
    need(parent["family_report"]["members"][member_index]["state"]=="REJECTED_UNDER_DECLARED_MODEL","unsupported member")
    spec=json.loads(parent["family"]["specification"])
    k,alpha,minimum=spec["seed_pairs"],spec["alpha"],spec["minimum_log_effect"]
    pred=e["predicate"]
    need((pred["seed_pairs_per_evaluation"],pred["alpha"])==(k,alpha) and
         math.isclose(pred["minimum_log_effect"],minimum,rel_tol=0,abs_tol=1e-15),"predicate drift")
    with ro(folder/"run.sqlite") as db:
        discovery_seeds={s for (s,) in db.execute("SELECT seed FROM v3_pairs")}
    holdout=set(spec["holdout_seeds"])
    member=e["member"]
    axis=spec["members"][member_index]["axis"]
    scope=set(e["scope_axes"])
    need(axis in scope and set(member["context"])<=scope,"scope omits frozen axes")
    direction=member["direction"]
    evals=e["evaluations"]
    need([v["block"] for v in evals]==list(range(len(evals))),"noncontiguous blocks")
    roles=[v["role"] for v in evals]
    shrinks=roles.count("SHRINK")
    need(roles==["SHRINK"]*shrinks+["REPRODUCE"]*(len(roles)-shrinks) and shrinks<=budget and
         1<=len(roles)-shrinks<=2,"evaluation roles/budget")
    # Original endpoint params, independently read from the frozen holdout.
    with ro(folder/"confirmation"/"family.sqlite") as db:
        original=json.loads(db.execute("""SELECT c.params FROM v3_pairs p JOIN v3_cases c
            ON c.case_id=p.left_case_id WHERE p.pair_number=0""").fetchone()[0])
    seen=set()
    current=original
    accepted=[]
    for v in evals:
        seeds=block_seeds(parent["family"]["family_sha256"],member_index,v["block"],k)
        need([p["seed"] for p in v["pairs"]]==seeds,"seed derivation")
        need(not (set(seeds)&(seen|discovery_seeds|holdout)) and all(BASE<=s<2*BASE for s in seeds),"seed reuse/leakage")
        seen|=set(seeds)
        left,right=json.loads(v["left_params"]),json.loads(v["right_params"])
        need({key for key in left if left[key]!=right[key]}=={axis},"endpoints differ outside primary axis")
        need(str(left[axis]).lower()==str(member["left"]).lower() and
             str(right[axis]).lower()==str(member["right"]).lower(),"primary edge labels changed")
        for name,label in member["context"].items():
            need(str(left[name]).lower()==str(label).lower(),"context changed")
        valid=positive=0
        for j,p in enumerate(v["pairs"]):
            need(p["left_first"]==(((p["seed"]^j)&1)==0),"presentation order rule")
            lm,rm=side_metric(p["left"],algorithm),side_metric(p["right"],algorithm)
            if lm is None or rm is None:
                need(p["log_contrast"] is None,"censored pair given a contrast")
                continue
            need(math.isclose(p["log_contrast"],rm-lm,rel_tol=1e-12,abs_tol=1e-12),"contrast not reconstructed")
            valid+=1
            positive+=direction*p["log_contrast"]>minimum
        pval=sign_tail(k,positive) if valid==k else 1.
        holds=valid==k and pval<=alpha
        need((v["valid_pairs"],v["positive_pairs"],v["holds"])==(valid,positive,holds) and
             math.isclose(v["p"],pval,rel_tol=1e-12,abs_tol=1e-15),"decision not reconstructed")
        if v["role"]=="SHRINK":
            changed={key for key in left if left[key]!=current[key]}
            need(changed and changed<=SHRINKABLE[v["action"]] and not changed&scope and
                 all(left[key]<current[key] for key in changed if key!="density_ppm"),"illegal shrink step")
            if holds:
                current=left
                accepted.append(left)
    outcome=e["outcome"]
    reproduce=[v for v in evals if v["role"]=="REPRODUCE"]
    state=("NOT_REPRODUCED" if not any(v["holds"] for v in reproduce) else
           "REDUCED_REPRODUCED" if outcome["reduced"] and reproduce[0]["holds"] else "ORIGINAL_REPRODUCED")
    need(outcome["state"]==state and outcome["reduced"]==bool(accepted),"outcome state")
    if accepted:
        need(json.loads(reproduce[0]["left_params"])==accepted[-1],"reproduced witness is not last accepted shrink")
    if reproduce[-1]["action"]=="reproduce_original":
        need(json.loads(reproduce[-1]["left_params"])==original,"original witness altered")
    final=next((i for i,v in enumerate(evals) if v["role"]=="REPRODUCE" and v["holds"]),None)
    need(outcome["final_evaluation"]==final,"final evaluation index")
    need(not e["claim_limits"]["confirmed"] and not e["claim_limits"]["hardware_general"],"claim limits")
    return doc


def fixture(binary,root,name,cfg,manifest,direction=1):
    path=root/f"{name}.json"
    path.write_text(json.dumps(cfg))
    folder=root/name
    run(binary,"conditional-tree","--config",path,"--out",folder,"--pairs","6")
    man=root/f"{name}-manifest.json"
    man.write_text(json.dumps({**manifest,"members":[{"discovery_pair":0,"direction":direction}]}))
    run(binary,"family-freeze-journaled","--run",folder,"--manifest",man)
    return folder


def main():
    p=argparse.ArgumentParser()
    p.add_argument("--binary",required=True,type=Path)
    p.add_argument("--config",required=True,type=Path)
    p.add_argument("--out",required=True,type=Path)
    a=p.parse_args()
    binary=a.binary.resolve()
    root=a.out.resolve()
    shutil.rmtree(root,ignore_errors=True)
    root.mkdir(parents=True)
    key,pub,other_pub=root/"key.pem",root/"pub.pem",root/"other_pub.pem"
    openssl("genpkey","-algorithm","ed25519","-out",key)
    openssl("pkey","-in",key,"-pubout","-out",pub)
    openssl("genpkey","-algorithm","ed25519","-out",root/"other.pem")
    openssl("pkey","-in",root/"other.pem","-pubout","-out",other_pub)

    base=json.loads(a.config.read_text())
    base["search"].pop("admissible_subspaces",None)
    base["search"]["max_cases"]=12
    cal={**base,"axes":{"x":{"values":[1,2]},"negative":{"values":[False]}},"fixed":{"threshold":2}}
    srt={**base,"experiment_id":"v3-minimize-sort","problem_family":"integer_array","algorithms":["std_sort"],
         "primary_axis":"n","axes":{"n":{"values":[64,256]},"distribution":{"values":["uniform"]}},
         "fixed":{"value_range":1000000}}
    manifest={"family_id":"minimize-test","seed_pairs":20,"alpha":.05,"sign_model":"independent_seed_signs_declared"}
    cal_run=fixture(binary,root,"cal",cal,manifest)
    # Gate: no minimization of an incomplete family; nothing reserved on refusal.
    run(binary,"witness-minimize","--run",cal_run,"--member","0","--key",key,"--out",root/"early",
        expect=2,reason="complete frozen holdout")
    need(not (root/"early").exists(),"refused minimization reserved output")
    run(binary,"family-confirm","--run",cal_run)
    srt_run=fixture(binary,root,"sort",srt,manifest)
    run(binary,"family-confirm","--run",srt_run)
    wrong=fixture(binary,root,"wrongdir",cal,manifest,direction=-1)
    run(binary,"family-confirm","--run",wrong)
    run(binary,"witness-minimize","--run",wrong,"--member","0","--key",key,"--out",root/"w-unsupported",
        expect=2,reason="holdout-rejected family member")
    for args,reason in ((("--member","1"),"outside frozen family"),(("--member","0","--budget","33"),"--budget"),
                        (("--member","-1"),"invalid --member")):
        run(binary,"witness-minimize","--run",cal_run,*args,"--key",key,"--out",root/"w-bad",expect=2,reason=reason)
    need(not (root/"w-unsupported").exists() and not (root/"w-bad").exists(),"refusal reserved output")

    results={}
    for name,folder,budget,algorithm in (("cal",cal_run,8,"calibration"),("sort",srt_run,6,"std_sort")):
        parent=root/f"{name}-parent"
        run(binary,"witness-export","--run",folder,"--key",key,"--out",parent)
        before=(snapshot(folder/"run.sqlite"),snapshot(folder/"confirmation"/"family.sqlite"))
        out=root/f"{name}-min"
        args=["--budget",str(budget)] if budget!=8 else []
        summary=json.loads(run(binary,"witness-minimize","--run",folder,"--member","0","--key",key,"--out",out,*args))
        run(binary,"witness-minimize","--run",folder,"--member","0","--key",key,"--out",out,expect=2,reason="immutable export")
        doc=audit(folder,out,parent,pub,0,budget,algorithm)
        need(summary["state"]==doc["evidence"]["outcome"]["state"],"summary state")
        verified=json.loads(run(binary,"witness-verify-minimized","--run",folder,"--witness",out,
                                "--public-key",pub,"--reproduce","true"))
        need(verified["verified"] and verified["state"]==summary["state"],"genuine minimized witness rejected")
        rep=verified["independent_reproduction"]
        need(rep["block"]==8192 and rep["valid_pairs"]==20 and rep["holds"],
             f"{name}: verifier fresh-seed reproduction failed {rep}")
        need((snapshot(folder/"run.sqlite"),snapshot(folder/"confirmation"/"family.sqlite"))==before,
             "minimization/verification mutated evidence")
        results[name]=(folder,out,doc)

    cal_doc=results["cal"][2]["evidence"]
    need(cal_doc["outcome"]["state"]=="ORIGINAL_REPRODUCED" and not cal_doc["outcome"]["reduced"],"cal preserved original")
    shrink=[v for v in cal_doc["evaluations"] if v["role"]=="SHRINK"]
    need(len(shrink)==1 and shrink[0]["action"]=="shrink_threshold" and not shrink[0]["holds"],
         "destroying shrink must be recorded as a failed attempt")
    srt_doc=results["sort"][2]["evidence"]
    need(srt_doc["outcome"]["reduced"] and srt_doc["outcome"]["state"] in ("REDUCED_REPRODUCED","ORIGINAL_REPRODUCED"),
         "sort reduction")
    final_params=json.loads(srt_doc["outcome"]["final_left_params"]) if srt_doc["outcome"]["state"]=="REDUCED_REPRODUCED" else None
    need(final_params is None or final_params["value_range"]<1000000,"reduced witness not smaller")

    # Deterministic design: a re-run reproduces every seed, case identity and
    # (for fixed outcomes) every decision; timings legitimately differ.
    again=root/"cal-min-again"
    run(binary,"witness-minimize","--run",cal_run,"--member","0","--key",key,"--out",again)
    redo=json.loads((again/"minimized_witness.json").read_text())["evidence"]
    ident=lambda doc:[[(p["seed"],p["left_case_id"],p["right_case_id"]) for p in v["pairs"]] for v in doc["evaluations"]]
    need(ident(redo)==ident(cal_doc) and redo["outcome"]==cal_doc["outcome"],"nondeterministic minimization design")

    srt_run,srt_out,_=results["sort"]
    def forged(name,reason,mutate=None,resign=True,extra=None,run_dir=srt_run,trusted=pub):
        d=root/name
        shutil.copytree(srt_out,d)
        if mutate:
            doc=json.loads((d/"minimized_witness.json").read_text())
            mutate(doc["evidence"])
            (d/"minimized_witness.json").write_text(json.dumps(doc)+"\n")
            if resign:
                openssl("pkeyutl","-sign","-inkey",key,"-rawin","-in",d/"minimized_witness.json",
                        "-out",d/"minimized_witness.sig")
        if extra:
            extra(d)
        run(binary,"witness-verify-minimized","--run",run_dir,"--witness",d,"--public-key",trusted,expect=2,reason=reason)

    def flip_holds(e): e["evaluations"][0]["holds"]=not e["evaluations"][0]["holds"]
    def hide_first(e):
        e["evaluations"].pop(0)
        for i,v in enumerate(e["evaluations"]):
            v["block"]=i
    def raw_ns(e): e["evaluations"][0]["pairs"][0]["left"][0]["ns"]+=1
    def seed(e): e["evaluations"][0]["pairs"][0]["seed"]+=1
    def budget0(e): e["shrink_budget"]=0
    def promote(e): e["claim_limits"]["confirmed"]=True
    def drop_pair(e): e["evaluations"][-1]["pairs"].pop()
    forged("min-unsigned-edit",SIG,flip_holds,resign=False)
    forged("min-resigned-flip",DIFFERS,flip_holds)
    forged("min-resigned-hide-attempt",(IDENTITY,DIFFERS),hide_first)
    forged("min-resigned-raw",DIFFERS,raw_ns)
    forged("min-resigned-seed",IDENTITY,seed)
    # Budget 0 makes the replayed driver reproduce the original at block 0,
    # which conflicts with the signed block-0 shrink identities.
    forged("min-resigned-budget",(IDENTITY,DIFFERS),budget0)
    forged("min-resigned-claim",DIFFERS,promote)
    forged("min-resigned-drop-pair",("signed evaluation lacks pair",DIFFERS),drop_pair)
    forged("min-wrong-trusted-key",SIG,trusted=other_pub)
    forged("min-interrupted","interrupted",extra=lambda d:(d/"STARTED").write_text("x"))
    forged("min-cross-run",("v3 pairs:",),run_dir=results["cal"][0])
    broken=root/"sort-altered-holdout"
    shutil.copytree(srt_run,broken)
    with sqlite3.connect(broken/"confirmation"/"family.sqlite") as db:
        db.execute("UPDATE v3_samples SET cpu_ns=COALESCE(cpu_ns,0)+1 WHERE pair_number=2")
    run(binary,"witness-verify-minimized","--run",broken,"--witness",srt_out,"--public-key",pub,
        expect=2,reason="holdout effect/status/CPU replay mismatch")
    print("PASS witness minimization: scope-preserving shrinks, fresh disjoint seed blocks re-derived, raw "
          "decisions reconstructed, original preserved when shrink destroys boundary, reduced witness reproduced, "
          "verifier fresh-seed reproduction, forged/hidden/re-signed/altered records rejected")


if __name__=="__main__":
    main()
