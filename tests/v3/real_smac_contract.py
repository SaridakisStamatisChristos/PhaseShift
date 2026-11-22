#!/usr/bin/env python3
"""Pre-execution SMAC3 contract falsifiers; never open synthetic holdout."""
import copy,json,math,sys,tempfile,unittest
from pathlib import Path
from unittest.mock import patch
sys.path.insert(0,str(Path(__file__).resolve().parents[2]/"benchmarks"/"v3"))
import run_real_smac as lab

class SMACContract(unittest.TestCase):
 def test_manifest_pinned_and_native_workers_only(self):
  m,by=lab.manifest()
  self.assertEqual(m["seeds"],list(range(33001,33021)))
  self.assertEqual(m["smac_version"],"2.4.1")
  self.assertEqual(m["policies"],list(lab.POLICIES))
  self.assertTrue(set(m["seeds"]).isdisjoint(range(7101,7121)))
  self.assertEqual(m["pair_budget"],12)
  self.assertEqual(m["confirmation_pairs"],20)
  self.assertFalse(m["eligible_for_sota"])
  for f in m["scenarios"]:
   source=by[f["source"]]
   c=lab.config(f,source,33001,True)
   e=lab.config(f,source,33001,False)
   self.assertEqual(c["algorithms"],e["algorithms"])
   self.assertEqual(c.get("fixed",{}).get("n"),e.get("fixed",{}).get("n"))
   self.assertEqual(len(lab.allowed_edges(source))+1,len(source["config"]["axes"][source["config"]["primary_axis"]]["values"]))
 def test_loss_censor_and_effect(self):
  self.assertEqual(lab.smac_loss(None),1.)
  self.assertEqual(lab.smac_loss(0),1.)
  self.assertAlmostEqual(lab.smac_loss(1.),0.5)
  self.assertAlmostEqual(lab.smac_loss(-1.),0.5)
  with self.assertRaises(ValueError):lab.smac_loss(float("nan"))
  with self.assertRaises(ValueError):lab.smac_loss(float("inf"))
 def test_manifest_fail_closed(self):
  m,_=lab.manifest()
  changes=[]
  for fn in (
   lambda x:x["seeds"].__setitem__(0,7101),
   lambda x:x.__setitem__("smac_version","0"),
   lambda x:x.__setitem__("pair_budget",13),
   lambda x:x.__setitem__("policies",["smac3_hpo"]),
   lambda x:x.__setitem__("evaluation_reused",True),
   lambda x:x["source_v18_code_blobs"].__setitem__("apps/phaseshift_v3_pairs.cpp","0"*40),
  ):
   t=copy.deepcopy(m);fn(t);changes.append(t)
  with tempfile.TemporaryDirectory() as d:
   for n,bad in enumerate(changes):
    p=Path(d)/(str(n)+".json");p.write_text(json.dumps(bad))
    with patch.object(lab,"PLAN",p):
     with self.assertRaises((ValueError,KeyError)):lab.manifest()
if __name__=="__main__":unittest.main()
