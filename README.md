# C++ to RISC-V Compiler

## Toolchain & dependencies

- __Compiler__: Clang 18.1.3
- __Generator__: CMake 3.28.3
- __Build system__: ninja 1.11.1
- __Testing__: [GoogleTest](https://github.com/google/googletest) v1.18.0
- __Lexer generator__: [flex](https://github.com/westes/flex) 2.6.4

## Getting started

Clone with submodules in one step:

```bash
git clone --recurse-submodules <repo-url>
cd INT3402E
```

Already cloned without `--recurse-submodule`? Pull them in:

```bash
git submodule update --init --recursive
```

Configure, build and test (Debug is the default preset):

```bash
cmake --preset debug
cmake --build --preset debug
ctest --preset debug
```

Try it out:

```bash
./bin/debug/int3402e <source-file>
```

For a release build, simply swap `debug` for `release` in each command.

## Module structure

The source code is organized by compiler pipeline stages.
Each _stage_ has a corresponding _module_.

```
include/<module>/ - public headers
src/<module>/     - implementation (including unit tests)
```

Module                 | Pipeline
---------------------- | ----------------------------------------
[ast](src/ast)         | AST node types
[lexer](src/lexer)     | Source text -> token stream
[parser](src/parser)   | Token stream -> AST
[sema](src/sema)       | Semantic analysis / AST type checking
[ir](src/ir)           | Intermediate representation
[opt](src/opt)         | Optimizations
[codegen](src/codegen) | IR -> RISC-V assembly
[support](src/support) | Shared utility: diagnostic, reporting...
