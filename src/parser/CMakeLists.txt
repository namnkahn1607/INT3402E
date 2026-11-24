add_module(parser
    SOURCES expr_parser.cpp parser_core.cpp parser_decl.cpp parser_stmt.cpp
    DEPS_PUBLIC INT3402E::common INT3402E::ast)
