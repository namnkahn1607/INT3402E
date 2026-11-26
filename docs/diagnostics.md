# Source-Snippet Underlining Specification

## Purpose and Scope

This document specifies Clang-style source-snippet underlining for `common::DiagnosisEngine`. It is designed to stay localized inside `include/common/diagnostic.h`, `src/common/diagnostic.cpp`, and `src/main.cpp` so the rest of the pipeline (`lexer`, `parser`, `sema`, `ir`, `codegen`) stays decoupled.

Supported scope:
- **Tier 1 (Required):** Single-caret source line rendering (`^`) at a 1-based `Location { line, col }`. Works out of the box for all existing diagnostics with zero upstream edits.
- **Tier 2 (Opt-in):** Single-token underlining (`^~~~`) using an optional `int length = 1` field on `Diagnostic` and `Note`. Callers that already know a token or identifier length can set `.length`; all other callers default to `1`.

Explicitly **out of scope (Tier 3)**:
- Full multi-token AST `SourceSpan { start, end }` tracking, multiline range underlining, and overlapping subexpression carets (`~~~~ ^ ~~~~`). Adding spans would require updating `lexer::Token`, every AST node (`Expr`, `Stmt`, `Decl`), and `ExprParser::ParsePrimary` (which discards `(` and `)` locations).
- ANSI color escape codes. If added later, color must be disabled when `!isatty` and disabled in tests.

---

## Data Model and API Extensions

### `include/common/diagnostic.h`

Add `#include <string_view>` and extend `Note`, `Diagnostic`, and `DiagnosisEngine`:

```cpp
struct Note {
    Location    loc;
    std::string message;
    int         length = 1;
};

struct Diagnostic {
    Location          loc;
    Severity          severity;
    std::string       message;
    std::vector<Note> notes  = {};
    int               length = 1;
};
```

Add source-aware `Print` overloads on `DiagnosisEngine` alongside the existing header-only overloads:

```cpp
void Print(std::ostream& out, std::string_view source) const;
void Print(std::string_view source) const;  // delegates to Print(std::cerr, source)
```

Backward compatibility:
- `length = 1` keeps every existing aggregate/designated-initializer call site valid with zero `-Wmissing-field-initializers` warnings.
- `Print(std::ostream&)` and `Print()` remain unchanged for unit tests and contexts where source text is unavailable (e.g., `common::LoadSource` failures).

### Driver Integration (`src/main.cpp`)

- If `common::LoadSource` fails (`!loaded`): keep calling `diag.Print(); diag.Summarize();` (no source buffer exists).
- Once `source` is loaded or read from `std::cin`: call `diag.Print(source); diag.Summarize();`.

---

## Output Format

