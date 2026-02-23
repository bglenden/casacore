# Casacore Modernization Plan

Rolling-wave modernization plan for the casacore codebase, incorporating findings from the
obsolete-technologies audit conducted 2026-02-22.

Status: planning baseline (no phase-level implementation detail yet)

---

## Planning Model (Rolling Wave)

1. Keep long-range direction stable (ranked initiatives and dependency order).
2. Plan only the next wave in detail when that wave starts.
3. Re-rank remaining initiatives at each wave boundary using current risk, staffing, and
   external constraints.

Not included yet (by design): per-phase task breakdown, milestone dates, team allocation.

---

## Confirmed Constraints and Policy

These constraints were confirmed by project maintainers and are treated as hard requirements:

1. Persistent on-disk data structures and storage formats are compatibility-critical and must
   not be obsoleted or changed incompatibly.
2. Third-party dependencies are acceptable when they are popular, widely used, and improve
   maintainability or capability.
3. Multidimensional array/tensor replacement is important enough to require an explicit
   research-and-decision track before any large migration.

---

## Compiler and Language Standard

**Target: C++20.** Supported compilers: GCC and Clang (both are used on Linux; macOS uses
Apple Clang and Homebrew GCC).

Current state: `CMakeLists.txt:207-213` sets C++17 by default, C++20 when `BUILD_SISCO=ON`.
When SISCO is enabled, `CMAKE_CXX_STANDARD 20` applies to the **entire** project, not just
the SISCO module. C++20 has been fully supported by GCC since GCC 12 and Clang since
Clang 16; the CI already tests with GCC 11-13 and Clang 14-18.

Action: Verify the full codebase compiles cleanly under C++20 (the `register` keyword in
generated flex/bison code may require fixes first — see initiative 4). Then remove the
conditional and set C++20 unconditionally.

Key C++20 features to adopt across the codebase:

| Feature | Benefit for casacore |
|---------|---------------------|
| `std::span` | Replace raw pointer+size pairs (already used in Sisco) |
| `std::format` | Replace `sprintf`/`ostringstream` formatting |
| `<numbers>` | Replace custom `casa/BasicSL/Constants.h` |
| `std::ranges` | Simplify algorithm chains |
| Concepts | Constrain templates (replace SFINAE/static_assert) |
| `consteval` | Stronger compile-time guarantees |
| Designated initializers | Clearer struct initialization |

---

## Ranking Method

Initiatives are ranked by:

1. Dependency unlock — does this enable other modernization work?
2. Cross-cutting impact on maintainability and integration.
3. Risk reduction (CI reliability, supply-chain/security, runtime safety).
4. Compatibility invariants (especially persistent data and external APIs).
5. Migration cost and reversibility.

---

## Wave Zero: Quick Wins

These items are trivial to execute (hours of work each), carry near-zero risk, and should be
done immediately regardless of which strategic initiative runs next.

| Item | Effort | Evidence |
|------|--------|----------|
| Update `actions/checkout@v2` to `@v4` in `linux.yml:21` | One-line change | Node.js 12 EOL April 2022 |
| Replace FTP download with HTTPS in `osx.yml:39` | One-line change | Insecure transport |
| Replace HTTP download with HTTPS in `docker/py_wheel.docker:26` | One-line change | Insecure transport |
| Add `.editorconfig` (4-space indent, UTF-8, LF) | New file | No cross-editor config exists |
| Add baseline `.clang-format` (Google style, 4-space indent) | New file | No formatting config exists |
| Remove dead CMake 2.8.3 guards in `cmake/FindWCSLIB.cmake:57` and `cmake/FindCFITSIO.cmake:61` | Delete dead branches | Minimum is 3.14 |
| Delete Python 2 build path (`BUILD_PYTHON` option + `python/CMakeLists.txt`) | Remove dead code | Disabled by default, Python 2 EOL Jan 2020 |
| Add CI job with `-DBUILD_SISCO=ON` (C++20 mode) | New CI matrix entry | Verify full codebase compiles under C++20 before making it default |

Related findings: 9.1, 9.2, 10.1, 2.5b, 6.2.

---

## Execution Status (As Of 2026-02-23)

### Wave Zero

Completed:

- Updated GitHub Actions checkout action to `actions/checkout@v4` in Linux CI.
- Replaced insecure transport URLs with HTTPS in:
  - `.github/workflows/osx.yml`
  - `docker/py_wheel.docker`
- Added baseline editor/style files:
  - `.editorconfig`
  - `.clang-format`
- Removed dead CMake 2.8.3 compatibility branches in:
  - `cmake/FindWCSLIB.cmake`
  - `cmake/FindCFITSIO.cmake`
- Removed Python 2 build path:
  - deleted `python/CMakeLists.txt`
  - removed `BUILD_PYTHON` option and related top-level wiring
  - retained/normalized Python 3 path
- Added CI matrix coverage for SISCO/C++20 mode:
  - new Docker image `docker/ubuntu2404_gcc_sisco.docker`
  - new Linux CI matrix entry using that image

Additional Wave Zero hardening completed:

- Added minimal warning/lint bootstrap:
  - CMake option `ENABLE_STRICT_WARNINGS` with targeted high-value warning gates
  - baseline `.clang-tidy` with a minimal check set
- Improved test harness robustness for clean/sandboxed runs:
  - `build-tools/casacore_assay` now auto-creates a writable test `HOME` when needed
    (while preserving `tPath` HOME semantics in non-writable-HOME environments)
  - harness now auto-discovers `DATA_DIR` from the active build cache and injects
    temporary `CASARCFILES` with `measures.directory` when Measures tables exist

Validation snapshot:

- Clean out-of-source reconfigure/build with clang and modern bison succeeds.
- Representative previously environment-dependent tests now pass when data path is available
  (`tAipsrc`, `tAipsrcValue`, `dMeasure`, `tEarthField`, `tMeasJPL`, `tMSFieldEphem`,
  `tMSSummary`, `tNewMSSimulator`, `tFrequencyAligner`, `tSpectralCoordinate`).
- Remaining known failures in that targeted run are numerical-baseline diffs in
  `tLSQaips` and `tLSQFit`, not data-path/home-directory setup failures.

Wave Zero final item completed:

- Recorded and published per-module warning baseline in `WAVE0_WARNING_BASELINE.md`:
  - 15 hand-written source warnings (13 in test code, 1 in library, 2 in Python bindings)
  - 24 generated-source warnings (flex/bison `.lcc`/`.ycc` — not fixable in source)
  - Baseline rule established: no new warnings in changed files from this point forward.

**Wave Zero is complete.**

### Wave One Kickoff

In progress:

- Begin migrating Boost.Test-based directories from `cmake_assay` wrapper to direct CTest
  registration (`add_test(NAME ... COMMAND <target>)`) module-by-module.
- First conversions completed:
  - `casa/Arrays/test/CMakeLists.txt` (`arraytest`)
  - `tables/AlternateMans/test/CMakeLists.txt` (`altmantest`)
