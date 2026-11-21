#!/usr/bin/env python3
"""Independent SQLite audit: real-worker, two frozen performance hypotheses."""
import argparse,json,pathlib,sqlite3
p=argparse.ArgumentParser();p.add_argument("--run",required=True);args=p.parse_args()
with sqlite3.connect(pathlib.Path(args.run)/"run.sqlite") as db:
 assert db.execute("PRAGMA user_version").fetchone()[0]==9
 campaign=db.execute("SELECT state,manifest_sha256,required_pairs FROM campaigns_v2").fetchone()
 assert campaign and campaign[0]=="FINALIZED" and campaign[2]==20,campaign
 members=db.execute("SELECT h.id,h.status,o.raw_p,o.holm_adjusted_p,"
 "o.familywise_reject,s.seed_pairs,s.case_count,"
 "(SELECT COUNT(*) FROM confirmation_evidence e WHERE e.hypothesis_id=h.id)"
 " FROM hypotheses h JOIN campaign_members_v2 m ON m.hypothesis_id=h.id"
 " JOIN campaign_outcomes_v2 o ON o.hypothesis_id=h.id"
 " LEFT JOIN confirmation_seals s ON s.hypothesis_id=h.id ORDER BY m.member_index").fetchall()
 assert len(members)==2,members
 for id,status,p,adjusted,reject,pairs,case_count,links in members:
  assert id in ("H1","H2") and status in ("CONFIRMED","NOT_CONFIRMED"),members
  assert 0<p<=1 and p<=adjusted<=1,(id,p,adjusted)
  if status=="CONFIRMED":
   assert pairs>=20 and case_count==links and links>=40, members
   assert db.execute("SELECT COUNT(*) FROM campaign_pvalues_v2 WHERE hypothesis_id=?",(id,)).fetchone()[0]==1
  else:
   assert p==1 and reject==0,members
  if reject: assert status=="CONFIRMED" and adjusted<=0.05,members
 seeds={}
 for id,status,*_ in members:
  if status=="CONFIRMED":
   seeds[id]={int(x[0]) for x in db.execute("SELECT DISTINCT seed_group FROM confirmation_evidence WHERE hypothesis_id=?",(id,))}
 if len(seeds)==2:assert seeds["H1"].isdisjoint(seeds["H2"]),seeds
 assert db.execute("SELECT COUNT(*) FROM campaign_outcomes_v2").fetchone()[0]==2
 print("REAL-WORKER V2 CAMPAIGN:",json.dumps(members))
