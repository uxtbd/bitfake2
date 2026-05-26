#include "Utilities/globals.hpp"

namespace globals {
fs::path inputFile;
fs::path outputFile;
bitfake::type::AudioFormat outputFormat = bitfake::type::AudioFormat::MP3;
bitfake::type::VBRQualities VBRQuality;
int opusBitrateKbps = 192; // default 192
bool outputToTerminal = true;
std::string version = "2.0";
fs::path conversionOutputDirectory;
std::string tag, val;
bool Parallel = true;
std::size_t threads = 0;
bool recursive = false;
bool musicbrainzConfirm = true;
bool quietMode = false;
} // namespace globals