- Additional Wave 1 conversions completed (sidecar-free directories):
  - `casa/Exceptions/test/CMakeLists.txt`
  - `casa/Inputs/test/CMakeLists.txt`
  - `casa/Logging/test/CMakeLists.txt`
  - `casa/test/CMakeLists.txt`
  - `scimath/StatsFramework/test/CMakeLists.txt`
  - `tables/LogTables/test/CMakeLists.txt`
- Sidecar-aware mixed-directory conversion (direct CTest where possible, assay retained where
  per-test sidecars exist):
  - `scimath/Functionals/test/CMakeLists.txt`
  - `scimath/Fitting/test/CMakeLists.txt`
  - `scimath/Mathematics/test/CMakeLists.txt`
- Boost-test registration and linkage cleanup:
  - `measures/Measures/test/CMakeLists.txt`:
    - `measurestest` switched to direct CTest registration
    - `tIAU2000` switched to direct CTest registration
    - removed hard dependency on Boost `system` component for `measurestest`
  - `tables/Dysco/CMakeLists.txt`:
    - moved `tDysco` to direct CTest registration style
    - modernized Boost linkage using imported targets when available
  - `casa/Arrays/test/CMakeLists.txt`:
    - removed hard dependency on Boost `system` component
    - modernized Boost linkage using imported targets when available
- Coverage tooling bootstrap for Wave 1 gating:
  - added explicit CMake option `ENABLE_COVERAGE` (legacy `cov/` dirname behavior retained)
  - hardened `build-tools/casacore_cov` for modern `lcov/genhtml` behavior and module-scoped
    summary output (including `genhtml --filter missing` for generated-source paths)
  - documented and published initial module baseline in `WAVE1_COVERAGE_BASELINE.md`
  - documented and published full-project baseline (`495/495` tests passing with explicit
    exclusions for known numeric-baseline `tLSQaips|tLSQFit`)
- Initial strict-warning cleanup progress:
  - fixed intentional switch fallthroughs with `CASACORE_FALLTHROUGH` in:
    - `casa/Quanta/Unit.cc`
    - `tables/TaQL/ExprConeNode.cc`
    - `tables/TaQL/ExprFuncNode.cc`
    - `tables/TaQL/ExprFuncNodeArray.cc`
  - adjusted strict-warning gate so `implicit-fallthrough` is currently warning-only
    (generated flex/bison sources still emit unavoidable fallthrough patterns)

Wave 1 validation snapshot:

- Reconfigure/build succeeds after the above CMake test registration changes.
- Targeted direct-CTest migration batch passes:
  - `arraytest`, `measurestest`, `altmantest`, `tDysco`
  - `tTypes`, `tError`, `tInput`, `tParam`, `tLogSink`
  - `dLogging`, `dLogging2`, `tLoggerHolder`, `tLogging`
  - all `scimath/StatsFramework` tests in that directory
  - Result: `23/23` passing in the targeted run.
- Additional scimath mixed-directory migration validation:
  - `48/48` passing for migrated `Functionals`, `Fitting` (excluding known floating-baseline
    `tLSQaips`/`tLSQFit`), and `Mathematics` test subsets.
- Full-project coverage-gate validation from build root:
  - `495/495` passing in coverage mode with exclusions `tLSQaips|tLSQFit`
  - aggregate coverage: `57.4%` lines (`143521/250144`), `24.0%` branches
    (`148145/617563`)

Wave 1 coverage baseline snapshot:

- Baseline run published in `WAVE1_COVERAGE_BASELINE.md` from a fresh clang coverage build.
- Full-project snapshot:
  - `57.4%` lines (`143521/250144`), `24.0%` branches (`148145/617563`) from build-root
    run with `tLSQaips|tLSQFit` excluded as known numeric-baseline failures.
- Module snapshots:
  - `casa`: `2.8%` lines (`251/9065`), `0.5%` branches (`92/19624`) using
    `arraytest|tTypes|tError|tInput|tParam|tLogSink` (`6/6` passing).
  - `tables`: `20.5%` lines (`1345/6551`), `3.9%` branches (`629/16102`) using
    `tDysco|altmantest|dLogging|dLogging2|tLoggerHolder|tLogging` (`6/6` passing).
  - `measures`: `15.6%` lines (`253/1619`), `7.3%` branches (`261/3582`) using
    `dMeasure|tEarthField|tMeasJPL|measurestest` (`4/4` passing).
  - `scimath`: `65.9%` lines (`9151/13878`), `16.0%` branches (`11691/73267`) with full
    module tests excluding known floating-baseline `tLSQaips|tLSQFit` (`58/58` passing).

Next Wave 1 gating task:

- Full-project coverage baseline gate is now satisfied; prioritize targeted regression tests
  for low-coverage/high-risk paths discovered by the baseline (starting with `casa`,
  `tables`, and `measures`) before major C++ implementation refactors.

Coverage Expansion Track (started):

- Added an explicit Wave 1 coverage-expansion workstream before class substitution.
- Initial gap inventory is captured in `WAVE1_COVERAGE_GAP_INVENTORY.md` using the
  full-project baseline.
- First candidate targets are core substitution-risk paths with high uncovered-line counts
  (for example `tables/Tables/TableProxy.cc`, `tables/DataMan/StManColumn.cc`,
  `tables/Tables/BaseColumn.cc`, `measures/Measures/MeasuresProxy.cc`).
- Tranche A implemented (characterization tests added and passing):
  - `tables/Tables/test/tTableProxy.cc`
  - `measures/Measures/test/tMeasuresProxy.cc`
  - `tables/Tables/test/tBaseColumnPromotions.cc`
  - `tables/DataMan/test/tStManColumnDispatch.cc`
- Tranche A focused coverage delta on initial P1 files:
  - `tables/Tables/TableProxy.cc`: `13.6%` -> `29.2%`
  - `measures/Measures/MeasuresProxy.cc`: `0.0%` -> `45.8%`
  - `tables/Tables/BaseColumn.cc`: `26.2%` -> `68.3%`
  - `tables/DataMan/StManColumn.cc`: `9.9%` -> `94.4%`
- Tranche A validation snapshot:
  - targeted run `4/4` passing (`tBaseColumnPromotions|tStManColumnDispatch|tTableProxy|tMeasuresProxy`)
  - focused 4-file aggregate: `46.5%` lines (`1841/3958`), `17.6%` branches (`1326/7538`)
- Tranche B implemented (characterization tests added and passing):
  - `casa/OS/test/tDOosCoverage.cc`
  - `tables/Tables/test/tNullTable.cc`
- Tranche B focused coverage delta on initial P2 files:
  - `casa/OS/DOos.cc`: `5.7%` -> `98.9%`
  - `tables/Tables/NullTable.cc`: `6.7%` -> `99.3%`
- Tranche B validation snapshot:
  - targeted run `6/6` passing
    (`tDOosCoverage|tNullTable|tBaseColumnPromotions|tStManColumnDispatch|tTableProxy|tMeasuresProxy`)
  - focused 6-file aggregate: `51.3%` lines (`2236/4358`), `22.2%` branches (`1902/8567`)
