# Three-Address Code (TAC) IR Specification

## Purpose and Team Contract

This specification defines the textual Three-Address Code (TAC) intermediate representation produced by AST lowering and consumed by RISC-V code generation. It serves as the standalone interface between parallel workstreams:

- **`nlnk` (Front-half):** Semantic analysis (`sema`), AST-to-TAC lowering (`ir`), and a reference TAC interpreter.
- **`bmngxn` (Back-half):** Textual TAC reader, RV32IM RISC-V assembly code generator (`codegen`), runtime `print` implementation, and simulator harness (e.g., QEMU).

### Differential Testing Oracle

For any semantically valid MiniC++ source program, the stdout output and process exit code of **`nlnk`'s TAC interpreter** and **`bmngxn`'s compiled RV32IM binary under QEMU** must match byte-for-byte.

---

## Value Model and Type Erasure

1. **Uniform runtime type:** Every value in TAC (variables, temporaries, and integer literals) is a **signed 32-bit two's-complement integer (`int32_t`)**, covering `[-2147483648, 2147483647]`.
2. **Boolean type erasure:** Source `bool` values are erased to `int32_t` during AST-to-TAC lowering:
   - `false` lowers to `0`.
   - `true` lowers to `1`.
   - Comparison instructions (`eq`, `gt`, `ge`) always write exact `0` or `1` to their destination.
   - Conditional jumps (`ifFalse`, `ifTrue`) test zero (`== 0`) vs. nonzero (`!= 0`).
3. **Arithmetic wrapping:** `add` and `mul` compute **32-bit two's-complement wrap-around** (`mod 2^32` signed).
   - **Interpreter requirement:** Perform `add` and `mul` using `uint32_t` arithmetic before converting back to `int32_t` so C++ signed-overflow undefined behavior never occurs.
   - **RISC-V target requirement:** Codegen targets **`RV32IM`** (32-bit RISC-V with the `M` extension for `mul`; RV32 `add`/`mul` natively wrap modulo `2^32`).
4. **Output (`print`) semantics:** `print <val>` writes the signed decimal representation of `<val>` followed by a single newline (`'\n'`) to stdout.
   - Because `bool` is type-erased to `0`/`1`, `print(true)` outputs `1\n` and `print(false)` outputs `0\n` (matching C++ `std::cout << flag << '\n'` without `std::boolalpha`).

---

## Lexical Structure and Alpha-Renaming

### Disjoint Identifier Namespaces

TAC enforces disjoint token patterns for every entity kind so a simple single-token-lookahead reader has zero ambiguity:

| Token | Pattern | Examples | Purpose |
| :--- | :--- | :--- | :--- |
| `VAR` | `[A-Za-z]+[0-9]*\.[0-9]+` | `x.1`, `x.2`, `flag.3`, `add.1` | Alpha-renamed source variables |
| `TEMP` | `t[0-9]+` | `t0`, `t1`, `t42` | Compiler-generated temporaries |
| `LABEL` | `L[0-9]+` | `L0`, `L1`, `L12` | Branch target labels |
| `INT` | `-?[0-9]+` | `0`, `1`, `42`, `-2147483648` | Signed 32-bit integer constants |
| Keywords | `add`, `mul`, `eq`, `gt`, `ge`, `goto`, `ifFalse`, `ifTrue`, `print` | `ifFalse` | Case-sensitive opcodes and control words |
| Punctuation | `=`, `:` | `=`, `:` | Assignment separator and label suffix |

### Unconditional Variable Alpha-Renaming

During AST-to-TAC lowering, **every** source `VarDecl` is unconditionally renamed to `<source_name>.<unique_id>` (e.g., `x.1`, `x.2`, `i.3`), using a strictly positive monotonic ID (`1, 2, ...`), even when the variable is never shadowed.

- **Scope elimination:** Inner-block declarations that shadow an outer variable (`int x = 1; { int x = 2; }`) produce distinct names (`x.1` and `x.2`). Codegen treats every `VAR` as a flat, global-to-program storage slot with zero lexical scoping rules.
- **Collision freedom:** Because `VAR` always contains `.<digits>` while `TEMP` (`t<digits>`), `LABEL` (`L<digits>`), and keywords never contain `.`, legal source variable names like `t0`, `L0`, `add`, or `goto` become `t0.1`, `L0.1`, `add.1`, `goto.1` and can never collide with TAC syntax.
- **Trade-off:** IR dumps are slightly noisier (`x.1` instead of `x`), in exchange for a simpler reader and completely scope-free codegen.

### Lexical Formatting Rules

