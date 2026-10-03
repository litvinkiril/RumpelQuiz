#pragma once

#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <boost/uuid/uuid.hpp>
#include <userver/components/component_base.hpp>
#include <userver/engine/single_consumer_event.hpp>

namespace RumpelQuiz {

// An in-process notification hub. The database remains the source of truth.
class GameEvents final : public userver::components::ComponentBase {
 public:
  static constexpr std::string_view kName = "game-events";
  using userver::components::ComponentBase::ComponentBase;

  struct Subscriber {
    explicit Subscriber(bool host) : is_host(host) {}
    const bool is_host;
    std::mutex mutex;
    std::deque<std::string> pending;
    userver::engine::SingleConsumerEvent wake;
  };

  enum class Audience { kAll, kHost, kStudents };

  std::shared_ptr<Subscriber> Subscribe(const boost::uuids::uuid& session,
                                        bool is_host);
  void Publish(const boost::uuids::uuid& session, Audience audience,
               std::string_view type, std::string_view json);
  static std::string Pop(const std::shared_ptr<Subscriber>& subscriber);

 private:
  std::mutex mutex_;
  std::unordered_map<std::string, std::vector<std::weak_ptr<Subscriber>>> rooms_;
};

}  // namespace RumpelQuiz
