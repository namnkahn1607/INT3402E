# C++ to RISC-V compiler

## Quick start

Development environments: Arch Linux and Ubuntu 24.04. The automated CI
baseline is Ubuntu 24.04, GCC and Clang, Debug and Release.

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

From the repository root:

```sh
cmake --preset debug
cmake --build --preset debug --parallel
ctest --preset debug
```

The CLI accepts either a source file or I/O redirection (pipe):

```sh
printf 'int answer = 42;\n' | ./build/debug/src/int3402e
./build/debug/src/int3402e <source-file.cpp>
```

It prints tokens to stdout and diagnostics to stderr.

Presets must be selected explicitly. Use `release` for an optimized build.
CMake chooses the system compiler by default. To choose another compiler,
specify `CXX=<compiler> cmake --preset <preset>` on the first configure of a
fresh build directory.

Tests download GoogleTest **v1.18.0** on first configure. Builds reuse the
downloaded source.

Unit test are colocated within the module it belongs to in [src](src/) - see
[Layout](#source-layout) below. E2E or integration test are put at [tests](tests/).

Build without GoogleTest:

```sh
cmake -S . -B build/no-tests -G Ninja -DBUILD_TESTING=OFF -DCMAKE_BUILD_TYPE=Release
cmake --build build/no-tests --parallel
```

## Source layout

Module                 | Pipeline
---------------------- | ----------------------------------------
[ast](src/ast)         | AST node types
[lexer](src/lexer)     | Source text -> token stream
[parser](src/parser)   | Token stream -> AST
[sema](src/sema)       | Semantic analysis / AST type checking
[ir](src/ir)           | Intermediate representation
[opt](src/opt)         | Optimizations
[codegen](src/codegen) | IR -> RISC-V assembly
[common](src/common)   | Shared utility: diagnostic, reporting...

## CI

GitHub Actions runs on pushes, pull requests, and manual dispatch. It builds and
tests GCC/Clang x Debug/Release with handwritten-code warnings treated as errors,
and checks a build with testing disabled.
