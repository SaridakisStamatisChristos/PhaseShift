#!/usr/bin/env python3
"""Independent replay of frozen 5x20 C++ noisy detector challenge and ruptures.

Detector-only: offline l2 penalized segmentation. The native exact DP uses at
most 3 changes, external ruptures.Pelt does not; incomparable rows preserved.
Planted discontinuity labels are not timing-confirmed boundaries. No SOTA.
"""
from __future__ import annotations
import argparse,csv,hashlib,json,math
from collections import defaultdict
from pathlib import Path

def need(ok,why):
 if not ok:raise ValueError(why)

def digest(file):return hashlib.sha256(file.read_bytes()).hexdigest()

def sse(values,breaks):
 last=0;v=0.
 for stop in breaks:
  need(stop>last and stop-last>=3,"illegal short segmentation")
  section=values[last:stop]
  center=math.fsum(section)/len(section)
  v+=math.fsum((x-center)**2 for x in section)
  last=stop
 need(last==len(values),"segment does not end at end of observation")
 return v

def audit(dataset, report):
 d=dataset.resolve();result=json.loads(report.read_text())
 raw=defaultdict(dict)
 with (d/"datasets.csv").open(newline="") as f:
  for x in csv.DictReader(f):
   k=(x["scenario"],int(x["seed"]));i=int(x["index"])
   need(k[0] in {str(j) for j in range(5)} and 0<=k[1]<20 and
        i not in raw[k] and 0<=i<48 and math.isfinite(float(x["value"])),
        "missing, duplicate or nonfinite native C++ scenario value")
   raw[k][i]=float(x["value"])
 need(len(raw)==100 and all(set(x)==set(range(48)) for x in raw.values()),
      "frozen 100 native C++ datasets incomplete")
 cpp={}
 with (d/"observations.csv").open(newline="") as f:
  for x in csv.DictReader(f):
   k=(x["scenario"],int(x["seed"]))
   if k[0]=="slope":continue # different model and axis spacing
   need(k not in cpp and k in raw and x["model_version"]=="piecewise_constant_v2",
        "no source C++ exact-DP record for detector")
   cpp[k]=x
 need(set(raw)==set(cpp),"missing C++ exact DP baseline row")
 need(result["total_scenario_seed_rows"]==100 and
      result["phaseshift_cpp_max_changes"]==3 and
      result["dataset_sha256"]==digest(d/"datasets.csv") and
      result["sota_claim"] is False and
      result["search_policy_comparison"] is False and
      result["library_version"]=="1.1.10",
      "not actual pinned ruptures or changed dataset")
 rows={}
 summary={str(s):{"trials":0,"ruptures_nonempty":0,"ruptures_near_planted":0,
                  "cpp_nonempty":0,"cpp_near_planted":0,
                  "matched_K_feasible":0,"unmatched_K_feasible":0,
                  "objective_mismatches":0} for s in range(5)}
 total_gap=0.
 for item in result["rows"]:
  k=(str(item["scenario"]),int(item["seed"]))
  need(k in raw and k not in rows and
       item["external_change_count"]==len(item["external_changes"]),
       "changed/duplicated external detector output")
  values=[raw[k][i] for i in range(48)]
  cut=list(item["external_changes"])
  need(all(type(v) is int and 3<=v<=45 for v in cut) and
       cut==sorted(set(cut)) and
       all(b-a>=3 for a,b in zip([0]+cut,cut+[48])),
       "external segmentation contains invalid change position")
  actual_cost=sse(values,cut+[48])+2.5*len(cut)
  need(math.isclose(actual_cost,item["external_penalized_sse"],
      abs_tol=1e-9,rel_tol=1e-11),
      "external PELT penalized objective not reproducible")
  comparable=len(cut)<=3
  base=float(cpp[k]["dp_sse"])
  gap=abs(actual_cost-base) if comparable else None
  need(comparable==item["objective_feasible_set_matched"] and
       (not comparable and item["absolute_objective_difference_if_matched"] is None or
        comparable and math.isclose(gap,
          item["absolute_objective_difference_if_matched"],abs_tol=1e-9)),
       "C++ feasible set or objective gap changed")
  s=summary[k[0]];s["trials"]+=1
  s["ruptures_nonempty"]+=bool(cut)
  s["cpp_nonempty"]+=int(cpp[k]["dp_changes"])>0
  s["matched_K_feasible"]+=comparable
  s["unmatched_K_feasible"]+=not comparable
  if comparable:
   total_gap+=gap
   s["objective_mismatches"]+=gap>0.001 # C++ stores rounded 6-decimal scores
  expected={"1":[24],"2":[16,32]}.get(k[0],[])
  if expected:
   s["ruptures_near_planted"]+=all(any(abs(t-u)<=2 for u in cut) for t in expected)
   # C++ per-row nearest only records cut1 localization; do not misstate both cuts
   s["cpp_near_planted"]+=int(cpp[k]["localization_error"])<=2
  rows[k]=item
 need(set(rows)==set(raw) and
      sum(s["matched_K_feasible"] for s in summary.values())==result["matched_feasible_set_rows"],
      "not all 100 native C++ and external detector cases evaluated")
 need(all(s["trials"]==20 for s in summary.values()),
      "unbalanced externally observed detector scenarios")
 return {"schema":1,"experiment":"C++_v2_exact_DP_and_unconstrained_ruptures_PELT_l2",
         "source_dataset_sha256":digest(d/"datasets.csv"),
         "source_cpp_observations_sha256":digest(d/"observations.csv"),
         "external_report_sha256":digest(report),
         "ruptures_version":"1.1.10",
         "scenarios":summary,
         "total_series":100,"length_per_series":48,
         "matched_feasible_rows":sum(s["matched_K_feasible"] for s in summary.values()),
         "incomparable_feasible_rows":sum(s["unmatched_K_feasible"] for s in summary.values()),
         "sum_absolute_penalized_objective_gap_comparable":total_gap,
         "detector_only":True,"native_exact_DP_has_K_le_3":True,
         "shared_min_segment_length":3,"shared_l2_penalty":2.5,
         "sealed_v18_evaluation_reused":False,
         "statistical_superiority_established":False,
         "search_acquisition_compared":False,"sota_claim":False}

def main():
 p=argparse.ArgumentParser()
 p.add_argument("--dataset-root",required=True,type=Path)
 p.add_argument("--external",required=True,type=Path)
 p.add_argument("--out",required=True,type=Path)
 p.add_argument("--verify-only",action="store_true")
 a=p.parse_args()
 value=audit(a.dataset_root,a.external)
 if a.verify_only:
  need(json.loads(a.out.read_text())==value,"tampered independent detector audit")
 else:
  with a.out.open("x") as f:
   json.dump(value,f,sort_keys=True,indent=2);f.write("\n")
 print(json.dumps({"status":"PASS","series":100,"scenarios":value["scenarios"],
            "sota_claim":False},sort_keys=True),flush=True)
if __name__=="__main__":main()