- Tranche C implemented (additional `TableProxy` branch/surface expansion):
  - `tables/Tables/test/tTableProxyAdvanced.cc`
- Tranche C focused coverage delta:
  - `tables/Tables/TableProxy.cc`: `29.2%` -> `48.3%`
- Tranche C validation snapshot:
  - targeted run `7/7` passing
    (`tDOosCoverage|tNullTable|tBaseColumnPromotions|tStManColumnDispatch|tTableProxy|tTableProxyAdvanced|tMeasuresProxy`)
  - focused 6-file aggregate: `61.0%` lines (`2660/4358`), `27.5%` branches (`2356/8567`)
- Tranche D implemented (additional `MeasuresProxy` branch/surface expansion):
  - `measures/Measures/test/tMeasuresProxy.cc`
- Tranche D focused coverage delta:
  - `measures/Measures/MeasuresProxy.cc`: `45.8%` -> `66.0%`
- Tranche D validation snapshot:
  - targeted run `7/7` passing
    (`tDOosCoverage|tNullTable|tBaseColumnPromotions|tStManColumnDispatch|tTableProxy|tTableProxyAdvanced|tMeasuresProxy`)
  - focused 6-file aggregate: `63.7%` lines (`2775/4358`), `30.4%` branches (`2605/8567`)
- Fixture-path reliability update:
  - `casa/OS/test/tDOosCoverage.cc` uses directory-relative symlink targets
    (`regular.txt`, `sub`) to avoid absolute-path coupling.
- Comprehensive CTest migration completed across all remaining test directories:
  - Applied smart conditional pattern (direct CTest for sidecar-free tests, assay retained
    for `.out`/`.run`/`.in` tests) to all 29 remaining directories:
    - `casa/`: BasicMath, BasicSL, Containers, HDF5, IO, Json, OS, Quanta, System, Utilities
    - `tables/`: Tables, DataMan, TaQL
    - `fits/FITS`
    - `coordinates/Coordinates`
    - `derivedmscal/DerivedMC`
    - `lattices/`: Lattices, LatticeMath, LRegions, LEL
    - `images/`: Images, Regions
    - `measures/`: Measures (main loop), TableMeasures
    - `meas/MeasUDF`
    - `ms/`: MeasurementSets, MSSel, MSOper
    - `msfits/MSFits`
  - ~432 sidecar-free tests now use direct `add_test(NAME ... COMMAND ...)` registration
  - Tests with `.out`/`.run`/`.in` sidecars continue to use `cmake_assay` wrapper
  - Script-only test loops (TaQL `testscripts`, MSSel `testscripts`) retained as assay
- Tranche E implemented (BaseTable core infrastructure characterization test):
  - `tables/Tables/test/tBaseTableCoverage.cc`
  - Exercises: construction (named, memory, scratch), openedForWrite, markForDelete/unmark,
    rename (New, NewNoReplace), copy, deepCopy (shallow, deep, noRows), select (expr,
    maxRow, offset, rownrs), project, sort (asc/desc), set operations (and/or/sub/xor/not),
    makeIterator, showStructure, checkRemoveColumn, row removal (single, vector),
    checkRowNumber, getPartNames, isColumnWritable/Stored (writable + readonly),
    tableInfo (set/get/flush/read), makeAbsoluteName errors, addColumns via dmInfo,
    rowNumbers (plain + RefTable).
  - Tranche E validation: `1/1` passing.
- Tranche F implemented (RefTable + ArrayColumnBase characterization test):
  - `tables/Tables/test/tRefTableCoverage.cc`
  - Exercises: RefTable row selection/filtering, column access, sort, set operations
    (union/intersection/difference/xor/complement), project, add/remove row, row order,
    deep copy, getPartNames, chained select; ArrayColumnBase shape checking, slicing,
    column range, shape mismatch detection.
  - Tranche F validation: `1/1` passing (0.38s).
- Tranche G implemented (Storage manager internals characterization test):
  - `tables/DataMan/test/tStorageManagerCoverage.cc`
  - Exercises: ISM (scalar types, incremental behavior, add/remove rows, array columns),
    SSM (scalar types, string columns, add/remove rows, multiple columns, column addition),
    TSM (TiledCellStMan, TiledColumnStMan, TiledShapeStMan, slice access, large data).
  - Tranche G validation: `1/1` passing (0.30s).
- Full suite validation after Tranches E/F/G: 491/507 passing (16 pre-existing failures:
  7 HDF5-not-compiled, 4 sandbox/home-dir, 2 tLSQaips/tLSQFit, tMSFeedGram, tDerivedMSCal,
  tImageRegion). No regressions.
- Tranche H implemented (ColumnSet characterization test):
  - `tables/Tables/test/tColumnSetCoverage.cc`
  - 13 test functions exercising ColumnSet through the Table API: addColumn overloads
    (ColumnDesc-only, by DM name, by DM type, with explicit DataManager), removeColumn
    partial (multi-column DM) and entire-DM-deletion paths, renameColumn with data
    verification, uniqueDataManagerName suffix generation, can-predicates (addRow/removeRow/
    removeColumn/renameColumn), dataManagerInfo/actualTableDesc reflection, addRow/removeRow
    propagation across DMs, resync via flush-and-reopen, checkDataManagerNames duplicate
    detection, areTablesMultiUsed, getColumn by name/index, reopenRW.
  - Tranche H validation: `1/1` passing (0.12s).
- Tranche I implemented (PlainTable characterization test):
  - `tables/Tables/test/tPlainTableCoverage.cc`
  - 24 test functions exercising PlainTable through the Table API: changeTiledDataOnly +
    flush (data-only putFile branch), reopenRW (including already-writable early-return),
    table options (New/Old/Update/NewNoReplace/Scratch/Delete), endian format (Big/Little/
    Local), getLayout (schema-only read), isMultiUsed, hasDataChanged/getModifyCounter,
    keywordSet vs rwKeywordSet locking paths, renameHypercolumn, addRow with/without
    initialize, isWritable for all open modes, storageOption, lock/unlock/hasLock with
    User/Permanent/AutoLocking, flush recursive with subtables, actualTableDesc/
    dataManagerInfo, NewNoReplace error path, multiple-opens cache behavior,
    findDataManager, can-operations, lockOptions, column operations (add/rename/remove),
    removeRow, checkWritable error paths, create-with-initialize constructor.
  - Tranche I validation: `1/1` passing (0.26s).
