#include <cstdlib>
#include <unordered_map>
#include "Utilities/filechecks.hpp"
namespace fc = FileChecks;
#include "Utilities/operations.hpp"
#include "Utilities/globals.hpp"
namespace gb = globals;
#include "Utilities/consoleout.hpp"
#include <algorithm>
using namespace ConsoleOut;
#include <taglib/fileref.h>
#include <taglib/tpropertymap.h>
namespace tl = TagLib;

namespace bitfake::tagging {
void StageMetaDataChanges(tl::PropertyMap &drawer, std::string key, const std::string &value) {
    std::transform(key.begin(), key.end(), key.begin(),
                   [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    drawer[key] = tl::String(value);
}

bool CommitMetaDataChanges(const std::filesystem::path &path, const tl::PropertyMap &drawer) {
    tl::FileRef f(path.c_str());
    if (f.isNull() || !f.tag()) {
        err("Failed to open file for metadata commit");
        return false;
    }

    tl::PropertyMap existingProps = f.file()->properties();

    for (auto const &[key, val] : drawer) {
        existingProps[key] = val;
    }

    f.file()->setProperties(existingProps);
    return f.file()->save();
}

void MassTagDirectory(const fs::path &dirPath, const std::string &tag, const std::string &value) {
    // fuck

    if (!fs::exists(dirPath) || !fs::is_directory(dirPath)) {
        err("Input path does not exist or is not a directory.");
        return;
    }

    if (tag.empty()) {
        err("Tag name cannot be empty.");
        return;
    }

    // ignore checking if a value is provided js incase user wants empty val
    // haii from the guru! :D

    fs::recursive_directory_iterator dirIter(dirPath);
    std::size_t taggedCount = 0;
    std::size_t failedCount = 0;
    for (const auto &entry : dirIter) {
        if (!entry.is_regular_file() || !fc::IsValidAudioFile(entry.path())) {
            ++failedCount;
            continue;
        }

        TagLib::FileRef f(entry.path().string().c_str());
        if (f.isNull() || !f.tag() || !f.file()) {
            ++failedCount;
            continue;
        }

        TagLib::PropertyMap properties = f.file()->properties();
        bitfake::tagging::StageMetaDataChanges(properties, tag, value);
        if (bitfake::tagging::CommitMetaDataChanges(entry.path(), properties)) {
            ++taggedCount;
        } else {
            ++failedCount;
        }
    }

    if (taggedCount > 0) {
        yay(("Successfully updated tag for " + std::to_string(taggedCount) + " file(s).").c_str());
        if (failedCount > 0) {
            warn(("Failed to update tag for " + std::to_string(failedCount) +
                  " file(s). (Don't Panic! It could just be the album cover!)")
                     .c_str());
        }
        return;
    } else {
        err("Failed to update tag for any files in the directory. Are you sure the directory has music? :(");
        return;
    }
}
}

namespace bitfake::nonuser {

bitfake::type::AudioFormat StringToAudioFormat(const std::string &str) {
    std::string key = str;
    std::transform(key.begin(), key.end(), key.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    // Function is now prepared to handle case-insensitive input and implementing
    // a dictionary for mapping string to enum values.

    static const std::unordered_map<std::string, bitfake::type::AudioFormat> formatMap = {
        {"mp3", bitfake::type::AudioFormat::MP3}, // fuck (static int 0)
        {"ogg", bitfake::type::AudioFormat::OGG},   {"m4a", bitfake::type::AudioFormat::M4A},   {"wav", bitfake::type::AudioFormat::WAV},
        {"flac", bitfake::type::AudioFormat::FLAC}, {"aac", bitfake::type::AudioFormat::AAC},   {"wma", bitfake::type::AudioFormat::WMA},
        {"opus", bitfake::type::AudioFormat::OPUS}, {"aiff", bitfake::type::AudioFormat::AIFF}, {"au", bitfake::type::AudioFormat::AU},
        {"ra", bitfake::type::AudioFormat::RA},     {"ga3", bitfake::type::AudioFormat::GA3},   {"amr", bitfake::type::AudioFormat::AMR},
        {"awb", bitfake::type::AudioFormat::AWB},   {"dss", bitfake::type::AudioFormat::DSS},   {"dvf", bitfake::type::AudioFormat::DVF},
        {"m4b", bitfake::type::AudioFormat::M4B},   {"m4p", bitfake::type::AudioFormat::M4P},   {"mmf", bitfake::type::AudioFormat::MMF},
        {"mpc", bitfake::type::AudioFormat::MPC},   {"msv", bitfake::type::AudioFormat::MSV},   {"nmf", bitfake::type::AudioFormat::NMF},
        {"oga", bitfake::type::AudioFormat::OGA},   {"raw", bitfake::type::AudioFormat::RAW},   {"rf64", bitfake::type::AudioFormat::RF64},
        {"sln", bitfake::type::AudioFormat::SLN},   {"tta", bitfake::type::AudioFormat::TTA},   {"voc", bitfake::type::AudioFormat::VOC},
        {"vox", bitfake::type::AudioFormat::VOX},   {"wv", bitfake::type::AudioFormat::WV},     {"webm", bitfake::type::AudioFormat::WEBM},
        {"svx8", bitfake::type::AudioFormat::SVX8}, {"cda", bitfake::type::AudioFormat::CDA}};

    auto it = formatMap.find(key);
    if (it != formatMap.end()) {
        return it->second;
    }
    err("Invalid audio format string");
    exit(EXIT_FAILURE);
}

bitfake::type::VBRQualities StringToVBRQuality(const std::string &str) {
    std::string key = str;
    std::transform(key.begin(), key.end(), key.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    static const std::unordered_map<std::string, bitfake::type::VBRQualities> vbrMap = {
        {"v0", bitfake::type::VBRQualities::V0}, // MP3 (0 - 8 static int)
        {"v1", bitfake::type::VBRQualities::V1}, {"v2", bitfake::type::VBRQualities::V2}, {"v3", bitfake::type::VBRQualities::V3}, {"v4", bitfake::type::VBRQualities::V4},
        {"v5", bitfake::type::VBRQualities::V5}, {"v6", bitfake::type::VBRQualities::V6}, {"v7", bitfake::type::VBRQualities::V7}, {"v8", bitfake::type::VBRQualities::V8},
        {"v9", bitfake::type::VBRQualities::V9}, {"q0", bitfake::type::VBRQualities::Q0}, // OGG VORBIS (9 - 13 static int)
        {"q3", bitfake::type::VBRQualities::Q3}, {"q6", bitfake::type::VBRQualities::Q6}, {"q9", bitfake::type::VBRQualities::Q9}, {"q10", bitfake::type::VBRQualities::Q10},
        {"l0", bitfake::type::VBRQualities::L0}, // FLAC encoding levels (14 - 22 static int)
        {"l1", bitfake::type::VBRQualities::L1}, {"l2", bitfake::type::VBRQualities::L2}, {"l3", bitfake::type::VBRQualities::L3}, {"l4", bitfake::type::VBRQualities::L4},
        {"l5", bitfake::type::VBRQualities::L5}, {"l6", bitfake::type::VBRQualities::L6}, {"l7", bitfake::type::VBRQualities::L7}, {"l8", bitfake::type::VBRQualities::L8}};

    auto it = vbrMap.find(key);
    if (it != vbrMap.end()) {
        return it->second;
    }
    err("Invalid VBR quality string");
    warn("Are you converting into something thats not mp3/ogg/flac? If so, VBR quality is not applicable yet :(");
    plog("Here's a list of valid VBR quality strings for supported formats:");
    plog("MP3: V0, V1, V2, V3, V4, V5, V6, V7, V8, V9");
    plog("OGG VORBIS: Q0, Q3, Q6, Q9, Q10");
    plog("FLAC encoding levels: L0, L1, L2, L3, L4, L5, L6, L7, L8");
    printf("hint: L8 is the slowest but highest compression level for FLAC, V0 is the highest quality for MP3\n");
    printf("hint: and for OGG VORBIS, Q10 is the highest quality :D\n");
    exit(EXIT_FAILURE);
}

}
