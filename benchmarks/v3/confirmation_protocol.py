#!/usr/bin/env python3
"""Independent V3-01d discovery selection + schema-16 holdout audit.

No synthetic truth is used to choose hypotheses; no evaluation data is opened.
A sign-tail rejection is *conditional on unverified independence assumptions*,
never a certified wall-time discovery, FWER bound or SOTA claim.
"""
from __future__ import annotations

import hashlib
import json
import math
from pathlib import Path
import sqlite3

from protocol import sha256_file, sqlite_snapshot_sha256
from run_paired_development import decode

MIN_RATIO = 1.2
ALPHA = .05
MAX_MEMBERS = 2
SEED_PAIRS = 20
SIGN_MODEL = "independent_seed_signs_declared"

def require(ok, message):
    if not ok:
        raise ValueError(message)

def read(path):
    return json.loads(Path(path).read_text())

def db_ro(path):
    return sqlite3.connect(Path(path).resolve().as_uri()+"?mode=ro",uri=True)

def nominate(source_db, eligible):
    """Freeze two highest-|effect| distinct *discovery* edges, ignoring truth."""
    if not eligible:
        return []
    with db_ro(source_db) as db:
        db.execute("PRAGMA query_only=ON")
        data=db.execute("""SELECT p.pair_number,p.status,p.log_contrast,
                          d.axis,d.left_coords,d.right_coords
                          FROM v3_pairs p JOIN v3_conditional_decisions d
                          USING(pair_number) ORDER BY p.pair_number""").fetchall()
    choices={}
    for i,status,effect,axis,left,right in data:
        require(type(i) is int and axis=="x" and status in ("ok","censored"),
                "unexpected discovery row/axis")
        require((effect is None)==(status=="censored"),"censored effect promotion")
        if effect is None:
            continue
        require(math.isfinite(effect),"nonfinite discovery contrast")
        if abs(effect)<math.log(MIN_RATIO):
            continue
        l,r=decode(left),decode(right)
        require(set(l)==set(r) and all(l[x]==r[x] for x in l if x!="x")
                and int(l["x"])<int(r["x"]),"invalid structural discovery")
        k=(left,right)
        cand={"discovery_pair":i,"direction":1 if effect>0 else -1}
        score=(-abs(effect),i)
        if k not in choices or score<choices[k][0]:
            choices[k]=(score,cand)
    return [item[1] for item in sorted(choices.values(),key=lambda x:x[0])[:MAX_MEMBERS]]

def family_manifest(scenario,policy,seed,choices):
    require(choices and len(choices)<=MAX_MEMBERS,"family needs candidates")
    require(all(type(c["discovery_pair"]) is int and
                c["direction"] in (-1,1) for c in choices),"bad choices")
    return {"family_id":"v3d-"+scenario[:30]+"-"+("v14" if policy=="conditional-tree" else "v15")+"-"+str(seed),
            "seed_pairs":SEED_PAIRS,"alpha":ALPHA,"sign_model":SIGN_MODEL,
            "members":choices}

def sign_tail(n,successes):
    require(0<=successes<=n and n==SEED_PAIRS,"invalid binomial support")
    return sum(math.comb(n,k) for k in range(successes,n+1))/2**n

def adjusted_pvalues(raw):
    order=sorted(range(len(raw)),key=lambda i:(raw[i],i))
    out=[1.]*len(raw)
    running=0.
    for rank,index in enumerate(order):
        running=max(running,min(1.,raw[index]*(len(raw)-rank)))
        out[index]=running
    return out

