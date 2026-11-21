#!/usr/bin/env python3
"""Executable v2 ordered-axis contract; no worker data or synthetic SOTA.

All declared integer/PPM morphology axes should satisfy the same real parser
and strictly increasing coordinate check as ordinary n/x/edges axes.
Independent config generation avoids repeated coverage from just one axis.
"""
import argparse
import json
import pathlib
import subprocess
import tempfile

parser=argparse.ArgumentParser()
parser.add_argument("--binary",type=pathlib.Path,required=True)
args=parser.parse_args()

# (axis, family, valid lower label, valid upper label)
valid=[
 ("x","calibration",8,16),
 ("n","integer_array",64,96),
 ("value_range","integer_array",8,16),
 ("unique_count","integer_array",2,4),
 ("run_length","integer_array",2,4),
 ("duplicate_ppm","integer_array",100000,200000),
 ("disorder_ppm","integer_array",100000,200000),
 ("sign_ppm","integer_array",100000,200000),
 ("duplicate_fraction","integer_array",0.2,0.4),
 ("disorder_fraction","integer_array",0.2,0.4),
 ("sign_fraction","integer_array",0.2,0.4),
 ("k","topk",1,16),
 ("vertices","graph",32,64),
 ("edges","graph",32,64),
 ("density_ppm","graph",100000,200000),
 ("degree_skew_ppm","graph",100000,200000),
 ("community_count","graph",2,4),
 ("max_weight","graph",4,8),
 ("density","graph",0.1,0.2),
 ("degree_skew","graph",0.1,0.2),
]
with tempfile.TemporaryDirectory(prefix="phaseshift-v2-axis-") as tmp:
    for axis,family,low,high in valid:
        algorithm={"integer_array":"std_sort","topk":"topk_heap",
                   "graph":"dijkstra_heap","calibration":"calibration"}[family]
        fixed={"n":64,"distribution":"uniform"} if family=="integer_array" else (
            {"n":64} if family=="topk" else
            {"vertices":64,"source":0,"weight_class":"general"} if family=="graph" else {})
        # Never fix the varying axis: preserve the generator's canonical
        # assignment semantics instead of masking a parser failure.
        fixed.pop(axis,None)
        config={
            "schema_version":1,
            "experiment_id":"v2-numeric-axis-"+axis,
            "problem_family":family,
            "algorithms":[algorithm],
            "seed":137,
            "primary_axis":axis,
            "axes":{axis:{"values":[low,high]}},
            "fixed":fixed,
            "search":{
                "method":"hierarchical_boundary_v2",
                "policy_version":"v2",
                "max_cases":2,
            },
            "measurement":{"paired_discovery_blocks":1,
                           "paired_confirm_blocks":20,"warmup_iterations":0},
            "limits":{"total_wall_seconds":15,"max_search_queue":16},
        }
        path=pathlib.Path(tmp)/(axis+".json")
        path.write_text(json.dumps(config,sort_keys=True)+"\n")
        proc=subprocess.run([str(args.binary),"validate","--config",str(path)],
                            capture_output=True,text=True,timeout=20)
        assert proc.returncode==0,(axis,proc.stdout,proc.stderr)
        assert "VALID" in proc.stdout,(axis,proc.stdout,proc.stderr)
        # Swapped labels must fail strict numeric ordering for every axis.
        config["axes"][axis]["values"]=[high,low]
        path.write_text(json.dumps(config,sort_keys=True)+"\n")
        backward=subprocess.run(
            [str(args.binary),"validate","--config",str(path)],
            capture_output=True,text=True,timeout=20)
        assert backward.returncode!=0,(axis,backward.stdout,backward.stderr)
print("PASS",len(valid),"typed primary axes and their descending-order rejects")
