# C++ to RISC-V compiler

Educational compiler project. Currently implements a Flex lexer and a token-dump
CLI; parsing, semantic analysis, and code generation are future work.

## Quick start

Development environments: Arch Linux and Ubuntu 24.04. The automated CI
baseline is Ubuntu 24.04, GCC and Clang, Debug and Release. Windows contributors can use WSL2
with Ubuntu. Other native platforms are not yet validated.

Requirements: CMake >= 3.28, Ninja, a C++20 compiler, Flex >= 2.6, and Git.
On Arch Linux (updates the system as well as installing build tools):

```sh
sudo pacman -Syu --needed base-devel clang cmake ninja flex git
```

On Ubuntu 24.04:

```sh
sudo apt-get update
sudo apt-get install -y build-essential clang cmake ninja-build flex git
```

Both distributions use the same commands below. Ubuntu installations must meet
the CMake >= 3.28 requirement; the package instructions target Ubuntu 24.04.

Clone normally; no submodules are required. From the repository root:

```sh
cmake --preset debug
cmake --build --preset debug --parallel
ctest --preset debug
echo "int answer = 42;\n" | ./build/debug/src/int3402e
```

Replace `debug` with `release` for an optimized build. Presets must be selected
explicitly. CMake chooses the system compiler; to choose Clang, use
`CXX=clang++ cmake --preset debug` on the first configure of a fresh build directory.
Use a separate build directory when changing compilers. Personal presets belong
in ignored `CMakeUserPresets.json`. clangd reads `build/debug/compile_commands.json`.

Tests download GoogleTest **v1.17.0**, pinned to commit
`52eb8108c5bdec04579160ae17225d66034bd723`, on first configure. GitHub access is
required then. Builds reuse the downloaded source. For offline development, pass
`-DFETCHCONTENT_SOURCE_DIR_GOOGLETEST=/absolute/path/to/googletest` to configure
with a local checkout of that commit. Dependency upgrades are explicit reviewed
changes to `tests/CMakeLists.txt`.

To build just the CLI, without downloading GoogleTest:

```sh
cmake -S . -B build/no-tests -G Ninja -DBUILD_TESTING=OFF -DCMAKE_BUILD_TYPE=Release
cmake --build build/no-tests --parallel
```

## Repository layout

```text
CMakeLists.txt              Project settings and component orchestration
CMakePresets.json           Shared configure/build/test commands
include/frontend/lexer/    Public lexer headers
src/main.cpp                Compiler CLI entry point
src/frontend/lexer/        Flex grammar and lexer library build
src/backend/               Backend component registration
tests/frontend/lexer/     Lexer unit tests
tests/                    CLI smoke test and input fixtures
.github/workflows/         Automated build and test checks
build/<preset>/            Ignored generated sources, dependencies, binaries
```

The frontend owns source-language processing: lexing, parsing, AST, and semantic
analysis. The backend owns target-specific lowering and code generation. The CLI
coordinates these components. The backend currently has no implementation target.

Use `include/<area>/<component>/`, `src/<area>/<component>/`, and
`tests/<area>/<component>/`. Keep implementation-only headers beside their sources.
Each implemented component owns a library target and declares its dependencies.
Add `common` only when multiple components need a concrete shared facility;
common code must not depend on frontend or backend code. Cross-stage IR and
optimization boundaries should be defined when those components are implemented.

## How the build works

1. **Configure:** CMake reads the preset, checks the compiler and Flex, creates
   targets and writes Ninja build rules. CTest enables `BUILD_TESTING` by default;
   only that branch fetches GoogleTest and defines test targets.
2. **Generate:** Ninja runs Flex on `src/frontend/lexer/lexer.ll` to produce
   `lexer.yy.cpp` and `lexer.yy.h` under the build directory. CMake tracks these
   outputs so a grammar change reruns Flex before affected targets compile.
3. **Compile and link:** Generated C++ becomes the static `lexer` library
   (`INT3402E::lexer` alias). The CLI and lexer tests link it. C++20 and public
   header paths follow the library dependency. Warning flags are private to
   handwritten-code targets; generated Flex and GoogleTest code are excluded.
4. **Test:** CTest runs the GoogleTest cases and a CLI smoke test that checks
   output and termination. Timeouts prevent an EOF regression from hanging CI.
   Test presets fail if no tests are discovered.

`include/frontend/lexer/scanner.h` exposes the Flex scanner API and supplies its
required state declaration.

## CI and collaboration

GitHub Actions runs on pushes, pull requests, and manual dispatch. It builds and
tests GCC/Clang × Debug/Release with handwritten-code warnings treated as errors,
and checks a build with testing disabled. Action dependencies are pinned to
commits and Dependabot proposes their updates. The token has read-only access.
Local and CI builds use the same presets. This is a reproducible build recipe,
not a bit-for-bit hermetic toolchain: runner packages receive updates.

See [CONTRIBUTING.md](CONTRIBUTING.md) for the review workflow, required GitHub
checks, and build conventions.
