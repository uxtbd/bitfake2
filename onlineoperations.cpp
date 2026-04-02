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

namespace bitfake::musicbrainz {

// Helper functions to help extract metadata from a JSON/XML
// which is expected from libcurl when we request metadata from musicbrainz
// i do not plan on adding any other database because MB is public, and FOSS.

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

    // determine what query is best based on what data is available.
    std::string searchExpr;
    if (!reqData.title.empty() && !reqData.artist.empty()) {
        searchExpr = "recording:\"" + reqData.title + "\" AND artist:\"" + reqData.artist + "\"";
    } else if (!reqData.album.empty() && !reqData.artist.empty()) {
        searchExpr = "release:\"" + reqData.album + "\" AND artist:\"" + reqData.artist + "\"";
    } else if (!reqData.title.empty()) {
        searchExpr = "recording:\"" + reqData.title + "\"";
    } else {
        warn("MusicBrainz metadata request: insufficient metadata to construct query.");
        curl_easy_cleanup(curl);
        return "";
    }

    char *encodedExpr = curl_easy_escape(curl, searchExpr.c_str(), static_cast<int>(searchExpr.size()));
    if (encodedExpr == nullptr) {
        err("MusicBrainz metadata request failed: unable to URL-encode query.");
        curl_easy_cleanup(curl);
        return "";
    }

    std::string MBurl = "https://musicbrainz.org/ws/2/recording?query=" + std::string(encodedExpr) +
                        "&fmt=xml&limit=1&inc=artist-credits+releases+media+recordings+tags+genres";
    curl_free(encodedExpr);

    curl_easy_setopt(curl, CURLOPT_URL, MBurl.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &XMLresponseStr);

    std::string userAgent = "bitfake2/" + gb::version + "(ray@atl.tools)";
    curl_easy_setopt(curl, CURLOPT_USERAGENT, userAgent.c_str());

    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L); // 30 second timeout for slow internets

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        err(("MusicBrainz metadata request failed: " + std::string(curl_easy_strerror(res))).c_str());
        curl_easy_cleanup(curl);
        return "";
    }

    auto extractAttributeFromTag = [](const std::string &xml, const std::string &tagName, const std::string &attribute,
                                      size_t startPos = 0) -> std::string {
        const std::string openTagWithAttrs = "<" + tagName + " ";
        size_t tagStart = xml.find(openTagWithAttrs, startPos);
        if (tagStart == std::string::npos) {
            const std::string openTagBare = "<" + tagName + ">";
            tagStart = xml.find(openTagBare, startPos);
        }
        if (tagStart == std::string::npos) {
            return "";
        }

        const size_t tagEnd = xml.find(">", tagStart);
        if (tagEnd == std::string::npos) {
            return "";
        }

        const std::string tagContent = xml.substr(tagStart, tagEnd - tagStart);
        const std::string attrPattern = attribute + "=\"";
        const size_t attrStart = tagContent.find(attrPattern);
        if (attrStart == std::string::npos) {
            return "";
        }

        const size_t valueStart = attrStart + attrPattern.length();
        const size_t valueEnd = tagContent.find("\"", valueStart);
        if (valueEnd == std::string::npos) {
            return "";
        }

        return tagContent.substr(valueStart, valueEnd - valueStart);
    };

    auto fetchEntityXml = [&](const std::string &url) -> std::string {
        std::string response;
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
        CURLcode fallbackRes = curl_easy_perform(curl);
        if (fallbackRes != CURLE_OK) {
            warn(
                ("MusicBrainz genre fallback request failed: " + std::string(curl_easy_strerror(fallbackRes))).c_str());
            return "";
        }
        return response;
    };

    size_t artistCreditStart = XMLresponseStr.find("<artist-credit>");
    const std::string artistId = extractAttributeFromTag(
        XMLresponseStr, "artist", "id", artistCreditStart == std::string::npos ? 0 : artistCreditStart);

    size_t releaseListStart = XMLresponseStr.find("<release-list>");
    const std::string releaseId = extractAttributeFromTag(XMLresponseStr, "release", "id",
                                                          releaseListStart == std::string::npos ? 0 : releaseListStart);

    if (!releaseId.empty()) {
        const std::string releaseUrl =
            "https://musicbrainz.org/ws/2/release/" + releaseId + "?fmt=xml&inc=media+recordings+tags+genres";
        const std::string releaseXml = fetchEntityXml(releaseUrl);
        if (!releaseXml.empty()) {
            XMLresponseStr += "\n" + releaseXml;
        }
    }

    if (!artistId.empty()) {
        const std::string artistUrl = "https://musicbrainz.org/ws/2/artist/" + artistId + "?fmt=xml&inc=tags+genres";
        const std::string artistXml = fetchEntityXml(artistUrl);
        if (!artistXml.empty()) {
            XMLresponseStr += "\n" + artistXml;
        }
    }

    curl_easy_cleanup(curl);
    // this data will be parsed by a seperate function.
    return XMLresponseStr;
}

