// This file features the C++ language subset this compiler project targets.
//
// Watch token stream:
//   clang++ -fsyntax-only -Xclang -dump-tokens lang_feature_demo.cpp > token_stream.txt 2>&1

// NOLINTBEGIN

// Function
void Nothing() {}

double Eval(bool disable = true) {
    // Primitive types & literals
    short  s = 1;
    int    i = -10;
    long   l = +100;
    float  f = 1.25f;
    double d = 0.0;
    bool   b = false;

    // Control flow statement: if-else
    if (!disable) {
        return s + i - l * f / d;  // Mathematical operators
    } else {
        return d;
    }
}

int main() {
    Nothing();  // Function invocation

    int branch_selector = 2;
    int mode_flag       = 0;
    // Control flow statement: switch-case
    switch (branch_selector) {
        case 1: mode_flag = 10; break;
        case 2: mode_flag = 20; break;
        default: mode_flag = -1;
    }

    int       global_like_counter = 0;
    const int max_loops           = 10;  // Constant identifier
    // Control flow statement: for loop
    for (int idx = 0; idx < max_loops; idx++) {
        if (idx == 1) {
            continue;
        }
        if (idx >= 4) {
            break;
        }
        ++global_like_counter;
    }

    int countdown = 4;
    // Control flow statement: while loop
    while (countdown > 0) {
        --countdown;
        countdown--;
    }

    if (mode_flag <= 20 || global_like_counter > 0 && countdown == 0) {
        return 0;
    }

    return 1;
}

// NOLINTEND
