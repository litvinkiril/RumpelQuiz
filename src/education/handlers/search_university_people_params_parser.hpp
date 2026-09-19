#pragma once

#include <optional>
#include <string_view>

#include "education/education_models.hpp"

namespace RumpelQuiz {

struct RawPeopleSearchParams {
    std::string_view query;
    std::optional<std::string_view> limit;
    std::optional<std::string_view> offset;
};

std::optional<PeopleSearchParams> ParsePeopleSearchParams(
    const RawPeopleSearchParams& raw_params
);

}  // namespace RumpelQuiz