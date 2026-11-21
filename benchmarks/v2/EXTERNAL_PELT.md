# Matched external PELT detection comparison (experimental)

Run `cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release && cmake --build build -j2`, then `ctest --test-dir build -R v2_noisy_holdout_segmentation_challenge --output-on-failure` to generate the frozen 20-seed C++ datasets under `build/ctest_v2_noisy/`.

In an isolated Python environment install [ruptures 1.1.10](https://pypi.org/project/ruptures/1.1.10/) and run:

```bash
python3 benchmarks/v2/external_ruptures.py \
 --dataset-root build/ctest_v2_noisy \
 --out build/ctest_v2_noisy/ruptures_comparison.json
```

This is **not** a fair full PHASESHIFT search SOTA challenge. It is only a shared-input, shared squared-error, shared minimum segment and penalty comparison of fixed offline change-point detection, and only rows with at most three external changes share the C++ solver's K bound. There is **no** valid inference about total CPU, wall time, detection precision, recall, or overall engine uplift from this adapter. All results, including unmatching objectives and negative outcomes, are recorded.

Source: `tests/v2_noisy_detection.cpp` writes the frozen 20-seed challenge data and C++ scores. The optional external adapter requires network access for a third-party dependency and does not run as part of the default 95% test gate.
