#ifndef GLOBALS_HPP
#define GLOBALS_HPP

#include <cstddef>
#include <filesystem>
#include <string>
#include "operations.hpp"

namespace fs = std::filesystem;

namespace globals {
extern fs::path inputFile;
extern fs::path outputFile;
extern bitfake::type::AudioFormat outputFormat;
extern bool outputToTerminal;
extern bitfake::type::VBRQualities VBRQuality;
extern int opusBitrateKbps;
extern std::string version;
extern fs::path conversionOutputDirectory; // For storing converted files if -po/--pathout is specified
extern std::string tag, val;
extern bool Parallel;
extern std::size_t threads;
extern bool recursive;
extern bool musicbrainzConfirm;
extern bool quietMode;
} // namespace globals
#endif