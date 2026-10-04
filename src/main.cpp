#include <iostream>

#include "app/version.hpp"

int main() {
    std::cout << app::kName << "\n";
    std::cout << "version: " << app::kVersion << "\n";
    return 0;
}
