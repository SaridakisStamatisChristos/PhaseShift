#!/usr/bin/env python3
"""V18 three-way competition integrity and sealed-holdout falsification."""
import copy
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0,str(Path(__file__).resolve().parents[2]/"benchmarks"/"v3"))
import run_coverage_competition as competition
from aggregate_coverage_competition import aggregate


class PolicyCompetitionFalsifiers(unittest.TestCase):
    def test_frozen_three_way_development_and_old_worker_schemas(self):
        m,mh=competition.manifest()
        self.assertEqual(m["seeds"],list(range(6301,6321)))
        self.assertEqual(m["policies"],["conditional-tree","acquisition-v3",
                                         "acquisition-v3-coverage"])
        self.assertEqual(len(m["fixtures"]),6)
        self.assertEqual(m["pairs"],12)
        self.assertEqual(competition.PINNED_SCHEMA["acquisition-v3-coverage"],18)
        self.assertEqual(competition.PINNED_SCHEMA["conditional-tree"],14)
        self.assertFalse(m["sota_claim"])
        self.assertEqual(len(mh),64)
        for f in m["fixtures"]:
            names=competition.order(m,False,f["id"])
            self.assertEqual(len(names),60)
            self.assertEqual(len({tuple(v) for v in names}),60)
            self.assertEqual({v[2] for v in names},set(m["seeds"]))
            self.assertEqual(len(competition.order(m,True,f["id"])),3)

    def test_distinct_seed_cohorts_cannot_change_into_sealed_evaluation(self):
        m,_=competition.manifest()
        self.assertFalse(set(m["seeds"])&set(range(7101,7121)))
        self.assertFalse(set(m["seeds"])&set(range(6101,6121)))
        original=competition.MANIFEST
        with tempfile.TemporaryDirectory() as d:
            altered=Path(d)/"manifest.json"
            bad=copy.deepcopy(m)
            bad["seeds"][0]=7101
            altered.write_text(json.dumps(bad))
            with patch.object(competition,"MANIFEST",altered):
                with self.assertRaises(ValueError):
                    competition.manifest()

    def test_older_fixture_definition_cannot_silently_change(self):
        m,_=competition.manifest()
        with tempfile.TemporaryDirectory() as d:
            altered=Path(d)/"manifest.json"
            bad=copy.deepcopy(m)
            bad["fixtures"][0]["threshold"]=[1,2]
            altered.write_text(json.dumps(bad))
            with patch.object(competition,"MANIFEST",altered):
                with self.assertRaises(ValueError):
                    competition.manifest()

    def test_missing_scenario_or_competitor_fails_closed(self):
        with self.assertRaises(ValueError):
            aggregate({})
        m,_=competition.manifest()
        with self.assertRaises(ValueError):
            competition.order(m,False,"not_a_real_fixture")


if __name__=="__main__":
    unittest.main()
