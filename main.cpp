#include <curl/curl.h>

#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <utility>

namespace {

std::string escapeJsonString(const std::string& value) {
    static const char hex[] = "0123456789abcdef";
    std::string escaped;
    escaped.reserve(value.size());

    for (unsigned char ch : value) {
        switch (ch) {
            case '"':
                escaped += "\\\"";
                break;
            case '\\':
                escaped += "\\\\";
                break;
            case '\b':
                escaped += "\\b";
                break;
            case '\f':
                escaped += "\\f";
                break;
            case '\n':
                escaped += "\\n";
                break;
            case '\r':
                escaped += "\\r";
                break;
            case '\t':
                escaped += "\\t";
                break;
            default:
                if (ch < 0x20) {
                    escaped += "\\u00";
                    escaped += hex[ch >> 4];
                    escaped += hex[ch & 0x0f];
                } else {
                    // Bytes outside the ASCII control range are preserved as UTF-8.
                    escaped += static_cast<char>(ch);
                }
        }
    }
    return escaped;
}

size_t writeResponse(char* data, size_t size, size_t count, void* userData) {
    if (size != 0 && count > static_cast<size_t>(-1) / size) {
        return 0;
    }
    const size_t byteCount = size * count;
    static_cast<std::string*>(userData)->append(data, byteCount);
    return byteCount;
}

void appendUtf8(std::string& output, std::uint32_t codePoint) {
    if (codePoint <= 0x7f) {
        output += static_cast<char>(codePoint);
    } else if (codePoint <= 0x7ff) {
        output += static_cast<char>(0xc0 | (codePoint >> 6));
        output += static_cast<char>(0x80 | (codePoint & 0x3f));
    } else if (codePoint <= 0xffff) {
        output += static_cast<char>(0xe0 | (codePoint >> 12));
        output += static_cast<char>(0x80 | ((codePoint >> 6) & 0x3f));
        output += static_cast<char>(0x80 | (codePoint & 0x3f));
    } else {
        output += static_cast<char>(0xf0 | (codePoint >> 18));
        output += static_cast<char>(0x80 | ((codePoint >> 12) & 0x3f));
        output += static_cast<char>(0x80 | ((codePoint >> 6) & 0x3f));
        output += static_cast<char>(0x80 | (codePoint & 0x3f));
    }
}

int hexValue(char ch) {
    if (ch >= '0' && ch <= '9') return ch - '0';
    if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
    if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
    return -1;
}

bool readHexCodeUnit(const std::string& json, size_t& position,
                     std::uint32_t& codeUnit) {
    if (position + 4 > json.size()) return false;

    codeUnit = 0;
    for (int i = 0; i < 4; ++i) {
        const int digit = hexValue(json[position++]);
        if (digit < 0) return false;
        codeUnit = (codeUnit << 4) | static_cast<std::uint32_t>(digit);
    }
    return true;
}

bool decodeJsonString(const std::string& json, size_t& position,
                      std::string& decoded) {
    if (position >= json.size() || json[position] != '"') return false;
    ++position;

    while (position < json.size()) {
        const unsigned char ch = static_cast<unsigned char>(json[position++]);
        if (ch == '"') return true;
        if (ch != '\\') {
            decoded += static_cast<char>(ch);
            continue;
        }

        if (position >= json.size()) return false;
        const char escaped = json[position++];
        switch (escaped) {
            case '"': decoded += '"'; break;
            case '\\': decoded += '\\'; break;
            case '/': decoded += '/'; break;
            case 'b': decoded += '\b'; break;
            case 'f': decoded += '\f'; break;
            case 'n': decoded += '\n'; break;
            case 'r': decoded += '\r'; break;
            case 't': decoded += '\t'; break;
            case 'u': {
                std::uint32_t codePoint = 0;
                if (!readHexCodeUnit(json, position, codePoint)) return false;

                if (codePoint >= 0xd800 && codePoint <= 0xdbff) {
                    if (position + 2 <= json.size() &&
                        json[position] == '\\' && json[position + 1] == 'u') {
                        position += 2;
                        std::uint32_t lowSurrogate = 0;
                        if (!readHexCodeUnit(json, position, lowSurrogate)) {
                            return false;
                        }
                        if (lowSurrogate >= 0xdc00 && lowSurrogate <= 0xdfff) {
                            codePoint = 0x10000 +
                                ((codePoint - 0xd800) << 10) +
                                (lowSurrogate - 0xdc00);
                        } else {
                            appendUtf8(decoded, 0xfffd);
                            appendUtf8(decoded, lowSurrogate);
                            break;
                        }
                    } else {
                        appendUtf8(decoded, 0xfffd);
                        break;
                    }
                } else if (codePoint >= 0xdc00 && codePoint <= 0xdfff) {
                    codePoint = 0xfffd;
                }
                appendUtf8(decoded, codePoint);
                break;
            }
            default:
                return false;
        }
    }
    return false;
}

void skipWhitespace(const std::string& text, size_t& position) {
    while (position < text.size() &&
           std::isspace(static_cast<unsigned char>(text[position]))) {
        ++position;
    }
}

bool extractGeneratedText(const std::string& response, std::string& text) {
    const size_t candidates = response.find("\"candidates\"");
    if (candidates == std::string::npos) return false;

    size_t key = response.find("\"text\"", candidates);
    while (key != std::string::npos) {
        size_t position = response.find(':', key + 6);
        if (position == std::string::npos) return false;
        ++position;
        skipWhitespace(response, position);
        if (position < response.size() && response[position] == '"') {
            std::string decoded;
            if (decodeJsonString(response, position, decoded)) {
                text = std::move(decoded);
                return true;
            }
        }
        key = response.find("\"text\"", key + 6);
    }
    return false;
}

}  // namespace

