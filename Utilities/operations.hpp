#ifndef OPERATIONS_HPP
#define OPERATIONS_HPP

#include <array>
#include <filesystem>
#include <map>
#include <string>
#include <string_view>
#include <vector>
#include <taglib/fileref.h>
#include <taglib/tag.h>
#include <taglib/tpropertymap.h>
namespace fs = std::filesystem;

namespace bitfake {
namespace type {
enum class AudioFormat {
    MP3,
    OGG,
    M4A,
    WAV,
    FLAC,
    ALAC,
    AAC,
    WMA,
    OPUS,
    AIFF,
    AU,
    RA,
    GA3,
    AMR,
    AWB,
    DSS,
    DVF,
    M4B,
    M4P,
    MMF,
    MPC,
    MSV,
    NMF,
    OGA,
    RAW,
    RF64,
    SLN,
    TTA,
    VOC,
    VOX,
    WV,
    WEBM,
    SVX8,
    CDA,
    GENERAL
    // General used for things we don't need a specfic format for
};

enum class VBRQualities : int {
    V0 = 0,
    V1 = 1,
    V2 = 2,
    V3 = 3,
    V4 = 4,
    V5 = 5,
    V6 = 6,
    V7 = 7,
    V8 = 8,
    V9 = 9, // MP3
    Q0 = 0,
    Q3 = 3,
    Q6 = 6,
    Q9 = 9,
    Q10 = 10, // OGG VORBIS
    L0 = 0,
    L1 = 1,
    L2 = 2,
    L3 = 3,
    L4 = 4,
    L5 = 5,
    L6 = 6,
    L7 = 7,
    L8 = 8 // FLAC
};

struct AudioMetadata {
    std::string title;
    std::string artist;
    std::string album;
    std::string genre;
    std::string date;
    int trackNumber = 0;
};

struct AudioMetadataResult {
    fs::path path;
    AudioMetadata metadata;
};

struct ReplayGainInfo {
    std::string title;
    std::string artist;
    std::string album;
    int trackNumber;
    float trackGain;
    float trackPeak;
    float albumGain;
    float albumPeak;
};

struct ReplayGainResult {
    fs::path path;
    ReplayGainInfo info;
};

struct MusicBrainzInfo {
    std::string mbid; // MusicBrainz Identifier
    std::string releaseGroup;
    std::string releaseDate;
};

struct SpectralAnalysisResult {
    std::string title;
    std::string artist;
    std::string album;
    bool likelyLossy;
    float frequencyCutoff;    // Where high-freq content drops sharply
    float noiseFlorElevation; // Elevated noise floor vs expected
    float bandingScore;       // Presence of quantization banding (0-1)
    std::string diagnosis;    // Human readable summary
};

struct ReplayGainByTrack {
    float trackGain;
    float trackPeak;
};

struct ReplayGainByAlbum {
    float albumGain;
    float albumPeak;
};

struct AttachedCoverArt {
    std::vector<unsigned char> imageData;
    std::string mimeType;
    std::string description;
    int pictureType = 3;
};

struct MusicBrainzXMLData {
    std::string recordingTitle;
    std::string artistName;
    std::string releaseTitle;
    std::string releaseDate;
    int trackNumber;
    std::string date;
    std::vector<std::string> genres;
    std::string MUSICBRAINZ_ALBUMID, MUSICBRAINZ_ARTISTID, MUSICBRAINZ_TRACKID;
};

struct MBRequestData {
    // useful data for searching for song
    // use Title - Artist as first check (Kiss The Ladder - Fleshwater) is most likely yielding correct results
    // if title missing, fall back to track id - album - artist
    // if artist missing, fall back to track id - album
    // if album missing, fall back to track id title/artist
    // do not prep MBReq for a file that is missing both title and artist, very risky
    std::string artist;
    std::string title;
    std::string album;
    int trackNumber;
};

inline const std::map<AudioFormat, std::string> ConversionLibMap = {
    {AudioFormat::MP3, "libmp3lame"},    {AudioFormat::OGG, "libvorbis"}, {AudioFormat::FLAC, "flac"},
    {AudioFormat::AAC, "libfdk-aac"},    {AudioFormat::OPUS, "libopus"},  {AudioFormat::WAV, "pcm_s16le"},
    {AudioFormat::GENERAL, "libavcodec"}};
} // namespace type

namespace nonuser {
type::AudioFormat StringToAudioFormat(const std::string &str);
type::VBRQualities StringToVBRQuality(const std::string &str);
bool ConvertToFileType(const fs::path &inputPath, const fs::path &outputPath, type::AudioFormat format,
                       type::VBRQualities quality);
} // namespace nonuser

namespace tagging {
void StageMetaDataChanges(TagLib::PropertyMap &drawer, std::string key, const std::string &value);
bool CommitMetaDataChanges(const std::filesystem::path &path, const TagLib::PropertyMap &drawer);
void MassTagDirectory(const fs::path &dirPath, const std::string &tag, const std::string &value);
} // namespace tagging

namespace extract {
type::AudioMetadata GetMetaData(const fs::path &path);
std::vector<type::AudioMetadataResult> GetMetaDataList(const fs::path &path);
type::ReplayGainInfo GetReplayGain(const fs::path &path);
std::vector<type::ReplayGainResult> GetReplayGainList(const fs::path &path);
} // namespace extract

namespace spectral {
type::SpectralAnalysisResult SpectralAnalysis(const fs::path &path);
std::vector<type::SpectralAnalysisResult> SpectralAnalysisList(const fs::path &path);
void GenerateSpectrogram(const fs::path &inputPath, const fs::path &outputImagePath);
} // namespace spectral

namespace coverart {
bool InputHasAttachedCover(const fs::path &inputPath);
bool GetAttachedCover(const fs::path &inputPath, type::AttachedCoverArt &coverArt);
bool WriteAttachedCover(const fs::path &outputPath, type::AudioFormat outputFormat,
                        const type::AttachedCoverArt &coverArt);
bool CopyAttachedCover(const fs::path &inputPath, const fs::path &outputPath, type::AudioFormat outputFormat);
bool FormatSupportsAttachedCover(type::AudioFormat format);
std::string OutputExtensionForFormat(type::AudioFormat format);
} // namespace coverart

namespace replaygain {
type::ReplayGainByTrack CalculateReplayGainTrack(const fs::path &path);
void ApplyReplayGain(const fs::path &path, type::ReplayGainByTrack trackGainInfo,
                     type::ReplayGainByAlbum albumGainInfo);
void CalculateReplayGainAlbum(const fs::path &path);
} // namespace replaygain

namespace sort {
void OrganizeIntoAlbums(const fs::path &inputDir, const fs::path &outputDir);
void OrganizeIntoArtistAlbum(const fs::path &inputDir, const fs::path &outputDir);
void RenameAlbumDirectoriesFromTags(const fs::path &rootDir);
void OrganizeAlbumsIntoArtists(const fs::path &rootDir);
void RenameFilesFromTags(const fs::path &rootDir);
} // namespace sort

namespace musicbrainz {
type::MBRequestData PrepareMBRequestData(const fs::path &inputPath);
std::string GetMBXML(const type::MBRequestData &reqData);
type::MusicBrainzXMLData ParseMBXML(const std::string &xmlStr);
void WriteMetaFromMBXML(const fs::path &inputPath, const type::MusicBrainzXMLData &mbData);
} // namespace musicbrainz
} // namespace bitfake
#endif // OPERATIONS_HPP