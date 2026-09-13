# C++ to RISC-V Compiler

## Toolchain & dependencies

- Compiler: Clang 18.1.3
- Generator: CMake 3.28.3
- Build system: Ninja 1.11.1
- Testing: [GoogleTest](https://github.com/google/googletest) v1.18.0

## Getting started

Clone with submodules in one step:

```bash
git clone --recurse-submodules <repo-url>
cd INT3402E
```

Already cloned without `--recurse-submodule`, pull them in:

```bash
git submodule update --init --recursive
```

Configure, build and test (Debug is the default preset):

```bash
cmake --preset debug
cmake --build --preset debug
ctest --preset debug
```

For a release build, simply swap `debug` for `release` in each command.

## Module structure

The source code is organized by compiler pipeline stages

```
include/<stage>/ - public headers
src/<stage>/     - implementation (including unit tests)
```

Stage                  | Pipeline
---------------------- | -------------------------------------
[ast](src/ast)         | AST node types
[lexer](src/lexer)     | Source text -> token stream
[parser](src/parser)   | Token stream -> AST
[sema](src/sema)       | Semantic analysis / AST type checking
[ir](src/ir)           | Intermediate representation
[opt](src/opt)         | Optimizations
[codegen](src/codegen) | IR -> RISC-V assembly