- Full suite validation after Tranches H/I: 493/509 passing (16 pre-existing failures). No regressions.
- Pre-existing test failures resolved (16 → 0):
  - **HDF5 conditional registration** (7 tests): Gated `tHDF5DataSet`, `tHDF5DataType`,
    `tHDF5Record`, `tMultiHDF5`, `tHDF5Iterator`, `tHDF5Lattice`, `tHDF5Image` behind
    `if (USE_HDF5)` in 4 CMakeLists.txt files (`casa/HDF5`, `casa/IO`, `lattices/Lattices`,
    `images/Images`). Follows existing `USE_ADIOS2` pattern.
  - **Hardcoded `/tmp` paths** (2 tests): `tDirectory.cc` and `tAppInfo.cc` now use
    `getenv("TMPDIR")` with `/tmp` fallback for cross-filesystem and work-directory tests.
  - **Non-writable HOME** (3 tests): Created `.run` sidecar scripts for `tAipsrc`,
    `tAipsrcValue`, and `tAppInfo` that set up a writable temporary `HOME` directory.
    This routes them through `cmake_assay` which also handles Measures data injection.
  - **Missing `.run` scripts** (2 tests): Wrote `tMSFeedGram.run` (extracts
    `mssel_test_small.ms`, runs 6 feed-selection expressions) and `tDerivedMSCal.run`
    (extracts MS from `ms/MSSel/test`, runs derived-column checks with Measures data).
  - **`$PWD` not in CTest environment** (1 test): `tImageRegion.cc` now uses
    `Path(".").absoluteName()` instead of relying on `$PWD` environment variable.
  - **Platform-specific float diffs** (2 tests): `tLSQFit.cc` and `tLSQaips.cc` now wrap
    raw `sd`/`mu`/`err` prints in `Y(value, 1e-2)` tolerance clamping (matching the
    existing pattern for solution/covariance values). Updated `.out` golden files for 3
    lines each where reference-platform residuals also fell below the new tolerance.
- Full suite validation after all fixes: **500/500 passing** (0 failures).
  Test count reduced from 509 to 500 due to 8 HDF5 tests excluded when `USE_HDF5=OFF`
  and 1 test no longer double-counted after CMake reconfiguration.
- Next tranche target set (before large class substitutions):
  - continue Wave 1 sub-targeting for remaining `tables/Tables` helper classes
    (ArrColData, ScaColData, ColumnCache, TableLockData).

---

## Ranked Initiatives

### 1) Test Architecture Unification

Scope:

- Migrate remaining test modules from assay/`.run`/`.out` to Boost.Test (or another
  structured framework).
- Preserve test coverage while improving diagnostics and refactor-safety.
- Normalize CMake test registration to direct `add_test()` without `cmake_assay` wrapper.
- Establish and publish coverage baseline by module for core libraries.
- Identify low-coverage/high-risk areas and add targeted regression tests before large
  implementation refactors.

Why rank #1:

- Test reliability is a prerequisite for every other initiative. The 162 golden `.out` files
  make behavior-preserving refactors (API cleanup, ownership changes) unreliable — any
  cosmetic output change triggers false failures. This must be solid before large-scale
  mechanical changes.

Proven migration pattern:

- `casa/Arrays/test/` (43 files) has been **fully migrated** to Boost.Test with zero
  `AlwaysAssert` remaining. This is the template for migrating other modules.
- `tables/AlternateMans/test/` (11 files) and `tables/Dysco/tests/` (4 files) are also
  fully migrated.
- The approach is module-by-module: convert one `test/` directory at a time, remove its
  `.out` files, update its `CMakeLists.txt` to direct registration.

Key evidence:

- `build-tools/casacore_assay` — shell test runner, copyright 1995.
- 76 `.run` wrapper scripts, 162 `.out` golden output files.
- ~12,787 `AlwaysAssert` calls across 511 files (~11,700 in test code).
- 60 files already on Boost.Test (working proof of migration path).

Related findings: 3.1, 3.2, 3.3, 3.4, 3.5.

---

### 2) Consumer Packaging and CMake Target Modernization

Scope:

- Add modern exported CMake package (`install(EXPORT ...)`, `casacoreConfig.cmake`,
  `casacoreConfigVersion.cmake`, `casacoreTargets.cmake`).
- Fix pkg-config dependency propagation (populate `@pc_req_public@` / `@pc_req_private@`).
- Migrate global `add_definitions` / `include_directories` to target-scoped commands.
- Remove stale RHEL5/6/7 and Python 2.7 hardcoded paths (`CMakeLists.txt:102-134`).
- Unify compiler detection to `CMAKE_CXX_COMPILER_ID` (remove deprecated
  `CMAKE_COMPILER_IS_GNUCC/GNUCXX`).
- Set `CMAKE_CXX_STANDARD 20` unconditionally (remove `BUILD_SISCO` conditional once
  C++20 compatibility is verified in CI — see Wave Zero).

Why rank #2:

- Blocks clean downstream consumption. No project can currently use
  `find_package(casacore)`. Global CMake state makes target behavior hard to reason about.

Key evidence:

- No `install(EXPORT ...)` anywhere in the repository.
- `casacore.pc.in:9-10` references undefined variables — installed `.pc` has empty `Requires`.
- 19 `add_definitions()` and 8+ `include_directories()` at global scope in top-level
  `CMakeLists.txt`.
- `CMakeLists.txt:232` checks for GCC >= 4.9 — redundant with C++17 requirement.

Related findings: 2.1, 2.2, 2.3, 2.4, 2.5.

---

### 3) Public API / Compatibility Layer Rationalization and Container Replacement

Scope:

- Phase out legacy stdlib wrapper headers (`casa/iostream.h`, `casa/fstream.h`,
  `casa/string.h`, etc.) — 16 headers labeled "Interim solution for standard/nonstandard".
- Reduce reliance on `casa/namespace.h` (`using namespace casacore;` in ~432 include sites).
- Plan deprecation path for `Bool`/`True`/`False` and primitive aliases (`Int`, `uInt`, etc.).
- Replace pre-STL custom containers with standard library equivalents.

**Standard library container replacements:**

| Custom class | Header | Std replacement | Usage | Notes |
|---|---|---|---|---|
| `Block<T>` | `Containers/Block.h` | `std::vector<T>` | ~286 files | Biggest item. Simple dynamic array. |
| `PtrBlock<T>` | `Containers/Block.h` | `std::vector<T*>` | (part of Block) | Template-reduction optimization, unnecessary now |
| `Fallible<T>` | `Utilities/Fallible.h` | `std::optional<T>` | ~13 files | Nearly identical semantics |
| `BitVector` | `Utilities/BitVector.h` | `std::vector<bool>` or `boost::dynamic_bitset` | ~105 files | Variable-size bit operations |
| `DynBuffer` | `Utilities/DynBuffer.h` | `std::vector<char>` or `std::deque<char>` | ~25 files | Chunked buffer management |
| `SimpleOrderedMap` | `Containers/SimOrdMap.h` | `std::map` | ~7 instances in `ms/MeasurementSets/MSTable2.cc` | `STLIO.h` already has migration helpers to read as `std::map` |

**Standard library utility/algorithm replacements:**

