#!/usr/bin/env python3
"""Falsify the opt-in schema-19 journaled holdout family (V3-09).

Every holdout worker launch must be durably journaled before measurement, so a
killed and re-measured holdout pair is visible rather than silently re-rolled.
Real SIGSTOP/SIGKILL, writer exclusion, three commit-boundary exits, an
independent journal/schedule reconstruction, tamper and downgrade attacks, and
an unchanged legacy schema-16 sidecar are checked against the real worker CLI.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import signal
import sqlite3
import subprocess
import time


def need(ok,msg):
    if not ok:
        raise AssertionError(msg)


def run(binary,*args,expect=0,fault=None):
    env=dict(os.environ)
    env.pop("PHASESHIFT_V3_FAILPOINT",None)
    if fault is not None:
        env["PHASESHIFT_V3_FAILPOINT"]=fault
    p=subprocess.run([str(binary),*map(str,args)],capture_output=True,text=True,timeout=120,env=env)
    need(p.returncode==expect,f"{args}: rc={p.returncode} expected={expect}\n{p.stdout}\n{p.stderr}")
    return p.stdout


def ro(path):
    return sqlite3.connect(path.resolve().as_uri()+"?mode=ro",uri=True)


def snapshot(path):
    # iterdump() omits PRAGMA user_version; include it so a downgrade is visible.
    with ro(path) as db:
        version=db.execute("PRAGMA user_version").fetchone()[0]
        return hashlib.sha256("\n".join([f"user_version={version}",*db.iterdump()]).encode()).hexdigest()


def write_json(path,doc):
    path.write_text(json.dumps(doc))
    return path


def journal(folder,states,pairs):
    """Independent reconstruction of the journal against the frozen schedule."""
    with ro(folder/"confirmation"/"family.sqlite") as db:
        need(db.execute("PRAGMA user_version").fetchone()[0]==19,"journaled schema")
        need(db.execute("SELECT protocol FROM v3_holdout_journal_origin").fetchall()==
             [("phaseshift.v3.holdout_attempt_journal.1",)],"journal provenance")
        spec=json.loads(db.execute("SELECT specification FROM v3_family").fetchone()[0])
        seeds=spec["holdout_seeds"]
        rows=db.execute("""SELECT attempt_id,pair_number,seed,left_first,left_case_id,right_case_id,
            state,committed_pair FROM v3_holdout_attempts ORDER BY attempt_id""").fetchall()
        committed=db.execute("""SELECT pair_number,seed,left_first,left_case_id,right_case_id
            FROM v3_pairs ORDER BY pair_number""").fetchall()
        need(db.execute("PRAGMA foreign_key_check").fetchall()==[],"journal foreign key")
        need(db.execute("SELECT COUNT(*) FROM v3_samples").fetchone()[0]==2*len(committed),"partial raw transaction")
    need([r[6] for r in rows]==states,f"journal states {[r[6] for r in rows]} != {states}")
    need(len(committed)==pairs,"committed holdout pair count")
    index=0
    identity={}
    for i,(aid,pair,seed,order,left,right,state,link) in enumerate(rows):
        need(aid==i and pair==index,"attempt order")
        need(seed==seeds[pair] and order==int((seed^pair)&1==0),"attempt seed/order not frozen")
        need(identity.setdefault(pair,(left,right))==(left,right),"retry changed case identity")
        if state=="COMMITTED":
            need(link==pair and committed[pair]==(pair,seed,order,left,right),"attempt/pair binding")
            index+=1
        else:
            need(link is None,"uncommitted attempt linked to evidence")
    need(index==pairs,"committed pair without journaled attempt")
    return rows


def report(binary,folder):
    sidecar=folder/"confirmation"/"family.sqlite"
    before=snapshot(sidecar)
    out=json.loads(run(binary,"family-report","--run",folder))
    need(snapshot(sidecar)==before,"read-only report mutated journal")
    need(not out["confirmed"] and not out["assumptions_empirically_validated"],"unsupported claim")
    return out


def disclosed(out,attempts,abandoned,started,interrupted,integrity):
    need(out["schema"]=="phaseshift.v3.fixed_edge_family_journaled.1" and
         out["holdout_attempt_protocol"]=="phaseshift.v3.holdout_attempt_journal.1","journal schema")
    got=(out["holdout_attempts"],out["abandoned_attempts"],out["started_attempts"],
         out["pairs_with_abandoned_attempts"],out["attempt_integrity"])
    need(got==(attempts,abandoned,started,interrupted,integrity),f"disclosure {got}")
    need(out["committed_attempts"]==out["committed_pairs"],"committed attempts")


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
    cfg=json.loads(a.config.read_text())
    cfg["axes"]={"x":{"values":[0,1]},"negative":{"values":[False,True]}}
    cfg["fixed"]={"threshold":1}
    cfg["search"]["admissible_subspaces"]=[{"negative":[False]},{"negative":[True]}]
    cfg["search"]["max_cases"]=12
    discovery=root/"discovery"
    run(binary,"conditional-tree","--config",write_json(root/"config.json",cfg),"--out",discovery,"--pairs","6")
    source=snapshot(discovery/"run.sqlite")
    doc={"family_id":"journaled-test","seed_pairs":20,"alpha":.05,
         "sign_model":"independent_seed_signs_declared",
         "members":[{"discovery_pair":0,"direction":1},{"discovery_pair":1,"direction":1}]}
    manifest=write_json(root/"manifest.json",doc)

    # Legacy default is byte-for-byte schema 16 without any journal table.
    legacy=root/"legacy"
    shutil.copytree(discovery,legacy)
    legacy_freeze=json.loads(run(binary,"family-freeze","--run",legacy,"--manifest",manifest))
    need(legacy_freeze["schema"]=="phaseshift.v3.fixed_edge_family.1" and
         "attempt_integrity" not in legacy_freeze,"legacy report changed")
    run(binary,"family-confirm","--run",legacy,"--pause-after","1")
    with ro(legacy/"confirmation"/"family.sqlite") as db:
        need(db.execute("PRAGMA user_version").fetchone()[0]==16,"legacy freeze no longer v16")
        need(db.execute("SELECT COUNT(*) FROM sqlite_master WHERE name LIKE 'v3_holdout%'").fetchone()[0]==0,
             "legacy sidecar gained journal")
        need(db.execute("SELECT COUNT(*) FROM v3_pairs").fetchone()[0]==1,"legacy holdout pair")

    frozen=json.loads(run(binary,"family-freeze-journaled","--run",discovery,"--manifest",manifest))
    disclosed(frozen,0,0,0,0,"CLEAN")
    run(binary,"family-freeze-journaled","--run",discovery,"--manifest",manifest,expect=2)
    run(binary,"family-freeze","--run",discovery,"--manifest",manifest,expect=2)
    with ro(discovery/"confirmation"/"family.sqlite") as db, ro(legacy/"confirmation"/"family.sqlite") as old:
        need(db.execute("SELECT specification FROM v3_family").fetchone()==
             old.execute("SELECT specification FROM v3_family").fetchone(),"journal changed frozen family/seeds")
    journal(discovery,[],0)

    # Real OS kill after the durable reservation and before any worker launch.
    env=dict(os.environ,PHASESHIFT_V3_FAILPOINT="stop_after_holdout_reservation")
    child=subprocess.Popen([str(binary),"family-confirm","--run",str(discovery),"--pause-after","1"],
                           env=env,stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True)
    try:
        deadline=time.monotonic()+60
        while True:
            pid,status=os.waitpid(child.pid,os.WNOHANG|os.WUNTRACED)
            if pid:
                need(os.WIFSTOPPED(status) and os.WSTOPSIG(status)==signal.SIGSTOP,"did not reach reservation")
                break
            need(time.monotonic()<deadline,"reservation stop timeout")
            time.sleep(.01)
        journal(discovery,["STARTED"],0)
        disclosed(report(binary,discovery),1,0,1,0,"INTERRUPTED_ATTEMPT_PENDING")
        # A competing writer must not abandon or duplicate the live attempt.
        run(binary,"family-confirm","--run",discovery,"--pause-after","1",expect=2)
        journal(discovery,["STARTED"],0)
    finally:
        child.kill()
        child.communicate(timeout=30)
    need(child.returncode==-signal.SIGKILL,"real SIGKILL not observed")
    journal(discovery,["STARTED"],0)

    run(binary,"family-confirm","--run",discovery,"--pause-after","1",fault="before_holdout_commit",expect=88)
    journal(discovery,["ABANDONED","STARTED"],0)
    run(binary,"family-confirm","--run",discovery,"--pause-after","1",fault="during_holdout_commit",expect=90)
    journal(discovery,["ABANDONED","ABANDONED","STARTED"],0)
    run(binary,"family-confirm","--run",discovery,"--pause-after","1",fault="after_holdout_commit",expect=89)
    states=["ABANDONED"]*3+["COMMITTED"]
    journal(discovery,states,1)
    partial=report(binary,discovery)
    need(partial["committed_pairs"]==1 and "members" not in partial,"interim p-value leak")
    disclosed(partial,4,3,0,1,"RETRIED_AFTER_INTERRUPTION")

    result=json.loads(run(binary,"family-confirm","--run",discovery))
    states+=["COMMITTED"]*39
    rows=journal(discovery,states,40)
    need(result["complete"] and len(result["members"])==2,"complete family verdict")
    disclosed(result,43,3,0,1,"RETRIED_AFTER_INTERRUPTION")
    need({r[1] for r in rows if r[6]=="ABANDONED"}=={0},"abandoned attempts outside interrupted pair")
    sidecar=discovery/"confirmation"/"family.sqlite"
    sealed=snapshot(sidecar)
    need(report(binary,discovery)==result,"report drift")
    need(json.loads(run(binary,"family-confirm","--run",discovery))==result,"completed resume changed verdict")
    need(snapshot(sidecar)==sealed and snapshot(discovery/"run.sqlite")==source,"replay mutated evidence")

    attacks=(
        ("hide-abandoned","DELETE FROM v3_holdout_attempts WHERE attempt_id=0"),
        ("drop-committed","DELETE FROM v3_holdout_attempts WHERE attempt_id=10"),
        ("uncommit","UPDATE v3_holdout_attempts SET state='ABANDONED',committed_pair=NULL WHERE attempt_id=10"),
        ("alter-seed","UPDATE v3_holdout_attempts SET seed=seed+1 WHERE attempt_id=1"),
        ("alter-order","UPDATE v3_holdout_attempts SET left_first=1-left_first WHERE attempt_id=2"),
        ("alter-case","UPDATE v3_holdout_attempts SET left_case_id=right_case_id,right_case_id=left_case_id WHERE attempt_id=0"),
        ("extra-attempt","INSERT INTO v3_holdout_attempts SELECT 43,39,seed,left_first,left_case_id,right_case_id,'ABANDONED',NULL FROM v3_holdout_attempts WHERE attempt_id=42"),
        ("beyond-schedule","INSERT INTO v3_holdout_attempts VALUES(43,40,1,0,'"+"a"*64+"','"+"b"*64+"','STARTED',NULL)"),
        ("drop-origin","DELETE FROM v3_holdout_journal_origin"),
        ("downgrade","PRAGMA user_version=16"),
        ("drop-journal","DROP INDEX v3_holdout_attempt_active; DROP TABLE v3_holdout_attempts; DROP TABLE v3_holdout_journal_origin; PRAGMA user_version=16"))
    for name,sql in attacks:
        broken=root/name
        shutil.copytree(discovery,broken)
        with sqlite3.connect(broken/"confirmation"/"family.sqlite") as db:
            db.executescript(sql)
        need(name=="drop-journal" or snapshot(broken/"confirmation"/"family.sqlite")!=sealed,f"{name} was a no-op")
        if name=="drop-journal":
            # Fully stripping the journal yields a genuine-looking v16 sidecar;
            # the journal is not a cryptographic log. Record, do not overclaim.
            out=json.loads(run(binary,"family-report","--run",broken))
            need("attempt_integrity" not in out,"stripped journal still disclosed")
            continue
        run(binary,"family-report","--run",broken,expect=2)
    print("PASS journaled holdout: durable reservation, real SIGKILL, writer exclusion, three commit "
          "boundaries, retry disclosure, frozen-schedule replay, tamper/downgrade rejection, legacy v16 unchanged")


if __name__=="__main__":
    main()
