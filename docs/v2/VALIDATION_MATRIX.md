# PHASESHIFT v2 — Validation Matrix (provisional; not a release gate pass)

| Gate | State | Evidence / remaining work |
|---|---|---|
| G0 baseline revision | VERIFIED | main `93b47590c7e44eaf062d97995e9c5cd33274a7a2`; original 3 workflows green |
| G1 legacy v1 | LOCALLY/CI VERIFIED ON INTERMEDIATE SHA | Original v1 CTest and CLI through GitHub CI, final exact SHA pending |
| G2 lazy huge space | TESTED INTERMEDIATE | >10^9 virtual cardinality without enumeration; max-memory/memory metrics not fully published |
| G3 deterministic resume | TESTED INTERMEDIATE | v2 uniform/hierarchical pause/resume CLI and scheduler transcript; crash injection pending |
| G4 search correctness | PARTIAL | unit tests, quotas, seed replay; conditional feasible-space and adaptive pairing pending |
| G5 multi changes | PARTIAL | exact DP brute-force oracle and opt-in worker fixture; noisy null/sealed trials pending |
| G6 conditional interaction | PARTIAL | 2x2 matched cohort, status separation, real worker event; higher-order and full isolation pending |
| G7 campaign holdout | UNVERIFIED FOR V2 FAMILIES | v1 H1 sealed holdout preserved; Holm primitive is not campaign integration |
| G8 measurement protocols | UNIMPLEMENTED | v1 semantics preserved |
| G9 minimization | UNIMPLEMENTED V2 | v1 evidence unchanged |
| G10 CI | PENDING FINAL SHA | Track PR #4 exact SHA |
| G11 95% line coverage | VERIFIED INTERMEDIATE ONLY | `2318928b`: 41/41 CTests, line 96.08%, branch 71.02%; later SHA pending |
| G12 benchmark fairness | PARTIAL | actual worker/CPU/wall scorecard, equal case budget; equal CPU+wall budget missing |
| G13 research uplift | UNVERIFIED | no held-out uplift claim |
| G14 external SOTA | UNVERIFIED | no executed matched external tools |

States are deliberately strict; no intermediate green workflow grants final-branch or SOTA approval.
