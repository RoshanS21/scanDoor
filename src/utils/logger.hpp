#pragma once
#include <spdlog/spdlog.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <memory>

class Logger
{
public:
    static void initialize(const std::string& doorId)
    {
        auto doorLogger = spdlog::rotating_logger_mt(
            "door_" + doorId,
            "logs/door_" + doorId + ".log",
            1024 * 1024 * 5,  // 5MB max file size
            3                 // Keep 3 old files
        );
        doorLogger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%n] [%l] %v");
    }

    static std::shared_ptr<spdlog::logger> getDoorLogger(const std::string& doorId)
    {
        return spdlog::get("door_" + doorId);
    }
};
