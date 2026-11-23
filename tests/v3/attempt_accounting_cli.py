#!/usr/bin/env python3
"""Crash/lock/replay falsification of opt-in V3-09 attempt accounting."""
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
    p=subprocess.run([str(binary),*map(str,args)],capture_output=True,text=True,timeout=90,env=env)
    need(p.returncode==expect,f"{args}: {p.returncode} != {expect}\n{p.stdout}\n{p.stderr}")
    return p.stdout


def snapshot(dbfile):
    with sqlite3.connect(dbfile.resolve().as_uri()+"?mode=ro",uri=True) as db:
        return hashlib.sha256("\n".join(db.iterdump()).encode()).hexdigest()


def inspect(binary,folder,states,pairs):
    dbfile=folder/"run.sqlite"
    before=snapshot(dbfile)
    report=json.loads(run(binary,"accounting","--run",folder))
    run(binary,"verify","--run",folder)
    need(snapshot(dbfile)==before,"read-only audit mutated evidence")
    with sqlite3.connect(dbfile.resolve().as_uri()+"?mode=ro",uri=True) as db:
        need(db.execute("PRAGMA user_version").fetchone()[0]==17,"wrong schema")
        rows=db.execute("""SELECT attempt_id,pair_number,seed,left_case_id,right_case_id,state,
            committed_pair,elapsed_ns,parent_cpu_ns,waited_child_cpu_ns FROM v3_attempts ORDER BY attempt_id""").fetchall()
        raw=db.execute("""SELECT pair_number,seed,left_case_id,right_case_id,pair_wall_ns,
            worker_timed_cpu_ns FROM v3_pairs ORDER BY pair_number""").fetchall()
        need(len(raw)==pairs and [r[5] for r in rows]==states,"lost/duplicated attempt")
        need(db.execute("SELECT COUNT(*) FROM v3_cases").fetchone()[0]==2*pairs,"partial case transaction")
        need(db.execute("SELECT COUNT(*) FROM v3_samples").fetchone()[0]==2*pairs,"partial raw transaction")
        need(db.execute("PRAGMA foreign_key_check").fetchall()==[],"bad FK")
        origin=db.execute("SELECT executable_sha256,worker_sha256 FROM v3_accounting_origin").fetchone()
        need(origin==(hashlib.sha256(binary.read_bytes()).hexdigest(),
                      hashlib.sha256(binary.with_name("phaseshift_worker").read_bytes()).hexdigest()),"wrong executable seal")
    count=0
    sums=[0,0,0]
    identity=None
    for idx,row in enumerate(rows):
        need(row[0]==idx and row[1]==count,"nonmonotone attempt index")
        current=row[2:5]
        need(identity is None or current==identity,"retry changed the proposal")
        identity=current
        if row[5]=="COMMITTED":
            need(row[6]==count and row[2:5]==raw[count][1:4] and row[7]>=raw[count][4]>0,
                 "unlinked measurement")
            for i,v in enumerate(row[7:10]):
                need(v>=0,"negative measurement")
                sums[i]+=v
            count+=1
            identity=None
        else:
            need(all(v is None for v in row[6:]),"fabricated abandoned or pending cost")
    need(count==pairs,"attempt count mismatch")
    expect={"attempts":len(states),"committed":pairs,"abandoned":states.count("ABANDONED"),
            "started":states.count("STARTED"),"observed_precommit_wall_ns":sums[0],
            "observed_parent_cpu_ns":sums[1],"observed_waited_child_cpu_ns":sums[2],
            "observed_worker_timed_cpu_ns":sum(r[5] for r in raw if r[5] is not None),
            "committed_pairs_without_worker_cpu":sum(r[5] is None for r in raw)}
    need(report["state"]=="ACCOUNTING_INCOMPLETE" and
         all(report[k]==v for k,v in expect.items()),"incorrect accounting report")
    return rows


