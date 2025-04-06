#ifndef INCLUDE_RECAP_LABELLER_CONFIG_HPP
#define INCLUDE_RECAP_LABELLER_CONFIG_HPP

#include <spdlog/spdlog.h>

#include <filesystem>

namespace recap::labeller::app {

#define RECAP_LABELLER_LABELLING_CONFIG_FIELDS(_)                                                  \
    _(double, max_snrf_time, 0.246)                                                                \
    _(bool, default_shuffle_tasks, true)

struct LabellingConfig {
#define APP_CONFIG_FIELD(type, name, default_val) type name = default_val;
    RECAP_LABELLER_LABELLING_CONFIG_FIELDS(APP_CONFIG_FIELD)
#undef APP_CONFIG_FIELD
};

inline void log_labelling_config(const LabellingConfig& c, spdlog::level::level_enum level)
{
#define APP_CONFIG_LOG(type, name, default_val) spdlog::log(level, "  " #name " = {}", c.name);
    RECAP_LABELLER_LABELLING_CONFIG_FIELDS(APP_CONFIG_LOG);
#undef APP_CONFIG_LOG
}

// Throws std::invalid_argument on error
LabellingConfig load_labelling_config(const std::filesystem::path& path);

}; // namespace recap::labeller::app

#endif // INCLUDE_RECAP_LABELLER_CONFIG_HPP