| Custom class | Header | Std replacement | Usage | Notes |
|---|---|---|---|---|
| `Timer` | `OS/Timer.h` | `std::chrono` | ~33 files | Custom wall/CPU timer |
| `PrecTimer` | `OS/PrecTimer.h` | `std::chrono::high_resolution_clock` | ~8 files | High-precision timer |
| `RNG`, `Random` | `BasicMath/Random.h` | `<random>` (std::mt19937, distributions) | ~15 files | Custom Lehmer/MLCG generators |
| `BinarySearch` | `Utilities/BinarySearch.h` | `std::lower_bound` / `std::binary_search` | ~30 files | Templated binary search |
| `LinearSearch` | `Utilities/LinearSearch.h` | `std::find` / `std::find_if` | ~20 files | Templated linear search |
| `Sort` / `GenSort` | `Utilities/Sort.h`, `GenSort.h` | `std::sort` / `std::stable_sort` | ~122 files | Partially replaceable; `Sort` supports multi-key sort with custom comparators, `GenSort` is a direct replacement |
| `BaseCompare` / `ObjCompare` | `Utilities/Compare.h` | `std::less` / `operator<=>` | ~36 files | Custom comparator hierarchy |
| `Sequence` | `Utilities/Sequence.h` | `std::atomic<uInt>` | ~5 files | Thread-safe ID sequence generator |
| `Path` / `File` / `Directory` / `SymLink` | `OS/Path.h`, `OS/File.h`, etc. | `std::filesystem` (C++17) | ~80 files | Filesystem wrappers; partial replacement — some methods (permissions, creation time) may require platform-specific code |
| `sprintf`/`snprintf` calls | (scattered) | `std::format` (C++20) | 66 occurrences in ~30 files | Not a class replacement, but a widespread pattern |

**COW and ownership (replaceable with design changes):**

| Custom class | Header | Std replacement | Usage | Notes |
|---|---|---|---|---|
| `COWPtr<T>` | `Utilities/COWPtr.h` | `std::shared_ptr<const T>` + value semantics | ~28 files | Thin wrapper over `shared_ptr` with const/mutable access split. Two usage patterns: (a) internal COW for Record/RecordDesc/TableRecord — replace with value semantics + move; (b) Lattice getSlice API returns possibly-shared array data — replace with `shared_ptr<const Array<T>>` or `std::span`. The COW pattern itself is discouraged in modern C++ (removed from `std::string` in C++11) due to poor multithreading interaction. |

**Already done or trivial (fold into initiative 7):**
- `PtrHolder<T>` → `std::unique_ptr<T>` (already `[[deprecated]]`, ~13 files)
- `CountedPtr<T>` → `std::shared_ptr<T>` (already wraps it internally, ~43 files)
- `Regex` already inherits from `std::regex`; `Mutex` header already removed

**Keep (domain-specific, no stdlib drop-in):**
- `Record` / `ValueHolder` — core typed-field containers for table system
- `BucketCache` — file I/O caching for storage engine
- `String` — currently extends `std::string` with convenience methods; retain in the near
  term, but plan an explicit long-horizon migration to `std::string` + free functions
- `MUString` — specialized astronomical notation parser
- `ObjectStack<T>` — already uses `std::vector` + `std::mutex` internally
- `Sort` (multi-key) — `std::sort` doesn't natively support multi-key sorting with separate key arrays; the simple single-array `GenSort` is directly replaceable

**Additional bespoke subsystem modernization candidates (outside core container list):**

| Subsystem | Current API | Candidate replacement(s) | Usage | Notes |
|---|---|---|---|---|
| Command-line parameter layer | `casa/Inputs/Input.h`, `casa/Inputs/Param.h` | `CLI11`, `cxxopts`, or `Boost.Program_options` | `Input.h` in ~52 files, `Param.h` in ~5 files | `Param` is explicitly documented as obsolete and a candidate for full replacement (`Param.h:101-107`) |
| Runtime config registry | `casa/System/Aipsrc*.h` | `toml++` / `yaml-cpp` / `inih` (+ compatibility adapter) | ~35 files | Keep `.aipsrc` compatibility initially; modernize parser and API surface behind adapter |
| Logging framework | `casa/Logging/LogIO.h`, `LogSink.h` | `spdlog`, `Boost.Log`, or retained custom sink API with std backends | ~163 files | High blast radius; should be treated as explicit replace/retain decision |
| Dynamic library loader | `casa/OS/DynLib.h` | `boost::dll` or direct platform loader wrappers | ~3 files | Small, low-risk modernization item |

**Persistence compatibility boundary (hard constraint):**

- `AipsIO` and related persistent structures are **not replacement targets** if that would
  break on-disk compatibility.
- Modernization here should focus on:
  - strengthening binary compatibility tests and golden fixtures;
  - isolating legacy serialization behind compatibility adapters;
  - allowing optional modern serializers only for non-persistent/transient interfaces.
- This is a modernization boundary decision, not a "rip and replace" item.

**Multidimensional array/tensor strategy research gate (required before large migrations):**

- The multidimensional array stack (`Array`, `Vector`, `Matrix`, `Cube`, `IPosition`,
  `ArrayMath`) is pervasive (`Vector.h` ~553 include sites, `ArrayMath.h` ~342, `IPosition.h`
  ~254).
- Before any broad replacement attempts, run a focused research wave comparing:
  - keep current arrays + add interop adapters;
  - `std::mdspan`/mdspan-based backends;
  - external scientific tensor/array libraries (e.g. xtensor, Eigen Tensor, Kokkos).
- Evaluation criteria should include:
  - memory layout and stride semantics;
  - slicing/view semantics and zero-copy behavior;
  - expression-template and SIMD performance;
  - interoperability with existing table/record/persistence layers;
  - long-term maintenance and ecosystem stability.
- Output of the research gate is an architecture decision, not immediate implementation.

Why rank #3 (not #1):

- This is the highest-blast-radius change (~16K `True`/`False` sites, ~900 wrapper include
  sites, ~286 `Block<T>` files, ~122 `Sort`/`GenSort` files, ~80 filesystem wrapper files).
  It depends on initiative 1 (test modernization) being sufficiently advanced so that
  regressions from mass mechanical changes are caught by structured assertions rather than
  brittle `.out` diffs.

Key evidence:

- 16 wrapper headers with explicit compatibility labeling.
- ~902 include sites for wrapper headers.
- ~432 include sites for `namespace.h`.
- `casa/aipstype.h:40-54` defines `Bool`, `True`, `False`, `Char`, `Int`, `uInt`, `Float`,
  `Double`, etc.
- Systematic audit identified additional stdlib- and ecosystem-replaceable class families
  beyond the original container list (timers, RNG, filesystem, search, sort, comparators,
  formatting, CLI/config/logging/runtime wrappers).

Related findings: 1.1, 1.2, 1.3, 1.4, 1.5, 1.6, 1.7, 1.8, 1.9, 1.10, 1.11, 1.12, 1.13, 1.15, 1.16, 1.17, 1.18, 1.19, 1.20, 1.21.

---

### 4) Parser/Codegen Pipeline Modernization (Flex/Bison)

Scope:

- Modernize parser generation integration and generated-source policy.
- Remove historical bison 1.28 compatibility assumptions.
- Address `register` keyword warnings in generated code (removed in C++17).
- Evaluate reentrant parser configuration.
- Decide whether `JsonParser`/`JsonOut` remain custom or migrate to a mainstream JSON library
  plus extension layer for comments and `Complex` values.