- Each line contains at most one `<item>`.
- Within `<item>`, adjacent tokens are separated by one or more spaces/tabs (`[ \t]+`).
- In `<label-def>`, `:` is attached directly to `LABEL` with **no whitespace** (`L0:`, not `L0 :`).
- `=` is always surrounded by whitespace (`t0 = add x.1 1`).
- Comments begin with `#` and run to the end of the line. Blank and comment-only lines are ignored.
- Line endings may be `\n` or `\r\n`; the final line may omit a trailing newline (`EOF`).

---

## Formal Grammar (BNF)

Quoted strings are exact terminals, `epsilon` is empty, and `EOF` is end of input.

```bnf
<program>     ::= <line-list> EOF

<line-list>   ::= <line> <line-list> | epsilon

<line>        ::= <opt-ws> <item> <opt-ws> <opt-comment> <line-end>
                | <opt-ws> <opt-comment> <line-end>

<item>        ::= <label-def>
                | <copy-inst>
                | <binop-inst>
                | <goto-inst>
                | <cond-goto>
                | <print-inst>

<label-def>   ::= LABEL ":"

<copy-inst>   ::= <dst> "=" <val>

<binop-inst>  ::= <dst> "=" <binop> <val> <val>

<binop>       ::= "add" | "mul" | "eq" | "gt" | "ge"

<goto-inst>   ::= "goto" LABEL

<cond-goto>   ::= "ifFalse" <val> "goto" LABEL
                | "ifTrue"  <val> "goto" LABEL

<print-inst>  ::= "print" <val>

<dst>         ::= VAR | TEMP

<val>         ::= VAR | TEMP | INT

<opt-comment> ::= "#" <non-newline>* | epsilon
<opt-ws>      ::= [ \t]*
<line-end>    ::= "\r\n" | "\n" | EOF
```

Notes:
- `<val>` permits negative `INT` (`-?[0-9]+`) within `int32_t` range (`-2147483648` to `2147483647`). Although `docs/CFG.md` currently has no unary `-`, supporting signed `INT` lets standalone TAC tests and future constant folding express negative literals.
- An empty program (zero `<item>` lines) is valid.
- A `<label-def>` may appear as the last `<item>` in a program with no instruction after it (branching to it terminates execution).

---

## Instruction Semantics and RISC-V Reference

Let `val(x)` denote the `int32_t` runtime value of operand `x` (either the literal value if `INT`, or the current value stored in slot `x`).

| Form | Semantics | Reference RV32IM Lowering Sketch |
| :--- | :--- | :--- |
| `L0:` | Defines branch target `L0` at the next instruction (or program end). | `.L0:` |
| `dst = v` | `slot[dst] = val(v)` | `lw`/`li` into reg, `sw` to `dst` |
| `dst = add a b` | `slot[dst] = wrap32(val(a) + val(b))` | `add rd, rs1, rs2` |
| `dst = mul a b` | `slot[dst] = wrap32(val(a) * val(b))` | `mul rd, rs1, rs2` |
| `dst = eq a b` | `slot[dst] = (val(a) == val(b)) ? 1 : 0` | `xor rd, rs1, rs2; seqz rd, rd` |
| `dst = gt a b` | `slot[dst] = (val(a) > val(b)) ? 1 : 0` | `slt rd, rs2, rs1` |
| `dst = ge a b` | `slot[dst] = (val(a) >= val(b)) ? 1 : 0` | `slt rd, rs1, rs2; xori rd, rd, 1` |
| `goto L0` | Unconditional jump to `L0`. | `j .L0` |
| `ifFalse v goto L0` | If `val(v) == 0`, jump to `L0`; else fall through. | `beqz rs, .L0` |
| `ifTrue v goto L0` | If `val(v) != 0`, jump to `L0`; else fall through. | `bnez rs, .L0` |
| `print v` | Print `val(v)` as signed decimal + `'\n'` to stdout. | Load `val(v)` to `a0`, `call __print_int` |

---

## Well-Formedness and Execution Rules

1. **Unique labels:** Every `LABEL` in `<label-def>` must be defined at most once in the program.
2. **Resolved branches:** Every `LABEL` referenced by `goto`, `ifFalse`, or `ifTrue` must have a matching `<label-def>` in the program.
3. **32-bit literal range:** Every `INT` literal must parse within `[-2147483648, 2147483647]`; out-of-range literals are malformed TAC.
4. **Implicit slot allocation:** There is no declaration instruction. The reader/codegen allocates one 4-byte slot for each distinct `VAR` and `TEMP` appearing in the program.
5. **Self-aliasing:** `<dst>` may be the same `VAR` or `TEMP` as either operand in `<copy-inst>` or `<binop-inst>` (e.g., `x.1 = add x.1 1`). Both operands are read before `<dst>` is written.
6. **Uninitialized reads are invalid:** Reading a `VAR` or `TEMP` on a dynamic path before any write to that slot is a runtime/lowering error.
   - **Zero-initialization at lowering:** AST-to-TAC lowering MUST emit an explicit `x.<id> = 0` instruction for every uninitialized `VarDecl` (`int x;`, `bool flag;`). Consequently, `nlnk`'s TAC interpreter (which traps on reads of unassigned slots to catch lowering bugs) and `bmngxn`'s RV32IM stack slots never silently diverge.
