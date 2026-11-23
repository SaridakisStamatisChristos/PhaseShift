#!/usr/bin/env python3
"""V3-09 storage fault matrix for the v3 SQLite writers, with real processes.

S1  write-limit sweep (RLIMIT_FSIZE, SIGXFSZ ignored) on fresh accounted runs
S2  write-limit sweep on the journaled holdout sidecar
S3  non-cooperating external SQLite write lock; long concurrent reader
S4  page corruption and truncation of the run and sidecar databases

Invariants: never die by a signal; fail closed with exit 2; never leave a
partial pair or an inconsistent attempt journal; read-only commands never
write; a command may report success on a corrupted file only if the logical
content is unchanged; recovery succeeds once the fault is removed.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import resource
import shutil
import signal
import sqlite3
import subprocess
import tempfile
import threading
import time


def need(ok,msg):
    if not ok:
        raise AssertionError(msg)


def run(binary,*args,expect=(0,),fsize=None,timeout=180):
    def limit():
        signal.signal(signal.SIGXFSZ,signal.SIG_IGN)
        resource.setrlimit(resource.RLIMIT_FSIZE,(fsize,fsize))
    env=dict(os.environ)
    for key in ("PHASESHIFT_V3_FAILPOINT","PHASESHIFT_WORKER_FAULT","PHASESHIFT_WORKER_FAULT_PIDFILE"):
        env.pop(key,None)
    with tempfile.TemporaryFile("w+") as out,tempfile.TemporaryFile("w+") as err:
        p=subprocess.run([str(binary),*map(str,args)],stdout=out,stderr=err,timeout=timeout,env=env,
                         preexec_fn=limit if fsize is not None else None)
        out.seek(0);err.seek(0)
        p.stdout,p.stderr=out.read(),err.read()
    need(p.returncode>=0,f"{args}: died by signal {-p.returncode}\n{p.stderr}")
    need(p.returncode in expect,f"{args}: rc={p.returncode} expected {expect}\n{p.stdout}\n{p.stderr}")
    if p.returncode==2:
        need("PHASESHIFT v3 error" in p.stderr,f"{args}: unstructured failure\n{p.stderr}")
    return p


def ro(path):
    return sqlite3.connect(path.resolve().as_uri()+"?mode=ro",uri=True)


def files(folder):
    return {p.name:p.read_bytes() for p in sorted(folder.glob("*.sqlite*"))}


def logical(path):
    # Dump a private copy (with any WAL) so the oracle never touches evidence.
    with tempfile.TemporaryDirectory() as scratch:
        for part in path.parent.glob(path.name+"*"):
            shutil.copy(part,Path(scratch)/part.name)
        try:
            db=sqlite3.connect(str(Path(scratch)/path.name))
            try:
                version=db.execute("PRAGMA user_version").fetchone()[0]
                return hashlib.sha256("\n".join([f"user_version={version}",*db.iterdump()]).encode()).hexdigest()
            finally:
                db.close()
        except sqlite3.DatabaseError as e:
            return f"unreadable:{type(e).__name__}"


def integrity(path):
    """Independent structural oracle on a private copy (covers indexes)."""
    with tempfile.TemporaryDirectory() as scratch:
        for part in path.parent.glob(path.name+"*"):
            shutil.copy(part,Path(scratch)/part.name)
        try:
            db=sqlite3.connect(str(Path(scratch)/path.name))
            try:
                return [r[0] for r in db.execute("PRAGMA integrity_check")]
            finally:
                db.close()
        except sqlite3.DatabaseError as e:
            return [f"unreadable:{e}"]


def discovery_state(folder):
    """Independent consistency audit; None when no plan was ever committed."""
    try:
        with ro(folder/"run.sqlite") as db:
            if db.execute("SELECT COUNT(*) FROM sqlite_master WHERE name='v3_plan'").fetchone()[0]==0:
                return None
            if db.execute("SELECT COUNT(*) FROM v3_plan").fetchone()[0]==0:
                return None
            pairs=[r[0] for r in db.execute("SELECT pair_number FROM v3_pairs ORDER BY pair_number")]
            samples=db.execute("SELECT COUNT(*) FROM v3_samples").fetchone()[0]
            orphans=db.execute("""SELECT COUNT(*) FROM v3_cases c WHERE NOT EXISTS(SELECT 1 FROM v3_pairs p
                WHERE p.left_case_id=c.case_id OR p.right_case_id=c.case_id)""").fetchone()[0]
            states=[s for (s,) in db.execute("SELECT state FROM v3_attempts ORDER BY attempt_id")]
            committed=db.execute("SELECT COUNT(*) FROM v3_attempts WHERE state='COMMITTED'").fetchone()[0]
            fk=db.execute("PRAGMA foreign_key_check").fetchall()
    except sqlite3.DatabaseError as e:
        raise AssertionError(f"{folder}: unreadable after fault: {e}")
    need(pairs==list(range(len(pairs))),"noncontiguous pairs")
    need(samples==2*len(pairs) and orphans==0 and not fk,"partial pair transaction survived a write fault")
    need(committed==len(pairs),f"committed attempts {committed} != pairs {len(pairs)}")
    need("STARTED" not in states or (states.count("STARTED")==1 and states[-1]=="STARTED"),
         f"inconsistent attempt journal {states}")
    return len(pairs),states


def hold_lock(path,seconds,ready,mode):
    db=sqlite3.connect(str(path),timeout=0,isolation_level=None)
    try:
        if mode=="write":
            db.execute("BEGIN IMMEDIATE")
        else:
            db.execute("BEGIN")
            db.execute("SELECT COUNT(*) FROM v3_pairs").fetchone()
        ready.set()
        time.sleep(seconds)
        db.execute("ROLLBACK")
    finally:
        db.close()


def with_lock(path,mode,seconds,action):
    ready=threading.Event()
    t=threading.Thread(target=hold_lock,args=(path,seconds,ready,mode))
    t.start()
    need(ready.wait(10),"lock holder failed to start")
    try:
        return action()
    finally:
        t.join()


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
    config=root/"config.json"
    config.write_text(json.dumps(cfg))
    pairs=4

    # S1: write-limit sweep on fresh accounted discovery runs.
    outcomes=[]
    for limit in (2048,8192,32768,65536,98304,131072,196608,262144,524288,2097152):
        folder=root/f"S1-{limit}"
        r=run(binary,"accounted-v3","--config",config,"--out",folder,"--pairs",str(pairs),expect=(0,2),fsize=limit)
        state=discovery_state(folder) if (folder/"run.sqlite").exists() else None
        if state is None:
            need(r.returncode==2,"run without a committed plan reported success")
            if (folder/"run.sqlite").exists():
                run(binary,"verify","--run",folder,expect=(2,))
            outcomes.append((limit,"NO_PLAN"))
            continue
        committed,states=state
        before=files(folder)
        if committed:
            run(binary,"verify","--run",folder)
            run(binary,"accounting","--run",folder)
        need(files(folder)==before,"read-only audit wrote to a faulted run")
        need((r.returncode==0)==(committed==pairs),"write-fault exit status contradicts committed pairs")
        run(binary,"resume","--run",folder)
        recovered,final_states=discovery_state(folder)
        need(recovered==pairs,"recovery after write fault incomplete")
        need(final_states.count("ABANDONED")==states.count("STARTED")+states.count("ABANDONED"),
             "interrupted reservation not disclosed after recovery")
        run(binary,"verify","--run",folder)
        outcomes.append((limit,"OK" if r.returncode==0 else f"FAILED_AT_{committed}"))
    print("S1",outcomes)
    need(any(o.startswith("FAILED_AT_") for _,o in outcomes),"sweep never failed after the plan committed")
    need(any(o=="OK" for _,o in outcomes),"sweep never completed; limits uninformative")

    # S2: write-limit sweep on the journaled holdout sidecar.
    source=root/"S2-source"
    run(binary,"accounted-v3","--config",config,"--out",source,"--pairs",str(pairs))
    manifest=root/"manifest.json"
    manifest.write_text(json.dumps({"family_id":"storage","seed_pairs":20,"alpha":.05,
        "sign_model":"independent_seed_signs_declared","members":[{"discovery_pair":0,"direction":1}]}))
    run(binary,"family-freeze-journaled","--run",source,"--manifest",manifest)
    sidecar_outcomes=[]
    for limit in (40960,65536,98304,131072,196608,262144,524288,2097152):
        folder=root/f"S2-{limit}"
        shutil.copytree(source,folder)
        run_before=files(folder)
        r=run(binary,"family-confirm","--run",folder,expect=(0,2),fsize=limit)
        need(files(folder)==run_before,"holdout confirm wrote to the discovery run")
        report=json.loads(run(binary,"family-report","--run",folder).stdout)
        need(report["committed_attempts"]==report["committed_pairs"] and report["started_attempts"]<=1,
             "holdout journal inconsistent after write fault")
        need(r.returncode==0 or not report["complete"],"holdout write fault reported success incompletely")
        final=json.loads(run(binary,"family-confirm","--run",folder).stdout)
        need(final["complete"] and final["committed_pairs"]==20,"holdout recovery incomplete")
        need(final["abandoned_attempts"]==report["abandoned_attempts"]+report["started_attempts"],
             "holdout interruption not disclosed after recovery")
        sidecar_outcomes.append((limit,"OK" if r.returncode==0 else f"FAILED_AT_{report['committed_pairs']}"))
    print("S2",sidecar_outcomes)
    need(any(o.startswith("FAILED_AT_") for _,o in sidecar_outcomes),"sidecar sweep never failed mid-run")

    # S3: non-cooperating external write lock, then a long concurrent reader.
    locked=root/"S3"
    run(binary,"accounted-v3","--config",config,"--out",locked,"--pairs",str(pairs),"--pause-after","1")
    snapshot=logical(locked/"run.sqlite")
    started=time.monotonic()
    r=with_lock(locked/"run.sqlite","write",9,lambda:run(binary,"resume","--run",locked,"--pause-after","1",expect=(2,)))
    need("locked" in r.stderr or "busy" in r.stderr,f"lock failure not reported as busy\n{r.stderr}")
    need(time.monotonic()-started<60,"busy writer did not fail within the bounded timeout")
    need(logical(locked/"run.sqlite")==snapshot,"busy failure changed authoritative evidence")
    with_lock(locked/"run.sqlite","read",6,lambda:run(binary,"resume","--run",locked,"--pause-after","1"))
    need(discovery_state(locked)[0]==2,"commit under a concurrent WAL reader failed")
    sidecar=root/"S3-sidecar"
    shutil.copytree(source,sidecar)
    family=sidecar/"confirmation"/"family.sqlite"
    r=with_lock(family,"write",9,lambda:run(binary,"family-confirm","--run",sidecar,"--pause-after","1",expect=(2,)))
    need("locked" in r.stderr or "busy" in r.stderr,f"sidecar lock failure not reported as busy\n{r.stderr}")
    run(binary,"family-confirm","--run",sidecar,"--pause-after","1")
    run(binary,"resume","--run",locked)
    need(discovery_state(locked)[0]==pairs,"recovery after lock contention incomplete")

    # S4: corruption and truncation of authoritative databases.
    complete=root/"S4-source"
    shutil.copytree(source,complete)
    run(binary,"family-confirm","--run",complete)
    for db_file in (complete/"run.sqlite",complete/"confirmation"/"family.sqlite"):
        with sqlite3.connect(str(db_file)) as db:
            db.execute("PRAGMA wal_checkpoint(TRUNCATE)")
    for target,command in (("run.sqlite",("verify",)),("run.sqlite",("accounting",)),
                           ("confirmation/family.sqlite",("family-report",))):
        original=complete/target
        good=logical(original)
        size=original.stat().st_size
        page=4096
        offsets=[0,24,100]+[page*i+page//2 for i in range(1,size//page)]
        trials=[("flip",o) for o in offsets]+[("truncate",size//2),("truncate",page),("truncate",0)]
        accepted=rejected=0
        for kind,offset in trials:
            folder=root/"S4-trial"
            shutil.rmtree(folder,ignore_errors=True)
            shutil.copytree(complete,folder)
            for stale in folder.glob("**/*.sqlite-*"):
                stale.unlink()
            victim=folder/target
            data=bytearray(victim.read_bytes())
            if kind=="flip":
                for i in range(offset,min(offset+16,len(data))):
                    data[i]^=0xA5
            else:
                del data[offset:]
            victim.write_bytes(bytes(data))
            before=victim.read_bytes()
            r=run(binary,*command,"--run",folder,expect=(0,2))
            need(victim.read_bytes()==before,f"{command} on corrupted {target} modified it")
            if r.returncode==0:
                # Success only on logically identical AND structurally sound
                # bytes (flips in unused page space); indexes are not in iterdump.
                need(logical(victim)==good,f"{command} accepted logically corrupted {target} ({kind}@{offset})")
                need(integrity(victim)==["ok"],f"{command} accepted structurally corrupt {target} ({kind}@{offset})")
                accepted+=1
            else:
                rejected+=1
        print("S4",target,command[0],"accepted_unchanged",accepted,"rejected",rejected)
        need(rejected>=3,f"{target}: corruption trials never reached evidence")
    # S5: well-formed semantic edits to previously unverified sample columns.
    fake="0"*64
    for target,command,sql,reason in (
        ("run.sqlite",("verify",),f"UPDATE v3_samples SET output_digest='{fake}' WHERE rowid=(SELECT MIN(rowid) FROM v3_samples WHERE status='ok')",
         "output digest disagrees with independent oracle"),
        ("run.sqlite",("verify",),"UPDATE v3_samples SET protocol='forged_protocol_v9' WHERE rowid=1",
         "measurement protocol mismatch"),
        ("confirmation/family.sqlite",("family-report",),f"UPDATE v3_samples SET output_digest='{fake}' WHERE rowid=(SELECT MIN(rowid) FROM v3_samples WHERE status='ok')",
         "output digest disagrees with independent oracle"),
        ("confirmation/family.sqlite",("family-report",),"UPDATE v3_samples SET protocol='forged_protocol_v9' WHERE rowid=1",
         "measurement protocol mismatch")):
        folder=root/"S5-trial"
        shutil.rmtree(folder,ignore_errors=True)
        shutil.copytree(complete,folder)
        with sqlite3.connect(str(folder/target)) as db:
            need(db.execute(sql).rowcount==1,f"S5 edit did not apply: {sql}")
        r=run(binary,*command,"--run",folder,expect=(2,))
        need(reason in r.stderr,f"S5 {target}: expected {reason!r}\n{r.stderr}")
    print("PASS storage fault matrix: write faults fail closed without partial pairs and recover; busy "
          "locks fail bounded without changing evidence; structural or semantic corruption never yields "
          "success on changed content")


if __name__=="__main__":
    main()
