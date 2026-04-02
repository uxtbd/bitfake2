#include "Utilities/globals.hpp"

namespace globals {
fs::path inputFile;
fs::path outputFile;
bitfake::type::AudioFormat outputFormat = bitfake::type::AudioFormat::MP3;
bitfake::type::VBRQualities VBRQuality;
int opusBitrateKbps = 192; // default 192
bool outputToTerminal = true;
std::string version = "0.1.9";
fs::path conversionOutputDirectory;
std::string tag, val;
} // namespace globals