Each `Diagnostic` (and each attached `Note`) renders as:
1. Header line: `<line>:<col>: <severity>: <message>\n` (`<severity>` is `warning`, `error`, or `note`).
2. Optional snippet block (only when the location maps to a valid source line; see [Edge Cases](#edge-cases-and-invariants)):
   - Line gutter: `<line_num>` right-aligned to `gutter_width` spaces, followed by `" | "`, followed by the source line (without `\r` or `\n`).
   - Caret gutter: `gutter_width` spaces, followed by `" | "`, followed by column padding, `'^'`, and `N` `'~'` characters (where `N = clamped_length - 1`).

Compute `gutter_width` as the decimal digit width of the maximum printable line number across the `Diagnostic` and all of its printable `Note`s (minimum `1`). Keeping a shared `gutter_width` per diagnostic group aligns the `|` bars between an error and its notes.

### Examples

**Tier 1 (`length == 1`, single caret):**

```text
3:11: error: invalid operands to '+' ('int' and 'bool')
3 | int x = a + flag;
  |           ^
```

**Tier 2 (`length == 4`, token underline):**

```text
3:13: error: use of undeclared identifier 'flag'
3 | int x = a + flag;
  |             ^~~~
```

**Multi-site error with attached `Note` and aligned gutters:**

```text
12:10: error: redeclaration of 'x'
12 |     bool x = true;
   |          ^
 3:5: note: previous declaration is here
 3 |     int x = 1;
   |         ^
```

---

## Line Indexing and Rendering Algorithm

Inside `DiagnosisEngine::Print(std::ostream& out, std::string_view source) const`:

1. **Lazy line table:** If `collections_.empty()`, return immediately without scanning `source`. Otherwise, build `std::vector<std::size_t> line_starts` in one pass:
   - Start with `line_starts = {0}`.
   - For each index `i` in `0 .. source.size() - 1`: if `source[i] == '\n'`, push `i + 1`.
2. **Line extraction helper** `ExtractLine(source, line_starts, line_num)` (1-based `line_num`):
   - `start = line_starts[line_num - 1]`.
   - `end = (line_num < line_starts.size()) ? line_starts[line_num] - 1 : source.size()`.
   - Slice `line = source.substr(start, end - start)`.
   - Strip a single trailing `'\r'` if `!line.empty() && line.back() == '\r'`.
3. **Shared entry renderer** `RenderEntry(out, loc, length, tag, message, source, line_starts, gutter_width)`:
   - Emit `<loc.line>:<loc.col>: <tag>: <message>\n`.
   - Check whether `loc` is renderable against `source` and `line_starts`; if not, return without printing a snippet.
   - Extract `line`, compute padding/caret/tildes per the clamping rules below, and write the two snippet lines.
4. For each `d` in `collections_`, compute `gutter_width` across `d` and its printable `d.notes`, render `d`, then render each note in `d.notes`.

---

## Edge Cases and Invariants

### 1. Sentinel (`{0, 0}`) and Virtual-EOF Locations (`last_line + 1, col 1`)

- **Why it happens:**
  - `common::LoadSource` (`src/common/source.cpp`) emits sentinel `{.line = 0, .col = 0}` for file I/O errors.
  - The parser's virtual EOF location (`openspec/specs/syntax-analysis/spec.md`) points past the last character of the file: on trailing `\n` (e.g., `"{\n// tail\n"`), EOF is `{.line = 3, .col = 1}`, and on empty input `""`, EOF is `{.line = 1, .col = 1}`.
  - Note that `line_starts` pushed `source.size()` when `source.back() == '\n'`, creating a trailing empty entry at `line_starts.back()`.
- **Invariant:** Omit the snippet block (emit only the header line) whenever any of the following holds:
  - `source.empty()`
  - `loc.line < 1 || loc.col < 1`
  - `static_cast<std::size_t>(loc.line) > line_starts.size()`
  - `static_cast<std::size_t>(loc.line) == line_starts.size() && line.empty() && source.back() == '\n'` (synthetic empty line after trailing newline)
- **Effect:** Never performs out-of-bounds indexing and never renders a blank line snippet for trailing-newline EOF errors.

### 2. Tab Alignment (`\t`)

- **Why it happens:** The Flex lexer counts `\t` as 1 column (see `tests/lexer/lexer_test.cpp`, where `"\ta1b 1e3"` places `a1b` at `col = 2`), whereas terminals render `\t` at tab stops (4 or 8 spaces). Printing `col - 1` spaces under a line containing `\t` causes `^` to drift left of the token.
- **Invariant:** Build the caret prefix by iterating `i` from `0` to `start_idx - 1` over the extracted `line`: emit `'\t'` if `line[i] == '\t'`, otherwise `' '`.
- **Trade-off:** Mirroring `'\t'` requires zero modification to the displayed source line and adapts to whatever tab stop width the user's terminal uses. (If you prefer deterministic fixed-width logs across terminals, expand `'\t'` to 4 spaces consistently in both the source line and the caret prefix instead.)

### 3. CRLF (`\r\n`) and Column/Length Clamping

- **Why it happens:**
  - Windows-formatted inputs contain `\r\n`. Printing raw `\r` moves the terminal cursor back to column 0 and corrupts the snippet line.
  - Missing-delimiter syntax errors (or end-of-line EOF locations without a trailing newline) can set `loc.col == line.size() + 1`, pointing one column past the last character.
  - Callers could also pass `length <= 0` or a `length` exceeding the remaining characters on the line.
- **Invariants:**
  - Strip trailing `'\r'` from `line` before rendering and before measuring `line.size()`.
  - Clamp column offset:
    ```cpp
    const std::size_t start_idx =
        std::min<std::size_t>(static_cast<std::size_t>(loc.col - 1), line.size());
    ```
    When `start_idx == line.size()`, `^` renders cleanly right after the last character of the line.
  - Normalize and clamp underline length:
    ```cpp
    const int         safe_len = std::max(length, 1);
    const std::size_t tilde_count =
        (start_idx < line.size())
            ? std::min<std::size_t>(static_cast<std::size_t>(safe_len - 1),
                                    line.size() - start_idx - 1)
            : 0;
    ```
  - Guarantees `^` is always emitted once and `~` never extends past the end of `line`.

---

## Tier 2 Call-Site Opt-In Guide

Setting `.length` is completely optional and can be added incrementally:

| Pass / Site | Available `.length` source | Fallback when omitted |
| :--- | :--- | :--- |
| Lexer (`src/lexer/lexer.cpp`) | `static_cast<int>(tok.lexeme.size())` for invalid tokens | `1` |
| Parser (`include/parser/cursor.h`) | Optional `PeekToken()->lexeme.size()` when reporting at current token | `1` |
| Sema — `DeclRefExpr` / `VarDecl` | `ref.name.size()` or `decl.name.size()` | `1` |
| Sema — `BinaryExpr` | `1` (`+`, `*`, `>`) or `2` (`>=`, `==`) | `1` |

---

## Unit Test Checklist (`tests/common/diagnostic_test.cpp`)

When implementing `Print(std::ostream&, std::string_view)`, add tests covering:
1. Tier 1 (`length == 1`) single-caret placement on an ordinary line.
2. Tier 2 (`length > 1`) token underline (`^~~~`) and clamping when `length` exceeds line end.
3. Sentinel `{0, 0}` and virtual-EOF (`last_line + 1, col 1`, empty source `{1, 1}`, and `col == line.size() + 1` without trailing `\n`).
4. Leading `\t` indentation alignment (`col` counts tabs as 1, caret prefix copies `\t`).
5. CRLF (`\r\n`) source lines stripped cleanly without stray `\r`.
6. Multi-site `Diagnostic` + `Note` with differing line digit counts (e.g., line `12` error and line `3` note) verifying gutter alignment.
