# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Casacore is a C++17 astronomical data processing library used primarily in radio astronomy. It provides core functionality for CASA (Common Astronomy Software Applications) including array handling, table storage, measurement sets, coordinate systems, and image processing.

## Build Commands

```bash
# Configure and build (out-of-source build required)
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug    # or Release
make -j$(nproc)

# Build and run all tests
make check

# Run tests (after building)
ctest                        # all tests
ctest --output-on-failure    # with failure details
ctest -R tArray              # run a single test by name

# Install
make install
```

### macOS Notes

- Prefer Homebrew Bison over the system `/usr/bin/bison` on macOS:
  - `-DBISON_EXECUTABLE=/opt/homebrew/opt/bison/bin/bison`
- `clang`/`clang++` builds are supported and commonly used:
  - `-DCMAKE_C_COMPILER=/usr/bin/clang`
  - `-DCMAKE_CXX_COMPILER=/usr/bin/clang++`

### Key CMake Options

- `-DMODULE=<name>` — Build subset: `casa`, `tables`, `measures`, `ms`, `msfits`, `images`, or `all` (default)
- `-DBUILD_TESTING=ON` — Enable test building (default: YES)
- `-DUSE_HDF5=ON` — Enable HDF5 support (default: NO)
- `-DDATA_DIR=<path>` — Path to Measures data tables (required for measures tests)
- `-DBUILD_PYTHON3=ON` — Build Python 3 bindings (default: YES)
- `-DENABLE_COVERAGE=ON` — Enable gcov/lcov instrumentation (works on GCC/Clang toolchains)

### Coverage Baseline Workflow

Use this for Wave 1 coverage gating before substantive C++ refactors in a module.

```bash
# Configure a fresh coverage build
cmake -S . -B cov \
  -DCMAKE_C_COMPILER=/usr/bin/clang \
  -DCMAKE_CXX_COMPILER=/usr/bin/clang++ \
  -DBISON_EXECUTABLE=/opt/homebrew/opt/bison/bin/bison \
  -DDATA_DIR=/path/to/measures/data \
  -DENABLE_COVERAGE=ON

# Build what you plan to measure (or build all)
cmake --build cov -j$(sysctl -n hw.ncpu)

# Run module-level coverage from inside a build subdirectory
cd cov/casa
CTEST_REGEX="arraytest|tError" CTEST_ARGS="--output-on-failure" ../../build-tools/casacore_cov
# Outputs: testcov.summary + cov/index.html
```

Notes:
- `casacore_cov` can be run from build root, module dir, or `*/test` dir.
- If `CTEST_REGEX`/`CTEST_ARGS` are omitted, it runs all tests visible from the current directory.

## Module Dependency Chain

```
casa  (base: Arrays, IO, Logging, OS, Quanta, Utilities, etc.)
└─ tables  (Table system, TaQL query language, storage managers)
   └─ scimath + scimath_f  (scientific math, Fortran components)
      └─ measures + meas  (coordinates, astronomical calculations)
         ├─ ms + derivedmscal  (Measurement Set format)
         │  └─ msfits  (MS ↔ FITS conversion)
         └─ fits  (FITS file I/O)
      ├─ lattices  (N-dimensional array operations)
      ├─ coordinates  (world coordinate systems, requires WCSLIB)
      └─ images  (uses coordinates, lattices, fits)
```

Additional modules: `mirlib` (MIRIAD format), `python3` (Python bindings), `build-tools` (test infrastructure).

## Code Conventions

- **Namespace**: All code lives in `namespace casacore`
- **C++ standard**: C++17 (C++20 if BUILD_SISCO=ON)
- **Style**: Google C++ Style Guide with 4-space indentation
- **Naming**: PascalCase classes, camelCase methods/variables, `_p` suffix for pointer members
- **Includes**: Always use full path: `#include <casacore/casa/Arrays/Array.h>`
- **Templates**: Implementations in `.tcc` files, headers in `.h`
- **Documentation**: Doxygen markup with `// <summary>`, `// <synopsis>` blocks in headers

## Test Structure

- Each module has a `test/` subdirectory
- Test executables are prefixed with `t` (e.g., `tArray.cc`, `tTable.cc`)
- Tests use `build-tools/casacore_assay` as the test harness (wrapped by `cmake/cmake_assay`)
- The harness auto-handles constrained environments:
  - If `HOME` is missing/non-writable, it creates a writable per-test home under the test working directory.
  - If `CASARCFILES` is not set, it reads `DATA_DIR` from the active build's `CMakeCache.txt` and injects a temporary `measures.directory` resource when `ephemerides/` or `geodetic/` exist there.
- Some tests use `.run` shell scripts, `.out` files for expected output comparison, and `.in` files for input data
- Test CMakeLists.txt pattern: executables listed in a `set(tests ...)` variable, iterated with `foreach`

### Test Data Expectations

- `DATA_DIR` must contain casacore Measures tables, typically with at least:
  - `ephemerides/`
  - `geodetic/`
- If measures tests fail with messages like `Cannot read leap second table TAI_UTC`, `Cannot read table of Observatories`, or `Corrupted JPL table DE200`, verify that `DATA_DIR` points to a populated Measures data tree.
- CI workflows and Docker images fetch `WSRT_Measures.ztar`; local builds must either do the same or point `-DDATA_DIR` at an existing populated directory.

## Compiler Warnings

Default flags: `-Wextra -Wall -W -Wpointer-arith -Woverloaded-virtual -Wwrite-strings -pedantic -Wno-long-long`. Debug builds define `AIPS_DEBUG`; Release builds define `NDEBUG`.

## External Dependencies

Core: BLAS, LAPACK, CFITSIO, WCSLIB, FFTW3, Flex, Bison. Optional: HDF5, ADIOS2, Boost-Python, Readline, SOFA (testing only).
