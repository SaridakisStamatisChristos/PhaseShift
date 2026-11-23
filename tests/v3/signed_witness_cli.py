#!/usr/bin/env python3
"""Independent falsification of the V3-09 signed boundary witness.

The witness must (1) carry a detached Ed25519 signature that the stock
`openssl` CLI verifies independently, (2) bind logical digests that this test
recomputes from raw SQLite without using the engine, and (3) fail closed on
any post-signing change to the witness, the discovery run or the holdout
sidecar, including a journal strip and re-signing with another or the
genuine key over altered evidence.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import sqlite3
import struct
import subprocess


def need(ok,msg):
    if not ok:
        raise AssertionError(msg)


def run(binary,*args,expect=0,fault=None,reason=None):
    env=dict(os.environ)
    env.pop("PHASESHIFT_V3_FAILPOINT",None)
    if fault is not None:
        env["PHASESHIFT_V3_FAILPOINT"]=fault
    p=subprocess.run([str(binary),*map(str,args)],capture_output=True,text=True,timeout=120,env=env)
    need(p.returncode==expect,f"{args}: rc={p.returncode} expected={expect}\n{p.stdout}\n{p.stderr}")
    # A rejection must fail for the intended reason, not e.g. a usage error.
    need(reason is None or reason in p.stderr,f"{args}: expected {reason!r} in stderr\n{p.stderr}")
    return p.stdout


def openssl(*args,expect=0):
    p=subprocess.run(["openssl",*map(str,args)],capture_output=True,timeout=60)
    need(p.returncode==expect,f"openssl {args}: rc={p.returncode}\n{p.stdout!r}\n{p.stderr!r}")
    return p.stdout


def ro(path):
    return sqlite3.connect(path.resolve().as_uri()+"?mode=ro",uri=True)


def snapshot(path):
    with ro(path) as db:
        version=db.execute("PRAGMA user_version").fetchone()[0]
        return hashlib.sha256("\n".join([f"user_version={version}",*db.iterdump()]).encode()).hexdigest()


def logical_digest(path,domain,queries):
    """Independent implementation of the documented length-prefixed digest."""
    out=bytearray(domain.encode())
    def put(raw):
        out.extend(str(len(raw)).encode()+b":"+raw)
    with ro(path) as db:
        for query in queries:
            put(query.encode())
            for row in db.execute(query):
                put(b"row")
                for value in row:
                    if value is None:
                        put(b"5")
                    elif isinstance(value,int):
                        put(b"1");put(str(value).encode())
                    elif isinstance(value,float):
                        put(b"2");put(str(struct.unpack("<Q",struct.pack("<d",value))[0]).encode())
                    elif isinstance(value,str):
                        put(b"3");put(value.encode())
                    else:
                        raise AssertionError("BLOB in logical digest")
    return hashlib.sha256(bytes(out)).hexdigest()


BAD_SIG="witness signature invalid"
DIFFERS="replayed evidence differs from signed witness"


SOURCE_QUERIES=["PRAGMA user_version","SELECT * FROM v3_plan ORDER BY id",
    "SELECT case_id,family,params,seed,input_sha256 FROM v3_cases ORDER BY case_id",
    "SELECT * FROM v3_pairs ORDER BY pair_number",
    "SELECT * FROM v3_samples ORDER BY pair_number,side,ordinal",
    "SELECT * FROM v3_conditional_decisions ORDER BY pair_number"]


def sidecar_queries(version):
    q=["PRAGMA user_version","SELECT type,name,tbl_name,sql FROM sqlite_master ORDER BY type,name",
       "SELECT * FROM v3_family ORDER BY id",
       "SELECT case_id,family,params,seed,input_sha256 FROM v3_cases ORDER BY case_id",
       "SELECT * FROM v3_pairs ORDER BY pair_number",
       "SELECT * FROM v3_samples ORDER BY pair_number,side,ordinal"]
    if version==19:
        q+=["SELECT * FROM v3_holdout_journal_origin ORDER BY id",
            "SELECT * FROM v3_holdout_attempts ORDER BY attempt_id"]
    return q


def audit_witness(binary,folder,out,public_pem,version):
    """Engine-independent check of signature, header and every bound digest."""
    witness_file,sig=out/"witness.json",out/"witness.sig"
    verified=openssl("pkeyutl","-verify","-pubin","-inkey",public_pem,"-rawin",
                     "-in",witness_file,"-sigfile",sig)
    need(b"Signature Verified Successfully" in verified,"openssl did not verify witness")
    need(len(sig.read_bytes())==64,"Ed25519 signature length")
    der=openssl("pkey","-pubin","-in",public_pem,"-outform","DER")
    w=json.loads(witness_file.read_text())
    need(w["schema"]=="phaseshift.v3.signed_boundary_witness.1" and w["signature_algorithm"]=="ed25519","header")
    need(w["public_key_sha256"]==hashlib.sha256(der).hexdigest(),"public key fingerprint")
    need(openssl("pkey","-pubin","-in",out/"public_key.pem","-outform","DER")==der,"bundled key")
    need(w["exporter"]["executable_sha256"]==hashlib.sha256(binary.read_bytes()).hexdigest(),"exporter digest")
    e=w["evidence"]
    sidecar=folder/"confirmation"/"family.sqlite"
    with ro(sidecar) as db:
        spec,seal=db.execute("SELECT specification,spec_sha256 FROM v3_family").fetchone()
        counts=dict(db.execute("SELECT state,COUNT(*) FROM v3_holdout_attempts GROUP BY state").fetchall()) if version==19 else {}
    need(e["family"]["specification"]==spec and e["family"]["family_sha256"]==seal==hashlib.sha256(spec.encode()).hexdigest(),
         "family spec binding")
    need(e["family"]["sidecar_schema"]==version,"sidecar schema")
    need(e["family"]["sidecar_logical_sha256"]==logical_digest(sidecar,"phaseshift.v3.family-sidecar.1",sidecar_queries(version)),
         "independent sidecar digest")
    need(e["discovery"]["source_snapshot_sha256"]==
         logical_digest(folder/"run.sqlite","phaseshift.v3.family-source.1",SOURCE_QUERIES)==
         json.loads(spec)["source_snapshot_sha256"],"independent discovery digest")
    need(json.loads(run(binary,"family-report","--run",folder))==e["family_report"],"family report binding")
    need(e["family_report"]["complete"] and not e["claim_limits"]["confirmed"] and
         not e["claim_limits"]["hardware_general"],"claim limits")
    for m,ev in zip(json.loads(spec)["members"],e["members"],strict=True):
        need((m["discovery_pair"],m["direction"],str(m["left"]),str(m["right"]))==
             (ev["discovery_pair"],ev["direction"],ev["left"],ev["right"]),"member endpoints")
    j=e["holdout_attempt_journal"]
    if version==19:
        need(j["retries_observable"] and j["holdout_attempts"]==sum(counts.values()) and
             j["abandoned_attempts"]==counts.get("ABANDONED",0),"journal binding")
    else:
        need(j=={"state":"ABSENT_SCHEMA_16","retries_observable":False},"legacy journal disclosure")
    return w


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
    key,pub=root/"key.pem",root/"pub.pem"
    other_key,other_pub=root/"other.pem",root/"other_pub.pem"
    for k,pk in ((key,pub),(other_key,other_pub)):
        openssl("genpkey","-algorithm","ed25519","-out",k)
        openssl("pkey","-in",k,"-pubout","-out",pk)
    rsa=root/"rsa.pem"
    openssl("genpkey","-algorithm","RSA","-pkeyopt","rsa_keygen_bits:2048","-out",rsa)

    cfg=json.loads(a.config.read_text())
    cfg["axes"]={"x":{"values":[0,1]},"negative":{"values":[False,True]}}
    cfg["fixed"]={"threshold":1}
    cfg["search"]["admissible_subspaces"]=[{"negative":[False]},{"negative":[True]}]
    cfg["search"]["max_cases"]=12
    cfgpath=root/"config.json"
    cfgpath.write_text(json.dumps(cfg))
    folder=root/"discovery"
    run(binary,"conditional-tree","--config",cfgpath,"--out",folder,"--pairs","6")
    manifest=root/"manifest.json"
    manifest.write_text(json.dumps({"family_id":"witness-test","seed_pairs":20,"alpha":.05,
        "sign_model":"independent_seed_signs_declared",
        "members":[{"discovery_pair":0,"direction":1},{"discovery_pair":1,"direction":1}]}))
    legacy=root/"legacy"
    shutil.copytree(folder,legacy)

    # No witness before a frozen, complete, unambiguous family.
    run(binary,"witness-export","--run",folder,"--key",key,"--out",root/"w-none",expect=2,reason="frozen family sidecar")
    run(binary,"family-freeze-journaled","--run",folder,"--manifest",manifest)
    run(binary,"family-confirm","--run",folder,"--pause-after","1",fault="before_holdout_commit",expect=88)
    run(binary,"witness-export","--run",folder,"--key",key,"--out",root/"w-pending",expect=2)
    run(binary,"family-confirm","--run",folder,"--pause-after","3")
    run(binary,"witness-export","--run",folder,"--key",key,"--out",root/"w-partial",expect=2,reason="complete frozen holdout")
    run(binary,"family-confirm","--run",folder)
    run(binary,"witness-export","--run",folder,"--key",rsa,"--out",root/"w-rsa",expect=2,reason="must be Ed25519")
    for absent in ("w-none","w-pending","w-partial","w-rsa"):
        need(not (root/absent).exists(),f"failed export left {absent}")

    before=(snapshot(folder/"run.sqlite"),snapshot(folder/"confirmation"/"family.sqlite"))
    w1,w2=root/"witness",root/"witness-again"
    summary=json.loads(run(binary,"witness-export","--run",folder,"--key",key,"--out",w1))
    run(binary,"witness-export","--run",folder,"--key",key,"--out",w1,expect=2,reason="immutable export")
    run(binary,"witness-export","--run",folder,"--key",key,"--out",w2)
    need((w1/"witness.json").read_bytes()==(w2/"witness.json").read_bytes() and
         (w1/"witness.sig").read_bytes()==(w2/"witness.sig").read_bytes(),"nondeterministic witness")
    need(summary["witness_sha256"]==hashlib.sha256((w1/"witness.json").read_bytes()).hexdigest(),"summary digest")
    w=audit_witness(binary,folder,w1,pub,19)
    need(w["evidence"]["holdout_attempt_journal"]["state"]=="RETRIED_AFTER_INTERRUPTION" and
         w["evidence"]["holdout_attempt_journal"]["abandoned_attempts"]==1,"retry disclosed in witness")
    ok=json.loads(run(binary,"witness-verify","--run",folder,"--witness",w1,"--public-key",pub))
    need(ok["verified"] and ok["exporter_executable_matches_verifier"],"genuine witness rejected")
    need((snapshot(folder/"run.sqlite"),snapshot(folder/"confirmation"/"family.sqlite"))==before,
         "export/verify mutated evidence")
    run(binary,"witness-verify","--run",folder,"--witness",w1,"--public-key",other_pub,expect=2,reason=BAD_SIG)
    run(binary,"witness-verify","--run",folder,"--witness",w1,expect=2)

    def forged(name,reason,mutate_json=None,resign=None,swap_pub=False,sig_bytes=None):
        d=root/name
        shutil.copytree(w1,d)
        if mutate_json:
            doc=json.loads((d/"witness.json").read_text())
            mutate_json(doc)
            (d/"witness.json").write_text(json.dumps(doc)+"\n")
        if resign:
            openssl("pkeyutl","-sign","-inkey",resign,"-rawin","-in",d/"witness.json","-out",d/"witness.sig")
        if swap_pub:
            shutil.copy(other_pub,d/"public_key.pem")
        if sig_bytes is not None:
            (d/"witness.sig").write_bytes(sig_bytes)
        run(binary,"witness-verify","--run",folder,"--witness",d,"--public-key",pub,expect=2,reason=reason)

    def flip_state(doc):
        doc["evidence"]["holdout_attempt_journal"]["state"]="CLEAN"
    def promote(doc):
        doc["evidence"]["claim_limits"]["confirmed"]=True
    forged("edit-unsigned",BAD_SIG,flip_state)
    forged("attacker-resign",BAD_SIG,flip_state,resign=other_key,swap_pub=True)
    forged("genuine-key-resign-altered-evidence",DIFFERS,promote,resign=key)
    forged("truncated-signature",BAD_SIG,sig_bytes=(w1/"witness.sig").read_bytes()[:63])
    forged("bundled-key-swap","bundled public key differs",swap_pub=True)
    raw=(w1/"witness.json").read_bytes()
    corrupted=root/"byte-flip"
    shutil.copytree(w1,corrupted)
    (corrupted/"witness.json").write_bytes(raw[:-3]+bytes([raw[-3]^1])+raw[-2:])
    run(binary,"witness-verify","--run",folder,"--witness",corrupted,"--public-key",pub,expect=2,reason=BAD_SIG)

    # Evidence altered after signing: the signed witness must no longer verify.
    evidence_attacks=(
        ("strip-journal","family.sqlite",DIFFERS,"DROP INDEX v3_holdout_attempt_active; DROP TABLE v3_holdout_attempts; "
                         "DROP TABLE v3_holdout_journal_origin; PRAGMA user_version=16"),
        ("hide-abandoned","family.sqlite",DIFFERS,"DELETE FROM v3_holdout_attempts WHERE state='ABANDONED'; "
                          "UPDATE v3_holdout_attempts SET attempt_id=attempt_id-1"),
        ("alter-holdout-raw","family.sqlite","holdout effect/status/CPU replay mismatch","UPDATE v3_samples SET cpu_ns=COALESCE(cpu_ns,0)+1 WHERE pair_number=3"),
        ("alter-discovery","run.sqlite","PHASESHIFT v3 error","UPDATE v3_pairs SET pair_wall_ns=pair_wall_ns+1 WHERE pair_number=0"))
    for name,target,reason,sql in evidence_attacks:
        broken=root/name
        shutil.copytree(folder,broken)
        dbfile=broken/"confirmation"/"family.sqlite" if target=="family.sqlite" else broken/"run.sqlite"
        with sqlite3.connect(dbfile) as db:
            db.executescript(sql)
        run(binary,"witness-verify","--run",broken,"--witness",w1,"--public-key",pub,expect=2,reason=reason)
    # The renumbered-journal rewrite defeats the unsigned journal (F-027) and is
    # accepted by family-report, but not by the previously signed witness.
    json.loads(run(binary,"family-report","--run",root/"hide-abandoned"))

    # Legacy v16 sidecar: witness discloses that retries are unobservable.
    run(binary,"family-freeze","--run",legacy,"--manifest",manifest)
    run(binary,"family-confirm","--run",legacy)
    lw=root/"legacy-witness"
    run(binary,"witness-export","--run",legacy,"--key",key,"--out",lw)
    audit_witness(binary,legacy,lw,pub,16)
    run(binary,"witness-verify","--run",legacy,"--witness",lw,"--public-key",pub)
    run(binary,"witness-verify","--run",legacy,"--witness",w1,"--public-key",pub,expect=2,reason=DIFFERS)
    print("PASS signed witness: openssl-independent Ed25519, engine-independent digests, deterministic export, "
          "fail-closed export gates, forged/re-signed/stripped/altered evidence rejected, legacy disclosure")


if __name__=="__main__":
    main()