bitfake::type::MusicBrainzXMLData ParseMBXML(const std::string &xmlStr) {
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

        replaceAll("&amp;", "&");
        replaceAll("&lt;", "<");
        replaceAll("&gt;", ">");
        replaceAll("&quot;", "\"");
        replaceAll("&apos;", "'");
        replaceAll("&#39;", "'");
        replaceAll("\xE2\x80\x99", "'");
        replaceAll("\xE2\x80\x98", "'");

        return value;
    };

    auto extractTagValue = [&xmlStr](const std::string &tag, size_t startSearchPos = 0) -> std::string {
        std::string openTag = "<" + tag + ">";
        std::string closeTag = "</" + tag + ">";
        size_t startPos = xmlStr.find(openTag, startSearchPos);
        size_t endPos = xmlStr.find(closeTag, startPos);
        if (startPos != std::string::npos && endPos != std::string::npos && endPos > startPos) {
            return xmlStr.substr(startPos + openTag.length(), endPos - (startPos + openTag.length()));
        }
        return "";
    };

    auto findExactTagStart = [&xmlStr](const std::string &tagName, size_t startSearchPos = 0) -> size_t {
        const std::string withAttrs = "<" + tagName + " ";
        size_t tagStart = xmlStr.find(withAttrs, startSearchPos);
        if (tagStart != std::string::npos) {
            return tagStart;
        }

        const std::string bareTag = "<" + tagName + ">";
        return xmlStr.find(bareTag, startSearchPos);
    };

    auto extractAttribute = [&xmlStr, &findExactTagStart](const std::string &tagName, const std::string &attribute,
                                                          size_t startSearchPos = 0) -> std::string {
        size_t tagStart = findExactTagStart(tagName, startSearchPos);
        if (tagStart == std::string::npos)
            return "";

        size_t tagEnd = xmlStr.find(">", tagStart);
        if (tagEnd == std::string::npos)
            return "";

        std::string tagContent = xmlStr.substr(tagStart, tagEnd - tagStart);
        std::string attrPattern = attribute + "=\"";
        size_t attrStart = tagContent.find(attrPattern);
        if (attrStart == std::string::npos)
            return "";

        size_t valueStart = attrStart + attrPattern.length();
        size_t valueEnd = tagContent.find("\"", valueStart);
        if (valueEnd == std::string::npos)
            return "";

        return tagContent.substr(valueStart, valueEnd - valueStart);
    };

    data.recordingTitle = sanitizeText(extractTagValue("title", 0));

    std::string recordingId = extractAttribute("recording", "id", 0);
    data.MUSICBRAINZ_TRACKID = recordingId;

    size_t artistCreditStart = xmlStr.find("<artist-credit>");
    if (artistCreditStart != std::string::npos) {
        data.artistName = sanitizeText(extractTagValue("name", artistCreditStart));
    }

    size_t artistStart = xmlStr.find("<artist ", artistCreditStart == std::string::npos ? 0 : artistCreditStart);
    if (artistStart == std::string::npos) {
        artistStart = xmlStr.find("<artist>", artistCreditStart == std::string::npos ? 0 : artistCreditStart);
    }
    if (artistStart != std::string::npos) {
        data.MUSICBRAINZ_ARTISTID = extractAttribute("artist", "id", artistStart);
        if (data.artistName.empty()) {
            data.artistName = sanitizeText(extractTagValue("name", artistStart));
        }
    }

    size_t releaseListStart = xmlStr.find("<release-list");
    if (releaseListStart != std::string::npos) {
        size_t releaseStart = findExactTagStart("release", releaseListStart);
        if (releaseStart != std::string::npos) {
            data.MUSICBRAINZ_ALBUMID = extractAttribute("release", "id", releaseStart);
            data.releaseTitle = sanitizeText(extractTagValue("title", releaseStart));
            data.releaseDate = sanitizeText(extractTagValue("date", releaseStart));

            const std::string recordingMarker =
                data.MUSICBRAINZ_TRACKID.empty() ? "" : "<recording id=\"" + data.MUSICBRAINZ_TRACKID + "\"";

            size_t searchPos = releaseStart;
            while (true) {
                size_t trackStart = xmlStr.find("<track", searchPos);
                if (trackStart == std::string::npos) {
                    break;
                }

                const size_t tagNameEnd = trackStart + 6;
                if (tagNameEnd < xmlStr.size() && xmlStr[tagNameEnd] != ' ' && xmlStr[tagNameEnd] != '>') {
                    searchPos = tagNameEnd;
                    continue;
                }

                size_t trackOpenEnd = xmlStr.find(">", trackStart);
                if (trackOpenEnd == std::string::npos) {
                    break;
                }

                const bool selfClosing = trackOpenEnd > trackStart && xmlStr[trackOpenEnd - 1] == '/';
                if (selfClosing) {
                    searchPos = trackOpenEnd + 1;
                    continue;
                }

                size_t trackEnd = xmlStr.find("</track>", trackOpenEnd);
                if (trackEnd == std::string::npos) {
                    break;
                }

                std::string trackBlock = xmlStr.substr(trackOpenEnd + 1, trackEnd - (trackOpenEnd + 1));
                const bool isMatchedRecording =
                    recordingMarker.empty() || trackBlock.find(recordingMarker) != std::string::npos;

                if (isMatchedRecording) {
                    size_t positionStart = trackBlock.find("<position>");
                    size_t positionEnd = trackBlock.find("</position>", positionStart);
                    if (positionStart != std::string::npos && positionEnd != std::string::npos &&
                        positionEnd > positionStart) {
                        std::string positionStr =
                            trackBlock.substr(positionStart + 10, positionEnd - (positionStart + 10));
                        try {
                            data.trackNumber = std::stoi(positionStr);
                        } catch (...) {
                            data.trackNumber = 0;
                        }
                        if (data.trackNumber > 0) {
                            break;
                        }
                    }
                }

                searchPos = trackEnd + 8;
            }
        }
    }

    data.date = data.releaseDate;

    auto appendGenresFromList = [&](const std::string &listTag) {
        const std::string openPrefix = "<" + listTag;
        const std::string closeTag = "</" + listTag + ">";
        size_t listStart = 0;

        while ((listStart = xmlStr.find(openPrefix, listStart)) != std::string::npos) {
            size_t listOpenEnd = xmlStr.find(">", listStart);
            size_t listEnd = xmlStr.find(closeTag, listOpenEnd == std::string::npos ? listStart : listOpenEnd);
            if (listOpenEnd == std::string::npos) {
                break;
            }

            const bool selfClosing = listOpenEnd > listStart && xmlStr[listOpenEnd - 1] == '/';
            if (selfClosing) {
                listStart = listOpenEnd + 1;
                continue;
            }

            if (listEnd == std::string::npos || listEnd <= listOpenEnd) {
                listStart = listOpenEnd + 1;
                continue;
            }

            std::string listStr = xmlStr.substr(listOpenEnd + 1, listEnd - (listOpenEnd + 1));
            size_t pos = 0;
            while ((pos = listStr.find("<name>", pos)) != std::string::npos) {
                size_t endPos = listStr.find("</name>", pos);
                if (endPos == std::string::npos) {
                    break;
                }

                std::string genre = sanitizeText(listStr.substr(pos + 6, endPos - (pos + 6)));
                if (!genre.empty() && std::find(data.genres.begin(), data.genres.end(), genre) == data.genres.end()) {
                    data.genres.push_back(genre);
                }
                pos = endPos + 7;
            }

            listStart = listEnd + closeTag.length();
        }
    };

    appendGenresFromList("tag-list");
    appendGenresFromList("genre-list");

    return data;
}

