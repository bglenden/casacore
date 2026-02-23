# Wave Zero Warning Baseline (2026-02-23)

Compiler: Apple clang 17.0 (Xcode default), C++17 mode, Debug build.
Flags: `-Wextra -Wall -W -Wpointer-arith -Woverloaded-virtual -Wwrite-strings -pedantic -Wno-long-long`

## Summary

| Category | Count |
|----------|------:|
| Hand-written source warnings | 15 |
| Generated source warnings (flex/bison `.lcc`/`.ycc`) | 24 |
| Ranlib empty-archive warnings (PCH stubs) | 5 |
| **Total (excluding ranlib/linker noise)** | **39** |

## Per-Module Breakdown (hand-written source only)

| Module | Warnings | Diagnostics |
|--------|----------|-------------|
| `casa` (test) | 2 | `tArrayIter1.cc:342` unused var, `tIPosition.cc:217` self-assign |
| `tables` (test) | 2 | `testtimeblockencoder.cc:138,139` unused vars |
| `scimath` (test) | 4 | `dFunction.cc:215,223,231,239` unused vars |
| `measures` (test) | 1 | `dM1950_2000.cc:53` array-parameter mismatch |
| `lattices` (lib+test) | 2 | `LatticeHistograms.tcc:792` unused var, `tLatticeIterator.cc:95` unused var |
| `derivedmscal` (test) | 1 | `tDerivedMSCal.cc:222` unused var |
| `python` (lib) | 2 | `PycRecord.cc:74`, `PycValueHolder.cc:231` unused vars |
| `images` | 0 | — |
| `ms` | 0 | — |
| `fits` | 0 | — |
| `msfits` | 0 | — |
| **Total** | **14** | |

Note: `lattices/LatticeMath/LatticeHistograms.tcc:792` is the only library (non-test)
warning in hand-written C++ code. The 2 `python/Converters` warnings are in binding code.

## Generated Source Warnings (not directly fixable)

| Module | Count | Diagnostic types |
|--------|------:|------------------|
| `ms` (generated `.ycc`) | 11 | `-Wunused-but-set-variable` (`*nerrs` vars) |
| `ms` (generated `.lcc`) | 11 | `-Wunneeded-internal-declaration` (`yyinput`) |
| `images` (generated) | 2 | Same as above (1 `.ycc` + 1 `.lcc`) |
| **Total** | **24** | |

These are emitted by bison/flex-generated code and cannot be fixed in source. They can be
suppressed per-file with `#pragma clang diagnostic ignored` wrappers or by upgrading to
newer bison/flex versions that produce cleaner output (see Initiative 4).

## Baseline Rule

From this point forward: **no new warnings in changed files.** Any commit that introduces
a new warning in hand-written source code should fix it before merge.
