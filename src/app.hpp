#pragma once

#include <filesystem>

namespace app {

int setup(std::filesystem::path labelling_tasks_path, std::filesystem::path data_dir);
bool draw();
void teardown();
void close();

}; // namespace app
