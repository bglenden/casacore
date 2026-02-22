# Wave 1 Coverage Baseline (2026-02-22)

This baseline was collected from a fresh coverage-instrumented build:

```bash
cmake -S . -B cov \
  -DCMAKE_C_COMPILER=/usr/bin/clang \
  -DCMAKE_CXX_COMPILER=/usr/bin/clang++ \
  -DBISON_EXECUTABLE=/opt/homebrew/opt/bison/bin/bison \
  -DDATA_DIR=/Users/brianglendenning/SoftwareProjects/casacore/casacor \
  -DENABLE_COVERAGE=ON \
  -DENABLE_STRICT_WARNINGS=ON
cmake --build cov -j8
```

Coverage collection used `build-tools/casacore_cov` from module build directories and from
the build root.

## Full-Project Baseline (Root Scope)

Run from build root:

```bash
cd cov
CTEST_ARGS='--output-on-failure -E tLSQaips|tLSQFit' ../build-tools/casacore_cov
```

Result:

- Tests: `495/495` passing (`tLSQaips` and `tLSQFit` excluded due known numeric-baseline
  diffs)
- Line coverage: `57.4%` (`143521/250144`)
- Branch coverage: `24.0%` (`148145/617563`)

Notes:

- The full run now includes `tPath`; harness fallback HOME handling was adjusted so
  non-writable HOME environments do not change `tPath` semantics.
- `tLatticeStatistics` is long-running under coverage instrumentation (roughly 4 minutes).
- `genhtml` can encounter missing generated-source paths (for example
  `tables/TaQL/TableGram.ll`); `casacore_cov` now uses `genhtml --filter missing` so HTML
  generation remains non-fatal while summary metrics stay authoritative in
  `testcov.summary`.

## Module Baselines

| Module | Test subset used | Result | Line coverage | Branch coverage |
|---|---|---|---|---|
| `casa` | `arraytest|tTypes|tError|tInput|tParam|tLogSink` | `6/6` passing | `2.8%` (`251/9065`) | `0.5%` (`92/19624`) |
| `tables` | `tDysco|altmantest|dLogging|dLogging2|tLoggerHolder|tLogging` | `6/6` passing | `20.5%` (`1345/6551`) | `3.9%` (`629/16102`) |
| `measures` | `dMeasure|tEarthField|tMeasJPL|measurestest` | `4/4` passing | `15.6%` (`253/1619`) | `7.3%` (`261/3582`) |
| `scimath` | full module tests, excluding known numeric-baseline tests `tLSQaips|tLSQFit` | `58/58` passing | `65.9%` (`9151/13878`) | `16.0%` (`11691/73267`) |

## Notes

- `lcov/genhtml` on this toolchain reports many `inconsistent` warnings with LLVM `gcov`.
  The coverage script now ignores known non-fatal `deprecated`, `gcov`, `inconsistent`,
  and `unused` issues so summary output remains usable.
- The scimath run intentionally excluded `tLSQaips` and `tLSQFit` due known floating
  baseline differences unrelated to this wave's test-registration work.
- This is the Wave 1 starting baseline for targeted-test expansion before major C++
  implementation refactors in lower-coverage/high-risk areas.
