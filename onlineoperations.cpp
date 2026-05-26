#include "Utilities/consoleout.hpp"
using namespace ConsoleOut;
#include <filesystem>
namespace fs = std::filesystem;
#include "Utilities/filechecks.hpp"
namespace fc = FileChecks;
#include "Utilities/operations.hpp"
#include "Utilities/globals.hpp"
namespace gb = globals;
#include <curl/curl.h>
#include <taglib/fileref.h>
#include <taglib/tpropertymap.h>
#include <sstream>
#include <iomanip>
#include <cctype>
#include <algorithm>
#include <set>
#include <cstring>
#include <vector>
#include <limits>
#include <climits>
#include <string>
#include <fstream>
#include <chrono>
#include <thread>

#include "Utilities/pugixml.hpp"

namespace bitfake::online {

// Helper functions to help extract metadata from a JSON/XML
// which is expected from libcurl when we request metadata from musicbrainz
// i do not plan on adding any other database because MB is public, and FOSS.
// i fucking LIED!! discogs sounds sm better accouring to a few soulseek souls. we must add it soon.

// example XML:
/*

<?xml version="1.0" encoding="UTF-8"?>
<metadata xmlns="http://musicbrainz.org/ns/mmd-2.0#">
<recording id="4e99fc0d-b153-4dc9-994f-ded05a3069c9">
<title>Yellow</title>
<length>266000</length>
<video>false</video>
<artist-credit>
  <name-credit>
    <artist id="cc197bad-dc9c-440d-a5b5-d52ba2e14234">
      <name>Coldplay</name>
      <sort-name>Coldplay</sort-name>
      <iso-3166-1-code-list>
         <iso-3166-1-code>GB</iso-3166-1-code>
      </iso-3166-1-code-list>
    </artist>
  </name-credit>
</artist-credit>
<release-list>
  <release id="a34a8e31-897b-4b15-99d7-83de6820ffeb">
    <title>Parachutes</title>
    <status id="4e304316-386d-3409-af2e-78857eec5cfe">Official</status>
    <quality>normal</quality>
    <text-representation>
      <language>eng</language>
      <script>Latn</script>
    </text-representation>
    <date>2000-07-10</date>
    <country>GB</country>
    <release-event-list>
      <release-event>
        <date>2000-07-10</date>
        <area id="8a754a16-0027-3a29-b6d7-2b40ea0481ed">
          <name>United Kingdom</name>
          <sort-name>United Kingdom</sort-name>
          <iso-3166-1-code-list>
            <iso-3166-1-code>GB</iso-3166-1-code>
          </iso-3166-1-code-list>
        </area>
      </release-event>
    </release-event-list>
  </release>
</release-list>
<isrc-list>
  <isrc id="GBP7M0000030"/>
  <isrc id="USN2Z1100072"/>
</isrc-list>
<tag-list>
  <tag count="2">
    <name>alternative rock</name>
  </tag>
  <tag count="1">
    <name>britpop</name>
  </tag>
</tag-list>
</recording>
</metadata>

END OF EXAMPLE XML
*/

bitfake::type::MBRequestData PrepareMBRequestData(const fs::path &inputPath) {
    bitfake::type::MBRequestData requestData;

    auto trim = [](std::string value) -> std::string {
        const std::string whitespace = " \t\n\r";
        const size_t start = value.find_first_not_of(whitespace);
        if (start == std::string::npos) {
            return "";
        }
        const size_t end = value.find_last_not_of(whitespace);
        return value.substr(start, end - start + 1);
    };

    auto hasDatePrefix = [](const std::string &value) -> bool {
        if (value.size() < 12) {
            return false;
        }

        auto isDigitAt = [&value](size_t idx) -> bool {
            return idx < value.size() && std::isdigit(static_cast<unsigned char>(value[idx])) != 0;
        };

        return isDigitAt(0) && isDigitAt(1) && isDigitAt(2) && isDigitAt(3) && value[4] == '-' && isDigitAt(5) &&
               isDigitAt(6) && value[7] == '-' && isDigitAt(8) && isDigitAt(9) && value[10] == ':' && value[11] == ' ';
    };

    auto inferAlbumFromPath = [&inputPath, &trim]() -> std::string {
        std::string folder = trim(inputPath.parent_path().filename().string());
        if (folder.empty()) {
            return "";
        }

        // Common music folder naming: "Album Name (YYYY)"
        if (folder.size() > 7 && folder.back() == ')') {
            size_t openParen = folder.rfind(" (");
            if (openParen != std::string::npos && openParen + 6 == folder.size() - 1) {
                const bool yearLike = std::isdigit(static_cast<unsigned char>(folder[openParen + 2])) != 0 &&
                                      std::isdigit(static_cast<unsigned char>(folder[openParen + 3])) != 0 &&
                                      std::isdigit(static_cast<unsigned char>(folder[openParen + 4])) != 0 &&
                                      std::isdigit(static_cast<unsigned char>(folder[openParen + 5])) != 0;
                if (yearLike) {
                    folder = trim(folder.substr(0, openParen));
                }
            }
        }

        return folder;
    };

    auto inferTitleFromFilename = [&inputPath, &trim]() -> std::string {
        const std::string stem = inputPath.stem().string();
        const size_t dashPos = stem.find(" - ");
        if (dashPos == std::string::npos || dashPos + 3 >= stem.size()) {
            return "";
        }

        const std::string prefix = trim(stem.substr(0, dashPos));
        if (prefix.empty()) {
            return "";
        }

        for (char c : prefix) {
            if (!std::isdigit(static_cast<unsigned char>(c)) && !std::isspace(static_cast<unsigned char>(c))) {
                return "";
            }
        }

        return trim(stem.substr(dashPos + 3));
    };

    // TagLib::FileRef f(inputPath.string().c_str());
    // if (f.isNull() || f.tag() == nullptr) {
    //     warn("MusicBrainz metadata request: unable to read tags from file.");
    //     return requestData;
    // }

    // TagLib::Tag *tag = f.tag();
    // requestData.artist = tag->artist().to8Bit(true);
    // requestData.title = tag->title().to8Bit(true);
    // requestData.album = tag->album().to8Bit(true);
    // requestData.trackNumber = tag->track();

    bitfake::type::AudioMetadata tmpMD = bitfake::extract::GetMetaData(inputPath);
    requestData.artist = tmpMD.artist;
    requestData.title = tmpMD.title;
    requestData.album = tmpMD.album;
    requestData.trackNumber = tmpMD.trackNumber;

    const std::string fileTitleHint = inferTitleFromFilename();
    if (!fileTitleHint.empty()) {
        requestData.title = fileTitleHint;
    }

    const std::string pathAlbumHint = inferAlbumFromPath();
    if ((!pathAlbumHint.empty() && requestData.album.empty()) || hasDatePrefix(requestData.album)) {
        requestData.album = pathAlbumHint;
    }

    if (!fileTitleHint.empty() && !pathAlbumHint.empty()) {
        requestData.album = pathAlbumHint;
    }

    if (requestData.title.empty()) {
        requestData.title = inputPath.stem().string();
    }

    return requestData;
}

// curl stuff

static size_t WriteCallback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t totalSize = size * nmemb;
    std::string *str = static_cast<std::string *>(userp);
    str->append(static_cast<char *>(contents), totalSize);
    return totalSize;
}

