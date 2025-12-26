#include "logger.h"
#include <spdlog/sinks/stdout_color_sinks.h>

void init_logger() {
    auto logger = spdlog::stdout_color_mt("core");
    spdlog::set_default_logger(logger);
    spdlog::set_pattern("[%Y-%m-%d %H:%M:%S] [%^%l%$] %v");
    spdlog::set_level(spdlog::level::info);
}
