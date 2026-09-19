#include "search_university_people_params_parser.hpp"

#include <charconv>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace RumpelQuiz {
namespace {

struct Utf8Symbol {
    std::size_t begin;
    std::size_t end;
    char32_t value;
};

bool IsContinuationByte(unsigned char byte) {
    return (byte & 0xC0U) == 0x80U;
}

std::optional<std::vector<Utf8Symbol>> DecodeUtf8(
    std::string_view text) {
    std::vector<Utf8Symbol> symbols;
    symbols.reserve(text.size());

    std::size_t position = 0;

    while (position < text.size()) {
        const std::size_t symbol_begin = position;
        const auto first =
            static_cast<unsigned char>(text[position++]);

        char32_t code_point = 0;
        char32_t minimum_code_point = 0;
        std::size_t continuation_count = 0;

        if (first <= 0x7FU) {
            code_point = first;
        } else if (first >= 0xC2U && first <= 0xDFU) {
            code_point = first & 0x1FU;
            minimum_code_point = 0x80;
            continuation_count = 1;
        } else if (first >= 0xE0U && first <= 0xEFU) {
            code_point = first & 0x0FU;
            minimum_code_point = 0x800;
            continuation_count = 2;
        } else if (first >= 0xF0U && first <= 0xF4U) {
            code_point = first & 0x07U;
            minimum_code_point = 0x10000;
            continuation_count = 3;
        } else {
            return std::nullopt;
        }

        if (position + continuation_count > text.size()) {
            return std::nullopt;
        }

        for (std::size_t index = 0;
             index < continuation_count;
             ++index) {
            const auto byte =
                static_cast<unsigned char>(text[position++]);

            if (!IsContinuationByte(byte)) {
                return std::nullopt;
            }

            code_point =
                (code_point << 6U) |
                static_cast<char32_t>(byte & 0x3FU);
        }

        if (continuation_count != 0 &&
            code_point < minimum_code_point) {
            return std::nullopt;
        }

        if (code_point > 0x10FFFFU) {
            return std::nullopt;
        }

        if (code_point >= 0xD800U &&
            code_point <= 0xDFFFU) {
            return std::nullopt;
        }

        // PostgreSQL TEXT не принимает нулевой байт.
        if (code_point == U'\0') {
            return std::nullopt;
        }

        symbols.push_back(Utf8Symbol{
            symbol_begin,
            position,
            code_point,
        });
    }

    return symbols;
}

bool IsUnicodeWhitespace(char32_t code_point) {
    return
        (code_point >= 0x0009U && code_point <= 0x000DU) ||
        code_point == 0x0020U ||
        code_point == 0x0085U ||
        code_point == 0x00A0U ||
        code_point == 0x1680U ||
        (code_point >= 0x2000U && code_point <= 0x200AU) ||
        code_point == 0x2028U ||
        code_point == 0x2029U ||
        code_point == 0x202FU ||
        code_point == 0x205FU ||
        code_point == 0x3000U;
}

std::optional<std::vector<std::string>> ParseSearchWords(
    std::string_view query) {
    const auto symbols = DecodeUtf8(query);

    if (!symbols || symbols->empty()) {
        return std::nullopt;
    }

    std::size_t first_non_space = 0;

    while (first_non_space < symbols->size() &&
           IsUnicodeWhitespace(
               (*symbols)[first_non_space].value)) {
        ++first_non_space;
    }

    if (first_non_space == symbols->size()) {
        return std::nullopt;
    }

    std::size_t last_non_space = symbols->size() - 1;

    while (last_non_space > first_non_space &&
           IsUnicodeWhitespace(
               (*symbols)[last_non_space].value)) {
        --last_non_space;
    }

    // Количество Unicode code points после удаления
    // начальных и конечных пробельных символов.
    const std::size_t query_length =
        last_non_space - first_non_space + 1;

    if (query_length < 2 || query_length > 100) {
        return std::nullopt;
    }

    std::vector<std::string> words;
    std::optional<std::size_t> word_begin;

    for (std::size_t index = first_non_space;
         index <= last_non_space;
         ++index) {
        const auto& symbol = (*symbols)[index];

        if (IsUnicodeWhitespace(symbol.value)) {
            if (word_begin) {
                words.emplace_back(
                    query.substr(
                        *word_begin,
                        symbol.begin - *word_begin
                    )
                );

                word_begin.reset();
            }

            continue;
        }

        if (!word_begin) {
            word_begin = symbol.begin;
        }
    }

    if (word_begin) {
        const std::size_t word_end =
            (*symbols)[last_non_space].end;

        words.emplace_back(
            query.substr(
                *word_begin,
                word_end - *word_begin
            )
        );
    }

    if (words.empty()) {
        return std::nullopt;
    }

    return words;
}

std::optional<int> ParseInteger(
    const std::optional<std::string_view>& text,
    int default_value,
    int minimum,
    int maximum) {
    if (!text) {
        return default_value;
    }

    if (text->empty()) {
        return std::nullopt;
    }

    int value = 0;

    const char* const begin = text->data();
    const char* const end = begin + text->size();

    const auto conversion =
        std::from_chars(begin, end, value);

    if (conversion.ec != std::errc{} ||
        conversion.ptr != end ||
        value < minimum ||
        value > maximum) {
        return std::nullopt;
    }

    return value;
}

}  // namespace

std::optional<PeopleSearchParams> ParsePeopleSearchParams(
    const RawPeopleSearchParams& raw_params) {
    const auto words =
        ParseSearchWords(raw_params.query);

    const auto limit =
        ParseInteger(raw_params.limit, 20, 1, 50);

    const auto offset =
        ParseInteger(raw_params.offset, 0, 0, 10000);

    if (!words || !limit || !offset) {
        return std::nullopt;
    }

    return PeopleSearchParams{
        std::move(*words),
        *limit,
        *offset,
    };
}

}  // namespace RumpelQuiz