void WriteMetaFromMBXML(const fs::path &inputPath, const bitfake::type::MusicBrainzXMLData &mbData) {
    TagLib::FileRef fileRef(inputPath.string().c_str());
    if (fileRef.isNull() || fileRef.tag() == nullptr) {
        err("Failed to write MusicBrainz metadata: unsupported format or unreadable tags.");
        return;
    }

    TagLib::PropertyMap drawer = fileRef.file()->properties();

    // Basic Metadata
    if (!mbData.recordingTitle.empty()) {
        bitfake::tagging::StageMetaDataChanges(drawer, "Title", mbData.recordingTitle);
    }
    if (!mbData.artistName.empty()) {
        bitfake::tagging::StageMetaDataChanges(drawer, "Artist", mbData.artistName);
        bitfake::tagging::StageMetaDataChanges(drawer, "Album Artist", mbData.artistName);
    }
    if (!mbData.releaseTitle.empty()) {
        bitfake::tagging::StageMetaDataChanges(drawer, "Album", mbData.releaseTitle);
    }
    if (!mbData.releaseDate.empty()) {
        bitfake::tagging::StageMetaDataChanges(drawer, "Date", mbData.releaseDate);
    }
    if (mbData.trackNumber > 0) {
        bitfake::tagging::StageMetaDataChanges(drawer, "TrackNumber", std::to_string(mbData.trackNumber));
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
    if (!genreStr.empty()) {
        bitfake::tagging::StageMetaDataChanges(drawer, "Genre", genreStr);
    }

    // musicbrainz ids
    if (!mbData.MUSICBRAINZ_ALBUMID.empty()) {
        bitfake::tagging::StageMetaDataChanges(drawer, "MUSICBRAINZ_ALBUMID", mbData.MUSICBRAINZ_ALBUMID);
    }
    if (!mbData.MUSICBRAINZ_ARTISTID.empty()) {
        bitfake::tagging::StageMetaDataChanges(drawer, "MUSICBRAINZ_ARTISTID", mbData.MUSICBRAINZ_ARTISTID);
    }
    if (!mbData.MUSICBRAINZ_TRACKID.empty()) {
        bitfake::tagging::StageMetaDataChanges(drawer, "MUSICBRAINZ_TRACKID", mbData.MUSICBRAINZ_TRACKID);
    }

    bitfake::tagging::CommitMetaDataChanges(inputPath, drawer);

    return;
}

} // namespace bitfake::musicbrainz