7. **Entry and exit:** Execution begins at the first `<item>` in source order and terminates cleanly with process exit code `0` when control falls off the end of the program (including empty programs or branching to a trailing `<label-def>`).

---

## Canonical AST Lowering Patterns

Each statement/expression from `docs/CFG.md` lowers straightforwardly:

- **Variable declaration (`VarDecl`):**
  - Uninitialized `int x;` or `bool flag;` -> `x.1 = 0` / `flag.2 = 0`.
  - Initialized `int x = <expr>;` -> `<t_expr> = ...; x.1 = <t_expr>` (or directly `x.1 = 1` for a simple literal).
  - Shadowed `bool x = true;` -> `x.2 = 1`.
- **Assignment (`AssignStmt`):**
  - `x = <expr>;` -> evaluate `<expr>` into `<t_val>`, then emit `x.k = <t_val>`.
- **If / Else (`IfStmt`):**
  - Without `else`:
    ```text
    <cond -> t_c>
    ifFalse t_c goto L_end
    <then_branch>
    L_end:
    ```
  - With `else`:
    ```text
    <cond -> t_c>
    ifFalse t_c goto L_else
    <then_branch>
    goto L_end
    L_else:
    <else_branch>
    L_end:
    ```
- **Do-While (`DoWhileStmt`):**
  ```text
  L_body:
  <body>
  <cond -> t_c>
  ifTrue t_c goto L_body
  ```
- **For (`ForStmt`):**
  ```text
  <init>
  L_cond:
  <cond -> t_c>
  ifFalse t_c goto L_end
  <body>
  <step>
  goto L_cond
  L_end:
  ```
- **Print (`PrintStmt`):**
  - Evaluate `<expr>` into `<t_val>`, then emit `print <t_val>`.

---

## End-to-End Example

### MiniC++ Source

```cpp
int x = 1 + 2 * 3;
bool flag = x >= 7 == true;
if (flag) {
    int x = 10;
    print(x);
} else {
    print(0);
}
for (int i = 0; 2 > i; i = i + 1) {
    do {
        x = x + 1;
    } while (x == 8);
}
print(x);
```

### Lowered TAC

```text
# int x = 1 + 2 * 3;
t0 = mul 2 3
t1 = add 1 t0
x.1 = t1

# bool flag = x >= 7 == true;
t2 = ge x.1 7
t3 = eq t2 1
flag.2 = t3

# if (flag) { int x = 10; print(x); } else { print(0); }
ifFalse flag.2 goto L0
x.3 = 10
print x.3
goto L1
L0:
print 0
L1:

# for (int i = 0; 2 > i; i = i + 1) { ... }
i.4 = 0
L2:
t4 = gt 2 i.4
ifFalse t4 goto L3
L4:
t5 = add x.1 1
x.1 = t5
t6 = eq x.1 8
ifTrue t6 goto L4
t7 = add i.4 1
i.4 = t7
goto L2
L3:

# print(x);
print x.1
```

Expected stdout (both TAC interpreter and QEMU RISC-V):

```text
10
10
```

---

## Key Design Trade-offs

| Choice | Alternative | Why chosen & trade-off |
| :--- | :--- | :--- |
| **Both `ifFalse` and `ifTrue`** | Only `ifFalse` (negating condition via `eq c 0` for `do-while`) | `do-while` emits a single `ifTrue` (`bnez` in RISC-V) instead of an extra temporary + comparison + `beqz`; costs ~3 lines in the reader/codegen. |
| **Prefix `<dst> = <op> a b`** | Infix `<dst> = a + b` | Uniform whitespace-split parsing for all binary ops (`add`, `mul`, `eq`, `gt`, `ge`) with zero precedence or operator-token ambiguity. |
| **Unconditional `<name>.<id>` renaming** | Rename only on shadow, or keep `decl` scopes | Eliminates all scope tracking in codegen and prevents valid MiniC++ identifiers (`t0`, `L0`, `add`) from colliding with TAC tokens, at a minor cost to IR readability. |
| **Type-erased `print`** | Typed `print_int` / `print_bool` | Keeps TAC completely untyped (`int32_t` everywhere); `print(bool)` emits `0`/`1`, matching default `std::cout`. |
| **Flat mutable TAC** | SSA with phi nodes | Much faster for a 2-person team to lower loops (`for`/`do-while`) and lower straight to stack/register RISC-V without phi-elimination; optional local optimization passes can still run later. |