std::string GetMBXML(const bitfake::type::MBRequestData &reqData) {
    // actually lib curl stuff!
    std::string XMLresponseStr; // returning this

    CURL *curl = curl_easy_init();
    if (!curl) {
        err("Failed to initialize libcurl for MusicBrainz metadata request.");
        return "";
    }

    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);

    std::string userAgent = "bitfake2/" + gb::version + "(ray@atl.tools)";
    curl_easy_setopt(curl, CURLOPT_USERAGENT, userAgent.c_str());

    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L); // 30 second timeout for slow internets

    // Escape special query syntax characters so tag text is treated as literal terms.
    auto escapeSearchValue = [](const std::string &value) -> std::string {
        std::string escaped;
        escaped.reserve(value.size() * 2);
        for (char c : value) {
            switch (c) {
                case '\\':
                case '+':
                case '-':
                case '!':
                case '(':
                case ')':
                case '{':
                case '}':
                case '[':
                case ']':
                case '^':
                case '"':
                case '~':
                case '*':
                case '?':
                case ':':
                case '/':
                case '|':
                case '&':
                    escaped.push_back('\\');
                    break;
                default:
                    break;
            }
            escaped.push_back(c);
        }
        return escaped;
    };

    auto executeSearchExpr = [&](const std::string &searchExpr, std::string &responseXml) -> bool {
        char *encodedExpr = curl_easy_escape(curl, searchExpr.c_str(), static_cast<int>(searchExpr.size()));
        if (encodedExpr == nullptr) {
            err("MusicBrainz metadata request failed: unable to URL-encode query.");
            return false;
        }

        const std::string MBurl = "https://musicbrainz.org/ws/2/recording?query=" + std::string(encodedExpr) +
                                  "&fmt=xml&limit=10&inc=artist-credits+releases+media+recordings+tags+genres";
        curl_free(encodedExpr);

        auto isRetryableCurlError = [](CURLcode code) -> bool {
            return code == CURLE_OPERATION_TIMEDOUT || code == CURLE_COULDNT_RESOLVE_HOST ||
                   code == CURLE_COULDNT_CONNECT || code == CURLE_RECV_ERROR || code == CURLE_SEND_ERROR;
        };

        const int maxAttempts = 3;
        for (int attempt = 1; attempt <= maxAttempts; ++attempt) {
            responseXml.clear();
            curl_easy_setopt(curl, CURLOPT_URL, MBurl.c_str());
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &responseXml);

            const CURLcode res = curl_easy_perform(curl);
            if (res != CURLE_OK) {
                if (attempt < maxAttempts && isRetryableCurlError(res)) {
                    warn(("MusicBrainz request transient network error; retrying (attempt " + std::to_string(attempt + 1) +
                          "/" + std::to_string(maxAttempts) + ").")
                             .c_str());
                    std::this_thread::sleep_for(std::chrono::milliseconds(500L * (1L << (attempt - 1))));
                    continue;
                }
                err(("MusicBrainz metadata request failed: " + std::string(curl_easy_strerror(res))).c_str());
                return false;
            }

            long httpCode = 0;
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
            const bool retryableHttp = (httpCode == 429) || (httpCode >= 500 && httpCode <= 599);
            if (retryableHttp && attempt < maxAttempts) {
                warn(("MusicBrainz request throttled/server error (HTTP " + std::to_string(httpCode) +
                      "); retrying (attempt " + std::to_string(attempt + 1) + "/" + std::to_string(maxAttempts) + ").")
                         .c_str());
                std::this_thread::sleep_for(std::chrono::milliseconds(500L * (1L << (attempt - 1))));
                continue;
            }

            if (httpCode >= 400) {
                err(("MusicBrainz metadata request failed with HTTP status " + std::to_string(httpCode) + ".").c_str());
                return false;
            }

            return true;
        }

        return false;
    };

    auto hasAnyRecordingResults = [](const std::string &xml) -> bool {
        pugi::xml_document doc;
        if (!doc.load_string(xml.c_str())) {
            return false;
        }

        std::vector<pugi::xml_node> stack;
        for (pugi::xml_node child = doc.first_child(); child; child = child.next_sibling()) {
            stack.push_back(child);
        }

        while (!stack.empty()) {
            pugi::xml_node node = stack.back();
            stack.pop_back();

            const char *nodeName = node.name();
            if (nodeName != nullptr) {
                const char *localName = std::strrchr(nodeName, ':');
                localName = (localName == nullptr) ? nodeName : localName + 1;
                if (std::strcmp(localName, "recording") == 0) {
                    return true;
                }
            }

            for (pugi::xml_node child = node.first_child(); child; child = child.next_sibling()) {
                stack.push_back(child);
            }
        }

        return false;
    };

    // determine what query is best based on what data is available.
    std::string primarySearchExpr;
    std::string fallbackSearchExpr;
    const std::string escapedTitle = escapeSearchValue(reqData.title);
    const std::string escapedArtist = escapeSearchValue(reqData.artist);
    const std::string escapedAlbum = escapeSearchValue(reqData.album);
    if (!reqData.title.empty() && !reqData.artist.empty() && !reqData.album.empty()) {
        primarySearchExpr = "recording:\"" + escapedTitle + "\" AND artist:\"" + escapedArtist +
                            "\" AND release:\"" + escapedAlbum + "\"";
        fallbackSearchExpr = "recording:\"" + escapedTitle + "\" AND artist:\"" + escapedArtist + "\"";
    } else if (!reqData.title.empty() && !reqData.artist.empty()) {
        primarySearchExpr = "recording:\"" + escapedTitle + "\" AND artist:\"" + escapedArtist + "\"";
    } else if (!reqData.album.empty() && !reqData.artist.empty()) {
        primarySearchExpr = "release:\"" + escapedAlbum + "\" AND artist:\"" + escapedArtist + "\"";
    } else if (!reqData.title.empty()) {
        primarySearchExpr = "recording:\"" + escapedTitle + "\"";
    } else {
        warn("MusicBrainz metadata request: insufficient metadata to construct query.");
        curl_easy_cleanup(curl);
        return "";
    }

    if (!executeSearchExpr(primarySearchExpr, XMLresponseStr)) {
        curl_easy_cleanup(curl);
        return "";
    }

    if (!fallbackSearchExpr.empty() && !hasAnyRecordingResults(XMLresponseStr)) {
        warn("MusicBrainz strict album query returned no results; retrying broader title/artist query.");
        if (!executeSearchExpr(fallbackSearchExpr, XMLresponseStr)) {
            curl_easy_cleanup(curl);
            return "";
        }
    }

    curl_easy_cleanup(curl);
    // this data will be parsed by a seperate function.
    return XMLresponseStr;
}

