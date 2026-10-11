
## C++ Subset Context-Free Grammar (CFG)

This grammar defines the supported syntax of the restricted subset of C++. It serves as the formal specification for the compiler's syntax analysis phase.

### Supported Features

- **Declarations:** `int` and `bool` variables with optional initialization.
- **Assignments:** Assigning expressions to identifiers.
- **Control flow:** `if/else`, `do-while`, and `for` loops with mandatory block bodies.
- **Output:** `print(expression);`.
- **Expressions:** Integer and boolean literals, identifiers, arithmetic (`+`, `*`), comparisons (`>`, `>=`), equality (`==`), and parentheses.
- **Operator precedence:** `*` > `+` > `>` / `>=` > `==`.

### Grammar

```bnf
<translation-unit> -> <statement-list> EOF

<block> -> "{" <statement-list> "}"

<statement-list> -> <statement> <statement-list> | epsilon

<statement> -> <declaration> ";"
             | <assignment> ";"
             | <print-statement> ";"
             | <if-statement>
             | <do-statement>
             | <for-statement>
             | <block>

<declaration> -> <type> ID <initializer>

<type> -> "int" | "bool"

<initializer> -> "=" <expression> | epsilon

<assignment> -> ID "=" <expression>

<print-statement> -> "print" "(" <expression> ")"

<if-statement> -> "if" "(" <expression> ")" <block> <else-part>

<else-part> -> "else" <block> | epsilon

<do-statement> -> "do" <block> "while" "(" <expression> ")" ";"

<for-statement> -> "for" "(" <for-initializer> ";" <expression> ";"
                   <assignment> ")" <block>

<for-initializer> -> <declaration> | <assignment>

<expression> -> <equality>

<equality> -> <comparison> <equality-part>

<equality-part> -> "==" <comparison> | epsilon

<comparison> -> <addition> <comparison-part>

<comparison-part> -> ">" <addition>
                   | ">=" <addition>
                   | epsilon

<addition> -> <term> <addition-tail>

<addition-tail> -> "+" <term> <addition-tail> | epsilon

<term> -> <factor> <term-tail>

<term-tail> -> "*" <factor> <term-tail> | epsilon

<factor> -> ID
          | INTEGER
          | "true"
          | "false"
          | "(" <expression> ")"
```

### Grammar Restrictions

- Assignment is a separate grammatical construct, not a general expression.
- `for` loops require an initializer, condition, and update assignment.
- Comparison and equality operators cannot be chained at the same precedence level without parentheses.
- Control-flow bodies must use `{}`.