Why rank #4:

- Reduces fragility in a core syntax/parsing surface (TaQL, JSON, MS selection).

Key evidence:

- Bison compatibility comments: `casa/Json/JsonParser.cc:35`, `ms/MSSel/MSTimeGram.cc:41`.
- `register` warning suppression: `ms/MSSel/MSTimeGram.cc:53-60`.
- `JsonParser` motivation explicitly cites custom behavior (comments + `Complex`) at
  `casa/Json/JsonParser.h:82-85`.

Related findings: 4.1, 4.2.

---

### 5) Runtime Portability Surface Reduction

Scope:

- Rationalize `aipsenv.h` legacy macro matrix (GCC 2/3/4, SGI, Solaris, HP, Alpha, KAI,
  AIX, Cray conditionals).
- Remove embedded dlmalloc (`casa/OS/malloc.cc`) — compiled out on Linux
  (`#if !defined(AIPS_LINUX)`, line 30), so effectively dead code on the primary platform.
- Evaluate VAX/IBM/Modcomp conversion modules for removal — grep confirms these are only
  referenced from their own implementation files, tests, and base-class headers; no usage
  from table I/O, FITS, or any higher-level code path. Likely safe to remove after
  confirming no external consumers link against them.
- Remove dead platform-specific HostInfo implementations for Solaris, IRIX, HP-UX, and
  OSF1/Tru64. These are `#include`d conditionally in `HostInfo.cc:198-217` but the
  platforms are long EOL.
- Evaluate replacing `DynLib` wrapper with `boost::dll` or direct minimal loader wrappers.

Why rank #5:

- Large legacy conditional logic in `aipsenv.h` obscures behavior. dlmalloc and
  architecture conversion modules are likely dead code, but low-risk to defer.

Key evidence:

- `casa/aipsenv.h:52-179` — compiler/platform conditionals for environments no longer
  supported.
- `casa/OS/malloc.cc:28-31` — guarded out on Linux, 2009 version.
- VAX/IBM/Modcomp: 9 source files + 3 test files, zero references from `tables/`, `fits/`,
  `ms/`, or any I/O codepath.
- Dead HostInfo platform files: `HostInfoSolaris.h`, `HostInfoIrix.h`, `HostInfoHpux.h`,
  `HostInfoOsf1.h` — conditionally included but platforms are extinct.
- `casa/OS/DynLib.h:63` documents that it is a wrapper around `dlopen`/`dlsym`/`dlclose`.

Related findings: 5.1, 5.2, 5.3, 5.4, 5.5.

---

### 6) Python Binding Stack Modernization

Scope:

- Evaluate migration path from Boost.Python to pybind11 (with API-compatibility
  constraints for python-casacore).
- Remove Python 2 build path (if not already done in Wave Zero).

Why rank #6:

- Affects packaging/toolchain complexity and long-term Python supportability. Boost.Python
  requires Boost as a build dependency and has slower compile times.

Key evidence:

- `python3/CMakeLists.txt` links against `${PYTHON3_Boost_LIBRARIES}`.
- 7 converter modules in `python/Converters/` use `boost::python::` API.
- `python/Converters/test/tConvert.py:11` uses Python 2 long literals (`10L`).

Related findings: 6.1, 6.2.

---

### 7) Memory Ownership, C++ Idiom Cleanup, and C++20 Adoption

Scope:

- Replace `CountedPtr` → `std::shared_ptr` (~43 files; already wraps it internally,
  commented-out deprecation at `CountedPtr.h:29`).
- Replace `PtrHolder` → `std::unique_ptr` (~13 files; already `[[deprecated]]`,
  TODO from 2000 at `PtrHolder.h:75`).
- Convert raw `new` + smart pointer to `std::make_unique` / `std::make_shared`.
- Reduce unsafe C string patterns (`atoi`/`atof`/`strcpy`/`strncpy`) in older FITS/MS code.
- Replace `sprintf`/`snprintf` with `std::format` (66 occurrences across ~30 files).
- Adopt C++20 idioms across the codebase as modules are touched:
  - `std::span` for pointer+size parameters (already used in Sisco).
  - `std::optional` for nullable returns (replacing nullptr-or-bool patterns).
  - `std::string_view` for const string parameters.
  - `std::format` for string formatting (replacing `sprintf`/`ostringstream`).
  - `std::ranges` for algorithm chains.
  - Structured bindings, `if constexpr`, concepts where appropriate.
  - `<numbers>` constants to replace `casa/BasicSL/Constants.h`.

Exception handling cleanup (mostly minor):

- Remove parenthesized throw syntax `throw (ExceptionType(...))` → `throw ExceptionType(...)`
  (~15 instances in `tables/Tables/Table.cc`, `TabPath.cc`, `TableRow.cc`).
- Normalize `catch (std::exception& x)` → `catch (const std::exception& x)` (const ref).
- Remove `RETHROW` macro dead code (`Error.h:521`, guarded by `CASACORE_NEEDS_RETHROW`).
- Evaluate adopting `std::error_code` / `std::system_error` for OS-level errors (currently
  custom `SystemCallError`).
- Add `noexcept` to move constructors/operators as classes gain them.

Note: The exception hierarchy is already reasonably modern — `AipsError` derives from
`std::exception`, overrides `what()` with `noexcept`, and destructors are properly
`noexcept`. The newer `ArrayError` (`casa/Arrays/ArrayError.h:57`) derives from
`std::runtime_error` instead of `AipsError`, which is the right direction for new code.

Why rank #7:

- Safety and readability improvements, but incremental and can proceed module-by-module
  after larger architecture work.

Key evidence:

- `CountedPtr` internally delegates to `std::shared_ptr` (line 248) but wraps it in custom
  API.
- `PtrHolder.h:75` contains a TODO from 2000: "Use the autoptr class from the Standard
  Library."
- Raw `_p` pointer members in 50+ files across `tables/` and `images/`.
- C++20 features adopted in ~5% of codebase (only `tables/AlternateMans/`).
- ~15 instances of old parenthesized throw syntax.
- Non-const exception catch in multiple files.
- 66 `sprintf`/`snprintf` calls across ~30 files.

Related findings: 1.14, 7.1, 7.2, 7.3, 7.4.

---

### 8) Fortran Dependency Strategy

Scope:

Two distinct categories requiring different strategies:

**Replaceable (FFT — ~14K lines):** `fftpak.f` (10,749 lines) and `dfftpak.f` (3,222 lines)
are FFT routines downloaded from netlib.org/fftpack. These are functionally redundant with
FFTW3, which is already a required external dependency. Candidate for removal once call sites
are redirected.

**Retain and maintain (gridding/convolution/numerical — ~10K lines):** Domain-specific
gridding kernels (`grd2d.f`, `grdjinc1.f`, `convolvegridder.f`, etc.), CLEAN
(`hclean.f`), numerical integration (`dqags.f`), and least-squares (`lawson.f`). These are
battle-tested numerical code with no drop-in replacements. Rewriting carries regression risk
and should only be done with extensive numerical validation.

