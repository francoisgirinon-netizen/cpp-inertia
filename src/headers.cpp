#include <inertia/attributes.hpp>
#include <inertia/headers.hpp>
#include <inertia/response.hpp>

#include <algorithm>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace inertia {

namespace {

bool isListSpace(char c) INERTIA_NONALLOCATING {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

std::string_view trim(std::string_view text) INERTIA_NONALLOCATING {
    while (!text.empty() && isListSpace(text.front())) {
        text.remove_prefix(1);
    }
    while (!text.empty() && isListSpace(text.back())) {
        text.remove_suffix(1);
    }
    return text;
}

// ASCII only, like the comparison it serves: std::tolower depends on the C locale, and cannot be
// called from a function that must not allocate.
char toLowerAscii(char c) INERTIA_NONALLOCATING {
    return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c;
}

} // namespace

namespace headers {

std::vector<std::string> parseList(const std::optional<std::string> &raw) {
    std::vector<std::string> items;
    if (!raw) {
        return items;
    }
    std::string_view rest = *raw;
    while (true) {
        const auto comma = rest.find(',');
        const auto item = trim(rest.substr(0, comma));
        if (!item.empty()) {
            items.emplace_back(item);
        }
        if (comma == std::string_view::npos) {
            break;
        }
        rest.remove_prefix(comma + 1);
    }
    return items;
}

bool iequals(std::string_view a, std::string_view b) INERTIA_NONALLOCATING {
    return std::ranges::equal(a, b,
                              [](char x, char y) { return toLowerAscii(x) == toLowerAscii(y); });
}

} // namespace headers

std::optional<std::string> Response::header(std::string_view name) const {
    for (const auto &[key, value] : headers) {
        if (inertia::headers::iequals(key, name)) {
            return value;
        }
    }
    return std::nullopt;
}

void Response::setHeader(std::string name, std::string value) {
    std::erase_if(headers,
                  [&](const auto &entry) { return inertia::headers::iequals(entry.first, name); });
    headers.emplace_back(std::move(name), std::move(value));
}

} // namespace inertia
