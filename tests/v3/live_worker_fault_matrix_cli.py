#!/usr/bin/env python3
"""V3-09 live-worker crash and containment matrix with real processes.

Faults are injected into the real phaseshift_worker through the test-only
PHASESHIFT_WORKER_FAULT hook and real OS signals:

  A  parent SIGKILL while a worker is live (accounted discovery)
  B  parent SIGTERM while a worker is live (journaled holdout)
  C  external SIGKILL of every live worker (accounted discovery)
  D  worker stalls past the sample deadline (timeout)
  E  worker leaves a grandchild in its process group after a normal exit
  F  worker leaves a setsid + double-fork escaped grandchild

Each case checks process liveness through /proc, that no failed or killed
measurement becomes a valid finite sample, the durable attempt journals, and
read-only replay/accounting.
"""
import argparse
import json
import os
from pathlib import Path
import shutil
import signal
import sqlite3
import subprocess
import tempfile
import time


def need(ok,msg):
    if not ok:
        raise AssertionError(msg)


def env_for(fault=None,pidfile=None):
    env=dict(os.environ)
    for key in ("PHASESHIFT_V3_FAILPOINT","PHASESHIFT_WORKER_FAULT","PHASESHIFT_WORKER_FAULT_PIDFILE"):
        env.pop(key,None)
    if fault:
        env["PHASESHIFT_WORKER_FAULT"]=fault
        env["PHASESHIFT_WORKER_FAULT_PIDFILE"]=str(pidfile)
    return env


def run(binary,*args,expect=0,fault=None,pidfile=None):
    # Capture through files, never pipes: a leaked descendant inheriting a
    # pipe would make the harness wait for it and mask a containment failure.
    with tempfile.TemporaryFile("w+") as out,tempfile.TemporaryFile("w+") as err:
        p=subprocess.run([str(binary),*map(str,args)],stdout=out,stderr=err,timeout=180,env=env_for(fault,pidfile))
        out.seek(0);err.seek(0)
        p.stdout,p.stderr=out.read(),err.read()
    need(p.returncode==expect,f"{args}: rc={p.returncode} expected={expect}\n{p.stdout}\n{p.stderr}")
    return p


def state(pid):
    try:
        text=Path(f"/proc/{pid}/stat").read_text()
    except (FileNotFoundError,ProcessLookupError):
        return None
    return text[text.rindex(")")+2]


def dead(pid):
    return state(pid) in (None,"Z","X")


def wait_dead(pids,seconds,what):
    deadline=time.monotonic()+seconds
    while time.monotonic()<deadline:
        if all(dead(p) for p in pids):
            return
        time.sleep(.02)
    raise AssertionError(f"{what}: still alive {[(p,state(p)) for p in pids if not dead(p)]}")


def noted(pidfile,role):
    if not pidfile.exists():
        return []
    return [int(line.split()[1]) for line in pidfile.read_text().splitlines() if line.split()[0]==role]


def wait_noted(pidfile,role,seconds=60):
    deadline=time.monotonic()+seconds
    while time.monotonic()<deadline:
        found=noted(pidfile,role)
        if found and not dead(found[-1]):
            return found[-1]
        time.sleep(.01)
    raise AssertionError(f"no live {role} appeared")


def ro(path):
    return sqlite3.connect(path.resolve().as_uri()+"?mode=ro",uri=True)


def attempts(folder):
    with ro(folder/"run.sqlite") as db:
        states=[s for (s,) in db.execute("SELECT state FROM v3_attempts ORDER BY attempt_id")]
        pairs=db.execute("SELECT COUNT(*) FROM v3_pairs").fetchone()[0]
    return states,pairs


def samples(dbfile,pair):
    with ro(dbfile) as db:
        status,effect=db.execute("SELECT status,log_contrast FROM v3_pairs WHERE pair_number=?",(pair,)).fetchone()
        rows=db.execute("SELECT status,ns,batch FROM v3_samples WHERE pair_number=?",(pair,)).fetchall()
    return status,effect,rows


def audit_readonly(binary,folder):
    report=json.loads(run(binary,"accounting","--run",folder).stdout)
    need(report["state"]=="ACCOUNTING_INCOMPLETE","accounting must stay explicitly incomplete")
    run(binary,"verify","--run",folder)
    return report


