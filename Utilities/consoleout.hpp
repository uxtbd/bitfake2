#ifndef CONSOLEOUT_HPP
#define CONSOLEOUT_HPP

#include <cstdlib>
#include <stdio.h>
#include <mutex>

// Forward declare global quiet mode flag
namespace globals {extern bool quietMode;}

namespace ConsoleOut {
static constexpr const char *RED = "\033[31m";
static constexpr const char *GREEN = "\033[32m";
static constexpr const char *YELLOW = "\033[33m";
static constexpr const char *BLUE = "\033[34m";
static constexpr const char *MAGENTA = "\033[35m";
static constexpr const char *CYAN = "\033[36m";
static constexpr const char *RESET = "\033[0m";

inline std::mutex ConsoleMutex;

inline void err(const char *msg = "") {
    std::lock_guard<std::mutex> lock(ConsoleMutex);
    if (msg[0] != '\0') {
        printf("%s[ERR]\t%s%s\n", RED, RESET, msg);
        fflush(stdout); // Ensure the error message is printed immediately on Windows
    } else {
        printf("%s[ERR]%sAn unknown error has occurred\n", RED, RESET);
        fflush(stdout);
    }
}
inline void warn(const char *msg) {
    if (::globals::quietMode) return;  // Suppress warnings in quiet mode
    std::lock_guard<std::mutex> lock(ConsoleMutex);
    if (msg[0] != '\0') {
        printf("%s[WRN]\t%s%s\n", YELLOW, RESET, msg);
        fflush(stdout);
    }
}
inline void plog(const char *msg) // Pretty log :D
{
    std::lock_guard<std::mutex> lock(ConsoleMutex);
    if (msg[0] != '\0') {
        printf("%s[LOG]\t%s%s\n", BLUE, RESET, msg);
        fflush(stdout);
    }
}
inline void yay(const char *msg) {
    std::lock_guard<std::mutex> lock(ConsoleMutex);
    if (msg[0] != '\0') {
        printf("%s[YAY]\t%s%s\n", GREEN, RESET, msg);
        fflush(stdout);
    }
}
} // namespace ConsoleOut

#endif // CONSOLEOUT_HPP