int main() {
    const char* apiKey = std::getenv("GEMINI_API_KEY");
    if (apiKey == nullptr || *apiKey == '\0') {
        std::cerr << "Error: GEMINI_API_KEY is not set. Add your Gemini API key "
                     "to the environment and run this program again.\n";
        return 1;
    }

    for (const unsigned char ch : std::string(apiKey)) {
        if (ch <= 0x20 || ch == 0x7f) {
            std::cerr << "Error: GEMINI_API_KEY contains invalid whitespace or "
                         "control characters.\n";
            return 1;
        }
    }

    std::ifstream lectureFile("lecture.txt", std::ios::binary);
    if (!lectureFile) {
        std::cerr << "Error: Could not open lecture.txt. Run the program from "
                     "the directory containing the file.\n";
        return 1;
    }

    const std::string lectureNotes(
        (std::istreambuf_iterator<char>(lectureFile)),
        std::istreambuf_iterator<char>());
    if (lectureNotes.find_first_not_of(" \t\r\n") == std::string::npos) {
        std::cerr << "Error: lecture.txt is empty.\n";
        return 1;
    }

    const std::string prompt =
        "Read the lecture notes below. Return a concise bulleted summary, "
        "followed by exactly 3 multiple-choice flashcard questions. Give each "
        "question four options labeled A, B, C, and D; identify the correct "
        "answer and include a brief explanation. Base everything only on the "
        "lecture notes.\n\nLecture notes:\n" + lectureNotes;

    const std::string requestBody =
        "{\"contents\":[{\"parts\":[{\"text\":\"" +
        escapeJsonString(prompt) + "\"}]}]}";
    const std::string endpoint =
        "https://generativelanguage.googleapis.com/v1beta/models/"
        "gemini-3.5-flash:generateContent";

    if (curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK) {
        std::cerr << "Error: Could not initialize libcurl.\n";
        return 1;
    }

    CURL* curl = curl_easy_init();
    if (curl == nullptr) {
        std::cerr << "Error: Could not create a libcurl request.\n";
        curl_global_cleanup();
        return 1;
    }

    std::string responseBody;
    std::string apiKeyHeader = "x-goog-api-key: ";
    apiKeyHeader += apiKey;

    curl_slist* headers =
        curl_slist_append(nullptr, "Content-Type: application/json");
    if (headers == nullptr) {
        std::cerr << "Error: Could not allocate HTTP request headers.\n";
        curl_easy_cleanup(curl);
        curl_global_cleanup();
        return 1;
    }
    curl_slist* headersWithApiKey =
        curl_slist_append(headers, apiKeyHeader.c_str());
    if (headersWithApiKey == nullptr) {
        std::cerr << "Error: Could not allocate HTTP request headers.\n";
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
        curl_global_cleanup();
        return 1;
    }
    headers = headersWithApiKey;

    curl_easy_setopt(curl, CURLOPT_URL, endpoint.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, requestBody.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE_LARGE,
                     static_cast<curl_off_t>(requestBody.size()));
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeResponse);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &responseBody);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 20L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 120L);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);

    char curlError[CURL_ERROR_SIZE] = {};
    curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, curlError);

    const CURLcode curlResult = curl_easy_perform(curl);
    long httpStatus = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpStatus);

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    curl_global_cleanup();

    if (curlResult != CURLE_OK) {
        std::cerr << "Error: Gemini request failed: "
                  << (curlError[0] != '\0' ? curlError
                                           : curl_easy_strerror(curlResult))
                  << '\n';
        return 1;
    }

    if (httpStatus < 200 || httpStatus >= 300) {
        std::cerr << "Error: Gemini returned HTTP " << httpStatus << ".\n";
        if (!responseBody.empty()) std::cerr << responseBody << '\n';
        return 1;
    }

    std::string generatedText;
    if (extractGeneratedText(responseBody, generatedText)) {
        std::cout << generatedText << '\n';
    } else {
        std::cout << "Gemini response:\n" << responseBody << '\n';
    }
    return 0;
}
