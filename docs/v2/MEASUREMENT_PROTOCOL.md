# PHASESHIFT v2 — Measurement Protocols

**legacy_retained_outputs_v1** remains the default and is unchanged. It performs oracle correctness calibration, warmups, balanced A/B timing, retained outputs up to 16 MiB, and validates every timed output after the clock. Its multi-step measured iteration selection retains historical semantics.

**calibrated_batch_v2** is experimental and opt-in. A separate bounded pilot selects one exact batch size, validates all pilot outputs and releases them. One final timed batch of that size then executes with output retention. Only the final batch's wall and CPU timings are reported; pilot wall cost and chosen iterations are recorded separately. The final batch checks every output after the clock. Parent process time and pilot cost are not silently counted as final per-operation time.

V2 still includes algorithm output allocation, copying and release within the measured operation; it is not kernel-only. The unsafe shortcut of subtracting output allocation/copy cost is prohibited. No optimization/precision benefit or equivalence to v1 is asserted without held-out measurement.

Unknown versions, including kernel_only_v2, fail closed. Status failures, timeouts, and wrong-answer results never become valid performance measurements. The new protocol is CPU-only.


## Opt-in configuration and persisted identification

Use `measurement.protocol_version` with value `calibrated_batch_v2` only inside an explicitly v2 search configuration. Absent this value, both v1 and v2 retain the historical legacy protocol. The frozen experiment manifest records the measurement version and generated report JSON exposes it. Each v2 calibrated sample's raw SQLite `detail` field also contains its protocol, measured pilot wall time and pilot iteration count, including failed samples when available. Independent CLI tests validate the new protocol, resume consistency and rejection of unsupported/kernel-only modes.

## V2 report-only drift observation

V2 report summaries optionally contain `block_drift_v2`, computed by streaming persisted `samples` grouped by exact case ID and algorithm. Each group requires at least six distinct valid measurement blocks; the first and last third of blocks are compared by median per-operation `ns/batch`. A >=20% early/late change increments a **heuristic descriptive flag**, not a statistical hypothesis rejection, corrected p-value, causal attribution, or automatic invalidation. Duplicate or invalid groups abstain. This is a basic drift instrument; population calibration, cross-run precision comparison, CPU-frequency/thermal controls and the M6 scientific verdict remain outstanding.
