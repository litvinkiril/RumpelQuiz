#pragma once

#include <optional>
#include <string>
#include <vector>

#include <boost/uuid/uuid.hpp>

namespace RumpelQuiz {

struct EducationSearchItem {
  boost::uuids::uuid id;
  std::string name;
  std::vector<std::string> facultet_names;
  std::optional<std::string> program_name;
};

}  // namespace RumpelQuiz
