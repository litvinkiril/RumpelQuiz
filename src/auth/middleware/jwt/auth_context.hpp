#pragma once

#include <boost/uuid/uuid.hpp>
#include <string_view>
#include <userver/server/request/request_context.hpp>

namespace RumpelQuiz {

inline constexpr std::string_view kAuthenticatedUserId =
    "authenticated-user-id";

inline const boost::uuids::uuid& GetAuthenticatedUserId(
    const userver::server::request::RequestContext& context) {
  return context.GetData<boost::uuids::uuid>(kAuthenticatedUserId);
}

}  // namespace RumpelQuiz
