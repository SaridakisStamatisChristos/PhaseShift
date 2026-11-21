#!/usr/bin/env python3
import argparse,sqlite3,json
p=argparse.ArgumentParser();p.add_argument("--run",required=True);a=p.parse_args()
with sqlite3.connect(a.run+"/run.sqlite") as db:
 rows=db.execute("SELECT detail FROM events WHERE event='v2_factorial_screen_discovery_only'").fetchall()
assert len(rows)==1,rows
result=json.loads(rows[0][0])
assert result["complete_cohorts"]==3 and result["incomplete_cohorts"]==0,result
assert result["classification"]=="CONDITIONAL_PERFORMANCE_CANDIDATE",result
print("v2 2x2 interaction discovery diagnostic:",result)
