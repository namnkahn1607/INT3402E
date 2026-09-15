# Contributing

## Workflow

- Agree on a small issue and acceptance criteria before substantial work.
- Work on a short-lived branch and open a focused pull request.
- Include the behavior change, validation results, and any limitations.
- Have the other contributor review it; resolve feedback and merge after CI passes.
- Keep dependency/toolchain upgrades separate from feature changes when practical.

In GitHub repository settings, enable a rule for your default branch requiring
pull requests, one approval, resolved conversations, and all four CI build checks
(`g++ / debug`, `g++ / release`, `clang++ / debug`, `clang++ / release`). Select
checks after the first workflow run. Require branches to be up to date, dismiss
stale approvals after new commits, and block force pushes and branch deletion.
Set the rule to Active and avoid bypass permissions for either contributor.
These settings must be enabled on GitHub; adding workflow files does not enforce
review or prevent merging a failing build.

## Before opening a pull request

```sh
cmake --preset debug -DINT3402E_WARNINGS_AS_ERRORS=ON
cmake --build --preset debug --parallel
ctest --preset debug
```

Use the existing `.clang-format` style for edited C++ files. `.clang-tidy` is
advisory tooling at this stage; neither formatting nor static analysis is a CI
gate yet. Agree on an exact formatter version before adding a formatting gate.
Add behavioral tests for fixes/features, keep tests outside production sources,
and do not commit generated files, build output, or personal editor settings.

When adding a library, define its includes and C++ requirements on its target.
Use PUBLIC for requirements consumers need and PRIVATE for implementation details.
Link `int3402e_options` privately to handwritten-code targets to enable project
warnings without imposing those warnings on consumers or generated code.

## Working across Arch and Ubuntu

Use the shared presets on both systems. Keep the Ubuntu 24.04 CI toolchain as
the compatibility baseline: newer Arch compilers can accept code or library
features unavailable to your teammate's compiler. Both GCC and Clang CI checks
must pass before merging. Compiler versions may differ locally; an exact compiler
version match is not required for this project.

After a major compiler upgrade on Arch, configure a new build directory (for
example, `cmake --preset debug -B build/debug-upgrade`), then build and test it
with `cmake --build build/debug-upgrade` and
`ctest --test-dir build/debug-upgrade --output-on-failure --no-tests=error`.
This avoids reusing objects or cached toolchain detection from the old compiler.
Do not share build directories or binaries between machines.

Avoid whole-repository formatting with different clang-format versions: it can
produce unrelated formatting changes even with the same `.clang-format` file.
Agree on a formatter version before standardizing format-on-save or making it a
required CI check. Include compiler and CMake versions when reporting a build
that fails only on one machine.