def kill_parent_during_live_worker(binary,args,pidfile,sig):
    # No output pipes: an orphan inheriting them would make communicate()
    # wait for the orphan itself and mask a containment failure.
    child=subprocess.Popen([str(binary),*map(str,args)],env=env_for("stall_ms=30000",pidfile),
                           stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
    try:
        worker=wait_noted(pidfile,"worker")
        need(state(worker) not in (None,"Z"),"worker not live before parent kill")
        os.kill(child.pid,sig)
        child.wait(timeout=10)
    finally:
        if child.poll() is None:
            child.kill()
            child.wait(timeout=10)
    need(child.returncode==-sig,f"parent did not die by signal {sig}: {child.returncode}")
    # PR_SET_PDEATHSIG: the stalled (30 s) worker must not outlive its parent.
    wait_dead([worker],5,"orphaned worker after parent death")


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
    fast=json.loads(json.dumps(cfg))
    fast.setdefault("limits",{})["max_sample_wall_ms"]=200
    fast_config=root/"fast.json"
    fast_config.write_text(json.dumps(fast))

    # A: parent SIGKILL during a live worker; durable attempt stays STARTED.
    a_run=root/"A"
    kill_parent_during_live_worker(binary,["accounted-v3","--config",config,"--out",a_run,"--pairs","3",
                                           "--pause-after","1"],root/"A.pids",signal.SIGKILL)
    need(attempts(a_run)==(["STARTED"],0),"parent kill must leave exactly one unresolved attempt")
    run(binary,"resume","--run",a_run)
    need(attempts(a_run)==(["ABANDONED","COMMITTED","COMMITTED","COMMITTED"],3),"recovery after parent kill")
    report=audit_readonly(binary,a_run)
    need(report["abandoned"]==1 and report["committed"]==3,"abandoned live-worker attempt not disclosed")

    # B: parent SIGTERM during a live holdout worker (journaled family).
    manifest=root/"manifest.json"
    manifest.write_text(json.dumps({"family_id":"fault-matrix","seed_pairs":20,"alpha":.05,
        "sign_model":"independent_seed_signs_declared","members":[{"discovery_pair":0,"direction":1}]}))
    run(binary,"family-freeze-journaled","--run",a_run,"--manifest",manifest)
    kill_parent_during_live_worker(binary,["family-confirm","--run",a_run,"--pause-after","1"],
                                   root/"B.pids",signal.SIGTERM)
    report=json.loads(run(binary,"family-report","--run",a_run).stdout)
    need(report["attempt_integrity"]=="INTERRUPTED_ATTEMPT_PENDING" and report["committed_pairs"]==0,
         "SIGTERM during live holdout worker must leave a disclosed pending attempt")
    run(binary,"family-confirm","--run",a_run,"--pause-after","1")
    report=json.loads(run(binary,"family-report","--run",a_run).stdout)
    need(report["abandoned_attempts"]==1 and report["committed_pairs"]==1,"holdout recovery after SIGTERM")

    # C: every live worker is SIGKILLed externally; the pair must be censored.
    c_run,c_pids=root/"C",root/"C.pids"
    child=subprocess.Popen([str(binary),"accounted-v3","--config",str(config),"--out",str(c_run),"--pairs","1"],
                           env=env_for("stall_ms=30000",c_pids),stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
    killed=set()
    deadline=time.monotonic()+90
    while child.poll() is None:
        need(time.monotonic()<deadline,"external worker kill scenario hung")
        for pid in noted(c_pids,"worker"):
            if pid not in killed and not dead(pid):
                os.kill(pid,signal.SIGKILL)
                killed.add(pid)
        time.sleep(.01)
    need(child.wait()==0,"parent must survive killed workers")
    need(len(killed)==2,f"expected one live worker per side, killed {killed}")
    status,effect,rows=samples(c_run/"run.sqlite",0)
    need(status=="censored" and effect is None,"killed worker promoted to finite contrast")
    need(rows and all(r[0]=="signaled" and r[1] is None for r in rows),f"killed worker samples {rows}")
    need(attempts(c_run)==(["COMMITTED"],1),"censored pair still commits exactly once")
    audit_readonly(binary,c_run)

    # D: worker stalls beyond the sample deadline; parent enforces timeout.
    d_run,d_pids=root/"D",root/"D.pids"
    run(binary,"accounted-v3","--config",fast_config,"--out",d_run,"--pairs","1",fault="stall_ms=4000",pidfile=d_pids)
    status,effect,rows=samples(d_run/"run.sqlite",0)
    need(status=="censored" and effect is None and rows and all(r[0]=="timeout" and r[1] is None for r in rows),
         f"timeout promoted to timing: {rows}")
    wait_dead(noted(d_pids,"worker"),5,"timed-out worker")
    audit_readonly(binary,d_run)

    # E/F: descendants left behind by a normally exiting worker are contained.
    for name,fault in (("E","group_grandchild"),("F","escaped_grandchild")):
        folder,pids=root/name,root/f"{name}.pids"
        result=run(binary,"conditional-tree","--config",config,"--out",folder,"--pairs","2",fault=fault,pidfile=pids)
        grandchildren=noted(pids,"grandchild")
        need(len(grandchildren)==len(noted(pids,"worker"))>=4,f"{name}: every worker must spawn one grandchild")
        wait_dead(grandchildren,5,f"{name}: leaked worker descendant")
        if fault=="escaped_grandchild":
            need("contained" in result.stderr,"escaped descendants must be reported as contained")
        status,effect,rows=samples(folder/"run.sqlite",0)
        need(all(r[0]=="ok" for r in rows) and status=="ok","containment must not corrupt a normal measurement")
        run(binary,"verify","--run",folder)

    # G: legacy v1/v2 CLI has no subreaper; the runner's process-group kill
    # alone must contain an in-group grandchild after each normal worker exit.
    legacy,g_pids=root/"G",root/"G.pids"
    run(binary.with_name("phaseshift"),"explore","--config",a.config,"--out",legacy,"--pause-after","2",
        fault="group_grandchild",pidfile=g_pids)
    grandchildren=noted(g_pids,"grandchild")
    need(len(grandchildren)>=2,"legacy workers did not spawn grandchildren")
    wait_dead(grandchildren,5,"G: legacy CLI leaked in-group descendant")
    print("PASS live-worker fault matrix: parent SIGKILL/SIGTERM leave no orphan worker and a durable unresolved "
          "attempt; killed and timed-out workers are censored, never timed; in-group and escaped descendants "
          "contained (v3 subreaper and legacy process-group kill)")


if __name__=="__main__":
    main()
