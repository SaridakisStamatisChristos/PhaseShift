# Local evidence ledger — 2026-11-20

Environment: Linux 6.18.57 x86-64, GCC 14.2.0, CMake 3.31.6, AMD EPYC 9V74 (virtualized). Toolchain libraries: SQLite 3.46.1, json-c 0.18, OpenSSL 3.5.7.

- `cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release && cmake --build build -j2`: **PASS**.
- `ctest --test-dir build --output-on-failure` (after clean smoke output): **PASS 4/4**, core, synthetic demo, negative control, synthetic search ablation.
- `cmake -S . -B build-sanitize -G Ninja -DCMAKE_BUILD_TYPE=Debug -DPHASESHIFT_SANITIZE=ON && cmake --build build-sanitize -j2 && ASAN_OPTIONS=detect_leaks=1 ctest --test-dir build-sanitize --output-on-failure`: **PASS 4/4** after disabling RLIMIT_AS only under ASan; initial attempt failed due ASan virtual shadow reservation.
- `phaseshift explore --config configs/negative_control.json --out runs/negative`: **PASS**, 16 cases / 32 observations / 0 failures, `NO_CONFIRMED_BOUNDARY`.
- `phaseshift explore --config configs/sorting_quicksort.json --out runs/sorting`: **PASS**, 15 cases / 180 observations / 0 failures, `NO_CONFIRMED_BOUNDARY` (multi-axis confounding; not evidence no pathologies exist).
- `phaseshift explore --config configs/topk_fraction.json --out runs/topk`: **PASS**, 12 cases / 144 observations / 0 failures, `NO_CONFIRMED_BOUNDARY` (multi-axis confounding).
- `phaseshift explore --config configs/graph_density.json --out runs/graph`: **PASS**, 6 cases / 72 observations / 0 failures, `NO_CONFIRMED_BOUNDARY`.
- CTest demo initial report showed engineered threshold between x=7 and x=8, `MINIMIZED_CONFIRMED`, 72 samples including 40 independent-seed holdouts. This is a deterministic work-unit fixture only.

No claims here about remote CI, external deployment, or local experimental superiority. Generated `runs/` and `build*/` artifacts are excluded from Git.

## v0.2 additional evidence

- `ctest --test-dir build --output-on-failure`: **PASS 4/4** (including 64-bin, 24-discovery-case synthetic ablation).
- `ASAN_OPTIONS=detect_leaks=1 ctest --test-dir build-sanitize --output-on-failure`: **PASS 4/4**.
- `phaseshift ablate --config configs/adaptive_sparse.json --out runs/ablation_sparse`: 24 discovery cases for each of grid, random, stratified, and adaptive. Grid abstained (had not reached injected threshold), random confirmed a boundary in bin [18,25] (one-bin localization error), stratified confirmed [18,24], and adaptive confirmed exact [23,24]. Each confirmed approach used 40 additional holdout cases; time and sample counts are in `ablation.csv`. This is one deterministic fixture and does not prove adaptive superiority on real workloads.
- `cmake -S . -B build-clang -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=clang++ && cmake --build build-clang -j2 && ctest --test-dir build-clang --output-on-failure`: **PASS 4/4**, Clang 17.0.0. The first Clang build exposed a sign-conversion warning; corrected the index cast and reverified.
