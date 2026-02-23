# Wave 1 Coverage Gap Inventory (2026-02-22)

Purpose: prioritize test additions needed before substantial class substitution work.

Baseline source:

- Full-project coverage run from `cov/` with known numeric-baseline exclusions
  `tLSQaips|tLSQFit`.
- Aggregate baseline: `57.4%` lines, `24.0%` branches.

Selection heuristic for initial targets:

- Core library code (exclude `test/`, apps, and wrappers).
- Low line coverage and large uncovered-line count.
- Areas likely to be touched by class/interface substitutions in Wave 1+.

## Initial Priority Targets

| Priority | File | Baseline line coverage | Current focused coverage | Lines | Why first | Status |
|---|---|---:|---:|---:|---|---|
| P1 | `tables/Tables/TableProxy.cc` | `13.6%` | `48.3%` | `2213` | Central proxy behavior and broad downstream blast radius | tranche C complete |
| P1 | `tables/DataMan/StManColumn.cc` | `9.9%` | `94.4%` | `504` | Core storage-manager column path; persistence-sensitive | tranche A complete |
| P1 | `tables/Tables/BaseColumn.cc` | `26.2%` | `68.3%` | `665` | Fundamental table column behavior used pervasively | tranche A complete |
| P1 | `measures/Measures/MeasuresProxy.cc` | `0.0%` | `66.0%` | `576` | Key proxy surface in measures layer, currently unexercised | tranche D complete |
| P2 | `casa/OS/DOos.cc` | `5.7%` | `98.9%` | `265` | Legacy OS abstraction path with low direct validation | tranche B complete |
| P2 | `tables/Tables/NullTable.cc` | `6.7%` | `99.3%` | `135` | Edge-case table behavior and null-object semantics | tranche B complete |

## Execution Update (Tranche A)

- Added and wired new characterization tests:
  - `tables/Tables/test/tTableProxy.cc`
  - `measures/Measures/test/tMeasuresProxy.cc`
  - `tables/Tables/test/tBaseColumnPromotions.cc`
  - `tables/DataMan/test/tStManColumnDispatch.cc`
- New test registrations:
  - `tables/Tables/test/CMakeLists.txt`
  - `measures/Measures/test/CMakeLists.txt`
  - `tables/DataMan/test/CMakeLists.txt`
- Focused validation run: `4/4` passing
  (`tBaseColumnPromotions|tStManColumnDispatch|tTableProxy|tMeasuresProxy`).
- Focused coverage capture for P1 files:
  - `46.5%` lines (`1841/3958`) and `17.6%` branches (`1326/7538`) across the 4-file set.

## Execution Update (Tranche B)

- Added and wired new characterization tests:
  - `casa/OS/test/tDOosCoverage.cc`
  - `tables/Tables/test/tNullTable.cc`
- New test registrations:
  - `casa/OS/test/CMakeLists.txt`
  - `tables/Tables/test/CMakeLists.txt`
- Focused validation run: `6/6` passing
  (`tDOosCoverage|tNullTable|tBaseColumnPromotions|tStManColumnDispatch|tTableProxy|tMeasuresProxy`).
- Focused coverage deltas on P2 files:
  - `casa/OS/DOos.cc`: `5.7%` -> `98.9%`
  - `tables/Tables/NullTable.cc`: `6.7%` -> `99.3%`
- Updated focused 6-file aggregate:
  - `51.3%` lines (`2236/4358`) and `22.2%` branches (`1902/8567`).

## Execution Update (Tranche C)

- Added and wired expanded `TableProxy` characterization test:
  - `tables/Tables/test/tTableProxyAdvanced.cc`
- New test registration:
  - `tables/Tables/test/CMakeLists.txt`
- Focused validation run: `7/7` passing
  (`tDOosCoverage|tNullTable|tBaseColumnPromotions|tStManColumnDispatch|tTableProxy|tTableProxyAdvanced|tMeasuresProxy`).
- Focused coverage delta on the largest remaining P1 file:
  - `tables/Tables/TableProxy.cc`: `29.2%` -> `48.3%`
- Updated focused 6-file aggregate after tranche C:
  - `61.0%` lines (`2660/4358`) and `27.5%` branches (`2356/8567`).

## Execution Update (Tranche D)

- Expanded `MeasuresProxy` characterization coverage in:
  - `measures/Measures/test/tMeasuresProxy.cc`
- Targeted validation run remains green: `7/7` passing
  (`tDOosCoverage|tNullTable|tBaseColumnPromotions|tStManColumnDispatch|tTableProxy|tTableProxyAdvanced|tMeasuresProxy`).
- Focused coverage delta on remaining P1 measures proxy path:
  - `measures/Measures/MeasuresProxy.cc`: `45.8%` -> `66.0%`
- Updated focused 6-file aggregate after tranche D:
  - `63.7%` lines (`2775/4358`) and `30.4%` branches (`2605/8567`).
- Symlink-fixture reliability note:
  - `casa/OS/test/tDOosCoverage.cc` uses directory-relative symlink targets
    (`regular.txt`, `sub`) to avoid install/build-root coupling.

Notes:

- Current percentages are from focused coverage runs of tranche-specific test sets.
- Tranche-B focused captures on clang/LLVM required lcov `--ignore-errors` for known
  `inconsistent/format` parser issues in unrelated templated headers.
- Full-suite recomputation is intentionally deferred until a larger Wave 1 tranche boundary.
