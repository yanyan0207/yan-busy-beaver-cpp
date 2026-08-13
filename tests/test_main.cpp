#include <cassert>
#include <print>

static void test_placeholder() {
    assert(1 + 1 == 2);
    std::println("test_placeholder: ok");
}

int main() {
    test_placeholder();
    std::println("All tests passed.");
}