def main():
    p=argparse.ArgumentParser()
    p.add_argument("--binary",required=True,type=Path)
    p.add_argument("--config",required=True,type=Path)
    p.add_argument("--out",required=True,type=Path)
    a=p.parse_args()
    binary=a.binary.resolve();root=a.out.resolve()
    shutil.rmtree(root,ignore_errors=True)
    root.mkdir(parents=True)
    folder=root/"accounted"
    # Real OS kill after durable attempt reservation, before worker launch.
    env=dict(os.environ,PHASESHIFT_V3_FAILPOINT="stop_after_attempt_reservation")
    child=subprocess.Popen([str(binary),"accounted-v3","--config",str(a.config),
                            "--out",str(folder),"--pairs","6","--pause-after","1"],
                           env=env,stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True)
    try:
        deadline=time.monotonic()+30
        while True:
            pid,status=os.waitpid(child.pid,os.WNOHANG|os.WUNTRACED)
            if pid:
                need(os.WIFSTOPPED(status) and os.WSTOPSIG(status)==signal.SIGSTOP,"did not reach reservation")
                break
            need(time.monotonic()<deadline,"reservation stop timeout")
            time.sleep(.01)
        inspect(binary,folder,["STARTED"],0)
        run(binary,"resume","--run",folder,"--pause-after","1",expect=2)
    finally:
        child.kill()
        child.communicate(timeout=30)
    need(child.returncode==-signal.SIGKILL,"real SIGKILL not observed")
    inspect(binary,folder,["STARTED"],0)
    run(binary,"resume","--run",folder,"--pause-after","1",fault="before_pair_commit",expect=88)
    inspect(binary,folder,["ABANDONED","STARTED"],0)
    run(binary,"resume","--run",folder,"--pause-after","1",fault="during_pair_commit",expect=90)
    inspect(binary,folder,["ABANDONED","ABANDONED","STARTED"],0)
    run(binary,"resume","--run",folder,"--pause-after","1",fault="after_pair_commit",expect=89)
    states=["ABANDONED"]*3+["COMMITTED"]
    inspect(binary,folder,states,1)
    run(binary,"resume","--run",folder)
    states+=["COMMITTED"]*5
    inspect(binary,folder,states,6)
    old=snapshot(folder/"run.sqlite")
    run(binary,"resume","--run",folder)
    need(snapshot(folder/"run.sqlite")==old,"completed resume changed evidence")
    # Existing v15 remains readable; new account command does not invent old costs.
    legacy=root/"legacy"
    run(binary,"acquisition-v3","--config",a.config,"--out",legacy,"--pairs","2")
    run(binary,"verify","--run",legacy)
    run(binary,"accounting","--run",legacy,expect=2)
    # Cross-check against the independent source-snapshot seal in schema 16.
    with sqlite3.connect(folder/"run.sqlite") as db:
        pair=db.execute("SELECT pair_number FROM v3_pairs WHERE status='ok' ORDER BY pair_number LIMIT 1").fetchone()
    need(pair is not None,"valid discovery missing")
    manifest=root/"family.json"
    manifest.write_text(json.dumps({"family_id":"accounted-family","seed_pairs":20,"alpha":.05,
        "sign_model":"unverified","members":[{"discovery_pair":pair[0],"direction":1}]}))
    run(binary,"family-freeze","--run",folder,"--manifest",manifest)
    run(binary,"family-report","--run",folder)
    attacks=[
        ("delete-attempt","DELETE FROM v3_attempts WHERE attempt_id=0","verify"),
        ("changed-proposal","UPDATE v3_attempts SET seed=seed+1 WHERE attempt_id=0","verify"),
        ("missing-origin","DELETE FROM v3_accounting_origin","verify"),
        ("wrong-binary","UPDATE v3_accounting_origin SET executable_sha256='0000000000000000000000000000000000000000000000000000000000000000'","resume"),
        ("changed-counter","UPDATE v3_attempts SET parent_cpu_ns=parent_cpu_ns+1 WHERE state='COMMITTED'","family-report"),
        ("lost-result","DELETE FROM v3_attempts WHERE attempt_id=8","verify"),
    ]
    for name,sql,command in attacks:
        broken=root/name
        shutil.copytree(folder,broken)
        with sqlite3.connect(broken/"run.sqlite") as db:
            db.execute(sql)
        run(binary,command,"--run",broken,expect=2)
    print("PASS real SIGKILL, writer exclusion, transactional crash recovery, attempt accounting, legacy and source seal")


if __name__=="__main__":
    main()