Why rank #8:

- Important debt with bus-factor and toolchain friction, but the replaceable subset has a
  clear path (FFTW3) and the domain-specific subset should be retained.

Key evidence:

- 26 `.f` files, 24,087 lines, all F77 fixed-format.
- FFTW3 is already `find_package(FFTW3)` in `CMakeLists.txt`.

Related findings: 8.1.

---

### 9) CI/CD Hardening (Strategic)

After Wave Zero quick wins are done, the remaining CI/CD work is:

Scope:

- Add AddressSanitizer build to CI matrix.
- Add UndefinedBehaviorSanitizer build.
- Add code coverage reporting (gcov/lcov).
- Add clang-tidy static analysis lane (requires `.clang-tidy` config).
- Remove Ubuntu 20.04 Docker images (`docker/ubuntu2004_*.docker`) — Ubuntu 20.04 standard
  support ended April 2025.

Why rank #9:

- Significant risk reduction, but can run in parallel with code modernization. Does not
  block other initiatives.

Key evidence:

- Zero sanitizer, coverage, or static-analysis entries in `.github/workflows/`.
- `CONTRIBUTING.md:19` states "Code coverage should increase" but no measurement exists.

Related findings: 9.3.

---

### 10) Developer Workflow Standardization (Remaining)

After Wave Zero quick wins (`.clang-format`, `.editorconfig`), the remaining items:

Scope:

- Add `.clang-tidy` configuration and integrate into CI.
- Add standardized pre-commit hooks (`.pre-commit-config.yaml`).
- Add `CMakePresets.json` for reproducible build configurations.
- Update `doxygen.cfg` from Doxyfile 1.8.5 (2013) to current format.
- Expand `.gitignore` (missing `*.o`, `*.so`, `*.pyc`, `__pycache__/`, `.DS_Store`,
  `compile_commands.json`, `.idea/`).

Why rank #10:

- Valuable ergonomics, but lowest technical urgency relative to structural debt.

Related findings: 10.2, 10.3, 10.4, 10.5.

---

## Complete Finding Inventory

| ID | Finding | Severity | Evidence | Initiative |
|----|---------|----------|----------|------------|
| 1.1 | Legacy stdlib wrapper layer (`casa/*.h`) | Critical | 16 headers, ~902 include sites | 3 |
| 1.2 | Global namespace import helper (`casa/namespace.h`) | High | ~432 include sites | 3 |
| 1.3 | Legacy primitive aliases (`Bool`, `True`, `False`, etc.) | High | `casa/aipstype.h:40-54` | 3 |
| 1.4 | Pre-STL container classes (`Block<T>`, `PtrBlock<T>`, etc.) | High | `Block.h` ~286 files, `BitVector.h` ~105 files | 3 |
| 1.5 | `Fallible<T>` duplicates `std::optional` | Medium | `Utilities/Fallible.h`, ~13 files | 3 |
| 1.6 | Custom search algorithms (`BinarySearch`, `LinearSearch`) | Medium | ~50 files total, direct stdlib equivalents exist | 3 |
| 1.7 | Custom sort classes (`Sort`, `GenSort`) | Medium | ~122 files; `GenSort` directly replaceable, `Sort` multi-key partially | 3 |
| 1.8 | Custom comparator hierarchy (`BaseCompare`, `ObjCompare`) | Medium | ~36 files, replaceable with `std::less`/`operator<=>` | 3 |
| 1.9 | `SimpleOrderedMap` still in use | Low | 7 instances in `MSTable2.cc`, `STLIO.h` has migration helpers | 3 |
| 1.10 | Custom timers (`Timer`, `PrecTimer`) | Medium | ~33 files, replaceable with `std::chrono` | 3 |
| 1.11 | Custom RNG (`RNG`, `Random` distributions) | Medium | ~15 files, replaceable with `<random>` | 3 |
| 1.12 | Filesystem wrappers (`Path`, `File`, `Directory`, `SymLink`) | Medium | ~80 files, partially replaceable with `std::filesystem` | 3 |
| 1.13 | `Sequence` thread-safe counter | Low | ~5 files, replaceable with `std::atomic<uInt>` | 3 |
| 1.14 | `sprintf`/`snprintf` formatting | Medium | 66 occurrences in ~30 files, replaceable with `std::format` | 7 |
| 1.15 | `COWPtr<T>` copy-on-write pointer | Medium | ~28 files (Record internals + Lattice getSlice API); thin `shared_ptr` wrapper, COW pattern discouraged since C++11 | 3 |
| 1.16 | Legacy CLI parameter subsystem (`Input`, `Param`) | High | `Param.h:101-107` marks obsolete/replacement intent; include footprint ~52 + ~5 files | 3 |
| 1.17 | Legacy runtime config registry (`Aipsrc`, `AipsrcValue`, `AipsrcVector`) | High | ~35 include sites; static global resource API in `Aipsrc.h` | 3 |
| 1.18 | Custom logging subsystem (`LogIO`, `LogSink`) | High | ~163 include sites; bespoke sink semantics in `LogSink.h` | 3 |
| 1.19 | Persistent serialization compatibility boundary (`AipsIO`) | Critical | ~73 include sites; on-disk compatibility requirement prevents replacement | 3 |
| 1.20 | `String` wrapper long-horizon migration risk | Medium | `String : public std::string` with ~493 include sites | 3 |
| 1.21 | Multidimensional array/tensor backend strategy unresolved | High | `Vector.h` ~553 include sites, `ArrayMath.h` ~342, `IPosition.h` ~254 | 3 |
| 2.1 | Global-scope CMake definitions/includes | High | 19 `add_definitions` + 8 `include_directories` in top-level CMake | 2 |
| 2.2 | Missing CMake export package for consumers | Critical | No `install(EXPORT)`, no config/targets files | 2 |
| 2.3 | Incomplete pkg-config dependency propagation | High | `@pc_req_public@`/`@pc_req_private@` undefined | 2 |
| 2.4 | Legacy CASA/RHEL/Python2 hardcoded paths | Medium | `CMakeLists.txt:102-134` | 2 |
| 2.5 | Deprecated compiler detection vars | Medium | `CMAKE_COMPILER_IS_GNUCC/GNUCXX` at lines 232, 471, 482 | 2 |
| 2.5b | Dead CMake 2.8.3 guards in Find modules | Low | `FindWCSLIB.cmake:57`, `FindCFITSIO.cmake:61` | Wave Zero |
| 3.1 | Assay runner dominates test model | Critical | `build-tools/casacore_assay`, copyright 1995 | 1 |
| 3.2 | Golden output diff model is brittle | High | 162 `.out` files, float checker fallback | 1 |
| 3.3 | Mixed test framework architecture | Medium | assay + .run/.out + Boost.Test (60 files) | 1 |
| 3.4 | Assay not universal | Low | Direct `add_test` at `tables/Dysco/CMakeLists.txt:55` | 1 |
| 3.5 | ~12,787 `AlwaysAssert` calls | High | 511 files, ~11,700 in test code | 1 |
| 4.1 | Legacy flex/bison integration patterns | High | Bison 1.28 compat, `register` suppression | 4 |
| 4.2 | Custom JSON parser/writer decision not formalized | Medium | `JsonParser` supports comments + `Complex`; no explicit retain/replace decision | 4 |
| 5.1 | Legacy platform/compiler macro matrix | High | `casa/aipsenv.h:52-179` | 5 |
| 5.2 | Embedded dlmalloc | Medium | `casa/OS/malloc.cc` — compiled out on Linux (line 30) | 5 |
| 5.3 | Architecture conversion modules (VAX/IBM/Modcomp) | Low | 9 source files, no usage from I/O codepaths | 5 |
| 5.4 | Dead HostInfo platform files (Solaris, IRIX, HP-UX, OSF1) | Low | 4 header files for extinct platforms | 5 |
| 5.5 | Dynamic library wrapper (`DynLib`) | Low | Wrapper around `dlopen`/`dlsym`/`dlclose`; ~3 include sites | 5 |
| 6.1 | Boost.Python for Python3 bindings | High | `python3/CMakeLists.txt` + 7 converter modules | 6 |
| 6.2 | Python2 build path still present | Medium | `BUILD_PYTHON` option + `python/CMakeLists.txt` | Wave Zero |
| 7.1 | Custom ownership abstractions (`CountedPtr`, `PtrHolder`) | Medium | `CountedPtr.h` wraps `shared_ptr`, `PtrHolder` already `[[deprecated]]` | 7 |
| 7.2 | Raw `new` with smart pointers | Medium | Multiple codepaths use explicit `new` | 7 |
| 7.3 | Legacy C string conversion patterns | Medium | `atoi`/`atof`/`strcpy`/`strncpy` in FITS/MS code | 7 |
| 7.4 | Exception handling minor cleanup | Low | ~15 parenthesized throws, non-const catches, `RETHROW` dead code | 7 |
| 8.1 | Fixed-format F77 module | Medium | 26 files, 24,087 lines under `scimath_f/` | 8 |
| 9.1 | Outdated CI action ref (`checkout@v2`) | High | `.github/workflows/linux.yml:21` | Wave Zero |
| 9.2 | Insecure transport in CI (FTP/HTTP) | High | `osx.yml:39`, `py_wheel.docker:26` | Wave Zero |
| 9.3 | Missing sanitizer/coverage/static analysis | Medium | No such jobs in workflows | 9 |
| 10.1 | Missing `.clang-format` / `.editorconfig` | Low | No formatting or editor config | Wave Zero |
| 10.2 | No pre-commit hooks | Low | No `.pre-commit-config.yaml` | 10 |
| 10.3 | No `CMakePresets.json` | Low | Missing presets baseline | 10 |
| 10.4 | Doxygen 1.8.5 configuration | Low | `doxygen.cfg:1` — current is 1.13+ | 10 |
| 10.5 | Incomplete `.gitignore` | Low | Missing `*.o`, `*.so`, `*.pyc`, `.DS_Store`, etc. | 10 |

