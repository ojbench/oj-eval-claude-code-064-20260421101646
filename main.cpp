#include "printf.hpp"
#include <vector>
#include <string>

int main() {
    sjtu::printf("Hello, %s!\n", "world");
    sjtu::printf("Integer: %d, Unsigned: %u\n", -123, 456u);
    sjtu::printf("Escape: %%\n");
    sjtu::printf("Any: %_, %_, %_, %_\n", 1, -1, std::string("test"), std::vector<int>{1, 2, 3});

    // Test multiple escapes
    sjtu::printf("Escapes: %%%% %% %%%s\n", "test");

    // Test nested vectors (if supported by requirement "adopt default format method for each member")
    // The requirement says "adopt default format method for each member",
    // so std::vector<std::vector<int>> should work if formatter<std::vector<T>> uses formatter<T>::format_to.
    sjtu::printf("Nested: %_\n", std::vector<std::vector<int>>{{1, 2}, {3, 4}});

    return 0;
}
