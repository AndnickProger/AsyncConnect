#pragma once

#include <chrono>
#include <optional>
#include <sys/stat.h>
#include <unistd.h>
#include <pwd.h>
#include <chrono>
#include <fcntl.h>
#include <filesystem>
#include <array>
#include <algorithm>
#include <string>
#include <sstream>
#include <iostream>

[[nodiscard]] std::optional<std::string> getUserName() noexcept;

[[nodiscard]] std::string getDateAsText();

enum class LogLevel
    {
        DEBUG,
        INFO,
        WARNING,
        ERROR,
        CRITICAL
    };

[[nodiscard]] std::string logLevelToString(const LogLevel& logLevel);

class ConsoleLogger
{
    public:

    ConsoleLogger(ConsoleLogger&& other) noexcept = default;
    ConsoleLogger& operator = (ConsoleLogger&& other) noexcept = default;

    static ConsoleLogger& getLogger();

    static void loggingMessage(const LogLevel& logLevel, const std::string& message);

    private:

    ConsoleLogger(){}
    ConsoleLogger(const ConsoleLogger& other) = default;
    ConsoleLogger& operator = (const ConsoleLogger& other) = default;
};