#include <cmath>
#include <cstdint>
#include <iostream>
#include <unordered_map>

using namespace std;

// fib_naive is the naive fibonacci algorithm (complexity: exponential)
uint64_t fib_naive(uint32_t n) {
    if (n <= 1) {
        return n;
    }
    return fib_naive(n - 1) + fib_naive(n - 2);
}

// fib_memo is a memoized, recursive version of fib_naive (complexity: linear)
uint64_t fib_memo(uint32_t n) {
    static unordered_map<uint32_t, uint64_t> memo{{0, 0}, {1, 1}};
    if (!memo.count(n)) {
        memo[n] = fib_memo(n - 1) + fib_memo(n - 2);
    }
    return memo[n];
}

// fib_iter is the iterative version of fibonacci (complexity: linear + no call stack)
uint64_t fib_iter(uint32_t n) {
    uint32_t prev = 0;
    uint32_t cur = 1;

    for (uint32_t i = 1; i <= n; ++i) {
        auto tmp = prev + cur;
        prev = cur;
        cur = tmp;
    }

    return cur;
}

// fib_const is evaluated at compile time
// Because it's constexpr, the compiler knows it is pure and thus caches repeated calls.
// Thus this is equivalent in performance to fib_memo (albeit at compile time).
constexpr uint64_t fib_const(uint32_t n) {
    if (n <= 1) {
        return 1;
    }

    return fib_const(n - 1) + fib_const(n - 2);
}

// TODO: add closed form for completeness and figure out precision issues.

int main() {
    for (int i = 0; i < 10; ++i) {
        cout << i << ": " << fib_const(i) << endl;
    }
}