def independent_sidecar(source_db,confirmation_db,frozen,report):
    """Recompute every sealed pair effect and Holm outcome from authoritative
    raw sample counts, without using the CLI's computed significance claims.
    """
    require(report["cohort"]=="confirmation" and
            report["complete"] is True and
            report["confirmed"] is False and
            report["assumptions_empirically_validated"] is False,
            "false scientific status")
    with db_ro(source_db) as source, db_ro(confirmation_db) as holdout:
        source.execute("PRAGMA query_only=ON")
        holdout.execute("PRAGMA query_only=ON")
        require(holdout.execute("PRAGMA user_version").fetchone()[0]==16,
                "not schema-16 frozen family")
        stored=holdout.execute("""SELECT manifest,specification,spec_sha256,source_revision
                                  FROM v3_family WHERE id=1""").fetchone()
        require(stored is not None,"missing immutable family specification")
        manifest,encoded,digest,revision=stored
        require(hashlib.sha256(encoded.encode()).hexdigest()==digest and
                json.loads(manifest)==frozen and len(revision)==40,
                "changed family specification")
        spec=json.loads(encoded)
        require(spec["sign_model"]==SIGN_MODEL and
                spec["seed_pairs"]==SEED_PAIRS and
                spec["alpha"]==ALPHA and
                spec["source_revision"]==revision and
                spec["multiplicity"]=="holm_all_members",
                "invalid sealed inference model")
        raw_seeds=[s[0] for s in
                   source.execute("SELECT seed FROM v3_pairs ORDER BY pair_number")]
        holdout_seeds=spec["holdout_seeds"]
        n=len(frozen["members"]);planned=n*SEED_PAIRS
        require(len(holdout_seeds)==planned and
                len(set(holdout_seeds))==planned and
                min(holdout_seeds)>=2**62 and max(holdout_seeds)<2**63 and
                not set(holdout_seeds).intersection(raw_seeds),
                "holdout seed disclosure/leak/duplication")
        edges=source.execute("""SELECT p.pair_number,d.left_coords,d.right_coords
            FROM v3_pairs p JOIN v3_conditional_decisions d USING(pair_number)
            ORDER BY p.pair_number""").fetchall()
        rows=holdout.execute("""SELECT pair_number,seed,status,log_contrast,
                                     left_first,left_case_id,right_case_id,
                                     pair_wall_ns,worker_timed_cpu_ns
            FROM v3_pairs ORDER BY pair_number""").fetchall()
        require(len(rows)==planned and report["planned_pairs"]==planned and
                report["committed_pairs"]==planned,
                "partial family/altered planned denominator")
        valid=[0]*n;positive=[0]*n;effects=[[] for _ in range(n)]
        for i,(number,seed,status,effect,left_first,lid,rid,wall,cpu) in enumerate(rows):
            j=i%n
            member=frozen["members"][j]
            base=edges[member["discovery_pair"]]
            require(number==i and seed==holdout_seeds[i] and
                    left_first==int((seed^i)&1==0) and
                    status in ("ok","censored") and wall>0,
                    "frozen pair ordering/seed/status mismatch")
            orig_left,orig_right=decode(base[1]),decode(base[2])
            cases={}
            for case_id in (lid,rid):
                row=holdout.execute("""SELECT family,params,seed,input_sha256,input
                                     FROM v3_cases WHERE case_id=?""",(case_id,)).fetchone()
                require(row is not None and row[0]=="calibration" and
                        row[2]==seed and hashlib.sha256(row[4]).hexdigest()==row[3],
                        "raw case identity, payload or digest mismatch")
                cases[case_id]=json.loads(row[1])
            for source_coords,case_id in ((orig_left,lid),(orig_right,rid)):
                actual=cases[case_id]
                require(all(str(actual[k]).lower()==v for k,v in source_coords.items()),
                        "holdout changed frozen structural edge")
            samples=holdout.execute("""SELECT s.side,s.algorithm,s.status,s.ns,s.batch,
                                             s.cpu_ns,c.seed
                          FROM v3_samples s JOIN v3_cases c USING(case_id)
                          WHERE s.pair_number=? ORDER BY s.side,s.ordinal""",(i,)).fetchall()
            require(len(samples)==2 and [a[0] for a in samples]==[0,1] and
                    all(a[1] in ("calibration","calibration_steps_v3",
                                 "calibration_smooth_v3") and a[6]==seed for a in samples),
                    "holdout raw sample/cardinality/seed mismatch")
            raw_cpu=None if any(s[5] is None for s in samples) else sum(s[5] for s in samples)
            require(raw_cpu==cpu,"holdout CPU differs from raw samples")
            ok=all(s[2]=="ok" and s[3] is not None and s[4] and s[3]>0
                   for s in samples)
            require((status=="ok")==ok and (effect is not None)==ok,
                    "censored measurements promoted")
            if ok:
                actual_delta=math.log(samples[1][3]/samples[1][4])-math.log(samples[0][3]/samples[0][4])
                require(math.isclose(actual_delta,effect,abs_tol=1e-12,rel_tol=1e-12),
                        "independent holdout timing contrast disagrees")
                valid[j]+=1
                positive[j]+=int(member["direction"]*actual_delta>spec["minimum_log_effect"])
                effects[j].append(actual_delta)
            else:
                effects[j].append(None)
        require(holdout.execute("SELECT COUNT(*) FROM v3_cases").fetchone()[0]==2*planned,
                "orphan/missing holdout cases")
        require(holdout.execute("SELECT COUNT(*) FROM v3_samples").fetchone()[0]==2*planned,
                "orphan/missing holdout raw samples")
        raw=[sign_tail(SEED_PAIRS,positive[j]) if valid[j]==SEED_PAIRS else 1. for j in range(n)]
        holm=adjusted_pvalues(raw)
        output=[]
        require(len(report["members"])==n and
                report["family_sha256"]==digest,
                "family verdict inventory/provenance mismatch")
        for j,item in enumerate(report["members"]):
            state=("ABSTAIN_CENSORED" if valid[j]!=SEED_PAIRS else
                   "REJECTED_UNDER_DECLARED_MODEL" if holm[j]<=ALPHA else
                   "NOT_REJECTED")
            require(item["member_index"]==j and item["state"]==state and
                    item["valid_pairs"]==valid[j] and
                    item["positive_pairs"]==positive[j] and
                    math.isclose(item["raw_p"],raw[j],abs_tol=1e-14) and
                    math.isclose(item["holm_p"],holm[j],abs_tol=1e-14),
                    "independent binomial-Holm audit disagrees")
            output.append({"member_index":j,"state":state,
                           "valid":valid[j],"positive":positive[j],
                           "raw_p":raw[j],"holm_p":holm[j]})
        return {"planned_holdout_pairs":planned,"observed_holdout_pairs":len(rows),
                "source_revision":revision,"family_sha256":digest,
                "source_snapshot_sha256":sqlite_snapshot_sha256(source_db),
                "holdout_snapshot_sha256":sqlite_snapshot_sha256(confirmation_db),
                "nominal_rejections":sum(s["state"]=="REJECTED_UNDER_DECLARED_MODEL"
                                         for s in output),
                "members":output,"confirmed":False,
                "sampling_assumptions_empirically_validated":False}
