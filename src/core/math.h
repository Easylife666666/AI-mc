#pragma once

namespace aimc {

// Correct floor division / modulo for negative operands (chunk lookups rely on it).
inline int floorDiv(int a, int b) {
    const int q = a / b;
    const int r = a % b;
    if (r != 0 && ((r < 0) != (b < 0))) {
        return q - 1;
    }
    return q;
}

inline int floorMod(int a, int b) {
    const int r = a % b;
    if (r != 0 && ((r < 0) != (b < 0))) {
        return r + b;
    }
    return r;
}

}  // namespace aimc