bitfake::type::MusicBrainzXMLData ParseMBXML(const std::string &xmlStr, const bitfake::type::MBRequestData &reqData) {
    bitfake::type::MusicBrainzXMLData data;
    data.trackNumber = 0;

    auto sanitizeText = [](std::string value) -> std::string {
        auto replaceAll = [&value](const std::string &from, const std::string &to) {
            size_t pos = 0;
            while ((pos = value.find(from, pos)) != std::string::npos) {
                value.replace(pos, from.length(), to);
                pos += to.length();
            }
        };

        replaceAll("\xE2\x80\x99", "'");
        replaceAll("\xE2\x80\x98", "'");

        return value;
    };

    auto nodeNameEquals = [](const pugi::xml_node &node, const char *name) -> bool {
        const char *nodeName = node.name();
        if (nodeName == nullptr || *nodeName == '\0') {
            return false;
        }
        const char *localName = std::strrchr(nodeName, ':');
        if (localName != nullptr) {
            ++localName;
        } else {
            localName = nodeName;
        }
        return std::strcmp(localName, name) == 0;
    };

    auto attrNameEquals = [](const pugi::xml_attribute &attr, const char *name) -> bool {
        const char *attrName = attr.name();
        if (attrName == nullptr || *attrName == '\0') {
            return false;
        }
        const char *localName = std::strrchr(attrName, ':');
        localName = (localName == nullptr) ? attrName : localName + 1;
        return std::strcmp(localName, name) == 0;
    };

    auto getIntAttrByLocalName = [&](const pugi::xml_node &node, const char *name, int fallback) -> int {
        for (const pugi::xml_attribute &attr : node.attributes()) {
            if (attrNameEquals(attr, name)) {
                return attr.as_int(fallback);
            }
        }
        return fallback;
    };

    auto firstChildByName = [&](const pugi::xml_node &parent, const char *name) -> pugi::xml_node {
        for (const pugi::xml_node &child : parent.children()) {
            if (nodeNameEquals(child, name)) {
                return child;
            }
        }
        return pugi::xml_node();
    };

    auto firstDescendantByName = [&](const pugi::xml_node &root, const char *name) -> pugi::xml_node {
        if (nodeNameEquals(root, name)) {
            return root;
        }

        std::vector<pugi::xml_node> stack;
        std::vector<pugi::xml_node> initialChildren;
        for (pugi::xml_node child = root.first_child(); child; child = child.next_sibling()) {
            initialChildren.push_back(child);
        }
        for (auto it = initialChildren.rbegin(); it != initialChildren.rend(); ++it) {
            stack.push_back(*it);
        }

        while (!stack.empty()) {
            pugi::xml_node node = stack.back();
            stack.pop_back();
            if (nodeNameEquals(node, name)) {
                return node;
            }

            std::vector<pugi::xml_node> children;
            for (pugi::xml_node child = node.first_child(); child; child = child.next_sibling()) {
                children.push_back(child);
            }
            for (auto it = children.rbegin(); it != children.rend(); ++it) {
                stack.push_back(*it);
            }
        }
        return pugi::xml_node();
    };

    auto normalizeForCompare = [&](const std::string &input) -> std::string {
        std::string value = sanitizeText(input);
        std::string normalized;
        normalized.reserve(value.size());

        bool prevWasSpace = true;
        for (unsigned char ch : value) {
            if (std::isalnum(ch) != 0) {
                normalized.push_back(static_cast<char>(std::tolower(ch)));
                prevWasSpace = false;
            } else if (std::isspace(ch) != 0) {
                if (!prevWasSpace) {
                    normalized.push_back(' ');
                    prevWasSpace = true;
                }
            }
        }

        if (!normalized.empty() && normalized.back() == ' ') {
            normalized.pop_back();
        }

        return normalized;
    };

    auto scoreTextMatch = [&](const std::string &needleRaw, const std::string &candidateRaw, int exactScore,
                              int containsScore) -> int {
        const std::string needle = normalizeForCompare(needleRaw);
        const std::string candidate = normalizeForCompare(candidateRaw);

        if (needle.empty() || candidate.empty()) {
            return 0;
        }
        if (needle == candidate) {
            return exactScore;
        }
        if (candidate.find(needle) != std::string::npos || needle.find(candidate) != std::string::npos) {
            return containsScore;
        }

        return 0;
    };

    auto datePrecisionScore = [](const std::string &dateStr) -> int {
        // Prefer full ISO date, then year-month, then year.
        if (dateStr.size() >= 10 && std::isdigit(static_cast<unsigned char>(dateStr[0])) != 0 &&
            std::isdigit(static_cast<unsigned char>(dateStr[1])) != 0 &&
            std::isdigit(static_cast<unsigned char>(dateStr[2])) != 0 &&
            std::isdigit(static_cast<unsigned char>(dateStr[3])) != 0 && dateStr[4] == '-' &&
            std::isdigit(static_cast<unsigned char>(dateStr[5])) != 0 &&
            std::isdigit(static_cast<unsigned char>(dateStr[6])) != 0 && dateStr[7] == '-' &&
            std::isdigit(static_cast<unsigned char>(dateStr[8])) != 0 &&
            std::isdigit(static_cast<unsigned char>(dateStr[9])) != 0) {
            return 3;
        }

        if (dateStr.size() >= 7 && std::isdigit(static_cast<unsigned char>(dateStr[0])) != 0 &&
            std::isdigit(static_cast<unsigned char>(dateStr[1])) != 0 &&
            std::isdigit(static_cast<unsigned char>(dateStr[2])) != 0 &&
            std::isdigit(static_cast<unsigned char>(dateStr[3])) != 0 && dateStr[4] == '-' &&
            std::isdigit(static_cast<unsigned char>(dateStr[5])) != 0 &&
            std::isdigit(static_cast<unsigned char>(dateStr[6])) != 0) {
            return 2;
        }

        if (dateStr.size() >= 4 && std::isdigit(static_cast<unsigned char>(dateStr[0])) != 0 &&
            std::isdigit(static_cast<unsigned char>(dateStr[1])) != 0 &&
            std::isdigit(static_cast<unsigned char>(dateStr[2])) != 0 &&
            std::isdigit(static_cast<unsigned char>(dateStr[3])) != 0) {
            return 1;
        }

        return 0;
    };

    auto chooseBetterDate = [&](const std::string &currentBest, const std::string &candidateRaw) -> std::string {
        const std::string candidate = sanitizeText(candidateRaw);
        if (candidate.empty()) {
            return currentBest;
        }

        const int currentScore = datePrecisionScore(currentBest);
        const int candidateScore = datePrecisionScore(candidate);
        if (candidateScore > currentScore) {
            return candidate;
        }

        if (candidateScore == currentScore && candidateScore > 0 &&
            (currentBest.empty() || candidate < currentBest)) {
            // On equal precision, prefer earliest date string for deterministic behavior.
            return candidate;
        }

        return currentBest;
    };

    pugi::xml_document doc;
    pugi::xml_parse_result parsed = doc.load_string(xmlStr.c_str());
    if (!parsed) {
        const std::string wrappedXml = "<bitfake-root>" + xmlStr + "</bitfake-root>";
        parsed = doc.load_string(wrappedXml.c_str());
    }
    if (!parsed) {
        warn(("MusicBrainz metadata parse failed: " + std::string(parsed.description())).c_str());
        return data;
    }

    pugi::xml_node root = doc;

    pugi::xml_node recordingNode;
    pugi::xml_node releaseNode;
    pugi::xml_node artistNode;

    for (const pugi::xml_node &metadataNode : root.children()) {
        if (!nodeNameEquals(metadataNode, "metadata")) {
            continue;
        }

        if (!recordingNode) {
            int bestCompositeScore = std::numeric_limits<int>::min();
            std::vector<pugi::xml_node> searchStack;
            for (pugi::xml_node child = metadataNode.first_child(); child; child = child.next_sibling()) {
                searchStack.push_back(child);
            }

            while (!searchStack.empty()) {
                pugi::xml_node node = searchStack.back();
                searchStack.pop_back();

                if (nodeNameEquals(node, "recording")) {
                    const int scoreAttr = getIntAttrByLocalName(node, "score", 0);
                    const std::string candidateTitle = sanitizeText(firstChildByName(node, "title").text().as_string());

                    std::string candidateArtist;
                    const pugi::xml_node artistCreditNode = firstChildByName(node, "artist-credit");
                    if (artistCreditNode) {
                        candidateArtist = sanitizeText(firstDescendantByName(artistCreditNode, "name").text().as_string());
                    }

                    std::string candidateRelease;
                    const pugi::xml_node releaseListNode = firstChildByName(node, "release-list");
                    if (releaseListNode) {
                        const pugi::xml_node releaseCandidateNode = firstChildByName(releaseListNode, "release");
                        if (releaseCandidateNode) {
                            candidateRelease =
                                sanitizeText(firstChildByName(releaseCandidateNode, "title").text().as_string());
                        }
                    }

                    int compositeScore = scoreAttr * 3;
                    compositeScore += scoreTextMatch(reqData.title, candidateTitle, 220, 120);
                    compositeScore += scoreTextMatch(reqData.artist, candidateArtist, 180, 90);
                    compositeScore += scoreTextMatch(reqData.album, candidateRelease, 100, 45);

                    if (!recordingNode || compositeScore > bestCompositeScore) {
                        recordingNode = node;
                        bestCompositeScore = compositeScore;
                    }
                }

                for (pugi::xml_node child = node.first_child(); child; child = child.next_sibling()) {
                    searchStack.push_back(child);
                }
            }
        }
        if (!releaseNode) {
            releaseNode = firstDescendantByName(metadataNode, "release");
        }
        if (!artistNode) {
            artistNode = firstDescendantByName(metadataNode, "artist");
        }
    }

    if (recordingNode) {
        data.MUSICBRAINZ_TRACKID = recordingNode.attribute("id").as_string();
        data.recordingTitle = sanitizeText(firstChildByName(recordingNode, "title").text().as_string());

        const pugi::xml_node artistCreditNode = firstChildByName(recordingNode, "artist-credit");
        if (artistCreditNode) {
            const pugi::xml_node creditedNameNode = firstDescendantByName(artistCreditNode, "name");
            if (creditedNameNode) {
                data.artistName = sanitizeText(creditedNameNode.text().as_string());
            }
        }

        const pugi::xml_node releaseListNode = firstChildByName(recordingNode, "release-list");
        if (releaseListNode) {
            const pugi::xml_node recordingReleaseNode = firstChildByName(releaseListNode, "release");
            if (recordingReleaseNode) {
                releaseNode = recordingReleaseNode;
            }
        }

        if (!artistNode) {
            const pugi::xml_node artistCreditNode = firstChildByName(recordingNode, "artist-credit");
            if (artistCreditNode) {
                artistNode = firstDescendantByName(artistCreditNode, "artist");
            }
        }
    }

    if (artistNode) {
        data.MUSICBRAINZ_ARTISTID = artistNode.attribute("id").as_string();
        if (data.artistName.empty()) {
            data.artistName = sanitizeText(firstChildByName(artistNode, "name").text().as_string());
        }
    }

    if (releaseNode) {
        data.MUSICBRAINZ_ALBUMID = releaseNode.attribute("id").as_string();
        data.releaseTitle = sanitizeText(firstChildByName(releaseNode, "title").text().as_string());
        data.releaseDate = sanitizeText(firstChildByName(releaseNode, "date").text().as_string());

        // Collect the most precise date available in this release subtree.
        std::vector<pugi::xml_node> dateStack;
        for (pugi::xml_node child = releaseNode.first_child(); child; child = child.next_sibling()) {
            dateStack.push_back(child);
        }
        while (!dateStack.empty()) {
            pugi::xml_node node = dateStack.back();
            dateStack.pop_back();

            if (nodeNameEquals(node, "date")) {
                data.releaseDate = chooseBetterDate(data.releaseDate, node.text().as_string());
            }

            for (pugi::xml_node child = node.first_child(); child; child = child.next_sibling()) {
                dateStack.push_back(child);
            }
        }

        auto toLower = [](std::string value) -> std::string {
            std::transform(value.begin(), value.end(), value.begin(),
                           [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            return value;
        };

        auto parseTrackNumber = [&firstChildByName](const pugi::xml_node &trackNode) -> int {
            int value = firstChildByName(trackNode, "position").text().as_int(0);
            if (value > 0) {
                return value;
            }

            const std::string numberStr = firstChildByName(trackNode, "number").text().as_string();
            int parsed = 0;
            bool hasDigit = false;
            for (char c : numberStr) {
                if (std::isdigit(static_cast<unsigned char>(c)) == 0) {
                    break;
                }
                hasDigit = true;
                parsed = parsed * 10 + (c - '0');
                if (parsed > INT_MAX / 10) {
                    break;
                }
            }
            return hasDigit ? parsed : 0;
        };

        const std::string recordingTitleLower = toLower(data.recordingTitle);
        int bestMatchedPosition = 0;
        int bestTitleMatchedPosition = 0;
        std::vector<pugi::xml_node> trackStack;
        for (pugi::xml_node child = releaseNode.first_child(); child; child = child.next_sibling()) {
            trackStack.push_back(child);
        }

        while (!trackStack.empty()) {
            pugi::xml_node trackNode = trackStack.back();
            trackStack.pop_back();

            if (nodeNameEquals(trackNode, "track")) {
                const pugi::xml_node trackRecordingNode = firstChildByName(trackNode, "recording");
                const int position = parseTrackNumber(trackNode);

                if (trackRecordingNode) {
                    const std::string trackRecordingId = trackRecordingNode.attribute("id").as_string();
                    if (!data.MUSICBRAINZ_TRACKID.empty() && trackRecordingId == data.MUSICBRAINZ_TRACKID) {
                        if (position > 0 && (bestMatchedPosition == 0 || position < bestMatchedPosition)) {
                            bestMatchedPosition = position;
                        }
                    }

                    if (bestTitleMatchedPosition == 0) {
                        std::string trackTitle = sanitizeText(firstChildByName(trackRecordingNode, "title").text().as_string());
                        if (trackTitle.empty()) {
                            trackTitle = sanitizeText(firstChildByName(trackNode, "title").text().as_string());
                        }
                        if (!recordingTitleLower.empty() && !trackTitle.empty() &&
                            toLower(trackTitle) == recordingTitleLower && position > 0) {
                            bestTitleMatchedPosition = position;
                        }
                    }
                } else {
                    const std::string trackTitle = sanitizeText(firstChildByName(trackNode, "title").text().as_string());
                    if (!recordingTitleLower.empty() && !trackTitle.empty() && toLower(trackTitle) == recordingTitleLower &&
                        position > 0 && (bestTitleMatchedPosition == 0 || position < bestTitleMatchedPosition)) {
                        bestTitleMatchedPosition = position;
                    }
                }
            }

            for (pugi::xml_node child = trackNode.first_child(); child; child = child.next_sibling()) {
                trackStack.push_back(child);
            }
        }

        if (bestMatchedPosition > 0) {
            data.trackNumber = bestMatchedPosition;
        } else if (bestTitleMatchedPosition > 0) {
            data.trackNumber = bestTitleMatchedPosition;
        }
    }

    if (recordingNode) {
        // Recording-level fallback when release date is absent/incomplete.
        data.releaseDate = chooseBetterDate(data.releaseDate, firstChildByName(recordingNode, "first-release-date").text().as_string());
    }

    data.date = data.releaseDate;

    std::set<std::string> seenGenres;
    auto appendGenresFromListNode = [&](const pugi::xml_node &listNode, const char *entryTagName) {
        for (const pugi::xml_node &entryNode : listNode.children()) {
            if (!nodeNameEquals(entryNode, entryTagName)) {
                continue;
            }

            std::string genre;
            const pugi::xml_node nameNode = firstChildByName(entryNode, "name");
            if (nameNode) {
                genre = sanitizeText(nameNode.text().as_string());
            } else {
                // Some MusicBrainz responses encode genre text directly on the node.
                genre = sanitizeText(entryNode.text().as_string());
            }

            if (!genre.empty() && seenGenres.insert(genre).second) {
                data.genres.push_back(genre);
            }
        }
    };

    auto collectGenresFromSubtree = [&](const pugi::xml_node &startNode) {
        if (!startNode) {
            return;
        }

        std::vector<pugi::xml_node> stack;
        stack.push_back(startNode);

        while (!stack.empty()) {
            const pugi::xml_node node = stack.back();
            stack.pop_back();

            if (nodeNameEquals(node, "tag-list")) {
                appendGenresFromListNode(node, "tag");
            } else if (nodeNameEquals(node, "genre-list")) {
                appendGenresFromListNode(node, "genre");
            }

            for (pugi::xml_node child = node.first_child(); child; child = child.next_sibling()) {
                stack.push_back(child);
            }
        }
    };

    // Prefer genres from the selected entities to avoid mixing tags from other search hits.
    collectGenresFromSubtree(recordingNode);
    collectGenresFromSubtree(releaseNode);
    collectGenresFromSubtree(artistNode);

    // Fallback for unexpected XML shapes where selected nodes have no tag/genre lists.
    if (data.genres.empty()) {
        collectGenresFromSubtree(root);
    }

    return data;
}

void WriteMetaFromMBXML(const fs::path &inputPath, const bitfake::type::MusicBrainzXMLData &mbData) {
    TagLib::FileRef fileRef(inputPath.string().c_str());
    if (fileRef.isNull() || fileRef.tag() == nullptr) {
        err("Failed to write MusicBrainz metadata: unsupported format or unreadable tags.");
        return;
    }

    TagLib::PropertyMap drawer = fileRef.file()->properties();
    std::vector<std::string> stagedKeys;

    auto stageIfNotEmpty = [&](const std::string &key, const std::string &value) {
        if (value.empty()) {
            return;
        }
        bitfake::tagging::StageMetaDataChanges(drawer, key, value);
        std::string printedKey = key;
        std::transform(printedKey.begin(), printedKey.end(), printedKey.begin(),
                       [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
        stagedKeys.push_back(printedKey);
    };

    // Basic Metadata
    stageIfNotEmpty("Title", mbData.recordingTitle);
    if (!mbData.artistName.empty()) {
        stageIfNotEmpty("Artist", mbData.artistName);
        stageIfNotEmpty("Album Artist", mbData.artistName);
    }
    stageIfNotEmpty("Album", mbData.releaseTitle);
    stageIfNotEmpty("Date", mbData.releaseDate);
    if (mbData.trackNumber > 0) {
        stageIfNotEmpty("TrackNumber", std::to_string(mbData.trackNumber));
    }

    std::string genreStr;
    if (!mbData.genres.empty()) {
        for (const std::string &genre : mbData.genres) {
            if (!genreStr.empty()) {
                genreStr += "; ";
            }
            genreStr += genre;
        }
    }
    stageIfNotEmpty("Genre", genreStr);

    // musicbrainz ids
    stageIfNotEmpty("MUSICBRAINZ_ALBUMID", mbData.MUSICBRAINZ_ALBUMID);
    stageIfNotEmpty("MUSICBRAINZ_ARTISTID", mbData.MUSICBRAINZ_ARTISTID);
    stageIfNotEmpty("MUSICBRAINZ_TRACKID", mbData.MUSICBRAINZ_TRACKID);

    const bool committed = bitfake::tagging::CommitMetaDataChanges(inputPath, drawer);
    if (!committed) {
        err("MusicBrainz metadata commit failed.");
        return;
    }

    if (stagedKeys.empty()) {
        warn("MusicBrainz write completed with no metadata keys staged.");
        return;
    }

    std::string keyList;
    for (const std::string &key : stagedKeys) {
        if (!keyList.empty()) {
            keyList += ", ";
        }
        keyList += key;
    }
    plog(("MusicBrainz applied metadata keys: " + keyList).c_str());

    return;
}

// musicbrainz end
// lrclib start

// type::LRCRequestData PrepareLRCRequestData(const fs::path &inputPath);
// type::LRCLibData GetLRCLibData(const type::LRCRequestData &reqData);
// type::LRCLibData ParseLRCLibData(const std::string &responseStr, const type::LRCRequestData &reqData);

type::LRCRequestData PrepareLRCRequestData(const fs::path &inputPath) {
    type::LRCRequestData requestData;

    auto trim = [](const std::string &str) -> std::string {
        const size_t first = str.find_first_not_of(" \t\n\r\f\v");
        if (first == std::string::npos) {
            return "";
        }
        const size_t last = str.find_last_not_of(" \t\n\r\f\v");
        return str.substr(first, last - first + 1);
    };

    const type::AudioMetadata md = bitfake::extract::GetMetaData(inputPath);
    requestData.inputPath = inputPath;
    requestData.artist = trim(md.artist);
    requestData.title = trim(md.title);
    requestData.album = trim(md.album);
    requestData.trackNumber = md.trackNumber;

    return requestData;
}

type::LRCLibData ParseLRCLibData(const std::string &responseStr, const type::LRCRequestData &reqData) {
    type::LRCLibData data;
    data.requestData = reqData;

    auto extractJsonString = [&](const std::string &json, const std::string &key) -> std::string {
        const std::string keyToken = "\"" + key + "\"";
        const size_t keyPos = json.find(keyToken);
        if (keyPos == std::string::npos) {
            return "";
        }

        const size_t colonPos = json.find(':', keyPos + keyToken.size());
        if (colonPos == std::string::npos) {
            return "";
        }

        size_t valuePos = colonPos + 1;
        while (valuePos < json.size() && std::isspace(static_cast<unsigned char>(json[valuePos])) != 0) {
            ++valuePos;
        }

        if (valuePos >= json.size() || json[valuePos] != '"') {
            return "";
        }

        ++valuePos;
        std::string out;
        while (valuePos < json.size()) {
            const char c = json[valuePos++];
            if (c == '\\' && valuePos < json.size()) {
                const char esc = json[valuePos++];
                switch (esc) {
                    case 'n':
                        out.push_back('\n');
                        break;
                    case 'r':
                        out.push_back('\r');
                        break;
                    case 't':
                        out.push_back('\t');
                        break;
                    case '"':
                        out.push_back('"');
                        break;
                    case '\\':
                        out.push_back('\\');
                        break;
                    default:
                        out.push_back(esc);
                        break;
                }
                continue;
            }

            if (c == '"') {
                break;
            }
            out.push_back(c);
        }

        return out;
    };

    data.SyncLyrics = extractJsonString(responseStr, "syncedLyrics");
    if (data.SyncLyrics.empty()) {
        data.SyncLyrics = extractJsonString(responseStr, "synced_lyrics");
    }

    data.NoSyncLyrics = extractJsonString(responseStr, "plainLyrics");
    if (data.NoSyncLyrics.empty()) {
        data.NoSyncLyrics = extractJsonString(responseStr, "plain_lyrics");
    }

    data.Sourcelink = "https://lrclib.net/";
    return data;
}

static std::string SanitizeFilenamePart(std::string value) {
    for (char &ch : value) {
        if (ch == '/' || ch == '\\' || ch == ':' || ch == '*' || ch == '?' || ch == '"' || ch == '<' || ch == '>' ||
            ch == '|') {
            ch = '_';
        }
    }

    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back())) != 0) {
        value.pop_back();
    }
    size_t start = 0;
    while (start < value.size() && std::isspace(static_cast<unsigned char>(value[start])) != 0) {
        ++start;
    }
    if (start > 0) {
        value.erase(0, start);
    }
    return value;
}

fs::path GetLRCLibOutputPath(const type::LRCRequestData &reqData) {
    fs::path outputDir = reqData.inputPath.empty() ? fs::current_path() : reqData.inputPath.parent_path();
    std::string baseName;

    if (!reqData.inputPath.empty() && !reqData.inputPath.stem().string().empty()) {
        baseName = reqData.inputPath.stem().string();
    } else {
        const std::string artistPart = SanitizeFilenamePart(reqData.artist);
        const std::string titlePart = SanitizeFilenamePart(reqData.title);
        if (!artistPart.empty() && !titlePart.empty()) {
            baseName = artistPart + " - " + titlePart;
        } else if (!titlePart.empty()) {
            baseName = titlePart;
        } else {
            baseName = "lyrics";
        }
    }

    return outputDir / (SanitizeFilenamePart(baseName) + ".lrc");
}

bool WriteLRCLibToFile(const type::LRCRequestData &reqData, const type::LRCLibData &lrcData, fs::path &writtenPath) {
    const std::string lyricsToWrite = !lrcData.SyncLyrics.empty() ? lrcData.SyncLyrics : lrcData.NoSyncLyrics;
    if (lyricsToWrite.empty()) {
        warn("LRCLib returned no lyrics to write.");
        return false;
    }

    const fs::path outPath = GetLRCLibOutputPath(reqData);
    std::ofstream outFile(outPath, std::ios::out | std::ios::trunc);
    if (!outFile.is_open()) {
        err(("Failed to write LRCLib lyrics file: " + outPath.string()).c_str());
        return false;
    }

    outFile << lyricsToWrite;
    outFile.close();

    writtenPath = outPath;
    plog(("LRCLib lyrics written to: " + outPath.string()).c_str());
    return true;
}

type::LRCLibData GetLRCLibData(const type::LRCRequestData &reqData) {
    type::LRCLibData emptyResult;
    emptyResult.requestData = reqData;

    if (reqData.title.empty() || reqData.artist.empty()) {
        warn("LRCLib request skipped: title and artist are required.");
        return emptyResult;
    }

    CURL *curl = curl_easy_init();
    if (!curl) {
        err("Failed to initialize libcurl for LRCLib request.");
        return emptyResult;
    }

    std::string response;
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

    const std::string userAgent = "bitfake2/" + gb::version + " (ray@atl.tools)";
    curl_easy_setopt(curl, CURLOPT_USERAGENT, userAgent.c_str());
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);

    char *encArtist = curl_easy_escape(curl, reqData.artist.c_str(), static_cast<int>(reqData.artist.size()));
    char *encTitle = curl_easy_escape(curl, reqData.title.c_str(), static_cast<int>(reqData.title.size()));
    char *encAlbum = nullptr;
    if (!reqData.album.empty()) {
        encAlbum = curl_easy_escape(curl, reqData.album.c_str(), static_cast<int>(reqData.album.size()));
    }

    if (encArtist == nullptr || encTitle == nullptr || (!reqData.album.empty() && encAlbum == nullptr)) {
        if (encArtist != nullptr) {
            curl_free(encArtist);
        }
        if (encTitle != nullptr) {
            curl_free(encTitle);
        }
        if (encAlbum != nullptr) {
            curl_free(encAlbum);
        }
        curl_easy_cleanup(curl);
        err("LRCLib request failed: unable to URL-encode query.");
        return emptyResult;
    }

    std::string url = "https://lrclib.net/api/get?artist_name=" + std::string(encArtist) +
                      "&track_name=" + std::string(encTitle);
    if (encAlbum != nullptr) {
        url += "&album_name=" + std::string(encAlbum);
    }

    curl_free(encArtist);
    curl_free(encTitle);
    if (encAlbum != nullptr) {
        curl_free(encAlbum);
    }

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());

    const CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        err(("LRCLib request failed: " + std::string(curl_easy_strerror(res))).c_str());
        curl_easy_cleanup(curl);
        return emptyResult;
    }

    long httpCode = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
    curl_easy_cleanup(curl);

    if (httpCode >= 400) {
        warn(("LRCLib request returned HTTP status " + std::to_string(httpCode) + ".").c_str());
        return emptyResult;
    }

    return ParseLRCLibData(response, reqData);
}

} // namespace bitfake::online
