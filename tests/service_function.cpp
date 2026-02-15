#include "service_function.hpp"

std::optional<std::string> getUserName() noexcept
{
    const auto UID = getuid();
    struct passwd *pw = getpwuid(UID);

    if (pw)
    {
        return std::optional<std::string>(std::string(pw->pw_name));
    }
    else
    {
        return std::optional<std::string>(std::nullopt);
    }
}

std::string getDateAsText()
{
     time_t now = time(0);
    struct tm tstruct;
    std::array<char, 100> buffer;

    auto begin = buffer.data();
    const auto bufferSize = buffer.size();
    auto end = begin + bufferSize;

    std::fill(begin, end, '!');

    tstruct = *localtime(&now);
    strftime(begin, bufferSize, "%a, %d %B %Y %X", &tstruct);

    auto charCount = std::count_if(begin, end, [](const char &element)
                                   { return element != '!'; });
    buffer[charCount] = ' ';
    ++charCount;
    buffer[charCount] = 'U';
    ++charCount;
    buffer[charCount] = 'T';
    ++charCount;
    buffer[charCount] = 'C';
    ++charCount;

    return std::string(buffer.data(), charCount);  
}

std::string logLevelToString(const LogLevel &logLevel)
{
    switch (logLevel)
    {
    case LogLevel::DEBUG:
        return std::string("DEBUG");

    case LogLevel::INFO:
        return std::string("INFO");

    case LogLevel::ERROR:
        return std::string("ERROR");

    case LogLevel::WARNING:
        return std::string("WARNING");

    case LogLevel::CRITICAL:
        return std::string("CRITICAL");

    default:
        return std::string("UNKNOWN");
    }
}

ConsoleLogger &ConsoleLogger::getLogger()
{
    static ConsoleLogger logger;
    return logger;
}

void ConsoleLogger::loggingMessage(const LogLevel &logLevel, const std::string &message)
{
    const auto userName = getUserName();
    std::stringstream stream;

    if(userName.has_value()) stream << userName.value() << " ";
    stream << getDateAsText() << " " << logLevelToString(logLevel) << " MESSAGE: " << message << "\n";
    std::cout << stream.str();
}
