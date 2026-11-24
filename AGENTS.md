## Project context

- A coursework compiler from a C++ subset to RISC-V assembly.
- Compilation correctness and output code quality comes FIRST, efficiency (how fast the compiler
  produces code) is strictly SECOND.

## Architecture

- Linear pipeline stages: Lexer -> Parser -> Sema -> Codegen. Whether the optimization phase exists
  has been decided yet. The source code organized each stage in a single module, plus a shared module
  (diagnostics, common struct/class, procedures shared across modules).
- Only the driver `main()` decides whether to halt the compilation process.
- A singleton diagnostic engine shared across stages that collects all diagnostic messages.
- Parser only produces a pure syntax AST with no semantic calls. Sema is a seperate pass over the
  completed AST (symbol table, type resolution, node annotation).
- AST is a classic polymorphic hierarchy of declarations / statements / expressions: base class with
  virtual destructor, kind enum, children owned via smart pointers. Deliberately not an arena: chosen
  for simplicity and speed of a working demo.

## How to work here

- Smallest diff that solves the problem. Do not rename, reformat, or refactor code you were
  not asked to touch.
- Do not invent APIs, flags, or CI job names. Grep first. If unsure, say so.

## Conventions (when writing code)

- Comments explain why, or state a non-obvious invariant / justification / fact but not restate
  what the code does. No banner or section comments. Do not rewrite an existing comment unless
  the code behavior it describe changes.
