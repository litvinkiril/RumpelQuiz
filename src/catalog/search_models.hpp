#pragma once
#include <boost/uuid/uuid.hpp>
#include <string>
#include <vector>

namespace RumpelQuiz {

struct TestInfo {
    boost::uuids::uuid test_id;
    std::string creator_first_name;
    std::string creator_last_name;
    std::string title;
    int question_count = 0;
    bool is_favourite = false;
};

struct SearchTestsResult {
    std::vector<TestInfo> tests_info;
};

struct QuizInfo {
    boost::uuids::uuid quiz_id;
    std::string creator_first_name;
    std::string creator_last_name;
    std::string title;
    int question_count = 0;
    bool is_favourite = false;
};

struct SearchQuizzesResult {
    std::vector<QuizInfo> quizzes_info;
};
} // namespace RumpelQuiz
