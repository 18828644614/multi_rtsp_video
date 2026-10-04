#pragma once

#if __has_include(<filesystem>)
#include <filesystem>
namespace app_fs = std::filesystem;
#else
#include <experimental/filesystem>
namespace app_fs = std::experimental::filesystem;
#endif
