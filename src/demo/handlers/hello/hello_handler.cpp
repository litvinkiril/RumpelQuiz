#include "hello_handler.hpp"

#include "demo/greeting/greeting.hpp"

namespace RumpelQuiz {

std::string
Hello::HandleRequestThrow(const userver::server::http::HttpRequest& request, userver::server::request::RequestContext&)
    const {
    return SayHelloTo(request.GetArg("name"), UserType::kFirstTime);
}

}  // namespace RumpelQuiz
