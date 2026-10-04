#include <iostream>

#include "app/version.hpp"

int main() {
    if (app::kName.empty()) {
        std::cerr << "application name must not be empty\n";
        return 1;
    }

    if (app::kVersion.empty()) {
        std::cerr << "application version must not be empty\n";
        return 1;
    }

    std::cout << "smoke_test passed\n";
    return 0;
}
