#include "user_repository.hpp"

namespace RumpelQuiz {

std::optional<UserData> UserRepository::FindByEmail(
    userver::storages::postgres::Transaction& transaction,
    std::string_view email) const {
  const auto result = transaction.Execute(
      "SELECT id, email, password_hash, email_verified "
      "FROM auth.users "
      "WHERE email = $1",
      email);

  if (result.IsEmpty()) {
    return std::nullopt;
  }

  const auto row = result[0];

  return UserData{
      row["id"].As<boost::uuids::uuid>(), row["email"].As<std::string>(),
      row["password_hash"].As<std::string>(), row["email_verified"].As<bool>()};
}

std::optional<UserData> UserRepository::FindById(
    userver::storages::postgres::Transaction& transaction,
    const boost::uuids::uuid& user_id) const {
  const auto result = transaction.Execute(
      "SELECT id, email, password_hash, email_verified "
      "FROM auth.users "
      "WHERE id = $1",
      user_id);

  if (result.IsEmpty()) {
    return std::nullopt;
  }

  const auto row = result[0];
  return UserData{
      row["id"].As<boost::uuids::uuid>(), row["email"].As<std::string>(),
      row["password_hash"].As<std::string>(), row["email_verified"].As<bool>()};
}

boost::uuids::uuid UserRepository::CreateUser(
    userver::storages::postgres::Transaction& transaction,
    std::string_view email, std::string_view password_hash) const {
  auto result = transaction.Execute(
      R"(
                INSERT INTO auth.users (
                    email,
                    password_hash
                )
                VALUES ($1, $2)
                RETURNING id
            )",
      email, password_hash);

  return result[0]["id"].As<boost::uuids::uuid>();
}

void UserRepository::UpdateUnverifiedUser(
    userver::storages::postgres::Transaction& transaction,
    const boost::uuids::uuid& user_id, std::string_view password_hash) const {
  transaction.Execute(
      R"(
                UPDATE auth.users
                SET password_hash = $1,
                    updated_at = NOW()
                WHERE id = $2
                AND email_verified = FALSE
            )",
      password_hash, user_id);
}

void UserRepository::VerifyUser(
    userver::storages::postgres::Transaction& transaction,
    const boost::uuids::uuid& user_id) const {
  transaction.Execute(
      R"(
                UPDATE auth.users
                SET email_verified = TRUE
                WHERE id = $1
            )",
      user_id);
}
}  // namespace RumpelQuiz
