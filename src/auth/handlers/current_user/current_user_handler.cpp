#include "current_user_handler.hpp"

#include <boost/uuid/uuid_io.hpp>
#include <userver/formats/json/value_builder.hpp>

#include "auth/middleware/jwt/auth_context.hpp"

namespace RumpelQuiz {

userver::formats::json::Value CurrentUserHandler::HandleRequestJsonThrow(
    const userver::server::http::HttpRequest&,
    const userver::formats::json::Value&,
    userver::server::request::RequestContext& context) const {
  userver::formats::json::ValueBuilder response;
  response["user_id"] =
      boost::uuids::to_string(GetAuthenticatedUserId(context));
  return response.ExtractValue();
}

}  // namespace RumpelQuiz
