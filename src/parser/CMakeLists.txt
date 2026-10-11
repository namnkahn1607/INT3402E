add_module(parser
    SOURCES parser_core.cpp parser_decl.cpp parser_stmt.cpp parser_expr.cpp
    DEPS_PUBLIC INT3402E::common INT3402E::ast)
