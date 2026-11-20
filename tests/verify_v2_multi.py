#!/usr/bin/env python3
import argparse,sqlite3,json
p=argparse.ArgumentParser();p.add_argument("--run",required=True);args=p.parse_args()
with sqlite3.connect(args.run+"/run.sqlite") as db:
 events=db.execute("SELECT detail FROM events WHERE event='v2_multiple_change_discovery_only'").fetchall()
 hyp=db.execute("SELECT left_value,right_value,status,kind FROM hypotheses WHERE id='H1'").fetchone()
 assert events, "multi-change detector produced no diagnostic candidate"
 found=[json.loads(x[0]) for x in events]
 assert any(x["left"]==7 and x["right"]==8 for x in found), found
 assert hyp and hyp[0]<=7 and hyp[1]>=8 and hyp[2]=="HYPOTHESIZED",hyp
 print("multi-change v2 discovery only: ", found, hyp)