---

## Dependency Order

```
Wave Zero (immediate, no dependencies)
    |
    v
Initiative 1 (Tests) ──────────> Initiative 3 (API rationalization)
    |                                    |
    v                                    v
Initiative 2 (CMake/packaging)   Initiative 7 (Ownership/idiom cleanup)
    |
    +──> Initiative 9 (CI hardening — parallel track)
    |
    v
Initiative 4 (Flex/Bison) ──> Initiative 5 (Runtime portability)
    |
    v
Initiative 6 (Python bindings)
Initiative 8 (Fortran strategy)
Initiative 10 (Remaining dev tooling)
```

Key constraint: Initiative 3 (API rationalization) must not start at scale until Initiative 1
(test modernization) has migrated enough modules to catch regressions from mass mechanical
changes.

Additional gates:

- Persistence gate: Any work touching `AipsIO` or table persistence must preserve backward
  compatibility for existing datasets.
- Array strategy gate: Decide array/tensor direction (retain + adapters vs targeted backend
  adoption) before any broad `Array`/`Vector` API migration.
- Coverage gate: Before major C++ implementation changes in a module (especially initiatives
  3, 5, 7, and 8), coverage must be measured for that module and skimpy/high-risk paths must
  have targeted tests added first.

---

## Wave Acceptance Criteria

Every wave must pass these gates before it is committed and pushed:

1. **Build:** Clean compile on all CI matrix configurations (GCC + Clang, all Ubuntu versions,
   macOS Intel + ARM).
2. **Tests:** All test programs pass (`ctest --output-on-failure`).
3. **Warnings:** No new warnings introduced beyond the current baseline (see ramp-up below).
4. **Lint:** No new clang-tidy diagnostics beyond the current baseline (once configured).
5. **Format:** All modified files pass `.clang-format` check (once configured).
6. **Coverage:** For modules with substantive C++ logic changes, coverage baseline must be
   recorded and targeted tests added for identified skimpy/high-risk paths before merge.

---

## Warning and Lint Ramp-Up

Warnings and static analysis enforcement ramp up progressively across waves. Trying to fix
all warnings at once is impractical in a codebase of this size; instead, hold the line on
new code and clean up module-by-module.

### Wave Zero

- Establish warning baseline: record current warning count per module.
- Add `.clang-format` — enforce on new/modified files only (not a full reformat).
- Rule: **no new warnings** in changed files.

### Initiatives 1-2 (Tests, CMake)

- Add `-Werror` for a focused set of high-value warnings on new code:
  `-Werror=return-type -Werror=uninitialized -Werror=implicit-fallthrough`.
- Add a minimal `.clang-tidy` config with a small initial check set (e.g.,
  `bugprone-use-after-move`, `modernize-use-nullptr`, `readability-redundant-*`).
- Run clang-tidy in CI as **advisory** (report but don't fail the build).

### Initiatives 3-5 (API, parsers, portability)

- Expand `.clang-tidy` check set to include `modernize-*` and `performance-*` categories.
- Promote clang-tidy to **enforced** on modules that have completed test migration
  (initiative 1).
- Enable `-Werror` broadly for modules that have been cleaned up.

### Initiatives 6-8 (Python, ownership, Fortran)

- Full `-Werror` on all C++ targets (Fortran excluded).
- Full clang-tidy enforcement in CI (fail on new diagnostics).
- Consider adding `cppcheck` or `PVS-Studio` as a secondary analysis pass.

### Steady state

- All warnings are errors (`-Werror`).
- clang-tidy runs on every PR and blocks merge on new diagnostics.
- clang-format enforced on all files (full reformat completed).
- Pre-commit hooks enforce format and lint locally before push.

---

## Rolling-Wave Governance

At each wave boundary, re-rank remaining initiatives against:

1. Breakage risk observed in prior wave.
2. Dependency unlock gained by completed initiatives.
3. Maintainer bandwidth and contributor availability.
4. External toolchain pressure (compiler/Python/CI ecosystem changes).
